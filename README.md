# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I'll try to keep it under 2500sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.

## Strength
Quite poor for a chess engine (around 2400) but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 fruit_21                      241      21    1185   80.0%   16.7%
   2 drosophila-win64               90      18    1185   62.7%   18.1%
   3 Minic 0.34                     37      16    1185   55.4%   36.5%
   4 Minic 0.33                      5      16    1185   50.7%   35.6%
   5 Minic 0.32                      4      16    1185   50.5%   35.3%
   6 Minic 0.31                      2      16    1185   50.3%   36.3%
   7 myrddin                         1      18    1185   50.1%   17.4%
   8 Minic 0.30                    -11      16    1185   48.4%   36.2%
   9 Minic 0.29                    -35      16    1185   44.9%   30.8%
  10 MadChess.Engine               -45      18    1185   43.5%   20.3%
  11 sungorus64                    -56      18    1185   42.0%   18.8%
  12 Bitfoot                       -64      18    1185   40.9%   16.9%
  13 Horizon_4_4                   -97      19    1185   36.4%   13.5%
  14 Minic 0.28                   -113      18    1185   34.3%   26.5%
```

Actual WAC score is 284 (10sec per position on an old i7-2600K).

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
* 0.31 : not much
* 0.32 : fix bug in eval
* 0.33 : more aggressive SEE, better move sorting, better time control. Preparing for Syzygy endgame table and end-game helpers KQK, KRK and KBNK.
* 0.34 : just another TC management improvement
