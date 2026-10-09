// Same bug, but the program never looks at the padding: the collector does.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdfil.h>
struct S { __int128 a; void* p; };
__attribute__((noinline)) void callee(struct S s) {
  zprintf("callee %P %P\n", s.p, ((void**)&s)[3]);
  zgc_request_and_wait();            // any GC cycle while the by-value copy is live
  printf("callee done %p\n", s.p);
}
int main(void){
  int dummy;
  char* o = malloc(32);
  struct S* sp = (struct S*)(o + 8);
  sp->p = &dummy;
  uint64_t* fake = malloc(16);
  fake[0] = (uint64_t)(uintptr_t)fake + 16 + 64;   // fake upper: lower + 64
  fake[1] = 0x414141414140ull;                     // fake aux pointer (flags = 0)
  callee(*sp);
  printf("survived\n");
  return 0;
}
