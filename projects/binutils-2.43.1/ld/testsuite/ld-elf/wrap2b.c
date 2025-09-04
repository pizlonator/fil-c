#include <stdio.h>
#include "wrap2.h"

static void
loaded(void)
{
  printf ("PASS\n");
}

struct ops __wrap_impl =
{
  .loaded = loaded,
};

int
main()
{
  impl.loaded ();
  return 0;
}
