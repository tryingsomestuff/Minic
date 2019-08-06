namespace Texel {

struct TexelInput {
    Position * p;
    int result;
};

template < typename T >
struct TexelParam {
public:
    TexelParam(T & accessor, const T& inf, const T& sup, const std::string & name, const std::function<void(const T&)> & hook = [](const T&){}) :accessor(&accessor), inf(inf), sup(sup), name(name), hook(hook) {}
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

double Sigmoid(Position * p) {
    assert(p);

    // /////////////////////////////////////////
    // eval returns (white2Play?+1:-1)*sc
    // so does qsearch or search : score from side to move point of view
    // tuning data score is +1 if white wins, -1 if black wins
    // scaled to +1 if white wins and 0 if black wins
    // so score used here must be >0 if white wins
    // /////////////////////////////////////////

    // qsearch
    /*
    DepthType seldepth = 0;
    double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,*p,1,seldepth);
    s *= (p->c == Co_White ? +1:-1);
    */

    /*
    // search
    Move m = INVALIDMOVE;
    DepthType d = 64;
    ScoreType s = 0;
    ThreadPool::instance().main().search(*p,m,4,s,seldepth);
    s *= (p->c == Co_White ? +1:-1);
    */

    // eval
    float gp;
    double s = eval(*p,gp);
    s *= (p->c == Co_White ? +1:-1);

    return 1. / (1. + std::pow(10, -K*s/400. ));
}

double E(const std::vector<Texel::TexelInput> &data, size_t miniBatchSize) {
    double e = 0;
    const bool progress = miniBatchSize > 100000;
    std::chrono::time_point<Clock> startTime = Clock::now();
    for (size_t k = 0; k < miniBatchSize; ++k) {
        e += std::pow((data[k].result+1)*0.5 - Sigmoid(data[k].p),2);
        if ( progress && k%10000 == 0) std::cout << "." << std::flush;
    }
    if ( progress ) {
        const int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTime).count();
        std::cout << " " << ms << "ms " << miniBatchSize/ms << "kps" << std::endl;
    }
    return e/miniBatchSize;
}

void Randomize(std::vector<Texel::TexelInput> & data, size_t miniBatchSize){ std::shuffle(data.begin(), data.end(), std::default_random_engine(0)); }

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

std::vector<double> ComputeGradient(std::vector<TexelParam<ScoreType> > & x0, std::vector<Texel::TexelInput> &data, size_t gradientBatchSize, bool normalized = true) {
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

std::vector<TexelParam<ScoreType> > TexelOptimizeGD(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize) {
    DynamicConfig::disableTT = true;
    std::ofstream str("tuning.csv");
    int it = 0;
    Randomize(data, batchSize);
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    std::vector<ScoreType> previousUpdate(batchSize,0);
    while (it < 100000 ) {
        std::vector<double> g = ComputeGradient(bestParam, data, batchSize, false);
        double gmax = -1;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            gmax = std::max(gmax,std::fabs(g[k]));
        }
        Logging::LogIt(Logging::logInfo) << "gmax " << gmax;
        if ( gmax < 0.0000001 ) break;

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
        Logging::LogIt(Logging::logInfo) << curE;
        // randomize for next iteration
        Randomize(data, batchSize);
        // display
        for (size_t k = 0; k < bestParam.size(); ++k) Logging::LogIt(Logging::logInfo) << bestParam[k].name << " " << bestParam[k];
        // write
        str << it << ";";
        for (size_t k = 0; k < bestParam.size(); ++k) str << bestParam[k] << ";";
        str << curE << std::endl;
        ++it;
    }
    return bestParam;
}

std::vector<TexelParam<ScoreType> > TexelOptimizeNaive(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize) {
    DynamicConfig::disableTT = true;
    std::ofstream str("tuning.csv");
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    int stepMax = 5;
    int step = 0;
    for (int loop = 0; loop < 5000; ++loop) {
        for (size_t k = 0; k < bestParam.size(); ++k) {
            Randomize(data, batchSize);
            double initE = E(data, batchSize);
            double curE = -1;
            step = 0;
            while ( step < stepMax ) {
                step++;
                const ScoreType oldValue = bestParam[k];
                bestParam[k] = ScoreType(oldValue - 1);
                if (bestParam[k] == oldValue) break;
                curE = E(data, batchSize);
                if ( curE < initE ){
                    Logging::LogIt(Logging::logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                    initE = curE;
                }
                else{
                    ScoreType oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue + 1);
                    //curE = E(data, batchSize);
                    break;
                }
            }
            step = 0;
            while ( step < stepMax ) {
                step++;
                const ScoreType oldValue = bestParam[k];
                bestParam[k] = ScoreType(oldValue + 1);
                if (bestParam[k] == oldValue) break;
                curE = E(data, batchSize);
                if ( curE < initE ){
                    Logging::LogIt(Logging::logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                    initE = curE;
                }
                else{
                    ScoreType oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue - 1);
                    //curE = E(data, batchSize);
                    break;
                }
            }
            // write
            str << loop << ";" << k << ";";
            for (size_t k = 0; k < bestParam.size(); ++k) str << bestParam[k] << ";";
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
    Logging::LogIt(Logging::logFatal) << "Bad position result " << s;
    return 0;
}

void TexelTuning(const std::string & filename) {
    std::vector<Texel::TexelInput> data;
    Logging::LogIt(Logging::logInfo) << "Running texel tuning with file " << filename;
    std::vector<std::string> positions;
    ExtendedPosition::readEPDFile(filename,positions);
    for(size_t k = 0 ; k < positions.size() ; ++k){
        ExtendedPosition * p = new ExtendedPosition(positions[k],false);
        data.push_back({p, getResult(p->_extendedParams["c9"][0])});
        // +1 white win, -1 black wins, 0 draw
        if (k % 50000 == 0) Logging::LogIt(Logging::logInfo) << k << " position read";
    }
    Logging::LogIt(Logging::logInfo) << "Data size : " << data.size();

    size_t batchSize = data.size()/10; // batch
    //size_t batchSize = 20000; // batch
    //size_t batchSize = 1024 ; // mini
    //size_t batchSize = 1; // stochastic

    //for(int k=0 ; k<13; ++k){Values[k] = 450; ValuesEG[k] = 450;}

    std::vector<Texel::TexelParam<ScoreType> > guess;

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wp+PieceShift],    20,  200, "pawn",     [](const ScoreType& s){Values[P_bp+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wn+PieceShift],   150,  600, "knight",   [](const ScoreType& s){Values[P_bn+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wb+PieceShift],   150,  600, "bishop",   [](const ScoreType& s){Values[P_bb+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wr+PieceShift],   200,  800, "rook",     [](const ScoreType& s){Values[P_br+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wq+PieceShift],   600, 1800, "queen",    [](const ScoreType& s){Values[P_bq+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wp+PieceShift],  50,  200, "EGpawn",   [](const ScoreType& s){ValuesEG[P_bp+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wn+PieceShift], 150,  600, "EGknight", [](const ScoreType& s){ValuesEG[P_bn+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wb+PieceShift], 150,  600, "EGbishop", [](const ScoreType& s){ValuesEG[P_bb+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wr+PieceShift], 200,  800, "EGrook",   [](const ScoreType& s){ValuesEG[P_br+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wq+PieceShift], 600, 1800, "EGqueen",  [](const ScoreType& s){ValuesEG[P_bq+PieceShift] = -s;}));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][1], -30, 30, "ap"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][2], -30, 30, "an"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][3], -30, 30, "ab"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][4], -30, 30, "ar"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][5], -30, 30, "aq"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[0][6], -30, 30, "ak"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][1], -30, 30, "dp"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][2], -30, 30, "dn"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][3], -30, 30, "db"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][4], -30, 30, "dr"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][5], -30, 30, "dq"));
    //guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttWeight[1][6], -30, 30, "dk"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttOpenfile , -400, 800, "kingAttOpenfile"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSemiOpenfileOur , -400, 800, "kingAttSemiOpenfileOur"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttSemiOpenfileOpp , -400, 800, "kingAttSemiOpenfileOpp"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttMax    , -400, 800, "kattmax",[](const ScoreType & ){initEval();}));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttScale  , -100, 100, "kattscalre",[](const ScoreType & ){initEval();}));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttTrans  , -100, 100, "kingAttTrans",[](const ScoreType & ){initEval();}));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingAttOffset , -100, 100, "kingAttOffset",[](const ScoreType & ){initEval();}));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnMobility[MG] , -500,  500,"pmobility0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnMobility[EG] , -500,  500,"pmobility1"));
    */

    /* 
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserFactor[MG] , -150,  150,"protectedPasserFactor"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserFactor[MG] , -150,  150,"freePasserFactor"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::protectedPasserFactor[EG] , -150,  150,"protectedPasserFactorEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::freePasserFactor[EG] , -150,  150,"freePasserFactorEG"));
    */

    /*
    for (int k = 0 ; k < 9 ; ++k ){
       guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjKnight[k][MG] , -150,  150,"adjKnightMG"+std::to_string(k)));
       guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjKnight[k][EG] , -150,  150,"adjKnightEG"+std::to_string(k)));
    }
    for (int k = 0 ; k < 9 ; ++k ){
       guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjRook[k][MG] , -150,  150,"adjRookMG"+std::to_string(k)));
       guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::adjRook[k][EG] , -150,  150,"adjRookEG"+std::to_string(k)));
    }
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::bishopPairBonus[MG] , -500,  500,"bishop pair"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightPairMalus[MG] , -500,  500,"knight pair"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookPairMalus  [MG] , -500,  500,"rook pair"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::bishopPairBonus[EG] , -500,  500,"bishop pair EG"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::knightPairMalus[EG] , -500,  500,"knight pair EG"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookPairMalus  [EG] , -500,  500,"rook pair EG"));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[1][MG], -1500, 1500,"passer 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[2][MG], -1500, 1500,"passer 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[3][MG], -1500, 1500,"passer 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[4][MG], -1500, 1500,"passer 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[5][MG], -1500, 1500,"passer 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[6][MG], -1500, 1500,"passer 6"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[1][EG], -1500, 1500,"passer 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[2][EG], -1500, 1500,"passer 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[3][EG], -1500, 1500,"passer 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[4][EG], -1500, 1500,"passer 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[5][EG], -1500, 1500,"passer 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::passerBonus[6][EG], -1500, 1500,"passer 6"));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::holesMalus[MG],-150,550,"holesMalus"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::holesMalus[EG],-150,550,"holesMalusEG"));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpost[MG],-150,550,"outpost"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::outpost[EG],-150,550,"outpostEG"));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawnMalus  [0][MG],-150,550,"doublePawnMalus[0]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawnMalus  [0][EG],-150,550,"doublePawnMalusEG[0]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawnMalus[0][MG],-150,550,"isolatedPawnMalus[0]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawnMalus[0][EG],-150,550,"isolatedPawnMalusEG[0]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawnMalus[0][MG], -150, 550, "backwardPawnMalus0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawnMalus[0][EG], -150, 550, "backwardPawnMalusEG0"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawnMalus  [1][MG], -150, 550, "doublePawnMalus[1]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::doublePawnMalus  [1][EG], -150, 550, "doublePawnMalusEG[1]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawnMalus[1][MG], -150, 550, "isolatedPawnMalus[1]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::isolatedPawnMalus[1][EG], -150, 550, "isolatedPawnMalusEG[1]"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawnMalus[1][MG], -150, 550, "backwardPawnMalus1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::backwardPawnMalus[1][EG], -150, 550, "backwardPawnMalusEG1"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnShieldBonus[MG]     ,-150,550,"pawnShieldBonus0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::pawnShieldBonus[EG]     ,-150,550,"pawnShieldBonus1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawn[MG],-150,550,"kingNearPassedPawn"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::kingNearPassedPawn[EG],-150,550,"kingNearPassedPawnEG"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookBehindPassed[MG] , -500,  500,"rookBehindPassed0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookBehindPassed[EG] , -500,  500,"rookBehindPassed1"));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[1][MG], -1500, 1500,"candidate 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[2][MG], -1500, 1500,"candidate 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[3][MG], -1500, 1500,"candidate 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[4][MG], -1500, 1500,"candidate 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[5][MG], -1500, 1500,"candidate 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[6][MG], -1500, 1500,"candidate 6"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[1][EG], -1500, 1500,"candidateEG 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[2][EG], -1500, 1500,"candidateEG 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[3][EG], -1500, 1500,"candidateEG 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[4][EG], -1500, 1500,"candidateEG 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[5][EG], -1500, 1500,"candidateEG 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::candidate[6][EG], -1500, 1500,"candidateEG 6"));
    */

    /*
    for (int k = 1 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 29 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOB[k][i][MG],-200,200,"mob"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }

    for (int k = 1 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 29 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOB[k][i][EG],-200,200,"mobeg"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }
    */

    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 64 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::PST[k][i][MG],-200,200,"pst"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }
    for (int k = 0 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 64 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::PST[k][i][EG],-200,200,"psteg"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenFile       [MG] , -500,  500,"rookOnOpenFile"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOpp[MG] , -500,  500,"rookOnOpenSemiFileOpp"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOur[MG] , -500,  500,"rookOnOpenSemiFileOur"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenFile       [EG] , -500,  500,"rookOnOpenFileEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOpp[EG] , -500,  500,"rookOnOpenSemiFileOppEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::rookOnOpenSemiFileOur[EG] , -500,  500,"rookOnOpenSemiFileOurEG"));
    */

    computeOptimalK(data);
    Logging::LogIt(Logging::logInfo) << "Optimal K " << Texel::K;

    Logging::LogIt(Logging::logInfo) << "Initial values :";
    for (size_t k = 0; k < guess.size(); ++k) Logging::LogIt(Logging::logInfo) << guess[k].name << " " << guess[k];
    std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeGD(guess, data, batchSize);
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeNaive(guess, data, batchSize);

    Logging::LogIt(Logging::logInfo) << "Optimized values :";
    for (size_t k = 0; k < optim.size(); ++k) Logging::LogIt(Logging::logInfo) << optim[k].name << " " << optim[k];
    for (size_t k = 0; k < data.size(); ++k) delete data[k].p;
}
