.PHONY: build

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

build: 
	$(ROOT_DIR)/Tools/build.sh

prerequisites: build

target: prerequisites 
