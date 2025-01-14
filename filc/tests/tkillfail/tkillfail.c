#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdfil.h>
#include <errno.h>

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
    ZASSERT(!sigemptyset(&ss));
    ZASSERT(sigsuspend(&ss) == -1);
    ZASSERT(errno == EINTR);
    ZASSERT(gotsig == SIGUSR1);
    return 0;
}

int
main()
{
    pthread_t t;
    sigset_t ss;
    ZASSERT(!sigemptyset(&ss));
    ZASSERT(!sigaddset(&ss, SIGUSR1));
    ZASSERT(!signal(SIGUSR1, onsig));
    ZASSERT(!sigprocmask(SIG_BLOCK, &ss, 0));
    ZASSERT(!pthread_create(&t, 0, SuspendWorker, 0));
    ZASSERT(pthread_kill(t, 33) == ENOSYS);
    return 0;
}
