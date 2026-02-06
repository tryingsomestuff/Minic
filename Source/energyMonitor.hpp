#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 * Energy monitoring class using RAPL (Running Average Power Limit)
 * Monitors CPU and RAM power consumption on Linux systems
 */
struct EnergyDomain {
   std::string energy_path;
   std::string name;
};

struct PowerResult {
   double cpu_sum;
   double ram_sum;
};

struct CpuTimes {
   long long user, nice, system, idle, iowait, irq, softirq, steal;
};

class EnergyMonitor {
  public:
   explicit EnergyMonitor(int period_ms = 500);
   ~EnergyMonitor();

   // non copyable
   EnergyMonitor(const EnergyMonitor&) = delete;
   EnergyMonitor(const EnergyMonitor&&) = delete;
   EnergyMonitor& operator=(const EnergyMonitor&) = delete;
   EnergyMonitor& operator=(const EnergyMonitor&&) = delete;

   void start();
   void stop();

   [[nodiscard]] double cpuW() const;
   [[nodiscard]] double ramW() const;

#ifdef WITH_MPI
   [[nodiscard]] PowerResult reducePower(int root = 0, MPI_Comm comm = MPI_COMM_WORLD) const;
#endif

   void reportPower(Logging::LogLevel level = Logging::logGUI) const;
   void reportEnergy(TimeType durationMs, Logging::LogLevel level = Logging::logGUI) const;
   void reportCost(TimeType durationMs, Logging::LogLevel level = Logging::logGUI) const;

  private:
   static constexpr double kWhPriceEuro = 0.2016;

   std::atomic<bool>   _running{false};
   std::thread         _worker;
   const int           _period;
   const int           _ncores_phys;
   std::vector<EnergyDomain>      _domains;
   std::map<std::string,long long> _lastEnergy;
   std::atomic<double> _cpuWLocal{0.0};
   std::atomic<double> _ramWLocal{0.0};

   static CpuTimes readCpuTimes();
   [[nodiscard]] static double computeLoadFraction(const CpuTimes& prev, const CpuTimes& cur);
   [[nodiscard]] static int countPhysicalCores();
   [[nodiscard]] static long long readUj(const char* path);
   [[nodiscard]] static std::string readFile(const std::string& path);
   static void warnPermission(const std::string& p);
   [[nodiscard]] std::vector<EnergyDomain> findEnergyDomains() const;
   void loop();
   void reportValues(double cpu, double ram, const std::string& unit, Logging::LogLevel level) const;
};

// Global accessor functions
EnergyMonitor* getEnergyMonitor();
void initEnergyMonitor(int period_ms = 500);
void finalizeEnergyMonitor();
