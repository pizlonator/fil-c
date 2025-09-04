/* Test ancillary data header creation.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <sys/socket.h>
#include <gnu/lib-names.h>
#include <support/xdlfcn.h>
#include <support/check.h>

#define PAYLOAD "Hello, World!"

/* CMSG_NXTHDR is a macro that calls an inline function defined in
   bits/socket.h.  In case the function cannot be inlined, libc.so carries
   a copy.  Both versions need to be tested.  */

#define CMSG_NXTHDR_IMPL CMSG_NXTHDR
#include "tst-cmsghdr-skeleton.c"
#undef CMSG_NXTHDR_IMPL

static struct cmsghdr * (* cmsg_nxthdr) (struct msghdr *, struct cmsghdr *);

#define CMSG_NXTHDR_IMPL cmsg_nxthdr
#include "tst-cmsghdr-skeleton.c"
#undef CMSG_NXTHDR_IMPL

static int
do_test (void)
{
  static void *handle;

  run_test_CMSG_NXTHDR ();

  handle = xdlopen (LIBC_SO, RTLD_LAZY);
  cmsg_nxthdr = (struct cmsghdr * (*) (struct msghdr *, struct cmsghdr *))
                  xdlsym (handle, "__cmsg_nxthdr");

  run_test_cmsg_nxthdr ();

  return 0;
}

#include <support/test-driver.c>
