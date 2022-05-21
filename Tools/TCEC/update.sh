#!/bin/bash

if [ ! -n "$VERSION" ];then
   echo "You must first set VERSION variable please, see example below"
   echo "> export VERSION=master && ./update.sh"
   echo "> export VERSION=3.19 && ./update.sh"
   exit 1
fi

NETNAME=nylon_nonchalance
export EMBEDDEDNNUEPATH=$NETNAME

# get minic
git clone --branch=$VERSION https://github.com/tryingsomestuff/Minic
cd Minic

# get net
echo "No net, downloading..."
mkdir -p Tourney
wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/${NETNAME}_partaa -O Tourney/nn_aa.bin --no-check-certificate
wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/${NETNAME}_partab -O Tourney/nn_ab.bin --no-check-certificate
wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/${NETNAME}_partac -O Tourney/nn_ac.bin --no-check-certificate
wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/${NETNAME}_partad -O Tourney/nn_ad.bin --no-check-certificate
cat Tourney/nn_*.bin > Tourney/nn.bin

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
