/* Test mpz_nextprime.

Copyright 2009, 2015, 2018, 2020 Free Software Foundation, Inc.

This file is part of the GNU MP Library test suite.

The GNU MP Library test suite is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

The GNU MP Library test suite is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
the GNU MP Library test suite.  If not, see https://www.gnu.org/licenses/.  */


#include <stdio.h>
#include <stdlib.h>

#include "gmp-impl.h"
#include "tests.h"

void
refmpz_nextprime (mpz_ptr p, mpz_srcptr t)
{
  mpz_add_ui (p, t, 1L);
  while (! mpz_probab_prime_p (p, 10))
    mpz_add_ui (p, p, 1L);
}

void
refmpz_prevprime (mpz_ptr p, mpz_srcptr t)
{
  if (mpz_cmp_ui(t, 2) <= 0)
    return;

  mpz_sub_ui (p, t, 1L);
  while (! mpz_probab_prime_p (p, 10))
    mpz_sub_ui (p, p, 1L);
}

void
test_largegap (mpz_t low, const int gap)
{
  mpz_t t, nxt;
  mpz_init (t);
  mpz_init (nxt);

  mpz_nextprime(nxt, low);
  mpz_sub(t, nxt, low);

  if (mpz_cmp_ui(t, gap) != 0)
     {
      gmp_printf ("nextprime gap %Zd => %Zd != %d\n", low, nxt, gap);
      abort ();
    }

  mpz_prevprime(t, nxt);
  if (mpz_cmp(t, low) != 0)
    {
      gmp_printf ("prevprime gap %Zd => %Zd != %d\n", nxt, t, gap);
      abort ();
    }

  mpz_clear (t);
  mpz_clear (nxt);
}

void
test_largegaps ()
{
  mpz_t n;

  mpz_init (n);

  // largest gap with start < 2^32.
  mpz_set_str (n, "3842610773", 10);
  test_largegap (n, 336);

  // largest gap with start < 2^64.
  mpz_set_str (n, "18361375334787046697", 10);
  test_largegap (n, 1550);

  // test high merit primegap in the P30 digit range.
  mpz_set_str (n, "3001549619028223830552751967", 10);
  test_largegap (n, 2184);

  // test high merit primegap in the P100 range.
  mpz_primorial_ui (n, 257);
  mpz_divexact_ui (n, n, 5610);
  mpz_mul_ui (n, n, 4280516017UL);
  mpz_sub_ui (n, n, 2560);
  test_largegap (n, 9006);

  // test high merit primegap in the P200 range.
  mpz_primorial_ui (n, 409);
  mpz_divexact_ui (n, n, 30);
  mpz_mul_ui (n, n, 3483347771UL);
  mpz_sub_ui (n, n, 7016);
  test_largegap (n, 15900);

  mpz_clear (n);
}

void
test_bitboundaries ()
{
  mpz_t n;
  mpz_init (n);

  mpz_set_str (n, "0xfff1", 0);
  test_largegap (n, 16);

  mpz_set_str (n, "0xfffffffb", 0);
  test_largegap (n, 20);

  mpz_set_str (n, "0xffffffffffc5", 0);
  test_largegap (n, 80);

  mpz_set_str (n, "0xffffffffffffffc5", 0);
  test_largegap (n, 72);

  mpz_set_str (n, "0xffffffffffffffffffbf", 0);
  test_largegap (n, 78);

  mpz_set_str (n, "0xffffffffffffffffffffffef", 0);
  test_largegap (n, 78);

  mpz_set_str (n, "0xffffffffffffffffffffffffffb5", 0);
  test_largegap (n, 100);

  mpz_set_str (n, "0xffffffffffffffffffffffffffffff61", 0);
  test_largegap (n, 210);

  mpz_set_str (n, "0xffffffffffffffffffffffffffffffffffffffffffffff13", 0);
  test_largegap (n, 370);

  mpz_clear (n);
}

void
run (const char *start, int reps, const char *end, short diffs[])
{
  mpz_t x, y;
  int i;

  mpz_init_set_str (x, start, 0);
  mpz_init (y);

  for (i = 0; i < reps; i++)
    {
      mpz_nextprime (y, x);
      mpz_sub (x, y, x);
      if (diffs != NULL &&
	  (! mpz_fits_sshort_p (x) || diffs[i] != (short) mpz_get_ui (x)))
	{
	  gmp_printf ("diff list discrepancy\n");
	  abort ();
	}
      mpz_swap (x, y);
    }

  mpz_set_str (y, end, 0);

  if (mpz_cmp (x, y) != 0)
    {
      gmp_printf ("got  %Zd\n", x);
      gmp_printf ("want %Zd\n", y);
      abort ();
    }

  mpz_clear (y);
  mpz_clear (x);
}

void
run_p (const char *start, int reps, const char *end, short diffs[])
{
  mpz_t x, y;
  int i;

  mpz_init_set_str (x, end, 0);
  mpz_init (y);

  // Last rep doesn't share same data with nextprime
  for (i = 0; i < reps - 1; i++)
    {
      mpz_prevprime (y, x);
      mpz_sub (x, x, y);
      if (diffs != NULL &&
	  (! mpz_fits_sshort_p (x) || diffs[reps - i - 1] != (short) mpz_get_ui (x)))
	{
	  gmp_printf ("diff list discrepancy %Zd, %d vs %d\n",
                y, diffs[i], mpz_get_ui (x));
	  abort ();
	}
      mpz_swap (x, y);
    }

  // starts aren't always prime, so check that result is less than or equal
  mpz_prevprime(x, x);

  mpz_set_str(y, start, 0);
  if (mpz_cmp (x, y) > 0)
    {
      gmp_printf ("got  %Zd\n", x);
      gmp_printf ("want %Zd\n", y);
      abort ();
    }

  mpz_clear (y);
  mpz_clear (x);
}


extern short diff1[];
extern short diff3[];
extern short diff4[];
extern short diff5[];
extern short diff6[];

void
test_ref (gmp_randstate_ptr rands, int reps,
          void (*func)(mpz_t, const mpz_t),
          void(*ref_func)(mpz_t, const mpz_t))
{
  int i;
  mpz_t bs, x, test_p, ref_p;
  unsigned long size_range;

  mpz_init (bs);
  mpz_init (x);
  mpz_init (test_p);
  mpz_init (ref_p);

  for (i = 0; i < reps; i++)
    {
      mpz_urandomb (bs, rands, 32);
      size_range = mpz_get_ui (bs) % 8 + 2; /* 0..1024 bit operands */

      mpz_urandomb (bs, rands, size_range);
      mpz_rrandomb (x, rands, mpz_get_ui (bs));

      func (test_p, x);
      ref_func (ref_p, x);
      if (mpz_cmp (test_p, ref_p) != 0)
        {
          gmp_printf ("start %Zd\n", x);
          gmp_printf ("got   %Zd\n", test_p);
          gmp_printf ("want  %Zd\n", ref_p);
	  abort ();
        }
    }

  mpz_clear (bs);
  mpz_clear (x);
  mpz_clear (test_p);
  mpz_clear (ref_p);
}

void
test_nextprime(gmp_randstate_ptr rands, int reps)
{
  run ("2", 1000, "0x1ef7", diff1);

  run ("3", 1000 - 1, "0x1ef7", NULL);

  run ("0x8a43866f5776ccd5b02186e90d28946aeb0ed914", 50,
       "0x8a43866f5776ccd5b02186e90d28946aeb0eeec5", diff3);

  run ("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF6C", 50, /* 2^148 - 148 */
       "0x100000000000000000000000000000000010ab", diff4);

  run ("0x1c2c26be55317530311facb648ea06b359b969715db83292ab8cf898d8b1b", 50,
       "0x1c2c26be55317530311facb648ea06b359b969715db83292ab8cf898da957", diff5);

  run ("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF80", 50, /* 2^128 - 128 */
       "0x10000000000000000000000000000155B", diff6);

  test_ref(
      rands, reps,
      (void (*)(mpz_t, const mpz_t)) mpz_nextprime,
      refmpz_nextprime);
}

void
test_prevprime (gmp_randstate_ptr rands, int reps)
{
  long i;
  int retval;
  mpz_t n, prvp;

  mpz_init (n);
  mpz_init (prvp);

  /* Test mpz_prevprime(n <= 2) returns 0, leaves rop unchanged. */
  {
    int temp = 123;
    mpz_set_ui (prvp, temp);
    for (i = 0; i <= 2; i++)
      {
        mpz_set_si(n, i);
        retval = mpz_prevprime (prvp, n);
        if ( retval != 0 || mpz_cmp_ui (prvp, temp) != 0 )
          {
            gmp_printf ("mpz_prevprime(%Zd) return (%d) rop (%Zd)\n", n, retval, prvp);
            abort ();
          }
      }
  }

  mpz_clear (n);
  mpz_clear (prvp);

  run_p ("2", 1000, "0x1ef7", diff1);

  run_p ("3", 1000 - 1, "0x1ef7", NULL);

  run_p ("0x8a43866f5776ccd5b02186e90d28946aeb0ed914", 50,
         "0x8a43866f5776ccd5b02186e90d28946aeb0eeec5", diff3);

  run_p ("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF6C", 50, /* 2^148 - 148 */
         "0x100000000000000000000000000000000010ab", diff4);

  run_p ("0x1c2c26be55317530311facb648ea06b359b969715db83292ab8cf898d8b1b", 50,
         "0x1c2c26be55317530311facb648ea06b359b969715db83292ab8cf898da957", diff5);

  run_p ("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF80", 50, /* 2^128 - 128 */
         "0x10000000000000000000000000000155B", diff6);

  // Cast away int return from mpz_prevprime for test ref.
  test_ref(
      rands, reps,
      (void (*)(mpz_t, const mpz_t)) mpz_prevprime,
      refmpz_prevprime);
}

int
main (int argc, char **argv)
{
  gmp_randstate_ptr rands;
  int reps = 20;

  tests_start();

  rands = RANDS;
  TESTS_REPS (reps, argv, argc);

  test_nextprime(rands, reps);
  test_prevprime(rands, reps);

  test_largegaps ();
  test_bitboundaries ();

  tests_end ();
  return 0;
}

short diff1[] =
{
  1,2,2,4,2,4,2,4,6,2,6,4,2,4,6,6,
  2,6,4,2,6,4,6,8,4,2,4,2,4,14,4,6,
  2,10,2,6,6,4,6,6,2,10,2,4,2,12,12,4,
  2,4,6,2,10,6,6,6,2,6,4,2,10,14,4,2,
  4,14,6,10,2,4,6,8,6,6,4,6,8,4,8,10,
  2,10,2,6,4,6,8,4,2,4,12,8,4,8,4,6,
  12,2,18,6,10,6,6,2,6,10,6,6,2,6,6,4,
  2,12,10,2,4,6,6,2,12,4,6,8,10,8,10,8,
  6,6,4,8,6,4,8,4,14,10,12,2,10,2,4,2,
  10,14,4,2,4,14,4,2,4,20,4,8,10,8,4,6,
  6,14,4,6,6,8,6,12,4,6,2,10,2,6,10,2,
  10,2,6,18,4,2,4,6,6,8,6,6,22,2,10,8,
  10,6,6,8,12,4,6,6,2,6,12,10,18,2,4,6,
  2,6,4,2,4,12,2,6,34,6,6,8,18,10,14,4,
  2,4,6,8,4,2,6,12,10,2,4,2,4,6,12,12,
  8,12,6,4,6,8,4,8,4,14,4,6,2,4,6,2,
  6,10,20,6,4,2,24,4,2,10,12,2,10,8,6,6,
  6,18,6,4,2,12,10,12,8,16,14,6,4,2,4,2,
  10,12,6,6,18,2,16,2,22,6,8,6,4,2,4,8,
  6,10,2,10,14,10,6,12,2,4,2,10,12,2,16,2,
  6,4,2,10,8,18,24,4,6,8,16,2,4,8,16,2,
  4,8,6,6,4,12,2,22,6,2,6,4,6,14,6,4,
  2,6,4,6,12,6,6,14,4,6,12,8,6,4,26,18,
  10,8,4,6,2,6,22,12,2,16,8,4,12,14,10,2,
  4,8,6,6,4,2,4,6,8,4,2,6,10,2,10,8,
  4,14,10,12,2,6,4,2,16,14,4,6,8,6,4,18,
  8,10,6,6,8,10,12,14,4,6,6,2,28,2,10,8,
  4,14,4,8,12,6,12,4,6,20,10,2,16,26,4,2,
  12,6,4,12,6,8,4,8,22,2,4,2,12,28,2,6,
  6,6,4,6,2,12,4,12,2,10,2,16,2,16,6,20,
  16,8,4,2,4,2,22,8,12,6,10,2,4,6,2,6,
  10,2,12,10,2,10,14,6,4,6,8,6,6,16,12,2,
  4,14,6,4,8,10,8,6,6,22,6,2,10,14,4,6,
  18,2,10,14,4,2,10,14,4,8,18,4,6,2,4,6,
  2,12,4,20,22,12,2,4,6,6,2,6,22,2,6,16,
  6,12,2,6,12,16,2,4,6,14,4,2,18,24,10,6,
  2,10,2,10,2,10,6,2,10,2,10,6,8,30,10,2,
  10,8,6,10,18,6,12,12,2,18,6,4,6,6,18,2,
  10,14,6,4,2,4,24,2,12,6,16,8,6,6,18,16,
  2,4,6,2,6,6,10,6,12,12,18,2,6,4,18,8,
  24,4,2,4,6,2,12,4,14,30,10,6,12,14,6,10,
  12,2,4,6,8,6,10,2,4,14,6,6,4,6,2,10,
  2,16,12,8,18,4,6,12,2,6,6,6,28,6,14,4,
  8,10,8,12,18,4,2,4,24,12,6,2,16,6,6,14,
  10,14,4,30,6,6,6,8,6,4,2,12,6,4,2,6,
  22,6,2,4,18,2,4,12,2,6,4,26,6,6,4,8,
  10,32,16,2,6,4,2,4,2,10,14,6,4,8,10,6,
  20,4,2,6,30,4,8,10,6,6,8,6,12,4,6,2,
  6,4,6,2,10,2,16,6,20,4,12,14,28,6,20,4,
  18,8,6,4,6,14,6,6,10,2,10,12,8,10,2,10,
  8,12,10,24,2,4,8,6,4,8,18,10,6,6,2,6,
  10,12,2,10,6,6,6,8,6,10,6,2,6,6,6,10,
  8,24,6,22,2,18,4,8,10,30,8,18,4,2,10,6,
  2,6,4,18,8,12,18,16,6,2,12,6,10,2,10,2,
  6,10,14,4,24,2,16,2,10,2,10,20,4,2,4,8,
  16,6,6,2,12,16,8,4,6,30,2,10,2,6,4,6,
  6,8,6,4,12,6,8,12,4,14,12,10,24,6,12,6,
  2,22,8,18,10,6,14,4,2,6,10,8,6,4,6,30,
  14,10,2,12,10,2,16,2,18,24,18,6,16,18,6,2,
  18,4,6,2,10,8,10,6,6,8,4,6,2,10,2,12,
  4,6,6,2,12,4,14,18,4,6,20,4,8,6,4,8,
  4,14,6,4,14,12,4,2,30,4,24,6,6,12,12,14,
  6,4,2,4,18,6,12,8
};

short diff3[] =
{
  33,32,136,116,24,22,104,114,76,278,238,162,36,44,388,134,
  130,26,312,42,138,28,24,80,138,108,270,12,330,130,98,102,
  162,34,36,170,90,34,14,6,24,66,154,218,70,132,188,88,
  80,82
};

short diff4[] =
{
  239,92,64,6,104,24,46,258,68,18,54,100,68,154,26,4,
  38,142,168,42,18,26,286,104,136,116,40,2,28,110,52,78,
  104,24,54,96,4,626,196,24,56,36,52,102,48,156,26,18,
  42,40
};

short diff5[] =
{
  268,120,320,184,396,2,94,108,20,318,274,14,64,122,220,108,
  18,174,6,24,348,32,64,116,268,162,20,156,28,110,52,428,
  196,14,262,30,194,120,300,66,268,12,428,370,212,198,192,130,
  30,80
};

short diff6[] =
{
  179,30,84,108,112,36,42,110,52,132,60,30,326,114,496,92,100,
  272,36,54,90,4,2,24,40,398,150,72,60,16,8,4,80,16,2,342,112,
  14,136,236,40,18,50,192,198,204,40,266,42,274
};
