all: main.cpp
	clang++ main.cpp -std=c++11

#remove named pipes
clean:
	rm pipe*
