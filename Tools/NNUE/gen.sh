#/bin/bash
dir=$(readlink -f $(dirname $0)/../../)

for i in $(seq 1 10000); do 
  $dir/Dist/Minic3/minic_dev_linux_x64 -selfplay 5 10000 -genFenDepth 8 -genFenDepthEG 12 -randomOpen 250 -FRC
done

