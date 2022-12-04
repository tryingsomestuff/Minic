#include "definition.hpp"

CONST_PIECE_TUNING array1d<ScoreType,NbPiece> Values    = {-8000, -1103, -538, -393, -359, -85, 0, 85, 359, 393, 538, 1103, 8000};
CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesEG  = {-8000, -1076, -518, -301, -290, -93, 0, 93, 290, 301, 518, 1076, 8000};
CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesGP  = {-8000, -1076, -518, -301, -290, -93, 0, 93, 290, 301, 518, 1076, 8000};
CONST_PIECE_TUNING array1d<ScoreType,NbPiece> ValuesSEE = {-8000, -1048, -571, -296, -231, -86, 0, 86, 231, 296, 571, 1048, 8000};

float MoveDifficultyUtil::variability = 1.0f;
