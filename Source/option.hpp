#pragma once

#include "definition.hpp"

#include "logging.hpp"

/* This is a convenient oldy from Weini to let the user specify some configuration from CLI
 * CLI interaction is kept for debug purpose
 */
namespace Options {

    extern std::vector<std::string> args;

    ///@todo use std::variant ? and std::optinal ?? c++17
    enum KeyType : unsigned char { k_bad = 0, k_bool, k_depth, k_int, k_score, k_ull, k_string};
    enum WidgetType : unsigned char { w_check = 0, w_string, w_spin, w_combo, w_button, w_max};
    const std::string widgetXboardNames[w_max] = {"check","string","spin","combo","button"};

    struct KeyBase {
      template < typename T > KeyBase(KeyType t, WidgetType w, const std::string & k, T * v, const std::function<void(void)> & cb = []{} ) :type(t), wtype(w), key(k), value((void*)v) {callBack = cb;}
      template < typename T > KeyBase(KeyType t, WidgetType w, const std::string & k, T * v, const T & _vmin, const T & _vmax, const std::function<void(void)> & cb = []{} ) :type(t), wtype(w), key(k), value((void*)v), vmin(_vmin), vmax(_vmax) {callBack = cb;}
      KeyBase(KeyType t, WidgetType w, const std::string & k, std::string * v, const std::function<void(void)> & cb = []{} ) :type(t), wtype(w), key(k), value((void*)v){callBack = cb;}
      KeyType     type;
      WidgetType  wtype;
      std::string key;
      void*       value;
      int         vmin = 0, vmax = 0; // assume int type is covering all the case (string excluded ...)
      bool        hotPlug = false;
      std::function<void(void)> callBack;
    };

    bool SetValue(const std::string & key, const std::string & value);

    void displayOptionsXBoard();

    void displayOptionsUCI();

    // from argv
    template<typename T> inline bool getOption(T & value, const std::string & key) {
        auto it = std::find(args.begin(), args.end(), std::string("-") + key);
        if (it == args.end()) { Logging::LogIt(Logging::logWarn) << "ARG key not given, " << key; return false; }
        std::stringstream str;
        ++it;
        if (it == args.end()) { Logging::LogIt(Logging::logError) << "ARG value not given, " << key; return false; }
        str << *it;
        str >> value;
        Logging::LogIt(Logging::logInfo) << "From ARG, " << key << " : " << value;
        return true;
    }

    void initOptions(int argc, char ** argv);
} // Options
