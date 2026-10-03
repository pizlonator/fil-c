#include <stdio.h>

long lc_top(long a, long b);

int main() {
    // Three localcalls into the subroutine's clone, whose tab lives in the
    // clone's own sub-frame: (3,5) + (6,7) + (15,0) ->
    // (30+50) + (60+70) + (150+0) = 360.
    if (lc_top(3, 5) != 360) return 1;
    printf("stackbuf localcall att ok\n");
    return 0;
}
