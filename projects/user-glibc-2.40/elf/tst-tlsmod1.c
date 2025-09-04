#include <stdio.h>


__thread int foo, bar __attribute__ ((tls_model("global-dynamic")));
extern __thread int baz __attribute__ ((tls_model("global-dynamic")));
extern __thread int foo_ie asm ("foo") __attribute__ ((tls_model("initial-exec")));
extern __thread int bar_ie asm ("bar") __attribute__ ((tls_model("initial-exec")));
extern __thread int baz_ie asm ("baz") __attribute__ ((tls_model("initial-exec")));


extern int in_dso (void);

int
in_dso (void)
{
  int result = 0;
  int *ap, *bp, *cp;

  /* Get variables using initial exec model.  */
  fputs ("get sum of foo and bar (IE)", stdout);
  asm ("" ::: "memory");
  ap = &foo_ie;
  bp = &bar_ie;
  printf (" = %d\n", *ap + *bp);
  result |= *ap + *bp != 3;
  if (*ap != 1)
    {
      printf ("foo = %d\n", *ap);
      result = 1;
    }
  if (*bp != 2)
    {
      printf ("bar = %d\n", *bp);
      result = 1;
    }


  /* Get variables using generic dynamic model or TLSDESC.  */
  fputs ("get sum of foo and bar and baz (GD or TLSDESC)", stdout);
  ap = &foo;
  bp = &bar;
  cp = &baz;
  printf (" = %d\n", *ap + *bp + *cp);
  result |= *ap + *bp + *cp != 6;
  if (*ap != 1)
    {
      printf ("foo = %d\n", *ap);
      result = 1;
    }
  if (*bp != 2)
    {
      printf ("bar = %d\n", *bp);
      result = 1;
    }
  if (*cp != 3)
    {
      printf ("baz = %d\n", *cp);
      result = 1;
    }

  return result;
}
