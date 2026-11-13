#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define CHECK(x) do { \
    if (!(x)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
        exit(1); \
    } \
} while (0)

static long rss_mib(void)
{
    FILE* f = fopen("/proc/self/status", "r");
    char line[256];
    long kib = 0;
    CHECK(f);
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "VmRSS:", 6))
            sscanf(line + 6, "%ld", &kib);
    }
    fclose(f);
    return kib / 1024;
}

/* Allocates 2 GiB of garbage in blocks of 64 KiB, and returns the most resident memory that it saw. */
static long churn(void)
{
    long peak = 0;
    long i;
    for (i = 0; i < 32768; ++i) {
        char* p = malloc(64 * 1024);
        CHECK(p);
        memset(p, 1, 64 * 1024);
        if (!(i % 256)) {
            long r = rss_mib();
            if (r > peak)
                peak = r;
        }
    }
    return peak;
}

#define LIMIT_MIB 600

int main(void)
{
    /* The size of a mapping is address space. If it counted as live memory, the collector would let the
       garbage grow to half of that size before it ran: 8 GiB for this mapping. */
    long base = churn();
    CHECK(base < LIMIT_MIB);

    /* Address space that nothing touches, kept alive to the end. */
    void* reservation = mmap(NULL, 16ul << 30, PROT_NONE,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    CHECK(reservation != MAP_FAILED);
    long with_reservation = churn();
    printf("peak resident: %ld MiB, with 16 GiB reserved: %ld MiB\n", base, with_reservation);
    CHECK(with_reservation < LIMIT_MIB);

    /* Memory that was touched and freed is dirty when it is reused. A pointer stored into a mapping makes
       the runtime allocate the aux of the mapping, which is as large as the mapping and has to be zero:
       zeroed by writing, it would be resident in full, however little of it the program uses. */
    enum { count = 8 };
    char* dirty[count];
    int i;
    for (i = 0; i < count; ++i) {
        dirty[i] = malloc(130ul << 20);
        CHECK(dirty[i]);
        memset(dirty[i], 1, 130ul << 20);
    }
    for (i = 0; i < count; ++i)
        dirty[i] = NULL;
    zgc_wait(zgc_request_fresh());
    zgc_wait(zgc_request_fresh());
    zscavenge_synchronously();
    long before = rss_mib();

    char* maps[count];
    for (i = 0; i < count; ++i) {
        maps[i] = mmap(NULL, 128ul << 20, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
        CHECK(maps[i] != MAP_FAILED);
        *(char**)maps[i] = maps[i] + 4096;
    }
    long after = rss_mib();
    printf("%d mappings of 128 MiB that each hold one pointer: resident %ld MiB, was %ld MiB\n",
           count, after, before);
    CHECK(after - before < 128);

    for (i = 0; i < count; ++i)
        CHECK(*(char**)maps[i] == maps[i] + 4096);
    CHECK(reservation != NULL);

    printf("mmaplivebytes ok\n");
    return 0;
}
