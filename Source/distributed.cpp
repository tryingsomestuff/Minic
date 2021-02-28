#include "distributed.h"

#ifdef WITH_MPI

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "smp.hpp"
#include "stats.hpp"

namespace Distributed{
   int worldSize;
   int rank;
   std::string name;

   MPI_Comm _commInput  = MPI_COMM_NULL;
   MPI_Comm _commSig    = MPI_COMM_NULL;
   MPI_Request _request = MPI_REQUEST_NULL;

   namespace{
      std::array<Counter,Stats::sid_maxid> _countersBufSend; 
      std::array<Counter,Stats::sid_maxid> _countersBufRecv; 
      
      uint64_t _sendCallCount;

      bool _stopFlag;

   }

   void init(){
      MPI_Init(NULL, NULL);
      MPI_Comm_size(MPI_COMM_WORLD, &worldSize);
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      char processor_name[MPI_MAX_PROCESSOR_NAME];
      int name_len;
      MPI_Get_processor_name(processor_name, &name_len);
      name = processor_name;

      if ( !isMainProcess() ) DynamicConfig::silent = true;

      MPI_Comm_dup(MPI_COMM_WORLD, &_commInput);
      MPI_Comm_dup(MPI_COMM_WORLD, &_commSig);
   
      _sendCallCount = 0ull;
   }

   void finalize(){

      MPI_Comm_free(&_commInput);
      MPI_Comm_free(&_commSig);

      MPI_Finalize();
   }

   bool isMainProcess(){
       return rank == 0;
   }

   // barrier
   void sync(){
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "syncing";
       MPI_Barrier(MPI_COMM_WORLD);  
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "...done";
   }

   void initSignals(){
       _countersBufSend.fill(0ull);
       _countersBufRecv.fill(0ull);

       _stopFlag = false;
   }

   void pollSignals(){
      // gather stats from all local threads
      for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
         for (auto & it : ThreadPool::instance() ){ 
           _countersBufSend[k] += it->stats.counters[k];  
         }
      }
      asyncAllReduceSum(_countersBufSend.data(),_countersBufRecv.data(),Stats::sid_maxid,_commSig);


      ++_sendCallCount;
   }

   void syncSignals(){

   }


}

#else
namespace Distributed{
   int rank       = 0;
   int _commInput = 0;
   int _commSig   = 0;
   int _request   = 0;
}

#endif