#include <stdio.h>
#include <stdlib.h>
#include <string.h>
long flagdf_copy(long* dst, long* src, long n);
int main() {
    long src[4] = {11, 22, 33, 44}, dst[4] = {0};
    long got = flagdf_copy(dst, src, 4);
    int ok = (got == 11 && dst[0] == 11 && dst[1] == 22
           && dst[2] == 33 && dst[3] == 44);
    printf("flagdf %s\n", ok ? "ok" : "BAD");
    return 0;
}
