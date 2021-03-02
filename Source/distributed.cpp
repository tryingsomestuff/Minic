#include "distributed.h"

#ifdef WITH_MPI

#include <memory>

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
 
   MPI_Win _winPtrStop;

   uint64_t _nbStatPoll;

   std::array<Counter,Stats::sid_maxid> _countersBufSend; 
   std::array<Counter,Stats::sid_maxid> _countersBufRecv; 

   bool test;

   void checkError(int err){
      if ( err != MPI_SUCCESS ){
         Logging::LogIt(Logging::logFatal) << "MPI error";
      }
   }

   void init(){
      //MPI_Init(NULL, NULL);
      int provided;
      checkError(MPI_Init_thread(NULL, NULL, MPI_THREAD_FUNNELED, &provided));

      checkError(MPI_Comm_size(MPI_COMM_WORLD, &worldSize));
      checkError(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
      char processor_name[MPI_MAX_PROCESSOR_NAME];
      int name_len;
      checkError(MPI_Get_processor_name(processor_name, &name_len));
      name = processor_name;

      if ( !isMainProcess() ) DynamicConfig::silent = true;
   
      checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commTT));
      checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commStat));
      checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commInput));
      checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commStop));

      _nbStatPoll = 0ull;

   }

   void lateInit(){
      checkError(MPI_Win_create(&ThreadPool::instance().main().stopFlag, sizeof(bool), sizeof(bool), MPI_INFO_NULL, _commStop, &_winPtrStop));
      checkError(MPI_Win_fence(0, _winPtrStop));
   }

   void finalize(){

      checkError(MPI_Comm_free(&_commTT));
      checkError(MPI_Comm_free(&_commStat));
      checkError(MPI_Comm_free(&_commInput));
      checkError(MPI_Comm_free(&_commStop));

      checkError(MPI_Win_free(&_winPtrStop));

      checkError(MPI_Finalize());
   }

   bool isMainProcess(){
       return rank == 0;
   }

   // barrier
   void sync(MPI_Comm & com){
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "syncing";
       checkError(MPI_Barrier(com));
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "...done";
   }

   void waitRequest(MPI_Request & req){
        // don't rely on MPI to do a "passive wait", most implementations are doing a busy-wait, so use 100% cpu
        if (Distributed::isMainProcess()){
            checkError(MPI_Wait(&req, MPI_STATUS_IGNORE));
        }
        else{
            while (true){ 
                int flag;
                checkError(MPI_Test(&req, &flag, MPI_STATUS_IGNORE));
                if (flag) break;
                else std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } 
   }


   void initStat(){
       _countersBufSend.fill(0ull);
       _countersBufRecv.fill(0ull);
   }

   void sendStat(){
      // launch an async reduce
      asyncAllReduceSum(_countersBufSend.data(),_countersBufRecv.data(),Stats::sid_maxid,_requestStat,_commStat);
      ++_nbStatPoll;
   }

   void pollStat(){
      int flag;
      checkError(MPI_Test(&_requestStat, &flag, MPI_STATUS_IGNORE));
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
          checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));
          sendStat();
      }

      // final resync
      checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));
      pollStat();
      checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));

      //showStat(); // debug
      sync(_commStat);
   }

   void showStat(){
      // show reduced stats ///@todo an accessor for those data used instead of local ones...
      for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
         Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << _countersBufRecv[k];
         //std::cout << "All rank reduced: " << rank << " " << Stats::Names[k] << " " << _countersBufRecv[k] << std::endl;
      } 
   }

}

#else
namespace Distributed{
   int worldSize = 0;
   int rank = 0;

   DummyType _commTT    = 0;
   DummyType _commStat  = 0;
   DummyType _commInput = 0;
   DummyType _commStop  = 0;

   DummyType _requestTT    = 0;
   DummyType _requestStat  = 0;
   DummyType _requestInput = 0;

   DummyType _winPtrStop = 0;
}

#endif