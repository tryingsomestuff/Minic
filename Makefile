.PHONY: build

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

build:
	chmod +x $(ROOT_DIR)/Tools/*.sh
	git clone https://github.com/jdart1/Fathom $(ROOT_DIR)/Fathom
	$(ROOT_DIR)/Tools/build.sh

prerequisites: build

target: prerequisites 
