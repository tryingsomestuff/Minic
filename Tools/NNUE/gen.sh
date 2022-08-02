#/bin/bash
dir=$(readlink -f $(dirname $0)/../../)

for i in $(seq 1 10000); do
  $dir/Dist/Minic3/minic_dev_linux_x64 -selfplay 5 10000 -genFenDepth 32 -genFenDepthEG 32 -maxNodes 5000 -forceNNUE 1 -randomOpen 350 -FRC 1 -DFRC 1 -genFen 1 -pgnOut 0 
done
