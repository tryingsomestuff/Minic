#include "distributed.h"

#include "searcher.hpp"
#include "smp.hpp"

#ifdef WITH_MPI

#include <memory>
#include <mutex>

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "stats.hpp"

namespace Distributed{
   int worldSize;
   int rank;
   std::string name;

   MPI_Comm _commTT    = MPI_COMM_NULL;
   MPI_Comm _commStat  = MPI_COMM_NULL;
   MPI_Comm _commInput = MPI_COMM_NULL;
   MPI_Comm _commStop  = MPI_COMM_NULL;
   
   MPI_Request _requestTT    = MPI_REQUEST_NULL;
   MPI_Request _requestStat  = MPI_REQUEST_NULL;
   MPI_Request _requestInput = MPI_REQUEST_NULL;

   MPI_Win _winPtrStop;

   uint64_t _nbStatPoll;

   std::array<Counter,Stats::sid_maxid> _countersBufSend; 
   std::array<Counter,Stats::sid_maxid> _countersBufRecv[2]; 
   unsigned char _doubleBufferStatParity = 0;

   const unsigned long long int _ttBufSize = 1024;
   unsigned long long int _ttCurPos;
   const DepthType _ttMinDepth = 3;
   std::vector<EntryHash> _ttBufSend[2];
   std::vector<EntryHash> _ttBufRecv;
   unsigned char _doubleBufferTTParity = 0;    
   std::mutex _ttMutex;
   std::atomic<bool> _ttSending;

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

      // buffer size depends on worldsize
      _ttBufSend[0].resize(_ttBufSize);
      _ttBufSend[1].resize(_ttBufSize);
      _ttBufRecv.resize(worldSize*_ttBufSize);
      _ttCurPos = 0ull;

      _ttSending = false;

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

   // classic busy wait
   void sync(MPI_Comm & com){
       if (worldSize < 2) return;
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "syncing";
       checkError(MPI_Barrier(com));
       if ( isMainProcess() ) Logging::LogIt(Logging::logDebug) << "...done";
   }

   // "softer" wait on other process
   void waitRequest(MPI_Request & req){
       if (worldSize < 2) return;
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
      if (worldSize < 2) return;
      _countersBufSend.fill(0ull);
      _countersBufRecv[0].fill(0ull);
      _countersBufRecv[1].fill(0ull);
   }

   void sendStat(){
      if (worldSize < 2) return;
      // launch an async reduce
      asyncAllReduceSum(_countersBufSend.data(),_countersBufRecv[_doubleBufferStatParity%2].data(),Stats::sid_maxid,_requestStat,_commStat);
      ++_nbStatPoll;
   }

   void pollStat(){
      if (worldSize < 2) return;
      int flag;
      checkError(MPI_Test(&_requestStat, &flag, MPI_STATUS_IGNORE));
      // if previous comm is done, launch another one
      if (flag){ 
         ++_doubleBufferStatParity;
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
      if (worldSize < 2) return;
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

      _countersBufRecv[(_doubleBufferStatParity+1)%2]=_countersBufRecv[(_doubleBufferStatParity)%2];

      //showStat(); // debug
      sync(_commStat);
   }

   void showStat(){
      // show reduced stats ///@todo an accessor for those data used instead of local ones...
      for(size_t k = 0 ; k < Stats::sid_maxid ; ++k){
         Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << _countersBufRecv[(_doubleBufferStatParity+1)%2][k];
         //std::cout << "All rank reduced: " << rank << " " << Stats::Names[k] << " " << _countersBufRecv[k] << std::endl;
      } 
   }

   Counter counter(Stats::StatId id){
      //because doubleBuffer is by design not initially filled, we return locals data at the beginning)
      const Counter global = _countersBufRecv[(_doubleBufferStatParity+1)%2][id];
      return global != 0 ? global : ThreadPool::instance().counter(id,true);
   }

   void setEntry(const Hash h, const TT::Entry & e){
      if (worldSize < 2) return;
      // do not share entry near leaf, their are changing to quickly
      if ( e.d > _ttMinDepth){ 
         if ( _ttMutex.try_lock()){ // this can be called from multiple threads of this process !
            _ttBufSend[_doubleBufferTTParity%2][_ttCurPos++] = {h,e};
            if (_ttCurPos == _ttBufSize){ // buffer is full
               ++_doubleBufferTTParity; // switch buffer
               //std::cout << "buffer full" << std::endl;
               _ttCurPos = 0ull; // reset index
               // send data
               if ( ! _ttSending.load() ){
                  _ttSending.store(true);
                  //std::cout << "sending data" << std::endl;
                  asyncAllGather(_ttBufSend[(_doubleBufferTTParity+1)%2].data(),_ttBufRecv.data(),_ttBufSize*sizeof(EntryHash),_requestTT,_commTT);
               }
            }
            _ttMutex.unlock();
         } // end of lock
      } // depth ok

      // treat sent data
      int flag;
      checkError(MPI_Test(&_requestTT, &flag, MPI_STATUS_IGNORE));
      // if previous comm is done, use the data
      if (flag && _ttSending.load()){ 
          //std::cout << "buffer received" << std::endl;
          for (const auto & i: _ttBufRecv ){
              TT::_setEntry(i.h,i.e); // always replace (favour data from other process)
          }
          _ttSending.store(false);
      }
   }
}

#else

namespace Distributed{
   int worldSize = 1;
   int rank = 0;

   DummyType _commTT    = 0;
   DummyType _commStat  = 0;
   DummyType _commInput = 0;
   DummyType _commStop  = 0;

   DummyType _requestTT    = 0;
   DummyType _requestStat  = 0;
   DummyType _requestInput = 0;

   DummyType _winPtrStop = 0;

   Counter counter(Stats::StatId id){
      return ThreadPool::instance().counter(id,true);
   }
   
}

#endif