#include "option.hpp"

#include "com.hpp"
#include "logging.hpp"
#include "searcher.hpp"
#include "smp.hpp"

#ifdef WITH_NNUE
#include "nnue.hpp"
#endif

namespace Options {

    std::vector<std::string> args;
    std::vector<KeyBase> _keys;

    KeyBase & GetKey(const std::string & key) {
        bool keyFound = false;
        static int dummy = 0;
        static KeyBase badKey(k_bad,w_button,"bad_default_key",&dummy,0,0);
        KeyBase * keyRef = &badKey;
        for (size_t k = 0; k < _keys.size(); ++k) { if (key == _keys[k].key) { keyRef = &_keys[k]; keyFound = true; break;} }
        if ( !keyFound) Logging::LogIt(Logging::logWarn) << "Key not found " << key;
        return *keyRef;
    }

    int GetValue(const std::string & key){ // assume we can convert to int safely (not valid for string of course !)
        const KeyBase & k = GetKey(key);
        switch (k.type) {
        case k_bool:   return (int)*static_cast<bool*>(k.value);
        case k_depth:  return (int)*static_cast<DepthType*>(k.value);
        case k_int:    return (int)*static_cast<int*>(k.value);
        case k_score:  return (int)*static_cast<ScoreType*>(k.value);
        case k_ull:    return (int)*static_cast<unsigned long long int*>(k.value);
        case k_string:
        case k_bad:
        default:       Logging::LogIt(Logging::logError) << "Bad key type"; return false;
        }
    }
    const std::string GetValueString(const std::string & key){ // the one for string
        const KeyBase & k = GetKey(key);
        if (k.type != k_string) Logging::LogIt(Logging::logError) << "Bad key type";
        return *static_cast<std::string*>(k.value);
    }
    void displayOptionsDebug(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key) << "\"";
            else if (it->type==k_bool )   Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (GetValue(it->key)?"true":"false") << "\"";
            else                          Logging::LogIt(Logging::logInfo) << "option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (int)GetValue(it->key)  <<  " " << it->vmin << " " << it->vmax << "\"";
    }
    void displayOptionsXBoard(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << GetValueString(it->key) << "\"";
            else if (it->type==k_bool )   Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << bool(GetValue(it->key)) << "\"";
            else                          Logging::LogIt(Logging::logGUI) << "feature option=\"" << it->key << " -" << widgetXboardNames[it->wtype] << " " << (int)GetValue(it->key)  <<  " " << it->vmin << " " << it->vmax << "\"";
    }
    void displayOptionsUCI(){
        for(auto it = _keys.begin() ; it != _keys.end() ; ++it)
            if      (it->type==k_string ) Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << GetValueString(it->key);
            else if (it->type==k_bool )   Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << (GetValue(it->key)?"true":"false");
            else                          Logging::LogIt(Logging::logGUI) << "option name " << it->key << " type " << widgetXboardNames[it->wtype] << " default " << (int)GetValue(it->key)  <<  " min " << it->vmin << " max " << it->vmax;
    }

#define SETVALUE(TYPEIN,TYPEOUT) {TYPEIN v; str >> std::boolalpha >> v; *static_cast<TYPEOUT*>(keyRef.value) = (TYPEOUT)v;} break;

    bool SetValue(const std::string & key, const std::string & val){
        KeyBase & keyRef = GetKey(key);
        if ( !keyRef.hotPlug && ThreadPool::instance().main().searching() ){
            Logging::LogIt(Logging::logError) << "Cannot change " << key << " during a search";
            return false;
        }
        std::string value = val;
        if ( keyRef.type == k_bool ){
            if ( value == "0") value = "false";
            else if ( value == "1") value = "true";
        }
        std::stringstream str(value);
        switch (keyRef.type) {
        case k_bool:   SETVALUE(bool,bool)
        case k_depth:  SETVALUE(int,DepthType)
        case k_int:    SETVALUE(int,int)
        case k_score:  SETVALUE(int,ScoreType)
        case k_ull:    SETVALUE(int,unsigned long long)
        case k_string: SETVALUE(std::string,std::string)
        case k_bad:
        default: Logging::LogIt(Logging::logError) << "Bad key type"; return false;
        }
        if ( keyRef.callBack ) keyRef.callBack();
        Logging::LogIt(Logging::logInfo) << "Option set " << key << "=" << value;
        displayOptionsDebug();
        return true;
    }
    void registerCOMOptions(){ // options exposed to GUI
       _keys.push_back(KeyBase(k_int,   w_spin,  "Level"                       , &DynamicConfig::level                          , (unsigned int)0  , (unsigned int)100 ));
       _keys.push_back(KeyBase(k_bool,  w_check, "UCI_LimitStrength"           , &DynamicConfig::limitStrength                  , false            , true ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "UCI_Elo"                     , &DynamicConfig::strength                       , (int)500         , (int)2800 ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "Hash"                        , &DynamicConfig::ttSizeMb                       , (unsigned int)1  , (unsigned int)256000                , &TT::initTable));
       _keys.push_back(KeyBase(k_int,   w_spin,  "Threads"                     , &DynamicConfig::threads                        , (unsigned int)1  , (unsigned int)(MAX_THREADS-1)       , std::bind(&ThreadPool::setup, &ThreadPool::instance())));
       _keys.push_back(KeyBase(k_bool,  w_check, "UCI_Chess960"                , &DynamicConfig::FRC                            , false            , true ));
       _keys.push_back(KeyBase(k_bool,  w_check, "Ponder"                      , &DynamicConfig::UCIPonder                      , false            , true ));
       _keys.push_back(KeyBase(k_bool,  w_check, "MateFinder"                  , &DynamicConfig::mateFinder                     , false            , true ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "MultiPV"                     , &DynamicConfig::multiPV                        , (unsigned int)1  , (unsigned int)4 ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "RandomOpen"                  , &DynamicConfig::randomOpen                     , (unsigned int)0  , (unsigned int)100 ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "MinMoveOverHead"             , &DynamicConfig::moveOverHead                   , (unsigned int)10 , (unsigned int)1000 ));
       _keys.push_back(KeyBase(k_score, w_spin,  "Contempt"                    , &DynamicConfig::contempt                       , (ScoreType)-50   , (ScoreType)50));
       _keys.push_back(KeyBase(k_score, w_spin,  "ContemptMG"                  , &DynamicConfig::contemptMG                     , (ScoreType)-50   , (ScoreType)50));
       #ifdef WITH_SYZYGY
       _keys.push_back(KeyBase(k_string,w_string,"SyzygyPath"                  , &DynamicConfig::syzygyPath                                                                              , &SyzygyTb::initTB));
       #endif
       #ifdef WITH_NNUE
       _keys.push_back(KeyBase(k_string,w_string,"NNUEFile"                    , &DynamicConfig::NNUEFile                                                                                , &NNUEWrapper::init));
       _keys.push_back(KeyBase(k_bool,  w_check, "forceNNUE"                   , &DynamicConfig::forceNNUE                      , false            , true                              ));
       #endif

       #ifdef WITH_GENFILE
       _keys.push_back(KeyBase(k_bool,  w_check, "GenFen"                      , &DynamicConfig::genFen                         , false            , true ));
       _keys.push_back(KeyBase(k_bool,  w_check, "GenFenSearchTree"            , &DynamicConfig::genFenSearchTree               , false            , true ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "GenFenDepth"                 , &DynamicConfig::genFenDepth                    , (unsigned int)2  , (unsigned int)20 ));
       _keys.push_back(KeyBase(k_int,   w_spin,  "GenFenSkip"                  , &DynamicConfig::genFenSkip                     , (unsigned int)1  , (unsigned int)10000000 ));
       
       _keys.push_back(KeyBase(k_int,   w_spin,  "RandomPly"                   , &DynamicConfig::randomPly                      , (unsigned int)0   , (unsigned int)20 ));

       #endif

       _keys.push_back(KeyBase(k_int, w_spin,  "StyleAttack"                   , &DynamicConfig::styleAttack                    , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StyleComplexity"               , &DynamicConfig::styleComplexity                , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StyleDevelopment"              , &DynamicConfig::styleDevelopment               , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StyleMaterial"                 , &DynamicConfig::styleMaterial                  , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StyleMobility"                 , &DynamicConfig::styleMobility                  , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StylePositional"               , &DynamicConfig::stylePositional                , (int)0   , (int)100                                    , &EvalFeatures::callBack));
       _keys.push_back(KeyBase(k_int, w_spin,  "StyleForwardness"              , &DynamicConfig::styleForwardness               , (int)0   , (int)100                                    , &EvalFeatures::callBack));

#ifdef WITH_SEARCH_TUNING
       _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin0"                  , &SearchConfig::qfutilityMargin[0]                  , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "qfutilityMargin1"                  , &SearchConfig::qfutilityMargin[1]                  , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "staticNullMoveMaxDepth0"           , &SearchConfig::staticNullMoveMaxDepth[0]           , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "staticNullMoveMaxDepth1"           , &SearchConfig::staticNullMoveMaxDepth[1]           , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthCoeff0"         , &SearchConfig::staticNullMoveDepthCoeff[0]         , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthCoeff1"         , &SearchConfig::staticNullMoveDepthCoeff[1]         , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthInit0"          , &SearchConfig::staticNullMoveDepthInit[0]          , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "staticNullMoveDepthInit1"          , &SearchConfig::staticNullMoveDepthInit[1]          , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthCoeff0"         , &SearchConfig::razoringMarginDepthCoeff[0]         , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthCoeff1"         , &SearchConfig::razoringMarginDepthCoeff[1]         , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthInit0"          , &SearchConfig::razoringMarginDepthInit[0]          , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "razoringMarginDepthInit1"          , &SearchConfig::razoringMarginDepthInit[1]          , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_depth, w_spin, "razoringMaxDepth0"                 , &SearchConfig::razoringMaxDepth[0]                 , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "razoringMaxDepth1"                 , &SearchConfig::razoringMaxDepth[1]                 , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "nullMoveMinDepth"                  , &SearchConfig::nullMoveMinDepth                    , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "nullMoveDynamicDivisor"            , &SearchConfig::nullMoveDynamicDivisor              , ScoreType(0)    , ScoreType(1500)     ));
       
       _keys.push_back(KeyBase(k_depth, w_spin, "historyPruningMaxDepth"            , &SearchConfig::historyPruningMaxDepth              , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "historyPruningThresholdInit"       , &SearchConfig::historyPruningThresholdInit         , ScoreType(-500) , ScoreType(500)      ));
       _keys.push_back(KeyBase(k_score, w_spin, "historyPruningThresholdDepth"      , &SearchConfig::historyPruningThresholdDepth        , ScoreType(-500) , ScoreType(500)      ));
       _keys.push_back(KeyBase(k_depth, w_spin, "CMHMaxDepth"                       , &SearchConfig::CMHMaxDepth                         , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "futilityMaxDepth0"                 , &SearchConfig::futilityMaxDepth[0]                 , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "futilityMaxDepth1"                 , &SearchConfig::futilityMaxDepth[1]                 , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthCoeff0"               , &SearchConfig::futilityDepthCoeff[0]               , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthCoeff1"               , &SearchConfig::futilityDepthCoeff[1]               , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthInit0"                , &SearchConfig::futilityDepthInit[0]                , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "futilityDepthInit1"                , &SearchConfig::futilityDepthInit[1]                , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "failHighReductionThresholdInit0"   , &SearchConfig::failHighReductionThresholdInit[0]   , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "failHighReductionThresholdInit1"   , &SearchConfig::failHighReductionThresholdInit[1]   , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "failHighReductionThresholdDepth0"  , &SearchConfig::failHighReductionThresholdDepth[0]  , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "failHighReductionThresholdDepth1"  , &SearchConfig::failHighReductionThresholdDepth[1]  , ScoreType(0)    , ScoreType(1500)     ));

       _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth"                       , &SearchConfig::iidMinDepth                         , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth2"                      , &SearchConfig::iidMinDepth2                        , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "iidMinDepth3"                      , &SearchConfig::iidMinDepth3                        , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "probCutMinDepth"                   , &SearchConfig::probCutMinDepth                     , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_int  , w_spin, "probCutMaxMoves"                   , &SearchConfig::probCutMaxMoves                     , 0               , 30                  ));
       _keys.push_back(KeyBase(k_score, w_spin, "probCutMargin"                     , &SearchConfig::probCutMargin                       , ScoreType(0)    , ScoreType(1500)     ));

       _keys.push_back(KeyBase(k_score, w_spin, "seeCaptureFactor"                  , &SearchConfig::seeCaptureFactor                    , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "seeQuietFactor"                    , &SearchConfig::seeQuietFactor                      , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "betaMarginDynamicHistory"          , &SearchConfig::betaMarginDynamicHistory            , ScoreType(0)    , ScoreType(1500)     ));

       _keys.push_back(KeyBase(k_depth, w_spin, "lmrMinDepth"                       , &SearchConfig::lmrMinDepth                         , DepthType(0)    , DepthType(30)       ));
       _keys.push_back(KeyBase(k_depth, w_spin, "singularExtensionDepth"            , &SearchConfig::singularExtensionDepth              , DepthType(0)    , DepthType(30)       ));
        
       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitPruning"                , &SearchConfig::dangerLimitPruning                  , ScoreType(0)    , ScoreType(1500)     ));
       _keys.push_back(KeyBase(k_score, w_spin, "dangerLimitReduction"              , &SearchConfig::dangerLimitReduction                , ScoreType(0)    , ScoreType(1500)     ));

       _keys.push_back(KeyBase(k_score, w_spin, "failLowRootMargin"                 , &SearchConfig::failLowRootMargin                   , ScoreType(0)    , ScoreType(1500)     ));

       ///@todo more ...
#endif

#ifdef WITH_PIECE_TUNING
       _keys.push_back(KeyBase(k_score, w_spin, "PawnValueMG"   , &Values  [P_wp+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "PawnValueEG"   , &ValuesEG[P_wp+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "KnightValueMG" , &Values  [P_wn+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "KnightValueEG" , &ValuesEG[P_wn+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "BishopValueMG" , &Values  [P_wb+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "BishopValueEG" , &ValuesEG[P_wb+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "RookValueMG"   , &Values  [P_wr+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "RookValueEG"   , &ValuesEG[P_wr+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "QueenValueMG"  , &Values  [P_wq+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
       _keys.push_back(KeyBase(k_score, w_spin, "QueenValueEG"  , &ValuesEG[P_wq+PieceShift]  , ScoreType(0)    , ScoreType(2000)     ,  [](){SymetrizeValue(); MaterialHash::InitMaterialScore(false);}));
#endif

    }

    // load command line args in memory
    void readOptions(int argc, char ** argv) {
        for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
    }

    // get option from command line
    void initOptions(int argc, char ** argv){
#define GETOPT(name,type) Options::getOption<type>(DynamicConfig::name,#name);
       registerCOMOptions();
       readOptions(argc,argv);
       GETOPT(quiet,            bool)  // first to be read
       GETOPT(debugMode,        bool)
       GETOPT(debugFile,        std::string)
       GETOPT(ttSizeMb,         unsigned int)
       GETOPT(FRC,              bool)
       GETOPT(threads,          unsigned int)
       GETOPT(mateFinder,       bool)
       GETOPT(fullXboardOutput, bool)
       GETOPT(level,            unsigned int)
       GETOPT(multiPV,          unsigned int)
       GETOPT(randomOpen,       unsigned int)
       GETOPT(limitStrength,    bool)
       GETOPT(strength,         int)
#ifdef WITH_SYZYGY
       GETOPT(syzygyPath,       std::string)
#endif
#ifdef WITH_NNUE
       //GETOPT(useNNUE,          bool)         
       GETOPT(NNUEFile,         std::string)
       GETOPT(skipLoadingEval,  bool)
       GETOPT(forceNNUE,        bool)
       
#endif
#ifdef WITH_GENFILE
       GETOPT(genFen,            bool)
       GETOPT(genFenSearchTree,  bool)
       GETOPT(genFenDepth,       unsigned int)
       GETOPT(genFenSkip,        unsigned int)
       GETOPT(randomPly,         unsigned int)
#endif
   }
} // Options
