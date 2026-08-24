#include <stdio.h>
#include <stdlib.h>

/* A 16-byte NEON q0 store starting 8 bytes into a 16-byte object covers
   [8, 24) — out of bounds. The access check must use the true 16-byte size
   (this previously escaped with an 8-byte check). */
void store16(void* p, long a, long b);

int main()
{
    void* p = malloc(16);
    printf("expect trap:\n");
    fflush(stdout);
    store16((char*)p + 8, 0x1111111111111111L, 0x2222222222222222L);
    printf("SHOULD NOT PRINT\n");
    return 0;
}
