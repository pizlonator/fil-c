/*-*- mode:c;indent-tabs-mode:t;c-basic-offset:8;tab-width:8;coding:utf-8   -*-│
│ vi: set noet ft=c ts=8 sw=8 fenc=utf-8                                   :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2026 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "third_party/nsync/timed.h"
#include "libc/cosmotime.h"
#include "libc/errno.h"
#include "third_party/nsync/time.h"
#include "libc/sysv/consts/clock.h"
#include "third_party/nsync/time.h"

/* Exercise the entire mu test suite against nsync_mu_timedlock() by routing
   every nsync_mu_lock() call through a no-deadline timedlock, which must
   always succeed and behave identically to nsync_mu_lock().  A real wrapper
   function (not a macro body) is required because the suite also takes the
   address &nsync_mu_lock to pass as a function pointer. */
static int timedlock_nodeadline (nsync_mu *mu) {
	int res = nsync_mu_timedlock (mu, nsync_time_no_deadline);
	unassert (res == 0);
	return 0;
}

static int timedlock_spinshort (nsync_mu *mu) {
	int clock = CLOCK_REALTIME;
	nsync_time timeout = timespec_frommillis(50);
	for (;;) {
		nsync_time now = nsync_time_now (clock);
		nsync_time abs_deadline = nsync_time_add (now, timeout);
		int res = nsync_mu_clocklock (mu, clock, abs_deadline);
		unassert (res == 0 ||
			  res == ETIMEDOUT);
		if (res == 0)
			return 0;
	}
}

#define nsync_mu_lock timedlock_spinshort

#include "third_party/nsync/testing/mu_test.inc"
