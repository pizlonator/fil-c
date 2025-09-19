/*
 * Copyright © 2014 Red Hat, Inc.
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
#include <inttypes.h>
#include <libevdev/libevdev.h>
#include <libinput.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util-libinput.h"
#include "util-macros.h"
#include "util-strings.h"

#include "libinput-version.h"
#include "linux/input.h"
#include "shared.h"

static struct tools_options options;
static bool show_keycodes;
static volatile sig_atomic_t stop = 0;
static bool be_quiet = false;
static bool compress_motion_events = false;
static bool is_tty = false;

#define printq(...) ({ if (!be_quiet)  printf(__VA_ARGS__); })

static int
handle_and_print_events(struct libinput *li, const struct libinput_print_options *opts)
{
	int rc = -1;
	struct libinput_event *ev;
	static struct libinput_device *last_device = NULL;
	static enum libinput_event_type last_event_type = LIBINPUT_EVENT_NONE;
	static size_t event_repeat_count = 0;
	static uint32_t last_log_serial = 0;

	tools_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		struct libinput_device *device = libinput_event_get_device(ev);
		enum libinput_event_type type = libinput_event_get_type(ev);

		if (type == LIBINPUT_EVENT_POINTER_AXIS) {
			libinput_event_destroy(ev);
			continue;
		}

		bool is_repeat = false;

		switch (type) {
		case LIBINPUT_EVENT_POINTER_MOTION:
		case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
		case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		case LIBINPUT_EVENT_TOUCH_MOTION:
		case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
		case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
			is_repeat = last_event_type == type && last_device == device &&
				    log_serial == last_log_serial;
			break;
		default:
			break;
		}

		if (is_repeat) {
			event_repeat_count++;
			if (compress_motion_events)
				printq("\e[1A");
		} else {
			event_repeat_count = 0;
		}

		if (type != LIBINPUT_EVENT_TOUCH_FRAME || !compress_motion_events) {
			_autofree_ char *event_str =
				libinput_event_to_str(ev, event_repeat_count + 1, opts);

			switch (type) {
			case LIBINPUT_EVENT_DEVICE_ADDED:
				tools_device_apply_config(libinput_event_get_device(ev),
							  &options);
				break;
			case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY: {
				struct libinput_event_tablet_tool *tev =
					libinput_event_get_tablet_tool_event(ev);
				struct libinput_tablet_tool *tool =
					libinput_event_tablet_tool_get_tool(tev);
				tools_tablet_tool_apply_config(tool, &options);
				break;
			}
			default:
				break;
			}

			printq("%s\n", event_str);
		}

		last_device = device;
		if (type != LIBINPUT_EVENT_TOUCH_FRAME)
			last_event_type = type;
		last_log_serial = log_serial;

		libinput_event_destroy(ev);
		rc = 0;
	}

	fflush(stdout);

	return rc;
}

static void
sighandler(int signal, siginfo_t *siginfo, void *userdata)
{
	stop = 1;
}

static void
mainloop(struct libinput *li)
{
	struct pollfd fds;

	fds.fd = libinput_get_fd(li);
	fds.events = POLLIN;
	fds.revents = 0;

	struct libinput_print_options opts = {
		.screen_width = 100,
		.screen_height = 100,
		.show_keycodes = show_keycodes,
		.start_time = 0,
	};

	/* Handle already-pending device added events */
	if (handle_and_print_events(li, &opts))
		fprintf(stderr,
			"Expected device added events on startup but got none. "
			"Maybe you don't have the right permissions?\n");

	/* time offset starts with our first received event */
	if (poll(&fds, 1, -1) > -1) {
		struct timespec tp;

		clock_gettime(CLOCK_MONOTONIC, &tp);
		opts.start_time = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;
		do {
			handle_and_print_events(li, &opts);
		} while (!stop && poll(&fds, 1, -1) > -1);
	}

	printf("\n");
}

static void
usage(struct option *opts)
{
	printf("Usage: libinput debug-events [options] [--udev <seat>|--device /dev/input/event0 ...]\n");

	if (opts)
		tools_print_usage_option_list(opts);
}

int
main(int argc, char **argv)
{
	struct libinput *li;
	enum tools_backend backend = BACKEND_NONE;
	const char *seat_or_devices[60] = { NULL };
	size_t ndevices = 0;
	bool grab = false;
	bool verbose = false;
	struct sigaction act;

	tools_init_options(&options);

	is_tty = isatty(STDOUT_FILENO);

	while (1) {
		int c;
		int option_index = 0;
		enum {
			OPT_DEVICE = 1,
			OPT_UDEV,
			OPT_GRAB,
			OPT_VERBOSE,
			OPT_SHOW_KEYCODES,
			OPT_QUIET,
			OPT_COMPRESS_MOTION_EVENTS,
		};
		/* clang-format off */
		static struct option opts[] = {
			CONFIGURATION_OPTIONS,
			{ "help",                      no_argument,       0, 'h' },
			{ "show-keycodes",             no_argument,       0, OPT_SHOW_KEYCODES },
			{ "device",                    required_argument, 0, OPT_DEVICE },
			{ "udev",                      required_argument, 0, OPT_UDEV },
			{ "grab",                      no_argument,       0, OPT_GRAB },
			{ "verbose",                   no_argument,       0, OPT_VERBOSE },
			{ "quiet",                     no_argument,       0, OPT_QUIET },
			{ "compress-motion-events",    no_argument,       0, OPT_COMPRESS_MOTION_EVENTS },
			{ 0, 0, 0, 0},
		};
		/* clang-format on */

		c = getopt_long(argc, argv, "h", opts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case '?':
			exit(EXIT_INVALID_USAGE);
			break;
		case 'h':
			usage(opts);
			exit(EXIT_SUCCESS);
			break;
		case OPT_SHOW_KEYCODES:
			show_keycodes = true;
			break;
		case OPT_QUIET:
			be_quiet = true;
			break;
		case OPT_DEVICE:
			if (backend == BACKEND_UDEV ||
			    ndevices >= ARRAY_LENGTH(seat_or_devices)) {
				usage(NULL);
				return EXIT_INVALID_USAGE;
			}
			backend = BACKEND_DEVICE;
			seat_or_devices[ndevices++] = optarg;
			break;
		case OPT_UDEV:
			if (backend == BACKEND_DEVICE ||
			    ndevices >= ARRAY_LENGTH(seat_or_devices)) {
				usage(NULL);
				return EXIT_INVALID_USAGE;
			}
			backend = BACKEND_UDEV;
			seat_or_devices[0] = optarg;
			ndevices = 1;
			break;
		case OPT_GRAB:
			grab = true;
			break;
		case OPT_VERBOSE:
			verbose = true;
			break;
		case OPT_COMPRESS_MOTION_EVENTS:
			/* We compress by using ansi escape sequences */
			compress_motion_events = is_tty;
			break;
		default:
			if (tools_parse_option(c, optarg, &options) != 0) {
				usage(NULL);
				return EXIT_INVALID_USAGE;
			}
			break;
		}
	}

	if (optind < argc) {
		if (backend == BACKEND_UDEV) {
			usage(NULL);
			return EXIT_INVALID_USAGE;
		}
		backend = BACKEND_DEVICE;
		do {
			if (ndevices >= ARRAY_LENGTH(seat_or_devices)) {
				usage(NULL);
				return EXIT_INVALID_USAGE;
			}
			seat_or_devices[ndevices++] = argv[optind];
		} while (++optind < argc);
	} else if (backend == BACKEND_NONE) {
		backend = BACKEND_UDEV;
		seat_or_devices[0] = "seat0";
	}

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sighandler;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGINT, &act, NULL) == -1) {
		fprintf(stderr,
			"Failed to set up signal handling (%s)\n",
			strerror(errno));
		return EXIT_FAILURE;
	}

	if (verbose)
		printf("libinput version: %s\n", LIBINPUT_VERSION);

	li = tools_open_backend(backend, seat_or_devices, verbose, &grab);
	if (!li)
		return EXIT_FAILURE;

	mainloop(li);

	libinput_unref(li);

	return EXIT_SUCCESS;
}
