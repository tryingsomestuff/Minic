#pragma once

#include "definition.hpp"
#include "dynamicConfig.hpp"
#include "tools.hpp"

namespace Opponent{

static std::map<std::string, unsigned short int> ratings = {
{"Fat Fritz 2", 3643},
{"Stockfish", 3637},
{"Dragon", 3587},
{"Lc0", 3525},
{"Allie", 3445},
{"Houdini", 3443},
{"Nemorino", 3439},
{"RubiChess", 3428},
{"SlowChess", 3410},
{"Igel", 3406},
{"Ethereal", 3404},
{"Stoofvlees", 3350},
{"Pedone", 3345},
{"Xiphos", 3341},
{"Fire", 3334},
{"Booot", 3320},
{"RofChade", 3308},
{"Defenchess", 3286},
{"Schooner", 3273},
{"Laser", 3272},
{"Shredder", 3270},
{"Fizbo", 3248},
{"Andscacs", 3230},
{"Minic", 3223},
{"Wasp", 3219},
{"Roc", 3218},
{"DanaSah", 3205},
{"Halogen", 3189},
{"Arasan", 3183},
{"Combusken", 3169},
{"Demolito", 3168},
{"Chiron", 3164},
{"NirvanaChess", 3164},
{"Equinox", 3161},
{"Critter", 3156},
{"Winter", 3155},
{"Marvin", 3145},
{"Strelka", 3142},
{"Orion", 3138},
{"PanChess", 3138},
{"Toga", 3135},
{"Vajolet2", 3128},
{"Weiss", 3119},
{"Koivisto", 3118},
{"Gull", 3117},
{"Texel", 3114},
{"Hannibal", 3110},
{"Rybka", 3110},
{"Stash", 3108},
{"iCE", 3108},
{"chess22k", 3097},
{"BlackMamba", 3091},
{"Protector", 3088},
{"Seer", 3086},
{"Senpai", 3085},
{"Pirarucu", 3060},
{"ChessBrainVB", 3050},
{"SmarThink", 3045},
{"FabChess", 3033},
{"Monolith", 3032},
{"Alfil", 3031},
{"Naum", 3031},
{"Bagatur", 3024},
{"Tucano", 3022},
{"Hakkapeliitta", 3021},
{"Deuterium", 3020},
{"Amoeba", 3019},
{"Rodent", 3005},
{"Cheng", 3000},
{"Gogobello", 2991},
{"Topple", 2987},
{"Hiarcs", 2985},
{"Counter", 2982},
{"Godel", 2982},
{"PeSTO", 2979},
{"Mr Bob", 2971},
{"Crafty", 2965},
{"Bobcat", 2961},
{"BBC", 2957},
{"Francesca", 2941},
{"Dirty", 2928},
{"Deep", 2926},
{"Spark", 2926},
{"Asymptote", 2924},
{"Atlas", 2923},
{"Invictus", 2923},
{"Spike", 2920},
{"EXchess", 2916},
{"DiscoCheck", 2915},
{"tomitankChess", 2906},
{"Scorpio", 2904},
{"Berserk", 2903},
{"Quazar", 2901},
{"Zurichess", 2901},
{"Deep", 2898},
{"Drofa", 2894},
{"Daydreamer", 2890},
{"Thinker", 2885},
{"Murka", 2882},
{"The Baron", 2882},
{"Gaviota", 2871}   
};

std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

void init(){
    ///@todo restore default
    DynamicConfig::ratingFactor = 1.; // this means "full" contempt given by user

    if ( DynamicConfig::opponent.empty()){
       Logging::LogIt(Logging::logInfo) << "No opponent string given ...";
    }
    else{
       Logging::LogIt(Logging::logInfo) << "Opponent string received: \"" << DynamicConfig::opponent << "\"";
       std::vector<std::string> tokens;
       tokenize(DynamicConfig::opponent,tokens);
       //Logging::LogIt(Logging::logInfo) << "nb token " << tokens.size();
       if ( tokens.size() >= 4 ){
          const int myRating = ratings["Minic"];

          std::string oppName = tokens[3];
          const std::string oppRatingStr = tokens[1];
          int oppRating = 0;
          bool ratingFound = false;

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
         
          if ( !ratingFound ){
            if ( !ratingFound ) Logging::LogIt(Logging::logWarn) << "No rating given";
            unsigned int i = 4;
            while(i < tokens.size()) oppName += " " + tokens[i++];
            oppName = str_tolower(oppName);
            Logging::LogIt(Logging::logInfo) << "Looking for Elo rating of " << oppName;
            for (auto const& it : ratings){
               if ( (oppName.find(str_tolower(it.first)) != std::string::npos) || (str_tolower(it.first).find(oppName) != std::string::npos) ){
                  oppRating = it.second;
                  Logging::LogIt(Logging::logInfo) << "Opponent is " << it.first << ", Elo " << oppRating;
                  ratingFound = true;
                  break;
               }
            }
            if ( !ratingFound ) Logging::LogIt(Logging::logWarn) << "Unknown opponent";
          }

          if ( ratingFound ){
             // ratingFactor will go from -2 to 2
             DynamicConfig::ratingFactor = 2*std::tanh(((myRating/double(oppRating))-1)*20.);
          }
       }
       else{
          Logging::LogIt(Logging::logWarn) << "Invalid opponent string";
       }

       Logging::LogIt(Logging::logInfo) << "Rating factor set to: " << DynamicConfig::ratingFactor;

       ///@todo play with style ?
    }
}

}
