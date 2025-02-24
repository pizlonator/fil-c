#include <pthread.h>
#include <stdio.h>
#include <stdfil.h>
#include <string.h>
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>

#define NTHREADS 10
#define REPEAT 10000

struct lock {
    int word;
};

#define LOCK_NOT_HELD 0
#define LOCK_HELD 1
#define LOCK_HELD_WAITING 2

static void lock_init(struct lock* lock)
{
    lock->word = LOCK_NOT_HELD;
}

static int int_cas(int* ptr, int expected, int new_value)
{
    __c11_atomic_compare_exchange_strong((_Atomic int*)ptr, &expected, new_value,
                                         __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return expected;
}

static void lock_lock(struct lock* lock)
{
    if (int_cas(&lock->word, LOCK_NOT_HELD, LOCK_HELD) == LOCK_NOT_HELD)
        return;

    /* The trick is that if we ever choose to wait, then we will acquire the lock in the waiting
       state. This ensures that the lock never forgets that there are threads waiting. It is also
       slightly conservative: if there's a queue of threads waiting, then the last thread in the
       queue will acquire in waiting mode and then do a wake when unlocking, even though it doesn't
       strictly have to since it's the last one. */
    int locked_state = LOCK_HELD;
    for (;;) {
        int old_state = lock->word;

        if (old_state == LOCK_NOT_HELD) {
            if (int_cas(&lock->word, LOCK_NOT_HELD, locked_state) == LOCK_NOT_HELD)
                return;
            continue;
        }

        if (old_state == LOCK_HELD) {
            if (int_cas(&lock->word, LOCK_HELD, LOCK_HELD_WAITING) != LOCK_HELD)
                continue;
        } else
            ZASSERT(old_state == LOCK_HELD_WAITING);
        locked_state = LOCK_HELD_WAITING;

        syscall(SYS_futex, &lock->word, FUTEX_WAIT_PRIVATE, LOCK_HELD_WAITING, NULL, NULL, 0);
    }
}

static void lock_unlock(struct lock* lock)
{
    for (;;) {
        if (int_cas(&lock->word, LOCK_HELD, LOCK_NOT_HELD) == LOCK_HELD)
            break;

        int old_state = lock->word;
        ZASSERT(old_state == LOCK_HELD || old_state == LOCK_HELD_WAITING);

        if (int_cas(&lock->word, LOCK_HELD_WAITING, LOCK_NOT_HELD) == LOCK_HELD_WAITING) {
            syscall(SYS_futex, &lock->word, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
            break;
        }
    }
}

typedef struct {
    const char* str;
} stuff;

static struct lock lock;
static unsigned count;

static void* thread_main(void* arg)
{
    unsigned i;
    for (i = REPEAT; i--;) {
        lock_lock(&lock);
        count++;
        lock_unlock(&lock);
    }
    stuff* s = (stuff*)arg;
    return (void*)s->str;
}

int main()
{
    pthread_t threads[NTHREADS];
    unsigned i;

    printf("lock is at %p\n", &lock);
    
    lock_init(&lock);

    for (i = NTHREADS; i--;) {
        stuff* s = zgc_alloc(sizeof(stuff));
        s->str = zasprintf("thread = %u", i);
        ZASSERT(!pthread_create(threads + i, NULL, thread_main, s));
    }

    for (i = NTHREADS; i--;) {
        const char* str;
        ZASSERT(!pthread_join(threads[i], (void**)&str));
        ZASSERT(!strcmp(str, zasprintf("thread = %u", i)));
    }
    
    printf("count = %u\n", count);
    return 0;
}

