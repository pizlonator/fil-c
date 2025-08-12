#include <string.h>

int foo(void* a, void* b)
{
    char buf[16];
    memmove(buf, b, 16);
    memmove(b, a, 16);
    memmove(a, buf, 16);
    return buf[4] + buf[11];
}

