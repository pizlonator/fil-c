#ifndef	_SEMAPHORE_H
#include_next <semaphore.h>

#ifndef _ISOMAC
extern __typeof (sem_post) __sem_post;
libc_hidden_proto (__sem_post)

extern int __sem_clockwait (sem_t *__sem,
			  clockid_t clock,
			  const struct timespec *__abstime);
libc_hidden_proto (__sem_clockwait)
extern int __sem_timedwait (sem_t *__sem,
			  const struct timespec *__abstime);
libc_hidden_proto (__sem_timedwait)

extern int __sem_trywait (sem_t *__sem);
libc_hidden_proto (__sem_trywait)
#endif

#endif
