#!/bin/bash

NETNAME=noctural_nadir.bin

# get minic
git clone --branch=$VERSION https://github.com/tryingsomestuff/Minic
cd Minic

# get net
wget https://raw.githubusercontent.com/tryingsomestuff/NNUE-Nets/master/$NETNAME
mkdir -p Tourney
cp $NETNAME Tourney/nn.bin

# get fathom
git clone https://github.com/jdart1/Fathom.git
read -p "Download done, press enter to continue"

# build
sed -i.bak 's/OPT="-s /OPT="-g /' ./Tools/build/build.sh
./Tools/build/build.sh $VERSION "-mavx2 -mbmi2"
cd Dist/Minic3
ln -s minic_${VERSION}_linux_x64_avx2_bmi2 minic_linux_x64_avx2_bmi2

EXE=$PWD/minic_linux_x64_avx2_bmi2
cd ../..
NET=$PWD/$NETNAME

# bench
$EXE bench 20 -NNUEFile $NET
