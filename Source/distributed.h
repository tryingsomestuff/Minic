#pragma once

#include "definition.hpp"

#ifdef WITH_MPI

#include "mpi.h"

namespace Distributed{

   extern int worldSize;
   extern int rank;
   extern std::string name;

   extern MPI_Comm _commInput;
   extern MPI_Comm _commSig;
   extern MPI_Request _request;

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
   template<>
   struct TraitMpiType<Counter>{static constexpr MPI_Datatype type = MPI_LONG_LONG_INT;};

   template<typename T>
   inline void bcast(T * v, int n, MPI_Comm com){
      ///@todo check for errors
      MPI_Bcast(v, n, TraitMpiType<T>::type, 0, com);
   }

   template<typename T>
   inline void allReduceSum(T * local, T* global, int n, MPI_Comm com){
      ///@todo check for errors
      MPI_Allreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com);    
   }

   template<typename T>
   inline void asyncAllReduceSum(T * local, T* global, int n, MPI_Comm com){
      ///@todo check for errors
      MPI_Iallreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com,&_request);    
   }

}

#else // ! WITH_MPI
namespace Distributed{

   extern int rank;

   extern int _commInput;
   extern int _commSig;
   extern int _request;

   inline void init(){}
   inline void finalize(){}
   inline bool isMainProcess(){return true;}
   inline void sync(){}
   inline void syncTable(){}
   template<typename T>
   inline void bcast(T * , int, int ){}
   template<typename T>
   inline void allReduceSum(T * , T* , int, int * ){}
}
#endif