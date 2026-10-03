#include <stdio.h>
#include <string.h>

extern long misalignesc(long x);

void pokew(long *p)
{
    *p = 0x0102030405060708L;   /* misaligned qword write at region+17 */
}

/* Pure-C mirror of misalignesc's asm sequence. R models the promoted frame's
   region (one GC allocation) byte-granularly; every asm access maps onto the
   same bytes, misalignment included. */
static long mirror(void)
{
    unsigned char R[128];
    unsigned long acc = 0;
    unsigned long long q;
    unsigned int l;
    unsigned short w;
    memset(R, 0, sizeof R);
    q = 0x0102030405060708ULL;
    memcpy(R + 17, &q, 8);      /* pokew(R+17) */
    memcpy(R + 17, &q, 8);      /* direct misaligned movq over the same home */
    memcpy(&q, R + 17, 8);
    acc += q;                   /* pointer read */
    memcpy(&q, R + 17, 8);
    acc += q;                   /* direct read */
    for (int i = 0; i < 8; i++)
        acc += R[17 + i];       /* byte decomposition: 8+7+...+1 = 36 */
    R[40] = 0xA1;
    w = 0xB2B3;
    memcpy(R + 41, &w, 2);
    l = 0xC4C5C6C7;
    memcpy(R + 43, &l, 4);
    q = 0x1112131415161718ULL;
    memcpy(R + 47, &q, 8);
    acc += R[40];
    memcpy(&w, R + 41, 2);
    acc += w;
    memcpy(&l, R + 43, 4);
    acc += l;
    memcpy(&q, R + 47, 8);
    acc += q;
    R[65] = 0x77;
    w = 0x1234;
    memcpy(R + 66, &w, 2);
    l = 0x9ABCDEF0;
    memcpy(R + 69, &l, 4);
    acc += R[65];
    memcpy(&w, R + 66, 2);
    acc += w;
    memcpy(&l, R + 69, 4);
    acc += l;
    return (long)acc;
}

int main(void)
{
    long want = mirror();
    if (want != 1375314350677790978L) {
        printf("FAIL: mirror self-check got %ld, want 1375314350677790978\n",
               want);
        return 1;
    }
    for (int i = 0; i < 2; i++) {
        long r = misalignesc(0);
        if (r != 1375314350677790978L) {
            printf("FAIL: got %ld, want 1375314350677790978\n", r);
            return 1;
        }
    }
    printf("frame escape misalign att ok\n");
    return 0;
}
