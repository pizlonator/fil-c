#ifndef _FUTEX_CALLS_H_
#define _FUTEX_CALLS_H_

#include <sys/time.h>

void yolo_futex_wake(volatile int *addr, int cnt, int priv);
void yolo_futex_wait(volatile int *addr, int val, int priv);

/* These return the errno as a negative value. */
int yolo_futex_timedwait(volatile int *addr, int val, int clock_id, const struct timespec *timeout, int priv);
int yolo_futex_unlock_pi(volatile int *addr, int priv);
int yolo_futex_lock_pi(volatile int *addr, int priv, const struct timespec *timeout);
int yolo_futex_requeue(volatile int *addr, int priv, int wake_count, int requeue_count, volatile int *addr2);

#endif /* _FUTEX_CALLS_H_ */

