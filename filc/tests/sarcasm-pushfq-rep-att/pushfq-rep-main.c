#include <stdio.h>
#include <string.h>

void cam_enc_tail(void *src, void *dst, unsigned long n);
void cam_dec_tail(void *src, void *dst, unsigned long n);
void cam_enc_std(void *src, void *dst, unsigned long n);
void cam_dec_std(void *src, void *dst, unsigned long n);

static int fails = 0;

static unsigned char inb[32], stage[32], outb[32];

static void fill(void) {
    for (int i = 0; i < 32; i++)
        inb[i] = (unsigned char)(i * 3 + 1);
}

static int same(const void *a, const void *b, unsigned long n) {
    return memcmp(a, b, n) == 0;
}

int main() {
    /* DF=0 entries: the pristine wrap copies forward and restores whatever
     * the caller had (DF=0). The 8-byte skew pins the copy DIRECTION:
     * stage[8..23] gets inb[0..15], so a backward copy would put inb[0] at
     * stage[0] instead. */
    fill();
    memset(stage, 0, sizeof stage);
    cam_enc_tail(inb, stage, 16);
    if (!same(stage + 8, inb, 16)) {
        printf("cmll pushfq enc df0 BAD\n");
        fails++;
    } else {
        printf("cmll pushfq enc df0 ok\n");
    }

    memset(outb, 0, sizeof outb);
    for (int i = 0; i < 32; i++) stage[i] = (unsigned char)(i * 7 + 3);
    cam_dec_tail(outb, stage, 12);
    /* decrypt shape: out = outb, ivec = stage: outb[0..11] = stage[8..19]. */
    if (!same(outb, stage + 8, 12)) {
        printf("cmll pushfq dec df0 BAD\n");
        fails++;
    } else {
        printf("cmll pushfq dec df0 ok\n");
    }

    /* DF=1 entries (the std callers): no trap, forward copy. */
    fill();
    memset(stage, 0, sizeof stage);
    cam_enc_std(inb, stage, 16);
    if (!same(stage + 8, inb, 16)) {
        printf("cmll pushfq enc df1 BAD\n");
        fails++;
    } else {
        printf("cmll pushfq enc df1 ok\n");
    }

    memset(outb, 0, sizeof outb);
    for (int i = 0; i < 32; i++) stage[i] = (unsigned char)(i * 7 + 3);
    cam_dec_std(outb, stage, 12);
    if (!same(outb, stage + 8, 12)) {
        printf("cmll pushfq dec df1 BAD\n");
        fails++;
    } else {
        printf("cmll pushfq dec df1 ok\n");
    }

    if (fails) return 1;
    printf("cmll pushfq rep all ok\n");
    return 0;
}
