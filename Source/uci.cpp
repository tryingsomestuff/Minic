#include "uci.hpp"

#include "cli.hpp"
#include "com.hpp"
#include "logging.hpp"
#include "opponent.hpp"
#include "option.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "timeMan.hpp"
#include "tools.hpp"

namespace UCI {

void init() {
   Logging::ct = Logging::CT_uci;
   Logging::LogIt(Logging::logInfo) << "Init uci";
   COM::init(COM::p_uci);
}

void uci() {
   bool iterate = true;

   while (iterate) {
      COM::readLine();
      std::istringstream iss(COM::command);
      std::string uciCommand;
      iss >> uciCommand;
      Logging::LogIt(Logging::logInfo) << "uci received command " << uciCommand;

      ///@todo protect some stuff when search is running !

      if (uciCommand == "uci") {
         std::string version = MinicVersion;
         if (Distributed::moreThanOneProcess()) version += "_" + std::to_string(Distributed::worldSize) + "proc";
         Logging::LogIt(Logging::logGUI) << "id name Minic " << version;
         Logging::LogIt(Logging::logGUI) << "id author Vivien Clauzon";
         Options::displayOptionsUCI();
         Logging::LogIt(Logging::logGUI) << "uciok";
      }
      else if (uciCommand == "setoption") {
         std::string tmpstr, key, val;
         iss >> tmpstr; //"name"
         iss >> key;
         iss >> tmpstr;     //"value"
         getline(iss, val); // get all the remaining stuff on the line
         //iss >> val;
         val = trim(val);
         if (!Options::SetValue(key, val)) Logging::LogIt(Logging::logError) << "Unable to set value " << key << " = " << val;
      }
      else if (uciCommand == "isready") {
         // callback that are not triggered immediatly when the option is received.
         Opponent::init(); // only implemented for UCI

         Logging::LogIt(Logging::logGUI) << "readyok";
      }
      else if (uciCommand == "debug") {
         std::string str;
         iss >> str;
         if ( str == "on" ) DynamicConfig::minOutputLevel = Logging::logInfo;
         else DynamicConfig::minOutputLevel = Logging::logGUI;
      }
      else if (uciCommand == "stop") {
         Logging::LogIt(Logging::logInfo) << "stop requested";
         COM::stop();
      }
      else if (uciCommand == "position") {
         auto startTimePos = Clock::now();
         COM::position.h   = nullHash; // invalidate position
         std::string type;
         while (iss >> type) {
            if (type == "startpos") { readFEN(startPosition, COM::position, false, true); }
            else if (type == "fen") {
               std::string fen;
               for (int i = 0; i < 6; i++) { // suppose always full fen ... ///@todo better?
                  std::string component;
                  iss >> component;
                  fen += component + " ";
               }
               if (!readFEN(fen, COM::position, false, true)) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
            }
            else if (type == "moves") {
               if (COM::position.h != nullHash) {
                  std::string mstr;
                  while (iss >> mstr) {
                     Move m = COM::moveFromCOM(mstr);
                     Logging::LogIt(Logging::logInfo) << "UCI applying move " << ToString(m);
                     if (!COM::makeMove(m, false, "")) { // make move
                        Logging::LogIt(Logging::logInfo) << "Bad move ! : " << mstr;
                        Logging::LogIt(Logging::logInfo) << ToString(COM::position);
                     }
                  }
               }
               else
                  Logging::LogIt(Logging::logGUI) << "info string no start position specified";
            }
         }
         // we take into account the time needed to parse input and get current position
         TimeMan::overHead = (int)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - startTimePos).count();
      }
      else if (uciCommand == "go") {
         if (!ThreadPool::instance().main().stopFlag) {
            Logging::LogIt(Logging::logGUI) << "info string go command received, but search already in progress";
         }
         else {
            if (COM::position.h != nullHash) {
               //MoveList root_moves;

               TimeMan::isDynamic       = false;
               TimeMan::nbMoveInTC      = -1;
               TimeMan::msecPerMove     = -1;
               TimeMan::msecInTC        = -1;
               TimeMan::msecInc         = -1;
               TimeMan::msecUntilNextTC = -1;
               TimeMan::moveToGo        = -1;
               COM::depth               = MAX_DEPTH; // infinity

               DynamicConfig::mateFinder = false;
               COM::State state          = COM::st_searching; //uci is/should be stateless but we use this anyhow

               std::string param;
               bool        noParam = true;
               while (iss >> param) {
                  Logging::LogIt(Logging::logInfo) << "received parameter " << param;
                  if (param == "infinite") {
                     noParam              = false;
                     TimeMan::msecPerMove = INFINITETIME;
                     state                = COM::st_analyzing;
                  }
                  else if (param == "depth") {
                     noParam = false;
                     int d   = 0;
                     iss >> d;
                     COM::depth           = d;
                     TimeMan::msecPerMove = INFINITETIME;
                  }
                  else if (param == "movetime") {
                     noParam = false;
                     iss >> TimeMan::msecPerMove;
                  }
                  else if (param == "nodes") {
                     noParam = false;
                     iss >> TimeMan::maxNodes;
                  }
                  else if (param == "searchmoves") {
                     Logging::LogIt(Logging::logGUI) << "info string " << param << " not implemented yet";
                  }
                  else if (param == "wtime") {
                     noParam = false;
                     int t;
                     iss >> t;
                     if (COM::position.c == Co_White) {
                        TimeMan::msecUntilNextTC = t;
                        TimeMan::isDynamic       = true;
                     }
                  }
                  else if (param == "btime") {
                     noParam = false;
                     int t;
                     iss >> t;
                     if (COM::position.c == Co_Black) {
                        TimeMan::msecUntilNextTC = t;
                        TimeMan::isDynamic       = true;
                     }
                  }
                  else if (param == "winc") {
                     int t;
                     iss >> t;
                     if (COM::position.c == Co_White) {
                        TimeMan::msecInc   = t;
                        TimeMan::isDynamic = true;
                     }
                  }
                  else if (param == "binc") {
                     int t;
                     iss >> t;
                     if (COM::position.c == Co_Black) {
                        TimeMan::msecInc   = t;
                        TimeMan::isDynamic = true;
                     }
                  }
                  else if (param == "movestogo") {
                     noParam = false;
                     int t;
                     iss >> t;
                     TimeMan::moveToGo  = t;
                     TimeMan::isDynamic = true;
                  }
                  else if (param == "ponder") {
                     state = COM::st_pondering;
                  }
                  else if (param == "mate") {
                     int d = 0;
                     iss >> d;
                     COM::depth                = d;
                     DynamicConfig::mateFinder = true;
                     TimeMan::msecPerMove      = INFINITETIME;
                  }
                  else {
                     Logging::LogIt(Logging::logGUI) << "info string " << param << " not implemented";
                  }
               }
               if (noParam) {
                  Logging::LogIt(Logging::logWarn) << "no parameters given for go command, going for a depth 10 search ...";
                  COM::depth           = 10;
                  TimeMan::msecPerMove = INFINITETIME;
               }
               Logging::LogIt(Logging::logInfo) << "uci search launched";
               COM::thinkAsync(state);
               Logging::LogIt(Logging::logInfo) << "uci async started";
            }
            else {
               Logging::LogIt(Logging::logGUI) << "info string search command received, but no position specified";
            }
         }
      }
      else if (uciCommand == "ponderhit") {
         Logging::LogIt(Logging::logInfo) << "received command ponderhit";
         COM::state = COM::st_searching; // will allow move reporting to GUI
         ThreadPool::instance().main().getData().isPondering = false; // will unlock busy loop
      }
      else if (uciCommand == "ucinewgame") {
         if (ThreadPool::instance().main().searching()) {
            Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " received but search in progress ...";
         }
         else {
            COM::init(COM::p_uci);
         }
      }
      else if (uciCommand == "eval") {
         Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " not implemented yet";
      }
      else if (uciCommand == "tbprobe") {
         std::string type;
         iss >> type;
         Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " not implemented yet";
      }
      else if (uciCommand == "print") {
         Logging::LogIt(Logging::logInfo) << ToString(COM::position);
      }
      else if (uciCommand == "d") {
         Logging::LogIt(Logging::logInfo) << GetFEN(COM::position);
      }
      else if (uciCommand == "quit") {
         COM::stop();
         iterate = false;
      }
      else if (uciCommand == "wait") {
         using namespace std::chrono_literals;
         while(!ThreadPool::instance().main().stopFlag){
            std::this_thread::sleep_for(20ms);
         }
         std::this_thread::sleep_for(200ms);
      }      
      else if (uciCommand == "bench") {
         int bd = 10;
         iss >> bd;
         bench(bd);
      }
      else if (!uciCommand.empty()) {
         Logging::LogIt(Logging::logGUI) << "info string unrecognised command " << uciCommand;
      }
   }
   Logging::LogIt(Logging::logInfo) << "Leaving UCI loop";
}

std::string wdlStat(ScoreType score, unsigned int ply) {
   std::stringstream ss;
   const int wdlW = (int)toWDLModel(score, ply);
   const int wdlL = (int)toWDLModel(-score, ply);
   const int wdlD = 1000 - wdlW - wdlL;
   ss << " wdl " << wdlW << " " << wdlD << " " << wdlL;
   return ss.str();
}

std::string uciScore(ScoreType score, unsigned int ply) {
   if (isMatedScore(score)) return "mate " + std::to_string((-MATE - score) / 2);
   if (isMateScore(score)) return "mate " + std::to_string((MATE - score + 1) / 2);
   return "cp " + std::to_string(score) + (DynamicConfig::withWDL ? wdlStat(score, ply) : "");
}

} // namespace UCI
