#!/bin/bash
ulimit -s unlimited

if grep "^#define WITH_MPI" Source/config.hpp ; then
   echo "Build with mpi"
   if (mpirun --version | grep Open); then
      export CXX=mpicxx
      export CC=mpicc
      #export NOPROFILE=1
      which mpirun
   else
      echo "Please use OpenMPI, only validated MPI distribution with Minic"
      exit 1
   fi
else
   if [ -z $CXX ]; then
      export CXX=g++
      export CC=gcc
   fi
fi

source $(dirname $0)/common
cd_root

FATHOM_PRESENT=0
if [ -e Fathom/src/tbprobe.h ]; then
   FATHOM_PRESENT=1
   echo "Found Fathom lib, trying to build"
   $(dirname $0)/buildFathom.sh "$@"
fi

do_title "Building Minic for Linux"

if [ -n "$FORCEDNAME" ]; then
   exe=$FORCEDNAME
   exe=${dir}/${exe}
else
   exe=${e}_${v}_linux_x64
   if [ "$t" != "-march=native" ]; then
      tname=$(echo $t | sed 's/-m//g' | sed 's/arch=//g' | sed 's/ /_/g')
      exe=${exe}_${tname}
   fi
   exe=${buildDir}/${exe}
fi

echo "Building $exe"
do_sep

WARN="-Wall -Wcast-qual -Wno-char-subscripts -Wno-reorder -Wmaybe-uninitialized -Wuninitialized -pedantic -Wextra -Wshadow -Wno-unknown-pragmas -Wno-unknown-warning-option -Wno-missing-braces -Wno-constant-logical-operand"
if [[ $CXX == *"clang"* ]]; then
   echo "Using clang ... will be slowwwww"
   WARN="$WARN -fconstexpr-steps=1000000000"
   PROF_GEN="-fprofile-generate=${buildDir}/"
   PROF_MERGE="llvm-profdata-18 merge -output=${buildDir}/code.profdata ${buildDir}/*.profraw"
   PROF_USE="-fprofile-use=${buildDir}/code.profdata"
else
   WARN="$WARN -fconstexpr-loop-limit=1000000000"
   PROF_GEN="-fprofile-generate"
   PROF_MERGE=""
   PROF_USE="-fprofile-use"
fi
#-fopt-info
#-Wconversion

if [ ! -n "$DEBUGMINIC" ]; then
  echo "******* RELEASE BUILD *******"
  OPT="-DNDEBUG -fno-math-errno -O3 $n"
  #-ffast-math ?
  if [ -n "$VTUNEMINIC" ]; then
     echo "***** with VTUNE params *****"
     # for VTUNE and other analyzer
     OPT="$OPT -g" ; DEPTH=20
  else
     OPT="$OPT -s" ; DEPTH=20
  fi
else
  echo "******* DEBUG BUILD *******"
  #OPT="-s -ffunction-sections -fdata-sections -Os -s -DNDEBUG -Wl,--gc-sections" ; DEPTH=16
  OPT="-DNDEBUG -O3 -g -ggdb -fno-omit-frame-pointer" ; DEPTH=10
  #OPT="-DNDEBUG -g" ; DEPTH=10
  #OPT="-g -rdynamic" ; DEPTH=10
fi

LIBS="-lpthread -ldl"
# -lopenblas"

if grep "^#define WITH_FMTLIB" Source/config.hpp ; then
   LIBS="$LIBS -lfmt"
fi

OPT="$WARN $d $OPT $t $STDVERSION -fno-exceptions"
if [ -n "$FORCEDNAME" ]; then
   OPT="$OPT -ffp-contract=off" # to ensure reproductible result in AVX2 (FMA) for OpenBench
fi

# -flto is making g++ 9.3.0 under cygwin segfault
uname -a | grep CYGWIN
if [ $? == 1 ]; then
   OPT="$OPT -flto=auto"
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

#echo $OPT $LIBS

rm -f *.gcda ${buildDir}/*.gcda *.profraw ${buildDir}/*.profraw *.profdata ${buildDir}/*.profdata

if [ -n "$NOPROFILE" ]; then
   echo "compilation without profiling"
   CMD="$CXX $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $exe $LIBS"
   echo "Build command : $CMD"
   $CMD
else
   CMD="$CXX $PROF_GEN $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $exe $LIBS"
   echo "Build command : $CMD"
   $CMD
   ret=$?
   echo "end of first compilation"
   if [ $ret = "0" ]; then
      echo "running Minic for profiling : $exe"
      $exe bench $DEPTH -minOutputLevel 0
      $PROF_MERGE
      #$exe bench $DEPTH -minOutputLevel 0 -NNUEFile none # force no NNUE
      echo "starting optimized compilation"
      CMD="$CXX $PROF_USE $OPT $STANDARDSOURCE -ISource -ISource/nnue -o $exe $LIBS"
      echo "Build command : $CMD"
      $CMD
      do_title "Done "
   else
      do_title "Some error"
   fi
fi

rm -f *.gcda ${buildDir}/*.gcda *.profraw ${buildDir}/*.profraw *.profdata ${buildDir}/*.profdata
