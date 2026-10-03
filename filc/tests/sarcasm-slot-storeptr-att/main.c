#include <stdio.h>
#include <stdlib.h>

long slot_roundtrip(long *p);
long slot_roundtrip_rsp(long *p);

int main(void)
{
    long *p = malloc(64);
    if (!p)
        return 1;
    p[0] = 111;
    p[1] = 222;
    if (slot_roundtrip(p) != 111) {
        printf("slot_roundtrip got %ld\n", slot_roundtrip(p));
        return 1;
    }
    if (slot_roundtrip_rsp(p) != 222) {
        printf("slot_roundtrip_rsp got %ld\n", slot_roundtrip_rsp(p));
        return 1;
    }
    printf("slot storeptr ok\n");
    return 0;
}
