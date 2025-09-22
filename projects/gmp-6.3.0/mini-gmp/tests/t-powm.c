/*

Copyright 2012, 2022, Free Software Foundation, Inc.

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

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

#include "testutils.h"

#define MAXBITS 400
#define COUNT 1000

void
testmain (int argc, char **argv)
{
  unsigned i;
  mpz_t b, e, m, res, ref;

  mpz_init (b);
  mpz_init (e);
  mpz_init (m);
  mpz_init (res);
  mpz_init (ref);

  for (i = 0; i < COUNT; i++)
    {
      mini_random_op4 (OP_POWM, MAXBITS, b, e, m, ref);
      mpz_powm (res, b, e, m);
      if (mpz_cmp (res, ref))
	{
	  fprintf (stderr, "mpz_powm failed:\n");
	  dump ("b", b);
	  dump ("e", e);
	  dump ("m", m);
	  dump ("r", res);
	  dump ("ref", ref);
	  abort ();
	}
    }

  /* res >= 0, come from the random choices above, */
  if (mpz_cmp_ui (res, 1) <= 0) /* if too small, */
    mpz_add_ui (res, res, 9); /* add an arbitrary value. */

  mpz_set_ui (e, 0);
  /* Test the case m^0 (mod m), expect 1 (m is greater than 1). */
  mpz_powm (res, res, e, res);
  if (mpz_cmp_ui (res, 1) != 0)
    {
      fprintf (stderr, "mpz_powm failed: b=m, e=0; 1 expected,\n");
      dump ("m", res);
      dump ("r", res);
      abort ();
    }

  /* Now res is 1. */
  /* Test the case 1^0 (mod 1), expect 0. */
  mpz_powm (res, res, e, res);
  if (mpz_size (res))
    {
      fprintf (stderr, "mpz_powm failed: b=1, e=0, m=1; 0 expected,\n");
      dump ("r", res);
      abort ();
    }

  mpz_clear (b);
  mpz_clear (e);
  mpz_clear (m);
  mpz_clear (res);
  mpz_clear (ref);
}
