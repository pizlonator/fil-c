#include <stdio.h>


__thread int baz;


int
in_dso (int n, int *caller_bazp)
{
  int *bazp;
  int result = 0;

  puts ("foo");			/* Make sure PLT is used before macros.  */
  asm ("" ::: "memory");

  bazp = &baz;

  if (caller_bazp != NULL && bazp != caller_bazp)
    {
      printf ("callers address of baz differs: %p vs %p\n", caller_bazp, bazp);
      result = 1;
    }
  else if (*bazp != n)
    {
      printf ("baz != %d\n", n);
      result = 1;
    }

  *bazp = 16;

  return result;
}
