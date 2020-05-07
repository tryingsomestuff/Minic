dir=$(readlink -f $(dirname $0)/../)
echo $dir/Tourney/minic_dev_linux_x64
ls $dir/Tourney/minic_dev_linux_x64
#xboard -zp -ics -icshost winboard.nl -icshelper timeseal -fcp "./minic_dev_linux_x64 -xboard" -fd "$dir/Tourney/" -autoKibitz -debug 
xboard -zp -ics -icshost winboard.nl -fcp "./minic_dev_linux_x64 -xboard" -fd "$dir/Tourney/" -autoKibitz -debug 

