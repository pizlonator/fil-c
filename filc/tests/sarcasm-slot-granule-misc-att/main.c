#include <stdio.h>
unsigned byteword(unsigned long x);
unsigned long strad(unsigned long x);
unsigned liveflags(unsigned long a, unsigned long b);
int main(){
  unsigned a = byteword(0);                 /* 0xaa + 0xaabb = 0xab65 */
  unsigned long b = strad(0);               /* 0x1122deadbeef7788 */
  unsigned c = liveflags(5, 3);             /* CF clear: 0x99 */
  unsigned d = liveflags(3, 5);             /* CF set:   0x9a */
  int ok = a == 0xab65u && b == 0x1122deadbeef7788UL && c == 0x99u && d == 0x9au;
  printf("%s (%08x %016lx %02x %02x)\n", ok ? "slot granule misc ok" : "slot granule misc BAD", a, b, c, d);
  return 0;
}
