#include <stdio.h>
unsigned ownweb(unsigned long x);
int main(){
  /* 0xcafebabe + 0xfeedface = 0x1c9ecb5bc, low 32 = 0xc9ecb5bc */
  unsigned r = ownweb(0);
  printf("%s (%08x)\n", r == 0xc9ecb58cu ? "slot granule own ok" : "slot granule own BAD", r);
  return 0;
}
