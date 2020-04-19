.PHONY: build

build: 
	./Tools/build.sh

prerequisites: build

target: prerequisites 
