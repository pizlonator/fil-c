/* Test of libgmp or its mini-gmp substitute.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

/* Specification.  */
#include <gmp.h>

#include <limits.h>
#include <string.h>

#include "macros.h"

#ifndef MINI_GMP_LIMB_TYPE
/* Verify that the gmp.h header file was generated for the same
   machine word size as we are using.  */
static_assert (GMP_NUMB_BITS == sizeof (mp_limb_t) * CHAR_BIT);
#endif

int
main ()
{
#ifndef MINI_GMP_LIMB_TYPE
  /* Verify that the gmp.h header file and the libgmp library come from
     the same GMP version.  */
  {
    char gmp_header_version[32];
    sprintf (gmp_header_version, "%d.%d.%d", __GNU_MP_VERSION,
             __GNU_MP_VERSION_MINOR, __GNU_MP_VERSION_PATCHLEVEL);
    if (strcmp (gmp_version, gmp_header_version) != 0)
      {
        char gmp_header_version2[32];
        if (__GNU_MP_VERSION_PATCHLEVEL > 0
            || (sprintf (gmp_header_version2, "%d.%d", __GNU_MP_VERSION,
                         __GNU_MP_VERSION_MINOR),
                strcmp (gmp_version, gmp_header_version2) != 0))
          {
            fprintf (stderr,
                     "gmp header version (%s) does not match gmp library version (%s).\n",
                     gmp_header_version, gmp_version);
            exit (1);
          }
      }
  }
#endif

  /* A simple sanity check that 2 + 2 = 4.  */
  static mp_limb_t const twobody[] = { 2 };
  static mpz_t const two = MPZ_ROINIT_N ((mp_limb_t *) twobody, 1);
  ASSERT (mpz_fits_slong_p (two));
  ASSERT (mpz_get_si (two) == 2);

  mpz_t four;
  mpz_init (four);
  mpz_add (four, two, two);
  ASSERT (mpz_fits_slong_p (four));
  ASSERT (mpz_get_si (four) == 4);
  mpz_clear (four);

  return 0;
}
