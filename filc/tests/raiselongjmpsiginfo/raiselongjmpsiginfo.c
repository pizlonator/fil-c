#include <signal.h>
#include <stdio.h>
#include <stdfil.h>
#include <setjmp.h>

static sigjmp_buf jb;

static void handler(int signo, siginfo_t* info, void* arg)
{
    siglongjmp(jb, 1410);
}

int main()
{
    volatile int x = 42;
    int result = sigsetjmp(jb, 1);
    if (result) {
        ZASSERT(result == 1410);
        printf("x = %d\n", x);
        return 0;
    }
    struct sigaction act;
    act.sa_sigaction = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    act.sa_flags = SA_SIGINFO;
    ZASSERT(!sigaction(SIGUSR1, &act, NULL));
    x = 666;
    raise(SIGUSR1);
    printf("Should not get here.\n");
    return 1;
}

