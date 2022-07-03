#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3693 }, 
{"Dragon", 3673 }, 
{"Berserk", 3596 }, 
{"Koivisto", 3557 }, 
{"Ethereal", 3526 }, 
{"SlowChess", 3522 }, 
{"Lc0", 3519 }, 
{"Revenge", 3509 }, 
{"RubiChess", 3509 }, 
{"RofChade", 3506 }, 
{"Seer", 3486 }, 
{"Minic", 3460 }, 
{"Fire", 3451 }, 
{"Allie", 3441 }, 
{"Houdini", 3441 }, 
{"Arasan", 3436 }, 
{"Igel", 3430 }, 
{"Nemorino", 3427 }, 
{"Uralochka", 3398 }, 
{"Booot", 3385 }, 
{"Clover", 3368 }, 
{"Rebel", 3367 }, 
{"Tucano", 3363 }, 
{"Wasp", 3359 }, 
{"Stoofvlees", 3348 }, 
{"Xiphos", 3334 }, 
{"Toga", 3312 }, 
{"Marvin", 3311 }, 
{"Zahak", 3308 }, 
{"Velvet", 3307 }, 
{"Weiss", 3304 }, 
{"Hiarcs", 3285 }, 
{"Defenchess", 3281 }, 
{"Laser", 3273 }, 
{"Schooner", 3270 }, 
{"Marlin", 3267 }, 
{"Shredder", 3267 }, 
{"Stash", 3258 }, 
{"DanaSah", 3252 }, 
{"Chiron", 3249 }, 
{"Fizbo", 3246 }, 
{"Goliath", 3246 }, 
{"Combusken", 3232 }, 
{"Andscacs", 3228 }, 
{"Roc", 3218 }, 
{"Expositor", 3194 }, 
{"Halogen", 3193 }, 
{"Rodent", 3183 }, 
{"Drofa", 3165 }, 
{"Demolito", 3164 }, 
{"Equinox", 3161 }, 
{"NirvanaChess", 3160 }, 
{"Critter", 3156 }, 
{"Winter", 3154 }, 
{"Counter", 3147 }, 
{"Strelka", 3143 }, 
{"PanChess", 3138 }, 
{"Orion", 3134 }, 
{"Vajolet2", 3128 }, 
{"Mantissa", 3117 }, 
{"Texel", 3113 }, 
{"Bob", 3111 }, 
{"Rybka", 3111 }, 
{"iCE", 3111 }, 
{"Hannibal", 3110 }, 
{"Invictus", 3105 }, 
{"Devre", 3104 }, 
{"chess22k", 3100 }, 
{"BlackMamba", 3094 }, 
{"Protector", 3088 }, 
{"Senpai", 3086 }, 
{"Bit-Genie", 3076 }, 
{"Pirarucu", 3055 }, 
{"Cheng", 3050 }, 
{"ChessBrainVB", 3047 }, 
{"SmarThink", 3045 }, 
{"Monolith", 3037 }, 
{"FabChess", 3033 }, 
{"Alfil", 3031 }, 
{"Amoeba", 3031 }, 
{"Naum", 3031 }, 
{"Bagatur", 3030 }, 
{"Deuterium", 3024 }, 
{"Hakkapeliitta", 3022 }, 
{"Topple", 3002 }, 
{"Gogobello", 2994 }, 
{"Godel", 2981 }, 
{"PeSTO", 2978 }, 
{"Crafty", 2965 }, 
{"Bobcat", 2961 }, 
{"Francesca", 2948 }, 
{"Dirty", 2930 }, 
{"Spark", 2926 }, 
{"Deep", 2925 }, 
{"Smallbrain", 2925 }, 
{"Asymptote", 2924 }, 
{"Atlas", 2923 }, 
{"Spike", 2921 }, 
{"DiscoCheck", 2916 }, 
{"EXchess", 2916 }, 
{"Nu-chess", 2908 }, 
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