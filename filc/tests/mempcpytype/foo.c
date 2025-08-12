#define _GNU_SOURCE

#include <string.h>

int foo(void* a, void* b)
{
    char buf[16];
    mempcpy(buf, b, 16);
    mempcpy(b, a, 16);
    mempcpy(a, buf, 16);
    return buf[4] + buf[11];
}

