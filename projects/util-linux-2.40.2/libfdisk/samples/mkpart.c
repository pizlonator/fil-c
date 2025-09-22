/*
 * Copyright (C) 2017 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * Libfdisk sample to create partitions by specify size, for example:
 *
 *	mkpart --label dos --device /dev/sdc 2M 2M 2M 10M 1M -
 *
  * creates 6 partitions:
  *	- 3 primary  (3x 2M)
  *	- 1 extended (1x 10M)
  *	- 2 logical  (1x 1M, 1x remaining-space-in-extended-partition)
  *
  * Notes:
  *     The sample specifies size and partno for MBR, and size only for another
  *     labels (e.g. GPT).
  *
  *     The Ask-API does not use anything else than warning/info. The
  *     partitionning has to be done non-interactive.
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
	int n = 0, c, nopartno = 0;
	unsigned int sectorsize;
	uint64_t grain = 0;

	static const struct option longopts[] = {
		{ "label",  required_argument, NULL, 'x' },
		{ "device", required_argument, NULL, 'd' },
		{ "nopartno", no_argument, NULL, 'p' },
		{ "grain", required_argument, NULL, 'g' },
		{ "help",   no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};

	setlocale(LC_ALL, "");	/* just to have enable UTF8 chars */

	fdisk_init_debug(0);

	while((c = getopt_long(argc, argv, "g:x:d:h", longopts, NULL)) != -1) {
		switch(c) {
		case 'x':
			label = optarg;
			break;
		case 'd':
			device = optarg;
			break;
		case 'p':
			nopartno = 1;
			break;
		case 'g':
			grain = strtosize_or_err(optarg, "failed to parse grain");
			break;
		case 'h':
			printf("%s [options] <size> ...", program_invocation_short_name);
			fputs(USAGE_SEPARATOR, stdout);
			fputs("Make disklabel and partitions.\n", stdout);
			fputs(USAGE_OPTIONS, stdout);
			fputs(" -x, --label <dos,gpt,...>    disk label type\n", stdout);
			fputs(" -d, --device <path>          block device\n", stdout);
			fputs(" -p, --nopartno               don't set partno (use default)\n", stdout);
			fputs(" -h, --help                   this help\n", stdout);
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

	if (grain)
		fdisk_save_user_grain(cxt, grain);

	pa = fdisk_new_partition();
	if (!pa)
		err_oom();

	if (fdisk_assign_device(cxt, device, 0))
		err(EXIT_FAILURE, "failed to assign device");
	if (fdisk_create_disklabel(cxt, label))
		err(EXIT_FAILURE, "failed to create disk label");

	sectorsize = fdisk_get_sector_size(cxt);

	fdisk_disable_dialogs(cxt, 1);

	while (optind < argc) {
		int rc;
		uint64_t size;
		const char *str = argv[optind];

		/* defaults */
		fdisk_partition_start_follow_default(pa, 1);
		fdisk_partition_end_follow_default(pa, 1);
		fdisk_partition_partno_follow_default(pa, 1);

		/* set size */
		if (isdigit(*str)) {
			size = strtosize_or_err(argv[optind], "failed to parse partition size");
			fdisk_partition_set_size(pa, size / sectorsize);
			fdisk_partition_end_follow_default(pa, 0);

		} else if (*str == '-') {
			fdisk_partition_end_follow_default(pa, 1);
		}

		if (fdisk_is_label(cxt, DOS)) {
			/* For MBR we want to avoid primary/logical dialog.
			 * This is possible by explicitly specified partition
			 * number, <4 means primary, >=4 means logical.
			 */
			if (!nopartno) {
				fdisk_partition_partno_follow_default(pa, 0);
				fdisk_partition_set_partno(pa, n);
			}

			/* Make sure last primary partition is extended if user
			 * wants more than 4 partitions.
			 */
			if (n == 3 && optind + 1 < argc) {
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
			errx(EXIT_FAILURE, "failed to add #%d partition", n + 1);
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
