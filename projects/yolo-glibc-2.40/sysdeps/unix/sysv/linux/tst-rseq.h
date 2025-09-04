/* Restartable Sequences tests header.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.

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

#include <errno.h>
#include <error.h>
#include <stdbool.h>
#include <stdint.h>
#include <support/check.h>
#include <syscall.h>
#include <sys/rseq.h>
#include <tls.h>

static inline bool
rseq_thread_registered (void)
{
  return THREAD_GETMEM_VOLATILE (THREAD_SELF, rseq_area.cpu_id) >= 0;
}

static inline int
sys_rseq (struct rseq *rseq_abi, uint32_t rseq_len, int flags, uint32_t sig)
{
  return syscall (__NR_rseq, rseq_abi, rseq_len, flags, sig);
}

static inline bool
rseq_available (void)
{
  int rc;

  rc = sys_rseq (NULL, 0, 0, 0);
  if (rc != -1)
    FAIL_EXIT1 ("Unexpected rseq return value %d", rc);
  switch (errno)
    {
    case ENOSYS:
      return false;
    case EINVAL:
      /* rseq is implemented, but detected an invalid rseq_len parameter.  */
      return true;
    default:
      FAIL_EXIT1 ("Unexpected rseq error %s", strerror (errno));
    }
}
