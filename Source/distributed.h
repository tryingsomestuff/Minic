#pragma once

#include "definition.hpp"
#include "stats.hpp"
#include "transposition.hpp"

#ifdef WITH_MPI

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Wcast-qual"
#include "mpi.h"
#pragma GCC diagnostic pop

//#define DEBUG_DISTRIBUTED
#ifdef DEBUG_DISTRIBUTED
#define DEBUGCOUT(x) Logging::LogIt(Logging::logDebug) << rank << " " << (x);
#else
#define DEBUGCOUT(x)
#endif

#include "logging.hpp"

/**
 * There is not much to do to use distributed memory the same way as we use 
 * concurrent threads and the TT in the lazy SMP shared memory approach.
 * The TT being lock-free, it can be asynchronously updated by all process.
 *
 * Other things to do are :
 *  - ensure only main process is reading input from stdin and broadcast command to other process.
 *  - ensure stats are reduce and main process can display and use them (this is done by two-sided comm)
 *  - ensure stop flag from master process is forwarded to other process (this is done using one-sided comm)
 *
 * To do so, because a process search tree will diverge (we want them to !), async comm are requiered.
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
   extern MPI_Comm _commTT2;
   extern MPI_Comm _commStat;
   extern MPI_Comm _commStat2;
   extern MPI_Comm _commInput;
   extern MPI_Comm _commStop;
   extern MPI_Comm _commMove;

   extern MPI_Request _requestTT;
   extern MPI_Request _requestStat;
   extern MPI_Request _requestInput;
   extern MPI_Request _requestMove;

   extern MPI_Win _winStop;

   void init();
   void lateInit();
   void finalize();
   bool isMainProcess();
   void sync(MPI_Comm & com, const std::string & msg = "");

   struct EntryHash{
      Hash h = nullHash;
      TT::Entry e;
   };

   template<typename T>
   struct TraitMpiType{};

   template<>
   struct TraitMpiType<bool>{static constexpr MPI_Datatype type = MPI_CXX_BOOL;};
   template<>
   struct TraitMpiType<char>{static constexpr MPI_Datatype type = MPI_CHAR;};
   template<>
   struct TraitMpiType<Counter>{static constexpr MPI_Datatype type = MPI_LONG_LONG_INT;};
   template<>
   struct TraitMpiType<EntryHash>{static constexpr MPI_Datatype type = MPI_CHAR;}; // WARNING : size must be adapted *sizeof(EntryHash)
   template<>
   struct TraitMpiType<Move>{static constexpr MPI_Datatype type = MPI_INT;};

   void checkError(int err);

   template<typename T>
   inline void bcast(T * v, int n, MPI_Comm & com){
      if (worldSize < 2) return;
      checkError(MPI_Bcast(v, n, TraitMpiType<T>::type, 0, com));
   }

   template<typename T>
   inline void asyncBcast(T * v, int n, MPI_Request & req, MPI_Comm & com){
      if (worldSize < 2) return;
      checkError(MPI_Ibcast(v, n, TraitMpiType<T>::type, 0, com, &req));
   }

   template<typename T>
   inline void allReduceSum(T * local, T* global, int n, MPI_Comm & com){
      if (worldSize < 2) return;
      checkError(MPI_Allreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com));
   }

   template<typename T>
   inline void allReduceMax(T * local, T* global, int n, MPI_Comm & com){
      if (worldSize < 2) return;
      checkError(MPI_Allreduce(local, global, n, TraitMpiType<T>::type, MPI_MAX, com));
   }

   template<typename T>
   inline void asyncAllReduceSum(T * local, T* global, int n, MPI_Request & req, MPI_Comm & com){
      if (worldSize < 2) return;
      checkError(MPI_Iallreduce(local, global, n, TraitMpiType<T>::type, MPI_SUM, com, &req));
   }

   template<typename T>
   inline void put(T * ptr, int n, MPI_Win & window, int target ){
      if (worldSize < 2) return;
      checkError(MPI_Put(ptr, n, TraitMpiType<T>::type, target, MPI_Aint(0), n, TraitMpiType<T>::type, window));
   }

   template<typename T>
   inline void get(T * ptr, int n, MPI_Win & window, int source ){
      if (worldSize < 2) return;
      checkError(MPI_Get(ptr, n, TraitMpiType<T>::type, source, MPI_Aint(0), n, TraitMpiType<T>::type, window));
   }

   template<typename T>
   inline void putMainToAll(T * ptr, int n, MPI_Win & window ){
      if (worldSize < 2) return;
      checkError(MPI_Win_lock_all(0, window));
      for(int r = 1 ; r < worldSize ; ++r){
         put(ptr,n,window,r);
      }
      //checkError(MPI_Win_flush_all(window)); 
      checkError(MPI_Win_unlock_all(window));
   }

   template<typename T>
   inline void asyncAllGather( T * inptr, T * outptr, int n, MPI_Request & req, MPI_Comm & com ){
      if (worldSize < 2) return;
      checkError(MPI_Iallgather(inptr, n, TraitMpiType<T>::type, outptr, n, TraitMpiType<T>::type, com, &req));
   }

   inline void winFence(MPI_Win & window){
      checkError(MPI_Win_fence(0, window));
   }

   void waitRequest(MPI_Request & req);

   void initStat();
   void sendStat();
   void pollStat();
   void syncStat();
   void showStat();
   Counter counter(Stats::StatId id);

   void setEntry(const Hash h, const TT::Entry & e);
   void syncTT();

}

#else // ! WITH_MPI
namespace Distributed{

   extern int worldSize;
   extern int rank;

   typedef int DummyType;
   extern DummyType _commTT;
   extern DummyType _commTT2;
   extern DummyType _commStat;
   extern DummyType _commStat2;
   extern DummyType _commInput;
   extern DummyType _commStop;
   extern DummyType _commMove;

   extern DummyType _requestTT;
   extern DummyType _requestStat;
   extern DummyType _requestInput;
   extern DummyType _requestMove;

   extern DummyType _winStop;

   inline void checkError(int ){}

   inline void init(){}
   inline void lateInit(){}
   inline void finalize(){}
   inline bool isMainProcess(){return true;}
   inline void sync(DummyType &, const std::string & ){}

   template<typename T>
   inline void asyncBcast(T *, int , DummyType &, DummyType & ){}

   template<typename T>
   inline void putMainToAll(T * , int , DummyType & ){}

   template<typename T>
   inline void get(T * , int , DummyType & , int  ){}

   inline void waitRequest(DummyType & ){}

   inline void winFence(DummyType & ){}

   inline void initStat(){}
   inline void sendStat(){}
   inline void pollStat(){}
   inline void syncStat(){}
   inline void showStat(){}
   Counter counter(Stats::StatId id);

   inline void setEntry(const Hash, const TT::Entry &){}
   inline void syncTT(){}

}
#endif