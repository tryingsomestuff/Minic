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
    DepthType seldepth = 0;
    double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,*p,1,seldepth);
    s *= (p->c == Co_White ? +1:-1);

    /*
    // search
    Move m = INVALIDMOVE;
    DepthType d = 64;
    ScoreType s = 0;
    ThreadPool::instance().main().search(*p,m,4,s,seldepth);
    s *= (p->c == Co_White ? +1:-1);
    */

    /*
    // eval
    float gp;
    double s = eval(*p,gp);
    s *= (p->c == Co_White ? +1:-1);
    */

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
    std::vector<double> g;
    const ScoreType dx = 2;
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

std::vector<TexelParam<ScoreType> > TexelOptimizeSecante(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> & data, size_t batchSize){

    DynamicConfig::disableTT = true;

    std::ofstream str("tuning.csv");
    int it = 0;
    Randomize(data, batchSize);
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    ScoreType dx = 2;
    double curEim1 = E(data, batchSize);
    while (true) {
        std::vector<double> g_i = ComputeGradient(bestParam, data, batchSize, false);
        double gmax = -1;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            const ScoreType oldValue = bestParam[k];
            bestParam[k] = oldValue - dx;
            gmax = std::max(gmax,std::fabs(g_i[k]));
        }
        Logging::LogIt(Logging::logInfo) << "gmax " << gmax;
        if ( gmax < 1e-14 ) break;
        std::vector<double> g_im1 = ComputeGradient(bestParam, data, batchSize, false);
        for (size_t k = 0; k < bestParam.size(); ++k) {
            const ScoreType oldValue = bestParam[k];
            bestParam[k] = oldValue + dx;
        }

        for (size_t k = 0; k < bestParam.size(); ++k) {
            const ScoreType oldValue = bestParam[k];
            if ( std::fabs(g_i[k] - g_im1[k] >= 1e-16 )) bestParam[k] = ScoreType(oldValue - dx * g_i[k] / (g_i[k]-g_im1[k]));
        }
        Logging::LogIt(Logging::logInfo) << "Computing new error";
        double curE = E(data, batchSize);
        Logging::LogIt(Logging::logInfo) << "Current error " << curE << " best was : " << curEim1;
        Logging::LogIt(Logging::logInfo) << "Current values :";
        str << it << ";";
        for (size_t k = 0; k < bestParam.size(); ++k) {
            Logging::LogIt(Logging::logInfo) << bestParam[k].name << " " << bestParam[k];
            str << bestParam[k] << ";";
        }
        str << curE << ";" << curEim1 << ";  ";
        for (size_t k = 0; k < bestParam.size(); ++k) {
            str << g_i[k] << ";" << g_im1[k] << ";  ";
        }
        str << std::endl;
        curEim1 = curE;
        // randomize for next iteration
        Randomize(data, batchSize);
        ++it;
    }
    return bestParam;
}

std::vector<TexelParam<ScoreType> > TexelOptimizeGD(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize) {
    DynamicConfig::disableTT = true;
    std::ofstream str("tuning.csv");
    int it = 0;
    Randomize(data, batchSize);
    std::vector<TexelParam<ScoreType> > bestParam = initialGuess;
    std::vector<ScoreType> previousUpdate(batchSize,0);
    std::vector<ScoreType> backup(batchSize,0);
    while (it < 100000 ) {
        std::vector<double> g = ComputeGradient(bestParam, data, batchSize, false);

        double gmax = -1;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            gmax = std::max(gmax,std::fabs(g[k]));
        }
        Logging::LogIt(Logging::logInfo) << "gmax " << gmax;

        if ( gmax < 0.0000001 ) break;

        double learningRate = 1./gmax;
        double alpha = 0.1;

        double baseE = E(data, batchSize);
        Logging::LogIt(Logging::logInfo) << "Base E "  << baseE;

        double coef = 0;
        size_t lsStep = 20;
        // lin-search
        for(size_t i = 1 ; i < lsStep ; ++i){
            const double curCoef = bestParam.size()*i*1./(lsStep-1);
            //try
            for (size_t k = 0; k < bestParam.size(); ++k) {
               backup[k] = bestParam[k];
               const ScoreType currentUpdate = ScoreType((1-alpha)*learningRate * curCoef * g[k] + alpha * previousUpdate[k]);
               //Logging::LogIt(Logging::logInfo) << i << " " << k << " update " << currentUpdate;
               bestParam[k] = ScoreType(backup[k] - currentUpdate);
            }
            //check
            double curE = E(data, batchSize);
            //Logging::LogIt(Logging::logInfo) << "ls coef " << curCoef << "LS E "  << curE;
            if ( curE < baseE ){
               baseE=curE;
               coef = curCoef;
            }
            //revert
            for (size_t k = 0; k < bestParam.size(); ++k) {
               bestParam[k] = backup[k];
            }
        }

        if (coef == 0 ) {
            Logging::LogIt(Logging::logInfo) << "Line search failed";
            break;
        }

        Logging::LogIt(Logging::logInfo) << "Applying gradient, learningRate = " << learningRate << ", alpha " << alpha << " LS best : " << coef;
        for (size_t k = 0; k < bestParam.size(); ++k) {
            const ScoreType oldValue = bestParam[k];
            const ScoreType currentUpdate = ScoreType((1-alpha)*learningRate * coef * g[k] + alpha * previousUpdate[k]);
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
        if ( it % 10 == 0 ){
            str << it << ";";
            for (size_t k = 0; k < bestParam.size(); ++k) str << bestParam[k] << ";";
            str << curE << std::endl;
        }
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

struct Particle{
    std::vector<ScoreType> param;
    std::vector<ScoreType> best;
    std::vector<ScoreType> velocity;
    double bestScore;
};

void RandomParam(std::vector<ScoreType> & p, ScoreType mini, ScoreType maxi){
    static std::mt19937 mt(42); // fixed seed for ZHash !!!
    std::uniform_int_distribution<unsigned long long int> dist(mini, maxi);
    for(auto it = p.begin() ; it != p.end() ; ++it){
        *it = (ScoreType)dist(mt);
    }
}

void copyParam( std::vector<TexelParam<ScoreType> > & param, const std::vector<ScoreType> & value, std::vector<ScoreType> & backup){
    for(size_t k = 0 ; k < param.size() ; ++k){
        backup[k] = param[k];
        param[k] = value[k];
    }
}

void restoreParam( std::vector<TexelParam<ScoreType> > & param, const std::vector<ScoreType> & value){
    for(size_t k = 0 ; k < param.size() ; ++k){
        param[k] = value[k];
    }
}

std::vector<TexelParam<ScoreType> > TexelOptimizePSO(const std::vector<TexelParam<ScoreType> >& initialGuess, std::vector<Texel::TexelInput> &data, const size_t batchSize) {
    DynamicConfig::disableTT = true;
    std::ofstream str("tuning.csv");

    int nbParticle = 35;

    std::vector< Particle > particles(nbParticle);

    double eBest = 1000000000000;

    std::vector<TexelParam<ScoreType> > curParam = initialGuess;

    std::vector<ScoreType> swarmBest(curParam.size());
    copyParam(curParam,swarmBest,swarmBest);

    int part = 0;
    for(auto it = particles.begin() ; it != particles.end() ; ++it){
        it->param.resize(curParam.size());
        it->best.resize(curParam.size());
        it->velocity.resize(curParam.size());
        RandomParam(it->param,50,1500);
        it->best = it->param;
        RandomParam(it->velocity,-10,10);
        std::vector<ScoreType> backup(curParam.size());
        copyParam(curParam,it->param,backup);
        double e = E(data, batchSize);
        it->bestScore = e;
        if ( e < eBest ){
            swarmBest = it->param;
            eBest = e;
        }
        restoreParam(curParam,backup);
        Logging::LogIt(Logging::logInfo) << "Initial particle " << part++;
        for (size_t k = 0; k < curParam.size(); ++k) Logging::LogIt(Logging::logInfo) << curParam[k].name << " " << it->param[k] << " " << it->velocity[k] << " " << it->bestScore;
    }

    Logging::LogIt(Logging::logInfo) << "Initial best ";
    for (size_t k = 0; k < curParam.size(); ++k) Logging::LogIt(Logging::logInfo) << curParam[k].name << " " << swarmBest[k] << " " << eBest;

    double omega = 0.8;
    double phip = 1.4;
    double phig = 1.4;

    int count = 0;

    while ( true ) {
        Randomize(data, batchSize);
        for(auto it = particles.begin() ; it != particles.end() ; ++it){
            for(size_t k = 0 ; k < curParam.size() ; ++k){
                static std::mt19937 mt(42); // fixed seed
                static std::uniform_real_distribution<double> dist(0,1);
                double rp = dist(mt);
                double rg = dist(mt);
                (*it).velocity[k] = ScoreType(omega * (*it).velocity[k] + phip * rp * ((*it).best[k] - (*it).param[k]) + phig * rg * (swarmBest[k] - ((*it).param[k])));
            }
            for(size_t k = 0 ; k < curParam.size() ; ++k){
                (*it).param[k] += (*it).velocity[k];
            }
            std::vector<ScoreType> backup(curParam.size());
            copyParam(curParam,it->param,backup);
            double e = E(data, batchSize);
            if ( e < it->bestScore ){
                it->best = it->param;
                it->bestScore = e;
                if ( e < eBest){
                    eBest = e;
                    swarmBest = it->best;
                }
            }
            restoreParam(curParam,backup);
        }

        std::cout << "=====================" << std::endl;

        for(auto it = particles.begin() ; it != particles.end() ; ++it){
            for (size_t k = 0; k < curParam.size(); ++k) Logging::LogIt(Logging::logInfo) << curParam[k].name << " " << it->param[k] << " " << it->velocity[k] << " " << it->bestScore;
        }

        std::cout << "------------------" << std::endl;

        restoreParam(curParam,swarmBest);
        // display
        for (size_t k = 0; k < curParam.size(); ++k) Logging::LogIt(Logging::logInfo) << curParam[k].name << " " << curParam[k];
        // write
        //if ( count % 10 == 0 ){
            str << count << ";";
            for (size_t k = 0; k < curParam.size(); ++k) str << curParam[k] << ";";
            str << eBest << std::endl;
        //}
        count++;
    }
    return curParam;
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

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][1], -30, 30, "ap"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][2], -30, 30, "an"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][3], -30, 30, "ab"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][4], -30, 30, "ar"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][5], -30, 30, "aq"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[0][6], -30, 30, "ak"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][1], -30, 30, "dp"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][2], -30, 30, "dn"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][3], -30, 30, "db"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][4], -30, 30, "dr"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][5], -30, 30, "dq"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_att_def_weight[1][6], -30, 30, "dk"));

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[0]  , -150, 150,"katt_attack_ 0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[1]  , -150, 150,"katt_attack_ 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[2]  , -150, 150,"katt_attack_ 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[3]  , -150, 150,"katt_attack_ 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[4]  , -150, 150,"katt_attack_ 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[5]  , -150, 150,"katt_attack_ 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_attack_weight[6]  , -150, 150,"katt_attack_ 6"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[0]  , -150, 150,"katt_defence_ 0"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[1]  , -150, 150,"katt_defence_ 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[2]  , -150, 150,"katt_defence_ 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[3]  , -150, 150,"katt_defence_ 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[4]  , -150, 150,"katt_defence_ 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[5]  , -150, 150,"katt_defence_ 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_defence_weight[6]  , -150, 150,"katt_defence_ 6"));

    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_max , -400, 800, "kattmax",[](const ScoreType & ){initEval();}));
    guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::katt_scale , 0, 100, "kattscalre",[](const ScoreType & ){initEval();}));
    */

    /*
    guess.push_back(Texel::TexelParam<ScoreType>(bishopPairBonus , -50,  50,"bishop pair"));
    guess.push_back(Texel::TexelParam<ScoreType>(knightPairMalus , -50,  50,"knight pair"));
    guess.push_back(Texel::TexelParam<ScoreType>(rookPairMalus   , -50,  50,"rook pair"));

    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[1]  , -150, 150,"passer 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[2]  , -150, 150,"passer 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[3]  , -150, 150,"passer 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[4]  , -150, 150,"passer 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[5]  , -150, 150,"passer 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonus[6]  , -150, 150,"passer 6"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[1], -150, 150,"passer 1"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[2], -150, 150,"passer 2"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[3], -150, 150,"passer 3"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[4], -150, 150,"passer 4"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[5], -150, 150,"passer 5"));
    guess.push_back(Texel::TexelParam<ScoreType>(passerBonusEG[6], -150, 150,"passer 6"));

    guess.push_back(Texel::TexelParam<ScoreType>(kingNearPassedPawnEG,-15,55,"kingNearPassedPawnEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(doublePawnMalus     ,-15,55,"doublePawnMalus"));
    guess.push_back(Texel::TexelParam<ScoreType>(doublePawnMalusEG   ,-15,55,"doublePawnMalusEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(isolatedPawnMalus   ,-15,55,"isolatedPawnMalus"));
    guess.push_back(Texel::TexelParam<ScoreType>(isolatedPawnMalusEG ,-15,55,"isolatedPawnMalusEG"));
    guess.push_back(Texel::TexelParam<ScoreType>(pawnShieldBonus     ,-15,55,"pawnShieldBonus"));
    */

    /*
    for (int k = 1 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 29 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOB[k][i],-200,200,"mob"+std::to_string(k)+"_"+std::to_string(i),[k,i](const ScoreType & s){EvalConfig::MOBEG[k][i] = s;}));
        }
    }
    */

    /*
    for (int k = 1 ; k < 6 ; ++k ){
        for(int i = 0 ; i < 29 ; ++i){
           guess.push_back(Texel::TexelParam<ScoreType>(EvalConfig::MOBEG[k][i],-200,200,"mobeg"+std::to_string(k)+"_"+std::to_string(i)));
        }
    }
    */

    computeOptimalK(data);
    Logging::LogIt(Logging::logInfo) << "Optimal K " << Texel::K;

    Logging::LogIt(Logging::logInfo) << "Initial values :";
    for (size_t k = 0; k < guess.size(); ++k) Logging::LogIt(Logging::logInfo) << guess[k].name << " " << guess[k];
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeGD(guess, data, batchSize);
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizePSO(guess, data, batchSize);
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeSecante(guess, data, batchSize);
    std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeNaive(guess, data, batchSize);

    Logging::LogIt(Logging::logInfo) << "Optimized values :";
    for (size_t k = 0; k < optim.size(); ++k) Logging::LogIt(Logging::logInfo) << optim[k].name << " " << optim[k];
    for (size_t k = 0; k < data.size(); ++k) delete data[k].p;
}
