#define _GNU_SOURCE

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static atomic_ulong completed;

static void *worker(void *unused)
{
    (void)unused;

    pthread_attr_t attributes;
    int result = pthread_getattr_np(pthread_self(), &attributes);
    if (result != 0)
    {
        errno = result;
        perror("pthread_getattr_np");
        exit(EXIT_FAILURE);
    }

    result = pthread_attr_destroy(&attributes);
    if (result != 0)
    {
        errno = result;
        perror("pthread_attr_destroy");
        exit(EXIT_FAILURE);
    }

    atomic_fetch_add_explicit(&completed, 1, memory_order_relaxed);
    return NULL;
}

int main(int argc, char **argv)
{
    unsigned long iterations = argc == 2 ? strtoul(argv[1], NULL, 10) : 100;
    for (unsigned long i = 0; i < iterations; ++i)
    {
        pthread_t thread;
        int result = pthread_create(&thread, NULL, worker, NULL);
        if (result != 0)
        {
            errno = result;
            perror("pthread_create");
            return EXIT_FAILURE;
        }

        result = pthread_join(thread, NULL);
        if (result != 0)
        {
            errno = result;
            perror("pthread_join");
            return EXIT_FAILURE;
        }
    }

    printf("completed %lu thread creations\n", atomic_load_explicit(&completed, memory_order_relaxed));
    return EXIT_SUCCESS;
}
