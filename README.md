<p align="center">
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png" width="350"> 
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo_dark.png" width="350">
<br>
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo_dark.png" width="350">
<img src="https://raw.githubusercontent.com/tryingsomestuff/Minic/master/logo.png" width="350"> 
</p>

# Minic
Minic is a chess engine I'm developing to learn about chess programming and modern C++ (please see this lovely [wiki](https://www.chessprogramming.org/Main_Page) for more details on chess programming).  

Minic has no graphic interface (GUI) but is compatible with both [CECP](https://www.gnu.org/software/xboard/engine-intf.html) (xboard) and [UCI](http://wbec-ridderkerk.nl/html/UCIProtocol.html) protocol so you can use it in your favorite software (for instance [Cutechess](https://github.com/cutechess/cutechess), [Arena](http://www.playwitharena.de/), [Banksia](https://banksiagui.com/), [Winboard/Xboard](https://www.gnu.org/software/xboard/), [c-chess-cli](https://github.com/lucasart/c-chess-cli), ...).  
Minic is currently one of the 15 best engines in major [rating](https://ccrl.chessdom.com/ccrl/4040/) lists and the strongest french one.

Here are some shortcuts to navigate in this document :

   * [Support Minic development](#support-minic-development)
   * [History &amp; the NNUE Minic story](#history--the-nnue-minic-story)
   * [Minic NNUE "originality" status](#minic-nnue-originality-status)
   * [Testing and strength](#strength)
   * [Rating Lists &amp; competitions](#rating-lists--competitions)
   * [Release process](#release-process)
   * [How to compile](#how-to-compile)
   * [Syzygy EGT](#syzygy-egt)
   * [How to run](#how-to-run)
   * [Options](#options)
   * [Videos](#videos)
   * [Thanks](#thanks)

## Support Minic development

Generating data, learning process, tuning, optimization and testing of a chess engine is quite hardware intensive ! I have some good hardware at home but this is far from enough. This is why I opened a [Patreon](https://www.patreon.com/minicchess) account for Minic ; if you want to support Minic development, it is the place to be ;-)

## History & the NNUE Minic story

### Code style
For a year and a half Minic was (mainly) a one-file-code with very dense lines. This is of course very wrong in terms of software design... So why is it so? First reason is that Minic was first developped as a week-end project (in mid-october 2018), the quick-and-dirty way, and since then I was having fun going on the way of minimal lines of code direction ; being a "small" code size engine was part of the stakes in developing it the first few months. 

Until version 2 of Minic, some optional features such as evaluation and searchs tuning, perft, tests, uci support, book generation ... were available in the Add-Ons directory ; they are now fused with the source code for a while.

Nowadays, in fact since the release of version "2", the engine is written in a more classic C++ style, although some very dense lines may still be present and recall Minic past compacity...

More details about Minic history in the next paragraphs...

### Week-end project (October 2018)
Initially, the code size of Minic was supposed not to go above 2000sloc. It started as a week-end project in october 2018 (http://talkchess.com/forum3/viewtopic.php?f=2&t=68701). But, as soon as more features (especially SMP, material tables and bitboard) came up, I tried to keep it under 4000sloc and then 5000sloc, ... This is why this engine was named Minic, this stands for "Minimal Chess" (and is not related to the GM Dragoljub Minić) but it has not much to do with minimalism anymore nowadays... For the record, here is a link to the very first published version of Minic (https://github.com/tryingsomestuff/Minic/blob/dbb2fc7f026d5cacbd35bc379d8a1cdc1cad5674/minic.cc).

### Version "1" (October 2019)
Version "1" was published as a one year anniversary release in october 2019. At this point Minic has already gone from a 1800 Elo 2-days-of-work engine, to a 2800 Elo engine being invited at TCEC qualification league !

### Version "2" (April 2020)
Version "2" was released for April 1st 2020 (during covid-19 confinement in France). For this version, the one file Minic was splitted into many header and source files, and commented a lot more, without negative impact on speed and strength. 

#### NNUE from release 2.47 to release 2.53 (from Stockfish implementation)
Minic2, since release 2.47 of August 8th 2020 (http://talkchess.com/forum3/viewtopic.php?f=2&t=73521&hilit=minic2&start=50#p855313), has the possibility to be build using a shameless copy of the NNUE framework of Stockfish. Integration of NNUE was done easily and I hoped this can be done for any engines, especially if NNUE is released as a standalone library (see https://github.com/dshawul/nncpu-probe for instance). First tests shown that "MinicNNUE" is around 200Elo stronger than Minic, around the level of Xiphos or Ethereal at this date at short TC and maybe something like 50Elo higher at longer TC (so around Komodo11). 

When using a NNUE network with this Stockfish implementation, it is important that Minic is called "MinicNNUE".
Indeed, MinicNNUE (with copy/pasted SF NNUE implementation), won't be the official Minic, as this is not my own work at all.

Later on, starting from version 2.50, the NNUE learner from NodChip repository has also been ported to Minic so that networks can be built using Minic data and search can be done.
The genFen part of Nodchip was not ported and instead replaced by an internal process to produce training data. This included both extracting position from fixed depth game and from random positions.

Nets I built are still available at https://github.com/tryingsomestuff/NNUE-Nets.

### Version "3" (November 2020)
Version "3" of Minic is released in november 2020 (during second covid-19 confinement) as a 2 years anniversary release and *is not using, nor compatible with, SF NNUE implementation anymore*. More about this just below...

#### NNUE from release 3.00 (initially from Seer implementation)
Starting from release 3.00, **Minic is not using Stockfish NNUE implementation anymore and is no more compatible with SF nets**. It was too much foreign code inside Minic to be fair, to be maintained, to be fun.
Seer chess engine author is offering a very well written implementation of NNUE that I borrowed and adapt to Minic (https://github.com/connormcmonigle/seer-nnue). The code was more or less 400 lines. I choose to keep some Stockfish code just for binary sfens format conversion as everyone (or at least many many) is using this data format for now. Training code is an external tool written in Python without any dependency to engine, also first adapted from Seer repository and then taking ideas from Gary Linscott pytorch trainer (https://github.com/glinscott/nnue-pytorch). A new story was written in Minic 3 and the code has diverged quite a lot from the initial one.

Nets I built are available at https://github.com/tryingsomestuff/NNUE-Nets. Beware there is no retrocompatibility of the net from version to version. 

### Some original stuff that were maybe introduced in Minic

- use of PST score in move sorter to compensate near 0 history of quiet move
- aggregate history score for move sorter and history heuristic (cmh + history[piece and color][tosquare] + history[color][square][square])
- danger (from evaluation) based pruning and reductions in search
- emergency (from IID loop instabilities) based pruning and reductions in search
- history aware (boom / moob) based pruning and reductions in search
- use mobility data in search
- contempt opponent model taking opponent name or opponent rating into account
- "features" based evaluation parameter available to the user to tune game play (HCE evaluation only, not for NNUE)
- using a depth factor for pruning and reduction that takes TT entry depth into account
- training NNUE on DFRC data

### Minic NNUE "originality" status

- Inference code : originally based on Seer one (Connor McMonigle), many refactoring and experiements inside (clipped ReLU, quantization on read, vectorization, sub-net depending on pieces number, ...).
- Network topology : Many many have been tested (with or without skip connections, bigger or smaller input layer, number of layers, pieces buckets, ...) mainly without success. Always trying to find a better idea ... Currently a multi-bucket (based on the number of pieces) net with a common input layer and 2 inner nets.
- Training code : mainly based on the Gary Linscott and Tomasz Sobczyk (@Sopel) pytorch trainer (https://github.com/glinscott/nnue-pytorch), adapted and tuned to Minic.
- Data generation code : fully original, pure Minic data. Many ideas has been tried (generate inside search tree, self-play, multi-pv, random, with or without syzygy, ...). Some LC0 data are also used after being rescored using previous Minic version.
- Other tools : many little tools around training process, borrowed here and there and adapated or developed by myself.

In brief, Minic NNUE world is vastly inspired from what others are doing and is using pure Minic data (Minic generated or otherwise rescored).

## Testing and strength

Minic is currently in the 20 best engines with a Elo rating around 3400 at [CCRL scale](https://ccrl.chessdom.com/ccrl/404/cgi/compare_engines.cgi?class=Open+source+single-CPU+engines&only_best_in_class=on&num_best_in_class=1&print=Rating+list&profile_step=50&profile_numbers=1&print=Results+table&print=LOS+table&table_size=100&ct_from_elo=0&ct_to_elo=10000&match_length=30&cross_tables_for_best_versions_only=1&sort_tables=by+rating&diag=0&reference_list=None&recalibrate=no).

### Various NNUE nets strenght 

This table shows the evolution of nets strength of various Minic nets on an AVX2 hardware at short TC (10s+0.1). Results will be a lot different on older hardware where NNUE evaluation is much slower. I thus encourage users to only use Minic with NNUE nets on recent hardware. As we see, nets strenght is increasing version after version, this is due to better data, new net topologies but also of course to the fact that I rescore (or regenerate) data with previous version of Minic before training a new net. Moreover, the decision to revert SF implementation and start my own work based on Seer initial implementation led to a 2 years net training journey to fill the performance gap ...

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
Well ... for NNUE, using AVX2 is very important. This can explain some strange results during some testing process and in rating list where I sometimes see my nets underperforming a lot. So please, use AVX2 hardware (and the corresponding Minic binary, i.e. the "skylake" one for Intel or at least the "znver1" for AMD) for NNUE testing if possible.

### Threading performances

I'd love to own a big enough hardware to test with more than 8 threads ... Here are 3s+0.1 TC results to illustrate threading capabilities. Minic is thus scaling very well.
```
Rank Name                          Elo     +/-   Games   Score    Draw 
   1 minic_3.19_8                   123      34     156   67.0%   58.3% 
   2 minic_3.19_6                    94      36     156   63.1%   54.5% 
   3 minic_3.19_4                    20      33     156   52.9%   63.5% 
   4 minic_3.19_2                   -45      35     156   43.6%   59.0% 
   5 minic_3.19_1                  -206      41     156   23.4%   42.9% 
```

Moreover, speed test on CCC hardware, 2x AMD EPYC 7H12 (128 physical cores) : 
```
250 threads : 101,008,415 NPS
125 threads :  73,460,779 NPS
```
and speed test on a 2x Intel(R) Xeon(R) Platinum 8269CY CPU @ 2.50GHz (52 physical cores) :
```
104 threads :  46,291,408 NPS
52 threads  :  25,175,817 NPS
```
suggest Minic is reacting quite well to hyperthreading.

### Home testing 
Here are some fast TC results of a gauntlet tournament (STC 10s+0.1) for Minic 3.36.
```
Rank Name                          Elo     +/-   Games   Score    Draw 
   0 minic_3.36                     22       5    9852   53.1%   50.6% 
   1 seer                          103      11    1642   64.4%   57.9% 
   2 rofChade3                      52      11    1642   57.4%   55.8% 
   3 Uralochka3.39e-avx2           -47      11    1642   43.3%   57.3% 
   4 komodo-13.02                  -78      13    1642   39.0%   41.7% 
   5 Wasp600-linux-avx             -80      12    1642   38.7%   49.2% 
   6 stockfish.8                   -81      13    1642   38.6%   41.9% 
```

### Random mover
Minic random-mover (level = 0) stats are the following :
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
Here is 4 years of CCRL progress (single thread)
![image](https://user-images.githubusercontent.com/5878710/172205840-b4bce912-7497-4bfd-9a82-ab08d19265b6.png)


### CCRL
- 40/15: Minic 3.31 + Natural Naughtiness is tested at 3447 on the [CCRL 40/15 scale, 4 cores](http://ccrl.chessdom.com/ccrl/4040/)  
- Blitz: Minic 3.31 + Natural Naughtiness is tested at 3609 on the [CCRL BLITZ scale, 8 cores](http://ccrl.chessdom.com/ccrl/404/)  
- FRC: Minic 3.32 + Natural Naughtiness is tested at 3591 on the [CCRL FRC list](http://ccrl.chessdom.com/ccrl/404FRC/)  

### CEGT
- 40/4: Minic 3.27 + Natural Naughtiness is tested at 3409 on the [CEGT 40/4 list](http://www.cegt.net/40_4_Ratinglist/40_4_single/rangliste.html)  
- 40/20: Minic Minic 3.32 + Natural Naughtiness is tested at 3388 on the [CEGT 40/20 list](http://www.cegt.net/40_40%20Rating%20List/40_40%20SingleVersion/rangliste.html)  
- 5+3 pb=on: 3.27 + Natural Naughtiness is tested at 3445 on the [CEGT 5+3 PB=ON list](http://www.cegt.net/5Plus3Rating/BestVersionsNEW/rangliste.html)  
- 25+8: Minic 3.22 + Nylon Nonchalance is tested at 3430 on the [CEGT 25+8 list](http://www.cegt.net/25plus8Rating/BestVersions/rangliste.html)  

### FGRL
- Minic 3.30 + Natural Naughtiness is tested at 3285 on the [fastgm 60sec+0.6sec rating list](http://www.fastgm.de/60-0.60.html)  
- Minic 3.30 + Natural Naughtiness is tested at 3347 on the [fastgm 10min+6sec rating list](http://www.fastgm.de/10min.html)  
- Minic 3.24 + Nylon Nonchalance is tested at 3303 on the [fastgm 60min+15sec rating list](http://www.fastgm.de/60min.html)  
- Minic 3.18 + Nimble Nothingness is tested at 3276 on the [fastgm 60sec+0.6sec 16 cores rating list](http://www.fastgm.de/16-60-0.6.html)

### SP-CC
- Minic 3.31 + Natural Naughtiness is tested at 3528 on the [SP-CC 3min+1s rating list](https://www.sp-cc.de/)  

### GRL
- Minic 3.17 + Nucleated Neurulation is tested at 3229 on the [GRL 40/2 rating list](http://rebel13.nl/grl-best-40-2.html)
- Minic 3.06 using Nocturnal Nadir net is tested at 3078 on the [GRL 40/15 rating list](http://rebel13.nl/grl-best-40-15.html)

### BRUCE
- Minic 3.18 + Nimble Nothingness is tested at 3305 on the [BRUCE rating list](https://e4e6.com/)

### Ipman Chess
- Minic 3.27 + Natural Naughtiness is tested at 3347 on the [IpmanChess rating list](http://ipmanchess.yolasite.com/i7-11800h.php)

### Test Suite (quite old results here)
- STS : 1191/1500 @10sec per position (single thread on an i7-9700K)  
- WAC : 291/300 @10sec per position (single thread on an i7-9700K)  

### TCEC stats

TCEC hardware: Minic is at 3440 (https://tcec-chess.com/bayeselo.txt)

### TCEC

Here are Minic results at TCEC main event (https://tcec-chess.com/)

TCEC15: 8th/10 in Division 4a (https://www.chessprogramming.org/TCEC_Season_15)    
TCEC16: 13th/18 in Qualification League (https://www.chessprogramming.org/TCEC_Season_16)   
TCEC17: 7th/16 in Q League, 13th/16 in League 2 (https://www.chessprogramming.org/TCEC_Season_17)  
TCEC18: 4th/10 in League 3 (https://www.chessprogramming.org/TCEC_Season_18)  
TCEC19: 3rd/10 in League 3 (https://www.chessprogramming.org/TCEC_Season_19)  
TCEC20: 2nd/10 in League 3, 9th/10 in League 2 (https://www.chessprogramming.org/TCEC_Season_20)  
TCEC21: 1st/12 in League 3, 6th/10 in League 2 (https://www.chessprogramming.org/TCEC_Season_21)  
TCEC22: 2nd/8 in League 2, 8th/8 in League 1 (well tried ;) )  (https://www.chessprogramming.org/TCEC_Season_22)  
TCEC23: 3rd/12 in League 1 ! (https://tcec-chess.com/#div=l1&game=1&season=23)  
TCEC24: 9rd/12 in League 1 ! (https://tcec-chess.com/#div=l1&game=1&season=24)  
TCEC25: 8rd/12 in League 1 ! (https://tcec-chess.com/#div=l1&game=1&season=25)

## Release process
WARNING : the former Dist directory as been REMOVED from the repository because it was starting to be too big. Unofficial releases are not available anymore here. All (including unofficial) releases are available in a new repo, here : https://github.com/tryingsomestuff/Minic-Dist, also available as a git submodule.

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

Please note that for Linux binaries to work you will need a recent libc installed on your system.  
Please note that Win32 binaries are very slow so please use Win64 one if possible.  
Please note that Minic has always been a little weaker under Windows OS (probably due to cross-compilation lacking PGO).
     
## How to compile
* Linux (g++>=11 or clang++>=18 requiered, Minic is using C++20): just type "make" (defining CC and CXX is possible), or use the given build script Tools/tools/build.sh (or make your own ...), or try to have a look at Tools/TCEC/update.sh for some hints. The executable will be available in Dist/Minic3 subdirectory.
* Windows : use the Linux cross-compilation script given or make your own. From time to time I also check that recent VisualStudio versions can compile Minic without warnings but I don't distribute any VS project.
* Android/RPi/... (experimental...) : use the given cross-compilation script or make your own.

A minimal working example on Linux for dev version would be
```
git clone https://github.com/tryingsomestuff/Minic.git
cd Minic
git submodule update --init Fathom
make
# done exe in Dist/Minic3
```

## Syzygy EGT
To compile with SYZYGY support you'll need to clone https://github.com/jdart1/Fathom as Fathom directory and activate WITH_SYZYGY definition at compile time (this is default behaviour).
This can be done using the given git submodule or by hand. To use EGT just specify syzygyPath in the command line or using the GUI option.

## How to run
add the command line option "-xboard" to go to xboard/winboard mode or -uci for UCI. If not option is given Minic will **default to use UCI protocol**.
Please note that if you want to force specific option from command line instead of using protocol option, you **have** to first specific protocol as the first command line argument. For instance `minic -uci -minOutputLevel 0` will give a very verbose Minic using uci. 

Other available options (depending on compilation option, see config.hpp) are mainly for development or debug purpose. They do not start the protocol loop. Here is an incompleted list :
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

Starting from Minic 3.07, no need to worry about this, the official corresponding net is embeded inside the binary using the *INCBIN* technology.

#### Adjust strength
Minic strength can be ajdusted using the level option (from command line or using protocol option support, using value from 0 to 100). Level 0 is a random mover, 1 to 30 very weak, ..., level 100 is full strength. For now it uses multipv, maximum depth adjustment and randomness to make Minic play weaker moves (also a fixed node option is available to avoid randomness if needed).

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
  - https://www.youtube.com/watch?v=Ub3ug-TYJz0
  - https://www.youtube.com/watch?v=w1RtRFXlf9E
  - https://www.youtube.com/watch?v=wlzKxeHtKBo
  - https://www.youtube.com/watch?v=04A9Qb6D-Xs
  - https://www.youtube.com/watch?v=9qvs7paQRtw
  - https://www.youtube.com/watch?v=JQdSmXhcpA4
  - https://www.youtube.com/watch?v=HvuFOM_wX2k
  - https://www.youtube.com/watch?v=RXui0aH-Mxo
  - https://www.youtube.com/watch?v=umgEDThmVxY
  - https://www.youtube.com/watch?v=xmmakWtLdIU
  - https://www.youtube.com/watch?v=pV4AJRlsxkc
  - https://www.youtube.com/watch?v=CntLrEGIuBI
  - https://www.youtube.com/watch?v=gQraJtAy5bM
  - https://www.youtube.com/watch?v=Lb2mWB3nBB4
  - https://www.youtube.com/shorts/l35q9S7Xstg
  - https://www.youtube.com/watch?v=ZUbMOMX0pp8
  - https://www.youtube.com/watch?v=_VT_0DszLz0
  - https://www.youtube.com/watch?v=in2snklUbyI
  - https://www.youtube.com/watch?v=v5ab9dZrqTw
  - https://www.youtube.com/watch?v=eLkaPFogTXg
  - https://www.youtube.com/shorts/l35q9S7Xstg
  - https://www.youtube.com/watch?v=cl1xaTnjwJw
  - https://www.youtube.com/watch?v=in2snklUbyI
  - https://www.youtube.com/watch?v=fwdBVOa3-QA
  - https://www.youtube.com/watch?v=Keb18-nF1Vk
  - https://www.youtube.com/watch?v=hsFOpD9tL7A
  
  
GM Matthew Sadler ([Silicon Road youtube channel](https://www.youtube.com/c/SiliconRoadChess)) game analysis
  - https://www.youtube.com/watch?v=9p_jaqHA3QM
  - https://www.youtube.com/watch?v=yTZuNEV40X4
  - https://www.youtube.com/watch?v=3ttQaGKMAy4
  - https://www.youtube.com/watch?v=lGYqW32iMD8
  - https://www.youtube.com/watch?v=QT9yz1x0_84
  - https://www.youtube.com/watch?v=-GxZRu0GHnQ
  
## Thanks

Of course many/most idea in Minic are taken from the beautifull chess developer community.
Here's a very incomplete list of open-source engines that were inspiring for me:

Arasan by Jon Dart  
Berserk by Jay Honnold  
CPW by Pawel Koziol and Edmund Moshammer  
Deepov by Romain Goussault  
Defenchess by Can Cetin and Dogac Eldenk  
Demolito by Lucas Braesch  
Dorpsgek by Dan Ravensloft  
Ethereal by Andrew Grant  
Galjoen by Werner Taelemans  
Koivisto by Kim Kåhre and Finn Eggers  
Madchess by Erik Madsen  
Rodent by Pawel Koziol  
RubiChess by Andreas Matthies  
Seer by Connor McMonigle  
Stockfish by the stockfish contributors (https://github.com/official-stockfish/Stockfish/blob/master/AUTHORS)  
Texel by Peter Österlund   
Topple by Vincent Tang  
TSCP by Tom Kerrigan  
Vajolet by Marco Belli  
Vice by BlueFeverSoft  
Weiss by Terje Kirstihagen  
Winter by Jonathan Rosenthal  
Xiphos by Milos Tatarevic  
Zurichess by Alexandru Moșoi   

Many thanks also to all testers for all those long time control tests, they really are valuable inputs in the chess engine development process. 

Also thanks to [TCEC](https://tcec-chess.com/) and [CCC](https://www.chess.com/computer-chess-championship) for letting Minic participate to many event, it is fun to see Minic on such a great hardware.

Thanks to Karlson Pfannschmidt for the bayesian optimization [chess-tuning-tools](https://github.com/kiudee/chess-tuning-tools) 
  
And of course thanks to all the members of the talkchess forum and CPW, and to H.G. Muller and Joost Buijs for hosting the well-known friendly monthly tourney.

## Info
- Am I a chess player ?
- yes
- good one ?
- no (https://lichess.org/@/xr_a_y)
