#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3550 }, 
{"Dragon", 3528 }, 
{"Berserk", 3491 }, 
{"Chess", 3488 }, 
{"Ethereal", 3487 }, 
{"Igel", 3487 }, 
{"RubiChess", 3474 }, 
{"Koivisto", 3470 }, 
{"Revenge", 3470 }, 
{"Sjeng", 3461 }, 
{"SlowChess", 3460 }, 
{"Clover", 3458 }, 
{"Rebel", 3457 }, 
{"Seer", 3450 }, 
{"RofChade", 3443 }, 
{"Caissa", 3442 }, 
{"Minic", 3441 }, 
{"Viridithas", 3440 }, 
{"Uralochka", 3431 }, 
{"Arasan", 3419 }, 
{"Fire", 3419 }, 
{"Ginkgo", 3403 }, 
{"Wasp", 3402 }, 
{"Velvet", 3399 }, 
{"Booot", 3396 }, 
{"Marlin", 3389 }, 
{"Halogen", 3382 }, 
{"Houdini", 3382 }, 
{"Nemorino", 3380 }, 
{"Marvin", 3374 }, 
{"Smallbrain", 3374 }, 
{"Lc0", 3370 }, 
{"BlackCore", 3365 }, 
{"Superultra", 3361 }, 
{"Tucano", 3356 }, 
{"Rice", 3333 }, 
{"Fritz", 3328 }, 
{"Xiphos", 3318 }, 
{"Alexandria", 3317 }, 
{"Devre", 3315 }, 
{"Mantissa", 3314 }, 
{"Weiss", 3314 }, 
{"Zahak", 3311 }, 
{"Expositor", 3310 }, 
{"Combusken", 3305 }, 
{"Counter", 3305 }, 
{"Stash", 3296 }, 
{"HIARCS", 3289 }, 
{"Frozenight", 3275 }, 
{"Laser", 3274 }, 
{"Winter", 3272 }, 
{"Defenchess", 3268 }, 
{"Shredder", 3267 }, 
{"Pawn", 3264 }, 
{"Fizbo", 3259 }, 
{"Drofa", 3257 }, 
{"Andscacs", 3251 }, 
{"StockDory", 3250 }
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
               for (const auto & it : ratings) {
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