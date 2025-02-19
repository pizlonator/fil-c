#include <stdfil.h>
#include <unistd.h>
#include <sched.h>
#include <stdbool.h>

static void foo(void) __attribute__((constructor));

static void foo(void)
{
    zprintf("gctor: gonna sleep a bit.\n");
    sched_yield();
    zprintf("gctor: done.\n");
}

void bar(void)
{
}
