/*
 * No copyright is claimed.  This code is in the public domain; do with
 * it what you wish.
 *
 * mcookie.c -- Generates random numbers for xauth
 * Created: Fri Feb  3 10:42:48 1995 by faith@cs.unc.edu
 * Revised: Fri Mar 19 07:48:01 1999 by faith@acm.org
 * Public Domain 1995, 1999 Rickard E. Faith (faith@acm.org)
 * This program comes with ABSOLUTELY NO WARRANTY.
 *
 * This program gathers some random bits of data and used the MD5
 * message-digest algorithm to generate a 128-bit hexadecimal number for
 * use with xauth(1).
 *
 * NOTE: Unless /dev/random is available, this program does not actually
 * gather 128 bits of random information, so the magic cookie generated
 * will be considerably easier to guess than one might expect.
 *
 * 1999-02-22 Arkadiusz Miśkiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-03-21 aeb: Added some fragments of code from Colin Plumb.
 *
 */

#include "c.h"
#include "md5.h"
#include "nls.h"
#include "closestream.h"
#include "randutils.h"
#include "strutils.h"
#include "xalloc.h"
#include "all-io.h"

#include <fcntl.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

enum {
	BUFFERSIZE = 4096,
	RAND_BYTES = 128
};

struct mcookie_control {
	struct	UL_MD5Context ctx;
	char	**files;
	size_t	nfiles;
	uint64_t maxsz;

	unsigned int verbose:1;
};

/* The basic function to hash a file */
static uint64_t hash_file(struct mcookie_control *ctl, int fd)
{
	unsigned char buf[BUFFERSIZE];
	uint64_t wanted, count;

	wanted = ctl->maxsz ? ctl->maxsz : sizeof(buf);

	for (count = 0; count < wanted; ) {
		size_t rdsz = sizeof(buf);
		ssize_t r;

		if (wanted - count < rdsz)
			rdsz = wanted - count;

		r = read_all(fd, (char *) buf, rdsz);
		if (r <= 0)
			break;
		ul_MD5Update(&ctl->ctx, buf, r);
		count += r;
	}
	/* Separate files with a null byte */
	buf[0] = '\0';
	ul_MD5Update(&ctl->ctx, buf, 1);
	return count;
}

static void __attribute__((__noreturn__)) usage(void)
{
	FILE *out = stdout;
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);

	fputs(USAGE_SEPARATOR, out);
	fputs(_("Generate magic cookies for xauth.\n"), out);

	fputs(USAGE_OPTIONS, out);
	fputs(_(" -f, --file <file>     use file as a cookie seed\n"), out);
	fputs(_(" -m, --max-size <num>  limit how much is read from seed files\n"), out);
	fputs(_(" -v, --verbose         explain what is being done\n"), out);

	fputs(USAGE_SEPARATOR, out);
	fprintf(out, USAGE_HELP_OPTIONS(23));

	fputs(USAGE_ARGUMENTS, out);
	fprintf(out, USAGE_ARG_SIZE(_("<num>")));

	fprintf(out, USAGE_MAN_TAIL("mcookie(1)"));

	exit(EXIT_SUCCESS);
}

static void randomness_from_files(struct mcookie_control *ctl)
{
	size_t i;

	for (i = 0; i < ctl->nfiles; i++) {
		const char *fname = ctl->files[i];
		size_t count;
		int fd;

		if (*fname == '-' && !*(fname + 1))
			fd = STDIN_FILENO;
		else
			fd = open(fname, O_RDONLY);

		if (fd < 0) {
			warn(_("cannot open %s"), fname);
		} else {
			count = hash_file(ctl, fd);
			if (ctl->verbose)
				fprintf(stderr,
					P_("Got %zu byte from %s\n",
					   "Got %zu bytes from %s\n", count),
					count, fname);

			if (fd != STDIN_FILENO && close(fd))
				err(EXIT_FAILURE, _("closing %s failed"), fname);
		}
	}
}

int main(int argc, char **argv)
{
	struct mcookie_control ctl = { .verbose = 0 };
	size_t i;
	unsigned char digest[UL_MD5LENGTH];
	unsigned char buf[RAND_BYTES];
	int c;

	static const struct option longopts[] = {
		{"file", required_argument, NULL, 'f'},
		{"max-size", required_argument, NULL, 'm'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	close_stdout_atexit();

	while ((c = getopt_long(argc, argv, "f:m:vVh", longopts, NULL)) != -1) {
		switch (c) {
		case 'v':
			ctl.verbose = 1;
			break;
		case 'f':
			if (!ctl.files)
				ctl.files = xmalloc(sizeof(char *) * argc);
			ctl.files[ctl.nfiles++] = optarg;
			break;
		case 'm':
			ctl.maxsz = strtosize_or_err(optarg,
						     _("failed to parse length"));
			break;

		case 'V':
			print_version(EXIT_SUCCESS);
		case 'h':
			usage();
		default:
			errtryhelp(EXIT_FAILURE);
		}
	}

	if (ctl.maxsz && ctl.nfiles == 0)
		warnx(_("--max-size ignored when used without --file"));

	ul_MD5Init(&ctl.ctx);
	randomness_from_files(&ctl);
	free(ctl.files);

	ul_random_get_bytes(&buf, RAND_BYTES);
	ul_MD5Update(&ctl.ctx, buf, RAND_BYTES);
	if (ctl.verbose)
		fprintf(stderr, P_("Got %d byte from %s\n",
				   "Got %d bytes from %s\n", RAND_BYTES),
				RAND_BYTES, random_tell_source());

	ul_MD5Final(digest, &ctl.ctx);
	for (i = 0; i < UL_MD5LENGTH; i++)
		printf("%02x", digest[i]);
	putchar('\n');

	return EXIT_SUCCESS;
}
