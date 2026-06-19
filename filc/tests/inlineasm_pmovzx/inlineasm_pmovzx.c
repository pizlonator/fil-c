#include <stdfil.h>

typedef unsigned char v16uqi __attribute__((vector_size(16)));
typedef unsigned short v8uhi __attribute__((vector_size(16)));
typedef unsigned int v4usi __attribute__((vector_size(16)));
typedef unsigned long long v2udi __attribute__((vector_size(16)));

int main(void)
{
    /* PMOVZXBW: zero-extend 8 packed bytes (low 8 bytes) to 8 words. */
    {
        v16uqi a = { 0x80, 0x7f, 0xff, 0x00, 0x01, 0xfe, 0x10, 0x90,
                     0, 0, 0, 0, 0, 0, 0, 0 };
        v8uhi r;
        asm volatile("pmovzxbw %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x0080);
        ZASSERT(r[1] == 0x007f);
        ZASSERT(r[2] == 0x00ff);
        ZASSERT(r[3] == 0x0000);
        ZASSERT(r[4] == 0x0001);
        ZASSERT(r[5] == 0x00fe);
        ZASSERT(r[6] == 0x0010);
        ZASSERT(r[7] == 0x0090);
    }

    /* PMOVZXBD: zero-extend 4 packed bytes (low 4 bytes) to 4 dwords. */
    {
        v16uqi a = { 0x80, 0x7f, 0xff, 0x05, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0 };
        v4usi r;
        asm volatile("pmovzxbd %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x00000080u);
        ZASSERT(r[1] == 0x0000007fu);
        ZASSERT(r[2] == 0x000000ffu);
        ZASSERT(r[3] == 0x00000005u);
    }

    /* PMOVZXBQ: zero-extend 2 packed bytes (low 2 bytes) to 2 qwords. */
    {
        v16uqi a = { 0x80, 0x7f, 0, 0, 0, 0, 0, 0,
                     0, 0, 0, 0, 0, 0, 0, 0 };
        v2udi r;
        asm volatile("pmovzxbq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x0000000000000080ULL);
        ZASSERT(r[1] == 0x000000000000007fULL);
    }

    /* PMOVZXWD: zero-extend 4 packed words (low 8 bytes) to 4 dwords. */
    {
        v8uhi a = { 0x8000, 0x7fff, 0xffff, 0x0007, 0, 0, 0, 0 };
        v4usi r;
        asm volatile("pmovzxwd %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x00008000u);
        ZASSERT(r[1] == 0x00007fffu);
        ZASSERT(r[2] == 0x0000ffffu);
        ZASSERT(r[3] == 0x00000007u);
    }

    /* PMOVZXWQ: zero-extend 2 packed words (low 4 bytes) to 2 qwords. */
    {
        v8uhi a = { 0x8000, 0x7fff, 0, 0, 0, 0, 0, 0 };
        v2udi r;
        asm volatile("pmovzxwq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x0000000000008000ULL);
        ZASSERT(r[1] == 0x0000000000007fffULL);
    }

    /* PMOVZXDQ: zero-extend 2 packed dwords (low 8 bytes) to 2 qwords. */
    {
        v4usi a = { 0x80000000u, 0x7fffffffu, 0, 0 };
        v2udi r;
        asm volatile("pmovzxdq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == 0x0000000080000000ULL);
        ZASSERT(r[1] == 0x000000007fffffffULL);
    }

    return 0;
}
