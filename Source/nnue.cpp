#include "nnue.hpp"

#ifdef WITH_NNUE

#include "com.hpp"
#include "dynamicConfig.hpp"
#include "evalDef.hpp"
#include "logging.hpp"
#include "moveGen.hpp"
#include "score.hpp"
#include "smp.hpp"

#include "nnue_impl.hpp"

// this is used to scale NNUE score to classic eval score. 
// This way search params can remain the same ... more or less ...
// see compute_scaling
int NNUEWrapper::NNUEscaling = 64; // from 32 to 128      x_scaled = x * NNUEscaling / 64

namespace NNUEWrapper{

void compute_scaling(int count){
    static std::random_device rd;
    static std::mt19937 g(63); // fixed seed !

    Logging::LogIt(Logging::logInfo) << "Automatic computation of NNUEscaling with " << count << " random positions ...";

    Position p;
    NNUEEvaluator evaluator;
    p.associateEvaluator(evaluator);
    readFEN(startPosition,p,true);

    EvalData data;

    float factor = 0;
    int k = 0;

    bool bkTT = DynamicConfig::disableTT;
    DynamicConfig::disableTT = true;

    while(k < count){
        MoveList moves;
        MoveGen::generate<MoveGen::GP_all>(p, moves, false);
        if (moves.empty()){
            readFEN(startPosition,p,true);
            continue;
        }
        std::shuffle(moves.begin(), moves.end(),g);
        bool found = false;
        for (auto it = moves.begin(); it != moves.end(); ++it) {
            Position p2 = p;
            if (!applyMove(p2, *it)) continue;
            found = true;
            p = p2;
            const Square to = Move2To(*it);
            if (p.c == Co_White && to == p.king[Co_Black]){
               readFEN(startPosition,p,true);
               break;
            }
            if (p.c == Co_Black && to == p.king[Co_White]){
               readFEN(startPosition,p,true);
               break;
            }

            DynamicConfig::useNNUE = false;
            const ScoreType eStd = eval(p,data,ThreadPool::instance().main());
            DynamicConfig::useNNUE = true;
            const ScoreType eNNUE = eval(p,data,ThreadPool::instance().main());

            std::cout << GetFEN(p) << " " << eStd << " " << eNNUE << std::endl;

            if ( std::abs(eStd) < 1000 && eStd*eNNUE > 0 ){
               ++k;
               factor += eStd/eNNUE;
            }
            break;
        }
        if ( !found ) readFEN(startPosition,p,true);        
    }
    NNUEWrapper::NNUEscaling = int(factor*64/k);
    Logging::LogIt(Logging::logInfo) << "NNUEscaling " << NNUEWrapper::NNUEscaling << " (" << factor/k << ")";

    DynamicConfig::disableTT = bkTT;
}

} // NNUEWrapper

#endif // WITH_NNUE