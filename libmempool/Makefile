all: libmempool.cpp
	g++ -c -g -O3 -Wall -Werror -fPIC -std=c++11 libmempool.cpp MemoryPool.tcc
	g++ -g -shared -o libmempool.so libmempool.o

install:
	cp libmempool.so /usr/lib/libmempool.so

clean:
	rm -f /usr/lib/libmempool.so
	rm -f *.so *.o
