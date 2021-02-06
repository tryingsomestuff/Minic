.PHONY: build release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/*.sh

fathom:
	git rev-parse --is-inside-work-tree > /dev/null && git submodule update --init -- Fathom || git clone https://github.com/jdart1/Fathom.git

release: config fathom
	$(ROOT_DIR)/Tools/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build.sh

dist: build
	mkdir -p Tourney
	cp Dist/Minic3/minic_dev_linux_x64 Tourney/

