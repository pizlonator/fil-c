#include <stdio.h>
unsigned fplea(unsigned long x);
int main(){
  /* 0x12345678 + low dword of x + 0x5a; x's low dword is 0x2468ace for 0x11223344552468ace? keep it simple */
  unsigned r = fplea(0x100000002468UL);  /* low dword 0x2468 */
  printf("%s (%u)\n", r == 0x12345678u + 0x2468u + 0x5au ? "fp-lea-rbp ok" : "fp-lea-rbp BAD", r);
  return 0;
}
