
#define TB_CUSTOM_BSWAP32 swapbits32
#define TB_CUSTOM_BSWAP64 swapbits

extern "C" {
#include "tbprobe.c"
}

namespace SyzygyTb {
    
const ScoreType TB_CURSED_SCORE = 1;
const ScoreType TB_WIN_SCORE = 1000;

const ScoreType valueMap[5]     = {-TB_WIN_SCORE, -TB_CURSED_SCORE, 0, TB_CURSED_SCORE, TB_WIN_SCORE};
const ScoreType valueMapNo50[5] = {-TB_WIN_SCORE, -TB_WIN_SCORE,    0, TB_WIN_SCORE,    TB_WIN_SCORE};

int MAX_TB_MEN = -1;

static MType getMoveType(const Position &p, unsigned res){
    const bool isCap = (p.b[TB_GET_TO(res)] != P_none);
    switch (TB_GET_PROMOTES(res)) {
       case TB_PROMOTES_QUEEN:  return isCap? T_cappromq:T_promq;
       case TB_PROMOTES_ROOK:   return isCap? T_cappromr:T_promr;
       case TB_PROMOTES_BISHOP: return isCap? T_cappromb:T_promb;
       case TB_PROMOTES_KNIGHT: return isCap? T_cappromn:T_promn;
       default:                 return isCap? T_capture :T_std;
    }
}

Move getMove(const Position &p, unsigned res) {
    return ToMove(TB_GET_FROM(res), TB_GET_TO(res), TB_GET_EP(res) ? T_ep : getMoveType(p,res)); // Note: castling not possible
}

bool initTB(const std::string &path){
   Logging::LogIt(Logging::logInfo) << "Init tb";
   bool ok = tb_init(path.c_str());
   if (!ok) MAX_TB_MEN = 0;
   else     MAX_TB_MEN = TB_LARGEST;
   Logging::LogIt(Logging::logInfo) << "MAX_TB_MEN :" << MAX_TB_MEN;
   return ok;
}

int probe_root(ThreadContext & context, const Position &p, ScoreType &score, MoveList &rootMoves){
   if ( MAX_TB_MEN <= 0 ) return -1;
   score = 0;
   unsigned results[TB_MAX_MOVES];
   unsigned result = tb_probe_root(p.allPieces[Co_White],
                                   p.allPieces[Co_Black],
                                   p.whiteKing()   | p.blackKing(),
                                   p.whiteQueen()  | p.blackQueen(),
                                   p.whiteRook()   | p.blackRook(),
                                   p.whiteBishop() | p.blackBishop(),
                                   p.whiteKnight() | p.blackKnight(),
                                   p.whitePawn()   | p.blackPawn(),
                                   p.fifty,
                                   p.castling != C_none,
                                   p.ep == INVALIDSQUARE ? 0 : p.ep,
                                   p.c == Co_White,
                                   results);

   if (result == TB_RESULT_FAILED) return -1;

   const unsigned wdl = TB_GET_WDL(result);
   assert(wdl<5);
   score = valueMap[wdl];
   if (context.isRep(p,false)) rootMoves.push_back(getMove(p,result));
   else {
      unsigned res;
      for (int i = 0; (res = results[i]) != TB_RESULT_FAILED; i++) {
         if (TB_GET_WDL(res) >= wdl) rootMoves.push_back(getMove(p,res));
      }
   }
   return TB_GET_DTZ(result);
}

int probe_wdl(const Position &p, ScoreType &score, bool use50MoveRule){
   if ( MAX_TB_MEN <= 0 ) return -1;
   score = 0;
   unsigned result = tb_probe_wdl(p.whitePiece,
                                  p.blackPiece,
                                  p.whiteKing()   | p.blackKing(),
                                  p.whiteQueen()  | p.blackQueen(),
                                  p.whiteRook()   | p.blackRook(),
                                  p.whiteBishop() | p.blackBishop(),
                                  p.whiteKnight() | p.blackKnight(),
                                  p.whitePawn()   | p.blackPawn(),
                                  p.fifty,
                                  p.castling != C_none,
                                  p.ep == INVALIDSQUARE ? 0 : p.ep,
                                  p.c == Co_White);

   if (result == TB_RESULT_FAILED) return 0;

   unsigned wdl = TB_GET_WDL(result);
   assert(wdl<5);
   if (use50MoveRule) score = valueMap[wdl];
   else               score = valueMapNo50[wdl];
   return 1;
}

} // SyzygyTb
