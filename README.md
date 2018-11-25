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
   0 Minic                          33      10    3890   54.7%   18.2%
   1 sungorus64                    108      35     324   65.1%   19.8%
   2 Bitfoot                        92      35     325   62.9%   16.9%
   3 Horizon_4_4                    80      36     324   61.3%   15.7%
   4 zevra                          27      31     324   53.9%   34.9%
   5 dorpsgek                       20      32     324   52.9%   27.5%
   6 asymptote                      -9      35     325   48.8%   16.9%
   7 ct800_v1.11_x64               -18      35     324   47.4%   16.4%
   8 rattatechess_nosferatu        -41      35     324   44.1%   13.6%
   9 weini1                        -57      35     324   41.8%   17.0%
  10 simplex                       -74      35     324   39.5%   17.9%
  11 fairymax                     -274      43     324   17.1%   15.7%
  12 tscp181                      -417      63     324    8.3%    6.2%

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
