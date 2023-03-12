#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3533 }, 
{"Lc0", 3530 }, 
{"Dragon", 3528 }, 
{"Ethereal", 3494 }, 
{"Berserk", 3490 }, 
{"Koivisto", 3474 }, 
{"Revenge", 3472 }, 
{"RubiChess", 3470 }, 
{"Igel", 3469 }, 
{"SlowChess", 3463 }, 
{"Rebel", 3457 }, 
{"Seer", 3451 }, 
{"RofChade", 3447 }, 
{"Minic", 3445 }, 
{"Uralochka", 3435 }, 
{"Arasan", 3420 }, 
{"Wasp", 3412 }, 
{"Ginkgo", 3405 }, 
{"Black", 3391 }, 
{"Caissa", 3384 }, 
{"Clover", 3384 }, 
{"Houdini", 3383 }, 
{"Nemorino", 3383 }, 
{"Halogen", 3379 }, 
{"Marvin", 3377 }, 
{"Fire", 3372 }, 
{"Velvet", 3369 }, 
{"Tucano", 3359 }, 
{"Booot", 3358 }, 
{"Fritz", 3331 }, 
{"Xiphos", 3320 }, 
{"Weiss", 3318 }, 
{"Mantissa", 3317 }, 
{"Devre", 3316 }, 
{"Expositor", 3315 }, 
{"Zahak", 3315 }, 
{"Smallbrain", 3311 }, 
{"BlackCore", 3307 }, 
{"Counter", 3307 }, 
{"Combusken", 3305 }, 
{"Stash", 3299 }, 
{"Hiarcs", 3292 }, 
{"Laser", 3277 }, 
{"Frozenight", 3273 }, 
{"Defenchess", 3270 }, 
{"Deep", 3269 }, 
{"Fizbo", 3262 }, 
{"Alexandria", 3261 }, 
{"Andscacs", 3253 }, 
{"Winter", 3253 }, 
{"Schooner", 3250 }, 
{"Chiron", 3249 }, 
{"Viridithas", 3246 }, 
{"Drofa", 3243 }, 
{"Little", 3240 }, 
{"NirvanaChess", 3215 }, 
{"Orion", 3210 }, 
{"Vajolet2", 3191 }, 
{"StockNemo", 3187 }, 
{"DanaSah", 3179 }, 
{"RukChess", 3177 }, 
{"ChessBrainVB", 3175 }, 
{"Gull", 3173 }, 
{"Equinox", 3166 } 
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