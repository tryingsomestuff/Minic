# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Code size shall not go above 2000sloc.
Minic stands for "Minimal Chess".
Minic is smaller than TSCP but stronger than Micro-Max.

## Strenght
Quite poor for a chess engine but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 minic                         204      39     167   76.3%   43.7%
   2 minic0.9                      -40      38     167   44.3%   47.9%
   3 fairymax                     -152      44     168   29.5%   35.1%
```

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

## History

* 0.1 : first commit with the initial code done in less than 2 days
* 0.2-0.5 : many bug fix
* 0.6 : IID
* 0.7 : try TT move first hoping for cut-off
* 0.8 : SEE and LMP
* 0.9 : little internal book
* 0.10 : singular extension for TT move  
* 0.11 : on the road to bitboard (only attack), use of "Dumb" HQBB code.  
* 0.12 : better use of bitboard, in generation and eval also  

