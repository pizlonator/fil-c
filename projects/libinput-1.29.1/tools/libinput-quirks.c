/*
 * Copyright © 2018 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "util-mem.h"

#include "builddir.h"
#include "libinput-util.h"
#include "quirks.h"
#include "shared.h"

static bool verbose = false;

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
log_handler(struct libinput *this_is_null,
	    enum libinput_log_priority priority,
	    const char *format,
	    va_list args)
{
	FILE *out = stdout;
	enum quirks_log_priorities p = (enum quirks_log_priorities)priority;
	char buf[256] = { 0 };
	const char *prefix = NULL;

	switch (p) {
	case QLOG_NOISE:
	case QLOG_DEBUG:
		if (!verbose)
			return;
		prefix = "quirks debug";
		break;
	case QLOG_INFO:
		prefix = "quirks info";
		break;
	case QLOG_ERROR:
		out = stderr;
		prefix = "quirks error";
		break;
	case QLOG_PARSER_ERROR:
		out = stderr;
		prefix = "quirks parser error";
		break;
	}

	snprintf(buf, sizeof(buf), "%s: %s", prefix, format);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	vfprintf(out, buf, args);
#pragma GCC diagnostic pop
}

static void
usage(void)
{
	printf("Usage:\n"
	       "  libinput quirks list [--data-dir /path/to/quirks/dir] /dev/input/event0\n"
	       "	Print the quirks for the given device\n"
	       "\n"
	       "  libinput quirks validate [--data-dir /path/to/quirks/dir]\n"
	       "	Validate the database\n");
}

static void
simple_printf(void *userdata, const char *val)
{
	printf("%s\n", val);
}

int
main(int argc, char **argv)
{
	const char *data_path = NULL, *override_file = NULL;
	bool validate = false;

	while (1) {
		int c;
		int option_index = 0;
		enum {
			OPT_VERBOSE,
			OPT_DATADIR,
		};
		static struct option opts[] = {
			{ "help", no_argument, 0, 'h' },
			{ "verbose", no_argument, 0, OPT_VERBOSE },
			{ "data-dir", required_argument, 0, OPT_DATADIR },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "h", opts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case '?':
			exit(1);
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case OPT_VERBOSE:
			verbose = true;
			break;
		case OPT_DATADIR:
			data_path = optarg;
			break;
		default:
			usage();
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		usage();
		return EXIT_FAILURE;
	}

	if (streq(argv[optind], "list")) {
		optind++;
		if (optind >= argc) {
			usage();
			return EXIT_FAILURE;
		}
	} else if (streq(argv[optind], "validate")) {
		optind++;
		if (optind < argc) {
			usage();
			return EXIT_FAILURE;
		}
		validate = true;
	} else {
		fprintf(stderr, "Unnkown action '%s'\n", argv[optind]);
		return EXIT_FAILURE;
	}

	/* Overriding the data dir means no custom override file */
	if (!data_path) {
		if (builddir_lookup(NULL)) {
			data_path = LIBINPUT_QUIRKS_SRCDIR;
		} else {
			data_path = LIBINPUT_QUIRKS_DIR;
			override_file = LIBINPUT_QUIRKS_OVERRIDE_FILE;
		}
	}

	_unref_(quirks_context) *quirks =
		quirks_init_subsystem(data_path,
				      override_file,
				      log_handler,
				      NULL,
				      QLOG_CUSTOM_LOG_PRIORITIES);
	if (!quirks) {
		fprintf(stderr,
			"Failed to initialize the device quirks. "
			"Please see the above errors "
			"and/or re-run with --verbose for more details\n");
		return EXIT_FAILURE;
	}

	if (validate)
		return EXIT_SUCCESS;

	_unref_(udev) *udev = udev_new();
	if (!udev)
		return EXIT_FAILURE;

	_unref_(udev_device) *device = NULL;
	const char *path = argv[optind];
	if (strstartswith(path, "/sys/")) {
		device = udev_device_new_from_syspath(udev, path);
	} else {
		struct stat st;
		if (stat(path, &st) < 0) {
			fprintf(stderr, "Error: %s: %m\n", path);
			return EXIT_FAILURE;
		}

		device = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
	}
	if (device) {
		tools_list_device_quirks(quirks, device, simple_printf, NULL);
		return EXIT_SUCCESS;
	} else {
		usage();
		return EXIT_FAILURE;
	}
}
