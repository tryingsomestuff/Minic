.PHONY: build

ROOT_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

build:
	chmod +x Tools/*.sh
	$(ROOT_DIR)/Tools/build.sh

prerequisites: build

target: prerequisites 
