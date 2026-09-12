#include <stdio.h>
#include <stdlib.h>

void rep_movsq(void *dst, void *src, size_t n);

int main() {
    long *src = malloc(2 * sizeof(long));
    long *dst = malloc(8 * sizeof(long));
    src[0] = 1; src[1] = 2;
    printf("rep movsq source OOB (SHOULD PANIC):\n");
    rep_movsq(dst, src, 8);
    printf("NOT REACHED\n");
    return 0;
}
