#include <errno.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FILC_HAS_ZGC_RESERVE_AT
#error "no zgc_reserve_at"
#endif

#define CHUNK (512ul * 1024)

#define CHECK(x) do { \
    if (!(x)) { \
        printf("FAIL %s:%d: %s (errno = %d)\n", __FILE__, __LINE__, #x, errno); \
        exit(1); \
    } \
} while (0)

#define NOINLINE __attribute__((noinline))

static const unsigned long base = 0x3c0000000000ul;

/* A freed object is reclaimed after a few GC cycles, and only then is its address available again. The
   caller must not keep any pointer to the old object, including in a dead local, so the phases below
   are functions: their frames are gone when they return. */
static char* reserve_after_gc(unsigned long address, size_t length)
{
    for (int n = 0; n < 10; n++) {
        char* result = zgc_reserve_at(address, length);
        if (result)
            return result;
        CHECK(errno == EEXIST);
        zgc_wait(zgc_request_fresh());
    }
    return NULL;
}

/* Reserves the first range, uses it, and unmaps it. Returns the dangling pointer. */
static NOINLINE char* first_round(size_t size, char* q)
{
    long page = sysconf(_SC_PAGESIZE);

    /* the exact address comes back, in a usable object */
    char* p = zgc_reserve_at(base, size);
    CHECK(p);
    CHECK((unsigned long)p == base);
    CHECK(zlength(p) == size);
    p[0] = 1;
    p[size - 1] = 2;
    CHECK(p[0] == 1 && p[size - 1] == 2);
    memset(p, 0x5a, size);
    CHECK(p[CHUNK + 5] == 0x5a);

    /* it is an ordinary mmap object: it can take pointers */
    char** pp = (char**)(p + page);
    *pp = p;
    CHECK(*pp == p);

    /* the range and what the runtime keeps around it are taken */
    CHECK(!zgc_reserve_at(base, size));
    CHECK(errno == EEXIST);
    errno = 0;
    CHECK(!zgc_reserve_at(base + CHUNK, CHUNK));
    CHECK(errno == EEXIST);
    errno = 0;
    CHECK(!zgc_reserve_at(base - CHUNK, 2 * CHUNK));
    CHECK(errno == EEXIST);
    errno = 0;
    CHECK(!zgc_reserve_at(base + size - page, CHUNK));
    CHECK(errno == EEXIST);

    /* another range is a separate object */
    CHECK(p[0] == 0x5a);
    q[0] = 7;
    CHECK(p[0] == 0x5a);

    /* an access past the end traps in a child */
    pid_t pid = fork();
    CHECK(pid >= 0);
    if (!pid) {
        volatile char* e = p;
        e[size] = 1;
        _exit(0);
    }
    int status;
    CHECK(waitpid(pid, &status, 0) == pid);
    CHECK(!WIFEXITED(status) || WEXITSTATUS(status) != 0);

    CHECK(munmap(p, size) == 0);
    return p;
}

/* The address comes back, fresh, in the same process. */
static NOINLINE void second_round(size_t size, char* q)
{
    char* r = reserve_after_gc(base, size);
    CHECK(r);
    CHECK((unsigned long)r == base);
    for (size_t i = 0; i < size; i += 1021)
        CHECK(r[i] == 0);
    r[0] = 9;
    r[size - 1] = 10;
    CHECK(q[0] == 7);

    /* the neighbours are still protected */
    errno = 0;
    CHECK(!zgc_reserve_at(base + CHUNK, CHUNK));
    CHECK(errno == EEXIST);
    CHECK(munmap(r, size) == 0);
}

/* A range inside a retired span works. */
static NOINLINE void third_round(void)
{
    char* r2 = reserve_after_gc(base + 2 * CHUNK, CHUNK);
    CHECK(r2);
    CHECK((unsigned long)r2 == base + 2 * CHUNK);
    r2[CHUNK - 1] = 1;
    CHECK(munmap(r2, CHUNK) == 0);
}

/* A file mapped over a part of the object. */
static NOINLINE void file_round(char* q)
{
    long page = sysconf(_SC_PAGESIZE);
    FILE* f = tmpfile();
    CHECK(f);
    char buf[16384];
    memset(buf, 0x33, sizeof(buf));
    CHECK(fwrite(buf, 1, sizeof(buf), f) == sizeof(buf));
    fflush(f);
    size_t maplen = 4 * page;
    void* m = mmap(q + CHUNK, maplen, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fileno(f), 0);
    CHECK(m == q + CHUNK);
    CHECK(((char*)m)[0] == 0x33);
    fclose(f);
}

static NOINLINE void bad_requests(unsigned long other)
{
    long page = sysconf(_SC_PAGESIZE);

    errno = 0;
    CHECK(!zgc_reserve_at(0, page));
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(other, 0));
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(other + 1, page));
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(other, page + 1));
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(other + page, page)); /* its header would be in the mark bit page */
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(~0ul - page + 1, 2 * page));
    CHECK(errno == EINVAL);
    errno = 0;
    CHECK(!zgc_reserve_at(1ul << 48, page));
    CHECK(errno == EINVAL);
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    size_t size = 4 * CHUNK + 3 * page;
    unsigned long other = base + 64 * CHUNK;

    /* a mapping that is not ours is not replaced: its bytes are untouched */
    unsigned char* occ = mmap(NULL, CHUNK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK(occ != MAP_FAILED);
    memset(occ, 0x77, CHUNK);

    bad_requests(other);

    char* q = zgc_reserve_at(other, 2 * CHUNK);
    CHECK(q);
    CHECK((unsigned long)q == other);
    file_round(q);

    /* unmapping frees it; the other one is not affected */
    char* volatile stale = first_round(size, q);
    CHECK(q[0] == 7);

    /* while something still refers to the old object the address stays taken, so a stale pointer
       can never see the new object */
    zgc_wait(zgc_request_fresh());
    zgc_wait(zgc_request_fresh());
    zgc_wait(zgc_request_fresh());
    errno = 0;
    CHECK(!zgc_reserve_at(base, size));
    CHECK(errno == EEXIST);

    /* once nothing refers to it, the address can be reserved again, in this process */
    stale = NULL;
    second_round(size, q);
    third_round();

    CHECK(munmap(q, 2 * CHUNK) == 0);
    zgc_wait(zgc_request_fresh());

    /* the occupied mapping never changed */
    for (size_t i = 0; i < CHUNK; i++)
        CHECK(occ[i] == 0x77);

    printf("reserveat ok\n");
    return 0;
}
