#/bin/bash
v=$(cat minic.cc | grep "MinicVersion =" | awk '{print $NF}' | sed 's/;//' | sed 's/"//g')
echo "Releasing version $v"
./build.sh $v
./buildGW.sh $v
