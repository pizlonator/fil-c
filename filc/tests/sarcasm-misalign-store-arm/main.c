#include <stdio.h>
#include <stdlib.h>

/* A `;! store ptr` through a base that is only 4-byte aligned must trip the
   alignment branch of the access check (pointers require 8-byte alignment) —
   the store twin of the sarcasm-misalign-arm load case. */
void misstore(void** p, void* v);

int main()
{
    char* buf = malloc(64);
    void** p = (void**)(buf + 4);   /* misaligned for a pointer store */
    int x = 1;
    printf("expect trap:\n");
    fflush(stdout);
    misstore(p, &x);
    printf("SHOULD NOT PRINT\n");
    return 0;
}
