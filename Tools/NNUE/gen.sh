#/bin/bash
dir=$(readlink -f $(dirname $0)/../../)

for i in $(seq 1 10000); do 
  $dir/Dist/Minic3/minic_dev_linux_x64 -selfplay 8 10000 -genFenDepth 8 -genFenDepthEG 12 -randomOpen 350 -FRC 1
done

