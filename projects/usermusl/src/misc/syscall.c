#define _BSD_SOURCE
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>
#include "syscall.h"
#include "fork_impl.h"

#undef syscall

/* Fil-C supports only a clone that is a fork. It also creates the child in new namespaces, and the
   parent keeps its own namespaces and its runtime threads. The runtime stops its threads for the fork
   and restarts them in both processes. A clone that shares memory with the child, or makes a thread,
   cannot get the state that the runtime needs. The caller must pass all five arguments, and everything
   after the flags must be zero, whatever order the architecture gives them. */
#define CLONE_FORK_FLAGS (CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWUTS | CLONE_NEWIPC | \
	CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET | CLONE_UNTRACED)

static long clone_as_fork(long flags, long a1, long a2, long a3, long a4)
{
	if (a1 || a2 || a3 || a4 || (flags & 0xff) != SIGCHLD ||
	    (flags & ~(long)(0xff | CLONE_FORK_FLAGS))) {
		errno = EINVAL;
		return -1;
	}
	return __Fork_flags((int)(flags & CLONE_FORK_FLAGS));
}

long syscall(long n, ...)
{
	if (n == SYS_clone) {
		va_list ap;
		long flags, a1, a2, a3, a4;
		va_start(ap, n);
		flags = va_arg(ap, long);
		a1 = va_arg(ap, long);
		a2 = va_arg(ap, long);
		a3 = va_arg(ap, long);
		a4 = va_arg(ap, long);
		va_end(ap);
		return clone_as_fork(flags, a1, a2, a3, a4);
	}
	return *(long*)zcall(zsys_syscall, zargs());
}
