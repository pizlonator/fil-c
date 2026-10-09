#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdfil.h>
struct S { long a, b, c; };   // no pointer-typed members, ABI align 8 => "word aligned" demote is used
__attribute__((noinline)) void callee(struct S s) {
  void** w = (void**)&s;
  zprintf("callee copy: %P | %P | %P\n", w[0], w[1], w[2]);
}
int main(int argc, char** argv){
  int k = 1;
  asm volatile("" : "+r"(k));
  void** o = malloc(64);
  for (int i = 0; i < 8; i++) o[i] = malloc(16);     // fills o's aux with valid lowers
  zprintf("o[0]=%P o[1]=%P\n", o[0], o[1]);
  struct S* sp = (struct S*)((char*)o + k);          // misaligned but fully in bounds
  callee(*sp);
  zgc_request_and_wait();
  printf("survived\n");
  return 0;
}
