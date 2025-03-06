#include <signal.h>
#include <filc_test_support.h>
#include <pizlonated_runtime.h>
#include <stdlib.h>

static unsigned counter;

static void handler(int signo, siginfo_t* info, void* context)
{
    counter++;
}

int main()
{
    struct sigaction act;
    act.sa_sigaction = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    act.sa_flags = SA_SIGINFO;
    ZASSERT(!sigaction(SIGUSR1, &act, NULL));

    unsigned count;
    for (count = 10; count--;) {
        ZASSERT(!counter);
        zincrement_signal_deferral_depth();
        unsigned sub_count;
        for (sub_count = 1000; sub_count--;)
            raise(SIGUSR1);
        ZASSERT(!counter);
        ZASSERT(znum_deferred_signals() >= 1000);
        zdecrement_signal_deferral_depth();
        ZASSERT(counter == 1000);
        ZASSERT(znum_deferred_signals() < 1000);
        counter = 0;
    }

    return 0;
}

