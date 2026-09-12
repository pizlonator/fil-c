#include <stdio.h>
#include <stdlib.h>

unsigned long *cmov_static_take(unsigned long *a, long sel);
unsigned long *cmov_keep_dest(unsigned long *a, long sel);
unsigned long cmov_merge_load(unsigned long *a, unsigned long *b, long sel);
void cmov_merge_store(unsigned long *a, unsigned long *b, long sel, unsigned long v);

int main(void)
{
    unsigned long *a = malloc(32);
    unsigned long *b = malloc(32);
    if (!a || !b)
        return 1;
    a[0] = 111;
    b[0] = 222;

    /* Static take: sel!=0 adopts a's capability (dereferenceable);
       sel==0 keeps the null value (no capability, no deref). */
    unsigned long *r = cmov_static_take(a, 1);
    if (r != a || r[0] != 111) {
        printf("FAIL static take\n");
        return 1;
    }
    r = cmov_static_take(a, 0);
    if (r != (unsigned long *)0) {
        printf("FAIL static untake %p\n", (void *)r);
        return 1;
    }

    /* Dest keeps its capability when the scalar source loses. */
    r = cmov_keep_dest(a, 0);
    if (r != a || r[0] != 111) {
        printf("FAIL keep dest\n");
        return 1;
    }

    /* Dynamic merge: the in-asm dereference follows the taken path. */
    if (cmov_merge_load(a, b, 0) != 111 || cmov_merge_load(a, b, 7) != 222) {
        printf("FAIL merge load\n");
        return 1;
    }
    cmov_merge_store(a, b, 0, 333);
    cmov_merge_store(a, b, 9, 444);
    if (a[0] != 333 || b[0] != 444) {
        printf("FAIL merge store %lu %lu\n", a[0], b[0]);
        return 1;
    }

    printf("cmov cap int ok\n");
    return 0;
}
