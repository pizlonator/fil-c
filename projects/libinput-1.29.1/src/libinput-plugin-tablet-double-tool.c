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
#include "libinput-plugin-tablet-double-tool.h"
#include "libinput-plugin.h"
#include "libinput-util.h"

enum {
	TOOL_PEN_DOWN,
	TOOL_PEN_UP,
	TOOL_ERASER_DOWN,
	TOOL_ERASER_UP,
	TOOL_DOUBLE_TOOL,
};

enum tool_filter {
	SKIP_PEN = bit(1),
	SKIP_ERASER = bit(2),
	PEN_IN_PROX = bit(3),
	PEN_OUT_OF_PROX = bit(4),
	ERASER_IN_PROX = bit(5),
	ERASER_OUT_OF_PROX = bit(6),
};

struct plugin_device {
	struct list link;
	struct libinput_device *device;
	bool ignore_pen;
	bitmask_t tools_seen;

	int pen_value;
	int eraser_value;
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

static struct evdev_frame *
double_tool_plugin_filter_frame(struct libinput_plugin *plugin,
				struct evdev_frame *frame_in,
				enum tool_filter filter)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame_in, &nevents);

	/* +2 because we may add BTN_TOOL_PEN and BTN_TOOL_RUBBER */
	struct evdev_frame *frame_out = evdev_frame_new(nevents + 2);
	evdev_frame_set_time(frame_out, evdev_frame_get_time(frame_in));

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *event = &events[i];

		switch (evdev_usage_enum(event->usage)) {
		case EVDEV_BTN_TOOL_PEN:
		case EVDEV_BTN_TOOL_RUBBER:
			/* skip */
			break;
		default:
			evdev_frame_append(frame_out, event, 1);
		}
	}

	if (filter & (PEN_IN_PROX | PEN_OUT_OF_PROX)) {
		evdev_frame_append_one(frame_out,
				       evdev_usage_from(EVDEV_BTN_TOOL_PEN),
				       (filter & PEN_IN_PROX) ? 1 : 0);
	}
	if (filter & (ERASER_IN_PROX | ERASER_OUT_OF_PROX)) {
		evdev_frame_append_one(frame_out,
				       evdev_usage_from(EVDEV_BTN_TOOL_RUBBER),
				       (filter & ERASER_IN_PROX) ? 1 : 0);
	}

	return frame_out;
}

/* Kernel tools are supposed to be mutually exclusive, but we may have
 * two bits set due to firmware/kernel bugs.
 * Two cases that have been seen in the wild:
 * - BTN_TOOL_PEN on proximity in, followed by
 *   BTN_TOOL_RUBBER later, see #259
 *   -> We force a prox-out of the pen, trigger prox-in for eraser
 * - BTN_TOOL_RUBBER on proximity in, but BTN_TOOL_PEN when
 *   the tip is down, see #702.
 *   -> We ignore BTN_TOOL_PEN
 * In both cases the eraser is what we want, so we bias
 * towards that.
 */
static void
double_tool_plugin_device_handle_frame(struct libinput_plugin *libinput_plugin,
				       struct plugin_device *device,
				       struct evdev_frame *frame)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	const struct evdev_event *eraser_toggle = NULL;
	const struct evdev_event *pen_toggle = NULL;

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *event = &events[i];

		switch (evdev_usage_enum(event->usage)) {
		case EVDEV_BTN_TOOL_RUBBER:
			eraser_toggle = event;
			device->eraser_value = event->value;
			break;
		case EVDEV_BTN_TOOL_PEN:
			pen_toggle = event;
			device->pen_value = event->value;
			break;
		default:
			break;
		}
	}

	bool eraser_is_down = !!device->eraser_value;
	bool pen_is_down = !!device->pen_value;
	bool eraser_toggled = eraser_toggle != NULL;
	bool pen_toggled = pen_toggle != NULL;

#ifdef EVENT_DEBUGGING
	plugin_log_debug(libinput_plugin,
			 "device %s: tool state: pen:%s eraser:%s\n",
			 libinput_device_get_name(device->device),
			 pen_toggled   ? (pen_is_down ? "↓" : "↑")
			 : pen_is_down ? "|"
				       : ".",
			 eraser_toggled   ? (eraser_is_down ? "↓" : "↑")
			 : eraser_is_down ? "|"
					  : ".");
#endif

	if (!bitmask_bit_is_set(device->tools_seen, TOOL_DOUBLE_TOOL)) {
		bitmask_t tool_mask = bitmask_from_bits(TOOL_PEN_DOWN,
							TOOL_PEN_UP,
							TOOL_ERASER_DOWN,
							TOOL_ERASER_UP);
		if (eraser_toggled) {
			if (eraser_is_down)
				bitmask_set_bit(&device->tools_seen, TOOL_ERASER_DOWN);
			else
				bitmask_set_bit(&device->tools_seen, TOOL_ERASER_UP);
		}
		if (pen_toggled) {
			if (pen_is_down)
				bitmask_set_bit(&device->tools_seen, TOOL_PEN_DOWN);
			else
				bitmask_set_bit(&device->tools_seen, TOOL_PEN_UP);
		}
		/* If we successfully get all four tool events without
		 * a doubled-up tool, assume the device is sane and
		 * unregister this device */
		if (bitmask_all(device->tools_seen, tool_mask)) {
			plugin_log_debug(
				libinput_plugin,
				"device %s: device is fine, unregistering device\n",
				libinput_device_get_name(device->device));
			plugin_device_destroy(device);
			return;
		}
	}

	/* rubber after pen */
	if (eraser_toggled) {
		if (eraser_is_down && pen_is_down) {
			if (!pen_toggled) {
				_unref_(evdev_frame) *pen_out_of_prox =
					double_tool_plugin_filter_frame(
						libinput_plugin,
						frame,
						SKIP_ERASER | PEN_OUT_OF_PROX);
				libinput_plugin_prepend_evdev_frame(libinput_plugin,
								    device->device,
								    pen_out_of_prox);
			}

			_unref_(evdev_frame) *eraser_in_prox =
				double_tool_plugin_filter_frame(libinput_plugin,
								frame,
								SKIP_PEN |
									ERASER_IN_PROX);

			libinput_plugin_prepend_evdev_frame(libinput_plugin,
							    device->device,
							    eraser_in_prox);
			device->ignore_pen = true;

			bitmask_set_bit(&device->tools_seen, TOOL_DOUBLE_TOOL);

			/* discard the original frame */
			evdev_frame_reset(frame);

			return;
		} else if (!eraser_is_down) {
			_unref_(evdev_frame) *eraser_out_of_prox =
				double_tool_plugin_filter_frame(
					libinput_plugin,
					frame,
					SKIP_PEN | ERASER_OUT_OF_PROX);

			libinput_plugin_prepend_evdev_frame(libinput_plugin,
							    device->device,
							    eraser_out_of_prox);

			/* Only revert back to the pen if the pen was actually toggled
			 * in this frame, otherwise it's just still set from before */
			if (pen_toggled && pen_is_down) {
				_unref_(evdev_frame) *pen_in_prox =
					double_tool_plugin_filter_frame(
						libinput_plugin,
						frame,
						SKIP_ERASER | PEN_IN_PROX);
				libinput_plugin_prepend_evdev_frame(libinput_plugin,
								    device->device,
								    pen_in_prox);
			}

			device->ignore_pen = false;

			/* discard the original frame */
			evdev_frame_reset(frame);

			return;
		}
	}

	/* pen after rubber */
	if (pen_toggled && eraser_is_down) {
		device->ignore_pen = true;
	}

	if (device->ignore_pen) {
		_unref_(evdev_frame) *frame_out =
			double_tool_plugin_filter_frame(libinput_plugin,
							frame,
							SKIP_PEN);
		size_t out_nevents;
		evdev_frame_set(frame,
				evdev_frame_get_events(frame_out, &out_nevents),
				nevents);
		bitmask_set_bit(&device->tools_seen, TOOL_DOUBLE_TOOL);
	} else if (pen_is_down) {
		_unref_(evdev_frame) *frame_out =
			double_tool_plugin_filter_frame(libinput_plugin,
							frame,
							PEN_IN_PROX);
		size_t out_nevents;
		evdev_frame_set(frame,
				evdev_frame_get_events(frame_out, &out_nevents),
				nevents);
	}
}

static void
double_tool_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			       struct libinput_device *device,
			       struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			double_tool_plugin_device_handle_frame(libinput_plugin,
							       pd,
							       frame);
			break;
		}
	}
}

static void
double_tool_plugin_device_added(struct libinput_plugin *libinput_plugin,
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
double_tool_plugin_device_removed(struct libinput_plugin *libinput_plugin,
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
	.device_added = double_tool_plugin_device_added,
	.device_removed = double_tool_plugin_device_removed,
	.evdev_frame = double_tool_plugin_evdev_frame,
};

void
libinput_tablet_plugin_double_tool(struct libinput *libinput)
{
	_destroy_(plugin_data) *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p = libinput_plugin_new(libinput,
							  "tablet-double-tool",
							  &interface,
							  steal(&plugin));
}
