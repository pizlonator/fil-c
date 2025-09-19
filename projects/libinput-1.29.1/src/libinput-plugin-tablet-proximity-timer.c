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
#include "libinput-plugin-tablet-proximity-timer.h"
#include "libinput-plugin.h"
#include "libinput-util.h"
#include "timer.h"

/* The tablet sends events every ~2ms , 50ms should be plenty enough to
   detect out-of-range.
   This value is higher during test suite runs */
static int FORCED_PROXOUT_TIMEOUT = 50 * 1000; /* µs */

struct plugin_device {
	struct list link;

	struct libinput_plugin_timer *prox_out_timer;
	bool proximity_out_forced;
	uint64_t last_event_time;

	bool pen_state;
	bitmask_t button_state;

	struct libinput_device *device;
	struct plugin_data *parent;
};

static void
plugin_device_destroy(void *d)
{
	struct plugin_device *device = d;

	list_remove(&device->link);
	libinput_plugin_timer_cancel(device->prox_out_timer);
	libinput_plugin_timer_unref(device->prox_out_timer);
	libinput_device_unref(device->device);

	free(device);
}

struct plugin_data {
	struct list devices;
	struct libinput_plugin *plugin;
};

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

static inline void
proximity_timer_plugin_set_timer(struct plugin_device *device, uint64_t time)
{
	libinput_plugin_timer_set(device->prox_out_timer,
				  time + FORCED_PROXOUT_TIMEOUT);
}

static void
tablet_proximity_out_quirk_timer_func(struct libinput_plugin *plugin,
				      uint64_t now,
				      void *data)
{
	struct plugin_device *device = data;

	if (!bitmask_is_empty(device->button_state)) {
		proximity_timer_plugin_set_timer(device, now);
		return;
	}

	if (device->last_event_time > now - FORCED_PROXOUT_TIMEOUT) {
		proximity_timer_plugin_set_timer(device, device->last_event_time);
		return;
	}

	plugin_log_debug(device->parent->plugin,
			 "%s: forcing proximity out after timeout\n",
			 libinput_device_get_name(device->device));

	_unref_(evdev_frame) *prox_out_frame = evdev_frame_new(2);
	evdev_frame_append_one(prox_out_frame, evdev_usage_from(EVDEV_BTN_TOOL_PEN), 0);
	evdev_frame_set_time(prox_out_frame, now);

	libinput_plugin_prepend_evdev_frame(device->parent->plugin,
					    device->device,
					    prox_out_frame);

	device->proximity_out_forced = true;
}

/*
 * Handling for the proximity out workaround. Some tablets only send
 * BTN_TOOL_PEN on the very first event, then leave it set even when the pen
 * leaves the detectable range. To libinput this looks like we always have
 * the pen in proximity.
 *
 * To avoid this, we set a timer on BTN_TOOL_PEN in. We expect the tablet to
 * continuously send events, and while it's doing so we keep updating the
 * timer. Once we go Xms without an event we assume proximity out and inject
 * a BTN_TOOL_PEN 0 event into the sequence through the timer func.
 *
 * On the next axis event after a prox out we enforce
 * BTN_TOOL_PEN 1 to force proximity in.
 */
static void
proximity_timer_plugin_device_handle_frame(struct libinput_plugin *libinput_plugin,
					   struct plugin_device *device,
					   struct evdev_frame *frame)
{
	uint64_t time = evdev_frame_get_time(frame);
	/* First event after adding a device - by definition the pen
	 *
	 * is in proximity if we get this one */
	if (device->last_event_time == 0)
		proximity_timer_plugin_set_timer(device, time);

	device->last_event_time = time;

	bool pen_toggled = false;

	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);
	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *event = &events[i];

		/* The proximity timeout is only needed for BTN_TOOL_PEN, devices
		 * that require it don't do erasers */
		switch (evdev_usage_enum(event->usage)) {
		case EVDEV_BTN_STYLUS:
		case EVDEV_BTN_STYLUS2:
		case EVDEV_BTN_STYLUS3:
		case EVDEV_BTN_TOUCH:
			if (event->value)
				bitmask_set_bit(&device->button_state,
						evdev_event_code(event) - BTN_STYLUS3);
			else
				bitmask_clear_bit(&device->button_state,
						  evdev_event_code(event) -
							  BTN_STYLUS3);
			break;
		case EVDEV_BTN_TOOL_PEN:
			pen_toggled = true;
			device->pen_state = event->value == 1;
			break;
		case EVDEV_BTN_TOOL_RUBBER:
		case EVDEV_BTN_TOOL_BRUSH:
		case EVDEV_BTN_TOOL_PENCIL:
		case EVDEV_BTN_TOOL_AIRBRUSH:
		case EVDEV_BTN_TOOL_FINGER:
		case EVDEV_BTN_TOOL_MOUSE:
		case EVDEV_BTN_TOOL_LENS:
			plugin_device_destroy(device);
			return;
		default:
			break;
		}
	}

	if (pen_toggled) {
		if (device->pen_state) {
			proximity_timer_plugin_set_timer(device, time);
		} else {
			/* If we get a BTN_TOOL_PEN 0 it means the tablet will
			 * give us the right events after all and we can disable
			 * our timer-based proximity out.
			 */
			libinput_plugin_timer_cancel(device->prox_out_timer);
			plugin_log_debug(libinput_plugin,
					 "%s: proximity out timer unloaded\n",
					 libinput_device_get_name(device->device));
			plugin_device_destroy(device);
			return;
		}
	} else if (device->proximity_out_forced) {
		plugin_log_debug(libinput_plugin,
				 "%s: forcing proximity in\n",
				 libinput_device_get_name(device->device));
		evdev_frame_append_one(frame, evdev_usage_from(EVDEV_BTN_TOOL_PEN), 1);
		device->proximity_out_forced = false;
		proximity_timer_plugin_set_timer(device, time);
	}
}

static void
proximity_timer_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
				   struct libinput_device *device,
				   struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			proximity_timer_plugin_device_handle_frame(libinput_plugin,
								   pd,
								   frame);
			break;
		}
	}
}

static void
proximity_timer_plugin_device_added(struct libinput_plugin *libinput_plugin,
				    struct libinput_device *device)
{
	if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
		return;

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd = zalloc(sizeof(*pd));
	pd->device = libinput_device_ref(device);
	pd->parent = plugin;
	pd->prox_out_timer =
		libinput_plugin_timer_new(libinput_plugin,
					  libinput_device_get_sysname(device),
					  tablet_proximity_out_quirk_timer_func,
					  pd);

	list_take_append(&plugin->devices, pd, link);
}

static void
proximity_timer_plugin_device_removed(struct libinput_plugin *libinput_plugin,
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

static void
plugin_destroy(struct libinput_plugin *libinput_plugin)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	plugin_data_destroy(plugin);
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = plugin_destroy,
	.device_new = NULL,
	.device_ignored = NULL,
	.device_added = proximity_timer_plugin_device_added,
	.device_removed = proximity_timer_plugin_device_removed,
	.evdev_frame = proximity_timer_plugin_evdev_frame,
};

void
libinput_tablet_plugin_proximity_timer(struct libinput *libinput)
{
	struct plugin_data *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	/* Stop false positives caused by the forced proximity code */
	if (getenv("LIBINPUT_RUNNING_TEST_SUITE"))
		FORCED_PROXOUT_TIMEOUT = 150 * 1000; /* µs */

	_unref_(libinput_plugin) *p = libinput_plugin_new(libinput,
							  "tablet-proximity-timer",
							  &interface,
							  plugin);
	plugin->plugin = p;
}
