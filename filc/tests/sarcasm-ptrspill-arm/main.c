#include <stdio.h>

/* 24 `;! load ptr` webs kept live across a bl call forces paired (iv,lo)
   spills plus GC root slots and reload-after-call. The weighted sum
   (k * pointee_k) catches any web permutation or lost reload. */
long load24(void** ptrs);

int main()
{
    static long vals[24];
    void* ptrs[24];
    for (long i = 0; i < 24; i++) {
        vals[i] = i + 1;
        ptrs[i] = &vals[i];
    }
    printf("%ld\n", load24(ptrs));   /* sum of k^2 for k=1..24 = 4900 */
    return 0;
}
