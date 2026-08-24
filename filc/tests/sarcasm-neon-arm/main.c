#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* In-bounds NEON accesses: q (16B), s/b/h NEON scalars, and q-pairs must use
   their true access sizes so that valid objects accept them. */
void copy16(void* dst, const void* src);
void copyscalar(void* dst, const void* src);
void copypair(void* dst, const void* src);

int main()
{
    char* a = malloc(16);
    char* b = malloc(16);
    for (int i = 0; i < 16; i++) a[i] = (char)(i * 7 + 3);
    memset(b, 0, 16);
    copy16(b, a);
    printf("q: %d\n", memcmp(a, b, 16) == 0);

    char* c = malloc(8);
    char* d = malloc(8);
    for (int i = 0; i < 8; i++) c[i] = (char)(i * 5 + 1);
    memset(d, 0, 8);
    copyscalar(d, c);
    printf("sbh: %d\n", memcmp(c, d, 8) == 0);

    char* e = malloc(32);
    char* f = malloc(32);
    for (int i = 0; i < 32; i++) e[i] = (char)(i * 3 + 2);
    memset(f, 0, 32);
    copypair(f, e);
    printf("pair: %d\n", memcmp(e, f, 32) == 0);
    return 0;
}
