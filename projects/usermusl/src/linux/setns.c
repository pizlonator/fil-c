#define _GNU_SOURCE
#include <sched.h>
#include "syscall.h"
#include <stdfil.h>

int setns(int fd, int nstype)
{
	return zsys_setns(fd, nstype);
}
