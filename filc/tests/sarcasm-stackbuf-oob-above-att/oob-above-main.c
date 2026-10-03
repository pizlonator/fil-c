#include <stdio.h>

int oob_above(unsigned long idx);

int main() {
    printf("got %d\n", oob_above(100));
    return 0;
}
