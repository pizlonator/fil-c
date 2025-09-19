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
#include "libinput-plugin-tablet-eraser-button.h"
#include "libinput-plugin.h"
#include "libinput-util.h"

static int ERASER_BUTTON_DELAY = 30 * 1000; /* µs */

enum frame_filter_state {
	DISCARD,
	PROCESS,
};

enum eraser_button_state {
	ERASER_BUTTON_NEUTRAL,
	ERASER_BUTTON_PEN_PENDING_ERASER,
	ERASER_BUTTON_BUTTON_HELD_DOWN,
	ERASER_BUTTON_BUTTON_RELEASED,
};

enum eraser_button_event {
	ERASER_EVENT_PEN_ENTERING_PROX,
	ERASER_EVENT_PEN_LEAVING_PROX,
	ERASER_EVENT_ERASER_ENTERING_PROX,
	ERASER_EVENT_ERASER_LEAVING_PROX,
	ERASER_EVENT_TIMEOUT,
};

static const char *
eraser_button_state_str(enum eraser_button_state state)
{
	switch (state) {
	CASE_RETURN_STRING(ERASER_BUTTON_NEUTRAL);
	CASE_RETURN_STRING(ERASER_BUTTON_PEN_PENDING_ERASER);
	CASE_RETURN_STRING(ERASER_BUTTON_BUTTON_HELD_DOWN);
	CASE_RETURN_STRING(ERASER_BUTTON_BUTTON_RELEASED);
	}
	abort();
}

static const char *
eraser_button_event_str(enum eraser_button_event event)
{
	switch (event) {
	CASE_RETURN_STRING(ERASER_EVENT_PEN_ENTERING_PROX);
	CASE_RETURN_STRING(ERASER_EVENT_PEN_LEAVING_PROX);
	CASE_RETURN_STRING(ERASER_EVENT_ERASER_ENTERING_PROX);
	CASE_RETURN_STRING(ERASER_EVENT_ERASER_LEAVING_PROX);
	CASE_RETURN_STRING(ERASER_EVENT_TIMEOUT);
	}
	abort();
}

struct plugin_device {
	struct list link;
	struct plugin_data *parent;
	struct libinput_device *device;

	bool pen_in_prox;
	bool eraser_in_prox;

	struct evdev_frame *last_frame;

	enum libinput_config_eraser_button_mode mode;
	/* The evdev code of the button to send */
	evdev_usage_t button;
	struct libinput_plugin_timer *timer;
	enum eraser_button_state state;
};

struct plugin_data {
	struct libinput_plugin *plugin;
	struct list devices;
};

static void
plugin_device_destroy(struct plugin_device *device)
{
	libinput_plugin_timer_cancel(device->timer);
	libinput_plugin_timer_unref(device->timer);
	libinput_device_unref(device->device);
	evdev_frame_unref(device->last_frame);
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
eraser_button_set_state(struct plugin_device *device, enum eraser_button_state to)
{
	enum eraser_button_state *state = &device->state;

	*state = to;
}

static void
eraser_button_set_timer(struct plugin_device *device, uint64_t time)
{
	libinput_plugin_timer_set(device->timer, time + ERASER_BUTTON_DELAY);
}

static void
eraser_button_cancel_timer(struct plugin_device *device)
{
	libinput_plugin_timer_cancel(device->timer);
}

static void
eraser_button_state_bug(struct plugin_device *device, enum eraser_button_event event)
{
	plugin_log_bug(device->parent->plugin,
		       "Invalid eraser button event %s in state %s\n",
		       eraser_button_event_str(event),
		       eraser_button_state_str(device->state));
}

enum tool_filter {
	SKIP_PEN = bit(1),
	SKIP_ERASER = bit(2),
	PEN_IN_PROX = bit(3),
	PEN_OUT_OF_PROX = bit(4),
	ERASER_IN_PROX = bit(5),
	ERASER_OUT_OF_PROX = bit(6),
	BUTTON_DOWN = bit(7),
	BUTTON_UP = bit(8),
	SKIP_BTN_TOUCH = bit(9),
};

static void
eraser_button_insert_frame(struct plugin_device *device,
			   struct evdev_frame *frame_in,
			   enum tool_filter filter,
			   evdev_usage_t *button)
{
	size_t nevents;
	const struct evdev_event *events = evdev_frame_get_events(frame_in, &nevents);

	/* +2 because we may add BTN_TOOL_PEN and BTN_TOOL_RUBBER */
	_unref_(evdev_frame) *frame_out = evdev_frame_new(nevents + 2);

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event event = events[i];
		switch (evdev_usage_enum(event.usage)) {
		case EVDEV_BTN_TOOL_PEN:
		case EVDEV_BTN_TOOL_RUBBER:
			/* filter */
			break;
		case EVDEV_BTN_TOUCH:
			if (!(filter & SKIP_BTN_TOUCH))
				evdev_frame_append(frame_out, &event, 1);
			break;
		default:
			if (button == NULL || evdev_usage_cmp(event.usage, *button))
				evdev_frame_append(frame_out, &event, 1);
			break;
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
	if (filter & (BUTTON_UP | BUTTON_DOWN)) {
		assert(button != NULL);
		evdev_frame_append_one(frame_out,
				       *button,
				       (filter & BUTTON_DOWN) ? 1 : 0);
	}

	evdev_frame_set_time(frame_out, evdev_frame_get_time(frame_in));
	libinput_plugin_prepend_evdev_frame(device->parent->plugin,
					    device->device,
					    frame_out);
}

static enum frame_filter_state
eraser_button_neutral_handle_event(struct plugin_device *device,
				   struct evdev_frame *frame,
				   enum eraser_button_event event,
				   uint64_t time)
{
	switch (event) {
	case ERASER_EVENT_PEN_ENTERING_PROX:
		break;
	case ERASER_EVENT_PEN_LEAVING_PROX:
		eraser_button_set_timer(device, time);
		eraser_button_set_state(device, ERASER_BUTTON_PEN_PENDING_ERASER);
		return DISCARD; /* Discard this event, it has garbage data anyway */
	case ERASER_EVENT_ERASER_ENTERING_PROX:
		/* Change eraser prox in into pen prox in + button down */
		eraser_button_insert_frame(device,
					   frame,
					   PEN_IN_PROX | SKIP_ERASER | BUTTON_DOWN,
					   &device->button);
		eraser_button_set_state(device, ERASER_BUTTON_BUTTON_HELD_DOWN);
		return DISCARD;
	case ERASER_EVENT_ERASER_LEAVING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_TIMEOUT:
		break;
	}

	return PROCESS;
}

static enum frame_filter_state
eraser_button_pending_eraser_handle_event(struct plugin_device *device,
					  struct evdev_frame *frame,
					  enum eraser_button_event event,
					  uint64_t time)
{
	switch (event) {
	case ERASER_EVENT_PEN_ENTERING_PROX:
		eraser_button_cancel_timer(device);
		eraser_button_set_state(device, ERASER_BUTTON_NEUTRAL);
		/* We just papered over a quick prox out/in here */
		break;
	case ERASER_EVENT_PEN_LEAVING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_ERASER_ENTERING_PROX:
		eraser_button_cancel_timer(device);
		eraser_button_insert_frame(device,
					   frame,
					   SKIP_ERASER | SKIP_PEN | BUTTON_DOWN,
					   &device->button);
		eraser_button_set_state(device, ERASER_BUTTON_BUTTON_HELD_DOWN);
		return DISCARD;
	case ERASER_EVENT_ERASER_LEAVING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_TIMEOUT:
		/* Pen went out of prox and we delayed expecting an eraser to
		 * come in prox. That didn't happen -> pen prox out */
		eraser_button_set_state(device, ERASER_BUTTON_NEUTRAL);
		eraser_button_insert_frame(device,
					   frame,
					   SKIP_ERASER | PEN_OUT_OF_PROX,
					   NULL);
		break;
	}

	return PROCESS;
}

static enum frame_filter_state
eraser_button_button_held_handle_event(struct plugin_device *device,
				       struct evdev_frame *frame,
				       enum eraser_button_event event,
				       uint64_t time)
{
	switch (event) {
	case ERASER_EVENT_PEN_ENTERING_PROX:
	case ERASER_EVENT_PEN_LEAVING_PROX:
		/* We should've seen an eraser out-of-prox out here */
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_ERASER_ENTERING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_ERASER_LEAVING_PROX:
		eraser_button_insert_frame(device,
					   device->last_frame,
					   SKIP_ERASER | SKIP_PEN | BUTTON_UP,
					   &device->button);
		eraser_button_set_state(device, ERASER_BUTTON_BUTTON_RELEASED);
		eraser_button_set_timer(device, time);
		return DISCARD; /* Discard the actual frame, it has garbage data anyway
				 */
	case ERASER_EVENT_TIMEOUT:
		/* Expected to be cancelled in previous state */
		eraser_button_state_bug(device, event);
		break;
	}

	return PROCESS;
}

static enum frame_filter_state
eraser_button_button_released_handle_event(struct plugin_device *device,
					   struct evdev_frame *frame,
					   enum eraser_button_event event,
					   uint64_t time)
{
	switch (event) {
	case ERASER_EVENT_PEN_ENTERING_PROX:
		eraser_button_cancel_timer(device);
		eraser_button_insert_frame(device, frame, SKIP_PEN | SKIP_ERASER, NULL);
		eraser_button_set_state(device, ERASER_BUTTON_NEUTRAL);
		return DISCARD;
	case ERASER_EVENT_PEN_LEAVING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_ERASER_ENTERING_PROX:
		break;
	case ERASER_EVENT_ERASER_LEAVING_PROX:
		eraser_button_state_bug(device, event);
		break;
	case ERASER_EVENT_TIMEOUT:
		/* Eraser went out of prox, we expected the pen to come back in prox but
		 * that didn't happen. We still have the pen simulated in-prox  -> pen
		 * prox out.
		 * We release the button first, then send the pen out-of-prox
		 * event sequence. This way the sequence of tip first/button first is
		 * predictable.
		 */
		eraser_button_insert_frame(device,
					   frame,
					   SKIP_PEN | SKIP_ERASER | BUTTON_UP,
					   &device->button);
		eraser_button_insert_frame(device, frame, PEN_OUT_OF_PROX, NULL);
		eraser_button_set_state(device, ERASER_BUTTON_NEUTRAL);
		break;
	}

	return PROCESS;
}

static enum frame_filter_state
eraser_button_handle_state(struct plugin_device *device,
			   struct evdev_frame *frame,
			   enum eraser_button_event event,
			   uint64_t time)
{
	enum eraser_button_state state = device->state;
	enum frame_filter_state ret = PROCESS;

	switch (state) {
	case ERASER_BUTTON_NEUTRAL:
		ret = eraser_button_neutral_handle_event(device, frame, event, time);
		break;
	case ERASER_BUTTON_PEN_PENDING_ERASER:
		ret = eraser_button_pending_eraser_handle_event(device,
								frame,
								event,
								time);
		break;
	case ERASER_BUTTON_BUTTON_HELD_DOWN:
		ret = eraser_button_button_held_handle_event(device,
							     frame,
							     event,
							     time);
		break;
	case ERASER_BUTTON_BUTTON_RELEASED:
		ret = eraser_button_button_released_handle_event(device,
								 frame,
								 event,
								 time);
		break;
	}

	if (state != device->state) {
		plugin_log_debug(device->parent->plugin,
				 "eraser button: state %s -> %s -> %s\n",
				 eraser_button_state_str(state),
				 eraser_button_event_str(event),
				 eraser_button_state_str(device->state));
	}
	return ret;
}

/**
 * Physical eraser button handling: if the physical eraser button is
 * disabled paper over any pen prox out/eraser prox in events and send a button event
 * instead.
 */
static void
eraser_button_handle_frame(struct plugin_device *device,
			   struct evdev_frame *frame,
			   uint64_t time)
{
	if (device->mode == LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT)
		return;

	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	bool pen_toggled = false;
	bool eraser_toggled = false;

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *event = &events[i];

		switch (evdev_usage_enum(event->usage)) {
		case EVDEV_BTN_TOOL_PEN:
			pen_toggled = true;
			device->pen_in_prox = !!event->value;
			break;
		case EVDEV_BTN_TOOL_RUBBER:
			eraser_toggled = true;
			device->eraser_in_prox = !!event->value;
			break;
		default:
			break;
		}
	}

	bool eraser_in_prox = device->eraser_in_prox;
	bool pen_in_prox = device->pen_in_prox;

	enum eraser_button_event eraser_event =
		eraser_in_prox ? ERASER_EVENT_ERASER_ENTERING_PROX
			       : ERASER_EVENT_ERASER_LEAVING_PROX;
	enum eraser_button_event pen_event = pen_in_prox
						     ? ERASER_EVENT_PEN_ENTERING_PROX
						     : ERASER_EVENT_PEN_LEAVING_PROX;

	enum frame_filter_state ret = PROCESS;

	/* bit awkward because we definitely want whatever goes out of prox to
	 * be handled first but if one sends discard and the other one process?
	 * Unclear...
	 */
	if (eraser_toggled && pen_toggled) {
		if (pen_in_prox) {
			eraser_button_handle_state(device, frame, eraser_event, time);
			ret = eraser_button_handle_state(device,
							 frame,
							 pen_event,
							 time);
		} else {
			eraser_button_handle_state(device, frame, pen_event, time);
			ret = eraser_button_handle_state(device,
							 frame,
							 eraser_event,
							 time);
		}
	} else if (eraser_toggled) {
		ret = eraser_button_handle_state(device, frame, eraser_event, time);
	} else if (pen_toggled) {
		ret = eraser_button_handle_state(device, frame, pen_event, time);
	}

	if (ret == PROCESS) {
		evdev_frame_reset(device->last_frame);
		evdev_frame_append(device->last_frame, events, nevents);
	} else if (ret == DISCARD) {
		evdev_frame_reset(frame);
	}
}

static void
eraser_button_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
				 struct libinput_device *device,
				 struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;
	uint64_t time = evdev_frame_get_time(frame);

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			eraser_button_handle_frame(pd, frame, time);
			break;
		}
	}
}

static void
eraser_button_timer_func(struct libinput_plugin *plugin, uint64_t now, void *d)
{
	struct plugin_device *device = d;

	if (!device->last_frame) {
		plugin_log_bug(
			device->parent->plugin,
			"Eraser button timer fired without a frame in state %s\n",
			eraser_button_state_str(device->state));
		return;
	}
	eraser_button_handle_state(device,
				   device->last_frame,
				   ERASER_EVENT_TIMEOUT,
				   now);
}

static void
eraser_button_plugin_device_added(struct libinput_plugin *libinput_plugin,
				  struct libinput_device *device)
{
	if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_TOOL))
		return;

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd = zalloc(sizeof(*pd));
	pd->device = libinput_device_ref(device);
	pd->parent = plugin;
	pd->last_frame = evdev_frame_new(64);
	pd->timer = libinput_plugin_timer_new(libinput_plugin,
					      libinput_device_get_sysname(device),
					      eraser_button_timer_func,
					      pd);

	list_take_append(&plugin->devices, pd, link);
}

static void
eraser_button_plugin_device_removed(struct libinput_plugin *libinput_plugin,
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
eraser_button_plugin_tool_configured(struct libinput_plugin *libinput_plugin,
				     struct libinput_tablet_tool *tool)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;
	list_for_each(pd, &plugin->devices, link) {
		/* FIXME: sigh, we need a separate list of tools? */
		pd->mode = libinput_tablet_tool_config_eraser_button_get_mode(tool);
		uint32_t button =
			libinput_tablet_tool_config_eraser_button_get_button(tool);

		pd->button = evdev_usage_from_code(EV_KEY, button);
	}
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = plugin_destroy,
	.device_new = NULL,
	.device_ignored = NULL,
	.device_added = eraser_button_plugin_device_added,
	.device_removed = eraser_button_plugin_device_removed,
	.evdev_frame = eraser_button_plugin_evdev_frame,
	.tool_configured = eraser_button_plugin_tool_configured,
};

void
libinput_tablet_plugin_eraser_button(struct libinput *libinput)
{
	if (getenv("LIBINPUT_RUNNING_TEST_SUITE"))
		ERASER_BUTTON_DELAY = ms2us(150);

	_destroy_(plugin_data) *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "tablet-eraser-button", &interface, NULL);
	plugin->plugin = p;
	libinput_plugin_set_user_data(p, steal(&plugin));
}
