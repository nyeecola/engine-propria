all: build run

build:
	mkdir -p bin
	gcc src/main.c -o bin/main.exe -lglfw -lGLEW -lGL -lm

run:
	bin/main.exe
