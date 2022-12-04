#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3534 }, 
{"Dragon", 3529 }, 
{"Revenge", 3475 }, 
{"Ethereal", 3474 }, 
{"Berserk", 3470 }, 
{"Koivisto", 3467 }, 
{"SlowChess", 3463 }, 
{"RubiChess", 3448 }, 
{"RofChade", 3446 }, 
{"Minic", 3438 }, 
{"Seer", 3432 }, 
{"Arasan", 3424 }, 
{"Uralochka", 3421 }, 
{"Rebel", 3416 }, 
{"Igel", 3409 }, 
{"Marlin", 3390 }, 
{"Houdini", 3384 }, 
{"Nemorino", 3384 }, 
{"Wasp", 3377 }, 
{"Fire", 3373 }, 
{"Lc0", 3373 }, 
{"Clover", 3368 }, 
{"Tucano", 3359 }, 
{"Velvet", 3354 }, 
{"Booot", 3352 }, 
{"Weiss", 3321 }, 
{"Xiphos", 3321 }, 
{"Zahak", 3313 }, 
{"Mantissa", 3310 }, 
{"Devre", 3309 }, 
{"Combusken", 3306 }, 
{"Marvin", 3305 }, 
{"Hiarcs", 3292 }, 
{"Stash", 3280 }, 
{"Laser", 3278 }, 
{"Defenchess", 3271 }
};

std::string strToLower(std::string s) {
   std::transform(s.begin(), s.end(), s.begin(), [](uint8_t c) { return std::tolower(c); });
   return s;
}

void init() {
   // restore default parameter
   DynamicConfig::ratingFactor = 1.; // this means "full" contempt given by user

   const int myRating = ratings["Minic"];

   // this TCEC specific option is prefered when present
   if (DynamicConfig::ratingAdvReceived) {
      Logging::LogIt(Logging::logInfo) << "Using ratingAdv...";
      // ratingFactor will go from -2 to 2
      const double x = 1. / (1 - DynamicConfig::ratingAdv / static_cast<double>(myRating));
      DynamicConfig::ratingFactor = 2 * std::tanh((x - 1) * 20.);
   }
   // else rely on the UCI_Opponent string
   else {
      Logging::LogIt(Logging::logInfo) << "No ratingAdv given (TCEC specific option)...";

      if (DynamicConfig::opponent.empty()) { Logging::LogIt(Logging::logInfo) << "No opponent string given ..."; }
      else {
         Logging::LogIt(Logging::logInfo) << "Opponent string received: \"" << DynamicConfig::opponent << "\"";
         std::vector<std::string> tokens;
         tokenize(DynamicConfig::opponent, tokens);
         //Logging::LogIt(Logging::logInfo) << "nb token " << tokens.size();
         if (tokens.size() >= 4) {
            std::string       oppName      = tokens[3];
            const std::string oppRatingStr = tokens[1];
            int               oppRating    = 0;
            bool              ratingFound  = false;

/*
            if ( oppRatingStr != "none" ){
               const int oppRatingRead = std::atoi(oppRatingStr.c_str());
               if ( oppRatingRead ){
                     oppRating = oppRatingRead;
                     Logging::LogIt(Logging::logInfo) << "Opponent rating is " << oppRating;
                     ratingFound = true;
               }
            }
*/

            if (!ratingFound) {
               //Logging::LogIt(Logging::logWarn) << "No rating given";
               unsigned int i = 4;
               while (i < tokens.size()) oppName += " " + tokens[i++];
               oppName = strToLower(oppName);
               Logging::LogIt(Logging::logInfo) << "Looking for Elo rating of " << oppName;
               for (auto const& it : ratings) {
                  if ((oppName.find(strToLower(it.first)) != std::string::npos) || (strToLower(it.first).find(oppName) != std::string::npos)) {
                     oppRating = it.second;
                     Logging::LogIt(Logging::logInfo) << "Opponent is " << it.first << ", Elo " << oppRating;
                     ratingFound = true;
                     break;
                  }
               }
               if (!ratingFound) Logging::LogIt(Logging::logWarn) << "Unknown opponent";
            }

            if (ratingFound) {
               // ratingFactor will go from -2 to 2
               DynamicConfig::ratingFactor = 2 * std::tanh((myRating / static_cast<double>(oppRating) - 1) * 20.);
            }
         }
         else {
            Logging::LogIt(Logging::logWarn) << "Invalid opponent string";
         }

         ///@todo play with style also ?
      }
   }

   Logging::LogIt(Logging::logInfo) << "Rating factor set to: " << DynamicConfig::ratingFactor;
}

void ratingReceived() { DynamicConfig::ratingAdvReceived = true; }

} // namespace Opponent