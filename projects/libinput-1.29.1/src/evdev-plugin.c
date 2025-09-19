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

#include "util-macros.h"
#include "util-mem.h"

#include "evdev-plugin.h"
#include "evdev.h"

static inline void
evdev_process_frame(struct evdev_device *device,
		    struct evdev_frame *frame,
		    uint64_t time)
{
	struct evdev_dispatch *dispatch = device->dispatch;

	libinput_timer_flush(evdev_libinput_context(device), time);

	dispatch->interface->process(dispatch, device, frame, time);
}

static inline void
evdev_device_dispatch_frame(struct libinput_plugin *plugin,
			    struct libinput_device *libinput_device,
			    struct evdev_frame *frame)
{
	struct evdev_device *device = evdev_device(libinput_device);
	uint64_t time = evdev_frame_get_time(frame);
	evdev_process_frame(device, frame, time);

	/* Discard event to make the plugin system aware we're done */
	evdev_frame_reset(frame);
}

static void
evdev_plugin_device_added(struct libinput_plugin *plugin,
			  struct libinput_device *device)

{
	libinput_plugin_enable_device_event_frame(plugin, device, true);
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = NULL,
	.device_new = NULL,
	.device_ignored = NULL,
	.device_added = evdev_plugin_device_added,
	.device_removed = NULL,
	.evdev_frame = evdev_device_dispatch_frame,
};

void
libinput_evdev_dispatch_plugin(struct libinput *libinput)
{
	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "evdev", &interface, NULL);
}
