#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
struct S { __int128 a; void* p; };   // { i128, ptr }: alloc size 32, last member ends at 24
__attribute__((noinline)) void callee(struct S s) {
  unsigned long* w = (unsigned long*)&s;
  printf("callee: w[0]=%lx w[2]=%lx tailpad w[3]=%lx\n", w[0], w[2], w[3]);
  zprintf("ptr in tail pad: %P\n", ((void**)&s)[3]);
}
int main(void){
  char* o = malloc(32); char* n = malloc(32);
  memset(o, 0x11, 32); memset(n, 0x22, 32);
  printf("o=%p..%p n=%p\n", o, zgetupper(o), n);
  struct S* sp = (struct S*)(o + 8);   // members [8,32) are in bounds, tail padding [32,40) is not
  callee(*sp);
  return 0;
}
