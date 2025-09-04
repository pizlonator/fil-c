/*
 * Copyright 2015 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config UEVENT
//config:	bool "uevent (3.5 kb)"
//config:	default y
//config:	help
//config:	uevent is a netlink listener for kernel uevent notifications
//config:	sent via netlink. It is usually used for dynamic device creation.

//applet:IF_UEVENT(APPLET(uevent, BB_DIR_SBIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_UEVENT) += uevent.o

//usage:#define uevent_trivial_usage
//usage:       "[PROG ARGS]"
//usage:#define uevent_full_usage "\n\n"
//usage:       "uevent runs PROG for every netlink notification."
//usage:   "\n""PROG's environment contains data passed from the kernel."
//usage:   "\n""Typical usage (daemon for dynamic device node creation):"
//usage:   "\n""	# uevent mdev & mdev -s"

#include "libbb.h"
#include "common_bufsiz.h"
#include <linux/netlink.h>

#define env ((char **)bb_common_bufsiz1)
#define INIT_G() do { setup_common_bufsiz(); } while (0)
enum {
	MAX_ENV = COMMON_BUFSIZE / sizeof(char*) - 1,
	// ^^^sizeof(env[0]) instead of sizeof(char*)
	// makes gcc-6.3.0 emit "strict-aliasing" warning.

	// socket receive buffer of 2MiB proved to be too small:
	//  http://lists.busybox.net/pipermail/busybox/2019-December/087665.html
	// udevd seems to use a whooping 128MiB.
	// The socket receive buffer size is just a resource limit.
	// The buffers are allocated lazily so the memory is not wasted.
	KERN_RCVBUF = 128 * 1024 * 1024,

	// Might be made smaller: the kernel v5.4 passes up to 32 environment
	// variables with a total of 2kb on each event.
	// On top of that the action string and device path are added.
	USER_RCVBUF = 16 * 1024,
};

int uevent_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int uevent_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;

	INIT_G();

	argv++;

	// Subscribe for UEVENT kernel messages.
	// Without a sufficiently big RCVBUF, a ton of simultaneous events
	// can trigger ENOBUFS on read, which is unrecoverable.
	// Reproducer:
	//	uevent mdev &
	// 	find /sys -name uevent -exec sh -c 'echo add >"{}"' ';'
 reopen:
	fd = create_and_bind_to_netlink(NETLINK_KOBJECT_UEVENT, /*groups:*/ 1 << 0, KERN_RCVBUF);

	for (;;) {
		char *netbuf;
		char *s, *end;
		ssize_t len;
		int idx;

		// In many cases, a system sits for *days* waiting
		// for a new uevent notification to come in.
		// We use a fresh mmap so that buffer is not allocated
		// until kernel actually starts filling it.
		netbuf = xmmap_anon(USER_RCVBUF);

		// Here we block, possibly for a very long time
		len = safe_read(fd, netbuf, USER_RCVBUF - 1);
		if (len < 0) {
			if (errno == ENOBUFS) {
				// Ran out of socket receive buffer
				bb_simple_error_msg("uevent overrun");
				close(fd);
				munmap(netbuf, USER_RCVBUF);
				goto reopen;
			}
			bb_simple_perror_msg_and_die("read");
		}
		end = netbuf + len;
		*end = '\0';

		// Each netlink message starts with "ACTION@/path"
		// (which we currently ignore),
		// followed by environment variables.
		if (!argv[0])
			putchar('\n');
		idx = 0;
		s = netbuf;
		while (s < end) {
			if (!argv[0])
				puts(s);
			if (strchr(s, '=') && idx < MAX_ENV)
				env[idx++] = s;
			s += strlen(s) + 1;
		}
		env[idx] = NULL;

		if (argv[0]) {
			idx = 0;
			while (env[idx])
				putenv(env[idx++]);
			spawn_and_wait(argv);
			idx = 0;
			while (env[idx])
				bb_unsetenv(env[idx++]);
		}
		munmap(netbuf, USER_RCVBUF);
	}

	return 0; // not reached
}
