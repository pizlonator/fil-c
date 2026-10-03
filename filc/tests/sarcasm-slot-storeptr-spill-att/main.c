#include <stdio.h>
#include <stdlib.h>

long spill_roundtrip(long *p);

int main(void)
{
    long *p = malloc(64);
    if (!p)
        return 1;
    p[0] = 55;
    long expect = p[0]
        + 0x1000 + 0x1001 + 0x1002 + 0x1003 + 0x1004
        + (0x100a + 0x2000) + (0x100b + 0x2000) + (0x100c + 0x2000)
        + (0x100d + 0x2000)
        + 0x1005 + 0x1006 + 0x1007 + 0x1008 + 0x1009;
    long got = spill_roundtrip(p);
    if (got != expect) {
        printf("spill_roundtrip got %ld expect %ld\n", got, expect);
        return 1;
    }
    printf("slot storeptr spill ok\n");
    return 0;
}
