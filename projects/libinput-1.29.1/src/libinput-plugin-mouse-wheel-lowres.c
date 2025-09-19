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

#include <libevdev/libevdev.h>

#include "evdev.h"
#include "libinput-plugin-mouse-wheel-lowres.h"
#include "libinput-plugin.h"
#include "src/evdev-frame.h"

static void
wheel_plugin_device_new(struct libinput_plugin *libinput_plugin,
			struct libinput_device *device,
			struct libevdev *libevdev,
			struct udev_device *udev_device)
{
	struct evdev_device *evdev = evdev_device(device);

	if (libevdev_has_event_code(libevdev, EV_REL, REL_WHEEL_HI_RES) ||
	    libevdev_has_event_code(libevdev, EV_REL, REL_HWHEEL_HI_RES))
		return;

	if (libevdev_has_event_code(libevdev, EV_REL, REL_WHEEL) ||
	    libevdev_has_event_code(libevdev, EV_REL, REL_HWHEEL))
		evdev_log_info(evdev,
			       "emulating high-resolution scroll wheel events.\n");

	if (libevdev_has_event_code(libevdev, EV_REL, REL_WHEEL))
		libevdev_enable_event_code(libevdev, EV_REL, REL_WHEEL_HI_RES, NULL);

	if (libevdev_has_event_code(libevdev, EV_REL, REL_HWHEEL))
		libevdev_enable_event_code(libevdev, EV_REL, REL_HWHEEL_HI_RES, NULL);

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);
}

static void
wheel_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			 struct libinput_device *device,
			 struct evdev_frame *frame)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	_unref_(evdev_frame) *filtered_frame = evdev_frame_new(nevents + 2);
	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *e = &events[i];

		switch (evdev_usage_enum(e->usage)) {
		case EVDEV_REL_WHEEL_HI_RES:
		case EVDEV_REL_HWHEEL_HI_RES:
			/* In the uncommon case that our device sends high-res events
			 * filter those out. This can happen on devices that have the
			 * highres scroll axes disabled via quirks. The device still
			 * sends events so when we re-enable the axis in
			 * wheel_plugin_device_new we get the device events again,
			 * effectively duplicating the high resolution scroll events.
			 */
			break;
		case EVDEV_REL_WHEEL:
			evdev_frame_append(filtered_frame, e, 1);
			evdev_frame_append_one(filtered_frame,
					       evdev_usage_from(EVDEV_REL_WHEEL_HI_RES),
					       e->value * 120);
			break;
		case EVDEV_REL_HWHEEL:
			evdev_frame_append(filtered_frame, e, 1);
			evdev_frame_append_one(
				filtered_frame,
				evdev_usage_from(EVDEV_REL_HWHEEL_HI_RES),
				e->value * 120);
			break;
		default:
			evdev_frame_append(filtered_frame, e, 1);
			break;
		}
	}

	evdev_frame_set(frame,
			evdev_frame_get_events(filtered_frame, NULL),
			evdev_frame_get_count(filtered_frame));
}

static const struct libinput_plugin_interface interface = {
	.device_new = wheel_plugin_device_new,
	.evdev_frame = wheel_plugin_evdev_frame,
};

void
libinput_mouse_plugin_wheel_lowres(struct libinput *libinput)
{
	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "mouse-wheel-lowres", &interface, NULL);
}
