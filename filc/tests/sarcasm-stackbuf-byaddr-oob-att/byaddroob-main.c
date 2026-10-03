#include <stdio.h>
long baoob(unsigned long idx);
int main() {
    printf("should not get here: %lx\n", baoob(0));
    return 0;
}
