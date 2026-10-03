#include <stdio.h>
unsigned twogran(unsigned long x);
int main(){
  unsigned r = twogran(0);
  printf("%s (%08x)\n", r == 0x44444444u ? "slot granule twogran ok" : "slot granule twogran BAD", r);
  return 0;
}
