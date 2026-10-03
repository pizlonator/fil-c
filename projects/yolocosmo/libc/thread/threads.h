#ifndef COSMOPOLITAN_LIBC_THREAD_THREADS_H_
#define COSMOPOLITAN_LIBC_THREAD_THREADS_H_
#include "libc/calls/struct/timespec.h"
COSMOPOLITAN_C_START_

#if !defined(__cplusplus) && defined(__STDC_VERSION__) && \
    __STDC_VERSION__ >= 201112L && __STDC_VERSION__ <= 201710L
#define thread_local _Thread_local
#endif

#define ONCE_FLAG_INIT 0

#define TSS_DTOR_ITERATIONS 4

#ifndef __cplusplus
#define _THREADS_ATOMIC(x) _Atomic(x)
#else
#define _THREADS_ATOMIC(x) x
#endif

enum {
  thrd_success = 0,
  thrd_busy = 1,
  thrd_error = 2,
  thrd_nomem = 3,
  thrd_timedout = 4,
  thrd_canceled = 5,
};

enum {
  mtx_plain = 0,
  mtx_recursive = 1,
  mtx_timed = 2,
};

typedef struct {
  _THREADS_ATOMIC(unsigned) _nsync;
  void *_waiters;
  _THREADS_ATOMIC(uint64_t) _word;
} mtx_t;

typedef struct {
  _THREADS_ATOMIC(unsigned) _nsync;
  void *_waiters;
} cnd_t;

typedef uintptr_t thrd_t;
typedef void (*tss_dtor_t)(void *);
typedef int (*thrd_start_t)(void *);
typedef unsigned once_flag;
typedef unsigned tss_t;

/* clang-format off */
int thrd_create(thrd_t *, thrd_start_t, void *) dontthrow paramsnonnull((1));
int thrd_detach(thrd_t) dontthrow;
int thrd_equal(thrd_t, thrd_t) dontthrow;
int thrd_join(thrd_t, int *) dontthrow;
int tss_create(tss_t *, tss_dtor_t) dontthrow;
int tss_set(tss_t, void *) dontthrow;
thrd_t thrd_current(void) dontthrow;
void *tss_get(tss_t) dontthrow;
void call_once(once_flag *, void (*)(void)) paramsnonnull();
void thrd_exit(int) wontreturn;
void thrd_yield(void) dontthrow;
void tss_delete(tss_t) dontthrow;
int mtx_init(mtx_t *, int) dontthrow paramsnonnull();
void mtx_destroy(mtx_t *) dontthrow paramsnonnull();
int mtx_lock(mtx_t *) dontthrow paramsnonnull();
int mtx_unlock(mtx_t *) dontthrow paramsnonnull();
int mtx_trylock(mtx_t *) dontthrow paramsnonnull();
int mtx_timedlock(mtx_t *, const struct timespec *) dontthrow paramsnonnull((1));
int cnd_init(cnd_t *) dontthrow paramsnonnull();
void cnd_destroy(cnd_t *) dontthrow paramsnonnull();
int cnd_broadcast(cnd_t *) dontthrow paramsnonnull();
int cnd_signal(cnd_t *) dontthrow paramsnonnull();
int cnd_timedwait(cnd_t *, mtx_t *, const struct timespec *) dontthrow paramsnonnull((1, 2));
int cnd_wait(cnd_t *, mtx_t *) dontthrow paramsnonnull();
int thrd_sleep(const struct timespec *, struct timespec *);
/* clang-format on */

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_LIBC_THREAD_THREADS_H_ */
