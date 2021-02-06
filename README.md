<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo2.png" width="350">

# Minic
A chess engine I made to learn about chess programming.
It is compatible with both xboard/winboard and UCI protocol.

## Code style
For a year and a half Minic was (mainly) a one-file-code with very dense lines. This is of course very wrong in terms of software design... So why is it so? First reason is that Minic was first developped as a week-end project (in mid-october 2018), the quick-and-dirty way, and since then I was having fun going on this way ; being a "small" code size engine was part of the stakes in developing it. Second reason is that it helps the compilers optimize the whole code (at least that is what I though...)

Until version 2 of Minic, some optional features such as Texel tuning, perft, tests, uci support, book generation ... were available in the Add-Ons directory ; they are now fused with the source code.

Indeed, since version "2" Minic is now written in a more classic c++ style, although very dense lines are still present and recall Minic past compacity...

## More history

### Week-end project
Initially, the code size was supposed not to go above 2000sloc. It started as a week-end project in october 2018 (http://talkchess.com/forum3/viewtopic.php?f=2&t=68701). But, as soon as more features (especially SMP, material tables and bitboard) came up, I tried to keep it under 4000sloc and then 5000sloc. This is why this engine was named Minic, this stands for "Minimal Chess" (and is not related to the GM Dragoljub Minić) but it has not much to do with minimalism anymore...  

### Version "1" 
was released as a one year anniversary release in october 2019. At this point Minic has already go from a 1800 Elo 2-days-of-work engine, to a 2800 Elo engine being invited at TCEC qualification league.

### Version "2"
is released for April 1st 2020 (during covid-19 confinement). For this version, the one file Minic was splitted into many header and source files, and commented a lot more without negative impact on speed and strength. 

### Version "3"
is released in november 2020 (during second covid-19 confinement) as a 2 years anniversary release and *is not using, nor compatible with, SF NNUE implementation anymore*.

### NNUE from release 2.47 to release 2.53 (from Stockfish implementation)
Minic, since release 2.47, has the possibility to be build using a shameless copy of the NNUE framework of Stockfish. Integration of NNUE was done easily and I hope this can be done for any engine, especially if NNUE is release as a standalone library. New UCI parameter NNUEFile is added and shall be the full path to the network file you want to use. To build such Minic you need to activate WITH_NNUE in definition.hpp and use the build script (or make your own, but do not forget to pass -DUSE_AVX2 or whatever your hardware supports to the NNUE part ...). First test shows that MinicNNUE is around 200Elo stronger than Minic, around the level of Xiphos or Ethereal currently at short TC and maybe something like 50Elo higher at longer TC (around Komodo11). This says that a lot more can (and will!) be done inside Minic standard evaluation function !

When using a NNUE network with this Stockfish implementation, it is important that Minic is called MinicNNUE (or Minnuec as introduced by Gekkehenker).
MinicNNUE, won't be the official Minic, as this NNUE work to not reflect my own work and skills at all !

Later on, since version 2.50, the NNUE learner from NodCHip repo has also been ported to Minic so that networks using Minic data and search can be done.
The genFen part was not ported and in internal process to produce training is used. This include both extracting position from fixed depth game and from random position.

Nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets.

### NNUE from release 3.00 (from Seer implementation)
Starting from release 3.00, **Minic is not using Stockfish NNUE implementation anymore and is no more compatible with SF nets**. It was too much foreign code inside Minic to be fair, to be maintained, to be fun.
Seer engine is offering a very well written implementation of NNUE that I borrowed and adapt to Minic. The code is more or less 400 lines. I choose to keep Stockfish code for binary sfens format as everyone is using this for now. Training code is an external tool written in Python without any dependency to engine, also adapted from Seer repository and Gary Linscott pytorch trainer.
For now, generated nets are still quite weak, but that is a starting point, a new story to be written, in Minic 3.

Nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets.

## Release process
WARNING : Dist directory as been REMOVED from the repository because it was starting to be too big. Unofficial releases are not available anymore here. All (unofficial) releases are available in a new repo, here : https://github.com/tryingsomestuff/Minic-Dist, also available as a git submodule.

Some stable/official ones will still be made available as github release. I "officially release" (create a github version) as soon as I have some validated elo (at least +10) or an important bug fix.

In a github release, a tester shall only use the given (attached) binaries. The full "source" package always contains everything (source code, test suites, opening suite, books, ...) using git "submodule" so that the main repository remains small. 

Starting from release 2.27 new binaries are named following this convention :

```
* minic_2.27_linux_x64_skylake     : fully optimized Linux64 (avx2+bmi2)   
* minic_2.27_linux_x64_nehalem     : optimized Linux64 (sse4.2)  
* minic_2.27_linux_x64_x86-64      : basic Linux64 (but at least sse3 if NNUE is activated)  
* minic_2.27_mingw_x64_skylake.exe : fully optimized Windows64 (avx2+bmi2)   
* minic_2.27_mingw_x64_nehalem.exe : optimized Windows64 (sse4.2)   
* minic_2.27_mingw_x64_x86-64.exe  : basic Windows64 (but at least sse3 if NNUE is activated)  
* minic_2.27_mingw_x32_skylake.exe : fully optimized Windows32 (avx2+bmi2)  
* minic_2.27_mingw_x32_nehalem.exe : optimized Windows32 (sse4.2)  
* minic_2.27_mingw_x32_i686.exe    : basic Windows32 
* minic_2.27_android               : android armv7  
```   
Please note that Win32 binaries are very slow (I don't know why yet, so please use Win64 one if possible).
Please note that Minic has always been a little weaker under Windows OS.
     
## Strength

### CCRL
Minic 2.46 is tested at 3054 on the [CCRL 40/15 scale](http://ccrl.chessdom.com/ccrl/4040/)  
Minic 2.33 is tested at 3159 on the [CCRL BLITZ 4CPU scale](http://ccrl.chessdom.com/ccrl/404/)  
Minic 2.43 is tested at 3116 on the [CCRL BLITZ 1CPU scale](http://ccrl.chessdom.com/ccrl/404/)  
Minic 3.02 is tested at 3154 on the [CCRL BLITZ 1CPU scale](http://ccrl.chessdom.com/ccrl/404/)  
Minic 2.48 is tested at 3025 on the [CCRL FRC list](http://ccrl.chessdom.com/ccrl/404FRC/)  

### CEGT
Minic 2.48 is tested at 2982 on the [CEGT 40/4 list](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html)  
MinicNNUE 2.51 using Nascent Nutrient is tested at 3216 on the [CEGT 40/4 list](http://www.cegt.net/40_40%20Rating%20List/40_40%20All%20Versions/rangliste.html)  
Minic 3.02 using Narcotized Nightshift is tested at 3044 on the [CEGT 40/20 list](http://www.cegt.net/40_40%20Rating%20List/40_40%20All%20Versions/rangliste.html)  
Minic 2.35 is tested at 3034 on the [CEGT 40/4 MP8CPU list](http://www.cegt.net/40_4_Ratinglist/40_4_mp/rangliste.html)  
Minic 2.07 is tested at 2935 on the [CEGT 5+3 PB=ON list](http://www.cegt.net/5Plus3Rating/BestVersionsNEW/rangliste.html)  
Minic 2.48 is tested at 2969 on the [CEGT 40/15 list](http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html)  

### FGRL
Minic 2.51 is tested at 2900 on the [fastgm 60sec+0.6sec rating list](http://www.fastgm.de/60-0.60.html)  
Minic 2.48 is tested at 2936 on the [fastgm 10min+6sec rating list](http://www.fastgm.de/10min.html)  
Minic 2.00 is tested at 2954 on the [fastgm 60min+15sec rating list](http://www.fastgm.de/60min.html)  

### SP-CC
MinicNNUE 2.51 using Nascent Nutrient net is tested at 3284 on the [SP-CC 3min+1s rating list](https://www.sp-cc.de/) 

### Test Suite
STS : 1191/1500 @10sec per position (single thread on an i7-9700K)  
WAC : 291/300 @10sec per position (single thread on an i7-9700K)  

### Home test
Here are some fast TC results (STC 10s+0.1)
```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 demolito                      105      16    1300   64.7%   31.5%
   2 igel-dev                       62      15    1300   58.8%   34.0%
   3 texel                          30      15    1300   54.3%   33.3%
   4 Vajolet2_2.8                   -3      15    1301   49.6%   33.1%
   5 minic_2.48                    -17      15    1300   47.5%   36.4%
   6 minic_2.47                    -26      15    1300   46.3%   33.7%
   7 minic_2.45                    -30      15    1301   45.7%   35.2%
   8 Winter                        -57      16    1299   41.8%   28.6%
   9 combusken                     -61      16    1299   41.3%   30.6%

5850 of 360000 games finished.
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
* Linux : just type "make", or use the given build script Tools/build.sh (or make your own ...), or try to have a look at Tools/TCEC/update.sh for some hints. The executable will be available in Dist/Minic3 subdirectory.
* Windows : use the Linux cross-compilation script given or make your own. From time to time I also check that recent VisualStudio versions can compile Minic without warnings but I don't distribute any VS project.
* Android/RPi : use the given cross-compilation script or make your own.

## Syzygy EGT
To compile with SYZYGY support you'll need to clone https://github.com/jdart1/Fathom as Fathom directory and activate WITH_SYZYGY definition at compile time.
To use EGT just specify syzygyPath in the command line or GUI option.

## How to run
add the command line option "-xboard" to go to xboard/winboard mode or -uci for UCI.

Other available options (depending on compilation option, see definition.hpp) are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -qsearch <"fen"> : just a qsearch ...
* -mateFinder <"fen"> depth : same as analysis but without prunings in search
* -pgn <file> : extraction tool to build tuning data
* -texel <file> : run a Texel tuning session
* -selfplay \[depth\] \[number of games\] (default are 15 and 1): launch some selfplay game with genfen activated
* ...

## Options

### Command line

Minic comes with some command line options :
* -quiet \[0 or 1\] (default is 1 which means "true"): make Minic more verbose for debug purpose. This option is often needed when using unsual things as Texel tuning or command line analysis for instance.
* -debugMode \[0 or 1\] (default is 0 which means "false"): will write every output also in a file (named minic.debug by default)
* -debugFile \[name_of_file\] (default is minic.debug): name of the debug output file
* -ttSizeMb \[number_in_Mb\]: force the size of the hash table. This is usefull for command line analysis mode for instance
* -FRC \[0 or 1\] (default is 0): activate Fisher random chess mode. This is usefull for command line analysis mode for instance
* -threads \[number_of_threads\] (default is 1): force the number of threads used. This is usefull for command line analysis mode for instance
* -mateFinder \[0 or 1\] (default is 0): activate mate finder mode, which essentially means no forward pruning
* -fullXboardOutput \[0 or 1\] (default is 0): activate additionnal output for Xboard protocol such as nps or tthit
* -multiPV \[from 1 to 4 \] (default is 1): search more lines at the same time
* -randomOpen \[from 0 to 100 \] (default is 0): value in cp that allows to add randomness for ply <10. Usefull when no book is used
* -level \[from 0 to 100\] (default is 100): change Minic skill level
* -limitStrength \[0 or 1\] (default is 0): take or not strength limitation into account
* -strength \[Elo_like_number\] (default is 1500): specify a Elo-like strength (not really well scaled for now ...)
* -syzygyPath \[path_to_egt_directory\] (default is none): specify the path to syzygy end-game table directory
* -NNUEFile \[path_to_neural_network_file\] (default is none): specify the neural network (NNUE) to be used and activate NNUE evaluation
* -forceNNUE \[0 or 1\] (default is false): if a NNUEFile is loaded, forceNNUE equal true will results in a pure NNUE evaluation, while the default is hybrid evaluation
* -genFen \[ 0 or 1 \] (default is 0): activate sfen generation
* -genFenDepth \[ 2 to 20\] (default is 8): specify depth of search for sfen generation
* -randomPly \[0 to 20 \] (default is 0): usefull when creating training data, play this number of total random ply at the begining of the game

### GUI/protocol (Xboard or UCI)

Starting from release 1.00 Minic support setting options using protocol (both XBoard and UCI). Option priority are as follow : command line option can be override by protocol option. This way, Minic supports strength limitation, FRC, pondering, can use contempt, ...  
If compiled with the WITH_SEARCH_TUNING definition, Minic can expose all search algorithm parameters so that they can be tweaked. Also, when compiled with WITH_PIECE_TUNING, Minic can expose all middle- and end-game pieces values.

### Training data generation

There are multiple ways of generating sfen data with Minic.
* First is classic play at fixed depth to generate pgn (I am using cutechess for this). Then use convert this way 
```
pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o data.plain games.pgn
```
Then use Minic -pgn2bin option to get a binary format sfen file. Note than position without score won't be taken into account.
* Use Minic random mover (level = 0) and play tons of random games activating the genFen option and setting the depth of search you like with genFenDepth. This will generate a "plain" format sfen file with game results always being 0, so you will use this with lambda=1 in your trainer to be sure to don't take game outcome into account. What you will get is some genfen_XXXXXX files (one for each Minic processus, note that with cutechess if only 2 engines are playing, only 2 process will run and been reused). Those files will be in workdir directory of the engine and are in "plain" format. So after that use Minic -plain2bin on that file to get a "binary" file. Minic will generate only quiet positions (at qsearch leaf).
* Use Minic "selfplay genfen" facility. In this case again, note that game result will always be draw because the file is written on the fly not at the end of the game. Again here you will obtain genfen_XXXXXX files. Minic will generate only quiet positions (at qsearch leaf).

## Style

Moreover, Minic implements some "style" parameters that allow the user to boost or minimize effects of :
- material
- attack
- development
- mobility
- positional play
- forwardness
- complexity (not yet implemented)

Default values are 50 and a range from 0 to 100 can be used. A value of 100 will double the effect, while a value of 0 will disable the feature (it is probably not a good idea to put material awareness to 0 for instance ...).

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
Seer by Connor McMonigle  
Stockfish by Tord Romstad, Marco Costalba, Joona Kiiski and Gary Linscott  
Texel by Peter Österlund   
Topple by Vincent Konsolas  
TSCP by Tom Kerrigan  
Vajolet by Marco Belli  
Vice by BlueFeverSoft  
Xiphos by Milos Tatarevic  
Zurichess by Alexandru Moșoi   

Many thanks also to all testers for all those long time control tests, they really are valuable inputs in the chess engine development process. 

Also thanks to TCEC for letting Minic participate to the Season 15 to 20, it is fun to see Minic on such a great hardware.

And of course thanks to all the members of the talkchess forum and CPW, and to H.G. Muller and Joost Buijs for hosting the well-known friendly monthly tourney.

## Info
- Am I a chess player ?
- yes
- good one ?
- no (https://lichess.org/@/xr_a_y)
