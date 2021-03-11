#include "texelTuning.hpp"

#ifdef WITH_TEXEL_TUNING

#include "dynamicConfig.hpp"
#include "evalConfig.hpp"
#include "evalDef.hpp"
#include "extendedPosition.hpp"
#include "logging.hpp"
#include "material.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "score.hpp"
#include "searcher.hpp"
#include "threading.hpp"
#include "tools.hpp"

namespace Texel {

struct TexelInput {
    std::shared_ptr<Position> p;
    int result;
};

template < typename T >
struct TexelParam {
public:
    TexelParam(T & _accessor, const T& _inf, const T& _sup, const std::string & _name, const std::function<void(const T&)> & _hook = [](const T&){}) 
               :accessor(&_accessor), inf(_inf), sup(_sup), name(_name), hook(_hook) {}
    T * accessor;
    T inf;
    T sup;
    std::string name;
    std::function<void(const T&)> hook;
    operator T&() { return *accessor; }
    operator const T&()const { return *accessor; }
    void operator=(const T& value){ *accessor = std::min(std::max(inf,value),sup); hook(value); }
};

template < typename T>
std::ostream& operator<<(std::ostream& os, const TexelParam<T>& p){const T& t = p; os << t; return os;}

double K = 0.23;

double Sigmoid(Position & p) {

    // /////////////////////////////////////////
    // eval returns (white2Play?+1:-1)*sc
    // so does qsearch or search : score from side to move point of view
    // tuning data score is +1 if white wins, -1 if black wins
    // scaled to +1 if white wins and 0 if black wins
    // so score used here must be >0 if white wins
    // /////////////////////////////////////////

    /*
    // qsearch
    DepthType seldepth = 0;
    double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,p,1,seldepth);
    s *= (p.c == Co_White ? +1:-1);
    */

    /*
    // search
    Move m = INVALIDMOVE;
    DepthType d = 64;
    ScoreType s = 0;
    ThreadPool::instance().main().search(p,m,4,s,seldepth);
    s *= (p.c == Co_White ? +1:-1);
    */

    // eval
    EvalData data;
    double s = eval(p,data,ThreadPool::instance().main());
    s *= (p.c == Co_White ? +1:-1);

    return 1. / (1. + std::pow(10, -K*s/400. ));
}

double E(const std::vector<Texel::TexelInput> &data, size_t miniBatchSize) {
    std::atomic<double> e(0);
    static Counter count(0);
    static Counter ms(0);
    std::mutex m;
    const bool progress = true;
    std::chrono::time_point<Clock> startTime = Clock::now();

    auto worker = [&] (size_t begin, size_t end, std::atomic<double> & acc) {
      double ee = 0;
      for(auto k = begin; k != end; ++k) {
        Position * p = data[k].p.get();
        assert(p);
        ee += std::pow((data[k].result+1)*0.5 - Sigmoid(*p),2);
      }
      {
          const std::lock_guard<std::mutex> lock(m);
          acc.store( acc.load() + ee );
      }
    };
    threadedWork(worker,DynamicConfig::threads,miniBatchSize, std::ref(e));

    if ( progress ) {
        count += miniBatchSize;
        ms    += (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
        if ( ms > 10000){
           Logging::LogIt(Logging::logInfo) << ms << "ms " << count/ms << "kps on " << DynamicConfig::threads << " threads";
           count = 0;
           ms    = 0;
        }
    }
    return e/miniBatchSize;
}

void Randomize(std::vector<Texel::TexelInput> & data){ 
    std::shuffle(data.begin(), data.end(), std::default_random_engine(0)); 
}

double computeOptimalK(const std::vector<Texel::TexelInput> & data) {
    double Kstart = 0.05, Kend = 3.0, Kdelta = 0.15;
    double thisError, bestError = 100;
    for (int i = 0; i < 5; ++i) {
        Logging::LogIt(Logging::logInfo) << "Computing K Iteration " << i;
        K = Kstart - Kdelta;
        while (K < Kend) {
            Logging::LogIt(Logging::logInfo) << "...";
            K += Kdelta;
            thisError = E(data,data.size());
            if (thisError < bestError) {
                bestError = thisError, Kstart = K;
                Logging::LogIt(Logging::logInfo) << "new best K = " << K << " E = " << bestError;
            }
        }
        Logging::LogIt(Logging::logInfo) << "iteration " << i << " K = " << Kstart << " E = " << bestError;
        Kend = Kstart + Kdelta;
        Kstart -= Kdelta;
        Kdelta /= 10.0;
    }
    K = Kstart;
    return Kstart;
}

std::vector<double> ComputeGradient(std::vector<TexelParam<ScoreType> > & x0, std::vector<Texel::TexelInput> &data, size_t gradientBatchSize, bool normalized) {
    Logging::LogIt(Logging::logInfo) << "Computing gradient";
    std::vector<double> g;    const ScoreType dx = 1;
    for (size_t k = 0; k < x0.size(); ++k) {
        const ScoreType oldvalue = x0[k];
        x0[k] = oldvalue + dx;
        double Ep1 = E(data, gradientBatchSize);
        x0[k] = oldvalue - dx;
        double Em1 = E(data, gradientBatchSize);
        x0[k] = oldvalue;
        double grad = (Ep1-Em1)/(2*dx);
        g.push_back(grad);
        //Logging::LogIt(Logging::logInfo) << "Gradient " << k << " " << grad;
    }
    if ( normalized){
        double norm = 0;
        for (size_t k = 0; k < x0.size(); ++k) norm += g[k] * g[k];
        norm = sqrt(norm);
        if ( norm > 1e-12){
            for (size_t k = 0; k < x0.size(); ++k) {
                g[k] /= norm;
                //Logging::LogIt(Logging::logInfo) << "Gradient normalized " << k << " " << g[k];
            }
        }
    }
    return g;
}

void displayTexel(const std::string prefixe, const std::vector<TexelParam<ScoreType> >& bestParam, int it, double curE){
    std::ofstream str("TuningOutput/tuning_"+prefixe+".csv",std::ofstream::out | std::ofstream::app);
    // display
    for (size_t k = 0; k < bestParam.size(); ++k) Logging::LogIt(Logging::logInfo) << bestParam[k].name << " " << bestParam[k];
    // write
    str << it << ";";
    for (size_t k = 0; k < bestParam.size(); ++k) str << bestParam[k] << ";";
    str << curE << std::endl;
}

std::vector<TexelParam<ScoreType> > TexelOptimizeGD(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize, const int loops, const std::string & prefix) {
    DynamicConfig::disableTT = true;
    int it = 0;
    Randomize(data);
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    std::vector<ScoreType> previousUpdate(batchSize,0);
    while (it < loops ) {
        std::vector<double> g = ComputeGradient(bestParam, data, batchSize, false);
        double gmax = -1;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            gmax = std::max(gmax,std::fabs(g[k]));
        }
        Logging::LogIt(Logging::logInfo) << "gmax " << gmax;
        if ( gmax < 0.0000000001 ){
            Logging::LogIt(Logging::logInfo) << "gradient is too small, quitting";
            break;
        }

        double learningRate = 2./gmax;
        double alpha = 0.1;

        double baseE = E(data, batchSize);
        Logging::LogIt(Logging::logInfo) << "Base E "  << baseE;

        Logging::LogIt(Logging::logInfo) << "Applying gradient, learningRate = " << learningRate << ", alpha " << alpha;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            const ScoreType oldValue = bestParam[k];
            const ScoreType currentUpdate = ScoreType((1-alpha)*learningRate * g[k] + alpha * previousUpdate[k]);
            bestParam[k] = ScoreType(oldValue - currentUpdate);
            previousUpdate[k] = currentUpdate;
        }

        Logging::LogIt(Logging::logInfo) << "Computing new error";
        double curE = E(data, batchSize);
        Logging::LogIt(Logging::logInfo) << "-> " << curE;
        // randomize for next iteration
        Randomize(data);
        displayTexel(prefix,bestParam,it,curE);
        ++it;
    }
    return bestParam;
}

std::vector<TexelParam<ScoreType> > TexelOptimizeNaive(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize) {
    DynamicConfig::disableTT = true;
    std::ofstream str("TuningOutput/tuning.csv");
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    int stepMax = 5;
    int step = 0;
    for (int loop = 0; loop < 5000; ++loop) {
        for (size_t k = 0; k < bestParam.size(); ++k) {
            Randomize(data);
            double initE = E(data, batchSize);
            double curE = -1;
            step = 0;
            while ( step < stepMax ) {
                step++;
                ScoreType oldValue = bestParam[k];
                bestParam[k] = ScoreType(oldValue - 1);
                if (bestParam[k] == oldValue) break;
                curE = E(data, batchSize);
                if ( curE < initE ){
                    Logging::LogIt(Logging::logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                    initE = curE;
                }
                else{
                    oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue + 1);
                    //curE = E(data, batchSize);
                    break;
                }
            }
            step = 0;
            while ( step < stepMax ) {
                step++;
                ScoreType oldValue = bestParam[k];
                bestParam[k] = ScoreType(oldValue + 1);
                if (bestParam[k] == oldValue) break;
                curE = E(data, batchSize);
                if ( curE < initE ){
                    Logging::LogIt(Logging::logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                    initE = curE;
                }
                else{
                    oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue - 1);
                    //curE = E(data, batchSize);
                    break;
                }
            }
            // write
            str << loop << ";" << k << ";";
            for (size_t kk = 0; kk < bestParam.size(); ++kk) str << bestParam[kk] << ";";
            str << curE << std::endl;
        }
    }
    return bestParam;
}

} // Texel

int getResult(const std::string & s){
    if ( s == "\"1-0\"" ) return 1;
    if ( s == "\"0-1\"" ) return -1;
    if ( s == "\"1/2-1/2\"" ) return 0;
    Logging::LogIt(Logging::logError) << "Bad position result \"" << s << "\" " << s.size();
    for (char c:s) Logging::LogIt(Logging::logError) << (int)c;
    return -2;
}

int getResult2(const std::string & s){
    if ( s == "\"1.000\"" ) return 1;
    if ( s == "\"0.000\"" ) return -1;
    if ( s == "\"0.500\"" ) return 0;
    Logging::LogIt(Logging::logError) << "Bad position result \"" << s << "\"";
    return -2;
}

int getResult3(const std::string & s){
    if ( s == "1-0" ) return 1;
    if ( s == "0-1" ) return -1;
    if ( s == "1/2-1/2" ) return 0;
    Logging::LogIt(Logging::logError) << "Bad position result \"" << s << "\" " << s.size();
    for (char c:s) Logging::LogIt(Logging::logError) << (int)c;
    return -2;
}

void TexelTuning(const std::string & filename) {
    std::vector<Texel::TexelInput> data;
    Logging::LogIt(Logging::logInfo) << "Running texel tuning with file " << filename;
    std::vector<std::string> positions;
    readEPDFile(filename,positions);
    for(size_t k = 0 ; k < positions.size() ; ++k){
        if (k % 50000 == 0) Logging::LogIt(Logging::logInfo) << k << " position read";
//#define TEST_TEXEL
#ifndef TEST_TEXEL
        std::shared_ptr<ExtendedPosition> p(new ExtendedPosition(positions[k],false));
        data.push_back({p, getResult2(p->_extendedParams["c2"][0])}); 
        //data.push_back({p, getResult(p->_extendedParams["c9"][0])});
#else
        const ExtendedPosition pp(positions[k],false);
        const Position pQuiet = Searcher::getQuiet(pp);
        std::shared_ptr<ExtendedPosition> p(new ExtendedPosition(GetFENShort2(pQuiet) + " c15 dummy;",false));
        p->_extendedParams.clear();
        p->_extendedParams["c1"] = pp._extendedParams.at("c1");
        //std::cout << pp.epdString() << std::endl;
        //std::cout << p->epdString() << std::endl;
        data.push_back({p, getResult3(p->_extendedParams["c1"][0])}); // fastGM converted
#endif
        if ( data.back().result == -2 ){
            Logging::LogIt(Logging::logFatal) << ToString(*reinterpret_cast<Position*>(p.get()));
        }
        // +1 white win, -1 black wins, 0 draw
    }
    Logging::LogIt(Logging::logInfo) << "Data size : " << data.size();

    //size_t batchSize = data.size()/50; // batch
    //size_t batchSize = 20000; // batch
    size_t batchSize = 1024*8 ; // mini
    //size_t batchSize = 1; // stochastic

    Logging::LogIt(Logging::logInfo) << "Texel mini batch size " << batchSize;        

    std::map<std::string, std::vector<Texel::TexelParam<ScoreType> > > guess;
#ifdef WITH_PIECE_TUNING
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(Values[P_wp+PieceShift],    20,  200, "pawn",     [](const ScoreType& s){Values[P_bp+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(Values[P_wn+PieceShift],   150,  600, "knight",   [](const ScoreType& s){Values[P_bn+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(Values[P_wb+PieceShift],   150,  600, "bishop",   [](const ScoreType& s){Values[P_bb+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(Values[P_wr+PieceShift],   200,  800, "rook",     [](const ScoreType& s){Values[P_br+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(Values[P_wq+PieceShift],   600, 1800, "queen",    [](const ScoreType& s){Values[P_bq+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wp+PieceShift],  50,  200, "EGpawn",   [](const ScoreType& s){ValuesEG[P_bp+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wn+PieceShift], 150,  600, "EGknight", [](const ScoreType& s){ValuesEG[P_bn+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wb+PieceShift], 150,  600, "EGbishop", [](const ScoreType& s){ValuesEG[P_bb+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wr+PieceShift], 200,  800, "EGrook",   [](const ScoreType& s){ValuesEG[P_br+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
    guess["piecesValue"].push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wq+PieceShift], 600, 1800, "EGqueen",  [](const ScoreType& s){ValuesEG[P_bq+PieceShift] = -s; MaterialHash::InitMaterialScore(false);}));
#endif 
    guess["safeChecks"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSafeCheck[0], -3000, 3000, "cp"));
    guess["safeChecks"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSafeCheck[1], -3000, 3000, "cn"));
    guess["safeChecks"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSafeCheck[2], -3000, 3000, "cb"));
    guess["safeChecks"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSafeCheck[3], -3000, 3000, "cr"));
    guess["safeChecks"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSafeCheck[4], -3000, 3000, "cq"));

    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][0], -3000, 3000, "ap"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][1], -3000, 3000, "an"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][2], -3000, 3000, "ab"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][3], -3000, 3000, "ar"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][4], -3000, 3000, "aq"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][5], -3000, 3000, "ak"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][0], -3000, 3000, "dp"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][1], -3000, 3000, "dn"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][2], -3000, 3000, "db"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][3], -3000, 3000, "dr"));
    guess["attDefKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][4], -3000, 3000, "dq"));

    guess["attOpenFile"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttOpenfile , -400, 800, "kingAttOpenfile"));
    guess["attOpenFile"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSemiOpenfileOur , -400, 800, "kingAttSemiOpenfileOur"));
    guess["attOpenFile"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSemiOpenfileOpp , -400, 800, "kingAttSemiOpenfileOpp"));

    guess["attNoqueen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttNoQueen , -400, 800, "kingAttNoQueen"));

    guess["attFunction"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttMax    , -400, 800, "kattmax",[](const ScoreType & ){EvalConfig::initEval();}));
    guess["attFunction"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttScale  , -400, 800, "kattscale",[](const ScoreType & ){EvalConfig::initEval();}));
    guess["attFunction"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttTrans  , -400, 800, "kingAttTrans",[](const ScoreType & ){EvalConfig::initEval();}));
    guess["attFunction"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttOffset , -400, 800, "kingAttOffset",[](const ScoreType & ){EvalConfig::initEval();}));

    guess["pawnMob"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnMobility[MG] , -500,  500,"pmobility0"));
    guess["pawnMob"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnMobility[EG] , -500,  500,"pmobility1"));
    
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[1][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[1][EG] , -150,  150,"protectedPasserFactorEG"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[2][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[2][EG] , -150,  150,"protectedPasserFactorEG"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[3][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[3][EG] , -150,  150,"protectedPasserFactorEG"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[4][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[4][EG] , -150,  150,"protectedPasserFactorEG"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[5][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[5][EG] , -150,  150,"protectedPasserFactorEG"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[6][MG] , -150,  150,"protectedPasserFactor"));
    guess["protectedPasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserBonus[6][EG] , -150,  150,"protectedPasserFactorEG"));

    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[1][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[1][EG] , -150,  150,"freePasserBonusEG"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[2][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[2][EG] , -150,  150,"freePasserBonusEG"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[3][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[3][EG] , -150,  150,"freePasserBonusEG"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[4][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[4][EG] , -150,  150,"freePasserBonusEG"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[5][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[5][EG] , -150,  150,"freePasserBonusEG"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[6][MG] , -150,  150,"freePasserBonus"));
    guess["freePasser"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserBonus[6][EG] , -150,  150,"freePasserBonusEG"));

    guess["Fawn"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFawnMalusKS[MG] , -150,  150,"pawnFawnMalusKS"));
    guess["Fawn"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFawnMalusKS[EG] , -150,  150,"pawnFawnMalusKSEG"));
    guess["Fawn"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFawnMalusQS[MG] , -150,  150,"pawnFawnMalusQS"));
    guess["Fawn"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFawnMalusQS[EG] , -150,  150,"pawnFawnMalusQSEG"));


    for (int k = 0 ; k < 9 ; ++k ){
       guess["adjustN"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjKnight[k][MG] , -150,  150,"adjKnightMG"+std::to_string(k)));
       guess["adjustN"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjKnight[k][EG] , -150,  150,"adjKnightEG"+std::to_string(k)));
    }

    for (int k = 0 ; k < 9 ; ++k ){
       guess["adjustR"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjRook[k][MG] , -150,  150,"adjRookMG"+std::to_string(k)));
       guess["adjustR"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjRook[k][EG] , -150,  150,"adjRookEG"+std::to_string(k)));
    }

    for (int k = 0 ; k < 9 ; ++k ){
       guess["adjustB"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::bishopPairBonus[k][MG] , -500,  500,"bishop pair"+std::to_string(k)));
       guess["adjustB"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::bishopPairBonus[k][EG] , -500,  500,"bishop pair EG"+std::to_string(k)));
    }

    for (int k = 0 ; k < 9 ; ++k ){
       guess["badBishop"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::badBishop[k][MG] , -500,  500,"badBishop"+std::to_string(k)));
       guess["badBishop"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::badBishop[k][EG] , -500,  500,"badBishopEG"+std::to_string(k)));
    }

    guess["pairAdjust"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightPairMalus[MG] , -500,  500,"knight pair"));
    guess["pairAdjust"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightPairMalus[EG] , -500,  500,"knight pair EG"));
    guess["pairAdjust"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookPairMalus  [MG] , -500,  500,"rook pair"));
    guess["pairAdjust"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookPairMalus  [EG] , -500,  500,"rook pair EG"));

    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenFile       [MG] , -500,  500,"rookOnOpenFile"));
    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenFile       [EG] , -500,  500,"rookOnOpenFileEG"));

    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOur[MG] , -500,  500,"rookOnOpenSemiFileOur"));
    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOur[EG] , -500,  500,"rookOnOpenSemiFileOurEG"));

    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOpp[MG] , -500,  500,"rookOnOpenSemiFileOpp"));
    guess["rookOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOpp[EG] , -500,  500,"rookOnOpenSemiFileOppEG"));
    
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[1][MG], -1500, 1500,"passer 1"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[1][EG], -1500, 1500,"passer 1"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[2][MG], -1500, 1500,"passer 2"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[2][EG], -1500, 1500,"passer 2"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[3][MG], -1500, 1500,"passer 3"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[3][EG], -1500, 1500,"passer 3"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[4][MG], -1500, 1500,"passer 4"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[4][EG], -1500, 1500,"passer 4"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[5][MG], -1500, 1500,"passer 5"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[5][EG], -1500, 1500,"passer 5"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[6][MG], -1500, 1500,"passer 6"));
    guess["passer"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[6][EG], -1500, 1500,"passer 6"));

    guess["pieceBlocking"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pieceFrontPawn[MG],-150,550,"pieceFrontPawn"));
    guess["pieceBlocking"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pieceFrontPawn[EG],-150,550,"pieceFrontPawnEG"));

    for (int r = 0 ; r < 8 ; ++r){
       guess["pawnFrontMinor"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFrontMinor[r][MG],-150,550,"pawnFrontMinor_"+ std::to_string(r)));
       guess["pawnFrontMinor"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnFrontMinor[r][EG],-150,550,"pawnFrontMinorEG_"+ std::to_string(r)));
    }

    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::holesMalus[MG],-150,550,"holesMalus"));
    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::holesMalus[EG],-150,550,"holesMalusEG"));
    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpostN[MG],-150,550,"outpostN"));
    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpostN[EG],-150,550,"outpostNEG"));
    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpostB[MG],-150,550,"outpostB"));
    guess["holes"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpostB[EG],-150,550,"outpostBEG"));

    guess["center"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::centerControl[MG],-150,550,"center"));
    guess["center"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::centerControl[EG],-150,550,"centerEG"));

    for (int r = 0 ; r < 8 ; ++r){
      guess["pawnStructure1"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawn  [r][0][MG], -150,550,  "doublePawn    " + std::to_string(r) + "0"));
      guess["pawnStructure1"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawn  [r][0][EG], -150,550,  "doublePawnEG  " + std::to_string(r) + "0"));
      guess["pawnStructure1"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawn  [r][1][MG], -150, 550, "doublePawn    " + std::to_string(r) + "1"));
      guess["pawnStructure1"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawn  [r][1][EG], -150, 550, "doublePawnEG  " + std::to_string(r) + "1"));

      guess["pawnStructure2"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawn[r][0][MG], -150,550,  "isolatedPawn  " + std::to_string(r) + "0"));
      guess["pawnStructure2"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawn[r][0][EG], -150,550,  "isolatedPawnEG" + std::to_string(r) + "0"));
      guess["pawnStructure2"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawn[r][1][MG], -150, 550, "isolatedPawn  " + std::to_string(r) + "1"));
      guess["pawnStructure2"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawn[r][1][EG], -150, 550, "isolatedPawnEG" + std::to_string(r) + "1"));

      guess["pawnStructure3"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawn[r][0][MG], -150, 550, "backwardPawn  " + std::to_string(r) + "0"));
      guess["pawnStructure3"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawn[r][0][EG], -150, 550, "backwardPawnEG" + std::to_string(r) + "0"));
      guess["pawnStructure3"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawn[r][1][MG], -150, 550, "backwardPawn  " + std::to_string(r) + "1"));
      guess["pawnStructure3"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawn[r][1][EG], -150, 550, "backwardPawnEG" + std::to_string(r) + "1"));

      guess["pawnStructure4"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::detachedPawn[r][0][MG], -150, 550, "detachedPawn  " + std::to_string(r) + "0"));
      guess["pawnStructure4"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::detachedPawn[r][0][EG], -150, 550, "detachedPawnEG" + std::to_string(r) + "0"));
      guess["pawnStructure4"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::detachedPawn[r][1][MG], -150, 550, "detachedPawn  " + std::to_string(r) + "1"));
      guess["pawnStructure4"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::detachedPawn[r][1][EG], -150, 550, "detachedPawnEG" + std::to_string(r) + "1"));
    }
  
    guess["shield"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnShieldBonus[MG]     ,-150,550,"pawnShieldBonus0"));
    guess["shield"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnShieldBonus[EG]     ,-150,550,"pawnShieldBonus1"));

    for (int k = 0; k < 8; ++k) {
        for (int r = 0; r < 8; ++r) {
           guess["kingNearPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawnSupport[k][r][MG],-150,550,"kingNearPassedPawnSupport"+ std::to_string(k)+ std::to_string(r)));
           guess["kingNearPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawnSupport[k][r][EG],-150,550,"kingNearPassedPawnSupport"+ std::to_string(k)+ std::to_string(r)));
        }
    }
    for (int k = 0; k < 8; ++k) {
        for (int r = 0; r < 8; ++r) {
           guess["kingNearPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawnDefend[k][r][MG],-150,550,"kingNearPassedPawnDefend"+ std::to_string(k)+ std::to_string(r)));
           guess["kingNearPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawnDefend[k][r][EG],-150,550,"kingNearPassedPawnDefend"+ std::to_string(k)+ std::to_string(r)));
        }
    }

    guess["rookBehindPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookBehindPassed[MG] , -500,  500,"rookBehindPassed0"));
    guess["rookBehindPassed"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookBehindPassed[EG] , -500,  500,"rookBehindPassed1"));

    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[1][MG], -1500, 1500,"candidate 1"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[1][EG], -1500, 1500,"candidateEG 1"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[2][MG], -1500, 1500,"candidate 2"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[2][EG], -1500, 1500,"candidateEG 2"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[3][MG], -1500, 1500,"candidate 3"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[3][EG], -1500, 1500,"candidateEG 3"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[4][MG], -1500, 1500,"candidate 4"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[4][EG], -1500, 1500,"candidateEG 4"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[5][MG], -1500, 1500,"candidate 5"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[5][EG], -1500, 1500,"candidateEG 5"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[6][MG], -1500, 1500,"candidate 6"));
    guess["candidate"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[6][EG], -1500, 1500,"candidateEG 6"));

    for (int k = 0; k < 6; ++k) {
       guess["minorThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByMinor[k][MG], -200, 200, "threatByMinor" + std::to_string(k) ));
       guess["minorThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByMinor[k][EG], -200, 200, "threatByMinorEG" + std::to_string(k)));
    }

    for (int k = 0; k < 6; ++k) {
        guess["rookThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByRook[k][MG], -200, 200, "threatByRook" + std::to_string(k)));
        guess["rookThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByRook[k][EG], -200, 200, "threatByRookEG" + std::to_string(k)));
    }

    for (int k = 0; k < 6; ++k) {
        guess["queenThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByQueen[k][MG], -200, 200, "threatByQueen" + std::to_string(k)));
        guess["queenThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByQueen[k][EG], -200, 200, "threatByQueenEG" + std::to_string(k)));
    }

    for (int k = 0; k < 6; ++k) {
        guess["kingThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByKing[k][MG], -200, 200, "threatByKing" + std::to_string(k)));
        guess["kingThreat"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::threatByKing[k][EG], -200, 200, "threatByKingEG" + std::to_string(k)));
    }

    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 15 ; ++i){
           guess["mobility"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOB[k][i][MG],-200,200,"mob"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }
    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 15 ; ++i){
           guess["mobility"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOB[k][i][EG],-200,200,"mobeg"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }

    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < NbSquare ; ++i){
           guess["PST"+std::to_string(k)].push_back(Texel::TexelParam<ScoreType>(EvalConfig::PST[k][i][MG],-200,200,"pst"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }
    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < NbSquare ; ++i){
           guess["PST"+std::to_string(k)].push_back(Texel::TexelParam<ScoreType>(EvalConfig::PST[k][i][EG],-200,200,"psteg"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }

    guess["rookQueenFile"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookQueenSameFile[MG] , -500,  500,"rookQueenSameFile"));
    guess["rookQueenFile"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookQueenSameFile[EG] , -500,  500,"rookQueenSameFileEG"));

    guess["rookFrontQueen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookFrontQueenMalus[MG] , -500,  500,"rookFrontQueenMalus"));
    guess["rookFrontQueen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookFrontQueenMalus[EG] , -500,  500,"rookFrontQueenMalusEG"));

    guess["rookFrontKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookFrontKingMalus[MG] , -500,  500,"rookFrontKingMalus"));
    guess["rookFrontKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookFrontKingMalus[EG] , -500,  500,"rookFrontKingMalusEG"));

    guess["hanging"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::hangingPieceMalus[MG] , -500,  500,"hangingPieceMalus"));
    guess["hanging"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::hangingPieceMalus[EG] , -500,  500,"hangingPieceMalusEG"));

    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[0][MG] , -500,  500,"initiativeMG0"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[0][EG] , -500,  500,"initiativeEG0"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[1][MG] , -500,  500,"initiativeMG1"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[1][EG] , -500,  500,"initiativeEG1"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[2][MG] , -500,  500,"initiativeMG2"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[2][EG] , -500,  500,"initiativeEG2"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[3][MG] , -500,  500,"initiativeMG3"));
    guess["initiative"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::initiative[3][EG] , -500,  500,"initiativeEG3"));

    guess["pawnAtt"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnSafeAtt[MG] , -500,  500,"pawnSafeAtt"));
    guess["pawnAtt"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnSafeAtt[EG] , -500,  500,"pawnSafeAttEG"));
    guess["pawnAtt"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnSafePushAtt[MG] , -500,  500,"pawnSafePushAtt"));
    guess["pawnAtt"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnSafePushAtt[EG] , -500,  500,"pawnSafePushAttEG"));

    guess["storm"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnStormMalus[MG] , -500,  500,"pawnStormMalus"));
    guess["storm"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnStormMalus[EG] , -500,  500,"pawnStormMalusEG"));

    guess["minorOnOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::minorOnOpenFile[MG] , -500,  500,"minorOnOpenFile"));
    guess["minorOnOpen"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::minorOnOpenFile[EG] , -500,  500,"minorOnOpenFileEG"));

    guess["queenNearKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::queenNearKing[MG] , -500,  500,"queenNearKing"));
    guess["queenNearKing"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::queenNearKing[EG] , -500,  500,"queenNearKingEG"));

    guess["pawnlessFlank"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnlessFlank[MG] , -500,  500,"pawnlessFlank"));
    guess["pawnlessFlank"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnlessFlank[EG] , -500,  500,"pawnlessFlankEG"));

    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [0][MG] , -100, 100, "ppinK"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [0][EG] , -100, 100, "ppinKEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [1][MG] , -100, 100, "npinK"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [1][EG] , -100, 100, "npinKEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [2][MG] , -100, 100, "bpinK"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [2][EG] , -100, 100, "bpinKEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [3][MG] , -100, 100, "rpinK"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [3][EG] , -100, 100, "rpinKEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [4][MG] , -100, 100, "qpinK"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedKing [4][EG] , -100, 100, "qpinKEG"));
    
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[0][MG] , -100, 100, "ppinq"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[0][EG] , -100, 100, "ppinqEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[1][MG] , -100, 100, "npinq"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[1][EG] , -100, 100, "npinqEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[2][MG] , -100, 100, "bpinq"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[2][EG] , -100, 100, "bpinqEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[3][MG] , -100, 100, "rpinq"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[3][EG] , -100, 100, "rpinqEG"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[4][MG] , -100, 100, "qpinq"));
    guess["pinned"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::pinnedQueen[4][EG] , -100, 100, "qpinqEG"));

    for (int k = 0 ; k < 8 ; ++k ){
       guess["knightTooFar"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightTooFar[k][MG] , -100, 100, "knightTooFarMG"+std::to_string(k)));
       guess["knightTooFar"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightTooFar[k][EG] , -100, 100, "knightTooFarEG"+std::to_string(k)));
    }

    guess["tempo"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::tempo[MG] , -500,  500,"tempo"));
    guess["tempo"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::tempo[EG] , -500,  500,"tempoEG"));

    for (Mat m1 = M_p; m1 <= M_q; ++m1) {
        for (Mat m2 = M_p; m2 <= m1; ++m2) {
            guess["imbalance"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::imbalance_mines[m1-1][m2-1][MG],  -3000, 3000, "imbalance_mines"  + std::to_string(m1) + "_" + std::to_string(m2)));
            guess["imbalance"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::imbalance_theirs[m1-1][m2-1][MG], -3000, 3000, "imbalance_theirs" + std::to_string(m1) + "_" + std::to_string(m2)));
            guess["imbalance"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::imbalance_mines[m1-1][m2-1][EG],  -3000, 3000, "imbalance_minesEG"  + std::to_string(m1) + "_" + std::to_string(m2)));
            guess["imbalance"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::imbalance_theirs[m1-1][m2-1][EG], -3000, 3000, "imbalance_theirsEG" + std::to_string(m1) + "_" + std::to_string(m2)));
        }
    }

    for (auto f = F_material ; f <= F_pawnStruct; ++f){
        for (auto g = F_material ; g <= f; ++g){
            guess["secondorder"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::secondOrderFeature[f][g][MG],  -3000, 3000, "secondorder"  + std::to_string(f) + "_" + std::to_string(g)));
            guess["secondorder"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::secondOrderFeature[f][g][EG],  -3000, 3000, "secondorderEG"  + std::to_string(f) + "_" + std::to_string(g)));
        }
    }

    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorWin , -1000, 1000, "scalingFactorWin"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorHardWin, -1000, 1000, "scalingFactorHardWin"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorLikelyDraw, -1000, 1000, "scalingFactorLikelyDraw"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorOppBishopAlone, -1000, 1000, "scalingFactorOppBishopAlone"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorOppBishopAloneSlope, -1000, 1000, "scalingFactorOppBishopAloneSlope"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorOppBishop, -1000, 1000, "scalingFactorOppBishop"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorOppBishopSlope, -1000, 1000, "scalingFactorOppBishopSlope"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorQueenNoQueen, -1000, 1000, "scalingFactorQueenNoQueen"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorQueenNoQueenSlope, -1000, 1000, "scalingFactorQueenNoQueenSlope"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorPawns, -1000, 1000, "scalingFactorPawns"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorPawnsSlope, -1000, 1000, "scalingFactorPawnsSlope"));
    guess["scaling"].push_back(Texel::TexelParam<ScoreType>(EvalConfig::scalingFactorPawnsOneSide, -1000, 1000, "scalingFactorPawnsOneSide"));

    for(auto it = guess.begin() ; it != guess.end(); ++it){
        std::cout << "\"" << it->first << "\",";
    }

    computeOptimalK(data);
    Logging::LogIt(Logging::logInfo) << "Optimal K " << Texel::K;

    std::vector<std::string> todo = {
        //"piecesValue",

        "scaling",
/*
        "PST0",
        "PST1",
        "PST2",
        "PST3",
        "PST4",
        "PST5",

        "mobility",
        
        "passer",
        "freePasser",
        "protectedPasser",
        "kingNearPassed",

        "pawnStructure1",
        "pawnStructure2",
        "pawnStructure3",
        "pawnStructure4",
        "candidate",

        "pawnMob",
        "pawnAtt",

        "holes",
        "center",

        "shield",
        "Fawn",
        "storm",
        "pawnlessFlank",

        "rookBehindPassed",
        "rookFrontKing",
        "rookFrontQueen",
        "rookOpen",
        "rookQueenFile",

        "pairAdjust",
        "adjustN",
        "adjustR",
        "adjustB",
        "badBishop",

        "queenNearKing",

        "pieceBlocking",
        "minorOnOpen",
        "knightTooFar",
        "pawnFrontMinor",

        "hanging",
        "pinned",

        "attDefKing",
        "attFunction",
        "attOpenFile",
        "attNoqueen",

        "imbalance",
        "initiative",

        "kingThreat",
        "minorThreat",
        "queenThreat",
        "rookThreat",
        //"secondorder",
        //"safeChecks",
        //"tempo",
*/

    };
    
    for(auto loops = 0 ; loops < 1000 ; ++loops){
        Logging::LogIt(Logging::logInfo) << "Starting loop : " << loops;
        for(auto it = todo.begin() ; it != todo.end(); ++it){
            if ( guess.find(*it) == guess.end() ){
                Logging::LogIt(Logging::logError) << "Not found :" << *it;
                continue;
            }
            std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeGD(guess[*it], data, batchSize, std::max(64ul,guess[*it].size()),*it);
            Logging::LogIt(Logging::logInfo) << "Optimized values :";
            for (size_t k = 0; k < optim.size(); ++k) Logging::LogIt(Logging::logInfo) << optim[k].name << " " << optim[k];
        }
        Logging::LogIt(Logging::logInfo) << "Final Optimized values :";
        for(auto it = todo.begin() ; it != todo.end(); ++it){
            for (size_t k = 0; k < guess[*it].size(); ++k) Logging::LogIt(Logging::logInfo) << guess[*it][k].name << " " << guess[*it][k];
        }
    }
}

#endif
