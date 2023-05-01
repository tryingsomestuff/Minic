#/bin/bash
dir=$(readlink -f $(dirname $0)/../../)

for i in $(seq 1 10000); do
  $dir/Dist/Minic3/minic_dev_linux_x64 -selfplay 50 10000 -genFenDepth 32 -genFenDepthEG 32 -maxNodes 10000 -forceNNUE 1 -randomOpen 150 -FRC 1 -DFRC 1 -genFen 1 -pgnOut 0 -minOutputLevel 3 
done
