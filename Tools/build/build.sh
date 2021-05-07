#!/bin/bash
ulimit -s unlimited

if grep "^#define WITH_MPI" Source/definition.hpp ; then
   echo "Build with mpi"
   if (mpirun --version | grep Intel); then
      export CXX=mpicxx
      export CC=mpicc
      which mpirun
   else
      echo "Please use IntelMPI, only validated MPI distribution with Minic"
      exit 1
   fi
else
   export CXX=g++
   export CC=gcc
fi

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "found Fathom lib, trying to build"
   $(dirname $0)/buildFathom.sh "$@"
fi

do_title "Building Minic for Linux"

if [ -n "$FORCEDNAME" ]; then
   exe=$FORCEDNAME
else
   exe=${e}_${v}_linux_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      exe=${exe}_${tname}
      exe=${buildDir}/${exe}
   fi
fi
echo "Building $exe"

WARN="-Wall -Wcast-qual -Wno-char-subscripts -Wno-reorder -Wmaybe-uninitialized -Wuninitialized -pedantic -Wextra -Wshadow -Wno-unknown-pragmas"
#-fopt-info"

OPT="-s -DNDEBUG -O3 $n" ; DEPTH=16
#OPT="-s -ffunction-sections -fdata-sections -Os -s -DNDEBUG -Wl,--gc-sections" ; DEPTH=16
#OPT="-DNDEBUG -O3 -g -ggdb -fno-omit-frame-pointer" ; DEPTH=10
#OPT="-DNDEBUG -g" ; DEPTH=10
#OPT="-g -rdynamic" ; DEPTH=10

LIBS="-lpthread -ldl" 
# -lopenblas"

OPT="$WARN $d $OPT $t --std=c++17 -fno-exceptions"
OPT="$OPT -ffp-contract=off" # to ensure reproductible result in AVX2 (FMA)

# -flto is making g++ 9.3.0 under cygwin segfault
uname -a | grep CYGWIN
if [ $? == 1 ]; then
   OPT="$OPT -flto"
fi

if [ $FATHOM_PRESENT = "1" ]; then
   lib=fathom_${v}_linux_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      lib=${lib}_${tname}
   fi
   lib=${lib}.o
   OPT="$OPT $dir/Fathom/src/$lib -I$dir/Fathom/src"
fi

echo $OPT $LIBS

rm -f *.gcda

STANDARDSOURCE="Source/*.cpp Source/nnue/learn/*.cpp"

$CXX -fprofile-generate $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $exe $LIBS 
ret=$?
echo "end of first compilation"
if [ $ret = "0" ]; then
   echo "running Minic for profiling : $exe"
   $exe bench $DEPTH -quiet 0 -NNUEFile Tourney/nn.bin -NNUEFileEG Tourney/nn.bin
   $exe bench $DEPTH -quiet 0 
   echo "starting optimized compilation"
   $CXX -fprofile-use $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $exe $LIBS
   echo "done "
else
   echo "some error"
fi

rm -f *.gcda
