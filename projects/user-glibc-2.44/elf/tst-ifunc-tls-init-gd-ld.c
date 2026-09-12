/* Check if dynamic-TLS variables (global-dynamic, local-dynamic) are
   correctly initialised in IFUNC resolvers reached via dlopen.
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

/* This test dlopens three modules covering the dynamic-TLS access variants:

     gd-lib         file-local 'sentinel' + tls_model("global-dynamic")
     ld-lib         file-local 'sentinel' + tls_model("local-dynamic")
     gd-global-lib  external 'sentinel' + tls_model("global-dynamic")

   In each case the IFUNC resolver's read of 'sentinel' must traverse the
   dynamic-TLS resolution path at the moment the resolver fires during
   dlopen-time relocation.

   Each variant additionally calls the resolved IFUNC from a thread spawned
   after dlopen, to verify per-thread DTV propagation for the newly-loaded
   module.  */

#include <support/check.h>
#include <support/xdlfcn.h>
#include <support/xthread.h>

#define SENTINEL 0x5A5A1234

struct ifunc_handles
{
  int (*get_last_seen_sentinel) (void);
  int (**fptr) (void);
  int (*ifunc_tls) (void);
};

static void *
ifunc_caller (void *arg)
{
  struct ifunc_handles *h = arg;
  TEST_COMPARE ((*h->fptr) (), SENTINEL);
  TEST_COMPARE (h->ifunc_tls (), SENTINEL);
  return NULL;
}

static void
test_lib (const char *soname)
{
  void *handle = xdlopen (soname, RTLD_LAZY | RTLD_LOCAL);

  struct ifunc_handles h;
  h.get_last_seen_sentinel = xdlsym (handle, "get_last_seen_sentinel");
  h.fptr = xdlsym (handle, "fptr");
  h.ifunc_tls = xdlsym (handle, "ifunc_tls");

  TEST_COMPARE (h.get_last_seen_sentinel (), SENTINEL);

  TEST_VERIFY (*h.fptr != NULL);
  TEST_COMPARE ((*h.fptr) (), SENTINEL);
  TEST_COMPARE (h.ifunc_tls (), SENTINEL);

  /* From a thread spawned *after* the dlopen, which exercises DTV propagation
     for the new module into a fresh TCB.  */
  pthread_t consumer = xpthread_create (NULL, ifunc_caller, &h);
  xpthread_join (consumer);

  xdlclose (handle);
}

static int
do_test (void)
{
  test_lib ("tst-ifunc-tls-init-gd-lib.so");
  test_lib ("tst-ifunc-tls-init-ld-lib.so");
  test_lib ("tst-ifunc-tls-init-gd-global-lib.so");
  return 0;
}

#include <support/test-driver.c>
