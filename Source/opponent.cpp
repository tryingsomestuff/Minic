#include "opponent.hpp"

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent {

static std::map<std::string, uint16_t> ratings = {
{"Stockfish", 3685 }, 
{"Dragon", 3664 }, 
{"SlowChess", 3528 }, 
{"Lc0", 3523 }, 
{"Berserk", 3521 }, 
{"Revenge", 3516 }, 
{"Ethereal", 3508 }, 
{"Koivisto", 3503 }, 
{"RubiChess", 3451 }, 
{"Allie", 3443 }, 
{"Houdini", 3443 }, 
{"Fire", 3438 }, 
{"Nemorino", 3434 }, 
{"Seer", 3432 }, 
{"Igel", 3423 }, 
{"Arasan", 3396 }, 
{"Stoofvlees", 3350 }, 
{"Xiphos", 3338 }, 
{"Booot", 3316 }, 
{"Weiss", 3307 }, 
{"Minic", 3305 }, 
{"RofChade", 3305 }, 
{"Defenchess", 3285 }, 
{"Wasp", 3280 }, 
{"Zahak", 3278 }, 
{"Laser", 3275 }, 
{"Schooner", 3271 }, 
{"Shredder", 3269 }, 
{"DanaSah", 3253 }, 
{"Chiron", 3249 }, 
{"Clover", 3249 }, 
{"Fizbo", 3248 }, 
{"Andscacs", 3230 }, 
{"Stash", 3226 }, 
{"Marvin", 3225 }, 
{"Roc", 3223 }, 
{"Dark", 3203 }, 
{"Halogen", 3193 }, 
{"Demolito", 3167 }, 
{"Combusken", 3165 }, 
{"Equinox", 3161 }, 
{"NirvanaChess", 3159 }, 
{"Critter", 3156 }, 
{"Winter", 3155 }, 
{"Strelka", 3142 }, 
{"PanChess", 3138 }, 
{"Velvet", 3133 }, 
{"Counter", 3129 }, 
{"Vajolet2", 3129 }, 
{"Orion", 3128 }, 
{"Texel", 3113 }, 
{"Rybka", 3111 }, 
{"iCE", 3111 }, 
{"Hannibal", 3110 }, 
{"Bob", 3110 }, 
{"Invictus", 3106 }, 
{"chess22k", 3103 }, 
{"Devre", 3096 }, 
{"BlackMamba", 3091 }, 
{"Protector", 3088 }, 
{"Senpai", 3086 }, 
{"Drofa", 3072 }, 
{"Bit-Genie", 3066 }, 
{"Pirarucu", 3058 }, 
{"Cheng", 3049 }, 
{"ChessBrainVB", 3046 }, 
{"SmarThink", 3045 }, 
{"Amoeba", 3040 }, 
{"FabChess", 3033 }, 
{"Monolith", 3033 }, 
{"Alfil", 3031 }, 
{"Naum", 3031 }, 
{"Bagatur", 3029 }, 
{"Deuterium", 3022 }, 
{"Hakkapeliitta", 3021 }, 
{"Tucano", 3019 }, 
{"Rodent", 3005 }, 
{"Topple", 2999 }, 
{"Mantissa", 2998 }, 
{"Gogobello", 2992 }, 
{"Godel", 2983 }, 
{"Hiarcs", 2982 }, 
{"PeSTO", 2978 }, 
{"Crafty", 2965 }, 
{"Bobcat", 2961 }, 
{"Francesca", 2934 }, 
{"Dirty", 2928 }, 
{"Deep", 2926 }, 
{"Spark", 2926 }, 
{"Atlas", 2924 }, 
{"Black", 2923 }, 
{"Asymptote", 2922 }, 
{"Spike", 2920 }, 
{"EXchess", 2917 }, 
{"DiscoCheck", 2915 }, 
{"tomitankChess", 2907 }, 
{"Scorpio", 2904 }, 
{"Zurichess", 2903 }, 
{"Nu-chess", 2902 }, 
{"Quazar", 2901 },
};

std::string str_tolower(std::string s) {
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
               oppName = str_tolower(oppName);
               Logging::LogIt(Logging::logInfo) << "Looking for Elo rating of " << oppName;
               for (auto const& it : ratings) {
                  if ((oppName.find(str_tolower(it.first)) != std::string::npos) || (str_tolower(it.first).find(oppName) != std::string::npos)) {
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

} // namespace Opponent