#include <pthread.h>
#include <stdfil.h>
#include <stdlib.h>
#include <unistd.h>

static void* thread_main(void* arg)
{
    ZASSERT(!arg);
    return NULL;
}

int main()
{
    pthread_t t;
    pthread_create(&t, NULL, thread_main, NULL);
    pthread_detach(t);
    usleep(10000);
    return 0;
}

