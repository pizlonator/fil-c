#include <stdio.h>
unsigned long lowstore(unsigned long x);
int main(){
  unsigned long r = lowstore(0);
  printf("%s (%016lx)\n", r == 0x11112222deadbeefUL ? "slot granule lowstore ok" : "slot granule lowstore BAD", r);
  return 0;
}
