#include <signal.h>
#include <stdfil.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

static bool handler_called = false;
static void handler(int signo)
{
    zprintf("handler!\n");
    ZASSERT(signo == SIGUSR1);
    handler_called = true;
}

int main()
{
    struct sigaction act;
    struct sigaction oact;
    bzero(&act, sizeof(act));
    bzero(&oact, sizeof(oact));
    act.sa_handler = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    ZASSERT(!sigaction(SIGUSR1, &act, &oact));
    ZASSERT(oact.sa_handler == SIG_DFL);
    ZASSERT(!handler_called);
    ZASSERT(!raise(SIGUSR1));
    ZASSERT(handler_called);
    handler_called = false;
    bzero(&act, sizeof(act));
    bzero(&oact, sizeof(oact));
    act.sa_handler = handler;
    ZASSERT(!sigfillset(&act.sa_mask));
    ZASSERT(sigaction(666666, &act, &oact) == -1);
    ZASSERT(errno == EINVAL);
    ZASSERT(!handler_called);
    ZASSERT(!raise(SIGUSR1));
    ZASSERT(handler_called);
    return 0;
}

