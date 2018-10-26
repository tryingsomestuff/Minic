# Minic
A simple chess engine to learn and play with.  
Code size shall not go above 2000sloc  
Minic stands for "Minimal Chess"  

# Strenght :
Quite poor for a chess engine but already way to strong for me !

Rank Name                          Elo     +/-   Games   Score   Draws
   1 fairymax                      104      18    1178   64.5%   21.8%
   2 Minic                         -41      16    1178   44.2%   33.0%
   3 tscp181                       -61      18    1178   41.3%   23.3%


## How to compile:  
* Linux : use the given build script (or make your own ...)  
* Windows : use the given VS2017 project (or make your own ...)  

## How to run : 
add the command line option "-xboard" to go to xboard/winboard mode.  

Other available options are :  
* -perft_test : run the inner perft test  
* -eval <"fen"> : static evaluation of the given position  
* -gen <"fen"> : move generation on the given position  
* -perft <"fen"> depth : perft on the given position and depth  
* -analyze <"fen"> depth : analysis on the given position and depth  
* -mateFinder <"fen"> depth : same as analysis but without prunings in search.  

