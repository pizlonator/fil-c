#include <stdio.h>
#include <stdlib.h>
#include <string.h>

long pc_loads(long *p, long sel);
long pc_stores(long *p, long sel);
long pc_loop(long *p, long n, long sel);
long pc_df_checks(void *dst, void *src, long n);

/* asm-driven `std` wrapper: simulates the ABI-violating caller that leaves
 * DF=1 across the call (System V requires DF=0; sarcasm never emits std, and
 * Fil-C's C compiler cannot emit it either, so the DF comes from assembly).
 * The trailing cld is test hygiene so the harness keeps running with DF=0
 * after the surviving call returns with DF=1 (pc_df_checks' popfq restores
 * the caller's DF=1). */
void pc_std_call(void *dst, void *src, long n);

static int fails = 0;

static void expect(const char *what, long got, long want) {
    if (got != want) {
        printf("%s BAD (got %ld want %ld)\n", what, got, want);
        fails++;
    } else {
        printf("%s ok\n", what);
    }
}

int main() {
    long v[4] = {10, 20, 30, 40};
    long out[4] = {0, 0, 0, 0};

    /* checked loads under the flags word: the injected checks preserve the
     * pending condition, so the popfq-restored ZF decides the branch. */
    expect("pushfq over checked loads ok", pc_loads(v, 0), 10 + 20 + 100);
    expect("pushfq over checked loads ne ok", pc_loads(v, 1), 10 + 20);

    /* checked stores under the flags word. */
    memset(out, 0, sizeof out);
    expect("pushfq over checked stores ok", pc_stores(out, 0), 5);
    if (out[0] != 0 || out[1] != 0) {
        printf("pushfq over checked stores BAD (stores)\n");
        fails++;
    }
    memset(out, 0, sizeof out);
    expect("pushfq over checked stores ne ok", pc_stores(out, 1), 0);
    if (out[0] != 1 || out[1] != 1) {
        printf("pushfq over checked stores BAD (stores2)\n");
        fails++;
    }

    /* a checked-load loop (bounds checks + the back-edge pollcheck) under the
     * flags word. */
    expect("pushfq over checked loop ok", pc_loop(v, 4, 4), 100 + 1000);
    expect("pushfq over checked loop neg ok", pc_loop(v, 4, -4), 100);
    expect("pushfq over checked loop zero ok", pc_loop(v, 0, 0), 0);

    /* DF restore across a checked rep: the caller's DF=1 (set by assembly
     * below) must come back after the popfq, and the cld inside must make the
     * copy run FORWARD (no DF=1 trap from the checked rep). */
    memset(out, 0xAA, sizeof out);
    pc_std_call(out, v, 16);
    if (memcmp(out, v, 16) != 0) {
        printf("pushfq over df-clobbering checks BAD (bytes)\n");
        fails++;
    } else {
        printf("pushfq over df-clobbering checks ok\n");
    }

    /* the same helper entered with DF=0 (the normal ABI) still copies. */
    memset(out, 0, sizeof out);
    expect("pushfq over df-clobbering checks df0 ok", pc_df_checks(out, v, 16), 16);
    if (memcmp(out, v, 16) != 0) {
        printf("pushfq over df-clobbering checks df0 BAD (bytes)\n");
        fails++;
    }

    if (fails) return 1;
    printf("pushfq checks all ok\n");
    return 0;
}
