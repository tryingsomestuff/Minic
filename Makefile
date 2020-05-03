.PHONY: build release

.DEFAULT_GOAL:= build 

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

config:
	chmod +x $(ROOT_DIR)/Tools/*.sh

fathom:
	rm -rf $(ROOT_DIR)/Fathom
	git clone https://github.com/jdart1/Fathom $(ROOT_DIR)/Fathom

release: config fathom
	$(ROOT_DIR)/Tools/release.sh

build: config fathom
	$(ROOT_DIR)/Tools/build.sh

dist: config fathom build
	cp Dist/Minic2/minic_dev_linux_x64 Tourney/

