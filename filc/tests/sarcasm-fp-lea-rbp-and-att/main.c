#include <stdio.h>
unsigned fpleaand(unsigned long x);
int main(){
  unsigned r = fpleaand(0x100000002468UL);  /* low dword 0x2468, byte 0 replaced by 0x7f: 0x24687f */
  printf("%s (%u)\n", r == 0x24687fu ? "fp-lea-rbp-and ok" : "fp-lea-rbp-and BAD", r);
  return 0;
}
