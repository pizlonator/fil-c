#include <stdio.h>
#include <stdlib.h>
long confheap(long** pp, long sel);
int main() {
    long* p = malloc(sizeof(long));
    *p = 42;
    long** pp = malloc(sizeof(long*));
    *pp = p;
    // sel=1 takes the heap path (%rax = %rdi); the stack path would trap.
    long got = confheap(pp, 1);
    printf("confheap %ld %s\n", got, got == 42 ? "ok" : "BAD");
    return 0;
}
