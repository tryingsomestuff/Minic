#include "definition.hpp"
#include "position.hpp"
#include "positionTools.hpp"
#include "searcher.hpp"
#include "threading.hpp"

struct SEETest {
   const std::string fen;
   const Move        m;
   const ScoreType   threshold;
};

bool TestSEE() {
   const ScoreType P = value(P_wp);
   const ScoreType N = value(P_wn);
   const ScoreType B = value(P_wb);
   const ScoreType R = value(P_wr);
   const ScoreType Q = value(P_wq);

   std::cout << "Piece values " << P << "\t" << N << "\t" << B << "\t" << R << "\t" << Q << "\t" << std::endl;

   // from Vajolet
   std::list<SEETest> posList;
   /* capture initial move */
   posList.emplace_back("3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - - 0 1", ToMove(Sq_d3, Sq_d4, T_capture), ScoreType(P - R + N - P));
   posList.emplace_back("1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1", ToMove(Sq_d3, Sq_e5, T_capture), ScoreType(N - N + B - R + N));
   posList.emplace_back("1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - - 0 1", ToMove(Sq_d3, Sq_e5, T_capture), ScoreType(P - N));
   posList.emplace_back("rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q - 1 1", ToMove(Sq_h4, Sq_f6, T_capture), ScoreType(N - B + P));
   posList.emplace_back("r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - - 1 1", ToMove(Sq_g4, Sq_f3, T_capture), ScoreType(N - B));
   posList.emplace_back("r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq - 2 1", ToMove(Sq_c6, Sq_d4, T_capture), ScoreType(P - N + N - P));
   posList.emplace_back("r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ - 2 1", ToMove(Sq_e6, Sq_g7, T_capture), ScoreType(B - N));
   posList.emplace_back("r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ - 2 1", ToMove(Sq_e6, Sq_g7, T_capture), ScoreType(B));
   posList.emplace_back("rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - 0 1", ToMove(Sq_d6, Sq_h2, T_capture), ScoreType(P - B));
   posList.emplace_back("rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq - 5 1", ToMove(Sq_d6, Sq_h2, T_capture), ScoreType(P));
   posList.emplace_back("r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq - 2 1", ToMove(Sq_g4, Sq_f3, T_capture), ScoreType(N - B + P));
   posList.emplace_back("rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ - 1 1", ToMove(Sq_b4, Sq_c3, T_capture), ScoreType(N - B));
   posList.emplace_back("r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 1", ToMove(Sq_b6, Sq_b2, T_capture), ScoreType(P - Q));
   posList.emplace_back("r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - - 1 1", ToMove(Sq_f6, Sq_d5, T_capture), ScoreType(P - N));
   posList.emplace_back("1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - - 0 29", ToMove(Sq_b8, Sq_b3, T_capture), ScoreType(P - R));
   posList.emplace_back("1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - - 0 1", ToMove(Sq_b8, Sq_b4, T_capture), ScoreType(P));
   posList.emplace_back("2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - - 1 22", ToMove(Sq_c8, Sq_c1, T_capture), ScoreType(R - R));
   posList.emplace_back("1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - - 0 1", ToMove(Sq_a5, Sq_a2, T_capture), ScoreType(P - Q));
   posList.emplace_back("1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - - 0 1", ToMove(Sq_a5, Sq_a2, T_capture), ScoreType(P));
   posList.emplace_back("r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - - 4 14", ToMove(Sq_d5, Sq_e7, T_capture), ScoreType(B - N));
   posList.emplace_back("rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - - 0 1", ToMove(Sq_d3, Sq_e4, T_capture), ScoreType(P));
   /* non capture initial move */
   posList.emplace_back("rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - 0 1", ToMove(Sq_e4, Sq_d6, T_std), ScoreType(-N + P));
   posList.emplace_back("r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq - 0 1", ToMove(Sq_e4, Sq_d6, T_std), ScoreType(-N + N));
   posList.emplace_back("2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k - 0 1", ToMove(Sq_d5, Sq_c6, T_std), ScoreType(-B));
   posList.emplace_back("2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k - 0 1", ToMove(Sq_d5, Sq_c6, T_std), ScoreType(-B + B));
   posList.emplace_back("2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - - 0 1", ToMove(Sq_d5, Sq_c6, T_std), ScoreType(-B + B - N));
   /* initial move promotion */
   posList.emplace_back("5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - 0 1", ToMove(Sq_d7, Sq_d8, T_promq), ScoreType(-P + Q));
   posList.emplace_back("r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ - 0 1", ToMove(Sq_d7, Sq_d8, T_promq), ScoreType(-P + Q - Q));
   posList.emplace_back("5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ - 0 1", ToMove(Sq_d7, Sq_d8, T_promq), ScoreType(-P + Q - Q + B));
   posList.emplace_back("4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk - 0 1", ToMove(Sq_c7, Sq_c8, T_promq), ScoreType(-P + Q - Q));
   posList.emplace_back("4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1", ToMove(Sq_c7, Sq_c8, T_promq), ScoreType(-P + Q - Q + B));
   posList.emplace_back("4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1", ToMove(Sq_c7, Sq_c8, T_promn), ScoreType(-P + N));
   posList.emplace_back("4kbnr/p1P1pppp/1b6/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk - 0 1", ToMove(Sq_c7, Sq_c8, T_promn), ScoreType(-P + N));
   /* initial move En Passant */
   posList.emplace_back("4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6 0 2", ToMove(Sq_g5, Sq_f6, T_ep), ScoreType(P - P));
   posList.emplace_back("4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6 0 2", ToMove(Sq_g5, Sq_f6, T_ep), ScoreType(P - P));
   /* initial move capture promotion */
   posList.emplace_back("1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk - 0 1", ToMove(Sq_c7, Sq_b8, T_cappromq), ScoreType(N + (-P + Q) - Q));
   posList.emplace_back("rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq - 0 1", ToMove(Sq_g1, Sq_h2, T_capture), ScoreType(B));
   posList.emplace_back("3N4/2K5/2n1n3/1k6/8/8/8/8 b - - 0 1", ToMove(Sq_c6, Sq_d8, T_capture), ScoreType(N));
   posList.emplace_back("3N4/2K5/2n5/1k6/8/8/8/8 b - - 0 1", ToMove(Sq_c6, Sq_d8, T_capture), ScoreType(0));
   /* promotion inside the loop */
   posList.emplace_back("3N4/2P5/2n5/1k6/8/8/8/4K3 b - - 0 1", ToMove(Sq_c6, Sq_d8, T_capture), ScoreType(N - N - Q + P));
   posList.emplace_back("3N4/2P5/2n5/1k6/8/8/8/4K3 b - - 0 1", ToMove(Sq_c6, Sq_b8, T_capture), ScoreType(-N - Q + P));
   posList.emplace_back("3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - - 0 1", ToMove(Sq_d2, Sq_d8, T_capture), ScoreType(N));
   posList.emplace_back("3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - - 0 1", ToMove(Sq_c7, Sq_d8, T_promq), ScoreType((N - P + Q) - Q + R));
   /* double promotion inside the loop */
   posList.emplace_back("r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - - 0 1", ToMove(Sq_e6, Sq_d8, T_capture), ScoreType(N));
   posList.emplace_back("8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - - 0 1", ToMove(Sq_e3, Sq_d1, T_capture), ScoreType(0));
   posList.emplace_back("8/8/1k6/8/8/2N1N3/2p1p1K1/3n4 w - - 0 1", ToMove(Sq_c3, Sq_d1, T_capture), ScoreType(N - N - Q + P));
   posList.emplace_back("8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - - 0 1", ToMove(Sq_c3, Sq_d1, T_capture), ScoreType(N - (N - P + Q) + Q));
   posList.emplace_back("r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4", ToMove(Sq_e1, Sq_h1, T_wks), ScoreType(0));

#ifdef WITH_NNUE
   NNUEEvaluator evaluator;
#endif

   for (const auto& t : posList) {
      RootPosition p;
#ifdef WITH_NNUE
      p.associateEvaluator(evaluator);
#endif
      readFEN(t.fen, p, true);
      Logging::LogIt(Logging::logInfo) << "============================== " << t.fen << " ==";
      const ScoreType s = ThreadPool::instance().main().SEE(p, t.m);
      if (s != t.threshold)
         Logging::LogIt(Logging::logError) << "wrong SEE value == " << ToString(p) << "\n" << ToString(t.m) << " " << s << " " << t.threshold;
   }
   return true;
}