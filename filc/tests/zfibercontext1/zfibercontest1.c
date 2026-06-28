#include <pizlonated_runtime.h>

static zfiber_context* ctx_main;
static zfiber_context* ctx_func1;
static zfiber_context* ctx_func2;

static void func1(void)
{
    zprintf("func1(swapcontext);");
    zfiber_context_swapcontext(ctx_func1, ctx_func2);
    zprintf("func1(setcontext);");
    zfiber_context_setcontext(ctx_main);
}

static void func2(void)
{
    zprintf("func2(swapcontext);");
    zfiber_context_swapcontext(ctx_func2, ctx_func1);
    zprintf("func2(setcontext);");
    zfiber_context_setcontext(ctx_func1);
}

int main()
{
    ctx_main = zfiber_context_new();
    ctx_func1 = zfiber_context_new();
    ctx_func2 = zfiber_context_new();

    zfiber_context_getcontext(ctx_func1);
    zfiber_context_makecontext(ctx_func1, 16384, func1);

    zfiber_context_getcontext(ctx_func2);
    zfiber_context_makecontext(ctx_func2, 16384, func2);

    zprintf("main(swapcontext);");
    zfiber_context_swapcontext(ctx_main, ctx_func2);
    zprintf("main(exiting);\n");

    return 0;
}

