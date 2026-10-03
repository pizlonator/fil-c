#include <stdio.h>

long pf_eq_restore(long a, long b);
long pf_s_restore(long a, long b);
long pf_c_restore(long a, long b);
long pf_identity(long a);
long pf_reg_roundtrip(long a);
long pf_word_readback(long a);

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
    /* eq restore: ZF from (a == 0) survives the clobbers. The clobbered %rsi
     * comes back as b + 18 in both directions, so the branch direction (not
     * the arithmetic) is what the outputs pin. */
    expect("pushfq restore eq ok", pf_eq_restore(0, 7), 7 + 18 + 111);
    expect("pushfq restore ne ok", pf_eq_restore(5, 7), 7 + 18);

    /* sign restore: SF from bit 63 of a (1 << 63 has bit 63 set -> SF=1). */
    expect("pushfq restore s ok", pf_s_restore(1, 7), 7 + 17 + 222);
    expect("pushfq restore ns ok", pf_s_restore(0, 7), 7 + 17);

    /* carry restore: CF from the unsigned borrow of (a - 16). */
    expect("pushfq restore c ok", pf_c_restore(3, 7), 7 + 17 + 333);
    expect("pushfq restore nc ok", pf_c_restore(20, 7), 7 + 17);

    /* identity: pushfq immediately followed by popfq. */
    expect("pushfq immediate identity ok", pf_identity(0), 0);
    expect("pushfq immediate identity ne ok", pf_identity(9), 1);

    /* register round-trip: the word parks in %r10 across flag-writing code
     * (the clobbering add/cmp results are returned, so DCE keeps them). */
    expect("pushfq register roundtrip e ok", pf_reg_roundtrip(0), 1000 + 22);
    expect("pushfq register roundtrip ne ok", pf_reg_roundtrip(2), 1002 + 11);

    /* word readback: pushfq; pop %r11 exposes the saved ZF bit as an integer. */
    expect("pushfq word readback ok", pf_word_readback(0), 64);
    expect("pushfq word readback ne ok", pf_word_readback(1), 0);

    return fails != 0;
}
