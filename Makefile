.PHONY: build release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/*.sh

fathom:
	git submodule update --init -- Fathom

release: config fathom
	$(ROOT_DIR)/Tools/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build.sh

dist: build
	mkdir -p Tourney
	cp Dist/Minic3/minic_dev_linux_x64 Tourney/

