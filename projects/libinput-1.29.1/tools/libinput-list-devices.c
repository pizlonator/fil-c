/*
 * Copyright © 2015 Red Hat, Inc.
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
#include <libevdev/libevdev.h>
#include <libinput-version.h>
#include <libinput.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util-strings.h"

#include "shared.h"

static const char *
tap_default(struct libinput_device *device)
{
	if (!libinput_device_config_tap_get_finger_count(device))
		return "n/a";

	if (libinput_device_config_tap_get_default_enabled(device))
		return "enabled";

	return "disabled";
}

static const char *
tap_button_map(struct libinput_device *device)
{
	if (!libinput_device_config_tap_get_finger_count(device))
		return "n/a";

	switch (libinput_device_config_tap_get_button_map(device)) {
	case LIBINPUT_CONFIG_TAP_MAP_LRM:
		return "left/right/middle";
	case LIBINPUT_CONFIG_TAP_MAP_LMR:
		return "left/middle/right";
	}

	return "<invalid value>";
}

static const char *
drag_default(struct libinput_device *device)
{
	if (!libinput_device_config_tap_get_finger_count(device))
		return "n/a";

	if (libinput_device_config_tap_get_default_drag_enabled(device))
		return "enabled";

	return "disabled";
}

static const char *
draglock_default(struct libinput_device *device)
{
	if (!libinput_device_config_tap_get_finger_count(device))
		return "n/a";

	if (libinput_device_config_tap_get_default_drag_lock_enabled(device))
		return "enabled";

	return "disabled";
}

static const char *
left_handed_default(struct libinput_device *device)
{
	if (!libinput_device_config_left_handed_is_available(device))
		return "n/a";

	if (libinput_device_config_left_handed_get_default(device))
		return "enabled";

	return "disabled";
}

static const char *
nat_scroll_default(struct libinput_device *device)
{
	if (!libinput_device_config_scroll_has_natural_scroll(device))
		return "n/a";

	if (libinput_device_config_scroll_get_default_natural_scroll_enabled(device))
		return "enabled";

	return "disabled";
}

static const char *
middle_emulation_default(struct libinput_device *device)
{
	if (!libinput_device_config_middle_emulation_is_available(device))
		return "n/a";

	if (libinput_device_config_middle_emulation_get_default_enabled(device))
		return "enabled";

	return "disabled";
}

static char *
calibration_default(struct libinput_device *device)
{
	char *str;
	float calibration[6];

	if (!libinput_device_config_calibration_has_matrix(device)) {
		xasprintf(&str, "n/a");
		return str;
	}

	if (libinput_device_config_calibration_get_default_matrix(device,
								  calibration) == 0) {
		xasprintf(&str, "identity matrix");
		return str;
	}

	xasprintf(&str,
		  "%.2f %.2f %.2f %.2f %.2f %.2f",
		  calibration[0],
		  calibration[1],
		  calibration[2],
		  calibration[3],
		  calibration[4],
		  calibration[5]);
	return str;
}

static char *
scroll_defaults(struct libinput_device *device)
{
	uint32_t scroll_methods;
	char *str;
	enum libinput_config_scroll_method method;

	scroll_methods = libinput_device_config_scroll_get_methods(device);
	if (scroll_methods == LIBINPUT_CONFIG_SCROLL_NO_SCROLL) {
		xasprintf(&str, "none");
		return str;
	}

	method = libinput_device_config_scroll_get_default_method(device);

	xasprintf(&str,
		  "%s%s%s%s%s%s",
		  (method == LIBINPUT_CONFIG_SCROLL_2FG) ? "*" : "",
		  (scroll_methods & LIBINPUT_CONFIG_SCROLL_2FG) ? "two-finger " : "",
		  (method == LIBINPUT_CONFIG_SCROLL_EDGE) ? "*" : "",
		  (scroll_methods & LIBINPUT_CONFIG_SCROLL_EDGE) ? "edge " : "",
		  (method == LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) ? "*" : "",
		  (scroll_methods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) ? "button"
									   : "");
	return str;
}

static const char *
scroll_button_default(struct libinput_device *device)
{
	uint32_t scroll_methods = libinput_device_config_scroll_get_methods(device);
	if (scroll_methods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) {
		uint32_t button =
			libinput_device_config_scroll_get_default_button(device);
		return libevdev_event_code_get_name(EV_KEY, button);
	}

	return "n/a";
}

static const char *
scroll_button_lock_default(struct libinput_device *device)
{
	uint32_t scroll_methods = libinput_device_config_scroll_get_methods(device);
	if (scroll_methods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN) {
		switch (libinput_device_config_scroll_get_default_button_lock(device)) {
		case LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_ENABLED:
			return "enabled";
		case LIBINPUT_CONFIG_SCROLL_BUTTON_LOCK_DISABLED:
			return "disabled";
		}
		return "<invalid value>";
	}

	return "n/a";
}

static char *
click_defaults(struct libinput_device *device)
{
	uint32_t click_methods;
	char *str;
	enum libinput_config_click_method method;

	click_methods = libinput_device_config_click_get_methods(device);
	if (click_methods == LIBINPUT_CONFIG_CLICK_METHOD_NONE) {
		xasprintf(&str, "none");
		return str;
	}

	method = libinput_device_config_click_get_default_method(device);
	xasprintf(&str,
		  "%s%s%s%s",
		  (method == LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS) ? "*" : "",
		  (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS)
			  ? "button-areas "
			  : "",
		  (method == LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) ? "*" : "",
		  (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER)
			  ? "clickfinger "
			  : "");
	return str;
}

static const char *
clickfinger_button_map(struct libinput_device *device)
{
	uint32_t click_methods = libinput_device_config_click_get_methods(device);
	if (click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER) {
		switch (libinput_device_config_click_get_default_clickfinger_button_map(
			device)) {
		case LIBINPUT_CONFIG_CLICKFINGER_MAP_LMR:
			return "left/middle/right";
		case LIBINPUT_CONFIG_CLICKFINGER_MAP_LRM:
			return "left/right/middle";
		}
		return "<invalid value>";
	} else {

		return "n/a";
	}
}

static char *
accel_profiles(struct libinput_device *device)
{
	uint32_t profiles;
	char *str;
	enum libinput_config_accel_profile profile;

	if (!libinput_device_config_accel_is_available(device)) {
		xasprintf(&str, "n/a");
		return str;
	}

	profiles = libinput_device_config_accel_get_profiles(device);
	if (profiles == LIBINPUT_CONFIG_ACCEL_PROFILE_NONE) {
		xasprintf(&str, "none");
		return str;
	}

	profile = libinput_device_config_accel_get_default_profile(device);
	xasprintf(&str,
		  "%s%s %s%s %s%s",
		  (profile == LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT) ? "*" : "",
		  (profiles & LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT) ? "flat" : "",
		  (profile == LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE) ? "*" : "",
		  (profiles & LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE) ? "adaptive" : "",
		  (profile == LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM) ? "*" : "",
		  (profiles & LIBINPUT_CONFIG_ACCEL_PROFILE_CUSTOM) ? "custom" : "");

	return str;
}

static const char *
dwt_default(struct libinput_device *device)
{
	if (!libinput_device_config_dwt_is_available(device))
		return "n/a";

	if (libinput_device_config_dwt_get_default_enabled(device))
		return "enabled";

	return "disabled";
}

static const char *
dwtp_default(struct libinput_device *device)
{
	if (!libinput_device_config_dwtp_is_available(device))
		return "n/a";

	if (libinput_device_config_dwtp_get_default_enabled(device))
		return "enabled";

	return "disabled";
}

static char *
rotation_default(struct libinput_device *device)
{
	char *str;
	double angle;

	if (!libinput_device_config_rotation_is_available(device)) {
		xasprintf(&str, "n/a");
		return str;
	}

	angle = libinput_device_config_rotation_get_angle(device);
	xasprintf(&str, "%.1f", angle);
	return str;
}

static char *
area_rectangle(struct libinput_device *device)
{
	if (libinput_device_config_area_has_rectangle(device)) {
		struct libinput_config_area_rectangle rect =
			libinput_device_config_area_get_default_rectangle(device);

		char *str;
		xasprintf(&str,
			  "(%.2f, %.2f) - (%.2f, %.2f)",
			  rect.x1,
			  rect.y1,
			  rect.x2,
			  rect.y2);
		return str;
	}

	return safe_strdup("n/a");
}

static void
print_pad_info(struct libinput_device *device)
{
	int nbuttons, nrings, nstrips, ndials, ngroups, nmodes;
	struct libinput_tablet_pad_mode_group *group;

	nbuttons = libinput_device_tablet_pad_get_num_buttons(device);
	nrings = libinput_device_tablet_pad_get_num_rings(device);
	nstrips = libinput_device_tablet_pad_get_num_strips(device);
	ndials = libinput_device_tablet_pad_get_num_dials(device);
	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);

	printf("Pad:\n");
	printf("    Rings:   %d\n", nrings);
	printf("    Strips:  %d\n", nstrips);
	printf("    Dials:   %d\n", ndials);
	printf("    Buttons: %d\n", nbuttons);
	printf("    Mode groups: %d\n", ngroups);
	for (int g = 0; g < ngroups; g++) {
		group = libinput_device_tablet_pad_get_mode_group(device, g);
		nmodes = libinput_tablet_pad_mode_group_get_num_modes(group);
		printf("        Group %d:\n", g);
		printf("            Modes: %d\n", nmodes);
		if (nbuttons > 0) {
			printf("            Buttons:");
			for (int b = 0; b < nbuttons; b++) {
				if (libinput_tablet_pad_mode_group_has_button(group, b))
					printf("%s%s%d",
					       b == 0 ? " " : ", ",
					       libinput_tablet_pad_mode_group_button_is_toggle(
						       group,
						       b)
						       ? "*"
						       : "",
					       b);
			}
			printf("\n");
		}
		if (nrings > 0) {
			printf("            Rings:");
			for (int r = 0; r < nrings; r++) {
				if (libinput_tablet_pad_mode_group_has_ring(group, r))
					printf("%s%d", r == 0 ? " " : ", ", r);
			}
			printf("\n");
		}
		if (nstrips > 0) {
			printf("            Strips:");
			for (int s = 0; s < nstrips; s++) {
				if (libinput_tablet_pad_mode_group_has_strip(group, s))
					printf("%s%d", s == 0 ? " " : ", ", s);
			}
			printf("\n");
		}
		if (ndials > 0) {
			printf("            Dials:");
			for (int d = 0; d < ndials; d++) {
				if (libinput_tablet_pad_mode_group_has_dial(group, d))
					printf("%s%d", d == 0 ? " " : ", ", d);
			}
			printf("\n");
		}
	}
}

#define print_aligned(topic, fmt, ...) do {\
	printf("%-25s" fmt "\n", topic ":", __VA_ARGS__); \
} while (0)

static void
print_device_notify(struct libinput_event *ev)
{
	struct libinput_device *dev = libinput_event_get_device(ev);
	struct libinput_seat *seat = libinput_device_get_seat(dev);
	struct libinput_device_group *group;
	struct udev_device *udev_device;
	double w, h;
	static int next_group_id = 0;
	intptr_t group_id;
	const char *devnode;
	char *str;
	const char *bustype = "<unknown>";

	group = libinput_device_get_device_group(dev);
	group_id = (intptr_t)libinput_device_group_get_user_data(group);
	if (!group_id) {
		group_id = ++next_group_id;
		libinput_device_group_set_user_data(group, (void *)group_id);
	}

	udev_device = libinput_device_get_udev_device(dev);
	devnode = udev_device_get_devnode(udev_device);

	print_aligned("Device", "%s", libinput_device_get_name(dev));
	print_aligned("Kernel", "%s", devnode);

	switch (libinput_device_get_id_bustype(dev)) {
	case BUS_USB:
		bustype = "usb";
		break;
	case BUS_BLUETOOTH:
		bustype = "bluetooth";
		break;
	case BUS_VIRTUAL:
		bustype = "virtual";
		break;
	case BUS_I2C:
		bustype = "i2c";
		break;
	case BUS_HOST:
		bustype = "host";
		break;
	case BUS_I8042:
		bustype = "serial";
		break;
	}
	print_aligned("Id",
		      "%s:%04x:%04x",
		      bustype,
		      libinput_device_get_id_vendor(dev),
		      libinput_device_get_id_product(dev));

	print_aligned("Group", "%d", (int)group_id);
	print_aligned("Seat",
		      "%s, %s",
		      libinput_seat_get_physical_name(seat),
		      libinput_seat_get_logical_name(seat));

	udev_device_unref(udev_device);

	if (libinput_device_get_size(dev, &w, &h) == 0)
		print_aligned("Size", "%.fx%.fmm", w, h);

	print_aligned(
		"Capabilities",
		"%s%s%s%s%s%s%s",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD)
			? "keyboard "
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER)
			? "pointer "
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TOUCH)
			? "touch "
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_TOOL)
			? "tablet "
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_PAD)
			? "tablet-pad"
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_GESTURE)
			? "gesture"
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_SWITCH)
			? "switch"
			: "");

	print_aligned("Tap-to-click", "%s", tap_default(dev));
	print_aligned("Tap-and-drag", "%s", drag_default(dev));
	print_aligned("Tap button map", "%s", tap_button_map(dev));
	print_aligned("Tap drag lock", "%s", draglock_default(dev));
	print_aligned("Left-handed", "%s", left_handed_default(dev));
	print_aligned("Nat.scrolling", "%s", nat_scroll_default(dev));
	print_aligned("Middle emulation", "%s", middle_emulation_default(dev));
	str = calibration_default(dev);
	print_aligned("Calibration", "%s", str);
	free(str);

	str = scroll_defaults(dev);
	print_aligned("Scroll methods", "%s", str);
	free(str);

	print_aligned("Scroll button", "%s", scroll_button_default(dev));
	print_aligned("Scroll button lock", "%s", scroll_button_lock_default(dev));

	str = click_defaults(dev);
	print_aligned("Click methods", "%s", str);
	free(str);

	print_aligned("Clickfinger button map", "%s", clickfinger_button_map(dev));

	print_aligned("Disable-w-typing", "%s", dwt_default(dev));
	print_aligned("Disable-w-trackpointing", "%s", dwtp_default(dev));

	str = accel_profiles(dev);
	print_aligned("Accel profiles", "%s", str);
	free(str);

	str = rotation_default(dev);
	print_aligned("Rotation", "%s", str);
	free(str);

	str = area_rectangle(dev);
	print_aligned("Area rectangle", "%s", str);
	free(str);

	if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_PAD))
		print_pad_info(dev);

	printf("\n");
}

static inline void
usage(void)
{
	printf("Usage: libinput list-devices [--help|--version]\n");
	printf("\n"
	       "--help ...... show this help and exit\n"
	       "--version ... show version information and exit\n"
	       "\n");
}

int
main(int argc, char **argv)
{
	struct libinput *li;
	struct libinput_event *ev;
	bool grab = false;

	while (1) {
		int c;
		int option_index = 0;
		enum {
			OPT_HELP = 1,
			OPT_VERBOSE,
		};
		static struct option opts[] = {
			CONFIGURATION_OPTIONS,
			{ "help", no_argument, 0, 'h' },
			{ "verbose", no_argument, 0, OPT_VERBOSE },
			{ 0, 0, 0, 0 }
		};
		c = getopt_long(argc, argv, "h", opts, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case '?':
			return EXIT_INVALID_USAGE;
		case 'h':
		case OPT_HELP:
			usage();
			return EXIT_SUCCESS;
		default:
			return EXIT_INVALID_USAGE;
		}
	}
	if (optind < argc) {
		const char *devices[32] = { NULL };
		size_t ndevices = 0;
		do {
			if (ndevices >= ARRAY_LENGTH(devices) - 1) {
				usage();
				return EXIT_INVALID_USAGE;
			}
			devices[ndevices++] = argv[optind];
		} while (++optind < argc);
		li = tools_open_backend(BACKEND_DEVICE, devices, false, &grab);
	} else {
		const char *seat[2] = { "seat0", NULL };
		li = tools_open_backend(BACKEND_UDEV, seat, false, &grab);
	}
	if (!li)
		return 1;

	libinput_dispatch(li);
	while ((ev = libinput_get_event(li))) {

		if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED)
			print_device_notify(ev);

		libinput_event_destroy(ev);
		libinput_dispatch(li);
	}

	libinput_unref(li);

	return EXIT_SUCCESS;
}
