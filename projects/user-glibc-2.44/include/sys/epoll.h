#ifndef _SYS_EPOLL_H
#include_next <sys/epoll.h>

# ifndef _ISOMAC

libc_hidden_proto (epoll_pwait)
#if __TIMESIZE == 64
# define __epoll_pwait2_time64 epoll_pwait2
#else
extern int __epoll_pwait2_time64 (int fd, struct epoll_event *ev, int maxev,
				  const struct __timespec64 *tmo,
				  const sigset_t *s)
       __nonnull ((2));
libc_hidden_proto (__epoll_pwait2_time64)
#endif

# endif /* !_ISOMAC */
#endif
