#pragma once

#include "definition.hpp"
#include "logging.hpp"

/*!
 * This is a convenient oldy from Weini to let the user specify some configuration from CLI
 * CLI interaction is kept for debug purpose
 */
namespace Options {

extern std::vector<std::string> args;

[[nodiscard]] bool SetValue(const std::string& key, const std::string& value);

void displayOptionsXBoard();

void displayOptionsUCI();

// from argv
template<typename T> inline bool getOption(T& value, const std::string& key) {
   auto it = std::ranges::find(args, std::string("-") + key);
   if (it == args.end()) { /*Logging::LogIt(Logging::logWarn) << "ARG key not given, " << key;*/
      return false;
   }
   std::stringstream str;
   ++it;
   if (it == args.end()) {
      Logging::LogIt(Logging::logError) << "ARG value not given, " << key;
      return false;
   }
   str << *it;
   str >> value;
   Logging::LogIt(Logging::logInfo) << "From ARG, " << key << " : " << value;
   return true;
}

void initOptions(int argc, char** argv);

} // namespace Options
