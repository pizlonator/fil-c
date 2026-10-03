#include <stdio.h>

long pi_interleave(long a);
long pi_nested(long a);
long pi_deep(long a, long *unused);
long pi_word_below(long a);

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
    long dummy = 0;
    /* interleave: the flags word sits below %r10's word. */
    expect("pushfq pop interleave ok", pi_interleave(0), 2);
    expect("pushfq pop interleave ne ok", pi_interleave(4), 1);

    /* nested pairs: the first popfq restores the inner (ZF=0) word, the
     * second the outer (ZF <- (a == 0)) word; a swap reports 112. */
    expect("pushfq nested pairs ok", pi_nested(0), 12 + 200);
    expect("pushfq nested pairs ne ok", pi_nested(9), 12);

    /* deep interleave: two register words pushed over the flags word. */
    expect("pushfq deep interleave ok", pi_deep(0, &dummy), 77);
    expect("pushfq deep interleave ne ok", pi_deep(3, &dummy), 1077);

    /* word below: popping the flags word into a register. */
    expect("pushfq word below ok", pi_word_below(0), 64);
    expect("pushfq word below ne ok", pi_word_below(1), 0);

    return fails != 0;
}
