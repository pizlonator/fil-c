#include <stdio.h>
long midandsave_test(long a, long b);
long midandsave16_test(long a, long b);
long midandsave64_test(long a, long b);
long midandsaveadd_test(long a, long b);
// Sweep call-stack depths so the entry rsp hits every mod-64 residue: each
// variant's `and $-N` slack (0..N-1) varies per residue, and any
// static-displacement miscompilation of the post-and slots (or of the
// carrier restore / rsp recovery) fails deterministically. 128 levels cover
// every mod-64 residue twice over (and every mod-16/mod-32 residue eight /
// four times).
static int check_one(long (*fn)(long, long), const char *name, int depth) {
    long a = 40 + depth, b = 2;
    long r = fn(a, b);
    if (r != a + b) {
        printf("midandsave BAD %s at depth %d: %ld != %ld\n", name, depth, r, a + b);
        return 0;
    }
    // nonzero first arg takes the bulk path; zero takes the early ret.
    r = fn(0, b);
    if (r != 0) {
        printf("midandsave BAD %s early-ret at depth %d: %ld != 0\n", name, depth, r);
        return 0;
    }
    return 1;
}
static int check_depth(int depth) {
    int ok = check_one(midandsave_test, "and32", depth)
        & check_one(midandsave16_test, "and16", depth)
        & check_one(midandsave64_test, "and64", depth)
        & check_one(midandsaveadd_test, "and32add", depth);
    if (!ok) return 0;
    if (depth >= 128) return 1;
    return check_depth(depth + 1);
}
int main() {
    int ok = check_depth(0);
    printf("midandsave %s\n", ok ? "ok" : "BAD");
    return 0;
}
