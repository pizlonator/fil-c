#define _GNU_SOURCE
#include <unistd.h>
#include "syscall.h"
#include <stdfil.h>

int syncfs(int fd)
{
	return zsys_syncfs(fd);
}
