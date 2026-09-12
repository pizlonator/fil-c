/* BZ 12420 */

#include <errno.h>
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <libc-diag.h>

static int
do_test (void)
{
  if (FE_ALL_EXCEPT == 0)
    {
      printf("Skipping test; no support for FP exceptions.\n");
      return 0;
    }

  int except_mask = 0;
#ifdef FE_DIVBYZERO
  except_mask |= FE_DIVBYZERO;
#endif
#ifdef FE_INVALID
  except_mask |= FE_INVALID;
#endif
#ifdef FE_OVERFLOW
  except_mask |= FE_OVERFLOW;
#endif
#ifdef FE_UNDERFLOW
  except_mask |= FE_UNDERFLOW;
#endif
  int status = feenableexcept (except_mask);

  except_mask = fegetexcept ();
  if (except_mask == -1)
    {
      printf("\nBefore getcontext(): fegetexcept returned: %d\n",
	     except_mask);
      return 1;
    }

  ucontext_t ctx;
  status = getcontext(&ctx);
  if (status)
    {
      printf("\ngetcontext failed, errno: %d.\n", errno);
      return 1;
    }

  printf ("\nDone with getcontext()!\n");
  fflush (NULL);

  int mask = fegetexcept ();
  if (mask != except_mask)
    {
      printf("\nAfter getcontext(): fegetexcept returned: %d, expected: %d.\n",
	     mask, except_mask);
      return 1;
    }

  printf("\nAt end fegetexcept() returned %d, expected: %d.\n",
	 mask, except_mask);

  return 0;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
