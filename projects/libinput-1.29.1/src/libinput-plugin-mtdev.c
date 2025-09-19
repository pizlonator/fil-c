/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2013 Jonas Ådahl
 * Copyright © 2013-2017 Red Hat, Inc.
 * Copyright © 2017 James Ye <jye836@gmail.com>
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
#include <mtdev-plumbing.h>
#include <mtdev.h>

#include "util-list.h"
#include "util-mem.h"
#include "util-strings.h"

#include "evdev-frame.h"
#include "libinput-log.h"
#include "libinput-plugin-mtdev.h"
#include "libinput-plugin.h"
#include "libinput-util.h"

struct plugin_device {
	struct list link;
	struct libinput_device *device;

	struct mtdev *mtdev;
};

struct plugin_data {
	struct list devices;
};

static void
plugin_device_destroy(struct plugin_device *device)
{
	libinput_device_unref(device->device);
	list_remove(&device->link);
	mtdev_close_delete(device->mtdev);
	free(device);
}

DEFINE_DESTROY_CLEANUP_FUNC(plugin_device);

static void
plugin_data_destroy(void *d)
{
	struct plugin_data *data = d;

	struct plugin_device *device;
	list_for_each_safe(device, &data->devices, link) {
		plugin_device_destroy(device);
	}

	free(data);
}

DEFINE_DESTROY_CLEANUP_FUNC(plugin_data);

static void
plugin_destroy(struct libinput_plugin *libinput_plugin)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	plugin_data_destroy(plugin);
}

static void
mtdev_plugin_device_handle_frame(struct libinput_plugin *libinput_plugin,
				 struct plugin_device *device,
				 struct evdev_frame *frame)
{
	uint64_t time = evdev_frame_get_time(frame);
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);
	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *ev = &events[i];
		struct input_event e = evdev_event_to_input_event(ev, time);
		mtdev_put_event(device->mtdev, &e);
	}
	evdev_frame_reset(frame);

	while (!mtdev_empty(device->mtdev)) {
		struct input_event e;

		mtdev_get_event(device->mtdev, &e);
		evdev_frame_append_input_event(frame, &e);
		if (e.type == EV_SYN && e.code == SYN_REPORT) {
			evdev_frame_set_time(frame, input_event_time(&e));
			/* mtdev can theoretically produce multiple frames but I
			 * dont think it ever does */
			if (!mtdev_empty(device->mtdev)) {
				plugin_log_bug(libinput_plugin,
					       "Didn't expect multiple frames");
				break;
			}
		}
	}
}

static void
mtdev_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			 struct libinput_device *device,
			 struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			mtdev_plugin_device_handle_frame(libinput_plugin, pd, frame);
			break;
		}
	}
}

static int
mtdev_needed(struct libevdev *evdev)
{
	return (libevdev_has_event_code(evdev, EV_ABS, ABS_MT_POSITION_X) &&
		libevdev_has_event_code(evdev, EV_ABS, ABS_MT_POSITION_Y) &&
		!libevdev_has_event_code(evdev, EV_ABS, ABS_MT_SLOT));
}

static void
mtdev_plugin_device_new(struct libinput_plugin *libinput_plugin,
			struct libinput_device *device,
			struct libevdev *evdev,
			struct udev_device *udev)
{
	if (!mtdev_needed(evdev))
		return;

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	_destroy_(plugin_device) *pd = zalloc(sizeof(*pd));
	pd->device = libinput_device_ref(device);
	pd->mtdev = mtdev_new();
	/* Shouldn't ever happen so no need to warn */
	if (!pd->mtdev) {
		libevdev_disable_event_code(evdev, EV_ABS, ABS_MT_POSITION_X);
		libevdev_disable_event_code(evdev, EV_ABS, ABS_MT_POSITION_Y);
		return;
	}
	mtdev_init(pd->mtdev);

	unsigned int codes[] = {
		ABS_MT_POSITION_X,  ABS_MT_POSITION_Y,  ABS_MT_TOUCH_MAJOR,
		ABS_MT_TOUCH_MINOR, ABS_MT_WIDTH_MAJOR, ABS_MT_WIDTH_MINOR,
		ABS_MT_ORIENTATION,
	};

	ARRAY_FOR_EACH(codes, code) {
		const struct input_absinfo *abs = libevdev_get_abs_info(evdev, *code);
		if (!abs)
			continue;

		mtdev_set_mt_event(pd->mtdev, *code, abs->value);
		mtdev_set_abs_minimum(pd->mtdev, *code, abs->minimum);
		mtdev_set_abs_maximum(pd->mtdev, *code, abs->maximum);
		mtdev_set_abs_fuzz(pd->mtdev, *code, abs->fuzz);
		mtdev_set_abs_resolution(pd->mtdev, *code, abs->resolution);
	}

	/* Let's pretend we have slots */
	const struct input_absinfo slot = {
		.minimum = 0,
		.maximum = 9,
		.value = 0,
	};
	const struct input_absinfo tid = {
		.minimum = 0,
		.maximum = 65535,
		.value = 0,
	};
	libevdev_enable_event_code(evdev, EV_ABS, ABS_MT_SLOT, &slot);
	libevdev_enable_event_code(evdev, EV_ABS, ABS_MT_TRACKING_ID, &tid);

	list_take_append(&plugin->devices, pd, link);
}

static void
mtdev_plugin_device_removed(struct libinput_plugin *libinput_plugin,
			    struct libinput_device *device)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *dev;
	list_for_each_safe(dev, &plugin->devices, link) {
		if (dev->device == device) {
			plugin_device_destroy(dev);
			return;
		}
	}
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = plugin_destroy,
	.device_new = mtdev_plugin_device_new,
	.device_ignored = mtdev_plugin_device_removed,
	.device_added = NULL,
	.device_removed = mtdev_plugin_device_removed,
	.evdev_frame = mtdev_plugin_evdev_frame,
};

void
libinput_mtdev_plugin(struct libinput *libinput)
{
	_destroy_(plugin_data) *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "mtdev", &interface, steal(&plugin));
}
