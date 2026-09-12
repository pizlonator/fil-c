#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void rep_movsq(void *dst, void *src, size_t n);
void rep_movsb(void *dst, void *src, size_t n);
void rep_movsw(void *dst, void *src, size_t n);
void rep_movsl(void *dst, void *src, size_t n);
void bare_movsq(void *dst, void *src);
void rep_zero_skip(void *dst, void *src);

int main() {
    char src[64], dst[64];
    for (int i = 0; i < 64; i++) src[i] = (char)(i + 1);

    memset(dst, 0, sizeof dst);
    rep_movsq(dst, src, 8);
    if (memcmp(dst, src, sizeof dst) != 0) { printf("movsq BAD\n"); return 1; }

    memset(dst, 0, sizeof dst);
    rep_movsb(dst, src, 64);
    if (memcmp(dst, src, sizeof dst) != 0) { printf("movsb BAD\n"); return 1; }

    memset(dst, 0, sizeof dst);
    rep_movsw(dst, src, 32);
    if (memcmp(dst, src, sizeof dst) != 0) { printf("movsw BAD\n"); return 1; }

    memset(dst, 0, sizeof dst);
    rep_movsl(dst, src, 16);
    if (memcmp(dst, src, sizeof dst) != 0) { printf("movsl BAD\n"); return 1; }

    memset(dst, 0, sizeof dst);
    bare_movsq(dst, src);
    if (memcmp(dst, src, 8) != 0) { printf("bare movsq BAD\n"); return 1; }

    // A zero count copies nothing and must not trap, even on null pointers.
    rep_zero_skip((void*)0, (void*)0);

    printf("rep movs ok\n");
    return 0;
}
