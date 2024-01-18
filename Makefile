SHELL := /bin/bash

# this is for openbench
ifdef EXE
   export FORCEDNAME=$(EXE)
endif

ifdef EVALFILE
   export EMBEDDEDNNUEPATH=$(EVALFILE)
   export SKIPMD5CHECK=1
else
   export EMBEDDEDNNUENAME=nyctophobic_narwhal.bin
endif

.PHONY: config fathom build dist release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/build/*.sh
	mkdir -p Tourney
	if [ ! -e $(ROOT_DIR)/Tourney/nn.bin ]; then \
	   echo "No net, downloading..." \
	   && curl -skL https://github.com/tryingsomestuff/NNUE-Nets/raw/master/$${EMBEDDEDNNUENAME} >> Tourney/nn.bin; \
	fi
	NETMD5=$$(md5sum $(ROOT_DIR)/Tourney/nn.bin | awk '{print $$1}') \
	&& echo "Net md5: $${NETMD5}" \
	&& if [ "$${SKIPMD5CHECK}" != "1" ] && [ "$${NETMD5}" != "4fb1dfb4309fc452423fc472a7a42eb7" ]; then \
	   echo "Bad net (md5: $${NETMD5}), downloading..." \
	   && curl -skL https://github.com/tryingsomestuff/NNUE-Nets/raw/master/$${EMBEDDEDNNUENAME} >> Tourney/nn.bin; \
	fi

fathom: config
	git rev-parse --is-inside-work-tree > /dev/null 2>&1 \
	    && git submodule update --init -- Fathom > /dev/null 2>&1 || (rm -rf Fathom && git clone https://github.com/jdart1/Fathom.git)

release: config fathom
	$(ROOT_DIR)/Tools/build/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build/build.sh

android: config fathom
	$(ROOT_DIR)/Tools/build/buildAndroid.sh

dist: build
	mkdir -p Tourney
	cp Dist/Minic3/minic_dev_linux_x64 Tourney/

