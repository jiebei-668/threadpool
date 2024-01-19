all: mainc maincpp
mainc: threadpool.c threadpool.h main.c
	gcc -g -o mainc threadpool.c main.c -std=c99 -lpthread
maincpp: threadpool.cpp threadpool_cpp.h main.cpp
	g++ -g -o maincpp threadpool.cpp main.cpp  -lpthread
clean:
	rm -f mainc maincpp
