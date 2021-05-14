#!/bin/bash

if [ ! -n "$VERSION" ];then
   echo "You must first set VERSION variable please, see example below"
   echo "> export VERSION=master && ./update.sh"
   echo "> export VERSION=3.07 && ./update.sh"
   exit 1
fi

NETNAME=naive_nostalgia.bin
export EMBEDEDNNUEPATH=$NETNAME

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
./Tools/build/build.sh minic $VERSION "-march=native"
cd Dist/Minic3
ln -s minic_${VERSION}_linux_x64 minic_linux_x64

EXE=$PWD/minic_linux_x64
cd ../..

# bench
$EXE bench 20
