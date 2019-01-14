# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I try to keep it under 2500sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.

## Strength
Quite poor for a chess engine (around 2350) but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 fruit_21                      289      19    1782   84.1%   12.6%
   2 drosophila-win64              118      15    1782   66.4%   17.0%
   3 Minic                          26      14    1782   53.7%   21.3%
   4 myrddin                        -2      15    1782   49.7%   16.7%
   5 Minic 0.29                     -9      14    1782   48.7%   23.9%
   6 MadChess.Engine               -38      15    1782   44.6%   18.6%
   7 sungorus64                    -63      15    1782   41.0%   19.1%
   8 Bitfoot                       -72      15    1782   39.8%   16.6%
   9 Minic 0.28                    -98      15    1782   36.3%   20.9%
  10 Horizon_4_4                  -101      15    1782   35.8%   15.8%
```

Actual WAC score is 274 (10sec per position on an old i7-2600K).

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
* 0.30 : add a simple mobility term inside evaluation
 
