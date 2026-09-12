/* Self-contained DF=1 residue-tail test (fully self-contained): the helper in
 * cmll-df.s enters with DF=1 (Fil-C rejects `std` in C inline asm, so the
 * DF comes from assembly, like the old model).
 *
 * - len 0 under DF=1 must NOT trap: the abort branch skips both the block
 *   loop and the residue rep, so no checked rep executes. dst must be
 *   untouched (C assert below).
 * - len 16 under DF=1 must NOT trap either (residue 0: the block loop
 *   runs once and skips the tail), and must copy the bytes exactly:
 *   pinned by a DF=0 copy round-trip first (proving the DF=0 path copies
 *   forward exactly), then the DF=1 len-16 output must equal the DF=0 one
 *   (C asserts below).
 * - len 8 under DF=1 MUST trap with SIGILL (residue 8 reaches the
 *   checked rep, which observes DF=1): the manifest expects `crash` with
 *   "Illegal instruction".
 */
#include <stdio.h>
#include <string.h>
/* fflush after every line: the final SIGILL cannot flush a
 * fully-buffered stdout, so without this the harness would lose the
 * proof lines printed before the trap. */
#define EMIT(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

void cmll_df0_copy(void *dst, void *src, unsigned long len);
void cmll_df1_copy(void *dst, void *src, unsigned long len);

int main() {
    static unsigned char inb[32], df0out[32], df1out[32], back[32];
    for (int i = 0; i < 32; i++)
        inb[i] = (unsigned char)(i * 3 + 1);

    /* DF=0 reference: copy 32 bytes, copy back, must round-trip. */
    memset(df0out, 0, sizeof df0out);
    cmll_df0_copy(df0out, inb, 32);
    memset(back, 0, sizeof back);
    cmll_df0_copy(back, df0out, 32);
    if (memcmp(back, inb, sizeof inb) != 0) {
        EMIT("cmll df0 roundtrip BAD\n");
        return 1;
    }
    EMIT("cmll df0 roundtrip ok\n");

    /* len 0 under DF=1: abort path, no rep, dst untouched. */
    memset(df1out, 0xAA, sizeof df1out);
    cmll_df1_copy(df1out, inb, 0);
    for (int i = 0; i < 32; i++) {
        if (df1out[i] != 0xAA) {
            EMIT("cmll zero-length copy under DF=1 BAD\n");
            return 1;
        }
    }
    EMIT("cmll zero-length copy under DF=1 ok (no trap)\n");

    /* len 16 under DF=1: residue 0, tail skipped, same bytes as DF=0. */
    memset(df1out, 0, sizeof df1out);
    cmll_df1_copy(df1out, inb, 16);
    if (memcmp(df1out, df0out, 16) != 0) {
        EMIT("cmll residue-0 copy under DF=1 BAD\n");
        return 1;
    }
    EMIT("cmll residue-0 copy under DF=1 ok (no trap)\n");

    /* len 8 under DF=1: nonzero residue reaches the checked rep: trap. */
    EMIT("cmll tail copy under DF=1 (SHOULD TRAP):\n");
    memset(df1out, 0, sizeof df1out);
    cmll_df1_copy(df1out, inb, 8);
    EMIT("NOT REACHED\n");
    return 0;
}
