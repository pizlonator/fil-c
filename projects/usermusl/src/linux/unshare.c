#define _GNU_SOURCE
#include <sched.h>
#include "syscall.h"
#include <stdfil.h>

int unshare(int flags)
{
	return zsys_unshare(flags);
}
