/* glibc test for TLS in ld.so.  */
#include <stdio.h>


__thread int foo, bar __attribute__ ((tls_model("local-exec")));
extern __thread int foo_gd asm ("foo") __attribute__ ((tls_model("global-dynamic")));
extern __thread int foo_ld asm ("foo") __attribute__ ((tls_model("local-dynamic")));
extern __thread int foo_ie asm ("foo") __attribute__ ((tls_model("initial-exec")));
extern __thread int bar_gd asm ("bar") __attribute__ ((tls_model("global-dynamic")));
extern __thread int bar_ld asm ("bar") __attribute__ ((tls_model("local-dynamic")));
extern __thread int bar_ie asm ("bar") __attribute__ ((tls_model("initial-exec")));

static int
do_test (void)
{
  int result = 0;
  int *ap, *bp;


  /* Set the variable using the local exec model.  */
  puts ("set bar to 1 (LE)");
  bar = 1;


  /* Get variables using initial exec model.  */
  fputs ("get sum of foo and bar (IE)", stdout);
  ap = &foo_ie;
  bp = &bar_ie;
  printf (" = %d\n", *ap + *bp);
  result |= *ap + *bp != 1;
  if (*ap != 0 || *bp != 1)
    {
      printf ("foo = %d\nbar = %d\n", *ap, *bp);
      result = 1;
    }


  /* Get variables using local dynamic model or TLSDESC.  */
  fputs ("get sum of foo and bar (LD or TLSDESC)", stdout);
  ap = &foo_ld;
  bp = &bar_ld;
  printf (" = %d\n", *ap + *bp);
  result |= *ap + *bp != 1;
  if (*ap != 0 || *bp != 1)
    {
      printf ("foo = %d\nbar = %d\n", *ap, *bp);
      result = 1;
    }


  /* Get variables using general dynamic model or TLSDESC.  */
  fputs ("get sum of foo and bar (GD or TLSDESC)", stdout);
  ap = &foo_gd;
  bp = &bar_gd;
  printf (" = %d\n", *ap + *bp);
  result |= *ap + *bp != 1;
  if (*ap != 0 || *bp != 1)
    {
      printf ("foo = %d\nbar = %d\n", *ap, *bp);
      result = 1;
    }


  return result;
}


#include <support/test-driver.c>
