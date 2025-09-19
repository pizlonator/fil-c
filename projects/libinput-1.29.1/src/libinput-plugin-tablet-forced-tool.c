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
#include <libevdev/libevdev.h>

#include "util-mem.h"
#include "util-strings.h"

#include "evdev-frame.h"
#include "libinput-log.h"
#include "libinput-plugin-tablet-forced-tool.h"
#include "libinput-plugin.h"
#include "libinput-util.h"

/*
 * Handling for tools that never set BTN_TOOL_PEN.
 */

enum tools {
	PEN = 0,
	RUBBER,
	BRUSH,
	PENCIL,
	AIRBRUSH,
	MOUSE,
	LENS,
};

struct plugin_device {
	struct list link;
	struct libinput_device *device;
	bitmask_t tool_state;
};

struct plugin_data {
	struct list devices;
};

static void
plugin_device_destroy(struct plugin_device *device)
{
	libinput_device_unref(device->device);
	list_remove(&device->link);
	free(device);
}

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
forced_tool_plugin_device_handle_frame(struct libinput_plugin *libinput_plugin,
				       struct plugin_device *device,
				       struct evdev_frame *frame)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	bool axis_change = false;

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *event = &events[i];
		switch (evdev_usage_enum(event->usage)) {
		case EVDEV_BTN_TOOL_PEN:
		case EVDEV_BTN_TOOL_RUBBER:
		case EVDEV_BTN_TOOL_BRUSH:
		case EVDEV_BTN_TOOL_PENCIL:
		case EVDEV_BTN_TOOL_AIRBRUSH:
		case EVDEV_BTN_TOOL_MOUSE:
		case EVDEV_BTN_TOOL_LENS:
			if (event->value == 1) {
				bitmask_set_bit(&device->tool_state,
						evdev_event_code(event) - BTN_TOOL_PEN);
			} else {
				bitmask_clear_bit(&device->tool_state,
						  evdev_event_code(event) -
							  BTN_TOOL_PEN);
			}
			return; /* Nothing to do */
		case EVDEV_ABS_X:
		case EVDEV_ABS_Y:
		case EVDEV_ABS_Z: /* rotation */
		/* not ABS_DISTANCE! */
		case EVDEV_ABS_PRESSURE:
		case EVDEV_ABS_TILT_X:
		case EVDEV_ABS_TILT_Y:
		case EVDEV_ABS_WHEEL: /* slider */
			/* no early return here, the BTN_TOOL updates
			 * may come after the ABS_ events */
			axis_change = true;
			break;
		case EVDEV_REL_WHEEL:
			/* no early return here, the BTN_TOOL updates
			 * may come after the REL_ events */
			axis_change = true;
			break;
		default:
			break;
		}
	}

	if (!axis_change)
		return;

	const bitmask_t all_tools =
		bitmask_from_bits(PEN, RUBBER, BRUSH, PENCIL, AIRBRUSH, MOUSE, LENS);
	if (bitmask_any(device->tool_state, all_tools))
		return;

	/* We need to force a BTN_TOOL_PEN if we get an axis event (i.e.
	 * stylus is def. in proximity). We don't do this for pure
	 * button events because we discard those.
	 */
	evdev_frame_append_one(frame,
			       evdev_usage_from(EVDEV_BTN_TOOL_PEN),
			       1); /* libinput's event frame will have space */
}

static void
forced_tool_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			       struct libinput_device *device,
			       struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			forced_tool_plugin_device_handle_frame(libinput_plugin,
							       pd,
							       frame);
			break;
		}
	}
}

static void
forced_tool_plugin_device_added(struct libinput_plugin *libinput_plugin,
				struct libinput_device *device)
{
	if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
		return;

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd = zalloc(sizeof(*pd));
	pd->device = libinput_device_ref(device);
	list_take_append(&plugin->devices, pd, link);
}

static void
forced_tool_plugin_device_removed(struct libinput_plugin *libinput_plugin,
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
	.device_new = NULL,
	.device_ignored = NULL,
	.device_added = forced_tool_plugin_device_added,
	.device_removed = forced_tool_plugin_device_removed,
	.evdev_frame = forced_tool_plugin_evdev_frame,
};

void
libinput_tablet_plugin_forced_tool(struct libinput *libinput)
{
	_destroy_(plugin_data) *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p = libinput_plugin_new(libinput,
							  "tablet-forced-tool",
							  &interface,
							  steal(&plugin));
}
