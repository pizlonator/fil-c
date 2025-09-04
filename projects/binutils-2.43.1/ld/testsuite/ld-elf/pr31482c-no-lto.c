#include <stdlib.h>
#include <stdio.h>

void
abort (void)
{
  printf ("PASS\n");
  exit (0);
}

void
foo (void)
{
  abort ();
}
