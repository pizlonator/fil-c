/* No pointer is ever forged/dereferenced by the program itself: it only passes a pointer-free struct by value from
   a 4-byte-aligned address inside a buffer that used to hold pointers. The collector then chases the torn lowers. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
struct S { int a[8]; };
struct Outer { long x; struct S s; };
static struct S *keep;
__attribute__((noinline)) int callee(struct S s) {
    keep = malloc(sizeof *keep);
    *keep = s;                      /* plain struct copy: carries the torn lowers along */
    return s.a[0];
}
int main(void) {
    void **arena = malloc(8 * sizeof(void*));
    for (int i = 0; i < 8; i++) arena[i] = malloc(32);      /* arena first used for pointers */
    memset(arena, 0x11, 4);                                  /* ... */
    struct Outer *o = (struct Outer*)((char*)arena + 4);     /* ... then re-used for packed records */
    for (int i = 0; i < 8; i++) o->s.a[i] = i;               /* int stores keep the stale lowers (by design) */
    printf("%d\n", callee(o->s));
    zgc_request_and_wait();
    puts("survived GC");
    return 0;
}
