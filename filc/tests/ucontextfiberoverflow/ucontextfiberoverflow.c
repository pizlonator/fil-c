#define _GNU_SOURCE
#include <stdfil.h>
#include <stdio.h>
#include <ucontext.h>

static ucontext_t parent_ctx;
static ucontext_t child_ctx;

__attribute__((noinline))
static int overflow_recurse(volatile int depth)
{
    volatile char buf[1024];
    buf[0] = (char)depth;
    buf[1023] = (char)(depth + 1);
    if (depth > 2000)
        return buf[0] + buf[1023];
    return overflow_recurse(depth + 1) + buf[0];
}

static void overflow_fiber(void)
{
    int v = overflow_recurse(1);
    printf("UNEXPECTED_FIBER_OVERFLOW_SURVIVED=%d\n", v);
    fflush(stdout);
    setcontext(&parent_ctx);
}

int main(void)
{
    ZASSERT(getcontext(&child_ctx) == 0);
    child_ctx.uc_stack.ss_size = 16384;
    child_ctx.uc_link = &parent_ctx;
    makecontext(&child_ctx, overflow_fiber, 0);
    swapcontext(&parent_ctx, &child_ctx);
    printf("UNEXPECTED_FIBER_OVERFLOW_RETURNED\n");
    return 0;
}
