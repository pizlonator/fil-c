#include <stdfil.h>
#include <unistd.h>

static void foo(void) __attribute__((constructor));

static void foo(void)
{
    zprintf("gctor2: gonna sleep a bit.\n");
    usleep(100);
}

void bar2(void)
{
}
