#include <stdio.h>
#include <stdlib.h>
void foo(int* x, size_t size);
int main() {
    int* x = malloc(10 * sizeof(int));          // too small for size=50
    printf("expect trap:\n");
    foo(x, 50);                                 // x[i] out of bounds -> trap
    printf("%s\n", "SHOULD NOT PRINT");
    return 0;
}
