#pragma once

#include "definition.hpp"

#ifdef WITH_MPI

#include "mpi.h"

/**
 * There is not much to do to use distributed memory the same way as we use 
 * concurrent threads and the TT in the lazy SMP shared memory approach.
 * The TT being lock-free, it can be asynchronously update by all process.
 * Some kind all async all reduce based on a "quality" of the data inside the TT entry.
 *
 * Other things to do are :
 *  - ensure only main process is reading input from stdin and broadcast command to other process.
 *  - ensure stats are reduce and main process can display and use them
 *  - ensure stop flag from master process is forwarded to other process
 *
 * To do so, and a process search tree will diverge (we want them to !), async comm are requiered.
 * So we try to send new data as soon as previous ones are received everywhere required.
 *
 * When WITH_MPI is not defined, everything in here does nothing so that implementation for
 * distributed memory is not intrusive too much inside the code base.
 *
 */

namespace Distributed{

   extern int worldSize;
   extern int rank;
   extern std::string name;

   extern MPI_Comm _commTT;   
   extern MPI_Comm _commStat;
   extern MPI_Comm _commInput;
   extern MPI_Comm _commStop;

   extern MPI_Request _requestTT;
   extern MPI_Request _requestStat;
   extern MPI_Request _requestInput;

   void init();
   void finalize();
   bool isMainProcess();
   void sync();

   template<typename T>
   struct TraitMpiType{};

   template<>
   struct TraitMpiType<bool>{static constexpr MPI_Datatype type = MPI_CXX_BOOL;};
   template<>
   struct TraitMpiType<char>{static constexpr MPI_Datatype type = MPI_CHAR;};
   template<>
   struct TraitMpiType<Counter>{static constexpr MPI_Datatype type = MPI_LONG_LONG_INT;};

   template<typename T>
   inline void bcast(T * v, int n, MPI_Comm & com){
      ///@todo check for errors
      MPI_Bcast(v, n, TraitMpiType<T>::type, 0, com);
   }

   template<typename T>
   inline void asyncBcast(T * v, int n, MPI_Request & req, MPI_Comm & com){
      ///@todo check for errors
      MPI_Ibcast(v, n, TraitMpiType<T>::type, 0, com, &req);
   }

   template<typename T>
   inline void allReduceSum(T * local, T* global, int n, MPI_Comm & com){
      ///@todo check for errors
      MPI_Allreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com);    
   }

   template<typename T>
   inline void allReduceMax(T * local, T* global, int n, MPI_Comm & com){
      ///@todo check for errors
      MPI_Allreduce(local, global, n, TraitMpiType<T>::type, MPI_MAX, com);    
   }

   template<typename T>
   inline void asyncAllReduceSum(T * local, T* global, int n, MPI_Request & req, MPI_Comm & com){
      ///@todo check for errors
      MPI_Iallreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com, &req);    
   }

   void waitRequest(MPI_Request & req);

   void initStat();
   void sendStat();
   void pollStat(bool display = false);
   void syncStat();

}

#else // ! WITH_MPI
namespace Distributed{

   extern int rank;

   typedef int DummyType;
   extern DummyType _commInput;
   extern DummyType _commSig;
   extern DummyType _requestTT;
   extern DummyType _requestStat;
   extern DummyType _requestInput;

   inline void init(){}
   inline void finalize(){}
   inline bool isMainProcess(){return true;}
   inline void sync(){}

   template<typename T>
   inline void asyncBcast(T *, int , DummyType &, DummyType & ){}

   inline void waitRequest(DummyType & ){}

   inline void initStat(){}
   inline void sendStat(){}
   inline void pollStat(){}
   inline void syncStat(){}

}
#endif