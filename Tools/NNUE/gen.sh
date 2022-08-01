#/bin/bash
dir=$(readlink -f $(dirname $0)/../../)

for i in $(seq 1 10); do 
  $dir/Dist/Minic3/minic_dev_linux_x64 -selfplay 5 10 -genFenDepth 7 -genFenDepthEG 9 -maxNodes 15000 -forceNNUE 1 -randomOpen 350 -FRC 1 -DFRC 1 -genFen 1 -pgnOut 0
done

