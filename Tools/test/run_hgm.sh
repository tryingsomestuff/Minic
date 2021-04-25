dir=$(readlink -f $(dirname $0)/../../)
echo $dir/Tourney/minic_dev_linux_x64
ls $dir/Tourney/minic_dev_linux_x64
#xboard -zp -ics -icshost winboard.nl -icshelper timeseal -fcp "./minic_dev_linux_x64 -xboard" -fd "$dir/Tourney/" -autoKibitz -debug 
#xboard -zp -ics -icshost winboard.nl -fcp "./minic_dev_linux_x64 -xboard" -fd "$dir/Tourney/" -autoKibitz -debug 
xboard -zp -ics -icshost nightmare-chess.nl -fcp "./minic_dev_linux_x64 -xboard -moveOverHead 3000 -ttSizeMb 4096 -threads 8 -NNUEFile /ssd/NNUE-Nets/nettling_nemesis.bin -syzygyPath /ssd/Minic/TB/tablebase.sesse.net/syzygy/3-4-5 -fullXboardOutput 1" -fd "$dir/Tourney/" -autoKibitz -debug
#xboard -zp -ics -icshost nightmare-chess.nl -fcp "./minic_2.36_linux_x64_skylake -xboard -moveOverHead 3000 -ttSizeMb 6400 -threads 8" -fd "$dir/Dist/Minic3/" -autoKibitz -debug
