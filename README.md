# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I try to keep it under 2500sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.

## Strenght
Quite poor for a chess engine (around 2300) but already way to strong for me !

```
     Name                          Elo     +/-   Games   Score   Draws
   0 Minic                          71      12    2593   60.1%   18.4%
   1 sungorus64                    103      44     216   64.4%   17.6%
   2 Bitfoot                       103      43     216   64.4%   20.4%
   3 dorpsgek                       29      40     216   54.2%   25.9%
   4 Horizon_4_4                    14      42     216   52.1%   18.1%
   5 zevra                         -18      39     216   47.5%   30.1%
   6 asymptote                     -66      42     217   40.6%   20.3%
   7 ct800_v1.11_x64               -75      44     216   39.4%   15.7%
   8 weini1                       -103      45     216   35.6%   14.8%
   9 rattatechess_nosferatu       -120      44     216   33.3%   18.5%
  10 simplex                      -182      48     216   25.9%   13.9%
  11 fairymax                     -338      55     216   12.5%   17.6%
  12 tscp181                      -396      74     216    9.3%    7.4%


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
* 0.20 : clean up code, especially draw management  
* 0.21 : add a little more evaluation (basic material bonus/malus)
* 0.22 : not much ...
* 0.23 : try to texel tune pieces value, not sure it is a success 

