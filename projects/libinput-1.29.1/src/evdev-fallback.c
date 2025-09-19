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

#include "util-input-event.h"

#include "evdev-fallback.h"

static void
fallback_keyboard_notify_key(struct fallback_dispatch *dispatch,
			     struct evdev_device *device,
			     uint64_t time,
			     evdev_usage_t usage,
			     enum libinput_key_state state)
{
	int down_count;

	down_count = evdev_update_key_down_count(device, usage, state);

	if ((state == LIBINPUT_KEY_STATE_PRESSED && down_count == 1) ||
	    (state == LIBINPUT_KEY_STATE_RELEASED && down_count == 0)) {
		keyboard_notify_key(&device->base,
				    time,
				    keycode_from_usage(usage),
				    state);
	}
}

static void
fallback_lid_notify_toggle(struct fallback_dispatch *dispatch,
			   struct evdev_device *device,
			   uint64_t time)
{
	if (dispatch->lid.is_closed ^ dispatch->lid.is_closed_client_state) {
		switch_notify_toggle(&device->base,
				     time,
				     LIBINPUT_SWITCH_LID,
				     dispatch->lid.is_closed);
		dispatch->lid.is_closed_client_state = dispatch->lid.is_closed;
	}
}

void
fallback_notify_physical_button(struct fallback_dispatch *dispatch,
				struct evdev_device *device,
				uint64_t time,
				evdev_usage_t button,
				enum libinput_button_state state)
{
	evdev_pointer_notify_physical_button(device, time, button, state);
}

static enum libinput_switch_state
fallback_interface_get_switch_state(struct evdev_dispatch *evdev_dispatch,
				    enum libinput_switch sw)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);

	switch (sw) {
	case LIBINPUT_SWITCH_TABLET_MODE:
		break;
	default:
		/* Internal function only, so we can abort here */
		abort();
	}

	return dispatch->tablet_mode.sw.state ? LIBINPUT_SWITCH_STATE_ON
					      : LIBINPUT_SWITCH_STATE_OFF;
}

static inline bool
post_button_scroll(struct evdev_device *device,
		   struct device_float_coords raw,
		   uint64_t time)
{
	if (device->scroll.method != LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
		return false;

	switch (device->scroll.button_scroll_state) {
	case BUTTONSCROLL_IDLE:
		return false;
	case BUTTONSCROLL_BUTTON_DOWN:
		/* if the button is down but scroll is not active, we're within the
		   timeout where we swallow motion events but don't post
		   scroll buttons */
		evdev_log_debug(device, "btnscroll: discarding\n");
		return true;
	case BUTTONSCROLL_READY:
		device->scroll.button_scroll_state = BUTTONSCROLL_SCROLLING;
		_fallthrough_;
	case BUTTONSCROLL_SCROLLING: {
		const struct normalized_coords normalized =
			filter_dispatch_scroll(device->pointer.filter,
					       &raw,
					       device,
					       time);
		evdev_post_scroll(device,
				  time,
				  LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS,
				  &normalized);
	}
		return true;
	}

	assert(!"invalid scroll button state");
}

static inline bool
fallback_filter_defuzz_touch(struct fallback_dispatch *dispatch,
			     struct evdev_device *device,
			     struct mt_slot *slot)
{
	struct device_coords point;

	if (!dispatch->mt.want_hysteresis)
		return false;

	point = evdev_hysteresis(&slot->point,
				 &slot->hysteresis_center,
				 &dispatch->mt.hysteresis_margin);
	slot->point = point;

	if (point.x == slot->hysteresis_center.x &&
	    point.y == slot->hysteresis_center.y)
		return true;

	slot->hysteresis_center = point;

	return false;
}

static inline struct device_float_coords
fallback_rotate_relative(struct fallback_dispatch *dispatch,
			 struct evdev_device *device)
{
	struct device_float_coords rel = { dispatch->rel.x, dispatch->rel.y };

	if (!device->base.config.rotation)
		return rel;

	matrix_mult_vec_double(&dispatch->rotation.matrix, &rel.x, &rel.y);

	return rel;
}

static void
fallback_flush_relative_motion(struct fallback_dispatch *dispatch,
			       struct evdev_device *device,
			       uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct normalized_coords accel;

	if (!(device->seat_caps & EVDEV_DEVICE_POINTER))
		return;

	struct device_float_coords raw = fallback_rotate_relative(dispatch, device);

	dispatch->rel.x = 0;
	dispatch->rel.y = 0;

	/* Use unaccelerated deltas for pointing stick scroll */
	if (post_button_scroll(device, raw, time))
		return;

	if (device->pointer.filter) {
		/* Apply pointer acceleration. */
		accel = filter_dispatch(device->pointer.filter, &raw, device, time);
	} else {
		evdev_log_bug_libinput(device, "accel filter missing\n");
		accel.x = accel.y = 0;
	}

	if (normalized_is_zero(accel))
		return;

	pointer_notify_motion(base, time, &accel, &raw);
}

static void
fallback_flush_wheels(struct fallback_dispatch *dispatch,
		      struct evdev_device *device,
		      uint64_t time)
{
	struct normalized_coords wheel_degrees = { 0.0, 0.0 };
	struct discrete_coords discrete = { 0.0, 0.0 };
	struct wheel_v120 v120 = { 0.0, 0.0 };

	if (!libinput_device_has_capability(&device->base, LIBINPUT_DEVICE_CAP_POINTER))
		return;

	/* This mouse has a trackstick instead of a mouse wheel and sends
	 * trackstick data via REL_WHEEL. Normalize it like normal x/y coordinates.
	 */
	if (device->model_flags & EVDEV_MODEL_LENOVO_SCROLLPOINT) {
		const struct device_float_coords raw = {
			.x = dispatch->wheel.lo_res.x,
			.y = dispatch->wheel.lo_res.y * -1,
		};
		const struct normalized_coords normalized =
			filter_dispatch_scroll(device->pointer.filter,
					       &raw,
					       device,
					       time);
		evdev_post_scroll(device,
				  time,
				  LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS,
				  &normalized);
		dispatch->wheel.hi_res.x = 0;
		dispatch->wheel.hi_res.y = 0;
		dispatch->wheel.lo_res.x = 0;
		dispatch->wheel.lo_res.y = 0;

		return;
	}

	if (dispatch->wheel.hi_res.y != 0) {
		int value = dispatch->wheel.hi_res.y;

		v120.y = -1 * value;
		wheel_degrees.y =
			-1 * value / 120.0 * device->scroll.wheel_click_angle.y;
		evdev_notify_axis_wheel(device,
					time,
					bit(LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL),
					&wheel_degrees,
					&v120);
		dispatch->wheel.hi_res.y = 0;
	}

	if (dispatch->wheel.lo_res.y != 0) {
		int value = dispatch->wheel.lo_res.y;

		wheel_degrees.y = -1 * value * device->scroll.wheel_click_angle.y;
		discrete.y = -1 * value;
		evdev_notify_axis_legacy_wheel(
			device,
			time,
			bit(LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL),
			&wheel_degrees,
			&discrete);
		dispatch->wheel.lo_res.y = 0;
	}

	if (dispatch->wheel.hi_res.x != 0) {
		int value = dispatch->wheel.hi_res.x;

		v120.x = value;
		wheel_degrees.x = value / 120.0 * device->scroll.wheel_click_angle.x;
		evdev_notify_axis_wheel(device,
					time,
					bit(LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL),
					&wheel_degrees,
					&v120);
		dispatch->wheel.hi_res.x = 0;
	}

	if (dispatch->wheel.lo_res.x != 0) {
		int value = dispatch->wheel.lo_res.x;

		wheel_degrees.x = value * device->scroll.wheel_click_angle.x;
		discrete.x = value;
		evdev_notify_axis_legacy_wheel(
			device,
			time,
			bit(LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL),
			&wheel_degrees,
			&discrete);
		dispatch->wheel.lo_res.x = 0;
	}
}

static void
fallback_flush_absolute_motion(struct fallback_dispatch *dispatch,
			       struct evdev_device *device,
			       uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct device_coords point;

	if (!(device->seat_caps & EVDEV_DEVICE_POINTER))
		return;

	point = dispatch->abs.point;
	evdev_transform_absolute(device, &point);

	pointer_notify_motion_absolute(base, time, &point);
}

static bool
fallback_flush_mt_down(struct fallback_dispatch *dispatch,
		       struct evdev_device *device,
		       int slot_idx,
		       uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	struct device_coords point;
	struct mt_slot *slot;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	slot = &dispatch->mt.slots[slot_idx];
	if (slot->seat_slot != -1) {
		evdev_log_bug_kernel(
			device,
			"driver sent multiple touch down for the same slot");
		return false;
	}

	seat_slot = ffs(~seat->slot_map) - 1;
	slot->seat_slot = seat_slot;

	if (seat_slot == -1)
		return false;

	seat->slot_map |= bit(seat_slot);
	point = slot->point;
	slot->hysteresis_center = point;
	evdev_transform_absolute(device, &point);

	touch_notify_touch_down(base, time, slot_idx, seat_slot, &point);

	return true;
}

static bool
fallback_flush_mt_motion(struct fallback_dispatch *dispatch,
			 struct evdev_device *device,
			 int slot_idx,
			 uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct device_coords point;
	struct mt_slot *slot;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	slot = &dispatch->mt.slots[slot_idx];
	seat_slot = slot->seat_slot;
	point = slot->point;

	if (seat_slot == -1)
		return false;

	if (fallback_filter_defuzz_touch(dispatch, device, slot))
		return false;

	evdev_transform_absolute(device, &point);
	touch_notify_touch_motion(base, time, slot_idx, seat_slot, &point);

	return true;
}

static bool
fallback_flush_mt_up(struct fallback_dispatch *dispatch,
		     struct evdev_device *device,
		     int slot_idx,
		     uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	struct mt_slot *slot;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	slot = &dispatch->mt.slots[slot_idx];
	seat_slot = slot->seat_slot;
	slot->seat_slot = -1;

	if (seat_slot == -1)
		return false;

	seat->slot_map &= ~bit(seat_slot);

	touch_notify_touch_up(base, time, slot_idx, seat_slot);

	return true;
}

static bool
fallback_flush_mt_cancel(struct fallback_dispatch *dispatch,
			 struct evdev_device *device,
			 int slot_idx,
			 uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	struct mt_slot *slot;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	slot = &dispatch->mt.slots[slot_idx];
	seat_slot = slot->seat_slot;
	slot->seat_slot = -1;

	if (seat_slot == -1)
		return false;

	seat->slot_map &= ~bit(seat_slot);

	touch_notify_touch_cancel(base, time, slot_idx, seat_slot);

	return true;
}

static bool
fallback_flush_st_down(struct fallback_dispatch *dispatch,
		       struct evdev_device *device,
		       uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	struct device_coords point;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	if (dispatch->abs.seat_slot != -1) {
		evdev_log_bug_kernel(
			device,
			"driver sent multiple touch down for the same slot");
		return false;
	}

	seat_slot = ffs(~seat->slot_map) - 1;
	dispatch->abs.seat_slot = seat_slot;

	if (seat_slot == -1)
		return false;

	seat->slot_map |= bit(seat_slot);

	point = dispatch->abs.point;
	evdev_transform_absolute(device, &point);

	touch_notify_touch_down(base, time, -1, seat_slot, &point);

	return true;
}

static bool
fallback_flush_st_motion(struct fallback_dispatch *dispatch,
			 struct evdev_device *device,
			 uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct device_coords point;
	int seat_slot;

	point = dispatch->abs.point;
	evdev_transform_absolute(device, &point);

	seat_slot = dispatch->abs.seat_slot;

	if (seat_slot == -1)
		return false;

	touch_notify_touch_motion(base, time, -1, seat_slot, &point);

	return true;
}

static bool
fallback_flush_st_up(struct fallback_dispatch *dispatch,
		     struct evdev_device *device,
		     uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	seat_slot = dispatch->abs.seat_slot;
	dispatch->abs.seat_slot = -1;

	if (seat_slot == -1)
		return false;

	seat->slot_map &= ~bit(seat_slot);

	touch_notify_touch_up(base, time, -1, seat_slot);

	return true;
}

static bool
fallback_flush_st_cancel(struct fallback_dispatch *dispatch,
			 struct evdev_device *device,
			 uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_seat *seat = base->seat;
	int seat_slot;

	if (!(device->seat_caps & EVDEV_DEVICE_TOUCH))
		return false;

	seat_slot = dispatch->abs.seat_slot;
	dispatch->abs.seat_slot = -1;

	if (seat_slot == -1)
		return false;

	seat->slot_map &= ~bit(seat_slot);

	touch_notify_touch_cancel(base, time, -1, seat_slot);

	return true;
}

static void
fallback_process_touch_button(struct fallback_dispatch *dispatch,
			      struct evdev_device *device,
			      uint64_t time,
			      int value)
{
	dispatch->pending_event |=
		(value) ? EVDEV_ABSOLUTE_TOUCH_DOWN : EVDEV_ABSOLUTE_TOUCH_UP;
}

static inline void
fallback_process_key(struct fallback_dispatch *dispatch,
		     struct evdev_device *device,
		     struct evdev_event *e,
		     uint64_t time)
{
	/* ignore kernel key repeat */
	if (e->value == 2)
		return;

	if (evdev_usage_eq(e->usage, EVDEV_BTN_TOUCH)) {
		if (!device->is_mt)
			fallback_process_touch_button(dispatch, device, time, e->value);
		return;
	}

	bool is_button = evdev_usage_is_button(e->usage);
	bool is_key = evdev_usage_is_key(e->usage);

	/* Ignore key release events from the kernel for keys that libinput
	 * never got a pressed event for or key presses for keys that we
	 * think are still down */
	if (is_button || is_key) {
		if ((e->value && hw_is_key_down(dispatch, e->usage)) ||
		    (e->value == 0 && !hw_is_key_down(dispatch, e->usage)))
			return;

		dispatch->pending_event |= EVDEV_KEY;
	}

	hw_set_key_down(dispatch, e->usage, e->value);

	if (is_key) {
		fallback_keyboard_notify_key(dispatch,
					     device,
					     time,
					     e->usage,
					     e->value ? LIBINPUT_KEY_STATE_PRESSED
						      : LIBINPUT_KEY_STATE_RELEASED);
	}
}

static void
fallback_process_touch(struct fallback_dispatch *dispatch,
		       struct evdev_device *device,
		       struct evdev_event *e,
		       uint64_t time)
{
	struct mt_slot *slot = &dispatch->mt.slots[dispatch->mt.slot];

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_ABS_MT_SLOT:
		if ((size_t)e->value >= dispatch->mt.slots_len) {
			evdev_log_bug_libinput(device,
					       "exceeded slot count (%d of max %zd)\n",
					       e->value,
					       dispatch->mt.slots_len);
			e->value = dispatch->mt.slots_len - 1;
		}
		dispatch->mt.slot = e->value;
		return;
	case EVDEV_ABS_MT_TRACKING_ID:
		if (e->value >= 0) {
			dispatch->pending_event |= EVDEV_ABSOLUTE_MT;
			slot->state = SLOT_STATE_BEGIN;
			if (dispatch->mt.has_palm) {
				int v;
				v = libevdev_get_slot_value(device->evdev,
							    dispatch->mt.slot,
							    ABS_MT_TOOL_TYPE);
				switch (v) {
				case MT_TOOL_PALM:
					/* new touch, no cancel needed */
					slot->palm_state = PALM_WAS_PALM;
					break;
				default:
					slot->palm_state = PALM_NONE;
					break;
				}
			} else {
				slot->palm_state = PALM_NONE;
			}
		} else {
			dispatch->pending_event |= EVDEV_ABSOLUTE_MT;
			slot->state = SLOT_STATE_END;
		}
		slot->dirty = true;
		break;
	case EVDEV_ABS_MT_POSITION_X:
		evdev_device_check_abs_axis_range(device, e->usage, e->value);
		dispatch->mt.slots[dispatch->mt.slot].point.x = e->value;
		dispatch->pending_event |= EVDEV_ABSOLUTE_MT;
		slot->dirty = true;
		break;
	case EVDEV_ABS_MT_POSITION_Y:
		evdev_device_check_abs_axis_range(device, e->usage, e->value);
		dispatch->mt.slots[dispatch->mt.slot].point.y = e->value;
		dispatch->pending_event |= EVDEV_ABSOLUTE_MT;
		slot->dirty = true;
		break;
	case EVDEV_ABS_MT_TOOL_TYPE:
		/* The transitions matter - we (may) need to send a touch
		 * cancel event if we just switched to a palm touch. And the
		 * kernel may switch back to finger but we keep the touch as
		 * palm - but then we need to reset correctly on a new touch
		 * sequence.
		 */
		switch (e->value) {
		case MT_TOOL_PALM:
			if (slot->palm_state == PALM_NONE)
				slot->palm_state = PALM_NEW;
			break;
		default:
			if (slot->palm_state == PALM_IS_PALM)
				slot->palm_state = PALM_WAS_PALM;
			break;
		}
		dispatch->pending_event |= EVDEV_ABSOLUTE_MT;
		slot->dirty = true;
		break;
	default:
		break;
	}
}

static inline void
fallback_process_absolute_motion(struct fallback_dispatch *dispatch,
				 struct evdev_device *device,
				 struct evdev_event *e)
{
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_ABS_X:
		evdev_device_check_abs_axis_range(device, e->usage, e->value);
		dispatch->abs.point.x = e->value;
		dispatch->pending_event |= EVDEV_ABSOLUTE_MOTION;
		break;
	case EVDEV_ABS_Y:
		evdev_device_check_abs_axis_range(device, e->usage, e->value);
		dispatch->abs.point.y = e->value;
		dispatch->pending_event |= EVDEV_ABSOLUTE_MOTION;
		break;
	default:
		break;
	}
}

static void
fallback_lid_keyboard_event(uint64_t time, struct libinput_event *event, void *data)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(data);

	if (!dispatch->lid.is_closed)
		return;

	if (event->type != LIBINPUT_EVENT_KEYBOARD_KEY)
		return;

	if (dispatch->lid.reliability == RELIABILITY_WRITE_OPEN) {
		int fd = libevdev_get_fd(dispatch->device->evdev);
		int rc;
		struct input_event ev[2];

		ev[0] = input_event_init(0, EV_SW, SW_LID, 0);
		ev[1] = input_event_init(0, EV_SYN, SYN_REPORT, 0);

		rc = write(fd, ev, sizeof(ev));

		if (rc < 0)
			evdev_log_error(dispatch->device,
					"failed to write SW_LID state (%s)",
					strerror(errno));

		/* In case write() fails, we sync the lid state manually
		 * regardless. */
	}

	/* Posting the event here means we preempt the keyboard events that
	 * caused us to wake up, so the lid event is always passed on before
	 * the key event.
	 */
	dispatch->lid.is_closed = false;
	fallback_lid_notify_toggle(dispatch, dispatch->device, time);
}

static void
fallback_lid_toggle_keyboard_listener(struct fallback_dispatch *dispatch,
				      struct evdev_paired_keyboard *kbd,
				      bool is_closed)
{
	assert(kbd->device);

	libinput_device_remove_event_listener(&kbd->listener);

	if (is_closed) {
		libinput_device_add_event_listener(&kbd->device->base,
						   &kbd->listener,
						   fallback_lid_keyboard_event,
						   dispatch);
	} else {
		libinput_device_init_event_listener(&kbd->listener);
	}
}

static void
fallback_lid_toggle_keyboard_listeners(struct fallback_dispatch *dispatch,
				       bool is_closed)
{
	struct evdev_paired_keyboard *kbd;

	list_for_each(kbd, &dispatch->lid.paired_keyboard_list, link) {
		if (!kbd->device)
			continue;

		fallback_lid_toggle_keyboard_listener(dispatch, kbd, is_closed);
	}
}

static inline void
fallback_process_switch(struct fallback_dispatch *dispatch,
			struct evdev_device *device,
			struct evdev_event *e,
			uint64_t time)
{
	enum libinput_switch_state state;
	bool is_closed;

	/* TODO: this should to move to handle_state */

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_SW_LID:
		is_closed = !!e->value;

		fallback_lid_toggle_keyboard_listeners(dispatch, is_closed);

		if (dispatch->lid.is_closed == is_closed)
			return;

		dispatch->lid.is_closed = is_closed;
		fallback_lid_notify_toggle(dispatch, device, time);
		break;
	case EVDEV_SW_TABLET_MODE:
		if (dispatch->tablet_mode.sw.state == e->value)
			return;

		dispatch->tablet_mode.sw.state = e->value;
		if (e->value)
			state = LIBINPUT_SWITCH_STATE_ON;
		else
			state = LIBINPUT_SWITCH_STATE_OFF;
		switch_notify_toggle(&device->base,
				     time,
				     LIBINPUT_SWITCH_TABLET_MODE,
				     state);
		break;
	default:
		break;
	}
}

static inline bool
fallback_reject_relative(struct evdev_device *device,
			 const struct evdev_event *e,
			 uint64_t time)
{
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_X:
	case EVDEV_REL_Y:
		if ((device->seat_caps & EVDEV_DEVICE_POINTER) == 0) {
			evdev_log_bug_libinput_ratelimit(
				device,
				&device->nonpointer_rel_limit,
				"REL_X/Y from a non-pointer device\n");
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

static void
fallback_rotate_wheel(struct fallback_dispatch *dispatch, struct evdev_event *e)
{
	/* Special case: if we're upside down (-ish),
	 * swap the direction of the wheels so that user-down
	 * means scroll down. This isn't done for any other angle
	 * since it's not clear what the heuristics should be.*/
	if (dispatch->rotation.angle >= 160.0 && dispatch->rotation.angle <= 220.0) {
		e->value *= -1;
	}
}

static inline void
fallback_process_relative(struct fallback_dispatch *dispatch,
			  struct evdev_device *device,
			  struct evdev_event *e,
			  uint64_t time)
{
	if (fallback_reject_relative(device, e, time))
		return;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_X:
		dispatch->rel.x += e->value;
		dispatch->pending_event |= EVDEV_RELATIVE_MOTION;
		break;
	case EVDEV_REL_Y:
		dispatch->rel.y += e->value;
		dispatch->pending_event |= EVDEV_RELATIVE_MOTION;
		break;
	case EVDEV_REL_WHEEL:
		fallback_rotate_wheel(dispatch, e);
		dispatch->wheel.lo_res.y += e->value;
		break;
	case EVDEV_REL_HWHEEL:
		fallback_rotate_wheel(dispatch, e);
		dispatch->wheel.lo_res.x += e->value;
		break;
	case EVDEV_REL_WHEEL_HI_RES:
		fallback_rotate_wheel(dispatch, e);
		dispatch->wheel.hi_res.y += e->value;
		break;
	case EVDEV_REL_HWHEEL_HI_RES:
		fallback_rotate_wheel(dispatch, e);
		dispatch->wheel.hi_res.x += e->value;
		break;
	default:
		break;
	}
}

static inline void
fallback_process_absolute(struct fallback_dispatch *dispatch,
			  struct evdev_device *device,
			  struct evdev_event *e,
			  uint64_t time)
{
	if (device->is_mt) {
		fallback_process_touch(dispatch, device, e, time);
	} else {
		fallback_process_absolute_motion(dispatch, device, e);
	}
}

static inline bool
fallback_any_button_down(struct fallback_dispatch *dispatch,
			 struct evdev_device *device)
{
	for (evdev_usage_t usage = evdev_usage_from(EVDEV_BTN_LEFT);
	     evdev_usage_lt(usage, EVDEV_BTN_JOYSTICK);
	     usage = evdev_usage_next(usage)) {
		if (libevdev_has_event_code(device->evdev,
					    EV_KEY,
					    evdev_usage_code(usage)) &&
		    hw_is_key_down(dispatch, usage))
			return true;
	}
	return false;
}

static inline bool
fallback_arbitrate_touch(struct fallback_dispatch *dispatch, struct mt_slot *slot)
{
	bool discard = false;
	struct device_coords point = slot->point;
	evdev_transform_absolute(dispatch->device, &point);

	if (dispatch->arbitration.state == ARBITRATION_IGNORE_RECT &&
	    point_in_rect(&point, &dispatch->arbitration.rect)) {
		slot->palm_state = PALM_IS_PALM;
		discard = true;
	}

	return discard;
}

static inline bool
fallback_flush_mt_events(struct fallback_dispatch *dispatch,
			 struct evdev_device *device,
			 uint64_t time)
{
	bool sent = false;

	for (size_t i = 0; i < dispatch->mt.slots_len; i++) {
		struct mt_slot *slot = &dispatch->mt.slots[i];

		if (!slot->dirty)
			continue;

		slot->dirty = false;

		/* Any palm state other than PALM_NEW means we've either
		 * already cancelled the touch or the touch was never
		 * a finger anyway and we didn't send the begin.
		 */
		if (slot->palm_state == PALM_NEW) {
			if (slot->state != SLOT_STATE_BEGIN)
				sent = fallback_flush_mt_cancel(dispatch,
								device,
								i,
								time);
			slot->palm_state = PALM_IS_PALM;
		} else if (slot->palm_state == PALM_NONE) {
			switch (slot->state) {
			case SLOT_STATE_BEGIN:
				if (!fallback_arbitrate_touch(dispatch, slot)) {
					sent = fallback_flush_mt_down(dispatch,
								      device,
								      i,
								      time);
				}
				break;
			case SLOT_STATE_UPDATE:
				sent = fallback_flush_mt_motion(dispatch,
								device,
								i,
								time);
				break;
			case SLOT_STATE_END:
				sent = fallback_flush_mt_up(dispatch, device, i, time);
				break;
			case SLOT_STATE_NONE:
				break;
			}
		}

		/* State machine continues independent of the palm state */
		switch (slot->state) {
		case SLOT_STATE_BEGIN:
			slot->state = SLOT_STATE_UPDATE;
			break;
		case SLOT_STATE_UPDATE:
			break;
		case SLOT_STATE_END:
			slot->state = SLOT_STATE_NONE;
			break;
		case SLOT_STATE_NONE:
			/* touch arbitration may swallow the begin,
			 * so we may get updates for a touch still
			 * in NONE state */
			break;
		}
	}

	return sent;
}

static void
fallback_handle_state(struct fallback_dispatch *dispatch,
		      struct evdev_device *device,
		      uint64_t time)
{
	bool need_touch_frame = false;

	/* Relative motion */
	if (dispatch->pending_event & EVDEV_RELATIVE_MOTION)
		fallback_flush_relative_motion(dispatch, device, time);

	/* Single touch or absolute pointer devices */
	if (dispatch->pending_event & EVDEV_ABSOLUTE_TOUCH_DOWN) {
		if (fallback_flush_st_down(dispatch, device, time))
			need_touch_frame = true;
	} else if (dispatch->pending_event & EVDEV_ABSOLUTE_MOTION) {
		if (device->seat_caps & EVDEV_DEVICE_TOUCH) {
			if (fallback_flush_st_motion(dispatch, device, time))
				need_touch_frame = true;
		} else if (device->seat_caps & EVDEV_DEVICE_POINTER) {
			fallback_flush_absolute_motion(dispatch, device, time);
		}
	}

	if (dispatch->pending_event & EVDEV_ABSOLUTE_TOUCH_UP) {
		if (fallback_flush_st_up(dispatch, device, time))
			need_touch_frame = true;
	}

	/* Multitouch devices */
	if (dispatch->pending_event & EVDEV_ABSOLUTE_MT)
		need_touch_frame = fallback_flush_mt_events(dispatch, device, time);

	if (need_touch_frame)
		touch_notify_frame(&device->base, time);

	fallback_flush_wheels(dispatch, device, time);

	/* Buttons and keys */
	if (dispatch->pending_event & EVDEV_KEY) {
		for (evdev_usage_t usage = evdev_usage_from(EVDEV_KEY_RESERVED);
		     evdev_usage_le(usage, EVDEV_KEY_MAX);
		     usage = evdev_usage_next(usage)) {
			if (!hw_key_has_changed(dispatch, usage))
				continue;

			if (evdev_usage_is_button(usage)) {
				enum libinput_button_state state =
					hw_is_key_down(dispatch, usage)
						? LIBINPUT_BUTTON_STATE_PRESSED
						: LIBINPUT_BUTTON_STATE_RELEASED;
				evdev_usage_t button =
					evdev_to_left_handed(device, usage);
				fallback_notify_physical_button(dispatch,
								device,
								time,
								button,
								state);
			}
		}

		hw_key_update_last_state(dispatch);
	}

	dispatch->pending_event = EVDEV_NONE;
}

static void
fallback_interface_process_event(struct evdev_dispatch *evdev_dispatch,
				 struct evdev_device *device,
				 struct evdev_event *event,
				 uint64_t time)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);
	static bool warned = false;

	if (dispatch->arbitration.in_arbitration) {
		if (!warned) {
			evdev_log_debug(device,
					"dropping events due to touch arbitration\n");
			warned = true;
		}
		return;
	}

	warned = false;

	uint16_t type = evdev_event_type(event);
	switch (type) {
	case EV_REL:
		fallback_process_relative(dispatch, device, event, time);
		break;
	case EV_ABS:
		fallback_process_absolute(dispatch, device, event, time);
		break;
	case EV_KEY:
		fallback_process_key(dispatch, device, event, time);
		break;
	case EV_SW:
		fallback_process_switch(dispatch, device, event, time);
		break;
	case EV_SYN:
		fallback_handle_state(dispatch, device, time);
		break;
	}
}

static void
fallback_interface_process(struct evdev_dispatch *dispatch,
			   struct evdev_device *device,
			   struct evdev_frame *frame,
			   uint64_t time)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	for (size_t i = 0; i < nevents; i++) {
		fallback_interface_process_event(dispatch, device, &events[i], time);
	}
}

static void
cancel_touches(struct fallback_dispatch *dispatch,
	       struct evdev_device *device,
	       const struct device_coord_rect *rect,
	       uint64_t time)
{
	unsigned int idx;
	bool need_frame = false;
	struct device_coords point;

	point = dispatch->abs.point;
	evdev_transform_absolute(device, &point);
	if (!rect || point_in_rect(&point, rect))
		need_frame = fallback_flush_st_cancel(dispatch, device, time);

	for (idx = 0; idx < dispatch->mt.slots_len; idx++) {
		struct mt_slot *slot = &dispatch->mt.slots[idx];
		point = slot->point;
		evdev_transform_absolute(device, &point);

		if (slot->seat_slot == -1)
			continue;

		if ((!rect || point_in_rect(&point, rect)) &&
		    fallback_flush_mt_cancel(dispatch, device, idx, time))
			need_frame = true;
	}

	if (need_frame)
		touch_notify_frame(&device->base, time);
}

static void
release_pressed_keys(struct fallback_dispatch *dispatch,
		     struct evdev_device *device,
		     uint64_t time)
{
	for (evdev_usage_t usage = evdev_usage_from(EVDEV_KEY_RESERVED);
	     evdev_usage_le(usage, EVDEV_KEY_MAX);
	     usage = evdev_usage_next(usage)) {
		int count = get_key_down_count(device, usage);

		if (count == 0)
			continue;

		if (count > 1) {
			evdev_log_bug_libinput(device,
					       "key %d is down %d times.\n",
					       evdev_usage_code(usage),
					       count);
		}

		if (evdev_usage_is_key(usage)) {
			fallback_keyboard_notify_key(dispatch,
						     device,
						     time,
						     usage,
						     LIBINPUT_KEY_STATE_RELEASED);
		} else if (evdev_usage_is_button(usage)) {
			/* Note: the left-handed configuration is nonzero for
			 * the mapped button (not the physical button), in
			 * get_key_down_count(). We must not map this to left-handed
			 * again, see #881.
			 */
			evdev_pointer_notify_button(device,
						    time,
						    usage,
						    LIBINPUT_BUTTON_STATE_RELEASED);
		}

		count = get_key_down_count(device, usage);
		if (count != 0) {
			evdev_log_bug_libinput(device,
					       "releasing key %d failed.\n",
					       evdev_usage_enum(usage));
			break;
		}
	}
}

static void
fallback_return_to_neutral_state(struct fallback_dispatch *dispatch,
				 struct evdev_device *device)
{
	struct libinput *libinput = evdev_libinput_context(device);
	uint64_t time;

	if ((time = libinput_now(libinput)) == 0)
		return;

	cancel_touches(dispatch, device, NULL, time);
	release_pressed_keys(dispatch, device, time);
	memset(dispatch->hw_key_mask, 0, sizeof(dispatch->hw_key_mask));
	memset(dispatch->hw_key_mask, 0, sizeof(dispatch->last_hw_key_mask));
}

static void
fallback_interface_suspend(struct evdev_dispatch *evdev_dispatch,
			   struct evdev_device *device)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);

	fallback_return_to_neutral_state(dispatch, device);
}

static void
fallback_interface_remove(struct evdev_dispatch *evdev_dispatch)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);
	struct evdev_paired_keyboard *kbd;

	libinput_timer_cancel(&dispatch->debounce.timer);
	libinput_timer_cancel(&dispatch->debounce.timer_short);
	libinput_timer_cancel(&dispatch->arbitration.arbitration_timer);

	libinput_device_remove_event_listener(&dispatch->tablet_mode.other.listener);

	list_for_each_safe(kbd, &dispatch->lid.paired_keyboard_list, link) {
		evdev_paired_keyboard_destroy(kbd);
	}
}

static void
fallback_interface_sync_initial_state(struct evdev_device *device,
				      struct evdev_dispatch *evdev_dispatch)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);
	uint64_t time = libinput_now(evdev_libinput_context(device));

	if (device->tags & EVDEV_TAG_LID_SWITCH) {
		struct libevdev *evdev = device->evdev;

		dispatch->lid.is_closed =
			libevdev_get_event_value(evdev, EV_SW, SW_LID);
		dispatch->lid.is_closed_client_state = false;

		/* For the initial state sync, we depend on whether the lid switch
		 * is reliable. If we know it's reliable, we sync as expected.
		 * If we're not sure, we ignore the initial state and only sync on
		 * the first future lid close event. Laptops with a broken switch
		 * that always have the switch in 'on' state thus don't mess up our
		 * touchpad.
		 */
		if (dispatch->lid.is_closed &&
		    dispatch->lid.reliability == RELIABILITY_RELIABLE) {
			fallback_lid_notify_toggle(dispatch, device, time);
		}
	}

	if (dispatch->tablet_mode.sw.state) {
		switch_notify_toggle(&device->base,
				     time,
				     LIBINPUT_SWITCH_TABLET_MODE,
				     LIBINPUT_SWITCH_STATE_ON);
	}
}

static void
fallback_interface_update_rect(struct evdev_dispatch *evdev_dispatch,
			       struct evdev_device *device,
			       const struct phys_rect *phys_rect,
			       uint64_t time)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);
	struct device_coord_rect rect;

	assert(phys_rect);

	/* Existing touches do not change, we just update the rect and only
	 * new touches in these areas will be ignored. If you want to paint
	 * over your finger, be my guest. */
	rect = evdev_phys_rect_to_units(device, phys_rect);
	dispatch->arbitration.rect = rect;
}

static void
fallback_interface_toggle_touch(struct evdev_dispatch *evdev_dispatch,
				struct evdev_device *device,
				enum evdev_arbitration_state which,
				const struct phys_rect *phys_rect,
				uint64_t time)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);
	struct device_coord_rect rect = { 0 };
	const char *state = NULL;

	if (which == dispatch->arbitration.state)
		return;

	switch (which) {
	case ARBITRATION_NOT_ACTIVE:
		/* if in-kernel arbitration is in use and there is a touch
		 * and a pen in proximity, lifting the pen out of proximity
		 * causes a touch begin for the touch. On a hand-lift the
		 * proximity out precedes the touch up by a few ms, so we
		 * get what looks like a tap. Fix this by delaying
		 * arbitration by just a little bit so that any touch in
		 * event is caught as palm touch. */
		libinput_timer_set(&dispatch->arbitration.arbitration_timer,
				   time + ms2us(90));
		state = "not-active";
		break;
	case ARBITRATION_IGNORE_RECT:
		assert(phys_rect);
		rect = evdev_phys_rect_to_units(device, phys_rect);
		cancel_touches(dispatch, device, &rect, time);
		dispatch->arbitration.rect = rect;
		state = "ignore-rect";
		break;
	case ARBITRATION_IGNORE_ALL:
		libinput_timer_cancel(&dispatch->arbitration.arbitration_timer);
		fallback_return_to_neutral_state(dispatch, device);
		dispatch->arbitration.in_arbitration = true;
		state = "ignore-all";
		break;
	}

	evdev_log_debug(device, "Touch arbitration state now %s\n", state);

	dispatch->arbitration.state = which;
}

static void
fallback_interface_destroy(struct evdev_dispatch *evdev_dispatch)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(evdev_dispatch);

	libinput_timer_destroy(&dispatch->arbitration.arbitration_timer);
	libinput_timer_destroy(&dispatch->debounce.timer);
	libinput_timer_destroy(&dispatch->debounce.timer_short);

	free(dispatch->mt.slots);
	free(dispatch);
}

static void
fallback_lid_pair_keyboard(struct evdev_device *lid_switch,
			   struct evdev_device *keyboard)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(lid_switch->dispatch);
	struct evdev_paired_keyboard *kbd;
	size_t count = 0;

	if ((keyboard->tags & EVDEV_TAG_KEYBOARD) == 0 ||
	    (lid_switch->tags & EVDEV_TAG_LID_SWITCH) == 0)
		return;

	if ((keyboard->tags & EVDEV_TAG_INTERNAL_KEYBOARD) == 0)
		return;

	list_for_each(kbd, &dispatch->lid.paired_keyboard_list, link) {
		count++;
		if (count > 3) {
			evdev_log_info(lid_switch,
				       "lid: too many internal keyboards\n");
			break;
		}
	}

	kbd = zalloc(sizeof(*kbd));
	kbd->device = keyboard;
	libinput_device_init_event_listener(&kbd->listener);
	list_insert(&dispatch->lid.paired_keyboard_list, &kbd->link);
	evdev_log_debug(lid_switch,
			"lid: keyboard paired with %s<->%s\n",
			lid_switch->devname,
			keyboard->devname);

	/* We need to init the event listener now only if the
	 * reported state is closed. */
	if (dispatch->lid.is_closed)
		fallback_lid_toggle_keyboard_listener(dispatch,
						      kbd,
						      dispatch->lid.is_closed);
}

static void
fallback_resume(struct fallback_dispatch *dispatch, struct evdev_device *device)
{
	if (dispatch->base.sendevents.current_mode ==
	    LIBINPUT_CONFIG_SEND_EVENTS_DISABLED)
		return;

	evdev_device_resume(device);
}

static void
fallback_suspend(struct fallback_dispatch *dispatch, struct evdev_device *device)
{
	evdev_device_suspend(device);
}

static void
fallback_tablet_mode_switch_event(uint64_t time,
				  struct libinput_event *event,
				  void *data)
{
	struct fallback_dispatch *dispatch = data;
	struct evdev_device *device = dispatch->device;
	struct libinput_event_switch *swev;

	if (libinput_event_get_type(event) != LIBINPUT_EVENT_SWITCH_TOGGLE)
		return;

	swev = libinput_event_get_switch_event(event);
	if (libinput_event_switch_get_switch(swev) != LIBINPUT_SWITCH_TABLET_MODE)
		return;

	switch (libinput_event_switch_get_switch_state(swev)) {
	case LIBINPUT_SWITCH_STATE_OFF:
		fallback_resume(dispatch, device);
		evdev_log_debug(device, "tablet-mode: resuming device\n");
		break;
	case LIBINPUT_SWITCH_STATE_ON:
		fallback_suspend(dispatch, device);
		evdev_log_debug(device, "tablet-mode: suspending device\n");
		break;
	}
}

static void
fallback_pair_tablet_mode(struct evdev_device *keyboard,
			  struct evdev_device *tablet_mode_switch)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(keyboard->dispatch);

	if (keyboard->tags & EVDEV_TAG_EXTERNAL_KEYBOARD)
		return;

	if (keyboard->tags & EVDEV_TAG_TRACKPOINT) {
		if (keyboard->tags & EVDEV_TAG_EXTERNAL_MOUSE)
			return;
		/* This filters out all internal keyboard-like devices (Video
		 * Switch) */
	} else if ((keyboard->tags & EVDEV_TAG_INTERNAL_KEYBOARD) == 0) {
		return;
	}

	if (evdev_device_has_model_quirk(keyboard, QUIRK_MODEL_TABLET_MODE_NO_SUSPEND))
		return;

	if ((tablet_mode_switch->tags & EVDEV_TAG_TABLET_MODE_SWITCH) == 0)
		return;

	if (dispatch->tablet_mode.other.sw_device)
		return;

	evdev_log_debug(keyboard,
			"tablet-mode: paired %s<->%s\n",
			keyboard->devname,
			tablet_mode_switch->devname);

	libinput_device_add_event_listener(&tablet_mode_switch->base,
					   &dispatch->tablet_mode.other.listener,
					   fallback_tablet_mode_switch_event,
					   dispatch);
	dispatch->tablet_mode.other.sw_device = tablet_mode_switch;

	if (evdev_device_switch_get_state(tablet_mode_switch,
					  LIBINPUT_SWITCH_TABLET_MODE) ==
	    LIBINPUT_SWITCH_STATE_ON) {
		evdev_log_debug(keyboard, "tablet-mode: suspending device\n");
		fallback_suspend(dispatch, keyboard);
	}
}

static void
fallback_interface_device_added(struct evdev_device *device,
				struct evdev_device *added_device)
{
	fallback_lid_pair_keyboard(device, added_device);
	fallback_pair_tablet_mode(device, added_device);
}

static void
fallback_interface_device_removed(struct evdev_device *device,
				  struct evdev_device *removed_device)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(device->dispatch);
	struct evdev_paired_keyboard *kbd;

	list_for_each_safe(kbd, &dispatch->lid.paired_keyboard_list, link) {
		if (!kbd->device)
			continue;

		if (kbd->device != removed_device)
			continue;

		evdev_paired_keyboard_destroy(kbd);
	}

	if (removed_device == dispatch->tablet_mode.other.sw_device) {
		libinput_device_remove_event_listener(
			&dispatch->tablet_mode.other.listener);
		libinput_device_init_event_listener(
			&dispatch->tablet_mode.other.listener);
		dispatch->tablet_mode.other.sw_device = NULL;
	}
}

static struct evdev_dispatch_interface fallback_interface = {
	.process = fallback_interface_process,
	.suspend = fallback_interface_suspend,
	.remove = fallback_interface_remove,
	.destroy = fallback_interface_destroy,
	.device_added = fallback_interface_device_added,
	.device_removed = fallback_interface_device_removed,
	.device_suspended = fallback_interface_device_removed, /* treat as remove */
	.device_resumed = fallback_interface_device_added,     /* treat as add */
	.post_added = fallback_interface_sync_initial_state,
	.touch_arbitration_toggle = fallback_interface_toggle_touch,
	.touch_arbitration_update_rect = fallback_interface_update_rect,
	.get_switch_state = fallback_interface_get_switch_state,
};

static void
fallback_change_to_left_handed(struct evdev_device *device)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(device->dispatch);

	if (device->left_handed.want_enabled == device->left_handed.enabled)
		return;

	if (fallback_any_button_down(dispatch, device))
		return;

	device->left_handed.enabled = device->left_handed.want_enabled;
}

static void
fallback_change_scroll_method(struct evdev_device *device)
{
	struct fallback_dispatch *dispatch = fallback_dispatch(device->dispatch);

	if (device->scroll.want_method == device->scroll.method &&
	    evdev_usage_cmp(device->scroll.want_button, device->scroll.button) == 0 &&
	    device->scroll.want_lock_enabled == device->scroll.lock_enabled)
		return;

	if (fallback_any_button_down(dispatch, device))
		return;

	device->scroll.method = device->scroll.want_method;
	device->scroll.button = device->scroll.want_button;
	device->scroll.lock_enabled = device->scroll.want_lock_enabled;
	evdev_set_button_scroll_lock_enabled(device, device->scroll.lock_enabled);
}

static int
fallback_rotation_config_is_available(struct libinput_device *device)
{
	/* This function only gets called when we support rotation */
	return 1;
}

static enum libinput_config_status
fallback_rotation_config_set_angle(struct libinput_device *libinput_device,
				   unsigned int degrees_cw)
{
	struct evdev_device *device = evdev_device(libinput_device);
	struct fallback_dispatch *dispatch = fallback_dispatch(device->dispatch);

	dispatch->rotation.angle = degrees_cw;
	matrix_init_rotate(&dispatch->rotation.matrix, degrees_cw);

	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static unsigned int
fallback_rotation_config_get_angle(struct libinput_device *libinput_device)
{
	struct evdev_device *device = evdev_device(libinput_device);
	struct fallback_dispatch *dispatch = fallback_dispatch(device->dispatch);

	return dispatch->rotation.angle;
}

static unsigned int
fallback_rotation_config_get_default_angle(struct libinput_device *device)
{
	return 0;
}

static void
fallback_init_rotation(struct fallback_dispatch *dispatch, struct evdev_device *device)
{
	if (device->tags & EVDEV_TAG_TRACKPOINT)
		return;

	dispatch->rotation.config.is_available = fallback_rotation_config_is_available;
	dispatch->rotation.config.set_angle = fallback_rotation_config_set_angle;
	dispatch->rotation.config.get_angle = fallback_rotation_config_get_angle;
	dispatch->rotation.config.get_default_angle =
		fallback_rotation_config_get_default_angle;
	matrix_init_identity(&dispatch->rotation.matrix);
	device->base.config.rotation = &dispatch->rotation.config;
}

static inline int
fallback_dispatch_init_slots(struct fallback_dispatch *dispatch,
			     struct evdev_device *device)
{
	struct libevdev *evdev = device->evdev;
	struct mt_slot *slots;
	int num_slots;
	int active_slot;
	int slot;

	if (evdev_is_fake_mt_device(device) ||
	    !libevdev_has_event_code(evdev, EV_ABS, ABS_MT_POSITION_X) ||
	    !libevdev_has_event_code(evdev, EV_ABS, ABS_MT_POSITION_Y))
		return 0;

	/* We only handle the slotted Protocol B in libinput.
	   Devices with ABS_MT_POSITION_* but not ABS_MT_SLOT
	   require mtdev for conversion. */
	if (!libevdev_has_event_code(evdev, EV_ABS, ABS_MT_SLOT))
		return 0;

	num_slots = libevdev_get_num_slots(device->evdev);
	active_slot = libevdev_get_current_slot(evdev);
	slots = zalloc(num_slots * sizeof(struct mt_slot));

	for (slot = 0; slot < num_slots; ++slot) {
		slots[slot].seat_slot = -1;
		slots[slot].point.x =
			libevdev_get_slot_value(evdev, slot, ABS_MT_POSITION_X);
		slots[slot].point.y =
			libevdev_get_slot_value(evdev, slot, ABS_MT_POSITION_Y);
	}
	dispatch->mt.slots = slots;
	dispatch->mt.slots_len = num_slots;
	dispatch->mt.slot = active_slot;
	dispatch->mt.has_palm =
		libevdev_has_event_code(evdev, EV_ABS, ABS_MT_TOOL_TYPE);

	if (device->abs.absinfo_x->fuzz || device->abs.absinfo_y->fuzz) {
		dispatch->mt.want_hysteresis = true;
		dispatch->mt.hysteresis_margin.x = device->abs.absinfo_x->fuzz / 2;
		dispatch->mt.hysteresis_margin.y = device->abs.absinfo_y->fuzz / 2;
	}

	return 0;
}

static inline void
fallback_dispatch_init_rel(struct fallback_dispatch *dispatch,
			   struct evdev_device *device)
{
	dispatch->rel.x = 0;
	dispatch->rel.y = 0;
}

static inline void
fallback_dispatch_init_abs(struct fallback_dispatch *dispatch,
			   struct evdev_device *device)
{
	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_X))
		return;

	dispatch->abs.point.x = device->abs.absinfo_x->value;
	dispatch->abs.point.y = device->abs.absinfo_y->value;
	dispatch->abs.seat_slot = -1;

	evdev_device_init_abs_range_warnings(device);
}

static inline void
fallback_dispatch_init_switch(struct fallback_dispatch *dispatch,
			      struct evdev_device *device)
{
	int val;

	list_init(&dispatch->lid.paired_keyboard_list);

	if (device->tags & EVDEV_TAG_LID_SWITCH) {
		dispatch->lid.reliability = evdev_read_switch_reliability_prop(device);
		dispatch->lid.is_closed = false;
	}

	if (device->tags & EVDEV_TAG_TABLET_MODE_SWITCH) {
		val = libevdev_get_event_value(device->evdev, EV_SW, SW_TABLET_MODE);
		dispatch->tablet_mode.sw.state = val;
	}

	libinput_device_init_event_listener(&dispatch->tablet_mode.other.listener);
}

static void
fallback_arbitration_timeout(uint64_t now, void *data)
{
	struct fallback_dispatch *dispatch = data;

	if (dispatch->arbitration.in_arbitration)
		dispatch->arbitration.in_arbitration = false;

	evdev_log_debug(dispatch->device, "touch arbitration timeout\n");
}

static void
fallback_init_arbitration(struct fallback_dispatch *dispatch,
			  struct evdev_device *device)
{
	char timer_name[64];

	snprintf(timer_name,
		 sizeof(timer_name),
		 "%s arbitration",
		 evdev_device_get_sysname(device));
	libinput_timer_init(&dispatch->arbitration.arbitration_timer,
			    evdev_libinput_context(device),
			    timer_name,
			    fallback_arbitration_timeout,
			    dispatch);
	dispatch->arbitration.in_arbitration = false;
}

struct evdev_dispatch *
fallback_dispatch_create(struct libinput_device *libinput_device)
{
	struct evdev_device *device = evdev_device(libinput_device);
	struct fallback_dispatch *dispatch;

	dispatch = zalloc(sizeof *dispatch);
	dispatch->device = evdev_device(libinput_device);
	dispatch->base.dispatch_type = DISPATCH_FALLBACK;
	dispatch->base.interface = &fallback_interface;
	dispatch->pending_event = EVDEV_NONE;
	list_init(&dispatch->lid.paired_keyboard_list);

	fallback_dispatch_init_rel(dispatch, device);
	fallback_dispatch_init_abs(dispatch, device);
	if (fallback_dispatch_init_slots(dispatch, device) == -1) {
		free(dispatch);
		return NULL;
	}

	fallback_dispatch_init_switch(dispatch, device);

	if (device->left_handed.want_enabled)
		evdev_init_left_handed(device, fallback_change_to_left_handed);

	if (evdev_usage_enum(device->scroll.want_button))
		evdev_init_button_scroll(device, fallback_change_scroll_method);

	if (device->scroll.natural_scrolling_enabled)
		evdev_init_natural_scroll(device);

	evdev_init_calibration(device, &dispatch->calibration);
	evdev_init_sendevents(device, &dispatch->base);
	fallback_init_rotation(dispatch, device);

	/* BTN_MIDDLE is set on mice even when it's not present. So
	 * we can only use the absence of BTN_MIDDLE to mean something, i.e.
	 * we enable it by default on anything that only has L&R.
	 * If we have L&R and no middle, we don't expose it as config
	 * option */
	if (libevdev_has_event_code(device->evdev, EV_KEY, BTN_LEFT) &&
	    libevdev_has_event_code(device->evdev, EV_KEY, BTN_RIGHT)) {
		bool has_middle =
			libevdev_has_event_code(device->evdev, EV_KEY, BTN_MIDDLE);
		bool want_config = has_middle;
		bool enable_by_default = !has_middle;

		evdev_init_middlebutton(device, enable_by_default, want_config);
	}

	fallback_init_arbitration(dispatch, device);

	return &dispatch->base;
}
