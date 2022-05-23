<p align="center">
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png" width="350"> 
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo_dark.png" width="350">
<br>
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo_dark.png" width="350">
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png" width="350"> 
</p>

# Minic
A chess engine I made to learn about chess programming.  
It is compatible with both CECP (xboard) and UCI protocol.  
It is currently inside top20 in major rating lists.

   * [Support Minic development](#support-minic-development)
   * [History &amp; the NNUE Minic story](#history--the-nnue-minic-story)
   * [Minic NNUE "originality" status](#minic-nnue-originality-status)
   * [Strength](#strength)
   * [Rating Lists &amp; competitions](#rating-lists--competitions)
   * [Release process](#release-process)
   * [How to compile](#how-to-compile)
   * [Syzygy EGT](#syzygy-egt)
   * [How to run](#how-to-run)
   * [Options](#options)
   * [Videos](#videos)
   * [Thanks](#thanks)

## Support Minic development

Generating data, learning process, tuning and testing of a chess engine is quite hardware intensive ! I have an i7-9700K at home and I'm renting an E5-2650v2 on the cloud but this is far from enough. This is why I opened a [Patreon](https://www.patreon.com/minicchess) account for Minic ; if you want to support Minic development, it is the place to be ;-)

## History & the NNUE Minic story

### Code style
For a year and a half Minic was (mainly) a one-file-code with very dense lines. This is of course very wrong in terms of software design... So why is it so? First reason is that Minic was first developped as a week-end project (in mid-october 2018), the quick-and-dirty way, and since then I was having fun going on this way ; being a "small" code size engine was part of the stakes in developing it. Second reason is that it helps the compilers optimize the whole code (at least that is what I though...)

Until version 2 of Minic, some optional features such as evaluation and searchs tuning, perft, tests, uci support, book generation ... were available in the Add-Ons directory ; they are now fused with the source code.

Nowadays, since the release of version "2" Minic is written in a more classic c++ style, although very dense lines are still present and recall Minic past compacity...

More details about Minic history in the next paragraphs...

### Week-end project (October 2018)
Initially, the code size of Minic was supposed not to go above 2000sloc. It started as a week-end project in october 2018 (http://talkchess.com/forum3/viewtopic.php?f=2&t=68701). But, as soon as more features (especially SMP, material tables and bitboard) came up, I tried to keep it under 4000sloc and then 5000sloc, ... This is why this engine was named Minic, this stands for "Minimal Chess" (and is not related to the GM Dragoljub Minić) but it has not much to do with minimalism anymore...  

### Version "1" (October 2019)
was released as a one year anniversary release in october 2019. At this point Minic has already go from a 1800 Elo 2-days-of-work engine, to a 2800 Elo engine being invited at TCEC qualification league.

### Version "2" (April 2020)
is released for April 1st 2020 (during covid-19 confinement). For this version, the one file Minic was splitted into many header and source files, and commented a lot more, without negative impact on speed and strength. 

#### NNUE from release 2.47 to release 2.53 (from Stockfish implementation)
Minic, since release 2.47 of August 8th 2020 (http://talkchess.com/forum3/viewtopic.php?f=2&t=73521&hilit=minic2&start=50#p855313), has the possibility to be build using a shameless copy of the NNUE framework of Stockfish. Integration of NNUE was done easily and I hope this can be done for any engine, especially if NNUE is release as a standalone library (update: see https://github.com/dshawul/nncpu-probe for instance). New UCI parameter NNUEFile is added and shall be the full path to the network file you want to use. To build such Minic you need to activate WITH_NNUE in definition.hpp and use the build script (or make your own, but do not forget to pass -DUSE_AVX2 or whatever your hardware supports to the NNUE part ...). First test shows that MinicNNUE is around 200Elo stronger than Minic, around the level of Xiphos or Ethereal currently at short TC and maybe something like 50Elo higher at longer TC (around Komodo11). This says that a lot more can (and will!) be done inside Minic standard evaluation function !

When using a NNUE network with this Stockfish implementation, it is important that Minic is called MinicNNUE (or Minnuec as introduced by Gekkehenker).
MinicNNUE, won't be the official Minic, as this NNUE work to not reflect my own work and skills at all !

Later on, since version 2.50, the NNUE learner from NodChip repo has also been ported to Minic so that networks using Minic data and search can be done.
The genFen part was not ported and in internal process to produce training is used. This include both extracting position from fixed depth game and from random position.

Nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets.

### Version "3" (November 2020)
is released in november 2020 (during second covid-19 confinement) as a 2 years anniversary release and *is not using, nor compatible with, SF NNUE implementation anymore*. More about this just below...

#### NNUE from release 3.00 (initially from Seer implementation)
Starting from release 3.00, **Minic is not using Stockfish NNUE implementation anymore and is no more compatible with SF nets**. It was too much foreign code inside Minic to be fair, to be maintained, to be fun.
Seer engine is offering a very well written implementation of NNUE that I borrowed and adapt to Minic (https://github.com/connormcmonigle/seer-nnue). The code is more or less 400 lines. I choose to keep Stockfish code for binary sfens format as everyone is using this for now. Training code is an external tool written in Python without any dependency to engine, also first adapted from Seer repository and then from Gary Linscott pytorch trainer (https://github.com/glinscott/nnue-pytorch).
For now, generated nets are still quite weak, but that is a starting point, a new story to be written, in Minic 3.

Nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets.

### Some stuff that were maybe introduced in Minic

- use of PST score in move sorter to compensate near 0 history of quiet move
- aggregate history score for move sorter and history heuristic (cmh + history[piece and color][tosquare] + history[color][square][square])
- danger (from evaluation) based pruning and reductions in search
- contempt opponent model taking opponent name or opponent rating into account
- "features" based evaluation parameter available to the user to tune game play
- using a depth factor for pruning and reduction that takes TT entry depth into account

### Minic NNUE "originality" status

- Inference code : originally based on Seer one (Connor McMonigle), many refactoring and experiements inside (clipped ReLU, quantization on read, vectorization, ...).
- Network topology : currently same at a previous Seer release. Many many others have been tested (with or without skip connections, bigger or smaller input layer, number of layers, ...) mainly without success except the small "Nibbled Nutshell" net. Still trying to find a better idea ...
- Training code : mainly based on the Gary Linscott and Tomasz Sobczyk (@Sopel) pytorch trainer (https://github.com/glinscott/nnue-pytorch), adapted and tuned to Minic.
- Data generation code : fully original, pure Minic data. Many ideas has been tried (generate inside search tree, self-play, multi-pv, random, ...). 
- Other tools : many little tools around training process, borrowed here and there or developed by myself.

In brief, Minic NNUE world is vastly inspired from what others are doing and is using pure Minic data.

## Strength

Minic is currently near the top20 with a Elo rating around 3200 at [CCRL scale](https://ccrl.chessdom.com/ccrl/404/cgi/compare_engines.cgi?class=Open+source+single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no).

### Various NNUE nets strenght 

This table shows nets strength of various Minic nets on an AVX2 hardware at short TC (10s+0.1). Results will be a lot different on older hardware where NNUE evaluation is much slower. I thus encourage users to only use Minic with NNUE nets on recent hardware.

```

Rank Name                          Elo     +/-   Games   Score    Draw 
   1 minic_2.53_nn-97f742aaefcd     165      24     458   72.1%   42.8% 
   2 minic_3.17_NiNe3               112      24     457   65.5%   44.0% 
   3 minic_2.53_napping_nexus        97      24     458   63.6%   42.1% 
   4 minic_3.18                      97      25     457   63.6%   40.5% 
   5 minic_3.17                      -5      24     457   49.3%   41.4% 
   6 minic_2.53_nascent_nutrient     -8      25     457   48.9%   40.5% 
   7 minic_3.14                     -55      23     458   42.1%   46.7% 
   8 minic_3.08                     -57      25     457   41.9%   40.9% 
   9 minic_3.02_nettling_nemesis    -72      25     458   39.7%   39.3% 
  10 minic_3.06_nocturnal_nadir    -114      25     457   34.1%   38.9% 
  11 minic_3.04_noisy_notch        -155      27     458   29.0%   33.2% 
```

More details about those nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets.

### A word on NNUE and vectorization

In this table, Minic 3.19 is used to compare NNUE performances on various CPU architecture (effect of vectorisation).
```
Rank Name                          Elo     +/-   Games   Score    Draw 
   1 minic_3.19_slylake             68      23     422   59.7%   50.7% 
   2 minic_3.19_sandybridge         14      23     423   52.0%   53.4% 
   3 minic_3.19_nehalem            -27      23     421   46.1%   52.3% 
   4 minic_3.19_core2              -55      24     422   42.2%   49.3% 
```
What does this say ?
Well ... for NNUE, using AVX2 is very important. This can explain some strange results during some testing process and in rating list where I sometimes see my nets underperform a lot. So please, use AVX2 hardware (and the corresponding Minic binary, i.e. the "skylake" one for Intel or at least the "znver1" for AMD) for NNUE testing if possible.

### Threading performances

I'd love to own a big enough hardware to test with more than 8 threads ... Here are 3s+0.1 TC results to illustrate threading capabilities.
```
Rank Name                          Elo     +/-   Games   Score    Draw 
   1 minic_3.19_8                   123      34     156   67.0%   58.3% 
   2 minic_3.19_6                    94      36     156   63.1%   54.5% 
   3 minic_3.19_4                    20      33     156   52.9%   63.5% 
   4 minic_3.19_2                   -45      35     156   43.6%   59.0% 
   5 minic_3.19_1                  -206      41     156   23.4%   42.9% 
```

### Home test
Here are some fast TC results of a gauntlet tournament (STC 10s+0.1) for Minic 3.19 (dev version of various engine mai 22 2022).
```
   # PLAYER                        :  RATING  ERROR   POINTS  PLAYED   (%)  CFS(%)
   1 berserk-x64-pext              :   328.7   23.5    555.0     642    86      99
   2 Koivisto                      :   285.4   29.6    534.0     641    83     100
   3 RubiChess                     :   189.1   22.1    476.5     641    74     100
   4 seer                          :   146.5   24.3    445.5     641    70     100
   5 stockfish.8                   :    68.1   19.8    381.0     641    59      61
   6 Igel                          :    62.9   24.2    376.5     641    59      67
   7 Ethereal                      :    54.9   21.7    369.5     641    58      85
   8 Pedone_linux_bmi2             :    40.8   20.8    357.0     641    56      96
   9 Clover.3.1                    :    14.5   19.0    333.5     641    52      54
  10 Halogen                       :    12.8   21.6    332.0     641    52      73
  11 arasanx-64-avx2-bmi2          :     4.4   22.3    325.0     642    51      65
  12 minic_3.19                    :     0.0   ----  10486.0   19233    55      89
  13 tucano_avx2                   :   -13.3   21.6    308.5     641    48      52
  14 nemorino_nnue                 :   -13.8   17.9    308.0     641    48      78
  15 stockfish.7                   :   -23.8   18.9    299.0     641    47      58
  16 komodo-10                     :   -26.6   20.0    296.5     641    46      50
  17 Wasp550-linux-avx             :   -26.6   20.0    296.5     641    46      73
  18 xiphos                        :   -35.0   18.9    289.0     641    45      80
  19 BlackMarlin                   :   -46.8   18.3    278.5     641    43      66
  20 minic_3.18                    :   -51.9   19.4    274.0     641    43      75
  21 zahak-linux-amd64-10.0-avx    :   -61.6   21.0    265.5     641    41      70
  22 rofChade_bmi2                 :   -68.5   17.0    259.5     641    40      95
  23 weiss                         :   -91.3   20.3    240.0     641    37     100
  24 stash-bot                     :  -161.0   22.7    184.5     641    29      65
  25 Defenchess_2.2                :  -167.1   22.4    180.0     641    28      83
  26 marvin                        :  -183.9   24.7    168.0     641    26      98
  27 Drofa_dev                     :  -215.3   20.1    147.0     641    23      92
  28 Winter                        :  -241.3   27.2    131.0     641    20      94
  29 combusken                     :  -268.0   24.3    116.0     641    18      52
  30 Vajolet2_2.8                  :  -268.9   22.7    115.5     641    18      88
  31 Bit-Genie                     :  -290.6   26.3    104.5     642    16     ---
```

### Random mover
Minic random-mover stats are the following :
```
   7.73%  0-1 {Black mates}
   7.50%  1-0 {White mates}
   2.45%  1/2-1/2 {Draw by 3-fold repetition}
  21.99%  1/2-1/2 {Draw by fifty moves rule}
  54.16%  1/2-1/2 {Draw by insufficient mating material}
   6.13%  1/2-1/2 {Draw by stalemate}
```

## Rating Lists & competitions

### Progress
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/Doc/minic_progress.JPG" width="450">

### CCRL
- 40/15: Minic 3.18 + Nimble Nothingness is tested at 3366 on the [CCRL 40/15 scale, 4 cores](http://ccrl.chessdom.com/ccrl/4040/)  
- Blitz: Minic 3.17 + Nucleated Neurulation is tested at 3458 on the [CCRL BLITZ scale, 8 cores](http://ccrl.chessdom.com/ccrl/404/)  
- FRC: Minic 3.18 + Nimble Nothingness is tested at 3468 on the [CCRL FRC list](http://ccrl.chessdom.com/ccrl/404FRC/)  

### CEGT
- 40/4: Minic 3.18 + Nimble Nothingness is tested at 3303 on the [CEGT 40/4 list](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html)  
- 40/20: Minic 3.18 + Nimble Nothingness is tested at 3259 on the [CEGT 40/20 list](http://www.cegt.net/40_40%20Rating%20List/40_40%20All%20Versions/rangliste.html)  
- 5+3 pb=on: Minic 3.18 + Nimble Nothingness is tested at 3341 on the [CEGT 5+3 PB=ON list](http://www.cegt.net/5Plus3Rating/BestVersionsNEW/rangliste.html)  
- 25+8: Minic 3.18 + Nimble Nothingness is tested at 3360 on the [CEGT 25+8 list]([http://www.cegt.net/5Plus3Rating/BestVersionsNEW/rangliste.html](http://www.cegt.net/25plus8Rating/BestVersions/rangliste.html))  

### FGRL
- Minic Minic 3.18 + Nimble Nothingness is tested at 3143 on the [fastgm 60sec+0.6sec rating list](http://www.fastgm.de/60-0.60.html)  
- Minic 3.17 + Nucleated Neurulation is tested at 3159 on the [fastgm 10min+6sec rating list](http://www.fastgm.de/10min.html)  
- Minic 3.17 + Nucleated Neurulation is tested at 3172 on the [fastgm 60min+15sec rating list](http://www.fastgm.de/60min.html)  
- Minic 3.18 + Nimble Nothingness is tested at 3276 on the [fastgm 60sec+0.6sec 16 cores rating list](http://www.fastgm.de/16-60-0.6.html)

### SP-CC
- Minic 3.18 + Nimble Nothingness is tested at 3383 on the [SP-CC 3min+1s rating list](https://www.sp-cc.de/)  

### GRL
- Minic 3.17 + Nucleated Neurulation is tested at 3229 on the [GRL 40/2 rating list](http://rebel13.nl/grl-best-40-2.html)
- Minic 3.06 using Nocturnal Nadir net is tested at 3078 on the [GRL 40/15 rating list](http://rebel13.nl/grl-best-40-15.html)

### BRUCE
- Minic 3.18 + Nimble Nothingness is tested at 3305 on the [BRUCE rating list](https://e4e6.com/)

### Test Suite
- STS : 1191/1500 @10sec per position (single thread on an i7-9700K)  
- WAC : 291/300 @10sec per position (single thread on an i7-9700K)  

### TCEC stats

TCEC hardware: Minic is at 3429 (https://tcec-chess.com/bayeselo.txt)

### TCEC

Here are Minic results at TCEC (https://tcec-chess.com/)

TCEC15: 8th/10 in Division 4a (https://www.chessprogramming.org/TCEC_Season_15)    
TCEC16: 13th/18 in Qualification League (https://www.chessprogramming.org/TCEC_Season_16)   
TCEC17: 7th/16 in Q League, 13th/16 in League 2 (https://www.chessprogramming.org/TCEC_Season_17)  
TCEC18: 4th/10 in League 3 (https://www.chessprogramming.org/TCEC_Season_18)  
TCEC19: 3rd/10 in League 3 (https://www.chessprogramming.org/TCEC_Season_19)  
TCEC20: 2nd/10 in League 3, 9th/10 in League 2 (https://www.chessprogramming.org/TCEC_Season_20)  
TCEC21: 1st/12 in League 3, 6th/10 in League 2 (https://www.chessprogramming.org/TCEC_Season_21)  
TCEC22: 2nd/8 in League 2, 8th/8 in League 1 (well tried ;) )  (https://www.chessprogramming.org/TCEC_Season_22)  

## Release process
WARNING : the former Dist directory as been REMOVED from the repository because it was starting to be too big. Unofficial releases are not available anymore here. All (unofficial) releases are available in a new repo, here : https://github.com/tryingsomestuff/Minic-Dist, also available as a git submodule.

Some stable/official ones will still be made available as github release (https://github.com/tryingsomestuff/Minic/releases). I "officially release" (create a github version) as soon as I have some validated elo (at least +10) or an important bug fix.

In a github release, a tester shall only use the given (attached) binaries. The full "source" package always contains everything (source code, test suites, opening suite, books, ...) but using git "submodule" so that the main repository remains small. 

Binaries are named following this convention :
```
Linux 64:
-- Intel --
* minic_X.YY_linux_x64_skylake         : fully optimized Linux64 (popcnt+avx2+bmi2)  
* minic_X.YY_linux_x64_sandybridge     : optimized Linux64 (popcnt+avx)  
* minic_X.YY_linux_x64_nehalem         : optimized Linux64 (popcnt+sse4.2)  
* minic_X.YY_linux_x64_core2           : basic Linux64 (nopopcnt+sse3)  

-- AMD --
* minic_X.YY_linux_x64_znver3          : fully optimized Linux64 (popcnt+avx2+bmi2)  
* minic_X.YY_linux_x64_znver1          : almost optimized Linux64 (popcnt+avx2)  
* minic_X.YY_linux_x64_bdver1          : optimized Linux64 (nopopcnt+avx)  
* minic_X.YY_linux_x64_barcelona       : optimized Linux64 (nopopcnt+sse4A)  
* minic_X.YY_linux_x64_athlon64-sse3   : basic Linux64 (nopopcnt+sse3)  

Windows 64:
Some as for Linux with naming convention like this minic_X.YY_mingw_x64_skylake.exe

Windows 32:
* minic_X.YY_mingw_x32_pentium2.exe    : very basic Windows32  

Others:
* minic_X.YY_android                   : android armv7  
* minic_X.YY_linux_x32_armv7           : RPi armv7
* minic_X.YY_linux_x64_armv8           : RPi armv8
```   

Please note that for Linux binaries to work you will need a recent libc (>=2.33, likely an ubuntu21.04 for instance) installed on your system.  
Please note that Win32 binaries are very slow so please use Win64 one if possible.  
Please note that Minic has always been a little weaker under Windows OS (probably due to cross-compilation).
     
## How to compile
* Linux (Gcc>9.2 requiered): just type "make", or use the given build script Tools/tools/build.sh (or make your own ...), or try to have a look at Tools/TCEC/update.sh for some hints. The executable will be available in Dist/Minic3 subdirectory.
* Windows : use the Linux cross-compilation script given or make your own. From time to time I also check that recent VisualStudio versions can compile Minic without warnings but I don't distribute any VS project.
* Android/RPi : use the given cross-compilation script or make your own.

## Syzygy EGT
To compile with SYZYGY support you'll need to clone https://github.com/jdart1/Fathom as Fathom directory and activate WITH_SYZYGY definition at compile time.
This can be done using the given git submodule or by hand. To use EGT just specify syzygyPath in the command line or using the GUI option.

## How to run
add the command line option "-xboard" to go to xboard/winboard mode or -uci for UCI. If not option is given Minic will **default to use UCI protocol**.
Please note that if you want to force specific option from command line instead of using protocol option, you **have** to first specific protocol as the first command line argument. For instance `minic -uci -minOutputLevel 0` will give a very verbose Minic using uci. 

Other available options (depending on compilation option, see definition.hpp) are mainly for development or debug purpose. They do not start the protocol loop. Here is an incompleted list :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -qsearch <"fen"> : just a qsearch ...
* -mateFinder <"fen"> depth : same as analysis but without prunings in search
* -pgn <file> : extraction tool to build tuning data
* -tuning <file> : run an evaluation tuning session (a.k.a. Texel tunings)
* -selfplay \[depth\] \[number of games\] (default are 15 and 1): launch some selfplay game with genfen activated
* ...

## Options

### GUI/protocol (Xboard or UCI)

Starting from release 1.00 Minic support setting options using protocol (both XBoard and UCI). Option priority are as follow : command line option can be override by protocol option. This way, Minic supports strength limitation, FRC, pondering, can use contempt, ...  

If compiled with the WITH_SEARCH_TUNING definition, Minic can expose all search algorithm parameters so that they can be tweaked. Also, when compiled with WITH_PIECE_TUNING, Minic can expose all middle- and end-game pieces values.

### Command line

Minic comes with some command line options :

#### Debug option
* -minOutputLevel \[from 0 or 8\] (default is 5 which means "classic GUI output"): make Minic more verbose for debug purpose (if set < 5). This option is often needed when using unsual things as evaluation tuning or command line analysis for instance in order to have full display of outputs. Here are the various level `logTrace = 0, logDebug = 1, logInfo = 2, logInfoPrio = 3, logWarn = 4, logGUI = 5, logError = 6, logFatal = 7, logOff = 8`
* -debugMode \[0 or 1\] (default is 0 which means "false"): will write every output also in a file (named minic.debug by default)
* -debugFile \[name_of_file\] (default is minic.debug): name of the debug output file

#### "Classic options"
* -ttSizeMb \[number_in_Mb\] (default is 128Mb, protocol option is "Hash"): force the size of the hash table. This is usefull for command line analysis mode for instance
* -threads \[number_of_threads\] (default is 1): force the number of threads used. This is usefull for command line analysis mode for instance
* -multiPV \[from 1 to 4 \] (default is 1): search more lines at the same time
* -syzygyPath \[path_to_egt_directory\] (default is none): specify the path to syzygy end-game table directory
* -FRC \[0 or 1\] (default is 0, protocol option is "UCI_Chess960"): activate Fisher random chess mode. This is usefull for command line analysis mode for instance

#### NNUE net

* -NNUEFile \[path_to_neural_network_file\] (default is none): specify the neural network (NNUE) to be used and activate NNUE evaluation
* -forceNNUE \[0 or 1\] (default is false): if a NNUEFile is loaded, forceNNUE equal true will results in a pure NNUE evaluation, while the default is hybrid evaluation

*Remark for Windows users* : it may be quite difficult to get the path format for the NNUE file ok under Windows. Here is a working example (thanks to Stefan Pohl) for cutechess-cli as a guide:

```cutechess-cli.exe -engine name="Minic3.06NoNa" cmd="C:/Engines/Minic/minic_3.06_mingw_x64_nehalem.exe" dir="C:/Engines/Minic" option.NNUEFile=C:/Engines/Minic/nocturnal_nadir.bin option.Hash=256 option.Threads=1 proto=uci```

Starting from Minic 3.07, no need to worry about this, the corresponding net is embeded inside the binary using the *INCBIN* technology.

#### Adjust strength
Minic strength can be ajdusted using the level option (from command line or using protocol option support, using value from 0 to 100). Level 0 is a random mover, 1 to 30 very weak, ..., level 100 is full strength. For now it uses multipv, maximum depth adjustment and randomness to make Minic play weaker moves.

Current level Elo are more or less so that even a beginner can beat low levels Minic. From level 50 or 60, you will start to struggle more! You can also use the UCI_Elo parameter if UCI_LimitStrenght is activated but the Elo fit is not good especially at low level. Level functionnaly will be enhanced in a near future.

Please also note that is *nodesBasedLevel* is activated, then no randomness is used to decrease Elo level of Minic, only number of nodes searched is changing.

* -level \[from 0 to 100\] (default is 100): change Minic skill level
* -limitStrength \[0 or 1\] (default is 0): take or not strength limitation into account
* -strength \[Elo_like_number\] (default is 1500): specify a Elo-like strength (not really well scaled for now ...)
* -nodesBasedLevel \[ 0 or 1 \] (default is 0): switch to node based only level (no randomness)
* -randomPly \[0 to 20 \] (default is 0): usefull when creating training data, play this number of total random ply at the begining of the game
* -randomOpen \[from 0 to 100 \] (default is 0): value in cp that allows to add randomness for ply <10. Usefull when no book is used

#### Training data generation

There are multiple ways of generating sfen data with Minic.
* First is classic play at fixed depth to generate pgn (I am using cutechess for this). Then use convert this way 
```
pgn-extract --fencomments -Wlalg --nochecks --nomovenumbers --noresults -w500000 -N -V -o data.plain games.pgn
```
Then use Minic -pgn2bin option to get a binary format sfen file. Note than position without score won't be taken into account.
* Use Minic random mover (level = 0) and play tons of random games activating the genFen option and setting the depth of search you like with genFenDepth. This will generate a "plain" format sfen file with game results always being 0, so you will use this with lambda=1 in your trainer to be sure to don't take game outcome into account. What you will get is some genfen_XXXXXX files (one for each Minic processus, note that with cutechess if only 2 engines are playing, only 2 process will run and been reused). Those files will be in workdir directory of the engine and are in "plain" format. So after that use Minic -plain2bin on that file to get a "binary" file. Minic will generate only quiet positions (at qsearch leaf).
* Use Minic "selfplay genfen" facility. In this case again, note that game result will always be draw because the file is written on the fly not at the end of the game. Again here you will obtain genfen_XXXXXX files. Minic will generate only quiet positions (at qsearch leaf).Data generation for learning process

* -genFen \[ 0 or 1 \] (default is 0): activate sfen generation
* -genFenDepth \[ 2 to 20\] (default is 8): specify depth of search for sfen generation
* -genFenDepthEG \[ 2 to 20\] (default is 12): specify depth of search in end-game for sfen generation

#### Other options

* -mateFinder \[0 or 1\] (default is 0): activate mate finder mode, which essentially means no forward pruning
* -fullXboardOutput \[0 or 1\] (default is 0): activate additionnal output for Xboard protocol such as nps or tthit
* -withWDL \[0 or 1\] (default is 0, protocol option is "WDL_display"): activate Win-Draw-Loss output. Converting score to WDL is done based on a fit perform on all recent Minic's game from main rating lists.
* -moveOverHead \[ 10 to 1000 \] (default is 50): time in milliseconds to keep as a security buffer
* -armageddon \[ 0 or 1 \] (default 0): play taking into account Armageddon mode (Black draw is a win)
* -UCI_Opponent \[ title rating type name \] (default empty): often a string like "none 3234 computer Opponent X.YY". Minic can take this information into account to adapt its contempt value to the opponent strength
* -UCI_RatingAdv \[ -10000 10000 \] (default not used): if received, Minic will use this value to adapt its contempt value to this rating advantage.

#### Style

Moreover, Minic implements some "style" parameters when using HCE (hand crafted evalaution in opposition to NNUE) that allow the user to boost or minimize effects of :
- material
- attack
- development
- mobility
- positional play
- forwardness
- complexity (not yet implemented)

Default values are 50 and a range from 0 to 100 can be used. A value of 100 will double the effect, while a value of 0 will disable the feature (it is probably not a good idea to put material awareness to 0 for instance ...).

## Videos

Minic on youtube, most often loosing to stronger engines ;-) :
  - https://www.youtube.com/watch?v=fPBtZ7VTBnQ
  - https://www.youtube.com/watch?v=juxxpN64Qcw
  - https://www.youtube.com/watch?v=jb3BifP8abA
  - https://www.youtube.com/watch?v=mrY4tTujC4g
  - https://www.youtube.com/watch?v=_6vbzpCTFyM
  - https://www.youtube.com/watch?v=EmWN79hHpZo
  - https://www.youtube.com/watch?v=yV1Z69oAOvA
  - https://www.youtube.com/watch?v=Ub3ug-TYJz0
  - https://www.youtube.com/watch?v=w1RtRFXlf9E
  - https://www.youtube.com/watch?v=wlzKxeHtKBo
  - https://www.youtube.com/watch?v=9p_jaqHA3QM
  - https://www.youtube.com/watch?v=yTZuNEV40X4
  
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
