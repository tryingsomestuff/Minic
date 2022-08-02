#pragma once

#include "definition.hpp"
#include "evalDef.hpp"
#include "material.hpp"
#include "score.hpp"
#include "stats.hpp"
#include "tables.hpp"
#include "threading.hpp"

#include <optional>

/*!
 * Searcher struct store all the information needed by a search thread
 * Implements main search function (driver, pvs, qsearch, see, display to GUI, ...)
 * This was inspired from the former thread Stockfish style management
 *
 * Many things are templates here, so other hpp file are included at the bottom of this one.
 */
struct Searcher {
   bool stopFlag = true;

   MoveDifficultyUtil::MoveDifficulty moveDifficulty = MoveDifficultyUtil::MD_std;
   TimeType                           currentMoveMs  = 777;

   TimeType getCurrentMoveMs()const; // use this (and not the variable) to take emergency time into account !

   struct StackData {
      Position  p;
      Hash      h      = nullHash;
      //EvalData  data;
      ScoreType eval   = 0;
      MiniMove  threat = INVALIDMINIMOVE;
   };
   std::array<StackData, MAX_PLY> stack;

   mutable Stats stats;

   inline void updatePV(PVList& pv, const Move& m, const PVList& childPV) {
      stats.incr(Stats::sid_PVupdate);
      pv.clear();
      pv.push_back(m);
      std::copy(childPV.begin(), childPV.end(), std::back_inserter(pv));
   }

   inline void displayStats() const {
      for (size_t k = 0; k < Stats::sid_maxid; ++k) {
         Logging::LogIt(Logging::logInfo) << Stats::Names[k] << " " << stats.counters[(Stats::StatId)k];
      }
   }

   std::vector<RootScores> rootScores;

   // used for move ordering
   Move previousBest = INVALIDMOVE;

   KillerT   killerT;
   HistoryT  historyT;
   CounterT  counterT;
   DepthType nullMoveMinPly = 0;
   Color     nullMoveVerifColor = Co_None;
   EvalScore contempt       = 0;
   bool      subSearch      = false;
   bool      isStoppable    = false; 
   DepthType _height        = 0; ///@todo use this everywhere, instead of passing height in pvs and qsearch call ?

#ifdef WITH_GENFILE
   std::ofstream genStream;
   void          writeToGenFile(const Position& p, bool getQuietPos, const ThreadData & d, const std::optional<int> result);
#endif
   std::ofstream pgnStream;

   static Position getQuiet(const Position& p, Searcher* searcher = nullptr, ScoreType* qScore = nullptr);

   static Searcher& getCoSearcher(size_t id);

   void                    getCMHPtr(const unsigned int ply, CMHPtrArray& cmhPtr);
   [[nodiscard]] ScoreType getCMHScore(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr) const;

   [[nodiscard]] bool isCMHGood(const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const;
   [[nodiscard]] bool isCMHBad (const Position& p, const Square from, const Square to, const CMHPtrArray& cmhPtr, const ScoreType threshold) const;

   [[nodiscard]] ScoreType drawScore(const Position& p, DepthType height) const;

   void timeCheck();

   template<bool pvnode>
   [[nodiscard]]
   ScoreType pvs(ScoreType                    alpha,
                 ScoreType                    beta,
                 const Position&              p,
                 DepthType                    depth,
                 DepthType                    height,
                 PVList&                      pv,
                 DepthType&                   seldepth,
                 DepthType                    extensions,
                 bool                         isInCheck,
                 bool                         cutNode,
                 const std::vector<MiniMove>* skipMoves = nullptr);

   [[nodiscard]] ScoreType qsearch(ScoreType       alpha,
                                   ScoreType       beta,
                                   const Position& p,
                                   DepthType       height,
                                   DepthType&      seldepth,
                                   DepthType       qply,
                                   bool            qRoot,
                                   bool            pvnode,
                                   int8_t          isInCheckHint = -1);

   // used for tuning not search !
   [[nodiscard]] ScoreType qsearchNoPruning(ScoreType       alpha,
                                            ScoreType       beta,
                                            const Position& p,
                                            DepthType       height,
                                            DepthType&      seldepth,
                                            PVList*         pv = nullptr);

   [[nodiscard]] static bool SEE_GE(const Position& p, const Move& m, ScoreType threshold);

   [[nodiscard]] static ScoreType SEE(const Position& p, const Move& m);

   void searchDriver(bool postMove = true);

   template<bool isPv = true>
   [[nodiscard]] std::optional<ScoreType> interiorNodeRecognizer(const Position& p, DepthType height) const;

   [[nodiscard]] bool isRep(const Position& p, bool isPv) const;
   [[nodiscard]] bool isMaterialDraw(const Position& p) const;
   [[nodiscard]] bool is50moves(const Position& p, bool afterMoveLoop) const;

   void displayGUI(DepthType          depth,
                   DepthType          seldepth,
                   ScoreType          bestScore,
                   unsigned int       ply,
                   const PVList&      pv,
                   int                multipv,
                   const std::string& mark = "");

   void idleLoop();

   void startThread();

   void wait();

   void searchLauncher();

   [[nodiscard]] size_t id() const;
   [[nodiscard]] bool   isMainThread() const;

   Searcher(size_t n);

   ~Searcher();

   void                            setData(const ThreadData& d);
   [[nodiscard]] const ThreadData& getData() const;
   [[nodiscard]] ThreadData&       getData();
   [[nodiscard]] const SearchData& getSearchData() const;
   [[nodiscard]] SearchData&       getSearchData();

   static std::atomic<bool> startLock;

   std::chrono::time_point<Clock> startTime;

   [[nodiscard]] bool searching() const;

#pragma pack(push, 1)
   struct PawnEntry {
      BitBoard    pawnTargets[2]   = {emptyBitBoard, emptyBitBoard};
      BitBoard    holes[2]         = {emptyBitBoard, emptyBitBoard};
      BitBoard    semiOpenFiles[2] = {emptyBitBoard, emptyBitBoard};
      BitBoard    passed[2]        = {emptyBitBoard, emptyBitBoard};
      BitBoard    openFiles        = emptyBitBoard;
      EvalScore   score            = {0, 0};
      ScoreType   danger[2]        = {0, 0};
      MiniHash    h                = nullHash;
      inline void reset() {
         score            = {0, 0};
         danger[Co_White] = 0;
         danger[Co_Black] = 0;
      }
   };
#pragma pack(pop)

   void initPawnTable();

   void clearPawnTT();

   void clearGame();
   void clearSearch(bool forceHistoryClear = false);

   [[nodiscard]] bool getPawnEntry(Hash h, PawnEntry*& pe);

   void prefetchPawn(Hash h);

  private:
   ThreadData              _data;
   std::mutex              _mutexPV;
   size_t                  _index;
   std::mutex              _mutex;
   std::condition_variable _cv;
   // next two MUST be initialized BEFORE _stdThread
   bool        _exit;
   bool        _searching;
   std::thread _stdThread;
};

#include "searcherDraw.hpp"
#include "searcherPVS.hpp"
