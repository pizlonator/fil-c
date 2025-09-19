/*
 * Copyright © 2016 Red Hat, Inc.
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
#include <stdbool.h>
#include <string.h>

#include "util-input-event.h"

#include "evdev-tablet-pad.h"

#ifdef HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#endif

#define pad_set_status(pad_, s_) (pad_)->status |= (s_)
#define pad_unset_status(pad_, s_) (pad_)->status &= ~(s_)
#define pad_has_status(pad_, s_) (!!((pad_)->status & (s_)))

static void
pad_get_buttons_pressed(struct pad_dispatch *pad, struct button_state *buttons)
{
	struct button_state *state = &pad->button_state;
	struct button_state *prev_state = &pad->prev_button_state;
	unsigned int i;

	for (i = 0; i < sizeof(buttons->bits); i++)
		buttons->bits[i] = state->bits[i] & ~(prev_state->bits[i]);
}

static void
pad_get_buttons_released(struct pad_dispatch *pad, struct button_state *buttons)
{
	struct button_state *state = &pad->button_state;
	struct button_state *prev_state = &pad->prev_button_state;
	unsigned int i;

	for (i = 0; i < sizeof(buttons->bits); i++)
		buttons->bits[i] = prev_state->bits[i] & ~(state->bits[i]);
}

static inline bool
pad_button_is_down(const struct pad_dispatch *pad, uint32_t button)
{
	return bit_is_set(pad->button_state.bits, button);
}

static inline bool
pad_any_button_down(const struct pad_dispatch *pad)
{
	const struct button_state *state = &pad->button_state;
	unsigned int i;

	for (i = 0; i < sizeof(state->bits); i++)
		if (state->bits[i] != 0)
			return true;

	return false;
}

static inline void
pad_button_set_down(struct pad_dispatch *pad, evdev_usage_t button, bool is_down)
{
	struct button_state *state = &pad->button_state;
	unsigned int code = evdev_usage_code(button);

	if (is_down) {
		set_bit(state->bits, code);
		pad_set_status(pad, PAD_BUTTONS_PRESSED);
	} else {
		clear_bit(state->bits, code);
		pad_set_status(pad, PAD_BUTTONS_RELEASED);
	}
}

static void
pad_process_relative(struct pad_dispatch *pad,
		     struct evdev_device *device,
		     struct evdev_event *e,
		     uint64_t time)
{
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_DIAL:
		pad->dials.dial1 = e->value * 120;
		pad->changed_axes |= PAD_AXIS_DIAL1;
		pad_set_status(pad, PAD_AXES_UPDATED);
		break;
	case EVDEV_REL_WHEEL:
		if (!pad->dials.has_hires_dial) {
			pad->dials.dial1 = -1 * e->value * 120;
			pad->changed_axes |= PAD_AXIS_DIAL1;
			pad_set_status(pad, PAD_AXES_UPDATED);
		}
		break;
	case EVDEV_REL_HWHEEL:
		if (!pad->dials.has_hires_dial) {
			pad->dials.dial2 = e->value * 120;
			pad->changed_axes |= PAD_AXIS_DIAL2;
			pad_set_status(pad, PAD_AXES_UPDATED);
		}
		break;
	case EVDEV_REL_WHEEL_HI_RES:
		pad->dials.dial1 = -1 * e->value;
		pad->changed_axes |= PAD_AXIS_DIAL1;
		pad_set_status(pad, PAD_AXES_UPDATED);
		break;
	case EVDEV_REL_HWHEEL_HI_RES:
		pad->dials.dial2 = e->value;
		pad->changed_axes |= PAD_AXIS_DIAL2;
		pad_set_status(pad, PAD_AXES_UPDATED);
		break;
	default:
		evdev_log_info(device,
			       "Unhandled EV_REL event code %#x\n",
			       evdev_usage_as_uint32_t(e->usage));
		break;
	}
}

static void
pad_update_changed_axis(struct pad_dispatch *pad,
			enum pad_axes axis,
			const struct evdev_event *e)
{
	if (pad->changed_axes & axis) {
		evdev_log_bug_kernel_ratelimit(
			pad->device,
			&pad->duplicate_abs_limit,
			"Multiple EV_ABS %s events in the same SYN_REPORT\n",
			evdev_event_get_code_name(e));

		/* Special heuristics probably good enough:
		 * if we get multiple EV_ABS in the same SYN_REPORT
		 * and one of them is zero, assume they're all
		 * zero and unchanged. That's not perfectly
		 * correct but probably covers all cases */
		if (e->value == 0) {
			pad->changed_axes &= ~axis;
			if (pad->changed_axes == 0)
				pad_unset_status(pad, PAD_AXES_UPDATED);
			return;
		}
	}

	pad->changed_axes |= axis;
	pad_set_status(pad, PAD_AXES_UPDATED);
}

static void
pad_process_absolute(struct pad_dispatch *pad,
		     struct evdev_device *device,
		     struct evdev_event *e,
		     uint64_t time)
{
	enum pad_axes axis = PAD_AXIS_NONE;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_ABS_WHEEL:
		axis = PAD_AXIS_RING1;
		break;
	case EVDEV_ABS_THROTTLE:
		axis = PAD_AXIS_RING2;
		break;
	case EVDEV_ABS_RX:
		axis = PAD_AXIS_STRIP1;
		break;
	case EVDEV_ABS_RY:
		axis = PAD_AXIS_STRIP2;
		break;
	case EVDEV_ABS_MISC:
		/* The wacom driver always sends a 0 axis event on finger
		   up, but we also get an ABS_MISC 15 on touch down and
		   ABS_MISC 0 on touch up, on top of the actual event. This
		   is kernel behavior for xf86-input-wacom backwards
		   compatibility after the 3.17 wacom HID move.

		   We use that event to tell when we truly went a full
		   rotation around the wheel vs. a finger release.

		   FIXME: On the Intuos5 and later the kernel merges all
		   states into that event, so if any finger is down on any
		   button, the wheel release won't trigger the ABS_MISC 0
		   but still send a 0 event. We can't currently detect this.
		 */
		pad->have_abs_misc_terminator = true;
		break;
	default:
		evdev_log_info(device,
			       "Unhandled EV_ABS event code %#x\n",
			       evdev_usage_as_uint32_t(e->usage));
		break;
	}

	if (axis != PAD_AXIS_NONE) {
		pad_update_changed_axis(pad, axis, e);
	}
}

static inline double
normalize_wacom_ring(const struct input_absinfo *absinfo)
{
	/* libinput has 0 as the ring's northernmost point in the device's
	   current logical rotation, increasing clockwise to 1. Wacom has
	   0 on the left-most wheel position.
	 */
	double range = absinfo_range(absinfo);
	double value = (absinfo->value - absinfo->minimum) / range - 0.25;

	if (value < 0.0)
		value += 1.0;

	return value;
}

static inline double
normalize_wacom_strip(const struct input_absinfo *absinfo)
{
	/* strip axes don't use a proper value, they just shift the bit left
	 * for each position. 0 isn't a real value either, it's only sent on
	 * finger release */
	double min = 0, max = log2(absinfo->maximum);
	double range = max - min;
	double value = (log2(absinfo->value) - min) / range;

	return value;
}

static inline double
normalize_strip(const struct input_absinfo *absinfo)
{
	return absinfo_normalize_value(absinfo, absinfo->value);
}

static inline double
pad_handle_ring(struct pad_dispatch *pad,
		struct evdev_device *device,
		unsigned int code)
{
	const struct input_absinfo *absinfo;
	double degrees;

	absinfo = libevdev_get_abs_info(device->evdev, code);
	assert(absinfo);

	degrees = normalize_wacom_ring(absinfo) * 360;

	if (device->left_handed.enabled)
		degrees = fmod(degrees + 180, 360);

	return degrees;
}

static inline double
pad_handle_strip(struct pad_dispatch *pad,
		 struct evdev_device *device,
		 unsigned int code)
{
	const struct input_absinfo *absinfo;
	double pos;

	absinfo = libevdev_get_abs_info(device->evdev, code);
	assert(absinfo);

	if (absinfo->value == 0)
		return 0.0;

	if (evdev_device_get_id_vendor(device) == VENDOR_ID_WACOM)
		pos = normalize_wacom_strip(absinfo);
	else
		pos = normalize_strip(absinfo);

	if (device->left_handed.enabled)
		pos = 1.0 - pos;

	return pos;
}

static inline struct libinput_tablet_pad_mode_group *
pad_dial_get_mode_group(struct pad_dispatch *pad, unsigned int dial)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (libinput_tablet_pad_mode_group_has_dial(group, dial))
			return group;
	}

	evdev_log_bug_libinput_ratelimit(pad->device,
					 &pad->modes.group_not_found,
					 "Unable to find mode group for dial %d",
					 dial);

	return NULL;
}

static inline struct libinput_tablet_pad_mode_group *
pad_ring_get_mode_group(struct pad_dispatch *pad, unsigned int ring)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (libinput_tablet_pad_mode_group_has_ring(group, ring))
			return group;
	}

	evdev_log_bug_libinput_ratelimit(pad->device,
					 &pad->modes.group_not_found,
					 "Unable to find mode group for ring %d",
					 ring);

	return NULL;
}

static inline struct libinput_tablet_pad_mode_group *
pad_strip_get_mode_group(struct pad_dispatch *pad, unsigned int strip)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (libinput_tablet_pad_mode_group_has_strip(group, strip))
			return group;
	}

	evdev_log_bug_libinput_ratelimit(pad->device,
					 &pad->modes.group_not_found,
					 "Unable to find mode group for strip %d",
					 strip);

	return NULL;
}

static void
pad_check_notify_axes(struct pad_dispatch *pad,
		      struct evdev_device *device,
		      uint64_t time)
{
	struct libinput_device *base = &device->base;
	struct libinput_tablet_pad_mode_group *group;
	double value;
	bool send_finger_up = false;

	/* Suppress the reset to 0 on finger up. See the
	   comment in pad_process_absolute */
	if (pad->have_abs_misc_terminator &&
	    libevdev_get_event_value(device->evdev, EV_ABS, ABS_MISC) == 0)
		send_finger_up = true;

	/* Unlike the ring axis we don't get an event when we release
	 * so we can't set a source */
	if (pad->changed_axes & PAD_AXIS_DIAL1) {
		group = pad_dial_get_mode_group(pad, 0);
		if (group)
			tablet_pad_notify_dial(base, time, 0, pad->dials.dial1, group);
	}

	if (pad->changed_axes & PAD_AXIS_DIAL2) {
		group = pad_dial_get_mode_group(pad, 1);
		if (group)
			tablet_pad_notify_dial(base, time, 1, pad->dials.dial2, group);
	}

	if (pad->changed_axes & PAD_AXIS_RING1) {
		value = pad_handle_ring(pad, device, ABS_WHEEL);
		if (send_finger_up)
			value = -1.0;

		group = pad_ring_get_mode_group(pad, 0);
		if (group)
			tablet_pad_notify_ring(base,
					       time,
					       0,
					       value,
					       LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER,
					       group);
	}

	if (pad->changed_axes & PAD_AXIS_RING2) {
		value = pad_handle_ring(pad, device, ABS_THROTTLE);
		if (send_finger_up)
			value = -1.0;

		group = pad_ring_get_mode_group(pad, 1);
		if (group)
			tablet_pad_notify_ring(base,
					       time,
					       1,
					       value,
					       LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER,
					       group);
	}

	if (pad->changed_axes & PAD_AXIS_STRIP1) {
		value = pad_handle_strip(pad, device, ABS_RX);
		if (send_finger_up)
			value = -1.0;

		group = pad_strip_get_mode_group(pad, 0);
		if (group)
			tablet_pad_notify_strip(base,
						time,
						0,
						value,
						LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER,
						group);
	}

	if (pad->changed_axes & PAD_AXIS_STRIP2) {
		value = pad_handle_strip(pad, device, ABS_RY);
		if (send_finger_up)
			value = -1.0;

		group = pad_strip_get_mode_group(pad, 1);
		if (group)
			tablet_pad_notify_strip(base,
						time,
						1,
						value,
						LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER,
						group);
	}

	pad->changed_axes = PAD_AXIS_NONE;
	pad->have_abs_misc_terminator = false;
}

static void
pad_process_key(struct pad_dispatch *pad,
		struct evdev_device *device,
		struct evdev_event *e,
		uint64_t time)
{
	uint32_t is_press = e->value != 0;

	/* ignore kernel key repeat */
	if (e->value == 2)
		return;

	pad_button_set_down(pad, e->usage, is_press);
}

static inline struct libinput_tablet_pad_mode_group *
pad_button_get_mode_group(struct pad_dispatch *pad, unsigned int button)
{
	struct libinput_tablet_pad_mode_group *group;

	list_for_each(group, &pad->modes.mode_group_list, link) {
		if (libinput_tablet_pad_mode_group_has_button(group, button))
			return group;
	}

	assert(!"Unable to find button mode group\n");

	return NULL;
}

static void
pad_notify_button_mask(struct pad_dispatch *pad,
		       struct evdev_device *device,
		       uint64_t time,
		       const struct button_state *buttons,
		       enum libinput_button_state state)
{
	struct libinput_device *base = &device->base;
	struct libinput_tablet_pad_mode_group *group;
	int32_t code;
	unsigned int i;

	for (i = 0; i < sizeof(buttons->bits); i++) {
		unsigned char buttons_slice = buttons->bits[i];

		code = i * 8;
		while (buttons_slice) {
			int enabled;
			key_or_button_map_t map;

			code++;
			enabled = (buttons_slice & 1);
			buttons_slice >>= 1;

			if (!enabled)
				continue;

			map = pad->button_map[code - 1];
			if (map_is_unmapped(map))
				continue;

			if (map_is_button(map)) {
				int32_t button = map_value(map);

				group = pad_button_get_mode_group(pad, button);
				pad_button_update_mode(group, button, state);
				tablet_pad_notify_button(
					base,
					time,
					pad_button_from_uint32_t(button),
					state,
					group);
			} else if (map_is_key(map)) {
				uint32_t key = map_value(map);

				tablet_pad_notify_key(base,
						      time,
						      key,
						      (enum libinput_key_state)state);
			} else {
				abort();
			}
		}
	}
}

static void
pad_notify_buttons(struct pad_dispatch *pad,
		   struct evdev_device *device,
		   uint64_t time,
		   enum libinput_button_state state)
{
	struct button_state buttons;

	if (state == LIBINPUT_BUTTON_STATE_PRESSED)
		pad_get_buttons_pressed(pad, &buttons);
	else
		pad_get_buttons_released(pad, &buttons);

	pad_notify_button_mask(pad, device, time, &buttons, state);
}

static void
pad_change_to_left_handed(struct evdev_device *device)
{
	struct pad_dispatch *pad = (struct pad_dispatch *)device->dispatch;

	if (device->left_handed.enabled == device->left_handed.want_enabled)
		return;

	if (pad_any_button_down(pad))
		return;

	device->left_handed.enabled = device->left_handed.want_enabled;
}

static void
pad_flush(struct pad_dispatch *pad, struct evdev_device *device, uint64_t time)
{
	if (pad_has_status(pad, PAD_AXES_UPDATED)) {
		pad_check_notify_axes(pad, device, time);
		pad_unset_status(pad, PAD_AXES_UPDATED);
	}

	if (pad_has_status(pad, PAD_BUTTONS_RELEASED)) {
		pad_notify_buttons(pad, device, time, LIBINPUT_BUTTON_STATE_RELEASED);
		pad_unset_status(pad, PAD_BUTTONS_RELEASED);

		pad_change_to_left_handed(device);
	}

	if (pad_has_status(pad, PAD_BUTTONS_PRESSED)) {
		pad_notify_buttons(pad, device, time, LIBINPUT_BUTTON_STATE_PRESSED);
		pad_unset_status(pad, PAD_BUTTONS_PRESSED);
	}

	/* Update state */
	memcpy(&pad->prev_button_state, &pad->button_state, sizeof(pad->button_state));
	pad->dials.dial1 = 0;
	pad->dials.dial2 = 0;
}

static void
pad_process_event(struct evdev_dispatch *dispatch,
		  struct evdev_device *device,
		  struct evdev_event *e,
		  uint64_t time)
{
	struct pad_dispatch *pad = pad_dispatch(dispatch);

	uint16_t type = evdev_event_type(e);
	switch (type) {
	case EV_REL:
		pad_process_relative(pad, device, e, time);
		break;
	case EV_ABS:
		pad_process_absolute(pad, device, e, time);
		break;
	case EV_KEY:
		pad_process_key(pad, device, e, time);
		break;
	case EV_SYN:
		pad_flush(pad, device, time);
		break;
	case EV_MSC:
		/* The EKR sends the serial as MSC_SERIAL, ignore this for
		 * now */
		break;
	default:
		evdev_log_error(device,
				"Unexpected event type %s (%#x)\n",
				libevdev_event_type_get_name(type),
				evdev_usage_as_uint32_t(e->usage));
		break;
	}
}

static void
pad_process(struct evdev_dispatch *dispatch,
	    struct evdev_device *device,
	    struct evdev_frame *frame,
	    uint64_t time)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	for (size_t i = 0; i < nevents; i++) {
		pad_process_event(dispatch, device, &events[i], time);
	}
}

static void
pad_suspend(struct evdev_dispatch *dispatch, struct evdev_device *device)
{
	struct pad_dispatch *pad = pad_dispatch(dispatch);
	struct libinput *libinput = pad_libinput_context(pad);

	for (evdev_usage_t usage = evdev_usage_from(EVDEV_KEY_ESC);
	     evdev_usage_le(usage, EVDEV_KEY_MAX);
	     usage = evdev_usage_next(usage)) {
		if (pad_button_is_down(pad, evdev_usage_code(usage)))
			pad_button_set_down(pad, usage, false);
	}

	pad_flush(pad, device, libinput_now(libinput));
}

static void
pad_destroy(struct evdev_dispatch *dispatch)
{
	struct pad_dispatch *pad = pad_dispatch(dispatch);

	pad_destroy_leds(pad);
	free(pad);
}

static struct evdev_dispatch_interface pad_interface = {
	.process = pad_process,
	.suspend = pad_suspend,
	.remove = NULL,
	.destroy = pad_destroy,
	.device_added = NULL,
	.device_removed = NULL,
	.device_suspended = NULL,
	.device_resumed = NULL,
	.post_added = NULL,
	.touch_arbitration_toggle = NULL,
	.touch_arbitration_update_rect = NULL,
	.get_switch_state = NULL,
};

static bool
pad_init_buttons_from_libwacom(struct pad_dispatch *pad,
			       struct evdev_device *device,
			       WacomDevice *tablet)
{
	bool rc = false;
#ifdef HAVE_LIBWACOM

	if (tablet) {
		int num_buttons = libwacom_get_num_buttons(tablet);
		int map = 0;
		for (int i = 0; i < num_buttons; i++) {
			unsigned int code;

			code = libwacom_get_button_evdev_code(tablet, 'A' + i);
			if (code == 0)
				continue;

			map_set_button_map(pad->button_map[code], map++);
		}

		pad->nbuttons = map;
		rc = true;
	}
#endif
	return rc;
}

static void
pad_init_buttons_from_kernel(struct pad_dispatch *pad, struct evdev_device *device)
{
	unsigned int code;
	int map = 0;

	/* we match wacom_report_numbered_buttons() from the kernel */
	for (code = BTN_0; code < BTN_0 + 10; code++) {
		if (libevdev_has_event_code(device->evdev, EV_KEY, code))
			map_set_button_map(pad->button_map[code], map++);
	}

	for (code = BTN_BASE; code < BTN_BASE + 2; code++) {
		if (libevdev_has_event_code(device->evdev, EV_KEY, code))
			map_set_button_map(pad->button_map[code], map++);
	}

	for (code = BTN_A; code < BTN_A + 6; code++) {
		if (libevdev_has_event_code(device->evdev, EV_KEY, code))
			map_set_button_map(pad->button_map[code], map++);
	}

	for (code = BTN_LEFT; code < BTN_LEFT + 7; code++) {
		if (libevdev_has_event_code(device->evdev, EV_KEY, code))
			map_set_button_map(pad->button_map[code], map++);
	}

	pad->nbuttons = map;
}

static void
pad_init_keys(struct pad_dispatch *pad, struct evdev_device *device)
{
	unsigned int codes[] = {
		KEY_BUTTONCONFIG,
		KEY_ONSCREEN_KEYBOARD,
		KEY_CONTROLPANEL,
	};

	/* Wacom's keys are the only ones we know anything about */
	if (libevdev_get_id_vendor(device->evdev) != VENDOR_ID_WACOM)
		return;

	ARRAY_FOR_EACH(codes, code) {
		if (libevdev_has_event_code(device->evdev, EV_KEY, *code))
			map_set_key_map(pad->button_map[*code], *code);
	}
}

static void
pad_init_buttons(struct pad_dispatch *pad,
		 struct evdev_device *device,
		 WacomDevice *wacom)
{
	size_t i;

	for (i = 0; i < ARRAY_LENGTH(pad->button_map); i++)
		map_init(pad->button_map[i]);

	if (!pad_init_buttons_from_libwacom(pad, device, wacom))
		pad_init_buttons_from_kernel(pad, device);

	pad_init_keys(pad, device);
}

static void
pad_init_left_handed(struct evdev_device *device, WacomDevice *wacom)
{
	bool has_left_handed = true;

#ifdef HAVE_LIBWACOM
	has_left_handed = !wacom || libwacom_is_reversible(wacom);
#endif
	if (has_left_handed)
		evdev_init_left_handed(device, pad_change_to_left_handed);
}

static int
pad_init(struct pad_dispatch *pad, struct evdev_device *device)
{
	int rc = 1;
	struct libinput *li = evdev_libinput_context(device);
	WacomDevice *wacom = NULL;
#ifdef HAVE_LIBWACOM
	WacomDeviceDatabase *db = libinput_libwacom_ref(li);
	if (db) {
		char event_path[64];
		snprintf(event_path,
			 sizeof(event_path),
			 "/dev/input/%s",
			 evdev_device_get_sysname(device));
		wacom = libwacom_new_from_path(db, event_path, WFALLBACK_NONE, NULL);
		if (!wacom) {
			wacom = libwacom_new_from_usbid(
				db,
				evdev_device_get_id_vendor(device),
				evdev_device_get_id_product(device),
				NULL);
		}
		if (!wacom) {
			evdev_log_info(
				device,
				"device \"%s\" (%04x:%04x) is not known to libwacom\n",
				evdev_device_get_name(device),
				evdev_device_get_id_vendor(device),
				evdev_device_get_id_product(device));
		}
	}
#endif

	pad->base.dispatch_type = DISPATCH_TABLET_PAD;
	pad->base.interface = &pad_interface;
	pad->device = device;
	pad->status = PAD_NONE;
	pad->changed_axes = PAD_AXIS_NONE;
	ratelimit_init(&pad->modes.group_not_found, h2us(1), 3);

	/* We expect the kernel to either give us both axes as hires or neither.
	 * Getting one is a kernel bug we don't need to care about */
	pad->dials.has_hires_dial =
		libevdev_has_event_code(device->evdev, EV_REL, REL_WHEEL_HI_RES) ||
		libevdev_has_event_code(device->evdev, EV_REL, REL_HWHEEL_HI_RES);

	if (libevdev_has_event_code(device->evdev, EV_REL, REL_WHEEL) &&
	    libevdev_has_event_code(device->evdev, EV_REL, REL_DIAL)) {
		log_bug_libinput(li,
				 "Unsupported combination REL_DIAL and REL_WHEEL\n");
	}

	pad_init_buttons(pad, device, wacom);
	pad_init_left_handed(device, wacom);

	rc = pad_init_leds(pad, device, wacom);

	/* at most 5 "Multiple EV_ABS events" log messages per hour */
	ratelimit_init(&pad->duplicate_abs_limit, s2us(60 * 60), 5);

#ifdef HAVE_LIBWACOM
	if (wacom)
		libwacom_destroy(wacom);
	if (db)
		libinput_libwacom_unref(li);
#endif
	return rc;
}

struct evdev_dispatch *
evdev_tablet_pad_create(struct evdev_device *device)
{
	struct pad_dispatch *pad;

	pad = zalloc(sizeof *pad);

	if (pad_init(pad, device) != 0) {
		pad_destroy(&pad->base);
		return NULL;
	}

	evdev_init_sendevents(device, &pad->base);

	return &pad->base;
}

int
evdev_device_tablet_pad_has_key(struct evdev_device *device, uint32_t code)
{
	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	return libevdev_has_event_code(device->evdev, EV_KEY, code);
}

int
evdev_device_tablet_pad_get_num_buttons(struct evdev_device *device)
{
	struct pad_dispatch *pad = (struct pad_dispatch *)device->dispatch;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	return pad->nbuttons;
}

int
evdev_device_tablet_pad_get_num_dials(struct evdev_device *device)
{
	int ndials = 0;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	if (libevdev_has_event_code(device->evdev, EV_REL, REL_WHEEL) ||
	    libevdev_has_event_code(device->evdev, EV_REL, REL_DIAL)) {
		ndials++;
		if (libevdev_has_event_code(device->evdev, EV_REL, REL_HWHEEL))
			ndials++;
	}

	return ndials;
}

int
evdev_device_tablet_pad_get_num_rings(struct evdev_device *device)
{
	int nrings = 0;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	if (libevdev_has_event_code(device->evdev, EV_ABS, ABS_WHEEL)) {
		nrings++;
		if (libevdev_has_event_code(device->evdev, EV_ABS, ABS_THROTTLE))
			nrings++;
	}

	return nrings;
}

int
evdev_device_tablet_pad_get_num_strips(struct evdev_device *device)
{
	int nstrips = 0;

	if (!(device->seat_caps & EVDEV_DEVICE_TABLET_PAD))
		return -1;

	if (libevdev_has_event_code(device->evdev, EV_ABS, ABS_RX)) {
		nstrips++;
		if (libevdev_has_event_code(device->evdev, EV_ABS, ABS_RY))
			nstrips++;
	}

	return nstrips;
}
