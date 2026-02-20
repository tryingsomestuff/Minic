#include "energyMonitor.hpp"

#include <filesystem>

namespace {
   std::unique_ptr<EnergyMonitor> energyMonitor;
}

EnergyMonitor* getEnergyMonitor() { 
   return energyMonitor.get(); 
}

EnergyMonitor::EnergyMonitor(int period_ms): 
   _period(period_ms), 
   _ncores_phys(std::max(1, countPhysicalCores())) {}

EnergyMonitor::~EnergyMonitor() { 
   stop(); 
}

void EnergyMonitor::start() {
   _startTime = Clock::now();
   _domains = findEnergyDomains();
   if (_domains.empty()) {
      Logging::LogIt(Logging::logWarn) << "No RAPL energy domains found";
      return;
   }

   for (const auto& d : _domains) {
      const long long v = readUj(d.energy_path.c_str());
      if (v < 0) warnPermission(d.energy_path);
      _lastEnergy[d.energy_path] = v;
   }

   _running = true;
   _worker = std::thread(&EnergyMonitor::loop, this);
}

void EnergyMonitor::stop() {
   _running = false;
   if (_worker.joinable()) _worker.join();
}

double EnergyMonitor::cpuW() const { 
   return _cpuWLocal.load(std::memory_order_relaxed); 
}

double EnergyMonitor::ramW() const { 
   return _ramWLocal.load(std::memory_order_relaxed); 
}

#ifdef WITH_MPI
PowerResult EnergyMonitor::reducePower(int root, MPI_Comm comm) const {
   const double cpu = cpuW();
   const double ram = ramW();

   PowerResult result{0.0, 0.0};
   MPI_Reduce(&cpu, &result.cpu_sum, 1, MPI_DOUBLE, MPI_SUM, root, comm);
   MPI_Reduce(&ram, &result.ram_sum, 1, MPI_DOUBLE, MPI_SUM, root, comm);

   return result;
}
#endif

void EnergyMonitor::reportValues(double cpu, double ram, const std::string& unit, Logging::LogLevel level) const {
   if (Distributed::isMainProcess()) {
      Logging::LogIt(level) << "CPU " << cpu << " " << unit 
                            << ", RAM " << ram << " " << unit;
   }
}

void EnergyMonitor::reportPower(Logging::LogLevel level) const {
#ifdef WITH_MPI
   const PowerResult result = reducePower();
   if (Distributed::isMainProcess()) {
      reportValues(result.cpu_sum, result.ram_sum, "W", level);
   }
#else
   reportValues(cpuW(), ramW(), "W", level);
#endif
}

void EnergyMonitor::reportEnergy(TimeType durationMs, Logging::LogLevel level) const {
   const double hours = durationMs / 1000.0 / 3600.0;
#ifdef WITH_MPI
   const PowerResult result = reducePower();
   if (Distributed::isMainProcess()) {
      reportValues(result.cpu_sum * hours, result.ram_sum * hours, "Wh", level);
   }
#else
   reportValues(cpuW() * hours, ramW() * hours, "Wh", level);
#endif
}

void EnergyMonitor::reportCost(TimeType durationMs, Logging::LogLevel level) const {
   const double hours = durationMs / 1000.0 / 3600.0;
#ifdef WITH_MPI
   const PowerResult result = reducePower();
   if (Distributed::isMainProcess()) {
      const double cpuCost = (result.cpu_sum * hours) / 1000.0 * kWhPriceEuro;
      const double ramCost = (result.ram_sum * hours) / 1000.0 * kWhPriceEuro;
      Logging::LogIt(level) << "CPU " << cpuCost << " €"
                            << ", RAM " << ramCost << " €"
                            << " (" << kWhPriceEuro << " €/kWh)";
   }
#else
   const double cpuCost = (cpuW() * hours) / 1000.0 * kWhPriceEuro;
   const double ramCost = (ramW() * hours) / 1000.0 * kWhPriceEuro;
   if (Distributed::isMainProcess()) {
      Logging::LogIt(level) << "CPU " << cpuCost << " €"
                            << ", RAM " << ramCost << " €"
                            << " (" << kWhPriceEuro << " €/kWh)";
   }
#endif
}

CpuTimes EnergyMonitor::readCpuTimes() {
   std::ifstream f("/proc/stat");
   std::string   line;
   CpuTimes      t{};
   if (std::getline(f, line)) {
      std::istringstream ss(line);
      std::string        cpu;
      ss >> cpu >> t.user >> t.nice >> t.system >> t.idle >> t.iowait >> t.irq >> t.softirq >> t.steal;
   }
   return t;
}

double EnergyMonitor::computeLoadFraction(const CpuTimes& prev, const CpuTimes& cur) {
   const long long prevIdle  = prev.idle + prev.iowait;
   const long long curIdle   = cur.idle + cur.iowait;
   const long long prevTotal = prev.user + prev.nice + prev.system + prevIdle + prev.irq + prev.softirq + prev.steal;
   const long long curTotal  = cur.user + cur.nice + cur.system + curIdle + cur.irq + cur.softirq + cur.steal;
   const long long deltaTotal = curTotal - prevTotal;
   const long long deltaIdle  = curIdle - prevIdle;

   if (deltaTotal <= 0) return 0.0;
   return double(deltaTotal - deltaIdle) / double(deltaTotal);
}

int EnergyMonitor::countPhysicalCores() {
   std::ifstream f("/proc/cpuinfo");
   std::string   line;
   std::set<std::pair<int, int>> cores;
   int phys = -1, core = -1;

   while (std::getline(f, line)) {
      if (line.find("physical id") != std::string::npos)
         phys = std::stoi(line.substr(line.find(":") + 1));
      else if (line.find("core id") != std::string::npos) {
         core = std::stoi(line.substr(line.find(":") + 1));
         if (phys >= 0 && core >= 0) cores.insert({phys, core});
      }
   }
   if (!cores.empty()) return static_cast<int>(cores.size());
#if defined(_SC_NPROCESSORS_ONLN)
   return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
   const unsigned int n = std::thread::hardware_concurrency();
   return static_cast<int>(n > 0 ? n : 1u);
#endif
}

long long EnergyMonitor::readUj(const char* path) {
   std::ifstream f(path);
   if (!f) return -1;
   long long v = 0;
   f >> v;
   return v;
}

std::string EnergyMonitor::readFile(const std::string& path) {
   std::ifstream f(path);
   std::string   s;
   std::getline(f, s);
   return s;
}

void EnergyMonitor::warnPermission(const std::string& p) {
   static std::atomic<bool> warned{false};
   if (!warned.exchange(true)) {
      Logging::LogIt(Logging::logWarn) << "\n[WARNING] Cannot read RAPL energy counters.\n"
                                       << "Check permissions on:\n  " << p << "\n"
                                       << "Try: sudo chmod a+r /sys/class/powercap/*/energy_uj\n";
   }
}

std::vector<EnergyDomain> EnergyMonitor::findEnergyDomains() const {
   std::vector<EnergyDomain> out;
   for (const auto& p : std::filesystem::directory_iterator("/sys/class/powercap")) {
      const auto energy = p.path() / "energy_uj";
      const auto name   = p.path() / "name";

      if (std::filesystem::exists(energy) && std::filesystem::exists(name)) {
         EnergyDomain d;
         d.energy_path = energy.string();
         d.name        = readFile(name.string());
         out.push_back(d);
      }
   }
   return out;
}

void EnergyMonitor::loop() {
   CpuTimes prevCpu = readCpuTimes();

   while (_running) {
      const auto t0     = std::chrono::steady_clock::now();
      const auto eStart = _lastEnergy;

      std::this_thread::sleep_for(std::chrono::milliseconds(_period));

      double cpuJ = 0.0;
      double ramJ = 0.0;

      for (const auto& d : _domains) {
         const long long e = readUj(d.energy_path.c_str());
         if (e < 0) continue;

         const double deltaJ = (e - eStart.at(d.energy_path)) * 1e-6;
         _lastEnergy[d.energy_path] = e;

         std::string lname = d.name;
         for (auto& c : lname) c = std::tolower(c);

         if (lname.find("package") != std::string::npos || lname.find("cpu") != std::string::npos)
            cpuJ += deltaJ;
         else if (lname.find("dram") != std::string::npos || lname.find("mem") != std::string::npos)
            ramJ += deltaJ;
      }

      const auto   t1       = std::chrono::steady_clock::now();
      const double dt       = std::chrono::duration<double>(t1 - t0).count();
      const CpuTimes curCpu = readCpuTimes();
      const double loadFrac = computeLoadFraction(prevCpu, curCpu);
      prevCpu               = curCpu;

      double activePhys = loadFrac * _ncores_phys;
      if (activePhys < 0.1) activePhys = 1.0;

      _cpuWLocal.store(cpuJ / dt / activePhys, std::memory_order_relaxed);
      _ramWLocal.store(ramJ / dt / activePhys, std::memory_order_relaxed);
   }
}

namespace {
   struct EnergyMonitorInitializer {
      static void create(int period_ms = 500) {
         energyMonitor = std::make_unique<EnergyMonitor>(period_ms);
      }
      static void destroy() {
         energyMonitor.reset();
      }
   };
}

void initEnergyMonitor(int period_ms) {
   EnergyMonitorInitializer::create(period_ms);
   if (energyMonitor) energyMonitor->start();
}

void finalizeEnergyMonitor() {
   if (energyMonitor) {
      energyMonitor->stop();
      const TimeType totalDuration = getTimeDiff(energyMonitor->getStartTime());
      if (Distributed::isMainProcess()) {
         Logging::LogIt(Logging::logInfo) << "Total execution time: " << totalDuration << " ms";
         energyMonitor->reportEnergy(totalDuration, Logging::logInfo);
         energyMonitor->reportCost(totalDuration, Logging::logInfo);
      }
      EnergyMonitorInitializer::destroy();
   }
}
