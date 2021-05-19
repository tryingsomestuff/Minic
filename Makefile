SHELL := /bin/bash

# this is for openbench
ifdef EXE
   export FORCEDNAME=$(EXE)
endif

ifdef EVALFILE
   export EMBEDEDNNUEPATH=$(EVALFILE)
endif

.PHONY: config fathom build dist release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/build/*.sh
	mkdir -p Tourney
	if [ ! -e $(ROOT_DIR)/Tourney/nn.bin ]; then \
	   echo "No net, downloading..." \
	   && wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/naive_nostalgia.bin -O Tourney/nn.bin ;\
	fi
	NETMD5=$$(md5sum $(ROOT_DIR)/Tourney/nn.bin | awk '{print $$1}') \
	&& echo "Net md5: $${NETMD5}" \
	&& if [ "$${NETMD5}" != "196efe65d3c89d6ea1ce5522633467d7" ]; then \
	   echo "Bad net (md5: $${NETMD5}), downloading..." \
	   && wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/naive_nostalgia.bin -O Tourney/nn.bin ;\
	fi

fathom:
	git rev-parse --is-inside-work-tree > /dev/null 2>&1 \
	    && git submodule update --init -- Fathom || (rm -rf Fathom && git clone https://github.com/jdart1/Fathom.git)

release: config fathom
	$(ROOT_DIR)/Tools/build/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build/build.sh

dist: build
	mkdir -p Tourney
	cp Dist/Minic3/minic_dev_linux_x64 Tourney/

