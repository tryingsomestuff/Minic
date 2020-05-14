#include "eval.hpp"

// due to bad design, I force instanciation of this function template here ...
struct Dummy{
  Dummy(){
    Position p;
    EvalData data;
    Searcher s(999);
    eval<false,false>(p,data,s);
    eval<false,true >(p,data,s);
    eval<true ,false>(p,data,s);
    eval<true ,true >(p,data,s);
  }
};

volatile Dummy dummy;
