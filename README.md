# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Code size shall not go above 2000sloc.
Minic stands for "Minimal Chess".
Minic is smaller than TSCP but stronger than Micro-Max.

## Strenght
Quite poor for a chess engine but already way to strong for me !

```
     Name                          Elo     +/-   Games   Score   Draws
   0 Minic                         -24      12    2670   46.6%   18.7%
   1 sungorus64                    162      43     243   71.8%   17.7%
   2 Bitfoot                       159      43     243   71.4%   18.5%
   3 dorpsgek                      105      38     243   64.6%   27.2%
   4 zevra                          90      39     242   62.6%   26.0%
   5 Horizon_4_4                    77      41     243   60.9%   15.6%
   6 rattatechess_nosferatu         68      40     243   59.7%   17.3%
   7 ct800_v1.11_x64                62      41     243   58.8%   16.5%
   8 asymptote                      46      39     243   56.6%   22.6%
   9 weini1                        -19      39     242   47.3%   20.2%
  10 fairymax                     -239      48     243   20.2%   14.8%
  11 tscp181                      -324      59     242   13.4%    9.5%
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

