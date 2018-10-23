# Minic
A simple chess engine to learn and play with.  
Code size shall not go above 2000sloc  
Minic stands for "Minimal Chess"  

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

