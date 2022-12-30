#pragma once

#include "definition.hpp"
#include "evalDef.hpp"
#include "material.hpp"
#include "score.hpp"
#include "stats.hpp"
#include "tables.hpp"
#include "threading.hpp"

/*!
 * Searcher struct store all the information needed by a search thread
 * Implements main search function (driver, pvs, qsearch, see, display to GUI, ...)
 * This was inspired from the former thread Stockfish style management
 *
 * Many things are templates here, so other hpp file are included at the bottom of this one.
 */
struct Searcher {
   bool stopFlag = true;

   struct StackData {
      Position  p;
      Hash      h      = nullHash;
      //EvalData  data;
      ScoreType eval   = 0;
      MiniMove  threat = INVALIDMINIMOVE;
   };

   struct PVSData{
      // CMH
      CMHPtrArray cmhPtr;

      // move counting
      int validMoveCount      {0};
      int validQuietMoveCount {0};
      int validNonPrunedCount {0};

      // from call
      ScoreType alpha {0};
      ScoreType beta  {0};
      bool isInCheck              {false};
      bool cutNode                {false};
      bool pvnode                 {false};
      bool rootnode               {false};
      bool theirTurn              {false};
      bool withoutSkipMove        {true};
      bool previousMoveIsNullMove {false};

      // tt related stuff
      DepthType marginDepth       {0};
      TT::Bound bound             {TT::B_none};
      bool ttMoveIsCapture        {false};
      bool ttMoveSingularExt      {false};
      bool ttMoveTried            {false};
      bool validTTmove            {false};
      bool ttHit                  {false};
      bool ttPV                   {false};
      bool ttIsCheck              {false};
      bool formerPV               {false};
      bool evalScoreIsHashScore   {false};

      // context
      bool improving              {false};
      bool isEmergencyDefence     {false};
      bool isEmergencyAttack      {false};
      bool isBoomingAttack        {false};
      bool isBoomingDefend        {false};
      bool isMoobingAttack        {false};
      bool isMoobingDefend        {false};
      bool isNotPawnEndGame       {false};
      bool lessZugzwangRisk       {false};
      bool isKnownEndGame         {false};

      // situation
      bool bestMoveIsCheck        {false};
      bool mateThreat             {false};
      bool alphaUpdated           {false};

      // current move
      bool isCheck                {false};
      bool isQuiet                {false};
      bool isAdvancedPawnPush     {false};
      bool earlyMove              {false};
      bool isTTMove               {false};

      // pruning and extension triggers
      bool futility               {false};
      bool lmp                    {false};
      bool historyPruning         {false};
      bool capHistoryPruning      {false};
      bool CMHPruning             {false};
      bool BMextension            {false};
   };

   MoveDifficultyUtil::MoveDifficulty moveDifficulty = MoveDifficultyUtil::MD_std;
   MoveDifficultyUtil::PositionEvolution positionEvolution = MoveDifficultyUtil::PE_std;

   TimeType currentMoveMs  = 777;
   TimeType getCurrentMoveMs()const; // use this (and not the variable) to take emergency time into account !

   array1d<StackData, MAX_PLY> stack;
   [[nodiscard]] bool isBooming(uint16_t halfmove); // from stack
   [[nodiscard]] bool isMoobing(uint16_t halfmove); // from stack

   mutable Stats stats;

   // beta cut more refined statistics are possible and gather using this function
   void updateStatBetaCut(const Position & p, const Move m, const DepthType height);

   FORCE_FINLINE void updatePV(PVList& pv, const Move& m, const PVList& childPV) {
      stats.incr(Stats::sid_PVupdate);
      pv.clear();
      pv.emplace_back(m);
      std::ranges::copy(childPV, std::back_inserter(pv));
   }

   void displayStats() const {
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
   bool      isStoppableCoSearcher = false; 
   DepthType height_        = 0; ///@todo use this everywhere, instead of passing height in pvs and qsearch call ?

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

   std::tuple<DepthType, DepthType, DepthType> depthPolicy(const Position & p, DepthType depth, DepthType height, Move m, const PVSData & pvsData, const EvalData & evalData, ScoreType evalScore, DepthType extensions, bool isReductible) const;

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

   explicit Searcher(size_t n);
   ~Searcher();
   // non copyable
   Searcher(const Searcher&) = delete;
   Searcher(const Searcher&&) = delete;
   Searcher& operator=(const Searcher&) = delete;
   Searcher& operator=(const Searcher&&) = delete;

   void                            setData(const ThreadData& d);
   [[nodiscard]] const ThreadData& getData() const;
   [[nodiscard]] ThreadData&       getData();
   [[nodiscard]] const SearchData& getSearchData() const;
   [[nodiscard]] SearchData&       getSearchData();

   static std::atomic<bool> startLock;

   std::chrono::time_point<Clock> startTime;

   [[nodiscard]] bool searching() const;

   struct PawnEntry {
      colored<BitBoard>  pawnTargets   = {emptyBitBoard, emptyBitBoard};
      colored<BitBoard>  holes         = {emptyBitBoard, emptyBitBoard};
      colored<BitBoard>  semiOpenFiles = {emptyBitBoard, emptyBitBoard};
      colored<BitBoard>  passed        = {emptyBitBoard, emptyBitBoard};
      BitBoard           openFiles     = emptyBitBoard;
      EvalScore          score         = {0, 0};
      MiniHash           h             = nullHash;
      colored<ScoreType> danger        = {0, 0};

      FORCE_FINLINE void reset() {
         score            = {0, 0};
         danger[Co_White] = 0;
         danger[Co_Black] = 0;
      }
   };

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
