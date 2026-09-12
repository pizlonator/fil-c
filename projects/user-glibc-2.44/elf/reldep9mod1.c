#include <stdlib.h>
void
foo (void)
{
  exit (0);
}

void
__attribute__((destructor))
bar (void)
{
  foo ();
}

void
__attribute__((constructor))
destr (void)
{
  extern void baz (void);
  baz ();
}
