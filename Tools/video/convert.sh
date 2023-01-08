#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "$0 file.pgn out [music.mp3]  (don't use extension for out)"
    exit 1
fi

python3 convert.py $1 $2.gif
ffmpeg -i $2.gif -movflags faststart -pix_fmt yuv420p -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -filter:v fps=fps=5 $2_nosound.mp4

if [ "$#" -gt 2 ]; then
   ffmpeg -i $2_nosound.mp4 -i $3 -map 0:v -map 1:a -c:v copy -shortest $2.mp4
fi
