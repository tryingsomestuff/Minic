#include "distributed.h"

#include "searcher.hpp"
#include "threading.hpp"

#ifdef WITH_MPI

#include <memory>
#include <mutex>

#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "stats.hpp"

namespace Distributed {
int         worldSize;
int         rank;
std::string name;

MPI_Comm _commTT    = MPI_COMM_NULL;
MPI_Comm _commTT2   = MPI_COMM_NULL;
MPI_Comm _commStat  = MPI_COMM_NULL;
MPI_Comm _commStat2 = MPI_COMM_NULL;
MPI_Comm _commInput = MPI_COMM_NULL;
MPI_Comm _commStop  = MPI_COMM_NULL;
MPI_Comm _commMove  = MPI_COMM_NULL;

MPI_Request _requestTT    = MPI_REQUEST_NULL;
MPI_Request _requestStat  = MPI_REQUEST_NULL;
MPI_Request _requestInput = MPI_REQUEST_NULL;
MPI_Request _requestMove  = MPI_REQUEST_NULL;

MPI_Win _winStop;

std::array<Counter, Stats::sid_maxid> _countersBufSend;
std::array<Counter, Stats::sid_maxid> _countersBufRecv[2];
uint8_t _doubleBufferStatParity;
uint64_t _nbStatPoll;

const uint64_t  _ttBufSize = 32;
uint64_t        _ttCurPos;
const DepthType _ttMinDepth = 3;
std::vector<EntryHash> _ttBufSend[2];
std::vector<EntryHash> _ttBufRecv;
uint8_t     _doubleBufferTTParity;
std::mutex  _ttMutex;
uint64_t    _nbTTTransfert;

void checkError(int err) {
   if (err != MPI_SUCCESS) { Logging::LogIt(Logging::logFatal) << "MPI error"; }
}

void init() {
   //MPI_Init(NULL, NULL);
   int provided;
   checkError(MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided));
   checkError(MPI_Comm_size(MPI_COMM_WORLD, &worldSize));
   checkError(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
   char processor_name[MPI_MAX_PROCESSOR_NAME];
   int  name_len;
   checkError(MPI_Get_processor_name(processor_name, &name_len));
   name = processor_name;

   if (!isMainProcess()) DynamicConfig::minOutputLevel = Logging::logOff;

   if (provided < MPI_THREAD_MULTIPLE) { Logging::LogIt(Logging::logFatal) << "The threading support level is lesser than needed"; }

   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commTT));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commTT2));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commStat));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commStat2));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commInput));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commStop));
   checkError(MPI_Comm_dup(MPI_COMM_WORLD, &_commMove));

   _nbStatPoll             = 0ull;
   _doubleBufferStatParity = 0;

   // buffer size depends on worldsize
   _ttBufSend[0].resize(_ttBufSize);
   _ttBufSend[1].resize(_ttBufSize);
   _ttBufRecv.resize(worldSize * _ttBufSize);
   _ttCurPos      = 0ull;
   _nbTTTransfert = 0ull;
   _doubleBufferTTParity = 0;
}

void lateInit() {
   if (moreThanOneProcess()) {
      checkError(MPI_Win_create(&ThreadPool::instance().main().stopFlag, sizeof(bool), sizeof(bool), MPI_INFO_NULL, _commStop, &_winStop));
      checkError(MPI_Win_fence(0, _winStop));
   }
}

void finalize() {
   checkError(MPI_Comm_free(&_commTT));
   checkError(MPI_Comm_free(&_commTT2));
   checkError(MPI_Comm_free(&_commStat));
   checkError(MPI_Comm_free(&_commStat2));
   checkError(MPI_Comm_free(&_commInput));
   checkError(MPI_Comm_free(&_commStop));
   checkError(MPI_Comm_free(&_commMove));

   if (moreThanOneProcess()) checkError(MPI_Win_free(&_winStop));

   checkError(MPI_Finalize());
}

bool isMainProcess() { return rank == 0; }

bool moreThanOneProcess() {
   return worldSize > 1;
}

// classic busy wait
void sync(MPI_Comm& com, const std::string& msg) {
   if (!moreThanOneProcess()) return;
   Logging::LogIt(Logging::logDebug) << "syncing: " << msg;
   checkError(MPI_Barrier(com));
   Logging::LogIt(Logging::logDebug) << "...done";
}

// "softer" wait on other process
void waitRequest(MPI_Request& req) {
   if (!moreThanOneProcess()) return;
   // don't rely on MPI to do a "passive wait", most implementations are doing a busy-wait, so use 100% cpu
   if (Distributed::isMainProcess()) { checkError(MPI_Wait(&req, MPI_STATUS_IGNORE)); }
   else {
      while (true) {
         int flag;
         checkError(MPI_Test(&req, &flag, MPI_STATUS_IGNORE));
         if (flag) break;
         else
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
   }
}

void initStat() {
   if (!moreThanOneProcess()) return;
   _countersBufSend.fill(0ull);
   _countersBufRecv[0].fill(0ull);
   _countersBufRecv[1].fill(0ull);
}

void sendStat() {
   if (!moreThanOneProcess()) return;
   // launch an async reduce
   asyncAllReduceSum(_countersBufSend.data(), _countersBufRecv[_doubleBufferStatParity % 2].data(), Stats::sid_maxid, _requestStat, _commStat);
   ++_nbStatPoll;
}

void pollStat() { // only called from main thread
   if (!moreThanOneProcess()) return;
   int flag;
   checkError(MPI_Test(&_requestStat, &flag, MPI_STATUS_IGNORE));
   // if previous comm is done, launch another one
   if (flag) {
      ++_doubleBufferStatParity;
      // gather stats from all local threads
      for (size_t k = 0; k < Stats::sid_maxid; ++k) {
         _countersBufSend[k] = 0ull;
         for (auto& it : ThreadPool::instance()) { _countersBufSend[k] += it->stats.counters[k]; }
      }
      sendStat();
   }
}

// get all rank to a common synchronous state at the end of search
void syncStat() { // only called from main thread
   if (!moreThanOneProcess()) return;
   // wait for equilibrium
   uint64_t globalNbPoll = 0ull;
   allReduceMax(&_nbStatPoll, &globalNbPoll, 1, _commStat2);
   if (_nbStatPoll < globalNbPoll) {
      checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));
      sendStat();
   }

   // final resync
   checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));
   pollStat();
   checkError(MPI_Wait(&_requestStat, MPI_STATUS_IGNORE));

   _countersBufRecv[(_doubleBufferStatParity + 1) % 2] = _countersBufRecv[(_doubleBufferStatParity) % 2];

   //showStat(); // debug
   sync(_commStat, __PRETTY_FUNCTION__);
}

void showStat() {
   // show reduced stats
   for (size_t k = 0; k < Stats::sid_maxid; ++k) {
      Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << _countersBufRecv[(_doubleBufferStatParity + 1) % 2][k];
   }
}

Counter counter(Stats::StatId id) {
   // because doubleBuffer is by design not initially filled, we return locals data at the beginning)
   const Counter global = _countersBufRecv[(_doubleBufferStatParity + 1) % 2][id];
   return global != 0 ? global : ThreadPool::instance().counter(id, true);
}

void setEntry(const Hash h, const TT::Entry& e) {
   if (!moreThanOneProcess()) return;
   DEBUGCOUT("set entry")
   // do not share entry near leaf, their are changing to quickly
   if (e.d > _ttMinDepth) {
      DEBUGCOUT("depth ok")
      if (_ttMutex.try_lock()) { // this can be called from multiple threads of this process !
         DEBUGCOUT("lock ok")
         _ttBufSend[_doubleBufferTTParity % 2][_ttCurPos++] = {h, e};
         if (_ttCurPos == _ttBufSize) { // buffer is full
            DEBUGCOUT("buffer full")
            _ttCurPos = 0ull; // reset index
            // if previous comm is done then use data and launch another one
            int flag;
            checkError(MPI_Test(&_requestTT, &flag, MPI_STATUS_IGNORE));
            if (flag) {
               // receive previous data
#ifdef DEBUG_DISTRIBUTED
               static uint64_t received = 0;
#endif
               DEBUGCOUT("buffer received " +std::to_string(received++))
               for (const auto& i : _ttBufRecv) {
                  TT::_setEntry(i.h, i.e); // always replace (favour data from other process)
               }
               // send new ones
               ++_doubleBufferTTParity; // switch buffer
               DEBUGCOUT("sending data " + std::to_string(_nbTTTransfert))
               asyncAllGather(_ttBufSend[(_doubleBufferTTParity + 1) % 2].data(), _ttBufRecv.data(), _ttBufSize * sizeof(EntryHash), _requestTT,
                              _commTT);
               ++_nbTTTransfert;
            }
            // else we reuse the same buffer and loosing current data
            /*else{
                  DEBUGCOUT("previous comm not done, skipping")
            }*/
         }
         _ttMutex.unlock();
      } // end of lock
   }    // depth ok
}

// get all rank to a common synchronous state at the end of search
void syncTT() { // only called from main thread
   if (!moreThanOneProcess()) return;
   // wait for equilibrium
   uint64_t globalNbPoll = 0ull;
   allReduceMax(&_nbTTTransfert, &globalNbPoll, 1, _commTT2);
   DEBUGCOUT("sync TT " + std::to_string(_nbTTTransfert) + " " + std::to_string(globalNbPoll))
   if (_nbTTTransfert < globalNbPoll) {
      DEBUGCOUT("sync TT middle wait")
      checkError(MPI_Wait(&_requestTT, MPI_STATUS_IGNORE));
      // we don't really care which buffer is sent here
      asyncAllGather(_ttBufSend[(_doubleBufferTTParity + 1) % 2].data(), _ttBufRecv.data(), _ttBufSize * sizeof(EntryHash), _requestTT, _commTT);
      ++_nbTTTransfert;
   }
   // final resync
   DEBUGCOUT("sync TT final wait")
   checkError(MPI_Wait(&_requestTT, MPI_STATUS_IGNORE));
   DEBUGCOUT("sync TT final wait done 1")
   sync(_commTT, __PRETTY_FUNCTION__);
   DEBUGCOUT("sync TT final wait done 2")
}
} // namespace Distributed

#else

namespace Distributed {
int worldSize = 1;
int rank      = 0;

DummyType _commTT    = 0;
DummyType _commTT2   = 0;
DummyType _commStat  = 0;
DummyType _commStat2 = 0;
DummyType _commInput = 0;
DummyType _commStop  = 0;
DummyType _commMove  = 0;

DummyType _requestTT    = 0;
DummyType _requestStat  = 0;
DummyType _requestInput = 0;
DummyType _requestMove  = 0;

DummyType _winStop = 0;

Counter counter(Stats::StatId id) { return ThreadPool::instance().counter(id, true); }

} // namespace Distributed

#endif
