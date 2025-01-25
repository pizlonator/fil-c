#include <pthread.h>
#include <stdfil.h>
#include "utils.h"
#include <stdlib.h>

#define NTHREADS 10
#define REPEAT 1000

static int x = 666;
static __thread int* ptr = &x;

static void* thread_main(void *arg)
{
    ZASSERT(ptr == &x);
    ZASSERT(*ptr == 666);
    int value = (int)arg;
    ZASSERT(value);
    ptr = malloc(sizeof(int));
    *ptr = value;
    ZASSERT(*(int*)opaque(ptr) == value);
    return NULL;
}

int main()
{
    pthread_t threads[NTHREADS];
    unsigned index;
    unsigned count;
    for (count = REPEAT; count--;) {
        for (index = NTHREADS; index--;)
            ZASSERT(!pthread_create(threads + index, NULL, thread_main, (void*)(1 + count + index)));
        for (index = NTHREADS; index--;)
            ZASSERT(!pthread_join(threads[index], NULL));
    }
    return 0;
}

