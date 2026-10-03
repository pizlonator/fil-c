/* GC stress for pointer round-trips through the sarcasm-assembled D9-promoted
   frame: the asm stores its argument pointer into the promoted frame with `#!
   store ptr`, a helper churns the allocator and forces stop-the-world
   collections while the pointer sits in the frame, and the asm reloads it with
   `#! load ptr` and writes through it. If the capability did not ride the
   region's sidecar across the store/reload (or the GC were not kept off the
   reloaded pointer's object), the write through the reloaded pointer would
   trap or the object would be corrupted. Run many threads with thread-distinct
   objects and per-iteration deterministic checksums, and scribble the native
   stack between calls so any unrooted root slot would be observed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdfil.h>
#include <filc_test_support.h>
#include "utils.h"

#define ASSERT(exp) do { \
    if ((exp)) \
        break; \
    fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", \
            __FILE__, __LINE__, __PRETTY_FUNCTION__, #exp); \
    abort(); \
} while (0)

long ptrslotfn(long *obj, long scalar);

/* every CHURN_STW_MASK+1-th helper call (per thread) forces a collection */
#ifndef CHURN_STW_MASK
#define CHURN_STW_MASK 7
#endif

/* Called with the asm's argument pointer while it sits in the promoted frame.
   Churns the allocator, forces a stop-the-world collection every K-th call,
   then reads obj[0] and returns it (the asm's own 4242 write happens after
   this call returns). */
static __thread unsigned long churn_calls;

__attribute__((noinline)) long churn(long *p)
{
    for (int i = 0; i < 32; i++) {
        long *m = malloc(64);
        if (!m)
            exit(1);
        volatile long *mv = m;
        *mv = i;
        free(m);
    }
    if ((churn_calls++ & CHURN_STW_MASK) == 0)
        zgc_request_and_wait();
    return p[0];
}

/* Leave wild-integer values over the native-stack region that the next call's
   frame (and hence its GC root slots) will overlap. The values come from
   volatile loads (so they can't be rematerialized) and are all live across a
   clobbering call (so they spill across this frame); the following ptrslotfn
   call's frame then reuses that region. */
static volatile unsigned long junk_source = 0xdeadbeef12345678UL;

__attribute__((noinline)) static unsigned long dirty_stack(void)
{
    unsigned long a0 = junk_source, a1 = junk_source + 1, a2 = junk_source + 2;
    unsigned long a3 = junk_source + 3, a4 = junk_source + 4, a5 = junk_source + 5;
    unsigned long a6 = junk_source + 6, a7 = junk_source + 7, a8 = junk_source + 8;
    unsigned long a9 = junk_source + 9, a10 = junk_source + 10, a11 = junk_source + 11;
    unsigned long a12 = junk_source + 12, a13 = junk_source + 13, a14 = junk_source + 14;
    unsigned long a15 = junk_source + 15, a16 = junk_source + 16, a17 = junk_source + 17;
    unsigned long a18 = junk_source + 18, a19 = junk_source + 19, a20 = junk_source + 20;
    unsigned long a21 = junk_source + 21, a22 = junk_source + 22, a23 = junk_source + 23;
    unsigned long a24 = junk_source + 24, a25 = junk_source + 25, a26 = junk_source + 26;
    unsigned long a27 = junk_source + 27, a28 = junk_source + 28, a29 = junk_source + 29;
    unsigned long a30 = junk_source + 30, a31 = junk_source + 31;
    opaque(NULL);   /* clobbers caller-saved regs: the values must be in the frame */
    return a0 ^ a1 ^ a2 ^ a3 ^ a4 ^ a5 ^ a6 ^ a7 ^ a8 ^ a9 ^ a10 ^ a11
        ^ a12 ^ a13 ^ a14 ^ a15 ^ a16 ^ a17 ^ a18 ^ a19 ^ a20 ^ a21 ^ a22 ^ a23
        ^ a24 ^ a25 ^ a26 ^ a27 ^ a28 ^ a29 ^ a30 ^ a31;
}

#ifndef PTRSLOT_REPEAT
#define PTRSLOT_REPEAT 100000
#endif

static size_t num_threads = 8;
static size_t repeat = PTRSLOT_REPEAT;

static volatile int hammer_stop = 0;

static void* hammer(void* arg)
{
    while (!hammer_stop) {
        zgc_request_and_wait();
        usleep(200);
    }
    return NULL;
}

static void* worker(void* arg)
{
    size_t tid = (size_t)arg;
    long *obj = malloc(4 * sizeof(long));
    unsigned long sink = 0;
    ASSERT(obj);
    obj[2] = -1215;
    obj[3] = -1216;
    size_t i;
    for (i = 0; i < repeat; i++) {
        long obj0 = (long)tid * 1000000 + (long)i;
        long obj1 = 2000 + (long)tid;
        long scalar = (long)tid * 1000 + (long)i;
        obj[0] = obj0;
        obj[1] = obj1;
        sink ^= dirty_stack();
        long r = ptrslotfn(obj, scalar);
        /* the asm returns the churn helper's return (obj0) + 4242 (its write
           through the reloaded pointer, read back through the second reload)
           + obj1 + the scalar + the movdqa lanes (7 + 8): */
        ASSERT(r == obj0 + 4242 + obj1 + scalar + 15);
        ASSERT(obj[0] == 4242);
        ASSERT(obj[1] == obj1);
        ASSERT(obj[2] == -1215);
        ASSERT(obj[3] == -1216);
    }
    return (void*)sink;
}

int main()
{
    if (zgc_is_stw()) {
        num_threads = 2;
        repeat = 5000;
    } else if (zgc_is_scribbling()) {
        num_threads = 2;
        repeat = 20000;
    }
    pthread_t th[num_threads];
    pthread_t hammer_thread;
    size_t i;
    ASSERT(!pthread_create(&hammer_thread, NULL, hammer, NULL));
    for (i = 0; i < num_threads; i++)
        ASSERT(!pthread_create(&th[i], NULL, worker, (void*)i));
    for (i = 0; i < num_threads; i++)
        ASSERT(!pthread_join(th[i], NULL));
    hammer_stop = 1;
    ASSERT(!pthread_join(hammer_thread, NULL));
    printf("sarcasm-frame-escape-ptrslot-gcstress ok\n");
    return 0;
}
