#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include <map>
#include <string>

#include "constant.h"
#include "log.h"

class ShmPiece
{
public:
	
	std::string name;
	int size;
	int shm_fd;
	void *ptr;

	ShmPiece(const char* name, int size);
	~ShmPiece();

	bool open();
	void remove();
};

#endif /* SHARED_MEMORY_H */
