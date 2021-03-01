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
   MPI_Request _requestStop  = MPI_REQUEST_NULL;

   MPI_Comm _commTT    = MPI_COMM_NULL;
   MPI_Comm _commStat  = MPI_COMM_NULL;
   MPI_Comm _commInput = MPI_COMM_NULL;
   MPI_Comm _commStop  = MPI_COMM_NULL;
 

   uint64_t _nbStatPoll;

   namespace{
      std::array<Counter,Stats::sid_maxid> _countersBufSend; 
      std::array<Counter,Stats::sid_maxid> _countersBufRecv; 
      
      uint64_t _sendCallCountStat;

      bool _stopFlag; ///@todo

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
   
      MPI_Comm_dup(MPI_COMM_WORLD, &_commTT);
      MPI_Comm_dup(MPI_COMM_WORLD, &_commStat);
      MPI_Comm_dup(MPI_COMM_WORLD, &_commInput);
      MPI_Comm_dup(MPI_COMM_WORLD, &_commStop);

      _sendCallCountStat = 0ull;
   }

   void finalize(){

      MPI_Comm_free(&_commTT);
      MPI_Comm_free(&_commStat);
      MPI_Comm_free(&_commInput);
      MPI_Comm_free(&_commStop);

      MPI_Finalize();
   }

   bool isMainProcess(){
       return rank == 0;
   }

   // barrier
   void sync(MPI_Comm & com){
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "syncing";
       MPI_Barrier(com);  
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
      // launch an async reduce
      asyncAllReduceSum(_countersBufSend.data(),_countersBufRecv.data(),Stats::sid_maxid,_requestStat,_commStat);
      ++_nbStatPoll;
   }

   void pollStat(bool display){
      int flag;
      MPI_Test(&_requestStat, &flag, MPI_STATUS_IGNORE);
      // if previous comm is done, launch another one
      if (flag){ 
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
      // wait for equilibrium
      uint64_t globalNbPoll = 0ull;
      allReduceMax(&_nbStatPoll,&globalNbPoll,1,_commStat);
      if ( _nbStatPoll < globalNbPoll ){
          MPI_Wait(&_requestStat, MPI_STATUS_IGNORE);
          sendStat();
      }

      // final resync
      MPI_Wait(&_requestStat, MPI_STATUS_IGNORE);
      pollStat();
      MPI_Wait(&_requestStat, MPI_STATUS_IGNORE);

      // show reduced stats ///@todo an accessor for those data used instead of local ones...
      for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
         //Logging::LogIt(Logging::logInfo) << "All rank reduced: " << Stats::Names[k] << " " << _countersBufRecv[k];
         std::cout << "All rank reduced: " << rank << " " << Stats::Names[k] << " " << _countersBufRecv[k] << std::endl;
      }      
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