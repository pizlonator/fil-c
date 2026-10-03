#include <sys/mman.h>
#include <stdlib.h>
#include <stdfil.h>

int main()
{
    /* Fil-C rounds malloc(100) up to a 112-byte allocation, so buf + 100 is still within the
       object's bounds, but locking 100 bytes starting there overruns it. */
    char* buf = malloc(100);
    ZASSERT(buf);
    mlock(buf + 100, 100);
    return 0;
}
