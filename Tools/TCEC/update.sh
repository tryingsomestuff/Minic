#!/bin/bash

if [ ! -n "$VERSION" ];then
   echo "You must first set VERSION variable please, see example below"
   echo "> export VERSION=master && ./update.sh"
   echo "> export VERSION=3.36 && ./update.sh"
   exit 1
fi

NETNAME=natural_naughtiness.bin

# get minic
git clone --branch=$VERSION https://github.com/tryingsomestuff/Minic
cd Minic

# get net
echo "No net, downloading..."
mkdir -p Tourney
wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/${NETNAME} -O Tourney/nn.bin --no-check-certificate

# get fathom
git clone https://github.com/jdart1/Fathom.git
read -p "Download done, press enter to continue"

# build
sed -i.bak 's/OPT="-s /OPT="-g /' ./Tools/build/build.sh
./Tools/build/build.sh minic $VERSION "-march=native"
cd Dist/Minic3/${VERSION}
ln -s minic_${VERSION}_linux_x64 minic_linux_x64

EXE=$PWD/minic_linux_x64
cd ../..

# bench
$EXE bench 20
