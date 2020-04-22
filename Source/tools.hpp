#pragma once

#include "definition.hpp"

#include "position.hpp"

std::string trim(const std::string& str, const std::string& whitespace = " \t");

void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters = " " );

void debug_king_cap(const Position & p);

std::string ToString(const PVList & moves);

std::string ToString(const Move & m    , bool withScore = false);

std::string ToString(const Position & p, bool noEval = false);

std::string ToString(const Position::Material & mat);

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