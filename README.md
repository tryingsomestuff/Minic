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
   0 Minic                          21       8    5275   53.0%   18.7%
   1 Bitfoot                       128      31     440   67.6%   19.8%
   2 sungorus64                    103      30     439   64.5%   21.9%
   3 dorpsgek                       71      27     440   60.1%   32.0%
   4 Horizon_4_4                    64      30     440   59.1%   15.0%
   5 zevra                          62      29     439   58.9%   23.9%
   6 ct800_v1.11_x64                15      30     440   52.2%   17.5%
   7 rattatechess_nosferatu         -9      29     440   48.8%   18.4%
   8 asymptote                     -27      29     440   46.1%   19.5%
   9 weini1                        -54      30     439   42.3%   16.2%
  10 simplex                       -84      30     439   38.2%   17.1%
  11 fairymax                     -281      38     440   16.6%   15.0%
  12 tscp181                      -397      50     439    9.2%    7.5%

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

