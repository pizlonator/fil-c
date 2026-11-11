#include <stdio.h>
#include <stdlib.h>
#include <cpuid.h>
long flagjcc(long* b, long n);
long flagbits(long* b, long x, long y, long sub);
long flagadx(long* b, long flags);
int main() {
    long* buf = malloc(16);
    buf[0] = 7;
    buf[1] = 9;
    // The program's `cmpq $5` is live ACROSS the checked load: the injected
    // bounds check must save/restore EFLAGS so the `jg` sees the program's
    // comparison, not the check's residue. n=3 takes the small path (0),
    // n=10 the big path (1).
    printf("%ld %ld\n", flagjcc(buf, 3), flagjcc(buf, 10));
    // Exhaust all byte additions/subtractions, including independent CF and OF
    // values, and check SF/ZF/AF/PF too. Expected flags come from C arithmetic.
    for (int sub = 0; sub < 2; ++sub)
        for (int x = 0; x < 256; ++x)
            for (int y = 0; y < 256; ++y) {
                int full = sub ? x - y : x + y;
                int r = full & 255;
                int parity = 1;
                for (int k = 0; k < 8; ++k) parity ^= (r >> k) & 1;
                int overflow = sub ? ((x ^ y) & (x ^ r) & 128) != 0
                                   : ((~(x ^ y)) & (x ^ r) & 128) != 0;
                long flags = (sub ? x < y : full > 255) | (parity << 2)
                    | ((x ^ y ^ r) & 16) | ((r == 0) << 6)
                    | (r & 128) | (overflow << 8);
                long got = flagbits(buf, x, y, sub);
                if (got != (flags ^ buf[0])) {
                    printf("flags BAD: %d %d %d: %ld != %ld\n",
                           sub, x, y, got, flags ^ buf[0]);
                    return 1;
                }
            }
    printf("all arithmetic flags ok\n");
    unsigned eax, ebx, ecx, edx;
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx) && (ebx & (1u << 19))) {
        long inputs[4] = {1, -1, 5, 7};
        for (int cf = 0; cf < 2; ++cf)
            for (int of = 0; of < 2; ++of) {
                long expected = ((6L + cf) << 32) | (of ? 8 : 6)
                    | ((long)!of << 16);
                if (flagadx(inputs, 2 | cf | (of << 11)) != expected) {
                    printf("dual carry BAD: CF=%d OF=%d\n", cf, of);
                    return 1;
                }
            }
        printf("dual carry chains ok\n");
    }
    return 0;
}
