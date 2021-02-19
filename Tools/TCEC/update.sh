#!/bin/bash
git clone --branch=$VERSION https://github.com/tryingsomestuff/Minic
cd Minic
wget https://raw.githubusercontent.com/tryingsomestuff/NNUE-Nets/master/noisy_notch.bin
mkdir -p Tourney
cp noisy_notch.bin Tourney/nn.bin
git clone https://github.com/jdart1/Fathom.git
read -p "Download done, press enter to continue"
sed -i.bak 's/OPT="-s /OPT="-g /' ./Tools/build.sh
./Tools/build.sh $VERSION "-mavx2 -mbmi2"
cd Dist/Minic3
ln -s minic_${VERSION}_linux_x64_avx2_bmi2 minic_linux_x64_avx2_bmi2
EXE=$PWD/minic_linux_x64_avx2_bmi2

