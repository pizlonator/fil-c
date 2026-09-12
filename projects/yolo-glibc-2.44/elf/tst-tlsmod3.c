#include <stdio.h>


extern int in_dso (int n, int *caller_foop);

extern __thread int foo;
__thread int comm_n;




int
in_dso2 (void)
{
  int *foop;
  int result = 0;
  static int n;
  int *np;

  puts ("foo");			/* Make sure PLT is used before macros.  */
  asm ("" ::: "memory");

  foop = &foo;
  np = &comm_n;

  if (n != *np)
    {
      printf ("n = %d != comm_n = %d\n", n, *np);
      result = 1;
    }

  result |= in_dso (*foop = 42 + n++, foop);

  *foop = 16;

  return result;
}
