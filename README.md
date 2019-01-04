# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I try to keep it under 2500sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.

## Strength
Quite poor for a chess engine (around 2400) but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   0 Minic                          84      15    1673   61.9%   20.8%
   1 Bitfoot                        25      50     152   53.6%   20.4%
   2 myrddin                        21      50     152   53.0%   19.1%
   3 sungorus64                    -18      50     152   47.4%   18.4%
   4 Horizon_4_4                   -55      52     152   42.1%   13.2%
   5 zevra                         -91      49     152   37.2%   25.7%
   6 Minic 0.28                    -94      43     152   36.8%   40.8%
   7 dorpsgek                     -101      49     152   35.9%   25.7%
   8 ct800_v1.11_x64              -148      55     152   29.9%   16.4%
   9 weini1                       -162      58     152   28.3%   10.5%
  10 asymptote                    -163      54     153   28.1%   19.6%
  11 rattatechess_nosferatu       -170      55     152   27.3%   19.1%
```

## What about SMP ?  
```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 Minic 3 threads                60      34     200   58.5%   49.0%
   2 Minic 2 threads               -16      33     200   47.8%   52.5%
   3 Minic                         -44      32     200   43.8%   54.5%
```

## How to compile
* Linux : use the given build script (or make your own ...)
* Windows : use the given VS2017 project (or make your own ...)

## How to run
add the command line option "-xboard" to go to xboard/winboard mode.

Other available options are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -mateFinder <"fen"> depth : same as analysis but without prunings in search.

## Options

Minic now comes with both a json configuration file where some parameters can be set
```
{
   "threads": 1,
   "mateFinder": false,
   "ttSizeMb": 512,
   "ttESizeMb": 512,
   "book": true
}
```
and a command line interface, using the same key word. For instance, forcing mateFinder mode can be done by adding "-mateFinder 1" when calling Minic.

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
* 0.24 : passer eval
* 0.25 : more aggressive pruning
* 0.26 : fix a TC bug
* 0.27 : smaller window, better singular extension
* 0.28 : xboard time command is used now
* 0.29 : refactoring, very bad TT bug fix, better timeman, better gamephase, work on SEE ... huuuuuge progress. Also preparing for a better eval (pawn structure)...
 
