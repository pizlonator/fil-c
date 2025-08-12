#include <string.h>

int foo(void* a, void* b)
{
    char buf[16];
    memcpy(buf, b, 16);
    memcpy(b, a, 16);
    memcpy(a, buf, 16);
    return buf[4] + buf[11];
}

