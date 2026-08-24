#include <stdio.h>
#include <stdlib.h>

/* An ldp x-pair reads 16 bytes; starting 8 bytes into a 16-byte object,
   only 8 of the needed 16 bytes are in bounds, so it must trap (the pair
   access size is 2x the element size, not 8). */
long loadpair(long* p);

int main()
{
    char* p = malloc(16);
    printf("expect trap:\n");
    fflush(stdout);
    printf("%ld\n", loadpair((long*)(p + 8)));
    printf("SHOULD NOT PRINT\n");
    return 0;
}
