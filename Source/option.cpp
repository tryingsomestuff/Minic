#include "option.hpp"

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "logging.hpp"
#include "opponent.hpp"
#include "searcher.hpp"
#include "searchConfig.hpp"
#include "threading.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"
#endif

namespace Options {

enum KeyType : uint8_t { k_bad = 0, k_bool, k_depth, k_int, k_score, k_ull, k_string };
enum WidgetType : uint8_t { w_check = 0, w_string, w_spin, w_combo, w_button, w_max };
struct KeyBase {
   template<typename T>
   KeyBase(
       KeyType            t,
       WidgetType         w,
       const std::string& k,
       T*                 v,
       const std::function<void(void)>& cb = [] {}):
       type(t), wtype(w), key(k), value((void*)v) {
      callBack = cb;
   }
   template<typename T>
   KeyBase(
       KeyType             t,
       WidgetType          w,
       const std::string&  k,
       T*                  v,
       const T&            _vmin,
       const T&            _vmax,
       const std::function<void(void)>& cb = [] {}):
       type(t), wtype(w), key(k), value((void*)v), vmin(_vmin), vmax(_vmax) {
      callBack = cb;
   }

   KeyType     type;
   WidgetType  wtype;
   std::string key;
   void*       value;
   int         vmin = 0, vmax = 0; // assume int type is covering all the case (string excluded ...)
   bool        hotPlug = false;
   std::function<void(void)> callBack;
};

std::vector<std::string> args;
std::vector<KeyBase>     _keys;

const std::string widgetXboardNames[w_max] = {"check", "string", "spin", "combo", "button"};

KeyBase& GetKey(const std::string& key) {
   bool           keyFound = false;
   static int     dummy    = 0;
   static KeyBase badKey(k_bad, w_button, "bad_default_key", &dummy, 0, 0);
   KeyBase*       keyRef = &badKey;
   for (size_t k = 0; k < _keys.size(); ++k) {
      if (key == _keys[k].key) {
         keyRef   = &_keys[k];
         keyFound = true;
         break;
      }
   }
   if (!keyFound) Logging::LogIt(Logging::logWarn) << "Key not found " << key;
   return *keyRef;
}

int GetValue(const std::string& key) { // assume we can convert to int safely (not valid for string of course !)
   const KeyBase& k = GetKey(key);
   switch (k.type) {
      case k_bool:  return static_cast<int>(*static_cast<bool*>(k.value));
      case k_depth: return static_cast<int>(*static_cast<DepthType*>(k.value));
      case k_int:   return static_cast<int>(*static_cast<int*>(k.value));
      case k_score: return static_cast<int>(*static_cast<ScoreType*>(k.value));
      case k_ull:   return static_cast<int>(*static_cast<uint64_t*>(k.value));
      case k_string:
      case k_bad:
      default:      Logging::LogIt(Logging::logError) << "Bad key type"; return false;
   }
}

std::string GetValueString(const std::string& key) { // the one for string
   const KeyBase& k = GetKey(key);
   if (k.type != k_string) Logging::LogIt(Logging::logError) << "Bad key type";
   return *static_cast<std::string*>(k.value);
}

void displayOptionsDebug() {
   for (auto it = _keys.begin(); it != _keys.end(); ++it)
      if (it->type == k_string)
         Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key) << "\"";
      else if (it->type == k_bool)
         Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " "
                                          << (GetValue(it->key) ? "true" : "false") << "\"";
      else
         Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << static_cast<int>(GetValue(it->key)) << " "
                                          << it->vmin << " " << it->vmax << "\"";
}

void displayOptionsXBoard() {
   for (auto it = _keys.begin(); it != _keys.end(); ++it)
      if (it->type == k_string)
         Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key)
                                         << "\"";
      else if (it->type == k_bool)
         Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << bool(GetValue(it->key))
                                         << "\"";
      else
         Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << static_cast<int>(GetValue(it->key))
                                         << " " << it->vmin << " " << it->vmax << "\"";
}

void displayOptionsUCI() {
   for (auto it = _keys.begin(); it != _keys.end(); ++it)
      if (it->type == k_string)
         Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default "
                                         << GetValueString(it->key);
      else if (it->type == k_bool)
         Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default "
                                         << (GetValue(it->key) ? "true" : "false");
      else
         Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default "
                                         << (int)GetValue(it->key) << " min " << it->vmin << " max " << it->vmax;
}

#define SETVALUE(TYPEIN, TYPEOUT)                        \
   {                                                     \
      TYPEIN v;                                          \
      str >> std::boolalpha >> v;                        \
      *static_cast<TYPEOUT*>(keyRef.value) = (TYPEOUT)v; \
   }                                                     \
   break;
#define SETVALUESTR(TYPEIN, TYPEOUT)                     \
   {                                                     \
      TYPEIN v;                                          \
      getline(str, v);                                   \
      *static_cast<TYPEOUT*>(keyRef.value) = (TYPEOUT)v; \
   }                                                     \
   break;

bool SetValue(const std::string& key, const std::string& val) {
   KeyBase& keyRef = GetKey(key);
   if (!keyRef.hotPlug && ThreadPool::instance().main().searching()) {
      Logging::LogIt(Logging::logError) << "Cannot change " << key << " during a search";
      return false;
   }
   std::string value = val;
   if (keyRef.type == k_bool) {
      if (value == "0") value = "false";
      else if (value == "1")
         value = "true";
   }
   std::stringstream str(value);
   switch (keyRef.type) {
      case k_bool: SETVALUE(bool, bool)
      case k_depth: SETVALUE(int, DepthType)
      case k_int: SETVALUE(int, int)
      case k_score: SETVALUE(int, ScoreType)
      case k_ull: SETVALUE(int, uint64_t)
      case k_string: SETVALUESTR(std::string, std::string)
      case k_bad:
      default: Logging::LogIt(Logging::logError) << "Bad key type"; return false;
   }
   Logging::LogIt(Logging::logInfo) << "Option set " << key << "=" << value;
   if (keyRef.callBack) {
      Logging::LogIt(Logging::logInfo) << "Calling callback for option " << key << "=" << value;
      keyRef.callBack();
   }
   displayOptionsDebug();
   return true;
}

template<typename C>
inline void addOptions(C & coeff){
   for(size_t k = 0; k < coeff.N; ++k){
      _keys.push_back(KeyBase(k_depth, w_spin, coeff.getName(SearchConfig::CNT_minDepth,k), &coeff.minDepth[k]       , DepthType(0)     , DepthType(MAX_DEPTH)   ));
      _keys.push_back(KeyBase(k_depth, w_spin, coeff.getName(SearchConfig::CNT_maxdepth,k), &coeff.maxDepth[k]       , DepthType(0)     , DepthType(MAX_DEPTH)   ));
      _keys.push_back(KeyBase(k_score, w_spin, coeff.getName(SearchConfig::CNT_slopeD,k)  , &coeff.slopeDepth[k]     , ScoreType(-1500) , ScoreType(1500) ));
      _keys.push_back(KeyBase(k_score, w_spin, coeff.getName(SearchConfig::CNT_slopeGP,k) , &coeff.slopeGamePhase[k] , ScoreType(-1500) , ScoreType(1500) ));
      _keys.push_back(KeyBase(k_score, w_spin, coeff.getName(SearchConfig::CNT_init,k)    , &coeff.init[k]           , ScoreType(-1500) , ScoreType(1500) ));
   }
   for(size_t k = 0; k < coeff.M; ++k){
      _keys.push_back(KeyBase(k_score, w_spin, coeff.getName(SearchConfig::CNT_bonus,k)   , &coeff.bonus[k]          , ScoreType(-1500) , ScoreType(1500) ));
   }
}

void registerCOMOptions() { // options exposed to GUI

#ifdef WITH_SEARCH_TUNING
   addOptions(SearchConfig::staticNullMoveCoeff);
   addOptions(SearchConfig::razoringCoeff);
   addOptions(SearchConfig::threatCoeff);
   addOptions(SearchConfig::historyPruningCoeff);
   addOptions(SearchConfig::captureHistoryPruningCoeff);
   addOptions(SearchConfig::futilityPruningCoeff);
   addOptions(SearchConfig::failHighReductionCoeff);
#endif

   _keys.push_back(KeyBase(k_int,   w_spin,  "Level"                       , &DynamicConfig::level                          , (unsigned int)0  , (unsigned int)100 ));
   _keys.push_back(KeyBase(k_bool,  w_check, "nodesBasedLevel"             , &DynamicConfig::nodesBasedLevel                , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "UCI_LimitStrength"           , &DynamicConfig::limitStrength                  , false            , true ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "UCI_Elo"                     , &DynamicConfig::strength                       , (int)500         , (int)2800 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "Hash"                        , &DynamicConfig::ttSizeMb                       , (unsigned int)1  , (unsigned int)256000                , &TT::initTable));
   _keys.push_back(KeyBase(k_int,   w_spin,  "PawnHash"                    , &DynamicConfig::ttPawnSizeMb                   , (unsigned int)1  , (unsigned int)256                   , &ThreadPool::initPawnTables));
   _keys.push_back(KeyBase(k_int,   w_spin,  "Threads"                     , &DynamicConfig::threads                        , (unsigned int)1  , (unsigned int)(MAX_THREADS-1)       , std::bind(&ThreadPool::setup, &ThreadPool::instance())));
   _keys.push_back(KeyBase(k_bool,  w_check, "UCI_Chess960"                , &DynamicConfig::FRC                            , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "Ponder"                      , &DynamicConfig::UCIPonder                      , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "MateFinder"                  , &DynamicConfig::mateFinder                     , false            , true ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "MultiPV"                     , &DynamicConfig::multiPV                        , (unsigned int)1  , (unsigned int)4 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "RandomOpen"                  , &DynamicConfig::randomOpen                     , (unsigned int)0  , (unsigned int)100 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "MinMoveOverHead"             , &DynamicConfig::moveOverHead                   , (unsigned int)10 , (unsigned int)1000 ));
   _keys.push_back(KeyBase(k_score, w_spin,  "Contempt"                    , &DynamicConfig::contempt                       , (ScoreType)-50   , (ScoreType)50));
   _keys.push_back(KeyBase(k_score, w_spin,  "ContemptMG"                  , &DynamicConfig::contemptMG                     , (ScoreType)-50   , (ScoreType)50));
   _keys.push_back(KeyBase(k_bool,  w_check, "Armageddon"                  , &DynamicConfig::armageddon                     , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "WDL_display"                 , &DynamicConfig::withWDL                        , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "bongCloud"                   , &DynamicConfig::bongCloud                      , false            , true ));
   _keys.push_back(KeyBase(k_bool,  w_check, "anarchy"                     , &DynamicConfig::anarchy                        , false            , true ));

#ifdef WITH_SYZYGY
   _keys.push_back(KeyBase(k_string,w_string,"SyzygyPath"                  , &DynamicConfig::syzygyPath                                                                              , &SyzygyTb::initTB));
#endif

#ifdef WITH_NNUE
   _keys.push_back(KeyBase(k_string,w_string,"NNUEFile"                    , &DynamicConfig::NNUEFile                                                                                , &NNUEWrapper::init));
   _keys.push_back(KeyBase(k_bool,  w_check, "forceNNUE"                   , &DynamicConfig::forceNNUE                      , false            , true                              ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "NNUEScaling"                 , &DynamicConfig::NNUEScaling                    , (int)32          , (int)256 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "NNUEThreshold"               , &DynamicConfig::NNUEThreshold                  , (int)-1500       , (int)1500 ));
#endif

#ifdef WITH_GENFILE
   _keys.push_back(KeyBase(k_bool,  w_check, "GenFen"                      , &DynamicConfig::genFen                         , false            , true ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "GenFenDepth"                 , &DynamicConfig::genFenDepth                    , (unsigned int)2  , (unsigned int)40 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "GenFenDepthEG"               , &DynamicConfig::genFenDepthEG                  , (unsigned int)2  , (unsigned int)40 ));
   _keys.push_back(KeyBase(k_int,   w_spin,  "RandomPly"                   , &DynamicConfig::randomPly                      , (unsigned int)0  , (unsigned int)40 ));
#endif

   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleAttack"                 , &DynamicConfig::styleAttack                    , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleComplexity"             , &DynamicConfig::styleComplexity                , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleDevelopment"            , &DynamicConfig::styleDevelopment               , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleMaterial"               , &DynamicConfig::styleMaterial                  , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleMobility"               , &DynamicConfig::styleMobility                  , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StylePositional"             , &DynamicConfig::stylePositional                , (int)0   , (int)100                                    , &EvalFeatures::callBack));
   _keys.push_back(KeyBase(k_int,   w_spin,  "StyleForwardness"            , &DynamicConfig::styleForwardness               , (int)0   , (int)100                                    , &EvalFeatures::callBack));

   _keys.push_back(KeyBase(k_string, w_string,"UCI_Opponent"               , &DynamicConfig::opponent                                                                                /*, &Opponent::init*/));
   _keys.push_back(KeyBase(k_int,    w_spin,  "UCI_RatingAdv"              , &DynamicConfig::ratingAdv                      , (int)-10000, (int)10000                                , &Opponent::ratingReceived));

#ifdef WITH_SEARCH_TUNING

   _keys.push_back(KeyBase(k_depth, w_spin, "aspirationMinDepth"                , &SearchConfig::aspirationMinDepth                  , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "aspirationInit"                    , &SearchConfig::aspirationInit                      , ScoreType(0)    , ScoreType(30)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "aspirationDepthInit"               , &SearchConfig::aspirationDepthInit                 , ScoreType(-150) , ScoreType(150)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "aspirationDepthCoef"               , &SearchConfig::aspirationDepthCoef                 , ScoreType(-10)  , ScoreType(10)       ));

   _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin0"                  , &SearchConfig::qfutilityMargin[0]                  , ScoreType(-200) , ScoreType(1500)     ));
   _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin1"                  , &SearchConfig::qfutilityMargin[1]                  , ScoreType(-200) , ScoreType(1500)     ));

   _keys.push_back(KeyBase(k_depth, w_spin, "nullMoveMinDepth"                  , &SearchConfig::nullMoveMinDepth                    , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "nullMoveVerifDepth"                , &SearchConfig::nullMoveVerifDepth                  , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "nullMoveMargin"                    , &SearchConfig::nullMoveMargin                      , ScoreType(-500) , ScoreType(500)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "nullMoveMargin2"                   , &SearchConfig::nullMoveMargin2                     , ScoreType(-500) , ScoreType(500)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "nullMoveDynamicDivisor"            , &SearchConfig::nullMoveDynamicDivisor              , ScoreType(1)    , ScoreType(1500)     ));

   _keys.push_back(KeyBase(k_depth, w_spin, "CMHMaxDepth0"                      , &SearchConfig::CMHMaxDepth[0]                      , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "CMHMaxDepth1"                      , &SearchConfig::CMHMaxDepth[1]                      , DepthType(0)    , DepthType(30)       ));
   //_keys.push_back(KeyBase(k_score, w_spin, "randomAggressiveReductionFactor"   , &SearchConfig::randomAggressiveReductionFactor     , ScoreType(-10)  , ScoreType(10)       ));

   _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth"                       , &SearchConfig::iidMinDepth                         , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth2"                      , &SearchConfig::iidMinDepth2                        , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth3"                      , &SearchConfig::iidMinDepth3                        , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "probCutMinDepth"                   , &SearchConfig::probCutMinDepth                     , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_int  , w_spin, "probCutMaxMoves"                   , &SearchConfig::probCutMaxMoves                     , 0               , 30                  ));
   _keys.push_back(KeyBase(k_score, w_spin, "probCutMargin"                     , &SearchConfig::probCutMargin                       , ScoreType(-500) , ScoreType(1500)     ));

   _keys.push_back(KeyBase(k_score, w_spin, "seeCaptureFactor"                  , &SearchConfig::seeCaptureFactor                    , ScoreType(0)    , ScoreType(1500)     ));
   _keys.push_back(KeyBase(k_score, w_spin, "seeCapDangerDivisor"               , &SearchConfig::seeCapDangerDivisor                 , ScoreType(1)    , ScoreType(32)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "seeQuietFactor"                    , &SearchConfig::seeQuietFactor                      , ScoreType(0)    , ScoreType(1500)     ));
   _keys.push_back(KeyBase(k_score, w_spin, "seeQuietDangerDivisor"             , &SearchConfig::seeQuietDangerDivisor               , ScoreType(1)    , ScoreType(32)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "seeQThreshold"                     , &SearchConfig::seeQThreshold                       , ScoreType(-200) , ScoreType(200)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "betaMarginDynamicHistory"          , &SearchConfig::betaMarginDynamicHistory            , ScoreType(0)    , ScoreType(1500)     ));

   _keys.push_back(KeyBase(k_depth, w_spin, "lmrMinDepth"                       , &SearchConfig::lmrMinDepth                         , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_depth, w_spin, "singularExtensionDepth"            , &SearchConfig::singularExtensionDepth              , DepthType(0)    , DepthType(30)       ));
   _keys.push_back(KeyBase(k_int  , w_spin, "lmrCapHistoryFactor"               , &SearchConfig::lmrCapHistoryFactor                 , 1               , 64                  ));

   _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitPruning"                , &SearchConfig::dangerLimitPruning                  , ScoreType(0)    , ScoreType(65)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitForwardPruning"         , &SearchConfig::dangerLimitForwardPruning           , ScoreType(0)    , ScoreType(65)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitReduction"              , &SearchConfig::dangerLimitReduction                , ScoreType(0)    , ScoreType(65)       ));
   _keys.push_back(KeyBase(k_score, w_spin, "dangerDivisor"                     , &SearchConfig::dangerDivisor                       , ScoreType(-50)  , ScoreType(256)      ));

   _keys.push_back(KeyBase(k_score, w_spin, "failLowRootMargin"                 , &SearchConfig::failLowRootMargin                   , ScoreType(-200) , ScoreType(1500)     ));

   _keys.push_back(KeyBase(k_score, w_spin, "deltaBadMargin"                    , &SearchConfig::deltaBadMargin                      , ScoreType(0)    , ScoreType(500)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "deltaBadSEEThreshold"              , &SearchConfig::deltaBadSEEThreshold                , ScoreType(-200) , ScoreType(200)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "deltaGoodMargin"                   , &SearchConfig::deltaGoodMargin                     , ScoreType(0)    , ScoreType(500)      ));
   _keys.push_back(KeyBase(k_score, w_spin, "deltaGoodSEEThreshold"             , &SearchConfig::deltaGoodSEEThreshold               , ScoreType(0)    , ScoreType(1000)     ));

   ///@todo more ...
#endif

#ifdef WITH_PIECE_TUNING
   _keys.push_back(KeyBase(k_score, w_spin, "PawnValueMG"   , &Values  [P_wp+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "PawnValueEG"   , &ValuesEG[P_wp+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "PawnValueGP"   , &ValuesGP[P_wp+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));

   _keys.push_back(KeyBase(k_score, w_spin, "KnightValueMG" , &Values  [P_wn+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "KnightValueEG" , &ValuesEG[P_wn+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "KnightValueGP" , &ValuesGP[P_wn+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));

   _keys.push_back(KeyBase(k_score, w_spin, "BishopValueMG" , &Values  [P_wb+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "BishopValueEG" , &ValuesEG[P_wb+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "BishopValueGP" , &ValuesGP[P_wb+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));

   _keys.push_back(KeyBase(k_score, w_spin, "RookValueMG"   , &Values  [P_wr+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "RookValueEG"   , &ValuesEG[P_wr+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "RookValueGP"   , &ValuesGP[P_wr+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));

   _keys.push_back(KeyBase(k_score, w_spin, "QueenValueMG"  , &Values  [P_wq+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "QueenValueEG"  , &ValuesEG[P_wq+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
   _keys.push_back(KeyBase(k_score, w_spin, "QueenValueGP"  , &ValuesGP[P_wq+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
#endif

}

// load command line args in memory
void readOptions(int argc, char** argv) {
   for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
}

// get option from command line
void initOptions(int argc, char** argv) {
#define GETOPT(name, type)              Options::getOption<type>(DynamicConfig::name, #name);
#define GETOPTVAR(name, variable, type) Options::getOption<type>(DynamicConfig::variable, #name);

   registerCOMOptions();
   readOptions(argc, argv);

   GETOPT(minOutputLevel, int) // first to be read !
   GETOPT(debugMode, bool)
   GETOPT(disableTT, bool)
   GETOPT(debugFile, std::string)
   GETOPT(ttSizeMb, unsigned int)
   GETOPT(ttPawnSizeMb, unsigned int)
   GETOPT(contempt, ScoreType)
   GETOPT(FRC, bool)
   GETOPT(DFRC, bool)
   GETOPT(threads, unsigned int)
   GETOPT(mateFinder, bool)
   GETOPT(fullXboardOutput, bool)
   GETOPT(level, unsigned int)
   GETOPT(multiPV, unsigned int)
   GETOPT(randomOpen, unsigned int)
   GETOPT(limitStrength, bool)
   GETOPT(nodesBasedLevel, bool)
   GETOPT(strength, int)
   GETOPT(moveOverHead, unsigned int)
   GETOPT(armageddon, bool)
   GETOPT(withWDL, bool)
   GETOPT(bongCloud, bool)
   GETOPT(anarchy, bool)

#ifdef WITH_SYZYGY
   GETOPT(syzygyPath, std::string)
#endif

#ifdef WITH_NNUE
   GETOPT(NNUEFile, std::string)
   GETOPT(forceNNUE, bool)
   GETOPT(NNUEScaling, int)
   GETOPT(NNUEThreshold, int)
#endif

#ifdef WITH_GENFILE
   GETOPT(genFen, bool)
   GETOPT(genFenDepth, unsigned int)
   GETOPT(genFenDepthEG, unsigned int)
   GETOPT(randomPly, unsigned int)
#endif

   if (DynamicConfig::DFRC) DynamicConfig::FRC = true;
}

} // namespace Options
