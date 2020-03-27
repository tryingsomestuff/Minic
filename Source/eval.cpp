#include "eval.hpp"

// due to bad design, I force instanciation of this function template here ...
struct Dummy{
  Dummy(){
    Position p;
    EvalData data;
    eval<false,false>(p,data,ThreadPool::instance().main());
    eval<false,true >(p,data,ThreadPool::instance().main());
    eval<true ,false>(p,data,ThreadPool::instance().main());
    eval<true ,true >(p,data,ThreadPool::instance().main());
  }
};

volatile Dummy dummy;
