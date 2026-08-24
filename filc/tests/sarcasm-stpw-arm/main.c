#include <stdio.h>

/* 32-bit stp/ldp on frame slots: the pair elements live at stride 4, so a
   w-load at [sp, #4] must read the SECOND stored element (the stride fix),
   and a sub sp, sp, #8 frame must accept a whole w-pair. */
long stpw_second(long a, long b);  /* stp w0,w1,[sp]; ldr w0,[sp,#4] -> b */
long stpw_both(long a, long b);    /* reload both singly -> a + b */
long stpw_sum(long a, long b);     /* 8-byte frame, stp/ldp w-pair -> a + b */

int main()
{
    printf("%ld\n", stpw_second(111, 222));   /* 222 */
    printf("%ld\n", stpw_both(7, 8));         /* 15 */
    printf("%ld\n", stpw_sum(1000, 2000));    /* 3000 */
    return 0;
}
