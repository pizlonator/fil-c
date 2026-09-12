#include <stdio.h>

void rep_stosq(void *dst, size_t n, long v);
void rep_stosb(void *dst, size_t n, long v);
void rep_stosw(void *dst, size_t n, long v);
void rep_stosl(void *dst, size_t n, long v);
void bare_stosq(void *dst, long v);

int main() {
    long q[8];
    rep_stosq(q, 8, 0x1122334455667788L);
    for (int i = 0; i < 8; i++)
        if (q[i] != 0x1122334455667788L) { printf("stosq BAD\n"); return 1; }

    char b[64];
    rep_stosb(b, 64, 0xAB);
    for (int i = 0; i < 64; i++)
        if (b[i] != (char)0xAB) { printf("stosb BAD\n"); return 1; }

    short w[8];
    rep_stosw(w, 8, 0x1234);
    for (int i = 0; i < 8; i++)
        if (w[i] != 0x1234) { printf("stosw BAD\n"); return 1; }

    int l[8];
    rep_stosl(l, 8, 0x55);
    for (int i = 0; i < 8; i++)
        if (l[i] != 0x55) { printf("stosl BAD\n"); return 1; }

    bare_stosq(q, 0x99);
    if (q[0] != 0x99) { printf("bare stosq BAD\n"); return 1; }

    printf("rep stos ok\n");
    return 0;
}
