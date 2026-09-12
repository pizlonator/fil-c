/* Test that name lookups do not crash after a failing res_init call.
   Copyright The GNU Toolchain Authors.
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

/* When res_init is called on a thread whose resolver state has
   already been initialized, it unconditionally invokes
   __res_iclose(&_res, true) before __res_vinit.  __res_iclose used
   to NULL every _u._ext.nsaddrs[ns] without resetting
   _u._ext.nscount, breaking the invariant that
   __res_context_send's validation loop relies on.  If __res_vinit
   then failed (for example, because fopen("/etc/resolv.conf")
   returned EMFILE), the resolver state was left with
   _u._ext.nscount > 0 and _u._ext.nsaddrs[*] == NULL.  The next
   name lookup walked the validation loop and dereferenced NULL in
   sock_eq.

   This test reproduces the crash:
     1. perform a name lookup so __res_context_send populates
        _u._ext.nsaddrs[] and sets _u._ext.nscount;
     2. drop RLIMIT_NOFILE so fopen("/etc/resolv.conf") fails with
        EMFILE on the next call;
     3. call res_init, which must report failure;
     4. perform another name lookup, which must not crash.  */

#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <sys/resource.h>

#include <support/check.h>
#include <support/resolv_test.h>
#include <support/xunistd.h>

static void
response (const struct resolv_response_context *ctx,
	  struct resolv_response_builder *b,
	  const char *qname, uint16_t qclass, uint16_t qtype)
{
  resolv_response_init (b, (struct resolv_response_flags) { 0 });
  resolv_response_add_question (b, qname, qclass, qtype);
  resolv_response_section (b, ns_s_an);
  resolv_response_open_record (b, qname, qclass, qtype, 0);
  if (qtype == T_A)
    {
      const char ipv4[4] = { 192, 0, 2, 1 };
      resolv_response_add_data (b, ipv4, sizeof (ipv4));
    }
  resolv_response_close_record (b);
}

static int
do_test (void)
{
  struct resolv_test *aux = resolv_test_start
    ((struct resolv_redirect_config)
     {
       .response_callback = response,
       /* Disable TCP servers to prevent test failures due to a race
	  condition: If the initial setrlimit call below executes
	  before the kernel has allocated the client FD in the accept
	  system call on the TCP DNS server threads, the system call
	  fails even if no client connections arrive.  (The FD is
	  pre-allocated.)  */
       .servers =
	 {
	   { .disable_tcp = true, },
	   { .disable_tcp = true, },
	   { .disable_tcp = true, },
	 },
     });

  /* Initial lookup.  This drives __res_context_send through its init
     block, which allocates _u._ext.nsaddrs[] and sets
     _u._ext.nscount to _res.nscount.  */
  struct hostent *h = gethostbyname ("primer.example");
  TEST_VERIFY_EXIT (h != NULL);

  /* Drop RLIMIT_NOFILE so that the next fopen of /etc/resolv.conf
     inside __resolv_conf_load fails with EMFILE.  Already-open
     descriptors (including those held by the resolv test harness)
     remain valid; only new descriptor allocations are blocked.  */
  struct rlimit rl;
  TEST_COMPARE (getrlimit (RLIMIT_NOFILE, &rl), 0);
  rlim_t saved_cur = rl.rlim_cur;
  rl.rlim_cur = 3;
  TEST_COMPARE (setrlimit (RLIMIT_NOFILE, &rl), 0);

  /* res_init must report failure: __res_iclose has already cleared
     the extended state, __res_vinit then fails to reload
     /etc/resolv.conf.  */
  TEST_COMPARE (res_init (), -1);

  /* The repro: without the fix in __res_iclose, this call would
     enter __res_context_send's validation loop with stale
     _u._ext.nscount and NULL _u._ext.nsaddrs[0], dereference NULL
     in sock_eq, and SIGSEGV.  We do not care whether the lookup
     succeeds (it will likely fail because socket() also returns
     EMFILE), only that it returns without crashing.  */
  (void) gethostbyname ("after.example");

  /* Restore the descriptor limit before resolv_test_end so its
     cleanup work is not constrained.  */
  rl.rlim_cur = saved_cur;
  TEST_COMPARE (setrlimit (RLIMIT_NOFILE, &rl), 0);

  resolv_test_end (aux);
  return 0;
}

#include <support/test-driver.c>
