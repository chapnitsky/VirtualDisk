all: main.cpp
	g++ main.cpp -o main
all-GDB: main.cpp
	g++ -g main.cpp -o main