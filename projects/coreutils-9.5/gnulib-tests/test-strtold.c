/* Test of strtold() in the "C" locale.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.

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

/* Written by Eric Blake.  */

#include <config.h>

/* Specification.  */
#include <stdlib.h>

#include "signature.h"
SIGNATURE_CHECK (strtold, long double, (char const *, char **));

#include <errno.h>
#include <float.h>
#include <math.h>
#include <string.h>

#include "isnanl-nolibm.h"
#include "minus-zero.h"
#include "macros.h"

#include "test-strtold.h"

int
main (void)
{
  test_function (strtold);

  return 0;
}
