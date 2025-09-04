/*
 * Copyright 2010 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config NBDCLIENT
//config:	bool "nbd-client (6.3 kb)"
//config:	default y
//config:	help
//config:	Network block device client

//applet:IF_NBDCLIENT(APPLET_NOEXEC(nbd-client, nbdclient, BB_DIR_USR_SBIN, BB_SUID_DROP, nbdclient))

//kbuild:lib-$(CONFIG_NBDCLIENT) += nbd-client.o

#include "libbb.h"
#include <netinet/tcp.h>
#include <linux/fs.h>
#include <getopt.h>

#define NBD_SET_SOCK          _IO(0xab, 0)
#define NBD_SET_BLKSIZE       _IO(0xab, 1)
#define NBD_SET_SIZE          _IO(0xab, 2)
#define NBD_DO_IT             _IO(0xab, 3)
#define NBD_CLEAR_SOCK        _IO(0xab, 4)
#define NBD_CLEAR_QUEUE       _IO(0xab, 5)
#define NBD_PRINT_DEBUG       _IO(0xab, 6)
#define NBD_SET_SIZE_BLOCKS   _IO(0xab, 7)
#define NBD_DISCONNECT        _IO(0xab, 8)
#define NBD_SET_TIMEOUT       _IO(0xab, 9)
#define NBD_SET_FLAGS         _IO(0xab, 10)

//usage:#define nbdclient_trivial_usage
//usage:       "{ [-b BLKSIZE] [-N NAME] [-t SEC] [-p] HOST [PORT] | -d } BLOCKDEV"
//usage:#define nbdclient_full_usage "\n\n"
//usage:       "Connect to HOST and provide network block device on BLOCKDEV"

//TODO: more compat with nbd-client version 3.17 -
//nbd-client host [ port ] nbd-device [ -connections num ] [ -sdp ] [ -swap ]
//	[ -persist ] [ -nofork ] [ -nonetlink ] [ -systemd-mark ]
//	[ -block-size block size ] [ -timeout seconds ] [ -name name ]
//	[ -certfile certfile ] [ -keyfile keyfile ] [ -cacertfile cacertfile ]
//	[ -tlshostname hostname ]
//nbd-client -unix path nbd-device [ -connections num ] [ -sdp ] [ -swap ]
//	[ -persist ] [ -nofork ] [ -nonetlink ] [ -systemd-mark ]
//	[ -block-size block size ] [ -timeout seconds ] [ -name name ]
//nbd-client nbd-device
//nbd-client -d nbd-device
//nbd-client -c nbd-device
//nbd-client -l host [ port ]
//nbd-client [ -netlink ] -l host
//
//Default value for blocksize is 4096
//Allowed values for blocksize are 512,1024,2048,4096

int nbdclient_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nbdclient_main(int argc, char **argv)
{
#if BB_MMU
	bool nofork;
#endif
	bool opt_d;
	bool opt_p;
	const char *host, *port, *device;
	const char *name;
	unsigned blksize, size_blocks;
	unsigned timeout;
	int ch;
	struct nbd_header_t {
		uint64_t magic1; // "NBDMAGIC"
		uint64_t magic2; // old style: 0x420281861253 big endian
		//               // new style: 0x49484156454F5054 (IHAVEOPT)
	} nbd_header;
	struct old_nbd_header_t {
		uint64_t devsize;
		uint32_t flags;
		char data[124];
	} old_nbd_header;
	struct new_nbd_header_t {
		uint64_t devsize;
		uint16_t transmission_flags;
		char data[124];
	} new_nbd_header;
	struct nbd_opt_t {
		uint64_t magic;
		uint32_t opt;
		uint32_t len;
	} nbd_opts;

	static const struct option long_options[] = {
		{ "block-size", required_argument, NULL, 'b' },
		{ "timeout"   , required_argument, NULL, 't' },
		{ "name"      , required_argument, NULL, 'n' },
		{ "persist"   , no_argument      , NULL, 'p' },
		{ NULL }
	};

	BUILD_BUG_ON(offsetof(struct old_nbd_header_t, data) != 8 + 4);
	BUILD_BUG_ON(offsetof(struct new_nbd_header_t, data) != 8 + 2);

#if !BB_MMU
	bb_daemonize_or_rexec(DAEMON_CLOSE_EXTRA_FDS, argv);
#endif

	// Parse args. nbd-client uses stupid "one-dash long options" style :(
	// Even though short forms (-b,-t,-N,-p) exist for all long opts,
	// older manpages only contained long forms, which probably resulted
	// in many scripts using them.
	blksize = 4096;
	timeout = 0;
	name = ""; // use of "" instead of NULL simplifies strlen() later
	opt_d = opt_p = 0;
	while ((ch = getopt_long_only(argc, argv, "dN:", long_options, NULL)) != -1) {
		switch (ch) {
		case 'p':	// -persist
			opt_p = 1;
			break;
		case 'd':	// -d
			opt_d = 1;
			break;
		case 'b':	// -block-size
			blksize = xatou(optarg);
			break;
		case 't':	// -timeout
			timeout = xatou(optarg);
			break;
		case 'N':	// -N
		case 'n':	// -name
			name = optarg;
			break;
		default:
			bb_show_usage();
		}
	}
	argv += optind;

	if (opt_d) { // -d
		if (argv[0] && !argv[1]) {
			int nbd = xopen(argv[0], O_RDWR);
			ioctl(nbd, NBD_DISCONNECT);
			ioctl(nbd, NBD_CLEAR_SOCK);
			if (ENABLE_FEATURE_CLEAN_UP)
				close(nbd);
			return 0;
		}
		bb_show_usage();
	}

	// Allow only argv[] of: HOST [PORT] BLOCKDEV
	if (!argv[0] || !argv[1] || (argv[2] && argv[3])) {
		bb_show_usage();
	}

	host = argv[0];
	port = argv[2] ? argv[1] : "10809";
	device = argv[2] ? argv[2] : argv[1];

	// Repeat until spanked if -persist
#if BB_MMU
	nofork = 0;
#endif
	do {
		int sock, nbd;
		int ro;
		int proto_new; // 0 for old, 1 for new
#if BB_MMU
		char *data;
#endif

		// Make sure BLOCKDEV exists
		nbd = xopen(device, O_RDWR);

		// Find and connect to server
		sock = create_and_connect_stream_or_die(host, xatou16(port));
		setsockopt_1(sock, IPPROTO_TCP, TCP_NODELAY);

		// Log on to the server
		xread(sock, &nbd_header, 8 + 8);
		if (memcmp(&nbd_header.magic1, "NBDMAGIC",
				sizeof(nbd_header.magic1)) != 0
		) {
			bb_simple_error_msg_and_die("login failed");
		}
		if (memcmp(&nbd_header.magic2,
				"\x00\x00\x42\x02\x81\x86\x12\x53",
				sizeof(nbd_header.magic2)) == 0
		) {
			proto_new = 0;
		} else if (memcmp(&nbd_header.magic2, "IHAVEOPT", 8) == 0) {
			proto_new = 1;
		} else {
			bb_simple_error_msg_and_die("login failed");
		}

		if (!proto_new) {
			xread(sock, &old_nbd_header,
					sizeof(old_nbd_header.devsize) +
					sizeof(old_nbd_header.flags) +
					sizeof(old_nbd_header.data));
			size_blocks = SWAP_BE64(old_nbd_header.devsize) / blksize;
			ioctl(nbd, NBD_SET_BLKSIZE, (unsigned long) blksize);
			ioctl(nbd, NBD_SET_SIZE_BLOCKS, size_blocks);
			ioctl(nbd, NBD_CLEAR_SOCK);
			ro = !!(old_nbd_header.flags & htons(2));
#if BB_MMU
			data = old_nbd_header.data;
#endif
		} else {
			unsigned namelen;
			uint16_t handshake_flags;

			xread(sock, &handshake_flags, sizeof(handshake_flags));
			xwrite(sock, &const_int_0, sizeof(const_int_0)); // client_flags

			memcpy(&nbd_opts.magic, "IHAVEOPT",
					sizeof(nbd_opts.magic));
			nbd_opts.opt = htonl(1); // NBD_OPT_EXPORT_NAME
			namelen = strlen(name);
			nbd_opts.len = htonl(namelen);
			xwrite(sock, &nbd_opts,
					sizeof(nbd_opts.magic) +
					sizeof(nbd_opts.opt) +
					sizeof(nbd_opts.len));
			xwrite(sock, name, namelen);

			xread(sock, &new_nbd_header,
					sizeof(new_nbd_header.devsize) +
					sizeof(new_nbd_header.transmission_flags) +
					sizeof(new_nbd_header.data));
			size_blocks = SWAP_BE64(new_nbd_header.devsize) / blksize;
			ioctl(nbd, NBD_SET_BLKSIZE, (unsigned long) blksize);
			ioctl(nbd, NBD_SET_SIZE_BLOCKS, size_blocks);
			ioctl(nbd, NBD_CLEAR_SOCK);
			ioctl(nbd, NBD_SET_FLAGS,
					ntohs(new_nbd_header.transmission_flags));
			ro = !!(new_nbd_header.transmission_flags & htons(2));
#if BB_MMU
			data = new_nbd_header.data;
#endif
		}

		if (ioctl(nbd, BLKROSET, &ro) < 0) {
			bb_simple_perror_msg_and_die("BLKROSET");
		}

		if (timeout) {
			if (ioctl(nbd, NBD_SET_TIMEOUT, (unsigned long) timeout)) {
				bb_simple_perror_msg_and_die("NBD_SET_TIMEOUT");
			}
		}

		if (ioctl(nbd, NBD_SET_SOCK, sock)) {
			bb_simple_perror_msg_and_die("NBD_SET_SOCK");
		}

		//if (swap) mlockall(MCL_CURRENT|MCL_FUTURE);
#if BB_MMU
		// Open the device to force reread of the partition table.
		// Need to do it in a separate process, since open(device)
		// needs some other process to sit in ioctl(nbd, NBD_DO_IT).
		if (fork() == 0) {
			/* child */
			char *s = strrchr(device, '/');
			sprintf(data, "/sys/block/%.32s/pid", s ? s + 1 : device);
			// Is it up yet?
			for (;;) {
				int fd = open(data, O_RDONLY);
				if (fd >= 0) {
					if (ENABLE_FEATURE_CLEAN_UP)
						close(fd);
					break;
				}
				sleep1();
			}
			open(device, O_RDONLY);
			return 0;
		}

		// Daemonize here
		if (!nofork) {
			daemon(0, 0);
			nofork = 1;
		}
#endif
		// This turns us (the process that calls this ioctl)
		// into a dedicated NBD request handler.
		// We block here for a long time.
		// When exactly ioctl returns? On a signal,
		// or if someone does ioctl(NBD_DISCONNECT) [nbd-client -d].
		if (ioctl(nbd, NBD_DO_IT) >= 0 || errno == EBADR) {
			// Flush queue and exit
			ioctl(nbd, NBD_CLEAR_QUEUE);
			ioctl(nbd, NBD_CLEAR_SOCK);
			break;
		}

		close(sock);
		close(nbd);
	} while (opt_p);

	return 0;
}
