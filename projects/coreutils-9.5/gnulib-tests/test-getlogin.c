/* Test of getting user name.
   Copyright (C) 2010-2024 Free Software Foundation, Inc.

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

/* Written by Bruno Haible and Paul Eggert.  */

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (getlogin, char *, (void));

#include "test-getlogin.h"

int
main (void)
{
  /* Test value.  */
  char *buf = getlogin ();
  int err = buf ? 0 : errno;
#if defined __sun
  if (buf == NULL && err == 0)
    {
      /* This can happen on Solaris 11 OpenIndiana in the MATE desktop.  */
      fprintf (stderr, "Skipping test: no entry in /var/adm/utmpx.\n");
      exit (77);
    }
#endif
  test_getlogin_result (buf, err);

  return 0;
}
