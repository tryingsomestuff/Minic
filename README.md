# Minic
A simple chess engine to learn and play with.
Code size shall not go above 2000sloc.
Minic stands for "Minimal Chess".
Minic is smaller than tscp but stronger.

## Strenght
Quite poor for a chess engine but already way to strong for me !

```
Rank Name                          Elo     +/-   Games   Score   Draws
   1 minic                         148      48     129   70.2%   39.5%
   2 fairymax                       41      51     128   55.9%   28.9%
   3 tscp                         -200      61     129   24.0%   20.2%
```

## How to compile:
* Linux : use the given build script (or make your own ...)
* Windows : use the given VS2017 project (or make your own ...)

## How to run :
add the command line option "-xboard" to go to xboard/winboard mode.

Other available options are :
* -perft_test : run the inner perft test
* -eval <"fen"> : static evaluation of the given position
* -gen <"fen"> : move generation on the given position
* -perft <"fen"> depth : perft on the given position and depth
* -analyze <"fen"> depth : analysis on the given position and depth
* -mateFinder <"fen"> depth : same as analysis but without prunings in search.

