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
baz (void)
{
}
