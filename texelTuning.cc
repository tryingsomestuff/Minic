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
        /*
        static unsigned long long int ncalls = 0;
        ncalls++;
        static std::chrono::time_point<Clock> sTime = Clock::now();
        unsigned long long int elapsed =  (unsigned long long int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - sTime).count();
        if ( ncalls % 10000 == 0 ){
           LogIt(logInfo) << "Sigmoids " << ncalls*1000/elapsed << " calls/sec";
        }
        */
        assert(p);

        // qsearch
        DepthType seldepth = 0;
        double s = ThreadPool::instance().main().qsearchNoPruning(-10000,10000,*p,1,seldepth);
        s *= (p->c == Co_White ? 1:-1);

        /*
        // search
        Move m = INVALIDMOVE;
        DepthType d = 64;
        ScoreType s = 0;
        ThreadPool::instance().main().search(*p,m,4,s,seldepth);
        s *= (p->c == Co_White ? 1:-1);
        */

        /*
        // eval
        float gp;
        double s = eval(*p,gp);
        s *= (p->c == Co_White ? 1:-1);
        */
        return 1. / (1. + std::pow(10, -/*K **/ s / 200.));
    }

    double E(const std::vector<Texel::TexelInput> &data, size_t miniBatchSize) {
        double e = 0;
        for (size_t k = 0; k < miniBatchSize; ++k) {
           const double r = (data[k].result+1)*0.5;
           const double s = Sigmoid(data[k].p);
           e += std::pow(r - s,2);
        }
        e /= miniBatchSize;
        return e;
    }

    void Randomize(std::vector<Texel::TexelInput> & data, size_t miniBatchSize){ std::shuffle(data.begin(), data.end(), std::default_random_engine(0)); }

    double computeOptimalK(const std::vector<Texel::TexelInput> & data) {
       double Kstart = 0.05, Kend = 3.0, Kdelta = 0.15;
       double thisError, bestError = 100;
       for (int i = 0; i < 5; ++i) {
          LogIt(logInfo) << "Computing K Iteration " << i;
          K = Kstart - Kdelta;
          while (K < Kend) {
              LogIt(logInfo) << "...";
              K += Kdelta;
              thisError = E(data,data.size());
              if (thisError <= bestError) {
                 bestError = thisError, Kstart = K;
                 LogIt(logInfo) << "new best K = " << K << " E = " << bestError;
              }
          }
          LogIt(logInfo) << "iteration " << i << " K = " << Kstart << " E = " << bestError;
          Kend = Kstart + Kdelta;
          Kstart -= Kdelta;
          Kdelta /= 10.0;
      }
      K = Kstart;
      return Kstart;
    }


    std::vector<double> ComputeGradient(std::vector<TexelParam<ScoreType> > & x0, std::vector<Texel::TexelInput> &data, size_t gradientBatchSize, bool normalized = true) {
        LogIt(logInfo) << "Computing gradient";
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
            //LogIt(logInfo) << "Gradient " << k << " " << grad;
        }
        if ( normalized){
           double norm = 0;
           for (size_t k = 0; k < x0.size(); ++k) norm += g[k] * g[k];
           norm = sqrt(norm);
           if ( norm > 1e-12){
              for (size_t k = 0; k < x0.size(); ++k) {
                 g[k] /= norm;
                 LogIt(logInfo) << "Gradient normalized " << k << " " << g[k];
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
            LogIt(logInfo) << "gmax " << gmax;
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
            LogIt(logInfo) << "Computing new error";
            double curE = E(data, batchSize);
            LogIt(logInfo) << "Current error " << curE << " best was : " << curEim1;
            LogIt(logInfo) << "Current values :";
            str << it << ";";
            for (size_t k = 0; k < bestParam.size(); ++k) {
               LogIt(logInfo) << bestParam[k].name << " " << bestParam[k];
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
        std::vector<double> previousUpdate(batchSize,0);
        //ScoreType previousValue[batchSize] = {0};
        //std::vector<double> previousGradient = {0};
        while (it < 100000 ) {
            std::vector<double> g = ComputeGradient(bestParam, data, batchSize);
            double learningRate = 20;
            double alpha = 0.9;
            double bestCoeff = 1;
            /*
            // line search
            double coeff = 1;
            LogIt(logInfo) << "Computing initial error (batch size)";
            double initE = E(data, batchSize);
            LogIt(logInfo) << initE;
            for ( int i = 0 ; i < 20 ; ++i){
               LogIt(logInfo) << "Applying gradient, learningRate = " << learningRate*coeff << ", alpha " << alpha;
               for (size_t k = 0; k < bestParam.size(); ++k) {
                   const ScoreType oldValue = bestParam[k];
                   double currentUpdate = ScoreType((1-alpha)*coeff*learningRate * g[k] + alpha * previousUpdate[k]);
                   // try
                   bestParam[k] = oldValue - currentUpdate;
               }
               LogIt(logInfo) << "Computing current error (batch size)";
               double curE = E(data, batchSize);
               LogIt(logInfo) << curE;
               for (size_t k = 0; k < bestParam.size(); ++k) {
                   const ScoreType oldValue = bestParam[k];
                   // restore
                   double currentUpdate = ScoreType((1-alpha)*coeff*learningRate * g[k] + alpha * previousUpdate[k]);
                   bestParam[k] = oldValue + currentUpdate;
               }
               if ( curE <= initE) {
                  bestCoeff = coeff;
                  initE = curE;
               }
               coeff -= 0.049;
            }
            LogIt(logInfo) << "Best coeff " << bestCoeff;
            if ( bestCoeff < 0 ){
                continue; // skip this sample
            }
            */
            LogIt(logInfo) << "Applying gradient, learningRate = " << learningRate*bestCoeff << ", alpha " << alpha;
            for (size_t k = 0; k < bestParam.size(); ++k) {
                const ScoreType oldValue = bestParam[k];
                double currentUpdate = ScoreType((1-alpha)*bestCoeff*learningRate * g[k] + alpha * previousUpdate[k]);
                bestParam[k] = ScoreType(oldValue - currentUpdate);
                previousUpdate[k] = currentUpdate;
                //previousValue[k] = bestParam[k];
            }
            //previousGradient = g;
            LogIt(logInfo) << "Computing new error";// (full data size)";
            double curE = E(data, batchSize/*data.size()*/);
            LogIt(logInfo) << curE;
            // randomize for next iteration
            Randomize(data, batchSize);
            // display
            for (size_t k = 0; k < bestParam.size(); ++k) LogIt(logInfo) << bestParam[k].name << " " << bestParam[k];
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
        for (int loop = 0; loop < 5000; ++loop) {
            for (size_t k = 0; k < bestParam.size(); ++k) {
                Randomize(data, batchSize);
                double initE = E(data, batchSize);
                double curE = -1;
                while ( true ) {
                    const ScoreType oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue - 1);
                    if (bestParam[k] == oldValue) break;
                    curE = E(data, batchSize);
                    if ( curE < initE ){
                        LogIt(logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                        initE = curE;
                    }
                    else{
                        ScoreType oldValue = bestParam[k];
                        bestParam[k] = ScoreType(oldValue + 1);
                        curE = E(data, batchSize);
                        break;
                    }
                }
                while ( true ) {
                    const ScoreType oldValue = bestParam[k];
                    bestParam[k] = ScoreType(oldValue + 1);
                    if (bestParam[k] == oldValue) break;
                    curE = E(data, batchSize);
                    if ( curE < initE ){
                        LogIt(logInfo) << curE << " " << initE << " " << bestParam[k].name << " " << bestParam[k];
                        initE = curE;
                    }
                    else{
                        ScoreType oldValue = bestParam[k];
                        bestParam[k] = ScoreType(oldValue - 1);
                        curE = E(data, batchSize);
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

void TexelTuning(const std::string & filename) {
    std::ifstream stream(filename);
    std::string line;
    std::vector<Texel::TexelInput> data;
    LogIt(logInfo) << "Running texel tuning with file " << filename;
    int count = 0;
    while (std::getline(stream, line)){
        nlohmann::json o;
        try { o = nlohmann::json::parse(line); }
        catch (...) { LogIt(logFatal) << "Cannot parse json " << line; }
        std::string fen = o["fen"];
        Position * p = new Position;
        readFEN(fen,*p,true);
        /*if (std::abs(o["result"].get<int>()) == 1)*/ data.push_back({p, o["result"]});
        ++count;
        if (count % 50000 == 0) LogIt(logInfo) << count << " position read";
    }

    LogIt(logInfo) << "Data size : " << data.size();

    //size_t batchSize = data.size(); // batch
    //size_t batchSize = 1024 ; // mini
    size_t batchSize = 1; // stochastic

    //for(int k=0 ; k<13; ++k){Values[k] = 450; ValuesEG[k] = 450;}

    std::vector<Texel::TexelParam<ScoreType> > guess;
    //guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wp+PieceShift], 20,  2000, "pawn",     [](const ScoreType& s){Values[P_bp+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wn+PieceShift],   150,  400, "knight",   [](const ScoreType& s){Values[P_bn+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wb+PieceShift],   150,  400, "bishop",   [](const ScoreType& s){Values[P_bb+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wr+PieceShift],   200,  700, "rook",     [](const ScoreType& s){Values[P_br+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(Values[P_wq+PieceShift],   600, 1200, "queen",    [](const ScoreType& s){Values[P_bq+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wp+PieceShift],  50,  200, "EGpawn",   [](const ScoreType& s){ValuesEG[P_bp+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wn+PieceShift], 150,  400, "EGknight", [](const ScoreType& s){ValuesEG[P_bn+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wb+PieceShift], 150,  400, "EGbishop", [](const ScoreType& s){ValuesEG[P_bb+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wr+PieceShift], 200,  700, "EGrook",   [](const ScoreType& s){ValuesEG[P_br+PieceShift] = -s;}));
    guess.push_back(Texel::TexelParam<ScoreType>(ValuesEG[P_wq+PieceShift], 600, 1400, "EGqueen",  [](const ScoreType& s){ValuesEG[P_bq+PieceShift] = -s;}));
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

    //computeOptimalK(data);

    LogIt(logInfo) << "Optimal K " << Texel::K;

    LogIt(logInfo) << "Initial values :";
    for (size_t k = 0; k < guess.size(); ++k) LogIt(logInfo) << guess[k].name << " " << guess[k];
    std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeGD(guess, data, batchSize);
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeSecante(guess, data, batchSize);
    //std::vector<Texel::TexelParam<ScoreType> > optim = Texel::TexelOptimizeNaive(guess, data, batchSize);

    LogIt(logInfo) << "Optimized values :";
    for (size_t k = 0; k < optim.size(); ++k) LogIt(logInfo) << optim[k].name << " " << optim[k];
    for (size_t k = 0; k < data.size(); ++k) delete data[k].p;
}
