#include <stdio.h>
#include <string.h>

void rep_movsq(void *dst, void *src, size_t n);
void rep_movsd(void *dst, void *src, size_t n);
void rep_stosd(void *dst, size_t n, long v);

int main() {
    long src[4] = {5, 6, 7, 8}, dst[4] = {0};
    rep_movsq(dst, src, 4);
    if (memcmp(dst, src, sizeof src) != 0) { printf("movsq BAD\n"); return 1; }

    int s32[8], d32[8];
    for (int i = 0; i < 8; i++) s32[i] = i * 11;
    memset(d32, 0, sizeof d32);
    rep_movsd(d32, s32, 8);
    if (memcmp(d32, s32, sizeof s32) != 0) { printf("movsd BAD\n"); return 1; }

    rep_stosd(d32, 8, 0x12345678);
    for (int i = 0; i < 8; i++)
        if (d32[i] != 0x12345678) { printf("stosd BAD\n"); return 1; }

    printf("rep intel ok\n");
    return 0;
}
