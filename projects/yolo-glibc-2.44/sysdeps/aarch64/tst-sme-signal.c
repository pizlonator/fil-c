/* Test handling of SME state in a signal handler.
   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include "tst-sme-skeleton.c"

#include <support/xsignal.h>

static struct _aarch64_ctx *
extension (void *p)
{
  return p;
}

#ifndef TPIDR2_MAGIC
#define TPIDR2_MAGIC 0x54504902
#endif

#ifndef ZA_MAGIC
#define ZA_MAGIC 0x54366345
#endif

#ifndef ZT_MAGIC
#define ZT_MAGIC 0x5a544e01
#endif

#ifndef EXTRA_MAGIC
#define EXTRA_MAGIC 0x45585401
#endif

/* We use a pipe to make sure that the final check of the SME state
   happens after signal handler finished.  */
static int pipefd[2];

#define WRITE(msg) xwrite (1, msg, sizeof (msg));

static void
handler (int signo, siginfo_t *si, void *ctx)
{
  TEST_VERIFY (signo == SIGUSR1);
  WRITE ("in the handler\n");
  check_sme_za_state ("during signal", true /* State is clear.  */);
  ucontext_t *uc = ctx;
  void *p = uc->uc_mcontext.__reserved;
  unsigned int found = 0;
  uint32_t m;
  while ((m = extension (p)->magic))
    {
      if (m == TPIDR2_MAGIC)
        {
          WRITE ("found TPIDR2_MAGIC\n");
          found += 1;
        }
      if (m == ZA_MAGIC)
        {
          WRITE ("found ZA_MAGIC\n");
          found += 1;
        }
      if (m == ZT_MAGIC)
        {
          WRITE ("found ZT_MAGIC\n");
          found += 1;
        }
      if (m == EXTRA_MAGIC)
        {
          WRITE ("found EXTRA_MAGIC\n");
          struct { struct _aarch64_ctx h; uint64_t data; } *e = p;
          p = (char *)e->data;
          continue;
        }
      p = (char *)p + extension (p)->size;
    }
  TEST_COMPARE (found, 3);

  /* Signal that the wait is over (see below).  */
  char message = '\0';
  xwrite (pipefd[1], &message, 1);
}

static void
run (struct blk *blk)
{
  xpipe (pipefd);

  struct sigaction sigact;
  sigemptyset (&sigact.sa_mask);
  sigact.sa_flags = 0;
  sigact.sa_flags |= SA_SIGINFO;
  sigact.sa_sigaction = handler;
  xsigaction (SIGUSR1, &sigact, NULL);

  enable_sme_za_state (blk);
  check_sme_za_state ("before signal", false /* State is not clear.  */);
  xraise (SIGUSR1);

  /* Wait for signal handler to complete.  */
  char response;
  xread (pipefd[0], &response, 1);

  check_sme_za_state ("after signal", false /* State is not clear.  */);
}
