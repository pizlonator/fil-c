#include <stdio.h>
#include <stdlib.h>

long g_small[2] = {7, 8};

long two_objects(long *small, long *big);
long two_objects_loop(long *a, long n);

int main(void)
{
    long *big = malloc(64);
    if (!big)
        return 1;
    for (int i = 0; i < 8; i++)
        big[i] = 100 + i;
    /* 103 (big[5]) + 7 (small[0]) + 107 (big[7]). The deep big-object derefs
       are out of reach of the small object's capability, so a capability that
       failed to track the store sequence would trap instead of returning. */
    long expect = big[5] + g_small[0] + big[7];
    long got = two_objects(g_small, big);
    if (got != expect) {
        printf("two_objects got %ld expect %ld\n", got, expect);
        return 1;
    }
    long a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    if (two_objects_loop(a, 8) != 36) {
        printf("two_objects_loop got %ld\n", two_objects_loop(a, 8));
        return 1;
    }
    printf("slot storeptr two objects ok\n");
    return 0;
}
