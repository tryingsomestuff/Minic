#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3691 }, 
{"Dragon", 3673 }, 
{"Koivisto", 3558 }, 
{"Berserk", 3540 }, 
{"Ethereal", 3527 }, 
{"SlowChess", 3522 }, 
{"Lc0", 3519 }, 
{"Revenge", 3512 }, 
{"RubiChess", 3510 }, 
{"RofChade", 3507 }, 
{"Seer", 3490 }, 
{"Fire", 3451 }, 
{"Allie", 3441 }, 
{"Houdini", 3441 }, 
{"Arasan", 3437 }, 
{"Nemorino", 3427 }, 
{"Igel", 3419 }, 
{"Minic", 3394 }, 
{"Booot", 3385 }, 
{"Rebel", 3369 }, 
{"Clover", 3368 }, 
{"Tucano", 3361 }, 
{"Wasp", 3360 }, 
{"Stoofvlees", 3348 }, 
{"Xiphos", 3334 }, 
{"Toga", 3312 }, 
{"Velvet", 3309 }, 
{"Zahak", 3309 }, 
{"Weiss", 3305 }, 
{"Hiarcs", 3292 }, 
{"Defenchess", 3281 }, 
{"Laser", 3273 }, 
{"Black", 3270 }, 
{"Schooner", 3270 }, 
{"Shredder", 3267 }, 
{"Stash", 3257 }, 
{"DanaSah", 3253 }, 
{"Chiron", 3248 }, 
{"Goliath", 3248 }, 
{"Fizbo", 3246 }, 
{"Combusken", 3230 }, 
{"Andscacs", 3228 }, 
{"Roc", 3218 }, 
{"Marvin", 3211 }, 
{"Halogen", 3193 }, 
{"Rodent", 3183 }, 
{"Drofa", 3165 }, 
{"Demolito", 3164 }, 
{"Equinox", 3161 }, 
{"NirvanaChess", 3160 }, 
{"Critter", 3155 }, 
{"Winter", 3153 }, 
{"Strelka", 3143 }, 
{"PanChess", 3138 }, 
{"Counter", 3134 }, 
{"Orion", 3133 }, 
{"Vajolet2", 3128 }, 
{"Expositor", 3118 }, 
{"Mantissa", 3117 }, 
{"Texel", 3113 }, 
{"Rybka", 3111 }, 
{"Hannibal", 3110 }, 
{"iCE", 3110 }, 
{"Invictus", 3106 }, 
{"Bob", 3106 }, 
{"Devre", 3103 }, 
{"chess22k", 3100 }, 
{"BlackMamba", 3093 }, 
{"Protector", 3087 }, 
{"Senpai", 3085 }, 
{"Bit-Genie", 3075 }, 
{"Pirarucu", 3055 }, 
{"Cheng", 3049 }, 
{"ChessBrainVB", 3046 }, 
{"SmarThink", 3045 }, 
{"Monolith", 3036 }, 
{"Amoeba", 3035 }, 
{"FabChess", 3032 }, 
{"Alfil", 3031 }, 
{"Naum", 3031 }, 
{"Bagatur", 3029 }, 
{"Deuterium", 3023 }, 
{"Hakkapeliitta", 3022 }, 
{"Topple", 3005 }, 
{"Gogobello", 2994 }, 
{"Godel", 2981 }, 
{"PeSTO", 2979 }, 
{"Crafty", 2965 }, 
{"Bobcat", 2960 }, 
{"Francesca", 2951 }, 
{"Dirty", 2929 }, 
{"Spark", 2926 }, 
{"Asymptote", 2925 }, 
{"Deep", 2925 }, 
{"Atlas", 2924 }, 
{"Spike", 2920 }, 
{"EXchess", 2916 }, 
{"DiscoCheck", 2915 }, 
{"Nu-chess", 2907 }, 
{"tomitankChess", 2907 }, 
{"Nalwald", 2905 }, 
{"Scorpio", 2904 }, 
{"Zurichess", 2904 }, 
{"Quazar", 2902 }
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
      const double x = 1. / (1 - DynamicConfig::ratingAdv / double(myRating));
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
               DynamicConfig::ratingFactor = 2 * std::tanh((myRating / double(oppRating) - 1) * 20.);
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