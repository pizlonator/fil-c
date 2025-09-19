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

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <libevdev/libevdev.h>
#include <libudev.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util-macros.h"
#include "util-strings.h"

#include "builddir.h"
#include "libinput-util.h"
#include "libinput.h"
#include "shared.h"

static uint32_t dispatch_counter = 0;
uint32_t log_serial = 0;

void
tools_dispatch(struct libinput *libinput)
{
	dispatch_counter++;
	libinput_dispatch(libinput);
}

LIBINPUT_ATTRIBUTE_PRINTF(3, 0)
static void
log_handler(struct libinput *li,
	    enum libinput_log_priority priority,
	    const char *format,
	    va_list args)
{
	static int is_tty = -1;
	static uint32_t last_dispatch_no = 0;
	static bool color_toggle = false;

	if (is_tty == -1)
		is_tty = isatty(STDOUT_FILENO);

	if (is_tty) {
		if (priority >= LIBINPUT_LOG_PRIORITY_ERROR) {
			if (strstr(format, "client bug: ") ||
			    strstr(format, "libinput bug: ") ||
			    strstr(format, "kernel bug: "))
				printf(ANSI_BRIGHT_RED);
			else
				printf(ANSI_RED);
		} else if (priority >= LIBINPUT_LOG_PRIORITY_INFO) {
			printf(ANSI_BOLD);
		} else if (priority == LIBINPUT_LOG_PRIORITY_DEBUG) {
			if (dispatch_counter != last_dispatch_no)
				color_toggle = !color_toggle;
			uint8_t r = 0, g = 135, b = 95 + (color_toggle ? 80 : 0);
			printf("\x1B[38;2;%u;%u;%um", r, g, b);
		}
	}

	if (priority < LIBINPUT_LOG_PRIORITY_INFO) {
		if (dispatch_counter != last_dispatch_no) {
			last_dispatch_no = dispatch_counter;
			printf("%4u: ", dispatch_counter);
		} else {
			printf(" %4s ", "...");
		}
	}
	vprintf(format, args);

	if (is_tty)
		printf(ANSI_NORMAL);

	log_serial++;
}

void
tools_init_options(struct tools_options *options)
{
	memset(options, 0, sizeof(*options));
	options->tapping = -1;
	options->tap_map = -1;
	options->drag = -1;
	options->drag_lock = -1;
	options->natural_scroll = -1;
	options->left_handed = -1;
	options->middlebutton = -1;
	options->dwt = -1;
	options->dwtp = -1;
	options->click_method = -1;
	options->scroll_method = -1;
	options->scroll_button = -1;
	options->scroll_button_lock = -1;
	options->speed = 0.0;
	options->profile = LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
	/* initialize accel args */
	static double points[] = { 0.0, 1.0 };
	options->custom_points = points;
	options->custom_npoints = ARRAY_LENGTH(points);
	options->custom_type = LIBINPUT_ACCEL_TYPE_FALLBACK;
	options->custom_step = 1.0;
	options->pressure_range[0] = 0.0;
	options->pressure_range[1] = 1.0;
	options->calibration[0] = 1.0;
	options->calibration[4] = 1.0;
	options->area.x1 = 0.0;
	options->area.y1 = 0.0;
	options->area.x2 = 1.0;
	options->area.y2 = 1.0;
	options->sendevents = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	options->eraser_button_mode = LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT;
	options->eraser_button_button = BTN_STYLUS;
	options->eraser_button_button = 0;
}

int
tools_parse_option(int option, const char *optarg, struct tools_options *options)
{
	switch (option) {
	case OPT_TAP_ENABLE:
		options->tapping = 1;
		break;
	case OPT_TAP_DISABLE:
		options->tapping = 0;
		break;
	case OPT_TAP_MAP:
		if (!optarg)
			return 1;

		if (streq(optarg, "lrm")) {
			options->tap_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
		} else if (streq(optarg, "lmr")) {
			options->tap_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
		} else {
			return 1;
		}
		break;
	case OPT_DRAG_ENABLE:
		options->drag = 1;
		break;
	case OPT_DRAG_DISABLE:
		options->drag = 0;
		break;
	case OPT_DRAG_LOCK_ENABLE:
		if (optarg) {
			if (streq(optarg, "sticky")) {
				options->drag_lock =
					LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_STICKY;
			} else if (streq(optarg, "timeout")) {
				options->drag_lock =
					LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_TIMEOUT;
			} else {
				return 1;
			}
		} else {
			options->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED_TIMEOUT;
		}
		break;
	case OPT_DRAG_LOCK_DISABLE:
		options->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
		break;
	case OPT_NATURAL_SCROLL_ENABLE:
		options->natural_scroll = 1;
		break;
	case OPT_NATURAL_SCROLL_DISABLE:
		options->natural_scroll = 0;
		break;
	case OPT_LEFT_HANDED_ENABLE:
		options->left_handed = 1;
		break;
	case OPT_LEFT_HANDED_DISABLE:
		options->left_handed = 0;
		break;
	case OPT_MIDDLEBUTTON_ENABLE:
		options->middlebutton = 1;
		break;
	case OPT_MIDDLEBUTTON_DISABLE:
		options->middlebutton = 0;
		break;
	case OPT_DWT_ENABLE:
		options->dwt = LIBINPUT_CONFIG_DWT_ENABLED;
		break;
	case OPT_DWT_DISABLE:
		options->dwt = LIBINPUT_CONFIG_DWT_DISABLED;
		break;
	case OPT_DWTP_ENABLE:
		options->dwtp = LIBINPUT_CONFIG_DWTP_ENABLED;
		break;
	case OPT_DWTP_DISABLE:
		options->dwtp = LIBINPUT_CONFIG_DWTP_DISABLED;
		break;
	case OPT_CLICK_METHOD:
		if (!optarg)
			return 1;

		if (streq(optarg, "none")) {
			options->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
		} else if (streq(optarg, "clickfinger")) {
			options->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
		} else if (streq(optarg, "buttonareas")) {
			options->click_method =
				LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
		} else {
			return 1;
		}
		break;
	case OPT_CLICKFINGER_MAP:
		if (!optarg)
			return 1;

		if (streq(optarg, "lrm")) {
			options->clickfinger_map = LIBINPUT_CONFIG_CLICKFINGER_MAP_LRM;
		} else if (streq(optarg, "lmr")) {
			options->clickfinger_map = LIBINPUT_CONFIG_CLICKFINGER_MAP_LMR;
		} else {
			return 1;
		}
		break;
	case OPT_SCROLL_METHOD:
		if (!optarg)
			return 1;

		if (streq(optarg, "none")) {
			options->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
		} else if (streq(optarg, "twofinger")) {
			options->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
		} else if (streq(optarg, "edge")) {
			options->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
		} else if (streq(optarg, "button")) {
			options->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
		} else {
			return 1;
		}
		break;
	case OPT_SCROLL_BUTTON:
		if (!optarg) {
			return 1;
		}
		options->scroll_button = libevdev_event_code_from_name(EV_KEY, optarg);
		if (options->scroll_button == -1) {
			fprintf(stderr, "Invalid button %s\n", optarg);
			return 1;
		}
		break;
	case OPT_SCROLL_BUTTON_LOCK_ENABLE:
		options->scroll_button_lock = true;
		break;
	case OPT_SCROLL_BUTTON_LOCK_DISABLE:
		options->scroll_button_lock = false;
		break;
	case OPT_SPEED:
		if (!optarg)
			return 1;
		options->speed = atof(optarg);
		break;
	case OPT_PROFILE:
		if (!optarg)
			return 1;

		if (streq(optarg, "adaptive"))
			options->profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
		else if (streq(optarg, "flat"))
			options->profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
		else if (streq(optarg, "custom"))
			options->profile = LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM;
		else
			return 1;
		break;
	case OPT_DISABLE_SENDEVENTS:
		if (!optarg)
			return 1;

		snprintf(options->disable_pattern,
			 sizeof(options->disable_pattern),
			 "%s",
			 optarg);
		break;
	case OPT_SENDEVENTS:
		if (streq(optarg, "disabled"))
			options->sendevents = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
		else if (streq(optarg, "enabled"))
			options->sendevents = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
		else if (streq(optarg, "disabled-on-external-mouse"))
			options->sendevents =
				LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
		else {
			fprintf(stderr, "Invalid sendevents mode: %s\n", optarg);
			return 1;
		}
		break;
	case OPT_APPLY_TO:
		if (!optarg)
			return 1;

		snprintf(options->match, sizeof(options->match), "%s", optarg);
		break;
	case OPT_CUSTOM_POINTS:
		if (!optarg)
			return 1;
		options->custom_points =
			double_array_from_string(optarg, ";", &options->custom_npoints);
		if (!options->custom_points || options->custom_npoints < 2) {
			fprintf(stderr,
				"Invalid --set-custom-points\n"
				"Please provide at least 2 points separated by a semicolon\n"
				" e.g. --set-custom-points=\"1.0;1.5\"\n");
			return 1;
		}
		break;
	case OPT_CUSTOM_STEP:
		if (!optarg)
			return 1;
		options->custom_step = strtod(optarg, NULL);
		break;
	case OPT_CUSTOM_TYPE:
		if (!optarg)
			return 1;
		if (streq(optarg, "fallback"))
			options->custom_type = LIBINPUT_ACCEL_TYPE_FALLBACK;
		else if (streq(optarg, "motion"))
			options->custom_type = LIBINPUT_ACCEL_TYPE_MOTION;
		else if (streq(optarg, "scroll"))
			options->custom_type = LIBINPUT_ACCEL_TYPE_SCROLL;
		else {
			fprintf(stderr,
				"Invalid --set-custom-type\n"
				"Valid custom types: fallback|motion|scroll\n");
			return 1;
		}
		break;
	case OPT_ROTATION_ANGLE:
		if (!optarg)
			return 1;

		if (!safe_atou(optarg, &options->angle)) {
			fprintf(stderr, "Invalid --set-rotation-angle value\n");
			return 1;
		}
		break;
	case OPT_PRESSURE_RANGE: {
		if (!optarg)
			return 1;

		size_t npoints = 0;
		_autofree_ double *range =
			double_array_from_string(optarg, ":", &npoints);
		if (npoints != 2 || !range || range[0] < 0.0 || range[1] > 1.0 ||
		    range[0] >= range[1]) {
			fprintf(stderr,
				"Invalid pressure range, must be in format \"min:max\"\n");
			return 1;
		}
		options->pressure_range[0] = range[0];
		options->pressure_range[1] = range[1];
		break;
	}
	case OPT_CALIBRATION: {
		if (!optarg)
			return 1;

		size_t npoints = 0;
		_autofree_ double *matrix =
			double_array_from_string(optarg, " ", &npoints);
		if (!matrix || npoints != 6) {
			fprintf(stderr,
				"Invalid calibration matrix, must be 6 space-separated values\n");
			return 1;
		}
		for (size_t i = 0; i < 6; i++)
			options->calibration[i] = matrix[i];
		break;
	}
	case OPT_AREA: {
		if (!optarg)
			return 1;

		double x1, x2, y1, y2;

		if (sscanf(optarg, "%lf/%lf %lf/%lf", &x1, &y1, &x2, &y2) != 4) {
			fprintf(stderr, "Invalid --set-area values\n");
			return 1;
		}
		options->area.x1 = x1;
		options->area.y1 = y1;
		options->area.x2 = x2;
		options->area.y2 = y2;
		break;
	}
	case OPT_3FG_DRAG:
		if (!optarg)
			return 1;
		if (streq(optarg, "3fg"))
			options->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG;
		else if (streq(optarg, "4fg"))
			options->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_ENABLED_4FG;
		else if (streq(optarg, "disabled"))
			options->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_DISABLED;
		else {
			fprintf(stderr,
				"Invalid --enable-3fg-drag\n"
				"Valid options: 3fg|4fg|disabled\n");
			return 1;
		}
		break;
	case OPT_ERASER_BUTTON_MODE:
		if (!optarg)
			return 1;
		if (streq(optarg, "default"))
			options->eraser_button_mode =
				LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT;
		else if (streq(optarg, "button"))
			options->eraser_button_mode =
				LIBINPUT_CONFIG_ERASER_BUTTON_BUTTON;
		else {
			fprintf(stderr,
				"Invalid --set-eraser-button-mode\n"
				"Valid options: default|button\n");
			return 1;
		}
		break;
	case OPT_ERASER_BUTTON_BUTTON:
		if (!optarg)
			return 1;
		if (streq(optarg, "BTN_STYLUS"))
			options->eraser_button_button = BTN_STYLUS;
		else if (streq(optarg, "BTN_STYLUS2"))
			options->eraser_button_button = BTN_STYLUS2;
		else if (streq(optarg, "BTN_STYLUS3"))
			options->eraser_button_button = BTN_STYLUS3;
		else {
			fprintf(stderr, "Unsupported eraser button %s\n", optarg);
			return 1;
		}
		break;
	}
	return 0;
}

static int
open_restricted(const char *path, int flags, void *user_data)
{
	bool *grab = user_data;
	int fd = open(path, flags);

	if (fd < 0)
		fprintf(stderr, "Failed to open %s (%s)\n", path, strerror(errno));
	else if (grab && *grab && ioctl(fd, EVIOCGRAB, (void *)1) == -1)
		fprintf(stderr,
			"Grab requested, but failed for %s (%s)\n",
			path,
			strerror(errno));

	return fd < 0 ? -errno : fd;
}

static void
close_restricted(int fd, void *user_data)
{
	close(fd);
}

static const struct libinput_interface interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

static struct libinput *
tools_open_udev(const char *seat, bool verbose, bool *grab)
{
	_unref_(udev) *udev = udev_new();
	if (!udev) {
		fprintf(stderr, "Failed to initialize udev\n");
		return NULL;
	}

	_unref_(libinput) *li = libinput_udev_create_context(&interface, grab, udev);
	if (!li) {
		fprintf(stderr, "Failed to initialize context from udev\n");
		return NULL;
	}

	libinput_log_set_handler(li, log_handler);
	if (verbose)
		libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);

	if (libinput_udev_assign_seat(li, seat)) {
		fprintf(stderr, "Failed to set seat\n");
		return NULL;
	}

	return steal(&li);
}

static struct libinput *
tools_open_device(const char **paths, bool verbose, bool *grab)
{
	_unref_(libinput) *li = libinput_path_create_context(&interface, grab);
	if (!li) {
		fprintf(stderr, "Failed to initialize path context\n");
		return NULL;
	}

	if (verbose) {
		libinput_log_set_handler(li, log_handler);
		libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);
	}

	const char **p = paths;
	while (*p) {
		struct libinput_device *device = libinput_path_add_device(li, *p);
		if (!device) {
			fprintf(stderr, "Failed to initialize device %s\n", *p);
			return NULL;
		}
		p++;
	}

	return steal(&li);
}

static void
tools_setenv_quirks_dir(void)
{
	if (builddir_lookup(NULL))
		setenv("LIBINPUT_QUIRKS_DIR", LIBINPUT_QUIRKS_SRCDIR, 0);
}

struct libinput *
tools_open_backend(enum tools_backend which,
		   const char **seat_or_device,
		   bool verbose,
		   bool *grab)
{
	struct libinput *li;

	tools_setenv_quirks_dir();

	switch (which) {
	case BACKEND_UDEV:
		li = tools_open_udev(seat_or_device[0], verbose, grab);
		break;
	case BACKEND_DEVICE:
		li = tools_open_device(seat_or_device, verbose, grab);
		break;
	default:
		abort();
	}

	return li;
}

void
tools_device_apply_config(struct libinput_device *device, struct tools_options *options)
{
	const char *name = libinput_device_get_name(device);

	if (libinput_device_config_send_events_get_modes(device) &
		    LIBINPUT_CONFIG_SEND_EVENTS_DISABLED &&
	    fnmatch(options->disable_pattern, name, 0) != FNM_NOMATCH) {
		libinput_device_config_send_events_set_mode(
			device,
			LIBINPUT_CONFIG_SEND_EVENTS_DISABLED);
	}

	if (strlen(options->match) > 0 &&
	    fnmatch(options->match, name, 0) == FNM_NOMATCH)
		return;

	libinput_device_config_send_events_set_mode(device, options->sendevents);

	if (options->tapping != -1)
		libinput_device_config_tap_set_enabled(device, options->tapping);
	if (options->tap_map != (enum libinput_config_tap_button_map) - 1)
		libinput_device_config_tap_set_button_map(device, options->tap_map);
	if (options->drag != -1)
		libinput_device_config_tap_set_drag_enabled(device, options->drag);
	if (options->drag_lock != -1)
		libinput_device_config_tap_set_drag_lock_enabled(device,
								 options->drag_lock);
	if (options->natural_scroll != -1)
		libinput_device_config_scroll_set_natural_scroll_enabled(
			device,
			options->natural_scroll);
	if (options->left_handed != -1)
		libinput_device_config_left_handed_set(device, options->left_handed);
	if (options->middlebutton != -1)
		libinput_device_config_middle_emulation_set_enabled(
			device,
			options->middlebutton);

	if (options->dwt != -1)
		libinput_device_config_dwt_set_enabled(device, options->dwt);

	if (options->dwtp != -1)
		libinput_device_config_dwtp_set_enabled(device, options->dwtp);

	if (options->click_method != (enum libinput_config_click_method) - 1)
		libinput_device_config_click_set_method(device, options->click_method);

	if (options->clickfinger_map !=
	    (enum libinput_config_clickfinger_button_map) - 1)
		libinput_device_config_click_set_clickfinger_button_map(
			device,
			options->clickfinger_map);

	if (options->scroll_method != (enum libinput_config_scroll_method) - 1)
		libinput_device_config_scroll_set_method(device,
							 options->scroll_method);
	if (options->scroll_button != -1)
		libinput_device_config_scroll_set_button(device,
							 options->scroll_button);
	if (options->scroll_button_lock != -1)
		libinput_device_config_scroll_set_button_lock(
			device,
			options->scroll_button_lock);

	if (libinput_device_config_accel_is_available(device)) {
		libinput_device_config_accel_set_speed(device, options->speed);
		if (options->profile != LIBINPUT_CONFIG_ACCEL_PROFILE_NONE)
			libinput_device_config_accel_set_profile(device,
								 options->profile);
	}

	if (options->profile == LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM) {
		_destroy_(libinput_config_accel) *config = libinput_config_accel_create(
			LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM);
		libinput_config_accel_set_points(config,
						 options->custom_type,
						 options->custom_step,
						 options->custom_npoints,
						 options->custom_points);
		libinput_device_config_accel_apply(device, config);
	}

	if (options->angle != 0)
		libinput_device_config_rotation_set_angle(device, options->angle % 360);

	if (libinput_device_config_calibration_has_matrix(device))
		libinput_device_config_calibration_set_matrix(device,
							      options->calibration);

	if (libinput_device_config_area_has_rectangle(device))
		libinput_device_config_area_set_rectangle(device, &options->area);

	if (libinput_device_config_3fg_drag_get_finger_count(device) >= 3)
		libinput_device_config_3fg_drag_set_enabled(device, options->drag_3fg);
}

void
tools_tablet_tool_apply_config(struct libinput_tablet_tool *tool,
			       struct tools_options *options)
{
	libinput_tablet_tool_config_pressure_range_set(tool,
						       options->pressure_range[0],
						       options->pressure_range[1]);
	if (options->eraser_button_button)
		libinput_tablet_tool_config_eraser_button_set_button(
			tool,
			options->eraser_button_button);
	libinput_tablet_tool_config_eraser_button_set_mode(tool,
							   options->eraser_button_mode);
}

static char *
find_device(const char *udev_tag)
{
	_unref_(udev) *udev = udev_new();
	if (!udev)
		return NULL;

	_unref_(udev_enumerate) *e = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(e, "input");
	udev_enumerate_scan_devices(e);

	struct udev_list_entry *entry = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		const char *path = udev_list_entry_get_name(entry);
		_unref_(udev_device) *device = udev_device_new_from_syspath(udev, path);
		if (!device)
			continue;

		const char *sysname = udev_device_get_sysname(device);
		if (!strstartswith("event", sysname)) {
			continue;
		}

		if (udev_device_get_property_value(device, udev_tag)) {
			char *device_node =
				safe_strdup(udev_device_get_devnode(device));
			if (device_node)
				return device_node;
		}
	}

	return NULL;
}

bool
find_touchpad_device(char *path, size_t path_len)
{
	_autofree_ char *devnode = find_device("ID_INPUT_TOUCHPAD");

	if (devnode)
		snprintf(path, path_len, "%s", devnode);

	return devnode != NULL;
}

bool
is_touchpad_device(const char *devnode)
{
	struct stat st;
	if (stat(devnode, &st) < 0)
		return false;

	_unref_(udev) *udev = udev_new();
	if (!udev)
		return false;

	_unref_(udev_device) *dev = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
	if (!dev)
		return false;

	return udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
}

static inline void
setup_path(void)
{
	const char *path = getenv("PATH");
	char new_path[PATH_MAX];
	const char *extra_path = LIBINPUT_TOOL_PATH;
	_autofree_ char *builddir = NULL;

	builddir_lookup(&builddir);
	snprintf(new_path,
		 sizeof(new_path),
		 "%s:%s",
		 builddir ? builddir : extra_path,
		 path ? path : "");
	setenv("PATH", new_path, 1);
}

int
tools_exec_command(const char *prefix, int real_argc, char **real_argv)
{
	char *argv[64] = { NULL };
	char executable[128];
	const char *command;
	int rc;

	assert((size_t)real_argc < ARRAY_LENGTH(argv));

	command = real_argv[0];

	rc = snprintf(executable, sizeof(executable), "%s-%s", prefix, command);
	if (rc >= (int)sizeof(executable)) {
		fprintf(stderr, "Failed to assemble command.\n");
		return EXIT_FAILURE;
	}

	argv[0] = executable;
	for (int i = 1; i < real_argc; i++)
		argv[i] = real_argv[i];

	setup_path();

	rc = execvp(executable, argv);
	if (rc) {
		if (errno == ENOENT) {
			fprintf(stderr, "libinput: %s is not installed\n", command);
			return EXIT_INVALID_USAGE;
		}
		fprintf(stderr,
			"Failed to execute '%s' (%s)\n",
			command,
			strerror(errno));
	}

	return EXIT_FAILURE;
}

static void
sprintf_event_codes(char *buf, size_t sz, struct quirks *quirks, enum quirk q)
{
	const struct quirk_tuples *t;
	size_t off = 0;
	int printed;
	const char *name;

	quirks_get_tuples(quirks, q, &t);
	name = quirk_get_name(q);
	printed = snprintf(buf, sz, "%s=", name);
	assert(printed != -1);
	off += printed;

	for (size_t i = 0; off < sz && i < t->ntuples; i++) {
		unsigned int type = t->tuples[i].first;
		unsigned int code = t->tuples[i].second;
		bool enable = t->tuples[i].third;

		const char *name = libevdev_event_code_get_name(type, code);

		printed = snprintf(buf + off,
				   sz - off,
				   "%c%s;",
				   enable ? '+' : '-',
				   name);
		assert(printed != -1);
		off += printed;
	}
}

static void
sprintf_input_props(char *buf, size_t sz, struct quirks *quirks, enum quirk q)
{
	const struct quirk_tuples *t;
	size_t off = 0;
	int printed;
	const char *name;

	quirks_get_tuples(quirks, q, &t);
	name = quirk_get_name(q);
	printed = snprintf(buf, sz, "%s=", name);
	assert(printed != -1);
	off += printed;

	for (size_t i = 0; off < sz && i < t->ntuples; i++) {
		unsigned int prop = t->tuples[i].first;
		bool enable = t->tuples[i].second;

		const char *name = libevdev_property_get_name(prop);

		printed = snprintf(buf + off,
				   sz - off,
				   "%c%s;",
				   enable ? '+' : '-',
				   name);
		assert(printed != -1);
		off += printed;
	}
}

void
tools_list_device_quirks(struct quirks_context *ctx,
			 struct udev_device *device,
			 void (*callback)(void *data, const char *str),
			 void *userdata)
{
	char buf[256];

	enum quirk q;

	_unref_(quirks) *quirks = quirks_fetch_for_device(ctx, device);
	if (!quirks)
		return;

	q = QUIRK_MODEL_ALPS_SERIAL_TOUCHPAD;
	do {
		if (quirks_has_quirk(quirks, q)) {
			const char *name;
			bool b;

			name = quirk_get_name(q);
			quirks_get_bool(quirks, q, &b);
			snprintf(buf, sizeof(buf), "%s=%d", name, b ? 1 : 0);
			callback(userdata, buf);
		}
	} while (++q < _QUIRK_LAST_MODEL_QUIRK_);

	q = QUIRK_ATTR_SIZE_HINT;
	do {
		if (quirks_has_quirk(quirks, q)) {
			const char *name;
			struct quirk_dimensions dim;
			struct quirk_range r;
			uint32_t v;
			char *s;
			double d;
			bool b;

			name = quirk_get_name(q);

			switch (q) {
			case QUIRK_ATTR_SIZE_HINT:
			case QUIRK_ATTR_RESOLUTION_HINT:
				quirks_get_dimensions(quirks, q, &dim);
				snprintf(buf,
					 sizeof(buf),
					 "%s=%zdx%zd",
					 name,
					 dim.x,
					 dim.y);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_TOUCH_SIZE_RANGE:
			case QUIRK_ATTR_PRESSURE_RANGE:
				quirks_get_range(quirks, q, &r);
				snprintf(buf,
					 sizeof(buf),
					 "%s=%d:%d",
					 name,
					 r.upper,
					 r.lower);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_PALM_SIZE_THRESHOLD:
			case QUIRK_ATTR_PALM_PRESSURE_THRESHOLD:
			case QUIRK_ATTR_THUMB_PRESSURE_THRESHOLD:
			case QUIRK_ATTR_THUMB_SIZE_THRESHOLD:
				quirks_get_uint32(quirks, q, &v);
				snprintf(buf, sizeof(buf), "%s=%u", name, v);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_LID_SWITCH_RELIABILITY:
			case QUIRK_ATTR_KEYBOARD_INTEGRATION:
			case QUIRK_ATTR_TRACKPOINT_INTEGRATION:
			case QUIRK_ATTR_TPKBCOMBO_LAYOUT:
			case QUIRK_ATTR_MSC_TIMESTAMP:
				quirks_get_string(quirks, q, &s);
				snprintf(buf, sizeof(buf), "%s=%s", name, s);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_TRACKPOINT_MULTIPLIER:
				quirks_get_double(quirks, q, &d);
				snprintf(buf, sizeof(buf), "%s=%0.2f", name, d);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_USE_VELOCITY_AVERAGING:
			case QUIRK_ATTR_TABLET_SMOOTHING:
			case QUIRK_ATTR_IS_VIRTUAL:
				quirks_get_bool(quirks, q, &b);
				snprintf(buf, sizeof(buf), "%s=%d", name, b);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_EVENT_CODE:
				sprintf_event_codes(buf, sizeof(buf), quirks, q);
				callback(userdata, buf);
				break;
			case QUIRK_ATTR_INPUT_PROP:
				sprintf_input_props(buf, sizeof(buf), quirks, q);
				callback(userdata, buf);
				break;
			default:
				abort();
				break;
			}
		}
	} while (++q < _QUIRK_LAST_ATTR_QUIRK_);
}
