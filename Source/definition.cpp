#include "definition.hpp"

CONST_PIECE_TUNING ScoreType   Values[NbPiece]        = { -8000, -1103, -538, -393, -359, -85, 0, 85, 359, 393, 538, 1103, 8000 };
CONST_PIECE_TUNING ScoreType   ValuesEG[NbPiece]      = { -8000, -1076, -518, -301, -290, -93, 0, 93, 290, 301, 518, 1076, 8000 };

float MoveDifficultyUtil::variability = 1.0f; 
