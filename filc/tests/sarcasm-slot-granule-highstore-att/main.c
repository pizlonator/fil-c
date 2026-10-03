#include <stdio.h>
unsigned long highstore(unsigned long x);
int main(){
  unsigned long r = highstore(0);
  printf("%s (%016lx)\n", r == 0xdeadbeef33334444UL ? "slot granule highstore ok" : "slot granule highstore BAD", r);
  return 0;
}
