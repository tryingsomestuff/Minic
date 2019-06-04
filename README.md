![Logo](https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png)

# Minic
A simple chess engine to learn and play with.
Compatible with xboard/winboard.
Initially, the code size was supposed not go above 2000sloc.
Now that more features (especially SMP and bitboard) has come up, I'll try to keep it under 3000sloc.
This is why Minic stands for "Minimal Chess" (and is not related to the GM Dragoljub Minić).

## Code style
Minic is (mainly) a one-file-code with very dense lines. This is of course very wrong in terms of software design... So why is it so? First reason is that Minic was first develop as a week-end project, the quick-and-dirty way, and since then I'm having fun going on this way ; being a "small" code size engine is part of the stakes in developing it. Second reason is that it helps the compilers optimize the whole code. The only dependency is the very good json header-only library nlohmann/json used to parse the config file. Some optional features such as Texel tuning, perft, tests, uci support, book generation ... are available in the Add-Ons directory.

## Release

Have a look in the Dist directory for releases. Some more stable/official ones will also be made available as github release.
Starting from release 0.50 new binaries are available :

```
* minic_0.50_linux_x64_avx2_bmi2    : fully optimized Linux64 (avx2+bmi2)   
* minic_0.50_linux_x64_sse4.2       : optimized Linux64 (sse4.2)  
* minic_0.50_linux_x64_x86-64       : basic Linux64  
* minic_0.50_mingw_x32_avx2_bmi2.exe: fully optimized Windows32 (avx2+bmi2)  
* minic_0.50_mingw_x32_sse4.2.exe   : optimized Windows32 (sse4.2)  
* minic_0.50_mingw_x32_i686.exe     : basic Windows32  
* minic_0.50_mingw_x64_avx2_bmi2.exe: fully optimized Windows64 (avx2+bmi2)  
* minic_0.50_mingw_x64_sse4.2.exe   : optimized Windows64 (sse4.2)  
* minic_0.50_mingw_x64_x86-64.exe   : basic Windows64   
```   

Please note that Win32 binaries are very slow (I don't know why yet, so please use Win64 one if possible).
   
## Strength
Minic 0.47 is 2650 on the CCRL 40/40 scale, so way to strong for me ! This is more or less like GM level for human being.

```
Rank Name                                Elo     +/-   Games   Score   Draws
   1 zurichess-neuchatel-linux-amd64     264      16    2177   82.1%   16.4%
   2 MinkoChess_1.3_x64                  239      16    2178   79.9%   16.7%
   3 ruy-1.1.9-linux                      93      13    2178   63.1%   21.1%
   4 redqueen-1.1.98.linux64              -2      13    2177   49.7%   22.0%
   5 minic_dev_linux_x64                 -10      13    2177   48.6%   24.5%
   6 wyldchess1.51_bmi                   -21      13    2177   47.0%   23.4%
   7 minic_0.53_linux_x64_avx2_bmi2      -32      13    2177   45.5%   23.9%
   8 minic_0.58_linux_x64_avx2_bmi2      -38      13    2177   44.5%   25.2%
   9 GreKo-Linux-64                      -49      13    2178   42.9%   18.1%
  10 Fridolin310                         -81      13    2178   38.6%   21.2%
  11 igel-x64_popcnt                    -112      14    2178   34.5%   18.9%
  12 asymptote-v0.4.2                   -202      15    2178   23.8%   14.7%

13065 of 66000 games finished.              
```

## How to compile
* Linux : use the given build script (or make your own ...)
* Windows : use the given VS2017 project (or make your own ...), or use the Linux cross-compilation script given.

## How to run
add the command line option "-xboard" to go to xboard/winboard mode.

Other available options are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -mateFinder <"fen"> depth : same as analysis but without prunings in search
* ...

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

## Opening books

Minic comes with some opening books written in an internal own binary format. There are currently 4 books
```
book_small : a very small book only with main variation of classic opening lines
book_big   : a bigger book (take 5 secondes to load) of nearly 400.000 lines
Carlsen    : based on Carlsen most used opening lines (thanks to Jonathan Cremers)
Anand      : based on Anand most used opening lines (thanks to Jonathan Cremers)
```

You can use both the json configuation file or the command line argument to select the book.

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
* 0.51 : use bishop pair bonus  
* 0.52 : fix history, check extension, some endgames (prepare for pawn hash table ...)  
* 0.53 : fix pawn eval  
* 0.54 : working on extension and eval  
* 0.55 : working on extension and eval  
* 0.56 : TC issue in multi-thread mode  
* 0.57 : fix 0.56  
* 0.58 and 0.59 : trying so add evaluation feature (without success ...)  
* 0.60 : use hash in qsearch  
* 0.61 : KPK and code clean up  
* 0.62 : fix an issue with material well castling, a lot of code refactoring

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

Many thanks also to all testers for all those long time control tests, they really are valuable inputs in the chess engine development process. 

Also thanks to TCEC for letting Minic participate to the Season 15 division 4a, it was fun to see Minic on such a great hardware.

And of course thanks to all the members of the talkchess forum and CPW, and to H.G. Muller for hosting the friendly monthly tourney.

## Info
- Am I a chess player ?
- yes
- good one ?
- no (https://lichess.org/@/xr_a_y)
