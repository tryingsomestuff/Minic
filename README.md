# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I try to keep it under 2500sloc.
Minic stands for "Minimal Chess".

## Strenght
Quite poor for a chess engine (around 2300) but already way to strong for me !

```
     Name                          Elo     +/-   Games   Score   Draws
   0 Minic                          52      16    1605   57.5%   18.1%
   1 sungorus64                    121      56     134   66.8%   18.7%
   2 Bitfoot                        52      53     134   57.5%   22.4%
   3 Horizon_4_4                    44      55     134   56.3%   15.7%
   4 dorpsgek                       29      51     134   54.1%   24.6%
   5 zevra                          -3      50     133   49.6%   28.6%
   6 asymptote                      -5      54     134   49.3%   17.9%
   7 ct800_v1.11_x64               -29      54     134   45.9%   17.2%
   8 rattatechess_nosferatu        -60      56     134   41.4%   14.2%
   9 simplex                       -93      53     134   36.9%   24.6%
  10 weini1                       -114      58     133   34.2%   14.3%
  11 fairymax                     -347      80     134   11.9%   11.9%
  12 tscp181                      -478     113     133    6.0%    7.5%

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
* 0.13 : some draw issue fix
* 0.14 : better lmp (use of "improving bool")
* 0.15 : keep track of material count directly in position
* 0.16 : use bitboard for pawn move generation also
* 0.17 : Minic goes SMP ! 
* 0.18 : configuration file (json) and threads management
* 0.19 : option from command line and fix a TT issue
