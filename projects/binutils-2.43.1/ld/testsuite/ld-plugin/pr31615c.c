#include <stdio.h>

__attribute__ ((weak))
void
bar (void)
{
  printf ("PASS\n");
}
