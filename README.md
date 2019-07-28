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

WARNING : Dist directory as been REMOVED from the repository because it was starting to be too big. Unofficial releases are not available anymore. This operation has changed Minic git history, so you shall probably re-clone a clean repo.

Some stable/official ones will also be made available as github release. I "officially release" (create a github version) as sson as I have some validated elo (at least +10).

In a github release, a tester shall only use the given (attached) binaries. The full "source" package always contains everything (source code, test suites, opening suite, books, ...). If you want to use Minic parameter file (minic.json) or some given book file, there are not attached in all release, there are downloadable from the github repo.

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

Minic finished 13/18 at TCEC16 qualification league (https://cd.tcecbeta.club/archive.html?season=16&div=ql&game=1)

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 zurichess-neuchatel           235      16    1884   79.4%   20.9%
   2 MinkoChess_1.3                200      16    1884   75.9%   19.7%
   3 ruy-1.1.9                      83      14    1884   61.8%   23.6%
   4 minic_0.73                     67      13    1884   59.5%   33.5%
   5 minic_0.70                     58      13    1886   58.2%   34.8%
   6 minic_0.72                     51      13    1885   57.3%   32.8%
   7 minic_0.71                     38      13    1885   55.4%   33.6%
   8 minic_0.69                     20      13    1885   52.9%   32.6%
   9 minic_0.65                     -9      13    1885   48.6%   33.0%
  10 minic_0.61                    -16      13    1884   47.7%   31.4%
  11 redqueen-1.1.98               -31      14    1884   45.6%   21.9%
  12 wyldchess1.51                 -32      14    1884   45.5%   25.6%
  13 minic_0.53                    -44      13    1883   43.7%   31.9%
  14 minic_0.57                    -51      13    1885   42.7%   29.9%
  15 minic_0.47                    -71      14    1885   39.9%   25.6%
  16 igel                          -92      14    1883   37.0%   21.3%
  17 Fridolin310                  -117      15    1884   33.8%   19.5%
  18 GreKo                        -117      15    1885   33.8%   19.7%
  19 asymptote-v0.4.2             -138      15    1885   31.1%   17.0%

17902 of 171000 games finished.        
```

Minic strength can be ajdusted using the level option (from command line or json configuration file). Level 0 is a random mover, 1 very weak, ..., level 10 is full strength. Level functionnaly will be enhanced in a near future.

Minic random mover stats are the following :

```
   7.73%  0-1 {Black mates}
   7.50%  1-0 {White mates}
   2.45%  1/2-1/2 {Draw by 3-fold repetition}
  21.99%  1/2-1/2 {Draw by fifty moves rule}
  54.16%  1/2-1/2 {Draw by insufficient mating material}
   6.13%  1/2-1/2 {Draw by stalemate}
```

Current level elo are more or less those :

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 minic_devlvl10                  631     156     175   97.4%    4.0%
   2 minic_devlvl9                   407      88     177   91.2%    5.1%
   3 minic_devlvl8                   231      61     177   79.1%    6.8%
   4 minic_devlvl7                   167      56     177   72.3%    4.5%
   5 minic_devlvl6                    65      51     178   59.3%    3.9%
   6 minic_devlvl5                     2      50     178   50.3%    5.1%
   7 minic_devlvl4                   -67      51     179   40.5%    2.8%
   8 minic_devlvl3                  -165      55     179   27.9%    5.6%
   9 minic_devlvl2                  -244      61     180   19.7%    7.2%
  10 minic_devlvl1                  -308      69     179   14.5%    6.7%
  11 minic_devlvl0                  -inf     nan     179    0.0%    0.0%
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
   "bookFile": "book.bin",
   "fullXboardOutput": true,
   "debugMode": false,
   "debugFile": "minic.debug",
   "level": 10,
   "syzygyPath": "/data/minic/TB/syzygy"
}

```
and a command line interface, using the same key word. For instance, forcing mateFinder mode can be done by adding "-mateFinder 1" when calling Minic.

## Syzygy EGT

To compile with SYZYGY support you'll need to clone https://github.com/jdart1/Fathom as Fathom directory and activate WITH_SYZYGY definition at compile time.
To use EGT just specify syzygyPath in the configuration file or in the command line.

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
* 0.62 : fix an issue with material when castling, a lot of code refactoring
* 0.63 : others fix
* 0.64 : others fix again, lot of refactoring
* 0.65 : working on search
* 0.66 : working on search again
* 0.67 : working on search again (quiet move SEE)
* 0.68 : working on search again (LMR when in check)
* 0.69..0.74 : tuning eval (texel tuning on Zurichess quiet.edp)
* 0.75 : backward pawn, tweak SEE  
* 0.76 : refactoring eval scoring  
* 0.77 : better use of bitboard in pawn eval  
* 0.78 : candidate  
* 0.79 : bug fix in SEE, pondering and analysis mode
* 0.80 : level from 0 to 10. 0 is random mover, 10 is full strength  
* 0.81 : working on random mover and pondering  
* 0.82 : finally a working pondering release ...

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
Texel by Peter Österlund   
Topple by Vincent Konsolas  
TSCP by Tom Kerrigan  
Vajolet by Marco Belli  
Vice by BlueFeverSoft  
Xiphos by Milos Tatarevic  
Zurichess by Alexandru Moșoi   

Many thanks also to all testers for all those long time control tests, they really are valuable inputs in the chess engine development process. 

Also thanks to TCEC for letting Minic participate to the Season 15 division 4a and Season 16 qualification league, it was fun to see Minic on such a great hardware.

And of course thanks to all the members of the talkchess forum and CPW, and to H.G. Muller for hosting the friendly monthly tourney.

## Info
- Am I a chess player ?
- yes
- good one ?
- no (https://lichess.org/@/xr_a_y)
