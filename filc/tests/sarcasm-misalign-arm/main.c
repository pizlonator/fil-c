#include <stdio.h>
#include <stdlib.h>

/* A `;! load ptr` through a base that is only 4-byte aligned must trip the
   alignment branch of the access check (pointers require 8-byte alignment). */
void* misload(void** p);

int main()
{
    char* buf = malloc(64);
    void** p = (void**)(buf + 4);   /* misaligned for a pointer load */
    printf("expect trap:\n");
    fflush(stdout);
    misload(p);
    printf("SHOULD NOT PRINT\n");
    return 0;
}
