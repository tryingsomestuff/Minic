![Logo](https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png)

# Minic
A chess engine I made to learn chess programming.
Compatible with both xboard/winboard and UCI.

## Code style
For a year and a half Minic was (mainly) a one-file-code with very dense lines. This is of course very wrong in terms of software design... So why is it so? First reason is that Minic was first developped as a week-end project (in mid-october 2018), the quick-and-dirty way, and since then I was having fun going on this way ; being a "small" code size engine was part of the stakes in developing it. Second reason is that it helps the compilers optimize the whole code (at least that is what I though...)

Until version 2 of Minic, some optional features such as Texel tuning, perft, tests, uci support, book generation ... were available in the Add-Ons directory ; they are now fused with the source code.

Indeed, since version "2" Minic is now written in a more classic c++ style, although very dense lines are still present and recall Minic past compacity...

## More history
Initially, the code size was supposed not to go above 2000sloc. It started as a week-end project in october 2018 (http://talkchess.com/forum3/viewtopic.php?f=2&t=68701). But, as soon as more features (especially SMP, material tables and bitboard) came up, I tried to keep it under 4000sloc and then 5000sloc. This is why this engine was named Minic, this stands for "Minimal Chess" (and is not related to the GM Dragoljub Minić) but it has not much to do with minimalism anymore...  

Version "1" was release as a one year anniversary release in october 2019. At this point Minic has already go from a 1800 Elo 2-days-of-work engine, to a 2800 Elo engine being invited at TCEC qualification league.

Version "2" is release for April 1st 2020 (during covid-19 confinement). For this version, the one file Minic was splitted into many header and source files, and commented a lot more without negative impact on speed and strength. 

## Release process
WARNING : Dist directory as been REMOVED from the repository because it was starting to be too big. Unofficial releases are not available anymore. This operation has changed Minic git history, so you shall probably re-clone a clean repo. All (unofficial) releases are available in a new repo, here : https://github.com/tryingsomestuff/Minic-Dist

Some stable/official ones will also be made available as github release. I "officially release" (create a github version) as soon as I have some validated elo (at least +10).

In a github release, a tester shall only use the given (attached) binaries. The full "source" package always contains everything (source code, test suites, opening suite, books, ...). 

Starting from release 2.27 new binaries are available :

```
* minic_2.27_linux_x64_skylake     : fully optimized Linux64 (avx2+bmi2)   
* minic_2.27_linux_x64_nehalem     : optimized Linux64 (sse4.2)  
* minic_2.27_linux_x64_x86-64      : basic Linux64  
* minic_2.27_mingw_x64_skylake.exe : fully optimized Windows64 (avx2+bmi2)  
* minic_2.27_mingw_x64_nehalem.exe : optimized Windows64 (sse4.2)  
* minic_2.27_mingw_x64_x86-64.exe  : basic Windows64   
* minic_2.27_mingw_x32_skylake.exe : fully optimized Windows32 (avx2+bmi2)  
* minic_2.27_mingw_x32_nehalem.exe : optimized Windows32 (sse4.2)  
* minic_2.27_mingw_x32_i686.exe    : basic Windows32 
* minic_2.27_android               : android armv7
```   
Please note that Win32 binaries are very slow (I don't know why yet, so please use Win64 one if possible).
   
Starting from release 1.00 Minic support setting options using protocol (both XBoard and UCI). Option priority are as follow : command line option can be override by protocol option.
   
## Strength

### CCRL
Minic 2.00 is tested at 2986 on the CCRL 40/15 scale (http://ccrl.chessdom.com/ccrl/4040/)  
Minic 2.00 is tested at 3138 on the CCRL BLITZ 4CPU scale (http://ccrl.chessdom.com/ccrl/404/).  
Minic 2.33 is tested at 3092 on the CCRL BLITZ 1CPU scale (http://ccrl.chessdom.com/ccrl/404/).  
Minic 2.21 is tested at 3000 on the CCRL FRC list (http://ccrl.chessdom.com/ccrl/404FRC/)  

### CEGT
Minic 2.07 is tested at 2906 on the CEGT 40/4 list (http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html)  
Minic 1.35 is tested at 3034 on the CEGT 40/4 MP8CPU list (http://www.cegt.net/40_4_Ratinglist/40_4_mp/rangliste.html)  
Minic 2.07 is tested at 2934 on the CEGT 5+3 PB=ON list (http://www.cegt.net/5Plus3Rating/BestVersionsNEW/rangliste.html)  
Minic 2.11 is tested at 2891 on the CEGT 40/15 list (http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html)  

### FGRL
Minic 2.00 is tested at 2794 on the fastgm 60sec+0.6sec rating list (http://www.fastgm.de/60-0.60.html)  
Minic 2.00 is tested at 2924 on the fastgm 10min+6sec rating list (http://www.fastgm.de/10min.html)  
Minic 2.00 is tested at 2951 on the fastgm 60min+15sec rating list (http://www.fastgm.de/60min.html)  

### Test Suite
STS : 1191/1500 @10sec per position (single thread on an i7-9700K)  
WAC : 291/300 @10sec per position (single thread on an i7-9700K)  

### Home test
Here are some fast TC results (CCRL Blitz TC)
```
Rank Name                          Elo     +/-   Games   Score   Draws
   0 minic_1.49                      9       6    7842   51.3%   46.2%
   1 RubiChess                     185      28     412   74.4%   35.2%
   2 demolito                      104      27     413   64.5%   36.1%
   3 igel-last                      76      25     413   60.8%   45.0%
   4 PeSTO_bmi2                     19      26     413   52.7%   38.5%
   5 Winter0.7                       5      26     412   50.7%   40.3%
   6 minic_1.48                     -1      19     412   49.9%   68.7%
   7 Hakkapeliitta                  -5      27     413   49.3%   33.2%
   8 amoeba                         -6      24     413   49.2%   47.0%
   9 minic_1.45                    -13      20     413   48.2%   64.4%
  10 Topple_master                 -17      26     412   47.6%   39.8%
  11 rodentIV                      -23      26     413   46.7%   39.7%
  12 minic_1.46                    -24      21     413   46.6%   61.3%
  13 cheng4_linux_x64              -30      25     413   45.6%   43.3%
  14 minic_1.39                    -45      20     413   43.6%   65.4%
  15 minic_1.35                    -66      22     413   40.7%   55.7%
  16 combusken-linux-64            -70      27     413   40.1%   38.0%
  17 zurichess-neuchatel           -74      26     412   39.6%   39.8%
  18 minic_1.19                    -76      23     413   39.2%   51.3%
  19 FabChessv1.13                -102      28     413   35.7%   34.6%
```

Minic strength can be ajdusted using the level option (from command line, json configuration file, or using protocol option support, using value from 0 to 100). Level 0 is a random mover, 1 to 30 very weak, ..., level 100 is full strength. Level functionnaly will be enhanced in a near future.

Minic random-mover stats are the following :
```
   7.73%  0-1 {Black mates}
   7.50%  1-0 {White mates}
   2.45%  1/2-1/2 {Draw by 3-fold repetition}
  21.99%  1/2-1/2 {Draw by fifty moves rule}
  54.16%  1/2-1/2 {Draw by insufficient mating material}
   6.13%  1/2-1/2 {Draw by stalemate}
```

Current level elo are more or less so that even a kid can beat low levels Minic. From level 50 or 60, you will start to struggle more! You can also use the UCI_Elo parameter if UCI_LimitStrenght is activated.

## How to compile
* Linux : use the given build script (or make your own ...)
* Windows : use the given VS2017 project (or make your own ...), or use the Linux cross-compilation script given.

## Syzygy EGT
To compile with SYZYGY support you'll need to clone https://github.com/jdart1/Fathom as Fathom directory and activate WITH_SYZYGY definition at compile time.
To use EGT just specify syzygyPath in the configuration file or in the command line.

## How to run
add the command line option "-xboard" to go to xboard/winboard mode or -uci for UCI.

Other available options are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -qsearch <"fen"> : just a qsearch ...
* -mateFinder <"fen"> depth : same as analysis but without prunings in search
* -pgn <file> : extraction tool to build tuning data
* -texel <file> : run a texel tuning session
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
   "level": 100,
   "syzygyPath": "/data/minic/TB/syzygy"
}

```
and a command line interface, using the same key word. For instance, forcing mateFinder mode can be done by adding "-mateFinder 1" when calling Minic.

Starting from release 1.00, important options are also available through protocol.

## Opening books

Minic comes with some opening books written in an internal own binary format. There are currently 4 books
```
book_small : a very small book only with main variation of classic opening lines
book_big   : a bigger book (take 5 secondes to load) of nearly 400.000 lines
Carlsen    : based on Carlsen most used opening lines (thanks to Jonathan Cremers)
Anand      : based on Anand most used opening lines (thanks to Jonathan Cremers)
```

You can use both the json configuation file or the command line argument to select the book.

## Thanks

Of course many/most idea in Minic are taken from the beautifull chess developer community.
Here's a very incomplete list of inspiring open-source engines :

Arasan by Jon Dart  
CPW by Pawel Koziol and Edmund Moshammer  
Deepov by Romain Goussault  
Defenchess by Can Cetin and Dogac Eldenk  
Demolito by Lucas Braesch  
Dorpsgek by Dan Ravensloft  
Ethereal by Andrew Grant  
Galjoen by Werner Taelemans  
Madchess by Erik Madsen  
Rodent by Pawel Koziol  
RubiChess by Andreas Matthies  
Stockfish by Tord Romstad, Marco Costalba, Joona Kiiski and Gary Linscott  
Texel by Peter Österlund   
Topple by Vincent Konsolas  
TSCP by Tom Kerrigan  
Vajolet by Marco Belli  
Vice by BlueFeverSoft  
Xiphos by Milos Tatarevic  
Zurichess by Alexandru Moșoi   

Many thanks also to all testers for all those long time control tests, they really are valuable inputs in the chess engine development process. 

Also thanks to TCEC for letting Minic participate to the Season 15, 16, 17 and 18, it is fun to see Minic on such a great hardware.

And of course thanks to all the members of the talkchess forum and CPW, and to H.G. Muller for hosting the friendly monthly tourney.

## Info
- Am I a chess player ?
- yes
- good one ?
- no (https://lichess.org/@/xr_a_y)
