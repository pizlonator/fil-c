#include <stddef.h>
#include <sys/mman.h>
#include <stdfil.h>

int main()
{
    mlock(zweak_new(NULL), 16);
    return 0;
}
