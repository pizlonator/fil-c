/* glibc test for TLS in ld.so.  */
#include <stdio.h>


__thread int foo, bar __attribute__ ((tls_model("initial-exec")));
__thread int baz __attribute__ ((tls_model("local-exec")));
extern __thread int foo_gd __attribute__ ((alias("foo"), tls_model("global-dynamic")));
extern __thread int bar_gd __attribute__ ((alias("bar"), tls_model("global-dynamic")));
extern __thread int baz_ld __attribute__ ((alias("baz"), tls_model("local-dynamic")));


extern int in_dso (void);


static int
do_test (void)
{
  int result = 0;
  int *ap, *bp, *cp;


  /* Set the variable using the local exec model.  */
  puts ("set baz to 3 (LE)");
  baz = 3;


  /* Get variables using initial exec model.  */
  puts ("set variables foo and bar (IE)");
  foo = 1;
  bar = 2;


  /* Get variables using local dynamic model.  */
  fputs ("get sum of foo, bar (GD) and baz (LD)", stdout);
  ap = &foo_gd;
  bp = &bar_gd;
  cp = &baz_ld;
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


  result |= in_dso ();

  return result;
}


#include <support/test-driver.c>
