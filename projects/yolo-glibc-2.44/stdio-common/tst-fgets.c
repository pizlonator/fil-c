/* Derived from the test case in
   https://sourceware.org/bugzilla/show_bug.cgi?id=713.  */
#include <stdio.h>

#include <support/xstdio.h>

static int
do_test (void)
{
  FILE *fp = xfmemopen ((char *) "hello", 5, "r");
  char buf[2];
  char *bp = fgets (buf, sizeof (buf), fp);
  printf ("fgets: %s\n", bp == buf ? "OK" : "ERROR");
  int res = bp != buf;
  bp = fgets_unlocked (buf, sizeof (buf), fp);
  printf ("fgets_unlocked: %s\n", bp == buf ? "OK" : "ERROR");
  xfclose (fp);
  res |= bp != buf;
  return res;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
