#!/bin/bash

dir=$(readlink -f $(dirname $0)/)

CUTECHESS=cutechess-cli
engines=""

engines="$engines -engine conf=minic_dev"
engines="$engines -engine conf=minic_devmpi2"

echo $engines

$CUTECHESS $engines -each proto=xboard tc=0/10+0.1 timemargin=50 -rounds 999999 -pgnout pgn_$(date +"%Y_%m_%d"_%H%M%S).out -ratinginterval 10 -repeat -openings file=$dir/Book_and_Test/OpeningBook/Hert500.pgn format=pgn order=random plies=100 -concurrency 1 -debug > tournament_self.log 2>&1 &


