#include <pizlonated_runtime.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdfil.h>
#include <errno.h>

static void* worker(void* arg)
{
    return NULL;
}

int
main()
{
    sigset_t ss;
    ZASSERT(!sigemptyset(&ss));
    ZASSERT(!sigaddset(&ss, SIGUSR1));
    ZASSERT(!sigprocmask(SIG_BLOCK, &ss, 0));
    void* t = zthread_create(worker, NULL);
    ZASSERT(t);
    ZASSERT(zthread_join(t, NULL));
    ZASSERT(!zthread_kill(t, SIGUSR1));
    ZASSERT(errno == ESRCH);
    return 0;
}
