/* Check that an IFUNC resolver in a dlopen'd DSO can read .tdata-initialised
   __thread storage when the TLS access is compiled as a TLSDESC sequence.
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

/* Sibling of tst-ifunc-tls-init-gd-ld, but the TLS access in the loaded
   DSO is compiled with the TLSDESC dialect (x86 gnu2, aarch64 desc).  */

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

static int
do_test (void)
{
  void *handle = xdlopen ("tst-ifunc-tls-init-tlsdesc-lib.so",
			  RTLD_LAZY | RTLD_LOCAL);

  struct ifunc_handles h;
  h.get_last_seen_sentinel = xdlsym (handle, "get_last_seen_sentinel");
  h.fptr = xdlsym (handle, "fptr");
  h.ifunc_tls = xdlsym (handle, "ifunc_tls");

  TEST_COMPARE (h.get_last_seen_sentinel (), SENTINEL);

  TEST_VERIFY (*h.fptr != NULL);
  TEST_COMPARE ((*h.fptr) (), SENTINEL);
  TEST_COMPARE (h.ifunc_tls (), SENTINEL);

  pthread_t consumer = xpthread_create (NULL, ifunc_caller, &h);
  xpthread_join (consumer);

  xdlclose (handle);
  return 0;
}

#include <support/test-driver.c>
