VERSION = -std=c++17

.PHONY: all build run create

all: build

create:
	@mkdir -p build

build: create
	g++ src/main.cpp -I include ${VERSION} -c -o build/main.o 
	g++ src/AssemblyLine.cpp -I include ${VERSION} -c -o build/AssemblyLine.o 
	g++ build/main.o build/AssemblyLine.o -obuild/main

run: create build
	./build/main