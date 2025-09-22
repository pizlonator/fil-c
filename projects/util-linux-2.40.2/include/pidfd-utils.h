/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 */
#ifndef UTIL_LINUX_PIDFD_UTILS
#define UTIL_LINUX_PIDFD_UTILS

#include <sys/types.h>
#include <sys/wait.h>

#ifdef HAVE_SYS_SYSCALL_H
# include <sys/syscall.h>

# if defined(SYS_pidfd_send_signal) && defined(SYS_pidfd_open)
#  ifdef HAVE_SYS_PIDFD_H
#   include <sys/pidfd.h>
#  endif
#  ifndef HAVE_PIDFD_SEND_SIGNAL
static inline int pidfd_send_signal(int pidfd, int sig, siginfo_t *info,
				    unsigned int flags)
{
	return syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags);
}
#  endif

#  ifndef HAVE_PIDFD_OPEN
static inline int pidfd_open(pid_t pid, unsigned int flags)
{
	return syscall(SYS_pidfd_open, pid, flags);
}
#  endif

#  define UL_HAVE_PIDFD 1

# endif	/* SYS_pidfd_send_signal */
#endif /* HAVE_SYS_SYSCALL_H */

#ifndef UL_HAVE_PIDFD
static inline int pidfd_send_signal(int pidfd __attribute__((unused)),
				    int sig __attribute__((unused)),
				    siginfo_t *info __attribute__((unused)),
				    unsigned int flags __attribute__((unused)))
{
	errno = ENOSYS;
	return -1;
}

static inline int pidfd_open(pid_t pid __attribute__((unused)),
			     unsigned int flags __attribute__((unused)))
{
	errno = ENOSYS;
	return -1;
}
#endif

#endif /* UTIL_LINUX_PIDFD_UTILS */
