#include <stdio.h>
#include <stdlib.h>

void rep_stosq(void *dst, size_t n, long v);

int main() {
    long *dst = malloc(2 * sizeof(long));
    printf("rep stosq OOB (SHOULD PANIC):\n");
    rep_stosq(dst, 8, 0);
    printf("NOT REACHED\n");
    return 0;
}
