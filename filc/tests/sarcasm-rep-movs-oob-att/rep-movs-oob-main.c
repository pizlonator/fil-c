#include <stdio.h>
#include <stdlib.h>

void rep_movsq(void *dst, void *src, size_t n);

int main() {
    long *src = malloc(8 * sizeof(long));
    long *dst = malloc(2 * sizeof(long));
    for (int i = 0; i < 8; i++) src[i] = i;
    printf("rep movsq dest OOB (SHOULD PANIC):\n");
    rep_movsq(dst, src, 8);
    printf("NOT REACHED %ld\n", dst[7]);
    return 0;
}
