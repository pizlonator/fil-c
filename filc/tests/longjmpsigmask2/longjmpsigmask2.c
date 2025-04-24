#include <setjmp.h>
#include <stdio.h>
#include <signal.h>
#include <stdfil.h>
#include <pizlonated_runtime.h>
#include <stdbool.h>

#undef setjmp

void* opaque(void*);

int main(int argc, char** argv)
{
    zmake_setjmp_save_sigmask(true);
    volatile int x = (int)opaque((void*)42);
    jmp_buf jb;
    sigset_t set;
    ZASSERT(!pthread_sigmask(0, 0, &set));
    ZASSERT(!sigismember(&set, SIGUSR1));
    if (setjmp(jb)) {
        printf("x = %d\n", x);
        ZASSERT(!pthread_sigmask(0, 0, &set));
        ZASSERT(!sigismember(&set, SIGUSR1));
        return 0;
    }
    ZASSERT(!sigemptyset(&set));
    ZASSERT(!sigaddset(&set, SIGUSR1));
    ZASSERT(!pthread_sigmask(SIG_SETMASK, &set, NULL));
    x = 666;
    longjmp(jb, 1);
    printf("Should not get here.\n");
    return 1;
}

