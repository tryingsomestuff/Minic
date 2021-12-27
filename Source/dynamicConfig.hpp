#pragma once

#include "definition.hpp"

/*!
 * Things that are widely used 
 * with values that can be varying due to 
 * GUI or CLI interaction
 */
namespace DynamicConfig {
extern bool         mateFinder;
extern bool         disableTT;
extern unsigned int ttSizeMb;
extern bool         fullXboardOutput;
extern bool         debugMode; // activate output in a file (see debugFile)
extern int          minOutputLevel; // minimum output level
extern std::string  debugFile;
extern unsigned int level;
extern unsigned int randomOpen;
extern unsigned int threads;
extern std::string  syzygyPath;
extern bool         FRC;
extern bool         UCIPonder;
extern unsigned int multiPV;
extern ScoreType    contempt;
extern ScoreType    contemptMG;
extern bool         limitStrength;
extern int          strength;
extern bool         nodesBasedLevel;
extern bool         useNNUE;
extern bool         forceNNUE;
extern std::string  NNUEFile;
extern int          NNUEScaling;
extern int          NNUEThreshold;
extern bool         genFen;
extern unsigned int genFenDepth;
extern unsigned int genFenDepthEG;
extern unsigned int randomPly;
extern unsigned int moveOverHead;
extern bool         armageddon;
extern bool         withWDL;
extern bool         bongCloud;

extern std::string opponent;
extern int         ratingAdv;
extern bool        ratingAdvReceived;
extern double      ratingFactor;

extern int styleComplexity;
extern int styleMaterial;
extern int stylePositional;
extern int styleDevelopment;
extern int styleMobility;
extern int styleAttack;
extern int stylePawnStruct;
extern int styleForwardness;

extern bool stylized;
} // namespace DynamicConfig
