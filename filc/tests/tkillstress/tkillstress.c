#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdfil.h>
#include <errno.h>

static void handler(int sig)
{
    ZASSERT(sig == SIGUSR1);
}

static void* thread_main(void* arg)
{
    return NULL;
}

int main()
{
    struct sigaction act;
    act.sa_handler = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    act.sa_flags = 0;
    ZASSERT(!sigaction(SIGUSR1, &act, NULL));
    
    unsigned count;
    for (count = 10000; count--;) {
        pthread_t t;
        ZASSERT(!pthread_create(&t, NULL, thread_main, NULL));
        sched_yield();
        int result = pthread_kill(t, SIGUSR1);
        ZASSERT(!result || result == ESRCH);
        ZASSERT(!pthread_join(t, NULL));
    }
    
    return 0;
}

