#include <stdio.h>

/* Post-index writeback on annotated ptr loads/stores: walking an array of
   pointers with ldr ..., [x0], #8 must advance the base capability each
   iteration; same for the fill loop's str ..., [x0], #8. */
long sum(void** pp, long n);
void fill(void** buf, void* v, long n);
long sumpre(void** pp, long n);
void fillpre(void** buf, void* v, long n);

int main()
{
    long a = 42, b = 32;
    void* arr[2];
    arr[0] = &a;
    arr[1] = &b;
    /* 74 only if the walk really advances to arr[1] on iteration two. */
    printf("%ld %ld %ld\n", sum(arr, 2), sum(arr, 1), sum(arr + 1, 1));
    long x = 1;
    void* buf[3];
    fill(buf, &x, 3);
    printf("%ld %ld %ld\n", *(long*)buf[0], *(long*)buf[1], *(long*)buf[2]);
    /* Pre-index twin: [x0, #8]! advances the base BEFORE the access, so the
       walk starts one slot past the incoming pointer (pass arr-1 / buf2-1). */
    printf("%ld %ld\n", sumpre(arr - 1, 2), sumpre(arr, 1));  /* 74 32 */
    void* buf2[3];
    fillpre(buf2 - 1, &x, 3);
    printf("%ld %ld %ld\n", *(long*)buf2[0], *(long*)buf2[1], *(long*)buf2[2]);
    return 0;
}
