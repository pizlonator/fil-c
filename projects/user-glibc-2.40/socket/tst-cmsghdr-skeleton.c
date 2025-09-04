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

/* We use the preprocessor to generate the function/macro tests instead of
   using indirection because having all the macro expansions alongside
   each other lets the compiler warn us about suspicious pointer
   arithmetic across subsequent CMSG_{FIRST,NXT}HDR expansions.  */

#include <stdint.h>

#define RUN_TEST_CONCAT(suffix) run_test_##suffix
#define RUN_TEST_FUNCNAME(suffix) RUN_TEST_CONCAT (suffix)

static void
RUN_TEST_FUNCNAME (CMSG_NXTHDR_IMPL) (void)
{
  struct msghdr m = {0};
  struct cmsghdr *cmsg;
  char cmsgbuf[3 * CMSG_SPACE (sizeof (PAYLOAD))] = {0};

  m.msg_control = cmsgbuf;
  m.msg_controllen = sizeof (cmsgbuf);

  /* First header should point to the start of the buffer.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);

  /* If the first header length consumes the entire buffer, there is no
     space remaining for additional headers.  */
  cmsg->cmsg_len = sizeof (cmsgbuf);
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg == NULL);

  /* The first header length is so big, using it would cause an overflow.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);
  cmsg->cmsg_len = (__typeof (cmsg->cmsg_len)) SIZE_MAX;
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg == NULL);

  /* The first header leaves just enough space to hold another header.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);
  cmsg->cmsg_len = sizeof (cmsgbuf) - sizeof (struct cmsghdr);
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg != NULL);

  /* The first header leaves space but not enough for another header.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);
  cmsg->cmsg_len ++;
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg == NULL);

  /* The second header leaves just enough space to hold another header.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);
  cmsg->cmsg_len = CMSG_LEN (sizeof (PAYLOAD));
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg != NULL);
  cmsg->cmsg_len = sizeof (cmsgbuf)
                   - CMSG_SPACE (sizeof (PAYLOAD)) /* First header.  */
                   - sizeof (struct cmsghdr);
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg != NULL);

  /* The second header leaves space but not enough for another header.  */
  cmsg = CMSG_FIRSTHDR (&m);
  TEST_VERIFY_EXIT ((char *) cmsg == cmsgbuf);
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg != NULL);
  cmsg->cmsg_len ++;
  cmsg = CMSG_NXTHDR_IMPL (&m, cmsg);
  TEST_VERIFY_EXIT (cmsg == NULL);

  return;
}
