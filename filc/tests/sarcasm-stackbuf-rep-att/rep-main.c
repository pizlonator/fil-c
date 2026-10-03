#include <stdio.h>
#include <string.h>

void sb_rep_iv_copy(unsigned char* dst, unsigned char* iv, unsigned long n);
void sb_rep_fill_copy(unsigned char* dst, unsigned long count, unsigned long fill);
void sb_rep_zero(unsigned char* dst, unsigned char* iv);

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while (0)

int main() {
    unsigned char iv[16], out[32];
    for (int i = 0; i < 16; i++) iv[i] = (unsigned char)(i + 1);

    // count < range
    memset(out, 0, sizeof out);
    sb_rep_iv_copy(out, iv, 8);
    CHECK(memcmp(out, iv, 8) == 0);
    CHECK(out[8] == 0 && out[15] == 0);

    // count == range (the exact buffer size)
    memset(out, 0, sizeof out);
    sb_rep_iv_copy(out, iv, 16);
    CHECK(memcmp(out, iv, 16) == 0);

    // fill the buffer, then copy it out byte-for-byte
    memset(out, 0, sizeof out);
    sb_rep_fill_copy(out, 16, 0xAB);
    {
        unsigned char want[16];
        memset(want, 0xAB, 16);
        CHECK(memcmp(out, want, 16) == 0);
    }

    // zero count: no trap, nothing copied
    memset(out, 0, sizeof out);
    sb_rep_zero(out, iv);
    CHECK(out[0] == 0 && out[15] == 0);

    if (fails == 0) printf("stack buffer rep ok\n");
    return fails ? 1 : 0;
}
