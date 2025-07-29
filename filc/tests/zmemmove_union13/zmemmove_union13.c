#include <filc_test_support.h>
#include <stdlib.h>
#include <stdfil.h>
#include <string.h>
#include <pthread.h>

#define SIZE 10000000
#define REPEAT 10

static char** one;
static char** two;

static void* thread_main(void* arg)
{
    while (!two[0])
        sched_yield();
    free(one);
    zgc_request_and_wait();
    unsigned i;
    for (i = SIZE; i--;)
        strdup("world");
    return NULL;
}

static void do_test(void)
{
    one = (char**)malloc(sizeof(char*) * SIZE);
    two = (char**)malloc(sizeof(char*) * SIZE);

    unsigned i;
    for (i = SIZE; i--;)
        one[i] = strdup("hello");

    zprintf("one[0] = %s\n", one[0]);

    pthread_t t;
    pthread_create(&t, NULL, thread_main, NULL);

    zmemmove_union(two, one, sizeof(char*) * SIZE);
    
    pthread_join(t, NULL);

    for (i = SIZE; i--;) {
        char* str = two[i];
        if (strcmp(str, "hello")) {
            zprintf("two[%u] = %O, %s\n", i, str, str);
            ZASSERT(!strcmp(str, "hello"));
        }
    }
}

int main()
{
    unsigned i;
    for (i = REPEAT; i--;)
        do_test();
    
    return 0;
}

