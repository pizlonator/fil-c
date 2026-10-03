#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long slot_across_call(long *keep, long *obj);
long churn(long *p);

int main(void)
{
    long keep[2] = {11, 0};
    long *obj = malloc(64);
    if (!obj)
        return 1;
    obj[0] = 1000;
    obj[1] = 200;
    /* Repeat with allocation churn so a GC (stop-the-world in some sub-runs)
       has to see the slot-held pointer's root while the caller is parked in
       or around churn(). The object must stay alive and the capability must
       still check out after every call. */
    for (int i = 0; i < 2000; i++) {
        obj[0] = 1000 + i;
        long expect = obj[0] + obj[1] + keep[0];
        long got = slot_across_call(keep, obj);
        if (got != expect) {
            printf("slot_across_call got %ld expect %ld (i=%d)\n", got, expect, i);
            return 1;
        }
        void *noise = malloc(64);
        if (!noise)
            return 1;
        memset(noise, 1, 64);
        free(noise);
    }
    printf("slot storeptr call ok\n");
    return 0;
}
