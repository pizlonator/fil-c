#include <stdio.h>
#include <stdlib.h>
long confstack(long** pp, long sel);
int main() {
    long* p = malloc(sizeof(long));
    *p = 42;
    long** pp = malloc(sizeof(long*));
    *pp = p;
    // sel=0 takes the stack path (%rax keeps the parked stack pointer):
    // the fail-closed capability check must trap (filc safety error),
    // never read caller stack as a pointer.
    long got = confstack(pp, 0);
    printf("confstack survived %ld (SHOULD TRAP)\n", got);
    return 0;
}
