VERSION = -std=c++17

.PHONY: all build run create

all: build

create:
	@mkdir -p build

build: create
	g++ src/main.cpp -I include ${VERSION} -c -o build/main.o 
	g++ src/Assembly_line.cpp -I include ${VERSION} -c -o build/Assembly_line.o 
	g++ build/main.o build/Assembly_line.o -obuild/main

run: create build
	./build/main