/* Symbols with version hash zero should not match any version (bug 29190).
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <support/check.h>
#include <support/xdlfcn.h>
#include <stddef.h>
#include <string.h>

static int
do_test (void)
{
  void *handle = xdlopen ("tst-version-hash-zero-mod.so", RTLD_NOW);

  /* This used to crash because some struct r_found_version entries
     with hash zero did not have valid version strings.  */
  TEST_VERIFY (xdlvsym (handle, "global_variable", "PPPPPPPPPPPP") != NULL);

  /* Consistency check.  */
  TEST_VERIFY (xdlsym (handle, "global_variable")
               == xdlvsym (handle, "global_variable", "PPPPPPPPPPPP"));

  /* This symbol version is supposed to be missing.  */
  TEST_VERIFY (dlvsym (handle, "global_variable", "OTHER_VERSION") == NULL);

  /* tst-version-hash-zero-refmod.so references
     global_variable@@OTHER_VERSION and is expected to fail to load.
     dlvsym sets the hidden flag during lookup.  Relocation does not,
     so this exercises a different failure case.  */
  TEST_VERIFY_EXIT (dlopen ("tst-version-hash-zero-refmod.so", RTLD_NOW)
                    == NULL);
  const char *message = dlerror ();
  if (strstr (message,
              ": undefined symbol: global_variable, version OTHER_VERSION")
      == NULL)
    FAIL_EXIT1 ("unexpected dlopen failure: %s", message);

  xdlclose (handle);
  return 0;
}

#include <support/test-driver.c>
