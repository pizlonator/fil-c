#include <stdfil.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

static ucontext_t ctx_main;
static ucontext_t ctx_func1;
static bool funcok;
static bool mainok;

static void func1(void)
{
    zprintf("in func1\n");
    swapcontext(&ctx_func1, &ctx_main);
    zprintf("in func1 again!\n");
    funcok = true;
}

static void exit_handler(void)
{
    ZASSERT(funcok);
    ZASSERT(mainok);
    zprintf("in exit_handler\n");
}

int main()
{
    atexit(exit_handler);
    
    getcontext(&ctx_func1);
    ctx_func1.uc_stack.ss_size = 16384;
    makecontext(&ctx_func1, func1, 0);
    zprintf("switching to func1\n");
    swapcontext(&ctx_main, &ctx_func1);
    zprintf("back from func1\n");
    mainok = true;

    setcontext(&ctx_func1);
    ZASSERT(!"Should not be reached");

    return 0;
}

