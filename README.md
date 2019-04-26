![Logo](https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png)

# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP) has come up, I'll try to keep it under 3000sloc.
Minic stands for "Minimal Chess".

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.
Starting from release 0.50 new binaries are available :

* minic_0.50_linux_x64_avx2_bmi2: fully optimized Linux64 (avx2+bmi2)   
* minic_0.50_linux_x64_sse4.2: optimized Linux64 (sse4.2)  
* minic_0.50_linux_x64_x86-64: basic Linux64  
* minic_0.50_mingw_x32_avx2_bmi2.exe: fully optimized Windows32 (avx2+bmi2)  
* minic_0.50_mingw_x32_sse4.2.exe: optimized Windows32 (sse4.2)  
* minic_0.50_mingw_x32_i686.exe: basic Windows32  
* minic_0.50_mingw_x64_avx2_bmi2.exe: fully optimized Windows64 (avx2+bmi2)  
* minic_0.50_mingw_x64_sse4.2.exe: optimized Windows64 (sse4.2)  
* minic_0.50_mingw_x64_i686.exe: basic Windows64   
   
 Please note that Win32 binaries are very slow (I don't know why yet, so please use Win64 one if possible).
   
## Strength
Quite poor for a chess engine (around 2650) but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   0 Minic                          62      16    1219   58.8%   32.2%
   1 rofChade                      196      51     174   75.6%   22.4%
   2 fruit_21                       44      47     174   56.3%   18.4%
   3 Minic 0.44                    -16      36     174   47.7%   52.9%
   4 Minic 0.43                   -118      35     174   33.6%   53.4%
   5 Minic 0.40                   -125      38     174   32.8%   47.1%
   6 myrddin                      -213      55     174   22.7%   16.7%
   7 MadChess.Engine              -244      58     175   19.7%   14.3%

1219 of 7000 games finished.                   
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
   "ttSizeMb": 1024,
   "book": true,
   "bookFile": "book_big.bin",
   "fullXboardOutput": false
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
* 0.35 : work on material draw
* 0.36 : little optimisations
* 0.37 : MoveList optimisations, fuse eval and score TT, refactoring a lot, bug fixes
* 0.38 : build a bigger book (not used yet), implement the so called easy-move
* 0.39 : fix a bad mate score TT issue, speed up pawn move generation
* 0.40 : pruning in endgame also
* 0.41 : try PSO texel tuning, prepare some evaluation feature, little speed optim
* 0.42 : fix a SMP issue (lock)
* 0.43 : fix another SMP issue
* 0.44 : fix null move that was in fact NOT activated ! activate probcut. Add some counters. New release are sse4.2 and avx2 for both windows and linux.
* 0.45 : lot of clean up in code, search tuning, starts UCI implementation, and fix null move ...
* 0.46 : little optim
* 0.47 : clean up, try uci support 
* 0.48 : some evaluation features about Pawn
* 0.49 : more pawn features
* 0.50 : more rook and pawn features (new Win32 releases)

## Thanks

Of course many/most idea in Minic are taken from the beautifull chess developer community.
Here's an incomplete list of inspiring open-source engines :

Arasan by Jon Dart  
CPW by Pawel Koziol and Edmund Moshammer  
Deepov by Romain Goussault  
Dorpsgek by Dan Ravensloft  
Ethereal by Andrew Grant  
Galjoen by Werner Taelemans  
Madchess by Erik Madsen  
Rodent III by Pawel Koziol  
Stockfish by Tord Romstad, Marco Costalba, Joona Kiiski and Gary Linscott  
Topple by Vincent Konsolas  
TSCP by Tom Kerrigan  
Vajolet by Marco Belli  
Vice by BlueFeverSoft  
Xiphos by Milos Tatarevic  
