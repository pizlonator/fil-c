#include <stdio.h>

void df_set_then_copy(void *dst, void *src, size_t n);

int main() {
    long src[4] = {1, 2, 3, 4}, dst[4] = {0};
    printf("rep movsq under DF=1 (SHOULD TRAP):\n");
    df_set_then_copy(dst, src, 4);
    printf("NOT REACHED\n");
    return 0;
}
