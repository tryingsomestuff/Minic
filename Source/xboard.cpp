#include "xboard.hpp"

#include "com.hpp"
#include "logging.hpp"
#include "option.hpp"
#include "position.hpp"
#include "searcher.hpp"
#include "timeMan.hpp"
#include "tools.hpp"

namespace XBoard {

bool display;

enum Ponder : uint8_t { p_off = 0, p_on = 1 };
Ponder ponder; // easy / hard

enum Mode : uint8_t { m_play_white = 0, m_play_black = 1, m_force = 2, m_analyze = 3 };
Mode mode;

enum SideToMove : uint8_t { stm_white = 0, stm_black = 1 };
SideToMove stm; ///@todo isn't this redundant with position.c ??

Position initialPos;

SideToMove opponent(SideToMove& s) { return s == stm_white ? stm_black : stm_white; }

bool sideToMoveFromFEN(const std::string& fen) {
   const bool b = readFEN(fen, COM::position, false, true);
   if (!b) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
   stm = COM::position.c == Co_White ? stm_white : stm_black;
   return b;
}

void newGame() {
   mode = m_force;
   stm  = stm_white;
}

void init() {
   Logging::ct = Logging::CT_xboard;
   Logging::LogIt(Logging::logInfo) << "Init xboard";
   display = false;
   ponder  = p_off;
   COM::init(COM::p_xboard);
   newGame();
}

void setFeature() {
   ///@todo more feature disable !!
   ///@todo use otim ?
   std::string version = MinicVersion;
   if (Distributed::moreThanOneProcess()) version += "_" + std::to_string(Distributed::worldSize) + "proc";
   Logging::LogIt(Logging::logGUI) << "feature ping=1 setboard=1 edit=0 Colors=0 usermove=1 memory=0 sigint=0 sigterm=0 otim=0 time=1 nps=0 draw=0 "
                                      "playother=0 variants=\"normal,fischerandom\" myname=\"Minic "
                                   << version << "\"";
   Options::displayOptionsXBoard();
   Logging::LogIt(Logging::logGUI) << "feature done=1";
}

bool receiveOppMove(const std::string& command) {
   std::string mstr(command);
   COM::stop();
   const size_t p = command.find("usermove");
   if (p != std::string::npos) mstr = mstr.substr(p + 8);
   Move m = COM::moveFromCOM(mstr);
   if (m == INVALIDMOVE) return false;
   Logging::LogIt(Logging::logInfo) << "XBOARD applying move " << ToString(m);
   if (!COM::makeMove(m, false, "")) { // make move
      Logging::LogIt(Logging::logInfo) << "Bad opponent move ! " << mstr << ToString(COM::position);
      mode = m_force;
      return false;
   }
   else {
      stm = opponent(stm);
      COM::moves.push_back(m);
   }
   return true;
}

void moveApplied(const bool success) {
   if (success) {
      // switch stm
      stm = opponent(stm);
   }
   else {
      mode = m_force;
   }
}

bool replay(size_t nbmoves) {
   assert(nbmoves < COM::moves.size());
   COM::State previousState = COM::state;
   COM::stop();
   mode = m_force;
   std::vector<Move> vm = COM::moves;
   if (!sideToMoveFromFEN(GetFEN(initialPos))) return false;
   initialPos = COM::position;
   COM::moves.clear();
   for (size_t k = 0; k < nbmoves; ++k) {
      if (!COM::makeMove(vm[k], false, "")) { // make move
         Logging::LogIt(Logging::logInfo) << "Bad move ! " << ToString(vm[k]);
         Logging::LogIt(Logging::logInfo) << ToString(COM::position);
         mode = m_force;
         return false;
      }
      else {
         stm = opponent(stm);
         COM::moves.push_back(vm[k]);
      }
   }
   if (previousState == COM::st_analyzing) { mode = m_analyze; }
   return true;
}

void xboard() {
   Logging::LogIt(Logging::logInfo) << "Starting XBoard main loop";

   bool iterate = true;
   while (iterate) {
      Logging::LogIt(Logging::logInfo) << "XBoard: mode  (before command)" << static_cast<int>(mode);
      Logging::LogIt(Logging::logInfo) << "XBoard: stm   (before command)" << static_cast<int>(stm);
      Logging::LogIt(Logging::logInfo) << "XBoard: state (before command)" << static_cast<int>(COM::state);
      bool commandOK = true;
      int  once = 0;
      while (once++ == 0 || !commandOK) { // loop until a good command is found
         commandOK = true;
         COM::readLine(); // read next command !
         if (COM::command == "force") mode = m_force;
         else if (COM::command == "xboard")
            Logging::LogIt(Logging::logInfo) << "This is minic!";
         else if (COM::command == "post")
            display = true;
         else if (COM::command == "nopost")
            display = false;
         else if (COM::command == "computer") {
         }
         else if (strncmp(COM::command.c_str(), "protover", 8) == 0) {
            setFeature();
         }
         else if (strncmp(COM::command.c_str(), "accepted", 8) == 0) {
         }
         else if (strncmp(COM::command.c_str(), "rejected", 8) == 0) {
         }
         else if (strncmp(COM::command.c_str(), "ping", 4) == 0) {
            std::string  str(COM::command);
            const size_t p = COM::command.find("ping");
            str = str.substr(p + 4);
            str = trim(str);
            Logging::LogIt(Logging::logGUI) << "pong " << str;
         }
         else if (COM::command == "new") { // not following protocol, should set infinite depth search
            COM::stop();
            COM::init(COM::p_xboard);
            newGame();
            if (!sideToMoveFromFEN(startPosition)) { commandOK = false; }
            initialPos = COM::position;
            DynamicConfig::FRC = false;
            COM::moves.clear();
            mode = static_cast<Mode>(static_cast<int>(stm)); ///@todo this is so wrong !
            if (mode != m_analyze) {
               mode = m_play_black;
               stm  = stm_white;
            }
         }
         else if (strncmp(COM::command.c_str(), "variant", 7) == 0) {
            const size_t v = COM::command.find("variant");
            const std::string var = trim(COM::command.substr(v + 7));
            if (var == "fischerandom") DynamicConfig::FRC = true;
            else {
               Logging::LogIt(Logging::logWarn) << "Unsupported variant : " << var;
            }
         }
         else if (COM::command == "white") { // deprecated
            COM::stop();
            mode = m_play_black;
            stm  = stm_white;
         }
         else if (COM::command == "black") { // deprecated
            COM::stop();
            mode = m_play_white;
            stm  = stm_black;
         }
         else if (COM::command == "go") {
            COM::stop();
            mode = static_cast<Mode>(static_cast<int>(stm));
         }
         else if (COM::command == "playother") {
            COM::stop();
            mode = static_cast<Mode>(static_cast<int>(opponent(stm)));
         }
         else if (strncmp(COM::command.c_str(), "usermove", 8) == 0) {
            if (!receiveOppMove(COM::command)) commandOK = false;
         }
         else if (strncmp(COM::command.c_str(), "setboard", 8) == 0) {
            COM::stop();
            std::string  fen(COM::command);
            const size_t p = COM::command.find("setboard");
            fen = fen.substr(p + 8);
            if (!sideToMoveFromFEN(fen)) { commandOK = false; }
            initialPos = COM::position;
            COM::moves.clear();
         }
         else if (strncmp(COM::command.c_str(), "result", 6) == 0) {
            COM::stop();
            mode = m_force;
         }
         else if (COM::command == "analyze") {
            COM::stop();
            mode = m_analyze;
         }
         else if (COM::command == "exit") {
            COM::stop();
            mode = m_force;
         }
         else if (COM::command == "easy") {
            COM::stopPonder();
            ponder = p_off;
         }
         else if (COM::command == "hard") {
            ponder = p_on;
         }
         else if (COM::command == "quit") {
            iterate = false;
         }
         else if (COM::command == "wait") {
            using namespace std::chrono_literals;
            while(!ThreadPool::instance().main().stopFlag){
               std::this_thread::sleep_for(20ms);
            }
            std::this_thread::sleep_for(200ms);
         }            
         else if (COM::command == "pause") {
            COM::stopPonder();
            COM::readLine();
            while (COM::command != "resume") {
               Logging::LogIt(Logging::logInfo) << "Error (paused): " << COM::command;
               COM::readLine();
            }
         }
         else if (strncmp(COM::command.c_str(), "time", 4) == 0) {
            COM::stopPonder();
            int centisec = 0;
            sscanf(COM::command.c_str(), "time %d", &centisec);
            // just updating remaining time in curren TC (shall be classic TC or sudden death)
            TimeMan::isDynamic       = true;
            TimeMan::msecUntilNextTC = centisec * 10;
            commandOK = false; // waiting for usermove to be sent !!!! ///@todo this is not pretty !
         }
         else if (strncmp(COM::command.c_str(), "st", 2) == 0) { // not following protocol, will update search time
            int msecPerMove = 0;
            sscanf(COM::command.c_str(), "st %d", &msecPerMove);
            // forced move time
            TimeMan::isDynamic       = false;
            TimeMan::nbMoveInTC      = -1;
            TimeMan::msecPerMove     = msecPerMove * 1000;
            TimeMan::msecInTC        = -1;
            TimeMan::msecInc         = -1;
            TimeMan::msecUntilNextTC = -1;
            TimeMan::moveToGo        = -1;
            COM::depth               = MAX_DEPTH; // infinity
         }
         else if (strncmp(COM::command.c_str(), "sd", 2) == 0) { // not following protocol, will update search depth
            int d = 0;
            sscanf(COM::command.c_str(), "sd %d", &d);
            COM::depth = clampDepth(d);
            if (COM::depth < 0) COM::depth = 8;
            // forced move depth
            TimeMan::isDynamic       = false;
            TimeMan::nbMoveInTC      = -1;
            TimeMan::msecPerMove     = INFINITETIME;
            TimeMan::msecInTC        = -1;
            TimeMan::msecInc         = -1;
            TimeMan::msecUntilNextTC = -1;
            TimeMan::moveToGo        = -1;
         }
         else if (strncmp(COM::command.c_str(), "level", 5) == 0) {
            int timeTC = 0, secTC = 0, inc = 0, mps = 0;
            // WARNING here, level command only supports integer ! timeTC is in minutes, and secTC and inc in secondes
            if (sscanf(COM::command.c_str(), "level %d %d %d", &mps, &timeTC, &inc) != 3)
               sscanf(COM::command.c_str(), "level %d %d:%d %d", &mps, &timeTC, &secTC, &inc);
            // classic TC is mps>0, else sudden death
            TimeMan::isDynamic       = false;
            TimeMan::nbMoveInTC      = mps;
            TimeMan::msecPerMove     = -1;
            TimeMan::msecInTC        = timeTC * 60000 + secTC * 1000;
            TimeMan::msecInc         = inc * 1000;
            TimeMan::msecUntilNextTC = timeTC; // just an init here, will be managed using "time" command later
            TimeMan::moveToGo        = -1;
            COM::depth               = MAX_DEPTH; // infinity
         }
         else if (COM::command == "edit") {
         }
         else if (COM::command == "?") {
            if (COM::state == COM::st_searching) COM::stop();
         }
         else if (COM::command == "draw") {
         }
         else if (COM::command == "undo") {
            replay(COM::moves.size() - 1);
         }
         else if (COM::command == "remove") {
            replay(COM::moves.size() - 2);
         }
         else if (COM::command == "hint") {
         }
         else if (COM::command == "bk") {
         }
         else if (COM::command == "random") {
         }
         else if (strncmp(COM::command.c_str(), "otim", 4) == 0) {
            commandOK = false;
         }
         else if (COM::command == ".") {
         }
         else if (strncmp(COM::command.c_str(), "option", 6) == 0) {
            std::stringstream str(COM::command);
            std::string tmpstr;
            str >> tmpstr; // option
            str >> tmpstr; // key[=value] in fact
            std::vector<std::string> kv;
            tokenize(tmpstr, kv, "=");
            kv.resize(2); // hacky ...
            if (!Options::SetValue(kv[0], kv[1])) Logging::LogIt(Logging::logError) << "Unable to set value " << kv[0] << " = " << kv[1];
         }
         //************ end of Xboard command ********//
         // let's try to read the unknown command as a move ... trying to fix a scid versus PC issue ...
         ///@todo try to be safer here
         else if (!receiveOppMove(COM::command))
            Logging::LogIt(Logging::logInfo) << "Xboard does not know this command \"" << COM::command << "\"";
      } // readline

      Logging::LogIt(Logging::logInfo) << "XBoard: mode  (after command)" << static_cast<int>(mode);
      Logging::LogIt(Logging::logInfo) << "XBoard: stm   (after command)" << static_cast<int>(stm);
      Logging::LogIt(Logging::logInfo) << "XBoard: state (after command)" << static_cast<int>(COM::state);

      // move as computer if mode is equal to stm
      if (static_cast<int>(mode) == static_cast<int>(stm) && COM::state == COM::st_none) {
         Logging::LogIt(Logging::logInfo) << "xboard search launched";
         COM::thinkAsync(COM::st_searching);
         Logging::LogIt(Logging::logInfo) << "xboard async started";
      }

      /*
      ///@todo Ponder won't work here because after this, as searchDriver() is async, we are probably stuck in readLine() ...
      // if not our turn, and ponder is on, let's think ...
      if(static_cast<int>(COM::mode) == static_cast<int>(COM::opponent(COM::stm)) && COM::ponder == COM::p_on && COM::state == COM::st_none) {
          Logging::LogIt(Logging::logInfo) << "xboard search launched (pondering)";
          COM::thinkAsync(COM::st_pondering);
          Logging::LogIt(Logging::logInfo) << "xboard async started (pondering)";
      }
      */

      if (mode == m_analyze && COM::state == COM::st_none) {
         Logging::LogIt(Logging::logInfo) << "xboard search launched (analysis)";
         COM::thinkAsync(COM::st_analyzing);
         Logging::LogIt(Logging::logInfo) << "xboard async started (analysis)";
      }

      Logging::LogIt(Logging::logInfo) << "XBoard: mode  (after search launch)" << static_cast<int>(mode);
      Logging::LogIt(Logging::logInfo) << "XBoard: stm   (after search launch)" << static_cast<int>(stm);
      Logging::LogIt(Logging::logInfo) << "XBoard: state (after search launch)" << static_cast<int>(COM::state);

   } // while true
   Logging::LogIt(Logging::logInfo) << "Leaving Xboard loop";
}
} // namespace XBoard
