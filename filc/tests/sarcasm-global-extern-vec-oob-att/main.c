/* An OOB 16-byte vector read through a `#! global ptr` flight pointer
   traps: the capability is the data object's payload, so the bounds
   check fires exactly like a C array access. */
#include <stdio.h>
void vec_oob(void *buf);

int main()
{
    int buf[4];
    printf("expect trap:\n");
    vec_oob(buf);   /* 16 bytes starting at byte 32 of a 32-byte object */
    printf("FAIL no panic\n");
    return 1;
}
