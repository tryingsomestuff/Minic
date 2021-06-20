dir=$(readlink -f $(dirname $0)/../../)
echo $dir/Tourney/minic_dev_linux_x64
ls $dir/Tourney/minic_dev_linux_x64
xboard -zp -ics -icshost nightmare-chess.nl -fcp "./minic_dev_linux_x64 -xboard -moveOverHead 3000 -ttSizeMb 4096 -threads 8 -syzygyPath /ssd/Minic/TB/tablebase.sesse.net/syzygy/3-4-5 -fullXboardOutput 1" -fd "$dir/Tourney/" -autoKibitz -debug
