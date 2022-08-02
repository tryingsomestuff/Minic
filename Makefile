SHELL := /bin/bash

# this is for openbench
ifdef EXE
   export FORCEDNAME=$(EXE)
endif

ifdef EVALFILE
   export EMBEDDEDNNUEPATH=$(EVALFILE)
   export SKIPMD5CHECK=1
else
   export EMBEDDEDNNUENAME=natural_naughtiness.bin
endif

.PHONY: config fathom build dist release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/build/*.sh
	mkdir -p Tourney
	if [ ! -e $(ROOT_DIR)/Tourney/nn.bin ]; then \
	   echo "No net, downloading..." \
	   && wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/$${EMBEDDEDNNUENAME} -O Tourney/nn.bin --no-check-certificate; \
	fi
	NETMD5=$$(md5sum $(ROOT_DIR)/Tourney/nn.bin | awk '{print $$1}') \
	&& echo "Net md5: $${NETMD5}" \
	&& if [ "$${SKIPMD5CHECK}" != "1" ] && [ "$${NETMD5}" != "a0f8be841e66a586b0afa0fd1a636378" ]; then \
	   echo "Bad net (md5: $${NETMD5}), downloading..." \
	   && wget https://github.com/tryingsomestuff/NNUE-Nets/raw/master/$${EMBEDDEDNNUENAME} -O Tourney/nn.bin --no-check-certificate; \
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

