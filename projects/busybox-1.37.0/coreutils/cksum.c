/* vi: set sw=4 ts=4: */
/*
 * cksum - calculate the CRC32 checksum of a file
 *
 * Copyright (C) 2006 by Rob Sullivan, with ideas from code by Walter Harms
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config CKSUM
//config:	bool "cksum (4.3 kb)"
//config:	default y
//config:
//config:config CRC32
//config:	bool "crc32 (4.2 kb)"
//config:	default y

//                APPLET_NOEXEC:name   main   location        suid_type     help
//applet:IF_CKSUM(APPLET_NOEXEC(cksum, cksum, BB_DIR_USR_BIN, BB_SUID_DROP, cksum))
//applet:IF_CRC32(APPLET_NOEXEC(crc32, cksum, BB_DIR_USR_BIN, BB_SUID_DROP, cksum))
/* bb_common_bufsiz1 usage here is safe wrt NOEXEC: not expecting it to be zeroed. */

//kbuild:lib-$(CONFIG_CKSUM) += cksum.o
//kbuild:lib-$(CONFIG_CRC32) += cksum.o

//usage:#define cksum_trivial_usage
//usage:       "FILE..."
//usage:#define cksum_full_usage "\n\n"
//usage:       "Calculate CRC32 checksum of FILEs"

#include "libbb.h"
#include "common_bufsiz.h"

/* This is a NOEXEC applet. Be very careful! */

#define IS_CKSUM (ENABLE_CKSUM && (!ENABLE_CRC32 || applet_name[1] == 'k'))
#define IS_CRC32 (ENABLE_CRC32 && (!ENABLE_CKSUM || applet_name[1] == 'r'))

int cksum_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cksum_main(int argc UNUSED_PARAM, char **argv)
{
	uint32_t *crc32_table = crc32_filltable(NULL, IS_CKSUM);
	exitcode_t exit_code = EXIT_SUCCESS;

#if ENABLE_DESKTOP
	getopt32(argv, ""); /* cksum coreutils 6.9 compat */
	argv += optind;
#else
	argv++;
#endif

	setup_common_bufsiz();
	do {
		uint32_t crc;
		IF_CKSUM(off_t filesize;)
		const char *fname = *argv ? *argv : bb_msg_standard_input;
		int fd = open_or_warn_stdin(fname);

		if (fd < 0) {
			exit_code = EXIT_FAILURE;
			continue;
		}

		crc = IS_CKSUM ? 0 : 0xffffffff;
		IF_CKSUM(filesize = 0;)
#define read_buf bb_common_bufsiz1
		for (;;) {
			int bytes_read = safe_read(fd, read_buf, COMMON_BUFSIZE);
			if (bytes_read < 0)
				bb_simple_perror_msg_and_die(fname);
			if (bytes_read > 0) {
				IF_CKSUM(filesize += bytes_read;)
			} else {
				IF_CKSUM(uoff_t t;)

				close(fd);
				if (IS_CRC32)
					break;
#if ENABLE_CKSUM
				fd = -1; /* break flag */
				/* Checksum filesize bytes, LSB first */
				t = filesize;
				/*bytes_read = 0; - already is */
				while (t != 0) {
					read_buf[bytes_read++] = (uint8_t)t;
					t >>= 8;
				}
#endif
			}
			crc = (IS_CKSUM ? crc32_block_endian1 : crc32_block_endian0)(crc, read_buf, bytes_read, crc32_table);
			if (ENABLE_CKSUM && fd < 0)
				break;
		}

		crc = ~crc;
#if ENABLE_CKSUM
		if (IS_CKSUM)
			printf((*argv ? "%u %"OFF_FMT"u %s\n" : "%u %"OFF_FMT"u\n"),
				(unsigned)crc, filesize, *argv);
		else
#endif
			printf((*argv ? "%08x %s\n" : "%08x\n"),
				(unsigned)crc, *argv);
	} while (*argv && *++argv);

	fflush_stdout_and_exit(exit_code);
}
