#include <unistd.h>
#include <signal.h>
#include "syscall.h"
#include "libc.h"
#include "lock.h"
#include "pthread_impl.h"
#include "aio_impl.h"
#include "fork_impl.h"

static void dummy(int x) { }
weak_alias(dummy, __aio_atfork);

void __post_Fork(int ret)
{
	if (!ret) {
		pthread_t self = __pthread_self();
		self->tid = zthread_self_id();
		self->robust_list.off = 0;
		self->robust_list.pending = 0;
		self->next = self->prev = self;
		__thread_list_lock = 0;
		libc.threads_minus_1 = 0;
		if (libc.need_locks) libc.need_locks = -1;
	}
	UNLOCK(__abort_lock);
	if (!ret) __aio_atfork(1);
}

/* Like _Fork, but also creates the child in new namespaces. The flags are clone flags
   (CLONE_NEWPID, ...) that this adds to the exit signal SIGCHLD. */
pid_t __Fork_flags(int flags)
{
	pid_t ret;
	sigset_t set;
	__block_all_sigs(&set);
	LOCK(__abort_lock);
	ret = flags ? zsys_fork_flags(flags) : zsys_fork();
	int saved_errno = errno;
	__post_Fork(ret);
	__restore_sigs(&set);
	errno = saved_errno;
	return ret;
}

pid_t _Fork(void)
{
	return __Fork_flags(0);
}
