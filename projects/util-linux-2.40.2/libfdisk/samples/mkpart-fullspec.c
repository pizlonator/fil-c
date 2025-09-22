/*
 * Copyright (C) 2017 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * Libfdisk sample to create partitions by specify all required partition
 * properties (partno, start and size). The default is only partition type
 * (except MBR where 4th partition is extended).
 */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "strutils.h"
#include "xalloc.h"

#include "libfdisk.h"

static int ask_callback(struct fdisk_context *cxt __attribute__((__unused__)),
			struct fdisk_ask *ask,
			void *data __attribute__((unused)))
{
	switch(fdisk_ask_get_type(ask)) {
	case FDISK_ASKTYPE_INFO:
		fputs(fdisk_ask_print_get_mesg(ask), stdout);
		fputc('\n', stdout);
		break;
	case FDISK_ASKTYPE_WARNX:
		fflush(stdout);
		fputs(fdisk_ask_print_get_mesg(ask), stderr);
		fputc('\n', stderr);
		break;
	case FDISK_ASKTYPE_WARN:
		fflush(stdout);
		fputs(fdisk_ask_print_get_mesg(ask), stderr);
		errno = fdisk_ask_print_get_errno(ask);
		fprintf(stderr, ": %m\n");
		break;
	default:
		break;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	struct fdisk_context *cxt;
	struct fdisk_partition *pa;
	const char *label = NULL, *device = NULL;
	int c;
	size_t n = 1;

	static const struct option longopts[] = {
		{ "label",  required_argument, NULL, 'x' },
		{ "device", required_argument, NULL, 'd' },
		{ "help",   no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	fdisk_init_debug(0);

	while((c = getopt_long(argc, argv, "x:d:h", longopts, NULL)) != -1) {
		switch(c) {
		case 'x':
			label = optarg;
			break;
		case 'd':
			device = optarg;
			break;
		case 'h':
			printf("%s [options] -- <partno,start,size> ...", program_invocation_short_name);
			fputs(USAGE_SEPARATOR, stdout);
			puts("Make disklabel and partitions.");
			puts(" <partno>                     1..n (4th is extended for MBR), or '-' for default");
			puts(" <start>                      partition start offset in sectors");
			puts(" <size>                       partition size in sectors");
			fputs(USAGE_OPTIONS, stdout);
			puts(" -x, --label <dos,gpt,...>    disk label type (default MBR)");
			puts(" -d, --device <path>          block device");
			puts(" -h, --help                   this help");
			fputs(USAGE_SEPARATOR, stdout);
			return EXIT_SUCCESS;
		}
	}

	if (!device)
		errx(EXIT_FAILURE, "no device specified");
	if (!label)
		label = "dos";

	cxt = fdisk_new_context();
	if (!cxt)
		err_oom();
	fdisk_set_ask(cxt, ask_callback, NULL);

	pa = fdisk_new_partition();
	if (!pa)
		err_oom();

	if (fdisk_assign_device(cxt, device, 0))
		err(EXIT_FAILURE, "failed to assign device");
	if (fdisk_create_disklabel(cxt, label))
		err(EXIT_FAILURE, "failed to create disk label");

	fdisk_disable_dialogs(cxt, 1);

	while (optind < argc) {
		int rc;
		unsigned int partno = 0;
		uint64_t start = 0, size = 0;
		const char *str = argv[optind];

		fdisk_reset_partition(pa);
		fdisk_partition_end_follow_default(pa, 0);

		if (*str == '-') {
			/* partno unspecified */
			if (sscanf(str, "-,%"SCNu64",%"SCNu64"", &start, &size) != 2)
				errx(EXIT_FAILURE, "failed to parse %s", str);
			fdisk_partition_partno_follow_default(pa, 1);
			fdisk_partition_unset_partno(pa);
		} else {
			/* partno specified */
			if (sscanf(str, "%u,%"SCNu64",%"SCNu64"", &partno, &start, &size) != 3)
				errx(EXIT_FAILURE, "failed to parse %s", str);

			fdisk_partition_partno_follow_default(pa, 0);
			fdisk_partition_set_partno(pa, partno - 1);     /* library uses 0..n */
		}

		fdisk_partition_set_start(pa, start);
		fdisk_partition_set_size(pa, size);

		if (fdisk_partition_has_partno(pa))
			fprintf(stdout, "Requested partition: <partno=%zu,start=%ju,size=%ju>\n",
					fdisk_partition_get_partno(pa),
					(uintmax_t) fdisk_partition_get_start(pa),
					(uintmax_t) fdisk_partition_get_size(pa));
		else
			fprintf(stdout, "Requested partition: <partno=<default>,start=%ju,size=%ju>\n",
					(uintmax_t) fdisk_partition_get_start(pa),
					(uintmax_t) fdisk_partition_get_size(pa));

		if (fdisk_is_label(cxt, DOS)) {
			/* Make sure last primary partition is extended if user
			 * wants more than 4 partitions.
			 */
			if ((partno == 4 || (n == 4 && !fdisk_partition_has_partno(pa)))
			    && optind + 1 < argc) {
				struct fdisk_parttype *type =
					fdisk_label_parse_parttype(
							fdisk_get_label(cxt, NULL), "05");
				if (!type)
					err_oom();
				fdisk_partition_set_type(pa, type);
				fdisk_unref_parttype(type);
			}
		}

		rc = fdisk_add_partition(cxt, pa, NULL);
		if (rc) {
			errno = -rc;
			errx(EXIT_FAILURE, "failed to add #%zu partition",
					fdisk_partition_has_partno(pa) ?
					fdisk_partition_get_partno(pa) + 1: n);
		}

		fdisk_reset_partition(pa);
		optind++;
		n++;
	}

	if (fdisk_write_disklabel(cxt))
		err(EXIT_FAILURE, "failed to write disk label");

	fdisk_deassign_device(cxt, 1);
	fdisk_unref_context(cxt);
	fdisk_unref_partition(pa);

	return EXIT_SUCCESS;
}
