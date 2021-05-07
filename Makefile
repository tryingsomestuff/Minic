# this is for openbench
ifdef EXE
   export FORCEDNAME=$(EXE)
endif

.PHONY: config fathom build dist release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/build/*.sh
	mkdir -p Tourney
	if [ ! -e $(ROOT_DIR)/Tourney/nn.bin ]; then wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/nocturnal_nadir.bin -O Tourney/nn.bin ; fi

fathom:
	git rev-parse --is-inside-work-tree > /dev/null 2>&1 && git submodule update --init -- Fathom || (rm -rf Fathom && git clone https://github.com/jdart1/Fathom.git)

release: config fathom
	$(ROOT_DIR)/Tools/build/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build/build.sh

dist: build
	mkdir -p Tourney
	cp Dist/Minic3/minic_dev_linux_x64 Tourney/

