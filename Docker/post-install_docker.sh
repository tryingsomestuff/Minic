#!/bin/bash

apt-get update
apt-get upgrade -y
DEBIAN_FRONTEND=noninteractive apt-get install -y git

cd /home/debian
git clone https://github.com/tryingsomestuff/Minic.git
cd Minic/Docker
docker build . -t openbench
docker run --rm --name openbench -d openbench USER PASSWORD 2 vm_$(hostname)

