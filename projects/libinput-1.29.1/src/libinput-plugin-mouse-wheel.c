/*
 * Copyright © 2010 Intel Corporation
 * Copyright © 2013 Jonas Ådahl
 * Copyright © 2013-2017 Red Hat, Inc.
 * Copyright © 2017 James Ye <jye836@gmail.com>
 * Copyright © 2021-2025 José Expósito
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

#include "evdev-fallback.h"
#include "evdev.h"
#include "libinput-log.h"
#include "libinput-plugin-mouse-wheel.h"
#include "libinput-plugin.h"
#include "libinput-util.h"

#define ACC_V120_TRIGGER_THRESHOLD 30  /* 1/4 of a wheel detent */
#define ACC_V120_THRESHOLD 47 /* Good for both high-ish multipliers (8/120) and the rest of the mice (30/120, 40/120, etc) */
#define WHEEL_SCROLL_TIMEOUT ms2us(500)

enum wheel_state {
	WHEEL_STATE_NONE,
	WHEEL_STATE_ACCUMULATING_SCROLL,
	WHEEL_STATE_SCROLLING,
};

enum wheel_direction {
	WHEEL_DIR_UNKNOW,
	WHEEL_DIR_VPOS,
	WHEEL_DIR_VNEG,
	WHEEL_DIR_HPOS,
	WHEEL_DIR_HNEG,
};

enum wheel_event {
	WHEEL_EVENT_SCROLL_ACCUMULATED,
	WHEEL_EVENT_SCROLL,
	WHEEL_EVENT_SCROLL_TIMEOUT,
	WHEEL_EVENT_SCROLL_DIR_CHANGED,
};

enum ignore_strategy {
	MAYBE,             /* use heuristics but don't yet accumulate */
	PASSTHROUGH,       /* do not accumulate, pass through */
	ACCUMULATE,        /* accumulate scroll wheel events */
	ALWAYS_ACCUMULATE, /* always accumulate wheel events */
};

struct plugin_device {
	struct list link;
	struct plugin_data *parent;
	struct libinput_device *device;

	enum wheel_state state;
	struct device_coords lo_res;
	struct device_coords hi_res;
	bool hi_res_event_received;
	struct libinput_plugin_timer *scroll_timer;
	enum wheel_direction dir;
	enum ignore_strategy ignore_small_hi_res_movements;
	int min_movement;

	struct ratelimit hires_warning_limit;
};

struct plugin_data {
	struct libinput_plugin *plugin;
	struct list devices;
};

static inline const char *
wheel_state_to_str(enum wheel_state state)
{
	switch (state) {
	CASE_RETURN_STRING(WHEEL_STATE_NONE);
	CASE_RETURN_STRING(WHEEL_STATE_ACCUMULATING_SCROLL);
	CASE_RETURN_STRING(WHEEL_STATE_SCROLLING);
	}
	return NULL;
}

static inline const char *
wheel_event_to_str(enum wheel_event event)
{
	switch (event) {
	CASE_RETURN_STRING(WHEEL_EVENT_SCROLL_ACCUMULATED);
	CASE_RETURN_STRING(WHEEL_EVENT_SCROLL);
	CASE_RETURN_STRING(WHEEL_EVENT_SCROLL_TIMEOUT);
	CASE_RETURN_STRING(WHEEL_EVENT_SCROLL_DIR_CHANGED);
	}
	return NULL;
}

static inline void
log_wheel_bug(struct plugin_device *pd, enum wheel_event event)
{
	plugin_log_bug_libinput(pd->parent->plugin,
				"invalid wheel event %s in state %s\n",
				wheel_event_to_str(event),
				wheel_state_to_str(pd->state));
}

static inline void
wheel_set_scroll_timer(struct plugin_device *pd, uint64_t time)
{
	if (!pd->scroll_timer)
		return;

	libinput_plugin_timer_set(pd->scroll_timer, time + WHEEL_SCROLL_TIMEOUT);
}

static inline void
wheel_cancel_scroll_timer(struct plugin_device *pd)
{
	if (!pd->scroll_timer)
		return;

	libinput_plugin_timer_cancel(pd->scroll_timer);
}

static void
wheel_handle_event_on_state_none(struct plugin_device *pd,
				 enum wheel_event event,
				 uint64_t time)
{
	switch (event) {
	case WHEEL_EVENT_SCROLL:
		switch (pd->ignore_small_hi_res_movements) {
		case ACCUMULATE:
		case ALWAYS_ACCUMULATE:
			pd->state = WHEEL_STATE_ACCUMULATING_SCROLL;
			break;
		case PASSTHROUGH:
		case MAYBE:
			pd->state = WHEEL_STATE_SCROLLING;
			break;
		}
		break;
	case WHEEL_EVENT_SCROLL_DIR_CHANGED:
		break;
	case WHEEL_EVENT_SCROLL_ACCUMULATED:
	case WHEEL_EVENT_SCROLL_TIMEOUT:
		log_wheel_bug(pd, event);
		break;
	}
}

static void
wheel_handle_event_on_state_accumulating_scroll(struct plugin_device *pd,
						enum wheel_event event,
						uint64_t time)
{
	switch (event) {
	case WHEEL_EVENT_SCROLL_ACCUMULATED:
		pd->state = WHEEL_STATE_SCROLLING;
		wheel_set_scroll_timer(pd, time);
		break;
	case WHEEL_EVENT_SCROLL:
		/* Ignore scroll while accumulating deltas */
		break;
	case WHEEL_EVENT_SCROLL_DIR_CHANGED:
		pd->state = WHEEL_STATE_NONE;
		break;
	case WHEEL_EVENT_SCROLL_TIMEOUT:
		log_wheel_bug(pd, event);
		break;
	}
}

static void
wheel_handle_event_on_state_scrolling(struct plugin_device *pd,
				      enum wheel_event event,
				      uint64_t time)
{
	switch (event) {
	case WHEEL_EVENT_SCROLL:
		wheel_set_scroll_timer(pd, time);
		break;
	case WHEEL_EVENT_SCROLL_TIMEOUT:
		pd->state = WHEEL_STATE_NONE;
		break;
	case WHEEL_EVENT_SCROLL_DIR_CHANGED:
		wheel_cancel_scroll_timer(pd);
		pd->state = WHEEL_STATE_NONE;
		break;
	case WHEEL_EVENT_SCROLL_ACCUMULATED:
		log_wheel_bug(pd, event);
		break;
	}
}

static void
wheel_handle_event(struct plugin_device *pd, enum wheel_event event, uint64_t time)
{
	enum wheel_state oldstate = pd->state;

	switch (oldstate) {
	case WHEEL_STATE_NONE:
		wheel_handle_event_on_state_none(pd, event, time);
		break;
	case WHEEL_STATE_ACCUMULATING_SCROLL:
		wheel_handle_event_on_state_accumulating_scroll(pd, event, time);
		break;
	case WHEEL_STATE_SCROLLING:
		wheel_handle_event_on_state_scrolling(pd, event, time);
		break;
	}

	if (oldstate != pd->state) {
		plugin_log_debug(pd->parent->plugin,
				 "wheel: %s → %s → %s\n",
				 wheel_state_to_str(oldstate),
				 wheel_event_to_str(event),
				 wheel_state_to_str(pd->state));
	}
}

static void
wheel_remove_scroll_events(struct evdev_frame *frame)
{
	size_t nevents;
	_unref_(evdev_frame) *copy = evdev_frame_clone(frame);
	struct evdev_event *events = evdev_frame_get_events(copy, &nevents);

	evdev_frame_reset(frame);

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *e = &events[i];

		switch (evdev_usage_enum(e->usage)) {
		case EVDEV_REL_WHEEL:
		case EVDEV_REL_WHEEL_HI_RES:
		case EVDEV_REL_HWHEEL:
		case EVDEV_REL_HWHEEL_HI_RES:
			/* Do not append scroll events */
			break;
		default:
			evdev_frame_append(frame, e, 1);
			break;
		}
	}
}

static void
wheel_queue_scroll_events(struct plugin_device *pd, struct evdev_frame *frame)
{
	if (pd->hi_res.y != 0) {
		evdev_frame_append_one(frame,
				       evdev_usage_from(EVDEV_REL_WHEEL_HI_RES),
				       pd->hi_res.y);
		pd->hi_res.y = 0;
	}

	if (pd->lo_res.y != 0) {
		evdev_frame_append_one(frame,
				       evdev_usage_from(EVDEV_REL_WHEEL),
				       pd->lo_res.y);
		pd->lo_res.y = 0;
	}

	if (pd->hi_res.x != 0) {
		evdev_frame_append_one(frame,
				       evdev_usage_from(EVDEV_REL_HWHEEL_HI_RES),
				       pd->hi_res.x);
		pd->hi_res.x = 0;
	}

	if (pd->lo_res.x != 0) {
		evdev_frame_append_one(frame,
				       evdev_usage_from(EVDEV_REL_HWHEEL),
				       pd->lo_res.x);
		pd->lo_res.x = 0;
	}
}

static void
wheel_handle_state_none(struct plugin_device *pd,
			struct evdev_frame *frame,
			uint64_t time)
{
}

static void
wheel_handle_state_accumulating_scroll(struct plugin_device *pd,
				       struct evdev_frame *frame,
				       uint64_t time)
{
	wheel_remove_scroll_events(frame);

	if (abs(pd->hi_res.x) > pd->min_movement ||
	    abs(pd->hi_res.y) > pd->min_movement) {
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL_ACCUMULATED, time);
		wheel_queue_scroll_events(pd, frame);
	}
}

static void
wheel_handle_state_scrolling(struct plugin_device *pd,
			     struct evdev_frame *frame,
			     uint64_t time)
{
	wheel_remove_scroll_events(frame);
	wheel_queue_scroll_events(pd, frame);
}

static void
wheel_handle_direction_change(struct plugin_device *pd,
			      struct evdev_event *e,
			      uint64_t time)
{
	enum wheel_direction new_dir = WHEEL_DIR_UNKNOW;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_WHEEL_HI_RES:
		new_dir = (e->value > 0) ? WHEEL_DIR_VPOS : WHEEL_DIR_VNEG;
		break;
	case EVDEV_REL_HWHEEL_HI_RES:
		new_dir = (e->value > 0) ? WHEEL_DIR_HPOS : WHEEL_DIR_HNEG;
		break;
	default:
		return;
	}

	if (new_dir != WHEEL_DIR_UNKNOW && new_dir != pd->dir) {
		pd->dir = new_dir;
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL_DIR_CHANGED, time);
	}
}

static inline void
wheel_update_strategy(struct plugin_device *pd, int32_t value)
{
	if (pd->ignore_small_hi_res_movements != ALWAYS_ACCUMULATE) {
		pd->min_movement = min(pd->min_movement, abs(value));

		/* Only if a wheel sends movements less than the trigger threshold
		 * activate the accumulation and debouncing of scroll directions, etc.
		 */
		if (pd->ignore_small_hi_res_movements == MAYBE &&
		    pd->min_movement < ACC_V120_TRIGGER_THRESHOLD)
			pd->ignore_small_hi_res_movements = ACCUMULATE;
	}
}

static void
wheel_process_relative(struct plugin_device *pd, struct evdev_event *e, uint64_t time)
{
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_WHEEL:
		pd->lo_res.y += e->value;
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL, time);
		break;
	case EVDEV_REL_HWHEEL:
		pd->lo_res.x += e->value;
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL, time);
		break;
	case EVDEV_REL_WHEEL_HI_RES:
		pd->hi_res.y += e->value;
		pd->hi_res_event_received = true;
		wheel_update_strategy(pd, e->value);
		wheel_handle_direction_change(pd, e, time);
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL, time);
		break;
	case EVDEV_REL_HWHEEL_HI_RES:
		pd->hi_res.x += e->value;
		pd->hi_res_event_received = true;
		wheel_update_strategy(pd, e->value);
		wheel_handle_direction_change(pd, e, time);
		wheel_handle_event(pd, WHEEL_EVENT_SCROLL, time);
		break;
	default:
		break;
	}
}

static void
wheel_handle_state(struct plugin_device *pd, struct evdev_frame *frame, uint64_t time)
{
	struct evdev_device *evdev = evdev_device(pd->device);

	if (!pd->hi_res_event_received && (pd->lo_res.x != 0 || pd->lo_res.y != 0)) {
		evdev_log_bug_kernel_ratelimit(
			evdev,
			&pd->hires_warning_limit,
			"device supports high-resolution scroll but only low-resolution events have been received.\n"
			"See %s/incorrectly-enabled-hires.html for details\n",
			HTTP_DOC_LINK);
		pd->hi_res.x = pd->lo_res.x * 120;
		pd->hi_res.y = pd->lo_res.y * 120;
	}

	switch (pd->state) {
	case WHEEL_STATE_NONE:
		wheel_handle_state_none(pd, frame, time);
		break;
	case WHEEL_STATE_ACCUMULATING_SCROLL:
		wheel_handle_state_accumulating_scroll(pd, frame, time);
		break;
	case WHEEL_STATE_SCROLLING:
		wheel_handle_state_scrolling(pd, frame, time);
		break;
	}
}

static void
wheel_on_scroll_timer_timeout(struct libinput_plugin *plugin, uint64_t now, void *data)
{
	struct plugin_device *pd = data;

	wheel_handle_event(pd, WHEEL_EVENT_SCROLL_TIMEOUT, now);
}

static struct plugin_device *
wheel_plugin_device_create(struct libinput_plugin *libinput_plugin,
			   struct plugin_data *plugin,
			   struct libinput_device *device)
{
	struct evdev_device *evdev = evdev_device(device);
	struct plugin_device *pd = zalloc(sizeof(*pd));

	pd->parent = plugin;
	pd->device = libinput_device_ref(device);
	pd->state = WHEEL_STATE_NONE;
	pd->dir = WHEEL_DIR_UNKNOW;
	pd->min_movement = ACC_V120_THRESHOLD;
	ratelimit_init(&pd->hires_warning_limit, s2us(24 * 60 * 60), 1);

	if (evdev_device_is_virtual(evdev))
		pd->ignore_small_hi_res_movements = PASSTHROUGH;
	else if (libinput_device_has_model_quirk(device,
						 QUIRK_MODEL_LOGITECH_MX_MASTER_3))
		pd->ignore_small_hi_res_movements = ALWAYS_ACCUMULATE;
	else
		pd->ignore_small_hi_res_movements = MAYBE;

	if (pd->ignore_small_hi_res_movements != PASSTHROUGH) {
		pd->scroll_timer =
			libinput_plugin_timer_new(libinput_plugin,
						  libinput_device_get_sysname(device),
						  wheel_on_scroll_timer_timeout,
						  pd);
	}

	return pd;
}

static void
wheel_plugin_device_destroy(struct plugin_device *pd)
{
	list_remove(&pd->link);

	if (pd->scroll_timer) {
		wheel_cancel_scroll_timer(pd);
		libinput_plugin_timer_unref(pd->scroll_timer);
	}

	libinput_device_unref(pd->device);

	free(pd);
}

static void
wheel_plugin_destroy(struct libinput_plugin *libinput_plugin)
{
	struct plugin_data *data = libinput_plugin_get_user_data(libinput_plugin);

	struct plugin_device *pd;
	list_for_each_safe(pd, &data->devices, link) {
		wheel_plugin_device_destroy(pd);
	}

	free(data);
}

static void
wheel_plugin_device_new(struct libinput_plugin *libinput_plugin,
			struct libinput_device *device,
			struct libevdev *libevdev,
			struct udev_device *udev_device)
{
	if (!libevdev_has_event_code(libevdev, EV_REL, REL_WHEEL_HI_RES) &&
	    !libevdev_has_event_code(libevdev, EV_REL, REL_HWHEEL_HI_RES))
		return;

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd =
		wheel_plugin_device_create(libinput_plugin, plugin, device);
	list_take_append(&plugin->devices, pd, link);
}

static void
wheel_plugin_device_added(struct libinput_plugin *libinput_plugin,
			  struct libinput_device *device)
{
	if (libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER))
		return;

	/* For any non-pointer device: check if we happened to have added
	 * it during device_new and if so, remove it. We only want to enable
	 * this on devices that have a wheel *and* are a pointer device */
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each_safe(pd, &plugin->devices, link) {
		if (pd->device == device) {
			wheel_plugin_device_destroy(pd);
			return;
		}
	}
}

static void
wheel_plugin_device_removed(struct libinput_plugin *libinput_plugin,
			    struct libinput_device *device)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each_safe(pd, &plugin->devices, link) {
		if (pd->device == device) {
			wheel_plugin_device_destroy(pd);
			return;
		}
	}
}

static void
wheel_handle_frame(struct plugin_device *pd, struct evdev_frame *frame, uint64_t time)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *e = &events[i];
		uint16_t type = evdev_event_type(e);

		switch (type) {
		case EV_REL:
			wheel_process_relative(pd, e, time);
			break;
		case EV_SYN:
			wheel_handle_state(pd, frame, time);
			break;
		}
	}
}

static void
wheel_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			 struct libinput_device *device,
			 struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;
	uint64_t time = evdev_frame_get_time(frame);

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			wheel_handle_frame(pd, frame, time);
			break;
		}
	}
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = wheel_plugin_destroy,
	.device_new = wheel_plugin_device_new,
	.device_ignored = wheel_plugin_device_removed,
	.device_added = wheel_plugin_device_added,
	.device_removed = wheel_plugin_device_removed,
	.evdev_frame = wheel_plugin_evdev_frame,
};

void
libinput_mouse_plugin_wheel(struct libinput *libinput)
{
	struct plugin_data *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "mouse-wheel", &interface, plugin);
	plugin->plugin = p;
}
