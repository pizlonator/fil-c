/*
 * signames.c - ps signal names
 *
 * Copyright © 2023      Craig Small <csmall@dropbear.xyz>
 * Copyright © 2020      Luis Chamberlain <mcgrof@kernel.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "common.h"
#include "signals.h"

/* For libraries like musl */
#ifndef __SIGRTMIN
#define __SIGRTMIN SIGRTMIN
#endif
#ifndef __SIGRTMAX
#define __SIGRTMAX SIGRTMAX
#endif

/*
 * The actual list of unsupported signals varies by operating system. This
 * program is Linux specific as it processes /proc/ for signal information and
 * there is no generic way to extract each process signal information for each
 * OS. This program also relies on Linux glibc defines to figure out which
 * signals are reserved for use by libc and then which ones are real time
 * specific.
 */

/*
 * As per glibc:
 *
 * A system that defines real time signals overrides __SIGRTMAX to be something
 * other than __SIGRTMIN. This also means we can count on __SIGRTMIN being the
 * first real time signal, meaning what Linux programs it for your architecture
 * in the kernel. SIGRTMIN then will be the application specific first real
 * time signal, that is, on top of libc. The values in between
 *
 * 	__SIGRTMIN .. SIGRTMIN
 *
 * are used by * libc, typically for helping threading implementation.
 */
static const char *sigstat_strsignal_abbrev(int sig, char *abbrev, size_t len)
{
	memset(abbrev, '\0', len);

	if (sig == 0 || sig >= NSIG) {
		snprintf(abbrev, len, "BOGUS_%02d", sig - _NSIG);
		return abbrev;
	}

	/*
	 * The standard lower signals we can count on this being the kernel
	 * specific SIGRTMIN.
	 */
	if (sig < __SIGRTMIN) {
                const char *signame = NULL;
#ifdef HAVE_SIGABBREV_NP
                signame = sigabbrev_np(sig);
#else
                signame = signal_number_to_name(sig);
#endif /* HAVE_SIGABBREV_NP */
                if (signame != NULL && signame[0] != '\0') {
                    strncpy(abbrev, signame, len);
                    return abbrev;
                }
	}

	/* This means your system should *not* have realtime signals */
	if (__SIGRTMAX == __SIGRTMIN) {
		snprintf(abbrev, len, "INVALID_%02d", sig);
		return abbrev;
	}

	/*
	 * If we're dealing with a libc real time signal start counting
	 * after libc's version of SIGRTMIN
	 */
	if (sig >= SIGRTMIN) {
		if (sig == SIGRTMIN)
			snprintf(abbrev, len, "RTMIN");
		else if (sig == SIGRTMAX)
			snprintf(abbrev, len, "RTMAX");
		else
			snprintf(abbrev, len, "RTMIN+%02d", sig - SIGRTMIN);
	} else
		snprintf(abbrev, len, "LIBC+%02d", sig - __SIGRTMIN);

	return abbrev;
}

/*
 * For instance SIGTERM is 15, but its actual mask value is
 * 1 << (15-1) = 0x4000
 */
static uint64_t mask_sig_val_num(int signum)
{
	return ((uint64_t) 1 << (signum -1));
}

int print_signame(char *restrict const outbuf, const char *restrict const sig, const size_t len_in)
{
	unsigned int i;
	char abbrev[PATH_MAX];
	unsigned int n = 0;
	char *c = outbuf;
        size_t len = len_in;
	uint64_t mask, mask_in;
	uint64_t test_val = 0;

        if (1 != sscanf(sig, "%" PRIu64, &mask_in))
            return 0;
        mask = mask_in;

	for (i=1; i < NSIG; i++) {
		test_val = mask_sig_val_num(i);
		if (test_val & mask) {
                        n = strlen(sigstat_strsignal_abbrev(i, abbrev, PATH_MAX));
                        if (n+1 >= len) { // +1 for the '+'
                            strcpy(c, "+");
                            len -= 1;
                            c += 1;
                            break;
                        } else {
			    n = snprintf(c, len, (c==outbuf)?"%s":",%s",
				     sigstat_strsignal_abbrev(i, abbrev,
					   		      PATH_MAX));
			    len -= n;
			    c+=n;
                        }
		}
	}
        if (c == outbuf) {
            n = snprintf(c, len, "%c", '-');
            len -= n;
            c += n;
        }
	return (int) (c-outbuf);
}
