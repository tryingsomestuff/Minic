namespace UCI {

    void init() {
        Logging::ct = Logging::CT_uci;
        Logging::LogIt(Logging::logInfo) << "Init uci";
        COM::init();
    }

    void uci() {

        while (true) {
            COM::readLine();
            std::istringstream iss(COM::command);
            std::string uciCommand;
            iss >> uciCommand;
            Logging::LogIt(Logging::logInfo) << "uci received command " << uciCommand;

            if (uciCommand == "uci") {
                Logging::LogIt(Logging::logGUI) << "id name Minic " << MinicVersion;
                Logging::LogIt(Logging::logGUI) << "id author Vivien Clauzon";

                ///@todo options !
                //Logging::LogIt(Logging::logGUI) << "option name Hash type spin default 128 min 1 max 1048576";
                //Logging::LogIt(Logging::logGUI) << "option name Threads type spin default 1 min 1 max 128";
                //Logging::LogIt(Logging::logGUI) << "option name SyzygyPath type string default <empty>";
                //Logging::LogIt(Logging::logGUI) << "option name SyzygyResolve type spin default 512 min 1 max 1024";
                //Logging::LogIt(Logging::logGUI) << "option name Ponder type check default false";

                Logging::LogIt(Logging::logGUI) << "uciok";
            }
            else if (uciCommand == "setoption") {
                std::string name;
                iss >> name;
                if (name == "name") iss >> name;
                /*
                if (name == "Hash") {
                    std::string value;
                    iss >> value; // Skip value
                    iss >> hash_size;
                    // Resize hash
                    delete tt;
                    tt = new tt::hash_t(hash_size * MB);
                }
                else if (name == "Threads") {
                    std::string value;
                    iss >> value; // Skip value
                    iss >> threads;
                }
                else if (name == "SyzygyPath") {
                    std::string value;
                    iss >> value; // Skip value

                    std::string path;
                    std::getline(iss, path);

                    path = path.substr(1, path.size() - 1);

                    std::cout << "Looking for tablebases in: " << path << std::endl;

                    init_tablebases(path.c_str());
                }
                else if (name == "SyzygyResolve") {
                    std::string value;
                    iss >> value; // Skip value
                    iss >> syzygy_resolve;
                }
                else if (name == "Ponder") {
                    // Do nothing
                }
                */
                else { Logging::LogIt(Logging::logGUI) << "info string unknown uci option " << name; }
            }
            else if (uciCommand == "isready") { Logging::LogIt(Logging::logGUI) << "readyok"; }
            else if (uciCommand == "stop") { Logging::LogIt(Logging::logInfo) << "stop requested";  ThreadContext::stopFlag = true; }
            else if (uciCommand == "position") {
                COM::position.h = 0ull; // invalidate position
                std::string type;
                while (iss >> type) {
                    if (type == "startpos") { COM::sideToMoveFromFEN(startPosition); }
                    else if (type == "fen") {
                        std::string fen;
                        for (int i = 0; i < 6; i++) { // always full fen ... ///@todo better?
                            std::string component;
                            iss >> component;
                            fen += component + " ";
                        }
                        if (!COM::sideToMoveFromFEN(fen)) Logging::LogIt(Logging::logFatal) << "Illegal FEN " << fen;
                    }
                    else if (type == "moves") {
                        if (COM::position.h != 0ull) {
                            std::string mstr;
                            while (iss >> mstr) {
                                Move m = COM::moveFromCOM(mstr);
                                if (!COM::makeMove(m, false, "")) { // make move
                                    Logging::LogIt(Logging::logInfo) << "Bad move ! : " << mstr;
                                    Logging::LogIt(Logging::logInfo) << ToString(COM::position);
                                    COM::mode = COM::m_force;
                                }
                                else COM::stm = COM::opponent(COM::stm);
                            }
                        }
                        else Logging::LogIt(Logging::logGUI) << "info string no start position specified";
                    }
                }
            }
            else if (uciCommand == "go") {
                if (!ThreadContext::stopFlag) { Logging::LogIt(Logging::logGUI) << "info string go command received, but search already in progress"; }
                else {
                    if (COM::position.h != 0ull) {
                        //MoveList root_moves;

                        TimeMan::isDynamic = false;
                        TimeMan::nbMoveInTC = -1;
                        TimeMan::msecPerMove = -1;
                        TimeMan::msecInTC = -1;
                        TimeMan::msecInc = -1;
                        TimeMan::msecUntilNextTC = -1;
                        TimeMan::moveToGo = -1;
                        COM::depth = MAX_DEPTH; // infinity

                        COM::ponder = COM::p_off;
                        DynamicConfig::mateFinder = false;

                        std::string param;
                        while (iss >> param) {
                            Logging::LogIt(Logging::logInfo) << "received parameter " << param;
                            if      (param == "infinite")    { TimeMan::msecPerMove = 60 * 60 * 1000 * 24; }
                            else if (param == "depth")       { int d = 0;  iss >> d; COM::depth = d; }
                            else if (param == "movetime")    { iss >> TimeMan::msecPerMove; }
                            else if (param == "nodes")       { unsigned long long int maxNodes = 0;  iss >> maxNodes; TimeMan::maxKNodes = int(maxNodes/1000); }
                            else if (param == "searchmoves") { Logging::LogIt(Logging::logGUI) << "info string " << param << " not implemented yet"; }
                            else if (param == "wtime")       { int t; iss >> t; if (COM::position.c == Co_White) { TimeMan::msecUntilNextTC = t; TimeMan::isDynamic = true; }}
                            else if (param == "btime")       { int t; iss >> t; if (COM::position.c == Co_Black) { TimeMan::msecUntilNextTC = t; TimeMan::isDynamic = true; }}
                            else if (param == "winc" )       { int t; iss >> t; if (COM::position.c == Co_White) { TimeMan::msecInc = t; }}
                            else if (param == "binc" )       { int t; iss >> t; if (COM::position.c == Co_Black) { TimeMan::msecInc = t; }}
                            else if (param == "movestogo")   { iss >> TimeMan::moveToGo; TimeMan::isDynamic = true; }
                            else if (param == "ponder")      { COM::ponder = COM::p_on; TimeMan::msecPerMove = 60 * 60 * 1000 * 24;} // infinite search time
                            else if (param == "mate")        { int d = 0;  iss >> d; COM::depth = d; DynamicConfig::mateFinder = true; TimeMan::msecPerMove = 60 * 60 * 1000 * 24; }
                            else                             { Logging::LogIt(Logging::logGUI) << "info string " << param << " not implemented"; }
                        }
                        Logging::LogIt(Logging::logInfo) << "uci search launched";
                        COM::thinkAsync(COM::st_searching);
                        Logging::LogIt(Logging::logInfo) << "uci async started";
                    }
                    else { Logging::LogIt(Logging::logGUI) << "info string search command received, but no position specified"; }
                }
            }
            else if (uciCommand == "ponderhit") { Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " not implemented yet"; }
            else if (uciCommand == "ucinewgame") {
                if (!ThreadContext::stopFlag) { Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " received but search in progress ..."; }
                else { COM::init(); }
            }
            else if (uciCommand == "eval") { Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " not implemented yet"; }
            else if (uciCommand == "tbprobe") {
                std::string type;
                iss >> type;
                Logging::LogIt(Logging::logGUI) << "info string " << uciCommand << " not implemented yet";
            }
            else if (uciCommand == "print") { Logging::LogIt(Logging::logInfo) << ToString(COM::position); }
            else if (uciCommand == "quit") {
                COM::stopPonder();
                COM::stop();
                _exit(0);
            }
            else if (!uciCommand.empty()) { Logging::LogIt(Logging::logGUI) << "info string unrecognised command " << uciCommand; }
        }
    }

} // UCI

