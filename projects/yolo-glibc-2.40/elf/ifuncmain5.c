/* Test STT_GNU_IFUNC symbols with dynamic function pointer only.  */

#include <stdlib.h>

extern int foo (void);
extern int foo_protected (void);

typedef int (*foo_p) (void);

foo_p
__attribute__ ((noinline))
get_foo (void)
{
  return foo;
}


/* Address-significant access to protected symbols is not supported in
   position-dependent mode on several architectures because GCC
   generates relocations that assume that the address is local to the
   main program.  */
#ifdef __PIE__
foo_p
__attribute__ ((noinline))
get_foo_protected (void)
{
  return foo_protected;
}
#endif

int
main (void)
{
  foo_p p;

  p = get_foo ();
  if ((*p) () != -1)
    abort ();

#ifdef __PIE__
  p = get_foo_protected ();
  if ((*p) () != 0)
    abort ();
#endif

  return 0;
}
