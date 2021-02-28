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

   MPI_Request _requestTT    = MPI_REQUEST_NULL;
   MPI_Request _requestStat  = MPI_REQUEST_NULL;
   MPI_Request _requestInput = MPI_REQUEST_NULL;

   uint64_t _nbStatPoll;

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
   
      _sendCallCount = 0ull;
   }

   void finalize(){
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

   void waitRequest(MPI_Request & req){
        // don't rely on MPI to do a "passive wait", most implementations are doing a busy-wait, so use 100% cpu
        if (Distributed::isMainProcess()){
            MPI_Wait(&req, MPI_STATUS_IGNORE);
        }
        else{
            while (true){ 
                int flag;
                MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
                if (flag) break;
                else std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } 
   }

   void initStat(){
       _countersBufSend.fill(0ull);
       _countersBufRecv.fill(0ull);

       _nbStatPoll = 0ull;

       _stopFlag = false;
   }

   void sendStat(){
      std::cout << "sendStat " << rank << std::endl;
      // launch an async reduce
      asyncAllReduceSum(_countersBufSend.data(),_countersBufRecv.data(),Stats::sid_maxid,_requestStat);
      ++_nbStatPoll;
   }

   void pollStat(){
      std::cout << "poolStat " << rank << std::endl;
      int flag;
      MPI_Test(&_requestStat, &flag, MPI_STATUS_IGNORE);
      // if previous comm is done, launch another one
      if (flag){ 
         std::cout << "poolStat previous done " << rank << std::endl;
         // show reduced stats
         for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
            Logging::LogIt(Logging::logInfo) << "All rank reduced: " << Stats::Names[k] << " " << _countersBufRecv[k];
         }

         // gather stats from all local threads
         for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
            _countersBufSend[k] = 0ull;
            for (auto & it : ThreadPool::instance() ){ 
              _countersBufSend[k] += it->stats.counters[k];  
            }
         }
         sendStat();
      }      
   }

   // get all rank to a common synchronous state at the end of search
   void syncStat(){
      std::cout << "syncStat " << rank << std::endl;
      uint64_t globalNbPoll = 0ull;
      allReduceMax(&_nbStatPoll,&globalNbPoll,1);
      if ( _nbStatPoll < globalNbPoll ){
          std::cout << "syncStat sync" << rank << std::endl;
          MPI_Wait(&_requestStat, MPI_STATUS_IGNORE);
          sendStat();
      }
      std::cout << "syncStat Wait" << rank << std::endl;
      MPI_Wait(&_requestStat, MPI_STATUS_IGNORE);
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