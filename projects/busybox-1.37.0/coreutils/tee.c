/* vi: set sw=4 ts=4: */
/*
 * tee implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config TEE
//config:	bool "tee (4.4 kb)"
//config:	default y
//config:	help
//config:	tee is used to read from standard input and write
//config:	to standard output and files.
//config:
//config:config FEATURE_TEE_USE_BLOCK_IO
//config:	bool "Enable block I/O (larger/faster) instead of byte I/O"
//config:	default y
//config:	depends on TEE
//config:	help
//config:	Enable this option for a faster tee, at expense of size.

//applet:IF_TEE(APPLET(tee, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TEE) += tee.o

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tee.html */

//usage:#define tee_trivial_usage
//usage:       "[-ai] [FILE]..."
//usage:#define tee_full_usage "\n\n"
//usage:       "Copy stdin to each FILE, and also to stdout\n"
//usage:     "\n	-a	Append to the given FILEs, don't overwrite"
//usage:     "\n	-i	Ignore interrupt signals (SIGINT)"
//usage:
//usage:#define tee_example_usage
//usage:       "$ echo \"Hello\" | tee /tmp/foo\n"
//usage:       "$ cat /tmp/foo\n"
//usage:       "Hello\n"

// Bare "tee" with no below options does not install SIGPIPE handler - just dies on it.
// TODO:
//	--output-error[=MODE]
//		'warn'		diagnose errors writing to any output
//		'warn-nopipe'	diagnose errors writing to any output not a pipe
//		'exit'		exit on error writing to any output
//		'exit-nopipe'	exit on error writing to any output not a pipe
// ^^^ all of these should set SIGPIPE to SIG_IGN.
// Because "exit" mode should print error message and exit1(1) - not die on SIGPIPE.
// "exit-nopipe" does not exit on EPIPE and does not set exitcode to 1 too.
//	-p	diagnose errors writing to non pipes
// ^^^^ this should set SIGPIPE to SIG_IGN. EPIPE is ignored (same as "warn-nopipe")

#include "libbb.h"
#include "common_bufsiz.h"

int tee_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int tee_main(int argc, char **argv)
{
	const char *mode = "w\0a";
	FILE **files;
	FILE **fp;
	char **names;
	char **np;
	char retval;
//TODO: make unconditional
#if ENABLE_FEATURE_TEE_USE_BLOCK_IO
	ssize_t c;
# define buf bb_common_bufsiz1
	setup_common_bufsiz();
#else
	int c;
#endif
	retval = getopt32(argv, "ia");	/* 'a' must be 2nd */
	argc -= optind;
	argv += optind;

	mode += (retval & 2);	/* Since 'a' is the 2nd option... */

	if (retval & 1) {
		signal(SIGINT, SIG_IGN);
	}
	retval = EXIT_SUCCESS;
	/* if (opt_p || opt_output_error)
		signal(SIGPIPE, SIG_IGN);
	 */

	/* Allocate an array of FILE *'s, with one extra for a sentinel. */
	fp = files = xzalloc(sizeof(FILE *) * (argc + 2));
	np = names = argv - 1;

	files[0] = stdout;
	goto GOT_NEW_FILE;

	do {
		*fp = stdout;
		if (NOT_LONE_DASH(*argv)) {
			*fp = fopen_or_warn(*argv, mode);
			if (*fp == NULL) {
				retval = EXIT_FAILURE;
				argv++;
				continue;
			}
		}
		*np = *argv++;
 GOT_NEW_FILE:
		setbuf(*fp, NULL);	/* tee must not buffer output. */
		fp++;
		np++;
	} while (*argv);
	/* names[0] will be filled later */

#if ENABLE_FEATURE_TEE_USE_BLOCK_IO
	while ((c = safe_read(STDIN_FILENO, buf, COMMON_BUFSIZE)) > 0) {
		fp = files;
		do
			fwrite(buf, 1, c, *fp);
			/* if (opt_p && fwrite() != c && !EPIPE) bb_error_msg("..."); */
		while (*++fp);
	}
	if (c < 0) {		/* Make sure read errors are signaled. */
		retval = EXIT_FAILURE;
	}
#else
	setvbuf(stdout, NULL, _IONBF, 0);
	while ((c = getchar()) != EOF) {
		fp = files;
		do
			putc(c, *fp);
			/* if (opt_p && putc() == EOF && !EPIPE) bb_error_msg("..."); */
		while (*++fp);
	}
#endif

	/* Now we need to check for i/o errors on stdin and the various
	 * output files.  Since we know that the first entry in the output
	 * file table is stdout, we can save one "if ferror" test by
	 * setting the first entry to stdin and checking stdout error
	 * status with fflush_stdout_and_exit()... although fflush()ing
	 * is unnecessary here. */
	np = names;
	fp = files;
	names[0] = (char *) bb_msg_standard_input;
	files[0] = stdin;
	do {	/* Now check for input and output errors. */
		/* Checking ferror should be sufficient, but we may want to fclose.
		 * If we do, remember not to close stdin! */
		die_if_ferror(*fp++, *np++);
	} while (*fp);

	fflush_stdout_and_exit(retval);
}
