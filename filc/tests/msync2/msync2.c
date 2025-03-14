#include <sys/mman.h>
#include <stdfil.h>

int main()
{
    msync(zgc_aligned_alloc(4096, 4096), 4096, MS_SYNC);
    return 0;
}
