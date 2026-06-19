#include <stdfil.h>

typedef signed char v16qi __attribute__((vector_size(16)));
typedef short v8hi __attribute__((vector_size(16)));
typedef int v4si __attribute__((vector_size(16)));
typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PMOVSXBW: sign-extend 8 packed bytes (low 8 bytes) to 8 words. */
    {
        v16qi a = { -128, 127, -1, 0, 1, -2, 16, -112,
                    0, 0, 0, 0, 0, 0, 0, 0 };
        v8hi r;
        asm volatile("pmovsxbw %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -128);
        ZASSERT(r[1] == 127);
        ZASSERT(r[2] == -1);
        ZASSERT(r[3] == 0);
        ZASSERT(r[4] == 1);
        ZASSERT(r[5] == -2);
        ZASSERT(r[6] == 16);
        ZASSERT(r[7] == -112);
    }

    /* PMOVSXBD: sign-extend 4 packed bytes (low 4 bytes) to 4 dwords. */
    {
        v16qi a = { -128, 127, -1, 5, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0 };
        v4si r;
        asm volatile("pmovsxbd %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -128);
        ZASSERT(r[1] == 127);
        ZASSERT(r[2] == -1);
        ZASSERT(r[3] == 5);
    }

    /* PMOVSXBQ: sign-extend 2 packed bytes (low 2 bytes) to 2 qwords. */
    {
        v16qi a = { -128, 127, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0 };
        v2di r;
        asm volatile("pmovsxbq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -128);
        ZASSERT(r[1] == 127);
    }

    /* PMOVSXWD: sign-extend 4 packed words (low 8 bytes) to 4 dwords. */
    {
        v8hi a = { -30000, 30000, -1, 7, 0, 0, 0, 0 };
        v4si r;
        asm volatile("pmovsxwd %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -30000);
        ZASSERT(r[1] == 30000);
        ZASSERT(r[2] == -1);
        ZASSERT(r[3] == 7);
    }

    /* PMOVSXWQ: sign-extend 2 packed words (low 4 bytes) to 2 qwords. */
    {
        v8hi a = { -30000, 30000, 0, 0, 0, 0, 0, 0 };
        v2di r;
        asm volatile("pmovsxwq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -30000);
        ZASSERT(r[1] == 30000);
    }

    /* PMOVSXDQ: sign-extend 2 packed dwords (low 8 bytes) to 2 qwords. */
    {
        v4si a = { -1000000000, 1000000000, 0, 0 };
        v2di r;
        asm volatile("pmovsxdq %1, %0" : "=x"(r) : "x"(a));
        ZASSERT(r[0] == -1000000000);
        ZASSERT(r[1] == 1000000000);
    }

    return 0;
}
