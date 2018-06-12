all: preload.c indexer.c
	gcc -c -g -O3 -Wall -fPIC preload.c indexer.c
	gcc -g -shared -o libfsocket.so preload.o indexer.o -ldl

clean:
	rm -f *.so *.o
