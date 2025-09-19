/*
 * Copyright © 2025 Red Hat, Inc.
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <libevdev/libevdev.h>
#include <libinput.h>
#include <libudev.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "util-files.h"
#include "util-input-event.h"
#include "util-macros.h"
#include "util-mem.h"

#include "shared.h"

DEFINE_UNREF_CLEANUP_FUNC(udev_device);

static volatile sig_atomic_t stop = 0;
static struct tools_options options;
static int termwidth = 78;

struct context {
	struct libinput *libinput;
	struct libinput_device *device;
	struct libevdev *evdev;

	/* fd[0] ... libinput fd
	   fd[1] ... libevdev fd */
	struct pollfd fds[2];

	/* libinput device state */
	double ring[2];
	double strip[2];
	double dial[2];
	unsigned int buttons_down[32];
	unsigned int evdev_buttons_down[BTN_START - BTN_0 + 1];
	/* keys[i] = keycode if a keycode is down, 8 keys simultaneously is enough */
	uint32_t keys[8];

	unsigned int nbuttons;

	/* libevdev device state */
	struct {
		int wheel;
		int throttle;
		int rx;
		int ry;
	} abs;

	struct {
		int wheel[2];
		int wheel_v120[2];
	} rel;
};

LIBINPUT_ATTRIBUTE_PRINTF(2, 3)
static void
print_line(const char *label, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	_autofree_ char *msg = strdup_vprintf(format, args);
	va_end(args);

	_autofree_ char *prefix = strdup_printf("%s:", label);
	printf(ANSI_CLEAR_LINE "  %-19s %s\n", prefix, msg);
}

static void
print_buttons(struct context *ctx, unsigned int *buttons, size_t nbuttons)
{
	_autostrvfree_ char **strv = NULL;
	for (size_t i = 0; i < nbuttons; i++) {
		strv = strv_append_printf(strv, "%2zd: %c", i, buttons[i] ? 'X' : ' ');
	}
	_autofree_ char *btnstr = strv_join(strv, " ");
	print_line("buttons", "%s", btnstr ? btnstr : "");
}

static void
print_dial(const char *prefix, double value)
{
	print_line(prefix, "% 8.2f", value);
}

static void
print_buttons_evdev(struct context *ctx, unsigned int *buttons, size_t nbuttons)
{
	_autostrvfree_ char **strv = NULL;
	for (size_t i = 0; i < nbuttons; i++) {
		if (!buttons[i])
			continue;

		unsigned int button = BTN_0 + i;
		strv = strv_append_printf(strv,
					  "%s",
					  libevdev_event_code_get_name(EV_KEY, button));
	}

	_autofree_ char *btnstr = strv_join(strv, ", ");
	print_line("buttons", "%s", btnstr ? btnstr : "");
}

static void
print_rel_wheel(struct context *ctx, unsigned int code, int value)
{
	print_line(libevdev_event_code_get_name(EV_REL, code), "% 5d", value);
}

static void
print_bar(const char *header, double value, double normalized)
{
	char empty[termwidth];
	bool oob = false;
	/* the bar is minimum 10 chars, otherwise 78 or whatever fits.
	   32 is the manually-added up length of the prefix + [|] */
	const int width = clamp(termwidth - 32, 10, 78);
	int left_pad, right_pad;

	memset(empty, '-', sizeof empty);

	if (normalized < 0.0 || normalized > 1.0) {
		normalized = clamp(normalized, 0.0, 1.0);
		oob = true;
	}

	left_pad = width * normalized + 0.5;
	right_pad = width - left_pad;

	print_line(header,
		   "%s%8.2f [%.*s|%.*s]%s",
		   oob ? ANSI_RED : "",
		   value,
		   left_pad,
		   empty,
		   right_pad,
		   empty,
		   oob ? ANSI_NORMAL : "");
}

static double
normalize(struct libevdev *evdev, int code, int value)
{
	const struct input_absinfo *abs;

	if (!evdev)
		return 0.0;

	abs = libevdev_get_abs_info(evdev, code);

	if (!abs)
		return 0.0;

	return 1.0 * (value - abs->minimum) / absinfo_range(abs);
}

static int
print_state(struct context *ctx)
{
	double w, h;
	int lines_printed = 0;

	if (!ctx->device) {
		printf(ANSI_RED "No device connected" ANSI_NORMAL ANSI_CLEAR_EOL "\n");
	} else {
		libinput_device_get_size(ctx->device, &w, &h);
		printf("Device: %s (%s)%s\n",
		       libinput_device_get_name(ctx->device),
		       libinput_device_get_sysname(ctx->device),
		       ANSI_CLEAR_EOL);
	}
	lines_printed++;

	printf("libinput:\n");
	print_bar("ring 0", ctx->ring[0], ctx->ring[0] / 360.0);
	print_bar("ring 1", ctx->ring[1], ctx->ring[1] / 360.0);
	print_bar("strip 0", ctx->strip[0], ctx->strip[0]);
	print_bar("strip 1", ctx->strip[1], ctx->strip[1]);
	print_dial("dial 0", ctx->dial[0]);
	print_dial("dial 1", ctx->dial[1]);
	print_buttons(ctx,
		      ctx->buttons_down,
		      min(ARRAY_LENGTH(ctx->buttons_down), ctx->nbuttons));
	lines_printed += 8;

	printf("evdev:\n");
	print_bar("ABS_WHEEL",
		  ctx->abs.wheel,
		  normalize(ctx->evdev, ABS_WHEEL, ctx->abs.wheel));
	print_bar("ABS_THROTTLE",
		  ctx->abs.throttle,
		  normalize(ctx->evdev, ABS_THROTTLE, ctx->abs.throttle));
	print_bar("ABS_RX", ctx->abs.rx, normalize(ctx->evdev, ABS_RX, ctx->abs.rx));
	print_bar("ABS_RY", ctx->abs.ry, normalize(ctx->evdev, ABS_RY, ctx->abs.ry));
	print_rel_wheel(ctx, REL_WHEEL, ctx->rel.wheel[0]);
	print_rel_wheel(ctx, REL_WHEEL_HI_RES, ctx->rel.wheel_v120[0]);
	print_rel_wheel(ctx, REL_HWHEEL, ctx->rel.wheel[1]);
	print_rel_wheel(ctx, REL_HWHEEL_HI_RES, ctx->rel.wheel_v120[1]);
	print_buttons_evdev(ctx,
			    ctx->evdev_buttons_down,
			    ARRAY_LENGTH(ctx->evdev_buttons_down));
	lines_printed += 10;

	return lines_printed;
}

static void
handle_device_added(struct context *ctx, struct libinput_event *ev)
{
	struct libinput_device *device = libinput_event_get_device(ev);
	_unref_(udev_device) *udev_device = NULL;
	const char *devnode;

	if (ctx->device)
		return;

	if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_PAD))
		return;

	ctx->device = libinput_device_ref(device);
	ctx->nbuttons = libinput_device_tablet_pad_get_num_buttons(device);

	udev_device = libinput_device_get_udev_device(device);
	if (!udev_device)
		return;

	devnode = udev_device_get_devnode(udev_device);
	if (devnode) {
		int fd = open(devnode, O_RDONLY | O_NONBLOCK);
		assert(fd != -1);
		assert(libevdev_new_from_fd(fd, &ctx->evdev) == 0);
	}
}

static void
handle_device_removed(struct context *ctx, struct libinput_event *ev)
{
	struct libinput_device *device = libinput_event_get_device(ev);

	if (ctx->device != device)
		return;

	libinput_device_unref(steal(&ctx->device));
	libevdev_free(steal(&ctx->evdev));
	xclose(&ctx->fds[1].fd);
}

static void
handle_libinput_events(struct context *ctx)
{
	struct libinput *li = ctx->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	uint32_t number;
	double value;
	enum libinput_button_state state;

	libinput_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_NONE:
			abort();
		case LIBINPUT_EVENT_DEVICE_ADDED:
			handle_device_added(ctx, ev);
			tools_device_apply_config(libinput_event_get_device(ev),
						  &options);
			break;
		case LIBINPUT_EVENT_DEVICE_REMOVED:
			handle_device_removed(ctx, ev);
			break;
		case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
			pev = libinput_event_get_tablet_pad_event(ev);
			number = libinput_event_tablet_pad_get_button_number(pev);
			state = libinput_event_tablet_pad_get_button_state(pev);
			ctx->buttons_down[number] =
				state == LIBINPUT_BUTTON_STATE_PRESSED ? 1 : 0;
			break;
		case LIBINPUT_EVENT_TABLET_PAD_RING:
			pev = libinput_event_get_tablet_pad_event(ev);
			number = libinput_event_tablet_pad_get_ring_number(pev);
			value = libinput_event_tablet_pad_get_ring_position(pev);
			ctx->ring[number] = value;
			break;
		case LIBINPUT_EVENT_TABLET_PAD_STRIP:
			pev = libinput_event_get_tablet_pad_event(ev);
			number = libinput_event_tablet_pad_get_strip_number(pev);
			value = libinput_event_tablet_pad_get_strip_position(pev);
			ctx->strip[number] = value;
			break;
		case LIBINPUT_EVENT_TABLET_PAD_DIAL: {
			pev = libinput_event_get_tablet_pad_event(ev);
			number = libinput_event_tablet_pad_get_dial_number(pev);
			value = libinput_event_tablet_pad_get_dial_delta_v120(pev);
			ctx->dial[number] = value;
			break;
		}
		case LIBINPUT_EVENT_TABLET_PAD_KEY: {
			pev = libinput_event_get_tablet_pad_event(ev);
			uint32_t key = libinput_event_tablet_pad_get_key(pev);
			if (libinput_event_tablet_pad_get_key_state(pev) ==
			    LIBINPUT_KEY_STATE_PRESSED) {
				ARRAY_FOR_EACH(ctx->keys, k) {
					if (*k == 0) {
						*k = key;
					}
				}
			} else {
				ARRAY_FOR_EACH(ctx->keys, k) {
					if (*k == key) {
						*k = 0;
					}
				}
			}
			break;
		}
		default:
			break;
		}

		libinput_event_destroy(ev);
	}
}

static void
handle_libevdev_events(struct context *ctx)
{
	struct libevdev *evdev = ctx->evdev;
	struct input_event event;

#define evbit(_t, _c) (((_t) << 16) | (_c))

	if (!evdev)
		return;

	while (libevdev_next_event(evdev, LIBEVDEV_READ_FLAG_NORMAL, &event) ==
	       LIBEVDEV_READ_STATUS_SUCCESS) {
		switch (evbit(event.type, event.code)) {
		case evbit(EV_KEY, BTN_0)... evbit(EV_KEY, BTN_START):
			ctx->evdev_buttons_down[event.code - BTN_0] =
				event.value ? event.code : 0;
			break;
		case evbit(EV_REL, REL_WHEEL):
			ctx->rel.wheel[0] = event.value;
			break;
		case evbit(EV_REL, REL_HWHEEL):
			ctx->rel.wheel[1] = event.value;
			break;
		case evbit(EV_REL, REL_WHEEL_HI_RES):
			ctx->rel.wheel_v120[0] = event.value;
			break;
		case evbit(EV_REL, REL_HWHEEL_HI_RES):
			ctx->rel.wheel_v120[1] = event.value;
			break;
		case evbit(EV_ABS, ABS_WHEEL):
			ctx->abs.wheel = event.value;
			break;
		case evbit(EV_ABS, ABS_THROTTLE):
			ctx->abs.throttle = event.value;
			break;
		case evbit(EV_ABS, ABS_RX):
			ctx->abs.rx = event.value;
			break;
		case evbit(EV_ABS, ABS_RY):
			ctx->abs.ry = event.value;
			break;
		}
	}
}

static void
sighandler(int signal, siginfo_t *siginfo, void *userdata)
{
	stop = 1;
}

static void
mainloop(struct context *ctx)
{
	unsigned int lines_printed = 20;

	ctx->fds[0].fd = libinput_get_fd(ctx->libinput);

	/* pre-load the lines */
	for (unsigned int i = 0; i < lines_printed; i++)
		printf("\n");

	do {
		handle_libinput_events(ctx);
		handle_libevdev_events(ctx);

		printf(ANSI_LEFT, 1000);
		printf(ANSI_UP, lines_printed);
		lines_printed = print_state(ctx);
	} while (!stop && poll(ctx->fds, 2, -1) > -1);

	printf("\n");
}

static void
usage(void)
{
	printf("Usage: libinput debug-tablet [options] [--udev <seat>|--device /dev/input/event0]\n");
}

static void
init_context(struct context *ctx)
{

	memset(ctx, 0, sizeof *ctx);

	ctx->fds[0].fd = -1; /* libinput fd */
	ctx->fds[0].events = POLLIN;
	ctx->fds[0].revents = 0;
	ctx->fds[1].fd = -1; /* libevdev fd */
	ctx->fds[1].events = POLLIN;
	ctx->fds[1].revents = 0;
}

int
main(int argc, char **argv)
{
	struct context ctx;
	struct libinput *li;
	enum tools_backend backend = BACKEND_NONE;
	const char *seat_or_device[2] = { "seat0", NULL };
	struct sigaction act;
	bool grab = false;

	init_context(&ctx);

	tools_init_options(&options);

	while (1) {
		int c;
		int option_index = 0;
		enum {
			OPT_DEVICE = 1,
			OPT_UDEV,
		};
		static struct option opts[] = {
			CONFIGURATION_OPTIONS,
			{ "help", no_argument, 0, 'h' },
			{ "device", required_argument, 0, OPT_DEVICE },
			{ "udev", required_argument, 0, OPT_UDEV },
			{ 0, 0, 0, 0 }
		};

		c = getopt_long(argc, argv, "h", opts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case '?':
			exit(EXIT_INVALID_USAGE);
			break;
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case OPT_DEVICE:
			backend = BACKEND_DEVICE;
			seat_or_device[0] = optarg;
			break;
		case OPT_UDEV:
			backend = BACKEND_UDEV;
			seat_or_device[0] = optarg;
			break;
		default:
			if (tools_parse_option(c, optarg, &options) != 0) {
				usage();
				return EXIT_INVALID_USAGE;
			}
			break;
		}
	}

	if (optind < argc) {
		if (optind < argc - 1 || backend != BACKEND_NONE) {
			usage();
			return EXIT_INVALID_USAGE;
		}
		backend = BACKEND_DEVICE;
		seat_or_device[0] = argv[optind];
	} else if (backend == BACKEND_NONE) {
		backend = BACKEND_UDEV;
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

	li = tools_open_backend(backend, seat_or_device, false, &grab);
	if (!li)
		return EXIT_FAILURE;

	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != -1)
		termwidth = w.ws_col;

	ctx.libinput = li;
	mainloop(&ctx);

	libinput_unref(li);

	return EXIT_SUCCESS;
}
