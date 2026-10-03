#include <stdio.h>

long pi_eq_restore(long a, long b);
long pi_bare(long a);
long pi_from_reg(long a);

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
    expect("pushfd intel restore eq ok", pi_eq_restore(0, 7), 7 + 18 + 111);
    expect("pushfd intel restore ne ok", pi_eq_restore(5, 7), 7 + 18);

    expect("pushf intel bare spellings ok", pi_bare(0), 2);
    expect("pushf intel bare spellings ne ok", pi_bare(9), 1);

    expect("popfd intel from register ok", pi_from_reg(0), 2);
    expect("popfd intel from register ne ok", pi_from_reg(1), 1);

    if (fails) return 1;
    printf("pushfq intel all ok\n");
    return 0;
}
