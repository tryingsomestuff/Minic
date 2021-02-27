#pragma once

#include "definition.hpp"

#ifdef WITH_MPI

#include "mpi.h"

namespace Distributed{

   extern int worldSize;
   extern int rank;
   extern std::string name;

   void init();
   void finalize();
   bool isMainProcess();
   void sync();
   void syncTable();
   void syncStop();

   template<typename T>
   struct TraitMpiType{};

   template<>
   struct TraitMpiType<bool>{static constexpr MPI_Datatype type = MPI_CXX_BOOL;};
   template<>
   struct TraitMpiType<char>{static constexpr MPI_Datatype type = MPI_CHAR;};

   template<typename T>
   inline void bcast(T * v, int n){
      ///@todo check for errors
      MPI_Bcast(v, n, TraitMpiType<T>::type, 0, MPI_COMM_WORLD);
   }
}

#else // ! WITH_MPI
   inline void init(){}
   inline void finalize(){}
   inline bool isMainProcess(){return true;}
   inline void sync(){}
   inline void syncTable(){}
   inline void bcast(T * v, int n){}
#endif