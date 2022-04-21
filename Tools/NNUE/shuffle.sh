#!/bin/bash

input_file=$1
output_file=${1}_shuffled
file_size=$(stat --printf="%s" ${input_file})
chunk_size=$((40*1024))
number_of_chunks=$(((file_size/chunk_size)-1))
i=0
runtime=1
speed="?"
for j in $(shuf --input-range=0-$number_of_chunks); do
  mod=$((i%20))
  if [ $mod == 0 ]; then
     start=`date +%s`
  fi
  dd bs=$chunk_size if=${input_file} of=${output_file} count=1 skip=$j seek=$i status=none conv=notrunc
  if [ $mod == 19 ]; then
     end=`date +%s`
     runtime=$((end-start))
     if [ $runtime == 0 ]; then 
        runtime=1
     fi
     speed=$(((20*chunk_size)/(runtime*1024*1024)))
  fi
  echo "$i->$j : $speed Mb/s"
  i=$((i + 1))
done
