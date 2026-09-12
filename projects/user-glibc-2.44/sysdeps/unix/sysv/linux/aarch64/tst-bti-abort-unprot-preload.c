/* This test checks if the process is aborted if LD_PRELOAD is used with a
   module without BTI markings and BTI is enforced.  */
#include "tst-bti-skeleton.c"

int fun (void)
{
  return 0;
}
