#pragma once

#include "definition.hpp"
#include "logging.hpp"
#include "position.hpp"

struct Searcher;

[[nodiscard]] std::string trim(const std::string& str, const std::string& whitespace = " \t");

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " ");
std::vector<std::string> tokenize(const std::string& str, const std::string& delimiters = " ");

void debug_king_cap(const Position& p);

[[nodiscard]] std::string ToString(const PVList& moves);

[[nodiscard]] std::string ToString(const MiniMove& m);

[[nodiscard]] std::string ToString(const Move& m, bool withScore = false);

[[nodiscard]] std::string ToString(const Position& p, bool noEval = false);

[[nodiscard]] std::string ToString(const Position::Material& mat);

[[nodiscard]] std::string ToString(const BitBoard& b);

template < typename T >
[[nodiscard]] std::string ToString(const std::array<T,64>& a){
   std::stringstream ss;
   for (int j = 7; j >= 0; --j) {
      ss << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "+---+---+---+---+---+---+---+---+"
         << "\n";
      ss << Logging::_protocolComment[Logging::ct] << "|";
      for (int i = 0; i < 8; ++i) ss << std::setw(3) << static_cast<int>(a[i + j * 8]) << '|';
   }
   ss << "\n";
   ss << Logging::_protocolComment[Logging::ct] << "+---+---+---+---+---+---+---+---+";
   return ss.str();
}

template<typename F, typename... Arguments> 
void threadedWork(F&& worker, const size_t nbthreads, const uint64_t size, Arguments... args) {
   std::vector<std::thread> threads(nbthreads);
   const size_t grainsize = size / nbthreads;
   size_t       work_iter = 0;
   for (auto it = std::begin(threads); it != std::end(threads) - 1; ++it) {
      *it = std::thread(worker, work_iter, work_iter + grainsize, args...);
      work_iter += grainsize;
   }
   threads.back() = std::thread(worker, work_iter, size, args...);
   for (auto&& i : threads) { i.join(); }
}

bool checkEval(const Position & p, ScoreType e, Searcher & context, const std::string & txt);

/*
#include <cassert>
#include <list>

class AbstractDestroyer {
public:
  virtual ~AbstractDestroyer();
protected:
  AbstractDestroyer();
  virtual void destroy() = 0;
};

class DestroyerContainer {
public:
  DestroyerContainer();
  virtual ~DestroyerContainer();
  static DestroyerContainer& Get();
  void addBack (AbstractDestroyer* destroyer);
  void addFront(AbstractDestroyer* destroyer);
private:
  std::list<AbstractDestroyer*> _destroyers;
};

template <class DOOMED>
class Destroyer : public AbstractDestroyer {
public:
    Destroyer(DOOMED * d):_doomed(d){ assert(d); }
    ~Destroyer(){ destroy(); }
    void destroy(){ if(_doomed) delete _doomed; _doomed = 0;}
private:
    DOOMED * _doomed;
    Destroyer(const Destroyer &){}
    Destroyer& operator = (const Destroyer &){return *this;}
};
*/