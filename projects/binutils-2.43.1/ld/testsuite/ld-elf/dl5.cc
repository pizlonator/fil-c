#include <stdio.h>
#include <stdlib.h>
#include <new>
#include "dl5.h"

int pass = 0;

void *
operator new (size_t sz, const std::nothrow_t&) throw ()
{
  void *p;
  pass++;
  p = malloc(sz);
  return p;
}

void *
operator new (size_t sz) throw (std::bad_alloc)
{
  void *p;
  pass++;
  p = malloc(sz);
  return p;
}

void
operator delete (void *ptr) throw ()
{
  pass++;
  if (ptr)
    free (ptr);
}

int
main (void)
{
  A *bb = new A[10];
  foo (bb);
  delete [] bb;
  bb = new (std::nothrow) A [10];
  foo (bb);
  delete [] bb;

  if (pass == 4)
    {
      printf ("PASS\n");
      return 0;
    }
  else
    {
      printf ("FAIL\n");
      return 1;
    }
}
