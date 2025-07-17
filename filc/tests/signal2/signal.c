#include <signal.h>
#include <stdfil.h>
#include <stdbool.h>

static bool handler_called = false;
static void handler(int signo)
{
    zprintf("handler!\n");
    ZASSERT(signo == SIGRTMIN);
    handler_called = true;
}

static bool handler2_called = false;
static void handler2(int signo)
{
    zprintf("handler2!\n");
    ZASSERT(signo == SIGRTMIN);
    handler2_called = true;
}

int main()
{
    ZASSERT(signal(SIGRTMIN, handler) == SIG_DFL);
    ZASSERT(signal(SIGRTMIN, handler2) == handler);
    ZASSERT(!raise(SIGRTMIN));
    ZASSERT(!handler_called);
    ZASSERT(handler2_called);
    return 0;
}

