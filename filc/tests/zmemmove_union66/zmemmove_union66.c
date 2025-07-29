#include <filc_test_support.h>
#include <pthread.h>
#include <stdfil.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define LENGTH 100
#define REPEAT 1000000
#define NTHREADS 10

static char** array;

static void* thread_main(void* arg)
{
    ZASSERT(!arg);
    size_t count;
    for (count = REPEAT; count--;) {
        char** new_array = malloc(sizeof(char*) * LENGTH);
        zmemmove_union(new_array, array, sizeof(char*) * LENGTH);
        zstore_store_fence();
        array = new_array;
    }
    return NULL;
}

int main()
{
    array = malloc(sizeof(char*) * LENGTH);
    size_t index;
    for (index = LENGTH; index--;)
        array[index] = zasprintf("index = %zu", index);

    pthread_t* threads = malloc(sizeof(pthread_t) * NTHREADS);
    for (index = NTHREADS; index--;)
        pthread_create(threads + index, NULL, thread_main, NULL);
    for (index = NTHREADS; index--;)
        pthread_join(threads[index], NULL);

    for (index = LENGTH; index--;)
        ZASSERT(!strcmp(array[index], zasprintf("index = %zu", index)));

    return 0;
}

