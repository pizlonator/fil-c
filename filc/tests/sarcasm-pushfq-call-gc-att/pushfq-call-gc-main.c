#include <stdio.h>
#include <stdlib.h>

void pg_churn(long n);
void pg_clobber(void);
long pg_caller(long sel, long n);
long pg_caller_reg(long sel, long n);
long pg_nested_call(long a);

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
    /* the flags word survives a call whose callee allocates/frees a lot (GC
     * pass plausible) and clobbers the flags. */
    expect("pushfq across call ok", pg_caller(0, 4000), 2);
    expect("pushfq across call ne ok", pg_caller(1, 4000), 1);

    /* the word rides a caller-saved register the callee clobbers, so it must
     * have been moved somewhere callee-safe; a GC pass during the churn is
     * again plausible. */
    expect("pushfq callee-clobbered flags ok", pg_caller_reg(0, 4000), 2);
    expect("pushfq callee-clobbered flags ne ok", pg_caller_reg(1, 4000), 1);

    /* two distinct parked words in one frame (inner vs outer restore). */
    expect("pushfq nested call ok", pg_nested_call(0), 2);
    expect("pushfq nested call ne ok", pg_nested_call(3), 1);
    expect("pushfq nested call five ok", pg_nested_call(5), 3);

    if (fails) return 1;
    printf("pushfq call-gc all ok\n");
    return 0;
}
