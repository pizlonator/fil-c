#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

unsigned long add_shr_base(unsigned long *base, unsigned long idx);
unsigned long add_usecap(unsigned long *a, unsigned long *b);
unsigned long sub_usecap(unsigned long *p, unsigned long off);
unsigned long *lea_usecap_idx(unsigned long *z, unsigned long *buf);
unsigned long shl_count_nocap(unsigned long *a, unsigned long *b);

int main(void)
{
    unsigned long *t = malloc(64);
    unsigned long *a = malloc(128);
    unsigned long *b = malloc(32);
    unsigned long *z = malloc(32);
    unsigned long *buf = malloc(32);
    if (!t || !a || !b || !z || !buf)
        return 1;
    for (int i = 0; i < 8; i++)
        t[i] = 100 + i;
    for (int i = 0; i < 16; i++)
        a[i] = 1000 + i;
    b[0] = 222;
    buf[0] = 333;

    /* shr $3 of 128 is 16: t+16 is t[2]. */
    if (add_shr_base(t, 128) != 102) {
        printf("FAIL add_shr_base\n");
        return 1;
    }
    /* rax = b-b = 0, so rdi stays a: the annotation keeps a's cap. */
    if (add_usecap(a, b) != 1000) {
        printf("FAIL add_usecap\n");
        return 1;
    }
    /* &t[2]-16 is t[0]; the annotation is redundant but must validate. */
    if (sub_usecap(&t[2], 16) != 100) {
        printf("FAIL sub_usecap\n");
        return 1;
    }
    /* Stale-zeroed base + live index: the lea keeps the index's cap. */
    unsigned long *r = lea_usecap_idx(z, buf);
    if (r != buf || r[0] != 333) {
        printf("FAIL lea_usecap_idx\n");
        return 1;
    }
    /* 8 << ((uintptr_t)b & 3), added to a: the shift result is scalar,
       so the add keeps a's capability and the load lands in bounds. */
    {
        unsigned long k = ((uintptr_t)b & 3);
        unsigned long want = a[1UL << k];
        if (shl_count_nocap(a, b) != want) {
            printf("FAIL shl_count_nocap\n");
            return 1;
        }
    }

    printf("add cap att ok\n");
    return 0;
}
