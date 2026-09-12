/* Check if static-TLS variables are correctly intialized in IFUNC resolvers.
   Copyright (C) 2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* Checks if a IFUNC resolver sees if a TLS blocker is properly initialized.
   The tst-ifunc-tls-init-lib.so carries:

   - a thread-local 'sentinel' initialised to SENTINEL.
   - an IFUNC 'ifunc_tls' whose resolver picks impl_ok when sentinel reads
     as SENTINEL and impl_bad (returning 0) otherwise.
   - an IFUNC-backed function-pointer global 'fptr' whose initialiser
     produces a non-PLT relocation.  */

#include <support/check.h>
#include <support/xdlfcn.h>

#define SENTINEL 0x5A5A1234

extern int ifunc_tls (void);
extern int (*fptr) (void);
extern int get_last_seen_sentinel (void);

static void
test_tls_ifunc (int (*check_sentinel)(void),
		int (*check_fptr)(void),
		int (*check_ifunc_tls)(void))
{
  /* Force the IFUNC to be resolved before inspecting what the resolver
     observed.  In a non-PIE executable the references to the IFUNC are
     satisfied through a canonical IPLT entry in the executable, which under
     lazy binding is resolved on first use rather than during startup
     relocation.  Calling fptr/ifunc_tls here guarantees the resolver has run
     regardless of PIE-ness or binding mode.   */
  TEST_VERIFY (check_fptr != NULL);
  TEST_COMPARE (check_fptr (), SENTINEL);

  /* Issue the ifunc directly as well.  */
  TEST_COMPARE (check_ifunc_tls (), SENTINEL);

  /* Primary check: 'get_last_seen_sentinel' returns the value of the DSO's
     thread-local 'sentinel' as observed by the resolver at the moment it ran
     for the IFUNC reloc that initialised fptr.  The getter is a regular
     PLT-resolved function in the DSO, so the read of the diagnostic global
     does NOT go through a COPY relocation that could overwrite the resolver's
     write.  */
  TEST_COMPARE (check_sentinel (), SENTINEL);
}

static int
do_test (void)
{
  test_tls_ifunc (get_last_seen_sentinel, fptr, ifunc_tls);

  /* Same as before, but check the dlopen path.  */
  void *handle = xdlopen ("tst-ifunc-tls-init-lib2.so",
			  RTLD_LAZY | RTLD_LOCAL);

  int (*get_last_seen_sentinel_dlopen) (void)
    = xdlsym (handle, "get_last_seen_sentinel");
  int (**fptr_dlopen) (void) = xdlsym (handle, "fptr");
  int (*ifunc_tls_dlopen) (void) = xdlsym (handle, "ifunc_tls");

  test_tls_ifunc (get_last_seen_sentinel_dlopen, *fptr_dlopen,
		  ifunc_tls_dlopen);

  xdlclose (handle);

  return 0;
}

#include <support/test-driver.c>
