#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>

int gotsig;

void
onsig(int sig)
{
    gotsig = sig;
}

void*
SuspendWorker(void* arg)
{
    sigset_t ss;
    sigemptyset(&ss);
    sigsuspend(&ss);
    if (gotsig != SIGUSR1)
        exit(3);
    return 0;
}

int
main()
{
    pthread_t t;
    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGUSR1);
    signal(SIGUSR1, onsig);
    sigprocmask(SIG_BLOCK, &ss, 0);
    pthread_create(&t, 0, SuspendWorker, 0);
    pthread_kill(t, SIGUSR1);
    pthread_join(t, 0);
    if (gotsig != SIGUSR1)
        exit(4);
    return 0;
}
