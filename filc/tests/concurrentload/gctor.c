#include <stdfil.h>
#include <unistd.h>

static void foo(void) __attribute__((constructor));

static void foo(void)
{
    zprintf("gctor: gonna sleep a bit.\n");
    usleep(100);
}

void bar(void)
{
}
