#include <stdfil.h>

/* Test that every SETcc condition variant assembles and executes. */
#define TEST_SETCC(cc) do {                          \
    unsigned char result = 0;                        \
    int a = 5, b = 5;                                \
    asm volatile("cmpl %1, %2\n\t"                   \
                 "set" #cc " %0"                     \
                 : "=q"(result)                      \
                 : "r"(a), "r"(b)                    \
                 : "cc");                            \
} while (0)

int main(void)
{
    TEST_SETCC(o);
    TEST_SETCC(no);
    TEST_SETCC(b);
    TEST_SETCC(c);
    TEST_SETCC(nae);
    TEST_SETCC(nb);
    TEST_SETCC(nc);
    TEST_SETCC(ae);
    TEST_SETCC(e);
    TEST_SETCC(z);
    TEST_SETCC(ne);
    TEST_SETCC(nz);
    TEST_SETCC(be);
    TEST_SETCC(na);
    TEST_SETCC(nbe);
    TEST_SETCC(a);
    TEST_SETCC(s);
    TEST_SETCC(ns);
    TEST_SETCC(p);
    TEST_SETCC(pe);
    TEST_SETCC(np);
    TEST_SETCC(po);
    TEST_SETCC(l);
    TEST_SETCC(nge);
    TEST_SETCC(nl);
    TEST_SETCC(ge);
    TEST_SETCC(le);
    TEST_SETCC(ng);
    TEST_SETCC(nle);
    TEST_SETCC(g);

    /* Verify correctness: equal case (5 == 5). */
    {
        unsigned char eq, ne, lt, gt;
        int a = 5, b = 5;
        /* cmpl src, dst computes dst - src; here a - b. */
        asm volatile("cmpl %5, %4\n\t"
                     "sete %0\n\t"
                     "setne %1\n\t"
                     "setl %2\n\t"
                     "setg %3"
                     : "=q"(eq), "=q"(ne), "=q"(lt), "=q"(gt)
                     : "r"(a), "r"(b)
                     : "cc");
        ZASSERT(eq == 1);
        ZASSERT(ne == 0);
        ZASSERT(lt == 0);
        ZASSERT(gt == 0);
    }

    /* Verify correctness: less-than case (3 < 5). */
    {
        unsigned char eq, ne, lt, gt;
        int a = 3, b = 5;
        asm volatile("cmpl %5, %4\n\t"
                     "sete %0\n\t"
                     "setne %1\n\t"
                     "setl %2\n\t"
                     "setg %3"
                     : "=q"(eq), "=q"(ne), "=q"(lt), "=q"(gt)
                     : "r"(a), "r"(b)
                     : "cc");
        ZASSERT(eq == 0);
        ZASSERT(ne == 1);
        ZASSERT(lt == 1);
        ZASSERT(gt == 0);
    }

    /* Verify correctness: greater-than case (7 > 5). */
    {
        unsigned char eq, ne, lt, gt;
        int a = 7, b = 5;
        asm volatile("cmpl %5, %4\n\t"
                     "sete %0\n\t"
                     "setne %1\n\t"
                     "setl %2\n\t"
                     "setg %3"
                     : "=q"(eq), "=q"(ne), "=q"(lt), "=q"(gt)
                     : "r"(a), "r"(b)
                     : "cc");
        ZASSERT(eq == 0);
        ZASSERT(ne == 1);
        ZASSERT(lt == 0);
        ZASSERT(gt == 1);
    }

    return 0;
}
