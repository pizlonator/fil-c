#include <signal.h>
#include <stdbool.h>
#include <stdfil.h>
#include <stdlib.h>
#include <unistd.h>

static bool did_run = false;

static void handler(int signo, siginfo_t* info, void* context)
{
    ZASSERT(signo == SIGUSR1);
    ZASSERT(info->si_signo == SIGUSR1);
    ZASSERT(info->si_value.sival_int == 666);
    did_run = true;
}

int main()
{
    struct sigaction act;
    act.sa_sigaction = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    act.sa_flags = SA_SIGINFO;
    ZASSERT(!sigaction(SIGUSR1, &act, NULL));
    union sigval sigval;
    sigval.sival_int = 666;
    ZASSERT(!sigqueue(getpid(), SIGUSR1, sigval));
    ZASSERT(did_run);
    return 0;
}

