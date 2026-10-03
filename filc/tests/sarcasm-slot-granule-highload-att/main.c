#include <stdio.h>
unsigned highload(unsigned long x);
int main(){
  unsigned r = highload(0);
  printf("%s (%08x)\n", r == 0x12345678u ? "slot granule highload ok" : "slot granule highload BAD", r);
  return 0;
}
