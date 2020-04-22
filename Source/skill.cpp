#include "skill.hpp"

#include "tools.hpp"

Move Skill::pick(std::vector<RootScores> & multiPVMoves) {
    const unsigned int multiPV = multiPVMoves.size();
    const ScoreType topScore = multiPVMoves[0].s;
    const ScoreType delta = std::min(ScoreType(topScore - multiPVMoves[multiPV - 1].s), *absValues[P_wp]);
    const ScoreType weakness = 120 - 2 * DynamicConfig::level/5;
    ScoreType maxScore = -MATE;

    Logging::LogIt(Logging::logInfo) << "Picking suboptimal move from " << multiPV << " best move(s)";

    // little shuffling of move thanks to both a deterministic value and a random one
    Move m = INVALIDMOVE;
    for (size_t i = 0; i < multiPV; ++i){
        if ( !VALIDMOVE(multiPVMoves[i].m)) continue;
        // magic formula from Stockfish
        const int add = (  weakness * int(topScore - multiPVMoves[i].s) + delta * randomInt<int,1234>(0,weakness)) / 128;
        std::cout << ToString(multiPVMoves[i].m) << " " << multiPVMoves[i].s << " " << add << std::endl;
        if (multiPVMoves[i].s + add >= maxScore){
            maxScore = multiPVMoves[i].s + add;
            m = multiPVMoves[i].m;
        }
    }
    return m;
}