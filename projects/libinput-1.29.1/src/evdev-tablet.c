/*
 * Copyright © 2014 Red Hat, Inc.
 * Copyright © 2014 Lyude Paul
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

#include "evdev-tablet.h"

#ifdef HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#else
typedef void *WacomStylus;
#endif

enum notify {
	DONT_NOTIFY,
	DO_NOTIFY,
};

#define tablet_set_status(tablet_, s_) (tablet_)->status |= (s_)
#define tablet_unset_status(tablet_, s_) (tablet_)->status &= ~(s_)
#define tablet_has_status(tablet_, s_) (!!((tablet_)->status & (s_)))

static inline void
tablet_get_pressed_buttons(struct tablet_dispatch *tablet, struct button_state *buttons)
{
	size_t i;
	const struct button_state *state = &tablet->button_state,
				  *prev_state = &tablet->prev_button_state;

	for (i = 0; i < sizeof(buttons->bits); i++)
		buttons->bits[i] = state->bits[i] & ~(prev_state->bits[i]);
}

static inline void
tablet_get_released_buttons(struct tablet_dispatch *tablet,
			    struct button_state *buttons)
{
	size_t i;
	const struct button_state *state = &tablet->button_state,
				  *prev_state = &tablet->prev_button_state;

	for (i = 0; i < sizeof(buttons->bits); i++)
		buttons->bits[i] = prev_state->bits[i] & ~(state->bits[i]);
}

static struct libinput_tablet_tool_pressure_threshold *
tablet_tool_get_threshold(struct tablet_dispatch *tablet,
			  struct libinput_tablet_tool *tool)
{
	return &tool->pressure.threshold;
}

/* Merge the previous state with the current one so all buttons look like
 * they just got pressed in this frame */
static inline void
tablet_force_button_presses(struct tablet_dispatch *tablet)
{
	struct button_state *state = &tablet->button_state,
			    *prev_state = &tablet->prev_button_state;
	size_t i;

	for (i = 0; i < sizeof(state->bits); i++) {
		state->bits[i] = state->bits[i] | prev_state->bits[i];
		prev_state->bits[i] = 0;
	}
}

static inline size_t
tablet_history_size(const struct tablet_dispatch *tablet)
{
	return tablet->history.size;
}

static inline void
tablet_history_reset(struct tablet_dispatch *tablet)
{
	tablet->history.count = 0;
}

static inline void
tablet_history_push(struct tablet_dispatch *tablet, const struct tablet_axes *axes)
{
	unsigned int index = (tablet->history.index + 1) % tablet_history_size(tablet);

	tablet->history.samples[index] = *axes;
	tablet->history.index = index;
	tablet->history.count =
		min(tablet->history.count + 1, tablet_history_size(tablet));

	if (tablet->history.count < tablet_history_size(tablet))
		tablet_history_push(tablet, axes);
}

/**
 * Return a previous axis state, where index of 0 means "most recent", 1 is
 * "one before most recent", etc.
 */
static inline const struct tablet_axes *
tablet_history_get(const struct tablet_dispatch *tablet, unsigned int index)
{
	size_t sz = tablet_history_size(tablet);

	assert(index < sz);
	assert(index < tablet->history.count);

	index = (tablet->history.index + sz - index) % sz;
	return &tablet->history.samples[index];
}

static inline void
tablet_reset_changed_axes(struct tablet_dispatch *tablet)
{
	memset(tablet->changed_axes, 0, sizeof(tablet->changed_axes));
}

static bool
tablet_device_has_axis(struct tablet_dispatch *tablet,
		       enum libinput_tablet_tool_axis axis)
{
	struct libevdev *evdev = tablet->device->evdev;
	bool has_axis = false;
	unsigned int code;

	if (axis == LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z) {
		has_axis = (libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_MOUSE) &&
			    libevdev_has_event_code(evdev, EV_ABS, ABS_TILT_X) &&
			    libevdev_has_event_code(evdev, EV_ABS, ABS_TILT_Y));
		code = axis_to_evcode(axis);
		has_axis |= libevdev_has_event_code(evdev, EV_ABS, code);
	} else if (axis == LIBINPUT_TABLET_TOOL_AXIS_REL_WHEEL) {
		has_axis = libevdev_has_event_code(evdev, EV_REL, REL_WHEEL);
	} else {
		code = axis_to_evcode(axis);
		has_axis = libevdev_has_event_code(evdev, EV_ABS, code);
	}

	return has_axis;
}

static inline bool
tablet_filter_axis_fuzz(const struct tablet_dispatch *tablet,
			const struct evdev_device *device,
			const struct evdev_event *e,
			enum libinput_tablet_tool_axis axis)
{
	int delta, fuzz;
	int current, previous;

	previous = tablet->prev_value[axis];
	current = e->value;
	delta = previous - current;

	fuzz = libevdev_get_abs_fuzz(device->evdev, evdev_usage_code(e->usage));

	/* ABS_DISTANCE doesn't have have fuzz set and causes continuous
	 * updates for the cursor/lens tools. Add a minimum fuzz of 2, same
	 * as the xf86-input-wacom driver
	 */
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_ABS_DISTANCE:
		fuzz = max(2, fuzz);
		break;
	default:
		break;
	}

	return abs(delta) <= fuzz;
}

static void
tablet_process_absolute(struct tablet_dispatch *tablet,
			struct evdev_device *device,
			struct evdev_event *e,
			uint64_t time)
{
	enum libinput_tablet_tool_axis axis;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_ABS_X:
	case EVDEV_ABS_Y:
	case EVDEV_ABS_Z:
	case EVDEV_ABS_PRESSURE:
	case EVDEV_ABS_TILT_X:
	case EVDEV_ABS_TILT_Y:
	case EVDEV_ABS_DISTANCE:
	case EVDEV_ABS_WHEEL:
		axis = evdev_usage_to_axis(e->usage);
		if (axis == LIBINPUT_TABLET_TOOL_AXIS_NONE) {
			evdev_log_bug_libinput(device,
					       "Invalid ABS event code %#x\n",
					       evdev_usage_as_uint32_t(e->usage));
			break;
		}

		tablet->prev_value[axis] = tablet->current_value[axis];
		if (tablet_filter_axis_fuzz(tablet, device, e, axis))
			break;

		tablet->current_value[axis] = e->value;
		set_bit(tablet->changed_axes, axis);
		tablet_set_status(tablet, TABLET_AXES_UPDATED);
		break;
	/* tool_id is the identifier for the tool we can use in libwacom
	 * to identify it (if we have one anyway) */
	case EVDEV_ABS_MISC:
		tablet->current_tool.id = e->value;
		break;
	/* Intuos 3 strip data. Should only happen on the Pad device, not on
	   the Pen device. */
	case EVDEV_ABS_RX:
	case EVDEV_ABS_RY:
	/* Only on the 4D mouse (Intuos2), obsolete */
	case EVDEV_ABS_RZ:
	/* Only on the 4D mouse (Intuos2), obsolete.
	   The 24HD sends ABS_THROTTLE on the Pad device for the second
	   wheel but we shouldn't get here on kernel >= 3.17.
	   */
	case EVDEV_ABS_THROTTLE:
	default:
		evdev_log_info(device,
			       "Unhandled ABS event code %#x\n",
			       evdev_usage_as_uint32_t(e->usage));
		break;
	}
}

static inline int
axis_range_percentage(const struct input_absinfo *a, double percent)
{
	return (a->maximum - a->minimum) * percent / 100.0 + a->minimum;
}

static void
tablet_change_area(struct evdev_device *device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);

	if (memcmp(&tablet->area.rect,
		   &tablet->area.want_rect,
		   sizeof(tablet->area.rect)) == 0)
		return;

	if (!tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY))
		return;

	tablet->area.rect = tablet->area.want_rect;

	evdev_log_debug(device,
			"tablet-area: area is %.2f/%.2f - %.2f/%.2f\n",
			tablet->area.rect.x1,
			tablet->area.rect.y1,
			tablet->area.rect.x2,
			tablet->area.rect.y2);

	const struct input_absinfo *absx = device->abs.absinfo_x;
	const struct input_absinfo *absy = device->abs.absinfo_y;
	tablet->area.x.minimum =
		axis_range_percentage(absx, tablet->area.rect.x1 * 100);
	tablet->area.x.maximum =
		axis_range_percentage(absx, tablet->area.rect.x2 * 100);
	tablet->area.y.minimum =
		axis_range_percentage(absy, tablet->area.rect.y1 * 100);
	tablet->area.y.maximum =
		axis_range_percentage(absy, tablet->area.rect.y2 * 100);
}

static void
tablet_apply_rotation(struct evdev_device *device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);

	if (tablet->rotation.rotate == tablet->rotation.want_rotate)
		return;

	if (!tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY))
		return;

	tablet->rotation.rotate = tablet->rotation.want_rotate;

	evdev_log_debug(device,
			"tablet-rotation: rotation is %s\n",
			tablet->rotation.rotate ? "on" : "off");
}

static void
tablet_change_rotation(struct evdev_device *device, enum notify notify)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);
	struct evdev_device *touch_device = tablet->touch_device;
	struct evdev_dispatch *dispatch;
	bool tablet_is_left, touchpad_is_left;

	tablet_is_left = tablet->device->left_handed.enabled;
	touchpad_is_left = tablet->rotation.touch_device_left_handed_state;

	tablet->rotation.want_rotate = tablet_is_left || touchpad_is_left;
	tablet_apply_rotation(device);

	if (notify == DO_NOTIFY && touch_device) {
		bool enable = device->left_handed.want_enabled;

		dispatch = touch_device->dispatch;
		if (dispatch->interface->left_handed_toggle)
			dispatch->interface->left_handed_toggle(dispatch,
								touch_device,
								enable);
	}
}

static void
tablet_change_to_left_handed(struct evdev_device *device)
{
	if (device->left_handed.enabled == device->left_handed.want_enabled)
		return;

	device->left_handed.enabled = device->left_handed.want_enabled;

	tablet_change_rotation(device, DO_NOTIFY);
}

static void
tablet_update_tool(struct tablet_dispatch *tablet,
		   struct evdev_device *device,
		   enum libinput_tablet_tool_type tool,
		   bool enabled)
{
	assert(tool != LIBINPUT_TOOL_NONE);

	if (enabled) {
		tablet->current_tool.type = tool;
		tablet_set_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY);
		tablet_unset_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY);
	} else if (!tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY)) {
		tablet_set_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY);
	}
}

static inline double
normalize_slider(const struct input_absinfo *absinfo)
{
	return absinfo_normalize(absinfo) * 2 - 1;
}

static inline double
normalize_distance(const struct input_absinfo *absinfo)
{
	return absinfo_normalize(absinfo);
}

static inline double
normalize_pressure(struct libinput_tablet_tool_pressure_threshold *threshold,
		   int abs_value)
{
	/**
	 * Note: the upper threshold takes the offset into account so that
	 *            |- 4% -|
	 * min |------X------X-------------------------| max
	 *            |      |
	 *            |      + upper threshold / tip trigger
	 *            +- offset and lower threshold
	 *
	 * The axis is scaled into the range [lower, max] so that the lower
	 * threshold is 0 pressure.
	 */
	struct input_absinfo abs = threshold->abs_pressure;

	abs.minimum = threshold->threshold.lower;

	return absinfo_normalize_value(&abs, abs_value);
}

static inline double
adjust_tilt(const struct input_absinfo *absinfo)
{
	double value = absinfo_normalize(absinfo);
	const int WACOM_MAX_DEGREES = 64;

	/* If resolution is nonzero, it's in units/radian. But require
	 * a min/max less/greater than zero so we can assume 0 is the
	 * center */
	if (absinfo->resolution != 0 && absinfo->maximum > 0 && absinfo->minimum < 0) {
		value = rad2deg((double)absinfo->value / absinfo->resolution);
	} else {
		/* Wacom supports physical [-64, 64] degrees, so map to that by
		 * default. If other tablets have a different physical range or
		 * nonzero physical offsets, they need extra treatment
		 * here.
		 */
		/* Map to the (-1, 1) range */
		value = (value * 2) - 1;
		value *= WACOM_MAX_DEGREES;
	}

	return value;
}

static inline int32_t
invert_axis(const struct input_absinfo *absinfo)
{
	return absinfo->maximum - (absinfo->value - absinfo->minimum);
}

static void
convert_tilt_to_rotation(struct tablet_dispatch *tablet)
{
	const int offset = 5;
	double x, y;
	double angle = 0.0;

	/* Wacom Intuos 4, 5, Pro mouse calculates rotation from the x/y tilt
	   values. The device has a 175 degree CCW hardware offset but since we use
	   atan2 the effective offset is just 5 degrees.
	   */
	x = tablet->axes.tilt.x;
	y = tablet->axes.tilt.y;

	/* atan2 is CCW, we want CW -> negate x */
	if (x || y)
		angle = rad2deg(atan2(-x, y));

	angle = fmod(360 + angle - offset, 360);

	tablet->axes.rotation = angle;
	set_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
}

static double
convert_to_degrees(const struct input_absinfo *absinfo, double offset)
{
	/* range is [0, 360[, i.e. range + 1 */
	double value = (absinfo->value - absinfo->minimum) / absinfo_range(absinfo);

	return fmod(value * 360.0 + offset, 360.0);
}

static inline double
normalize_wheel(struct tablet_dispatch *tablet, int value)
{
	struct evdev_device *device = tablet->device;

	return value * device->scroll.wheel_click_angle.x;
}

static bool
is_inside_area(struct tablet_dispatch *tablet,
	       const struct device_coords *point,
	       double normalized_margin)
{
	if (tablet->area.rect.x1 == 0.0 && tablet->area.rect.x2 == 1.0 &&
	    tablet->area.rect.y1 == 0.0 && tablet->area.rect.y2 == 1.0)
		return true;

	assert(normalized_margin > 0.0);
	assert(normalized_margin <= 1.0);

	int xmargin =
		(tablet->area.x.maximum - tablet->area.x.minimum) * normalized_margin;
	int ymargin =
		(tablet->area.y.maximum - tablet->area.y.minimum) * normalized_margin;

	return (point->x >= tablet->area.x.minimum - xmargin &&
		point->x <= tablet->area.x.maximum + xmargin &&
		point->y >= tablet->area.y.minimum - ymargin &&
		point->y <= tablet->area.y.maximum + ymargin);
}

static void
apply_tablet_area(struct tablet_dispatch *tablet,
		  struct evdev_device *device,
		  struct device_coords *point)
{
	if (tablet->area.rect.x1 == 0.0 && tablet->area.rect.x2 == 1.0 &&
	    tablet->area.rect.y1 == 0.0 && tablet->area.rect.y2 == 1.0)
		return;

	/* The point is somewhere on the tablet in device coordinates,
	 * but we need it relative to the x/y offset.
	 * So clip it first, then offset it to our area min/max.
	 *
	 * Right now we're just clipping, we don't completely
	 * ignore events. What we should do is ignore events outside
	 * altogether and generate prox in/out events when we actually
	 * enter the area.
	 */
	point->x = min(point->x, tablet->area.x.maximum);
	point->y = min(point->y, tablet->area.y.maximum);

	point->x = max(point->x, tablet->area.x.minimum);
	point->y = max(point->y, tablet->area.y.minimum);
}

static inline void
tablet_update_xy(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	const struct input_absinfo *absinfo;
	int value;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_X) ||
	    !libevdev_has_event_code(device->evdev, EV_ABS, ABS_Y))
		return;

	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_X) ||
	    bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_Y)) {
		absinfo = device->abs.absinfo_x;

		if (tablet->rotation.rotate)
			value = invert_axis(absinfo);
		else
			value = absinfo->value;

		tablet->axes.point.x = value;

		absinfo = device->abs.absinfo_y;

		if (tablet->rotation.rotate)
			value = invert_axis(absinfo);
		else
			value = absinfo->value;

		tablet->axes.point.y = value;

		/* calibration and area are currently mutually exclusive so
		 * one of those is a noop */
		evdev_transform_absolute(device, &tablet->axes.point);
		apply_tablet_area(tablet, device, &tablet->axes.point);
	}
}

static inline struct normalized_coords
tablet_tool_process_delta(struct tablet_dispatch *tablet,
			  struct libinput_tablet_tool *tool,
			  const struct evdev_device *device,
			  struct tablet_axes *axes,
			  uint64_t time)
{
	const struct normalized_coords zero = { 0.0, 0.0 };
	struct device_coords delta = { 0, 0 };
	struct device_float_coords accel;

	/* When tool contact changes, we probably got a cursor jump. Don't
	   try to calculate a delta for that event */
	if (!tablet_has_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY) &&
	    !tablet_has_status(tablet, TABLET_TOOL_ENTERING_CONTACT) &&
	    !tablet_has_status(tablet, TABLET_TOOL_LEAVING_CONTACT) &&
	    (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_X) ||
	     bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_Y))) {
		delta.x = axes->point.x - tablet->last_smooth_point.x;
		delta.y = axes->point.y - tablet->last_smooth_point.y;
	}

	if (axes->point.x != tablet->last_smooth_point.x)
		set_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_X);
	if (axes->point.y != tablet->last_smooth_point.y)
		set_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_Y);

	tablet->last_smooth_point = axes->point;

	accel.x = 1.0 * delta.x;
	accel.y = 1.0 * delta.y;

	if (device_float_is_zero(accel))
		return zero;

	return filter_dispatch(device->pointer.filter, &accel, tool, time);
}

static inline void
tablet_update_pressure(struct tablet_dispatch *tablet,
		       struct evdev_device *device,
		       struct libinput_tablet_tool *tool)
{
	const struct input_absinfo *abs =
		libevdev_get_abs_info(device->evdev, ABS_PRESSURE);
	if (!abs)
		return;

	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE)) {
		struct libinput_tablet_tool_pressure_threshold *threshold =
			tablet_tool_get_threshold(tablet, tool);
		tablet->axes.pressure = normalize_pressure(threshold, abs->value);
	}
}

static inline void
tablet_update_distance(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	const struct input_absinfo *absinfo;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_DISTANCE))
		return;

	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_DISTANCE)) {
		absinfo = libevdev_get_abs_info(device->evdev, ABS_DISTANCE);
		tablet->axes.distance = normalize_distance(absinfo);
	}
}

static inline void
tablet_update_slider(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	const struct input_absinfo *absinfo;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_WHEEL))
		return;

	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_SLIDER)) {
		absinfo = libevdev_get_abs_info(device->evdev, ABS_WHEEL);
		tablet->axes.slider = normalize_slider(absinfo);
	}
}

static inline void
tablet_update_tilt(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	const struct input_absinfo *absinfo;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_TILT_X) ||
	    !libevdev_has_event_code(device->evdev, EV_ABS, ABS_TILT_Y))
		return;

	/* mouse rotation resets tilt to 0 so always fetch both axes if
	 * either has changed */
	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_X) ||
	    bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y)) {

		absinfo = libevdev_get_abs_info(device->evdev, ABS_TILT_X);
		tablet->axes.tilt.x = adjust_tilt(absinfo);

		absinfo = libevdev_get_abs_info(device->evdev, ABS_TILT_Y);
		tablet->axes.tilt.y = adjust_tilt(absinfo);

		if (device->left_handed.enabled) {
			tablet->axes.tilt.x *= -1;
			tablet->axes.tilt.y *= -1;
		}
	}
}

static inline void
tablet_update_artpen_rotation(struct tablet_dispatch *tablet,
			      struct evdev_device *device)
{
	const struct input_absinfo *absinfo;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_Z))
		return;

	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z)) {
		absinfo = libevdev_get_abs_info(device->evdev, ABS_Z);
		/* artpen has 0 with buttons pointing east */
		tablet->axes.rotation = convert_to_degrees(absinfo, 90);
	}
}

static inline void
tablet_update_mouse_rotation(struct tablet_dispatch *tablet,
			     struct evdev_device *device)
{
	if (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_X) ||
	    bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y)) {
		convert_tilt_to_rotation(tablet);
	}
}

static inline void
tablet_update_rotation(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	/* We must check ROTATION_Z after TILT_X/Y so that the tilt axes are
	 * already normalized and set if we have the mouse/lens tool */
	if (tablet->current_tool.type == LIBINPUT_TABLET_TOOL_TYPE_MOUSE ||
	    tablet->current_tool.type == LIBINPUT_TABLET_TOOL_TYPE_LENS) {
		tablet_update_mouse_rotation(tablet, device);
		clear_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_X);
		clear_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y);
		tablet->axes.tilt.x = 0;
		tablet->axes.tilt.y = 0;

		/* tilt is already converted to left-handed, so mouse
		 * rotation is converted to left-handed automatically */
	} else {

		tablet_update_artpen_rotation(tablet, device);

		if (device->left_handed.enabled) {
			double r = tablet->axes.rotation;
			tablet->axes.rotation = fmod(180 + r, 360);
		}
	}
}

static inline void
tablet_update_wheel(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	int a;

	a = LIBINPUT_TABLET_TOOL_AXIS_REL_WHEEL;
	if (bit_is_set(tablet->changed_axes, a)) {
		/* tablet->axes.wheel_discrete is already set */
		tablet->axes.wheel =
			normalize_wheel(tablet, tablet->axes.wheel_discrete);
	} else {
		tablet->axes.wheel = 0;
		tablet->axes.wheel_discrete = 0;
	}
}

static void
tablet_smoothen_axes(const struct tablet_dispatch *tablet, struct tablet_axes *axes)
{
	size_t i;
	size_t count = tablet_history_size(tablet);
	struct tablet_axes smooth = { 0 };

	for (i = 0; i < count; i++) {
		const struct tablet_axes *a = tablet_history_get(tablet, i);

		smooth.point.x += a->point.x;
		smooth.point.y += a->point.y;

		smooth.tilt.x += a->tilt.x;
		smooth.tilt.y += a->tilt.y;
	}

	axes->point.x = smooth.point.x / count;
	axes->point.y = smooth.point.y / count;

	axes->tilt.x = smooth.tilt.x / count;
	axes->tilt.y = smooth.tilt.y / count;
}

static bool
tablet_check_notify_axes(struct tablet_dispatch *tablet,
			 struct evdev_device *device,
			 struct libinput_tablet_tool *tool,
			 struct tablet_axes *axes_out,
			 uint64_t time)
{
	struct tablet_axes axes = { 0 };
	const char tmp[sizeof(tablet->changed_axes)] = { 0 };
	bool rc = false;

	if (memcmp(tmp, tablet->changed_axes, sizeof(tmp)) == 0) {
		axes = tablet->axes;
		goto out;
	}

	tablet_update_xy(tablet, device);
	tablet_update_pressure(tablet, device, tool);
	tablet_update_distance(tablet, device);
	tablet_update_slider(tablet, device);
	tablet_update_tilt(tablet, device);
	tablet_update_wheel(tablet, device);
	/* We must check ROTATION_Z after TILT_X/Y so that the tilt axes are
	 * already normalized and set if we have the mouse/lens tool */
	tablet_update_rotation(tablet, device);

	axes.point = tablet->axes.point;
	axes.pressure = tablet->axes.pressure;
	axes.distance = tablet->axes.distance;
	axes.slider = tablet->axes.slider;
	axes.tilt = tablet->axes.tilt;
	axes.wheel = tablet->axes.wheel;
	axes.wheel_discrete = tablet->axes.wheel_discrete;
	axes.rotation = tablet->axes.rotation;

	rc = true;

out:
	/* The tool position often jumps to a different spot when contact changes.
	 * If tool contact changes, clear the history to prevent axis smoothing
	 * from trying to average over the spatial discontinuity. */
	if (tablet_has_status(tablet, TABLET_TOOL_ENTERING_CONTACT) ||
	    tablet_has_status(tablet, TABLET_TOOL_LEAVING_CONTACT)) {
		tablet_history_reset(tablet);
	}

	tablet_history_push(tablet, &tablet->axes);
	tablet_smoothen_axes(tablet, &axes);

	/* The delta relies on the last *smooth* point, so we do it last */
	axes.delta = tablet_tool_process_delta(tablet, tool, device, &axes, time);

	*axes_out = axes;

	return rc;
}

static void
tablet_update_button(struct tablet_dispatch *tablet,
		     evdev_usage_t usage,
		     uint32_t enable)
{
	switch (evdev_usage_enum(usage)) {
	case EVDEV_BTN_LEFT:
	case EVDEV_BTN_RIGHT:
	case EVDEV_BTN_MIDDLE:
	case EVDEV_BTN_SIDE:
	case EVDEV_BTN_EXTRA:
	case EVDEV_BTN_FORWARD:
	case EVDEV_BTN_BACK:
	case EVDEV_BTN_TASK:
	case EVDEV_BTN_STYLUS:
	case EVDEV_BTN_STYLUS2:
	case EVDEV_BTN_STYLUS3:
		break;
	default:
		evdev_log_info(tablet->device,
			       "Unhandled button %s (%#x)\n",
			       evdev_usage_code_name(usage),
			       evdev_usage_as_uint32_t(usage));
		return;
	}

	if (enable) {
		set_bit(tablet->button_state.bits, evdev_usage_code(usage));
		tablet_set_status(tablet, TABLET_BUTTONS_PRESSED);
	} else {
		clear_bit(tablet->button_state.bits, evdev_usage_code(usage));
		tablet_set_status(tablet, TABLET_BUTTONS_RELEASED);
	}
}

static inline enum libinput_tablet_tool_type
tablet_evdev_usage_to_tool(evdev_usage_t usage)
{
	enum libinput_tablet_tool_type type;

	switch (evdev_usage_enum(usage)) {
	case EVDEV_BTN_TOOL_PEN:
		type = LIBINPUT_TABLET_TOOL_TYPE_PEN;
		break;
	case EVDEV_BTN_TOOL_RUBBER:
		type = LIBINPUT_TABLET_TOOL_TYPE_ERASER;
		break;
	case EVDEV_BTN_TOOL_BRUSH:
		type = LIBINPUT_TABLET_TOOL_TYPE_BRUSH;
		break;
	case EVDEV_BTN_TOOL_PENCIL:
		type = LIBINPUT_TABLET_TOOL_TYPE_PENCIL;
		break;
	case EVDEV_BTN_TOOL_AIRBRUSH:
		type = LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH;
		break;
	case EVDEV_BTN_TOOL_MOUSE:
		type = LIBINPUT_TABLET_TOOL_TYPE_MOUSE;
		break;
	case EVDEV_BTN_TOOL_LENS:
		type = LIBINPUT_TABLET_TOOL_TYPE_LENS;
		break;
	default:
		abort();
	}

	return type;
}

static void
tablet_process_key(struct tablet_dispatch *tablet,
		   struct evdev_device *device,
		   struct evdev_event *e,
		   uint64_t time)
{
	enum libinput_tablet_tool_type type;

	/* ignore kernel key repeat */
	if (e->value == 2)
		return;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_BTN_TOOL_FINGER:
		evdev_log_bug_libinput(device,
				       "Invalid tool 'finger' on tablet interface\n");
		break;
	case EVDEV_BTN_TOOL_PEN:
	case EVDEV_BTN_TOOL_RUBBER:
	case EVDEV_BTN_TOOL_BRUSH:
	case EVDEV_BTN_TOOL_PENCIL:
	case EVDEV_BTN_TOOL_AIRBRUSH:
	case EVDEV_BTN_TOOL_MOUSE:
	case EVDEV_BTN_TOOL_LENS:
		type = tablet_evdev_usage_to_tool(e->usage);
		tablet_set_status(tablet, TABLET_TOOL_UPDATED);
		if (e->value)
			tablet->tool_state |= bit(type);
		else
			tablet->tool_state &= ~bit(type);
		break;
	case EVDEV_BTN_TOUCH:
		if (!bit_is_set(tablet->axis_caps,
				LIBINPUT_TABLET_TOOL_AXIS_PRESSURE)) {
			if (e->value)
				tablet_set_status(tablet, TABLET_TOOL_ENTERING_CONTACT);
			else
				tablet_set_status(tablet, TABLET_TOOL_LEAVING_CONTACT);
		}
		break;
	default:
		tablet_update_button(tablet, e->usage, e->value);
		break;
	}
}

static void
tablet_process_relative(struct tablet_dispatch *tablet,
			struct evdev_device *device,
			struct evdev_event *e,
			uint64_t time)
{
	enum libinput_tablet_tool_axis axis;

	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_REL_WHEEL:
		axis = evdev_usage_to_axis(e->usage);
		if (axis == LIBINPUT_TABLET_TOOL_AXIS_NONE) {
			evdev_log_bug_libinput(device,
					       "Invalid ABS event code %#x\n",
					       evdev_usage_as_uint32_t(e->usage));
			break;
		}
		set_bit(tablet->changed_axes, axis);
		tablet->axes.wheel_discrete = -1 * e->value;
		tablet_set_status(tablet, TABLET_AXES_UPDATED);
		break;
	default:
		evdev_log_info(device,
			       "Unhandled relative axis %s (%#x)\n",
			       evdev_event_get_code_name(e),
			       evdev_usage_as_uint32_t(e->usage));
		return;
	}
}

static void
tablet_process_misc(struct tablet_dispatch *tablet,
		    struct evdev_device *device,
		    struct evdev_event *e,
		    uint64_t time)
{
	switch (evdev_usage_enum(e->usage)) {
	case EVDEV_MSC_SERIAL:
		if (e->value != -1)
			tablet->current_tool.serial = e->value;

		break;
	case EVDEV_MSC_SCAN:
		break;
	default:
		evdev_log_info(device,
			       "Unhandled MSC event code %s (%#x)\n",
			       evdev_event_get_code_name(e),
			       evdev_usage_as_uint32_t(e->usage));
		break;
	}
}

static inline void
copy_axis_cap(const struct tablet_dispatch *tablet,
	      struct libinput_tablet_tool *tool,
	      enum libinput_tablet_tool_axis axis)
{
	if (bit_is_set(tablet->axis_caps, axis))
		set_bit(tool->axis_caps, axis);
}

static inline void
copy_button_cap(const struct tablet_dispatch *tablet,
		struct libinput_tablet_tool *tool,
		uint32_t button)
{
	struct libevdev *evdev = tablet->device->evdev;
	if (libevdev_has_event_code(evdev, EV_KEY, button))
		set_bit(tool->buttons, button);
}

static inline bool
tool_set_bits_from_libwacom(const struct tablet_dispatch *tablet,
			    struct libinput_tablet_tool *tool,
			    const WacomStylus *s)
{
	bool rc = false;
#ifdef HAVE_LIBWACOM
	int code;
	WacomStylusType type;
	WacomAxisTypeFlags axes;

	if (!s)
		return rc;

	type = libwacom_stylus_get_type(s);
	if (type == WSTYLUS_PUCK) {
		for (code = BTN_LEFT;
		     code < BTN_LEFT + libwacom_stylus_get_num_buttons(s);
		     code++)
			copy_button_cap(tablet, tool, code);
	} else {
		if (libwacom_stylus_get_num_buttons(s) >= 3)
			copy_button_cap(tablet, tool, BTN_STYLUS3);
		if (libwacom_stylus_get_num_buttons(s) >= 2)
			copy_button_cap(tablet, tool, BTN_STYLUS2);
		if (libwacom_stylus_get_num_buttons(s) >= 1)
			copy_button_cap(tablet, tool, BTN_STYLUS);
	}

	if (libwacom_stylus_has_wheel(s))
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_REL_WHEEL);

	axes = libwacom_stylus_get_axes(s);

	if (axes & WACOM_AXIS_TYPE_TILT) {
		/* tilt on the puck is converted to rotation */
		if (type == WSTYLUS_PUCK) {
			set_bit(tool->axis_caps, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
		} else {
			copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_TILT_X);
			copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y);
		}
	}
	if (axes & WACOM_AXIS_TYPE_ROTATION_Z)
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
	if (axes & WACOM_AXIS_TYPE_DISTANCE)
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_DISTANCE);
	if (axes & WACOM_AXIS_TYPE_SLIDER)
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_SLIDER);
	if (axes & WACOM_AXIS_TYPE_PRESSURE)
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);

	rc = true;
#endif
	return rc;
}

static void
tool_set_bits(const struct tablet_dispatch *tablet,
	      struct libinput_tablet_tool *tool,
	      const WacomStylus *s)
{
	enum libinput_tablet_tool_type type = tool->type;

	copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_X);
	copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_Y);

	if (s && tool_set_bits_from_libwacom(tablet, tool, s))
		return;

	/* If we don't have libwacom, we simply copy any axis we have on the
	   tablet onto the tool. Except we know that mice only have rotation
	   anyway.
	 */
	switch (type) {
	case LIBINPUT_TABLET_TOOL_TYPE_PEN:
	case LIBINPUT_TABLET_TOOL_TYPE_ERASER:
	case LIBINPUT_TABLET_TOOL_TYPE_PENCIL:
	case LIBINPUT_TABLET_TOOL_TYPE_BRUSH:
	case LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH:
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_DISTANCE);
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_TILT_X);
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y);
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_SLIDER);

		/* Rotation is special, it can be either ABS_Z or
		 * BTN_TOOL_MOUSE+ABS_TILT_X/Y. Aiptek tablets have
		 * mouse+tilt (and thus rotation), but they do not have
		 * ABS_Z. So let's not copy the axis bit if we don't have
		 * ABS_Z, otherwise we try to get the value from it later on
		 * proximity in and go boom because the absinfo isn't there.
		 */
		if (libevdev_has_event_code(tablet->device->evdev, EV_ABS, ABS_Z))
			copy_axis_cap(tablet,
				      tool,
				      LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_MOUSE:
	case LIBINPUT_TABLET_TOOL_TYPE_LENS:
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
		copy_axis_cap(tablet, tool, LIBINPUT_TABLET_TOOL_AXIS_REL_WHEEL);
		break;
	default:
		break;
	}

	/* If we don't have libwacom, copy all pen-related buttons from the
	   tablet vs all mouse-related buttons */
	switch (type) {
	case LIBINPUT_TABLET_TOOL_TYPE_PEN:
	case LIBINPUT_TABLET_TOOL_TYPE_BRUSH:
	case LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH:
	case LIBINPUT_TABLET_TOOL_TYPE_PENCIL:
	case LIBINPUT_TABLET_TOOL_TYPE_ERASER:
		copy_button_cap(tablet, tool, BTN_STYLUS);
		copy_button_cap(tablet, tool, BTN_STYLUS2);
		copy_button_cap(tablet, tool, BTN_STYLUS3);
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_MOUSE:
	case LIBINPUT_TABLET_TOOL_TYPE_LENS:
		copy_button_cap(tablet, tool, BTN_LEFT);
		copy_button_cap(tablet, tool, BTN_MIDDLE);
		copy_button_cap(tablet, tool, BTN_RIGHT);
		copy_button_cap(tablet, tool, BTN_SIDE);
		copy_button_cap(tablet, tool, BTN_EXTRA);
		break;
	default:
		break;
	}
}

static bool
tablet_get_quirked_pressure_thresholds(struct tablet_dispatch *tablet, int *hi, int *lo)
{
	struct evdev_device *device = tablet->device;
	struct quirk_range r;
	bool status = false;

	/* Note: the quirk term "range" refers to the hi/lo settings, not the
	 * full available range for the pressure axis */
	_unref_(quirks) *q = libinput_device_get_quirks(&device->base);
	if (q && quirks_get_range(q, QUIRK_ATTR_PRESSURE_RANGE, &r)) {
		if (r.lower < r.upper) {
			*hi = r.lower;
			*lo = r.upper;
			status = true;
		} else {
			evdev_log_info(device,
				       "Invalid pressure range, using defaults\n");
		}
	}

	return status;
}

static void
apply_pressure_range_configuration(struct tablet_dispatch *tablet,
				   struct libinput_tablet_tool *tool,
				   bool force_update)
{
	struct evdev_device *device = tablet->device;

	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_PRESSURE) ||
	    (!force_update &&
	     tool->pressure.range.min == tool->pressure.wanted_range.min &&
	     tool->pressure.range.max == tool->pressure.wanted_range.max))
		return;

	tool->pressure.range.min = tool->pressure.wanted_range.min;
	tool->pressure.range.max = tool->pressure.wanted_range.max;

	struct libinput *libinput = tablet_libinput_context(tablet);
	libinput_plugin_system_notify_tablet_tool_configured(&libinput->plugin_system,
							     tool);
}

static inline void
tool_init_pressure_thresholds(struct tablet_dispatch *tablet,
			      struct libinput_tablet_tool *tool,
			      struct libinput_tablet_tool_pressure_threshold *threshold)
{
	struct evdev_device *device = tablet->device;
	const struct input_absinfo *pressure, *distance;

	threshold->tablet_id = tablet->tablet_id;
	threshold->offset = pressure_offset_from_double(0.0);
	threshold->has_offset = false;
	threshold->threshold.upper = 1;
	threshold->threshold.lower = 0;

	pressure = libevdev_get_abs_info(device->evdev, ABS_PRESSURE);
	if (!pressure)
		return;

	threshold->abs_pressure = *pressure;

	distance = libevdev_get_abs_info(device->evdev, ABS_DISTANCE);
	if (distance) {
		threshold->offset = pressure_offset_from_double(0.0);
		threshold->heuristic_state = PRESSURE_HEURISTIC_STATE_DONE;
	} else {
		threshold->offset = pressure_offset_from_double(1.0);
		threshold->heuristic_state = PRESSURE_HEURISTIC_STATE_PROXIN1;
	}

	apply_pressure_range_configuration(tablet, tool, true);
}

static int
pressure_range_is_available(struct libinput_tablet_tool *tool)
{
	return bit_is_set(tool->axis_caps, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);
}

static enum libinput_config_status
pressure_range_set(struct libinput_tablet_tool *tool, double min, double max)
{
	if (min < 0.0 || min >= 1.0 || max <= 0.0 || max > 1.0 || max <= min)
		return LIBINPUT_CONFIG_STATUS_INVALID;

	tool->pressure.wanted_range.min = min;
	tool->pressure.wanted_range.max = max;
	tool->pressure.has_configured_range = true;

	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static void
pressure_range_get(struct libinput_tablet_tool *tool, double *min, double *max)
{
	*min = tool->pressure.wanted_range.min;
	*max = tool->pressure.wanted_range.max;
}

static void
pressure_range_get_default(struct libinput_tablet_tool *tool, double *min, double *max)
{
	*min = 0.0;
	*max = 1.0;
}

static void
tablet_tool_apply_eraser_button(struct tablet_dispatch *tablet,
				struct libinput_tablet_tool *tool)
{
	if (bitmask_is_empty(tool->eraser_button.available_modes))
		return;

	if (tool->eraser_button.mode == tool->eraser_button.want_mode &&
	    tool->eraser_button.button == tool->eraser_button.want_button)
		return;

	if (!tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY))
		return;

	tool->eraser_button.mode = tool->eraser_button.want_mode;
	tool->eraser_button.button = tool->eraser_button.want_button;

	struct libinput *libinput = tablet_libinput_context(tablet);
	libinput_plugin_system_notify_tablet_tool_configured(&libinput->plugin_system,
							     tool);
}

static bitmask_t
eraser_button_get_modes(struct libinput_tablet_tool *tool)
{
	return tool->eraser_button.available_modes;
}

static void
eraser_button_toggle(struct libinput_tablet_tool *tool)
{
	struct libinput_device *libinput_device = tool->last_device;
	struct evdev_device *device = evdev_device(libinput_device);
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);

	tablet_tool_apply_eraser_button(tablet, tool);
}

static enum libinput_config_status
eraser_button_set_mode(struct libinput_tablet_tool *tool,
		       enum libinput_config_eraser_button_mode mode)
{
	if (mode != LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT &&
	    !bitmask_all(tool->eraser_button.available_modes, bitmask_from_u32(mode)))
		return LIBINPUT_CONFIG_STATUS_UNSUPPORTED;

	tool->eraser_button.want_mode = mode;

	eraser_button_toggle(tool);

	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static enum libinput_config_eraser_button_mode
eraser_button_get_mode(struct libinput_tablet_tool *tool)
{
	return tool->eraser_button.mode;
}

static enum libinput_config_eraser_button_mode
eraser_button_get_default_mode(struct libinput_tablet_tool *tool)
{
	return LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT;
}

static enum libinput_config_status
eraser_button_set_button(struct libinput_tablet_tool *tool, uint32_t button)
{
	switch (button) {
	case BTN_STYLUS:
	case BTN_STYLUS2:
	case BTN_STYLUS3:
		break;
	default:
		log_bug_libinput(libinput_device_get_context(tool->last_device),
				 "Unsupported eraser button 0x%x",
				 button);
		return LIBINPUT_CONFIG_STATUS_INVALID;
	}

	tool->eraser_button.want_button = button;

	eraser_button_toggle(tool);

	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static unsigned int
eraser_button_get_button(struct libinput_tablet_tool *tool)
{
	return tool->eraser_button.button;
}

static unsigned int
eraser_button_get_default_button(struct libinput_tablet_tool *tool)
{
	/* Which button we want is complicated. Other than Wacom no-one supports
	 * tool ids so we cannot know if an individual tool supports any of the
	 * BTN_STYLUS. e.g. any Huion tablet that supports the Huion PW600
	 * will have BTN_STYLUS3 - regardless if that tool is actually present.
	 * So we default to BTN_STYLUS3 because there's no placeholder BTN_STYLUS4.
	 * in the kernel.
	 */
	if (!libinput_tablet_tool_has_button(tool, BTN_STYLUS))
		return BTN_STYLUS;
	if (!libinput_tablet_tool_has_button(tool, BTN_STYLUS2))
		return BTN_STYLUS2;

	return BTN_STYLUS3;
}

static void
tool_init_eraser_button(struct tablet_dispatch *tablet,
			struct libinput_tablet_tool *tool,
			const WacomStylus *s)
{
	/* We provide an eraser button config if:
	 * - the tool is a pen
	 * - we don't know about the stylus (that's a good indication the
	 *   stylus doesn't have tool ids which means it'll follow the windows
	 *   pen protocol)
	 * - the tool does *not* have an eraser on the back end
	 *
	 * Because those are the only tools where the eraser button may
	 * get changed to a real button (by udev-hid-bpf).
	 */
	if (libinput_tablet_tool_get_type(tool) != LIBINPUT_TABLET_TOOL_TYPE_PEN)
		return;

#ifdef HAVE_LIBWACOM
	/* libwacom's API is a bit terrible here:
	 * - has_eraser is true on styli that have a separate eraser, all
	 *   those are INVERT so we can exclude them
	 * - get_eraser_type() returns something on actual eraser tools
	 *   but we don't have any separate erasers with buttons so
	 *   we only need to exclude INVERT
	 */
	if (s && libwacom_stylus_has_eraser(s) &&
	    libwacom_stylus_get_eraser_type(s) == WACOM_ERASER_INVERT) {
		return;
	}
#endif
	/* All other pens need eraser button handling because most of the time
	 * we don't know if they have one (Huion, XP-Pen, ...) */
	bitmask_t available_modes =
		bitmask_from_masks(LIBINPUT_CONFIG_ERASER_BUTTON_BUTTON);

	tool->eraser_button.available_modes = available_modes;
	tool->eraser_button.want_button = eraser_button_get_default_button(tool);
	tool->eraser_button.button = tool->eraser_button.want_button;
}

static struct libinput_tablet_tool *
tablet_new_tool(struct tablet_dispatch *tablet,
		enum libinput_tablet_tool_type type,
		uint32_t tool_id,
		uint32_t serial)
{
	struct libinput_tablet_tool *tool = zalloc(sizeof *tool);
	const WacomStylus *s = NULL;
#ifdef HAVE_LIBWACOM
	WacomDeviceDatabase *db;

	db = tablet_libinput_context(tablet)->libwacom.db;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	if (db)
		s = libwacom_stylus_get_for_id(db, tool_id);
#pragma GCC diagnostic pop
#endif

	*tool = (struct libinput_tablet_tool){
		.type = type,
		.serial = serial,
		.tool_id = tool_id,
		.refcount = 1,
		.last_device = NULL,

		.pressure.range.min = 0.0,
		.pressure.range.max = 0.0, /* to trigger configuration */
		.pressure.wanted_range.min = 0.0,
		.pressure.wanted_range.max = 1.0,

		.eraser_button.available_modes = bitmask_new(),
		.eraser_button.mode = LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT,
		.eraser_button.want_mode = LIBINPUT_CONFIG_ERASER_BUTTON_DEFAULT,
		.eraser_button.button = BTN_STYLUS2,
		.eraser_button.want_button = BTN_STYLUS2,

		.config.pressure_range.is_available = pressure_range_is_available,
		.config.pressure_range.set = pressure_range_set,
		.config.pressure_range.get = pressure_range_get,
		.config.pressure_range.get_default = pressure_range_get_default,

		.config.eraser_button.get_modes = eraser_button_get_modes,
		.config.eraser_button.set_mode = eraser_button_set_mode,
		.config.eraser_button.get_mode = eraser_button_get_mode,
		.config.eraser_button.get_default_mode = eraser_button_get_default_mode,
		.config.eraser_button.set_button = eraser_button_set_button,
		.config.eraser_button.get_button = eraser_button_get_button,
		.config.eraser_button.get_default_button =
			eraser_button_get_default_button,
	};

	tool_init_pressure_thresholds(tablet, tool, &tool->pressure.threshold);
	tool_set_bits(tablet, tool, s);
	tool_init_eraser_button(tablet, tool, s);

	return tool;
}

static struct libinput_tablet_tool *
tablet_get_tool(struct tablet_dispatch *tablet,
		enum libinput_tablet_tool_type type,
		uint32_t tool_id,
		uint32_t serial)
{
	struct evdev_device *device = tablet->device;
	struct libinput *libinput = tablet_libinput_context(tablet);
	struct libinput_tablet_tool *tool = NULL, *t;
	struct list *tool_list;

	if (serial) {
		tool_list = &libinput->tool_list;
		/* Check if we already have the tool in our list of tools */
		list_for_each(t, tool_list, link) {
			if (type == t->type && serial == t->serial) {
				tool = t;
				break;
			}
		}
	}

	/* If we get a tool with a delayed serial number, we already created
	 * a 0-serial number tool for it earlier. Re-use that, even though
	 * it means we can't distinguish this tool from others.
	 * https://bugs.freedesktop.org/show_bug.cgi?id=97526
	 */
	if (!tool) {
		tool_list = &tablet->tool_list;
		/* We can't guarantee that tools without serial numbers are
		 * unique, so we keep them local to the tablet that they come
		 * into proximity of instead of storing them in the global tool
		 * list
		 * Same as above, but don't bother checking the serial number
		 */
		list_for_each(t, tool_list, link) {
			if (type == t->type) {
				tool = t;
				break;
			}
		}

		/* Didn't find the tool but we have a serial. Switch
		 * tool_list back so we create in the correct list */
		if (!tool && serial)
			tool_list = &libinput->tool_list;
	}

	/* If we didn't already have the new_tool in our list of tools,
	 * add it */
	if (!tool) {
		tool = tablet_new_tool(tablet, type, tool_id, serial);
		list_insert(tool_list, &tool->link);
	}

	struct libinput_device *last = tool->last_device;
	tool->last_device = libinput_device_ref(&device->base);
	if (last)
		libinput_device_unref(last);

	tool->last_tablet_id = tablet->tablet_id;

	return tool;
}

static void
tablet_notify_button_mask(struct tablet_dispatch *tablet,
			  struct evdev_device *device,
			  uint64_t time,
			  struct libinput_tablet_tool *tool,
			  const struct button_state *buttons,
			  enum libinput_button_state state)
{
	struct libinput_device *base = &device->base;
	size_t i;
	size_t nbits = 8 * sizeof(buttons->bits);
	enum libinput_tablet_tool_tip_state tip_state;

	if (tablet_has_status(tablet, TABLET_TOOL_IN_CONTACT))
		tip_state = LIBINPUT_TABLET_TOOL_TIP_DOWN;
	else
		tip_state = LIBINPUT_TABLET_TOOL_TIP_UP;

	for (i = 0; i < nbits; i++) {
		if (!bit_is_set(buttons->bits, i))
			continue;

		tablet_notify_button(base,
				     time,
				     tool,
				     tip_state,
				     &tablet->axes,
				     button_code_from_uint32_t(i),
				     state,
				     &tablet->area.x,
				     &tablet->area.y);
	}
}

static void
tablet_notify_buttons(struct tablet_dispatch *tablet,
		      struct evdev_device *device,
		      uint64_t time,
		      struct libinput_tablet_tool *tool,
		      enum libinput_button_state state)
{
	struct button_state buttons;

	if (state == LIBINPUT_BUTTON_STATE_PRESSED)
		tablet_get_pressed_buttons(tablet, &buttons);
	else
		tablet_get_released_buttons(tablet, &buttons);

	tablet_notify_button_mask(tablet, device, time, tool, &buttons, state);
}

static void
sanitize_pressure_distance(struct tablet_dispatch *tablet,
			   struct libinput_tablet_tool *tool)
{
	bool tool_in_contact;
	const struct input_absinfo *distance, *pressure;

	distance = libevdev_get_abs_info(tablet->device->evdev, ABS_DISTANCE);
	/* Note: for pressure/distance sanitization we use the real pressure
	   axis, not our configured one */
	pressure = libevdev_get_abs_info(tablet->device->evdev, ABS_PRESSURE);

	if (!pressure || !distance)
		return;

	bool pressure_changed =
		bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);
	bool distance_changed =
		bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_DISTANCE);

	if (!pressure_changed && !distance_changed)
		return;

	/* Note: this is an arbitrary "in contact" decision rather than "tip
	 * down". We use the lower threshold as minimum pressure value,
	 * anything less than that gets filtered away */
	struct libinput_tablet_tool_pressure_threshold *threshold =
		tablet_tool_get_threshold(tablet, tool);
	tool_in_contact = (pressure->value > threshold->threshold.lower);

	/* Keep distance and pressure mutually exclusive */
	if (distance && distance->value > distance->minimum &&
	    pressure->value > pressure->minimum) {
		if (tool_in_contact) {
			clear_bit(tablet->changed_axes,
				  LIBINPUT_TABLET_TOOL_AXIS_DISTANCE);
			tablet->axes.distance = 0;
		} else {
			clear_bit(tablet->changed_axes,
				  LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);
			tablet->axes.pressure = 0;
		}
	} else if (pressure_changed && !tool_in_contact) {
		/* Make sure that the last axis value sent to the caller is a 0 */
		if (tablet->axes.pressure == 0)
			clear_bit(tablet->changed_axes,
				  LIBINPUT_TABLET_TOOL_AXIS_PRESSURE);
		else
			tablet->axes.pressure = 0;
	}
}

static inline void
sanitize_mouse_lens_rotation(struct tablet_dispatch *tablet)
{
	/* If we have a mouse/lens cursor and the tilt changed, the rotation
	   changed. Mark this, calculate the angle later */
	if ((tablet->current_tool.type == LIBINPUT_TABLET_TOOL_TYPE_MOUSE ||
	     tablet->current_tool.type == LIBINPUT_TABLET_TOOL_TYPE_LENS) &&
	    (bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_X) ||
	     bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_TILT_Y)))
		set_bit(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_ROTATION_Z);
}

static void
sanitize_tablet_axes(struct tablet_dispatch *tablet, struct libinput_tablet_tool *tool)
{
	sanitize_pressure_distance(tablet, tool);
	sanitize_mouse_lens_rotation(tablet);
}

static void
set_pressure_offset(struct libinput_tablet_tool_pressure_threshold *threshold,
		    pressure_offset_t offset_in_percent)
{
	threshold->offset = offset_in_percent;
	threshold->has_offset = true;

	/* Adjust the tresholds accordingly - we use the same gap (4% in
	 * device coordinates) between upper and lower as before which isn't
	 * technically correct (our range shrunk) but it's easy to calculate.
	 */
	int units =
		pressure_offset_to_absinfo(offset_in_percent, &threshold->abs_pressure);
	int gap = threshold->threshold.upper - threshold->threshold.lower;
	threshold->threshold.lower = units;
	threshold->threshold.upper = units + gap;
}

static void
update_pressure_offset(struct tablet_dispatch *tablet,
		       struct evdev_device *device,
		       struct libinput_tablet_tool *tool)
{
	const struct input_absinfo *pressure =
		libevdev_get_abs_info(device->evdev, ABS_PRESSURE);

	if (!pressure || tool->pressure.has_configured_range ||
	    !bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE))
		return;

	/* If we have an event that falls below the current offset, adjust
	 * the offset downwards. A fast contact can start with a
	 * higher-than-needed pressure offset and then we'd be tied into a
	 * high pressure offset for the rest of the session.
	 *
	 * If we are still pending the offset decision, only update the observed
	 * offset value, don't actually set it to have an offset.
	 */
	pressure_offset_t offset =
		pressure_offset_from_absinfo(pressure, pressure->value);
	struct libinput_tablet_tool_pressure_threshold *threshold =
		tablet_tool_get_threshold(tablet, tool);
	if (threshold->has_offset) {
		if (pressure_offset_cmp(offset, threshold->offset) < 0)
			set_pressure_offset(threshold, offset);
	} else if (threshold->heuristic_state != PRESSURE_HEURISTIC_STATE_DONE) {
		threshold->offset = pressure_offset_min(offset, threshold->offset);
	}
}

static void
detect_pressure_offset(struct tablet_dispatch *tablet,
		       struct evdev_device *device,
		       struct libinput_tablet_tool *tool)
{
	const struct input_absinfo *pressure, *distance;

	if (tool->pressure.has_configured_range ||
	    !bit_is_set(tablet->changed_axes, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE))
		return;

	struct libinput_tablet_tool_pressure_threshold *threshold =
		tablet_tool_get_threshold(tablet, tool);
	if (threshold->has_offset)
		return;

	pressure = libevdev_get_abs_info(device->evdev, ABS_PRESSURE);
	distance = libevdev_get_abs_info(device->evdev, ABS_DISTANCE);

	if (!pressure)
		return;

	int units = pressure->value;
	if (units <= pressure->minimum)
		return;

	pressure_offset_t offset = pressure_offset_from_absinfo(pressure, units);
	if (distance) {
		/* If we're closer than 50% of the distance axis, skip pressure
		 * offset detection, too likely to be wrong */
		if (distance->value < axis_range_percentage(distance, 50))
			return;
	} else {
		/* A device without distance will always have some pressure on
		 * contact. Offset detection is delayed for a few proximity ins
		 * in the hope we'll find the minimum value until then. That
		 * offset is updated during motion events so by the time the
		 * deciding prox-in arrives we should know the minimum offset.
		 */
		if (units > pressure->minimum)
			threshold->offset =
				pressure_offset_min(offset, threshold->offset);

		switch (threshold->heuristic_state) {
		case PRESSURE_HEURISTIC_STATE_PROXIN1:
		case PRESSURE_HEURISTIC_STATE_PROXIN2:
			threshold->heuristic_state++;
			return;
		case PRESSURE_HEURISTIC_STATE_DECIDE:
			threshold->heuristic_state++;
			units = pressure_offset_to_absinfo(threshold->offset, pressure);
			offset = threshold->offset;
			break;
		case PRESSURE_HEURISTIC_STATE_DONE:
			return;
		}
	}

	if (units <= pressure->minimum) {
		return;
	}

	if (pressure_offset_gt(offset, 0.5)) {
		evdev_log_error(
			device,
			"Ignoring pressure offset greater than 50%% detected on tool %s (serial %#x). "
			"See %s/tablet-support.html\n",
			tablet_tool_type_to_string(tool->type),
			tool->serial,
			HTTP_DOC_LINK);
		return;
	}

	evdev_log_info(device,
		       "Pressure offset detected on tool %s (serial %#x).  "
		       "See %s/tablet-support.html\n",
		       tablet_tool_type_to_string(tool->type),
		       tool->serial,
		       HTTP_DOC_LINK);

	set_pressure_offset(threshold, offset);
}

static void
detect_tool_contact(struct tablet_dispatch *tablet,
		    struct evdev_device *device,
		    struct libinput_tablet_tool *tool)
{
	const struct input_absinfo *p;
	int pressure;

	if (!bit_is_set(tool->axis_caps, LIBINPUT_TABLET_TOOL_AXIS_PRESSURE))
		return;

	/* if we have pressure, always use that for contact, not BTN_TOUCH */
	if (tablet_has_status(tablet, TABLET_TOOL_ENTERING_CONTACT))
		evdev_log_bug_libinput(device, "Invalid status: entering contact\n");
	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_CONTACT) &&
	    !tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY))
		evdev_log_bug_libinput(device, "Invalid status: leaving contact\n");

	p = libevdev_get_abs_info(tablet->device->evdev, ABS_PRESSURE);
	if (!p) {
		evdev_log_bug_libinput(device, "Missing pressure axis\n");
		return;
	}
	pressure = p->value;

	struct libinput_tablet_tool_pressure_threshold *threshold =
		tablet_tool_get_threshold(tablet, tool);
	if (pressure <= threshold->threshold.lower &&
	    tablet_has_status(tablet, TABLET_TOOL_IN_CONTACT)) {
		tablet_set_status(tablet, TABLET_TOOL_LEAVING_CONTACT);
	} else if (pressure >= threshold->threshold.upper &&
		   !tablet_has_status(tablet, TABLET_TOOL_IN_CONTACT)) {
		tablet_set_status(tablet, TABLET_TOOL_ENTERING_CONTACT);
	}
}

static void
tablet_mark_all_axes_changed(struct tablet_dispatch *tablet,
			     struct libinput_tablet_tool *tool)
{
	static_assert(sizeof(tablet->changed_axes) == sizeof(tool->axis_caps),
		      "Mismatching array sizes");

	memcpy(tablet->changed_axes, tool->axis_caps, sizeof(tablet->changed_axes));
}

static void
tablet_update_proximity_state(struct tablet_dispatch *tablet,
			      struct evdev_device *device,
			      struct libinput_tablet_tool *tool)
{
	const struct input_absinfo *distance;
	int dist_max = tablet->cursor_proximity_threshold;
	int dist;

	distance = libevdev_get_abs_info(tablet->device->evdev, ABS_DISTANCE);
	if (!distance)
		return;

	dist = distance->value;
	if (dist == 0)
		return;

	/* Tool got into permitted range */
	if (dist < dist_max &&
	    (tablet_has_status(tablet, TABLET_TOOL_OUT_OF_RANGE) ||
	     tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY))) {
		tablet_unset_status(tablet, TABLET_TOOL_OUT_OF_RANGE);
		tablet_unset_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY);
		tablet_set_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY);
		tablet_mark_all_axes_changed(tablet, tool);

		tablet_set_status(tablet, TABLET_BUTTONS_PRESSED);
		tablet_force_button_presses(tablet);
		return;
	}

	if (dist < dist_max)
		return;

	/* Still out of range/proximity */
	if (tablet_has_status(tablet, TABLET_TOOL_OUT_OF_RANGE) ||
	    tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY))
		return;

	/* Tool entered prox but is outside of permitted range */
	if (tablet_has_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY)) {
		tablet_set_status(tablet, TABLET_TOOL_OUT_OF_RANGE);
		tablet_unset_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY);
		return;
	}

	/* Tool was in prox and is now outside of range. Set leaving
	 * proximity, on the next event it will be OUT_OF_PROXIMITY and thus
	 * caught by the above conditions */
	tablet_set_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY);
}

static struct phys_rect
tablet_calculate_arbitration_rect(struct tablet_dispatch *tablet)
{
	struct evdev_device *device = tablet->device;
	struct phys_rect r = { 0 };
	struct phys_coords mm;

	mm = evdev_device_units_to_mm(device, &tablet->axes.point);

	/* The rect we disable is 20mm left of the tip, 100mm north of the
	 * tip, and 200x250mm large.
	 * If the stylus is tilted left (tip further right than the eraser
	 * end) assume left-handed mode.
	 *
	 * Obviously if we'd run out of the boundaries, we clip the rect
	 * accordingly.
	 */
	if (tablet->axes.tilt.x > 0) {
		r.x = mm.x - 20;
		r.w = 200;
	} else {
		r.x = mm.x + 20;
		r.w = 200;
		r.x -= r.w;
	}

	if (r.x < 0) {
		r.w += r.x;
		r.x = 0;
	}

	r.y = mm.y - 100;
	r.h = 250;
	if (r.y < 0) {
		r.h += r.y;
		r.y = 0;
	}

	return r;
}

static inline void
tablet_update_touch_device_rect(struct tablet_dispatch *tablet,
				const struct tablet_axes *axes,
				uint64_t time)
{
	struct evdev_dispatch *dispatch;
	struct phys_rect rect = { 0 };

	if (tablet->touch_device == NULL ||
	    tablet->arbitration != ARBITRATION_IGNORE_RECT)
		return;

	rect = tablet_calculate_arbitration_rect(tablet);

	dispatch = tablet->touch_device->dispatch;
	if (dispatch->interface->touch_arbitration_update_rect)
		dispatch->interface->touch_arbitration_update_rect(dispatch,
								   tablet->touch_device,
								   &rect,
								   time);
}

static inline bool
tablet_send_proximity_in(struct tablet_dispatch *tablet,
			 struct libinput_tablet_tool *tool,
			 struct evdev_device *device,
			 struct tablet_axes *axes,
			 uint64_t time)
{
	if (!tablet_has_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY))
		return false;

	tablet_notify_proximity(&device->base,
				time,
				tool,
				LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN,
				tablet->changed_axes,
				axes,
				&tablet->area.x,
				&tablet->area.y);
	tablet_unset_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY);
	tablet_unset_status(tablet, TABLET_AXES_UPDATED);

	tablet_reset_changed_axes(tablet);
	axes->delta.x = 0;
	axes->delta.y = 0;

	return true;
}

static inline void
tablet_send_proximity_out(struct tablet_dispatch *tablet,
			  struct libinput_tablet_tool *tool,
			  struct evdev_device *device,
			  struct tablet_axes *axes,
			  uint64_t time)
{
	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY) &&
	    !tablet_has_status(tablet, TABLET_TOOL_OUTSIDE_AREA)) {
		tablet_notify_proximity(&device->base,
					time,
					tool,
					LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_OUT,
					tablet->changed_axes,
					axes,
					&tablet->area.x,
					&tablet->area.y);
	}
}

static inline bool
tablet_send_tip(struct tablet_dispatch *tablet,
		struct libinput_tablet_tool *tool,
		struct evdev_device *device,
		struct tablet_axes *axes,
		uint64_t time)
{
	if (tablet_has_status(tablet, TABLET_TOOL_ENTERING_CONTACT)) {
		tablet_notify_tip(&device->base,
				  time,
				  tool,
				  LIBINPUT_TABLET_TOOL_TIP_DOWN,
				  tablet->changed_axes,
				  axes,
				  &tablet->area.x,
				  &tablet->area.y);
		tablet_unset_status(tablet, TABLET_AXES_UPDATED);
		tablet_unset_status(tablet, TABLET_TOOL_ENTERING_CONTACT);
		tablet_set_status(tablet, TABLET_TOOL_IN_CONTACT);

		tablet_reset_changed_axes(tablet);
		axes->delta.x = 0;
		axes->delta.y = 0;

		return true;
	}

	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_CONTACT)) {
		tablet_notify_tip(&device->base,
				  time,
				  tool,
				  LIBINPUT_TABLET_TOOL_TIP_UP,
				  tablet->changed_axes,
				  axes,
				  &tablet->area.x,
				  &tablet->area.y);
		tablet_unset_status(tablet, TABLET_AXES_UPDATED);
		tablet_unset_status(tablet, TABLET_TOOL_LEAVING_CONTACT);
		tablet_unset_status(tablet, TABLET_TOOL_IN_CONTACT);

		tablet_reset_changed_axes(tablet);
		axes->delta.x = 0;
		axes->delta.y = 0;

		return true;
	}

	return false;
}

static inline void
tablet_send_axes(struct tablet_dispatch *tablet,
		 struct libinput_tablet_tool *tool,
		 struct evdev_device *device,
		 struct tablet_axes *axes,
		 uint64_t time)
{
	enum libinput_tablet_tool_tip_state tip_state;

	if (!tablet_has_status(tablet, TABLET_AXES_UPDATED))
		return;

	if (tablet_has_status(tablet, TABLET_TOOL_IN_CONTACT))
		tip_state = LIBINPUT_TABLET_TOOL_TIP_DOWN;
	else
		tip_state = LIBINPUT_TABLET_TOOL_TIP_UP;

	tablet_notify_axis(&device->base,
			   time,
			   tool,
			   tip_state,
			   tablet->changed_axes,
			   axes,
			   &tablet->area.x,
			   &tablet->area.y);
	tablet_unset_status(tablet, TABLET_AXES_UPDATED);
	tablet_reset_changed_axes(tablet);
	axes->delta.x = 0;
	axes->delta.y = 0;
}

static inline void
tablet_send_buttons(struct tablet_dispatch *tablet,
		    struct libinput_tablet_tool *tool,
		    struct evdev_device *device,
		    uint64_t time)
{
	if (tablet_has_status(tablet, TABLET_BUTTONS_RELEASED)) {
		tablet_notify_buttons(tablet,
				      device,
				      time,
				      tool,
				      LIBINPUT_BUTTON_STATE_RELEASED);
		tablet_unset_status(tablet, TABLET_BUTTONS_RELEASED);
	}

	if (tablet_has_status(tablet, TABLET_BUTTONS_PRESSED)) {
		tablet_notify_buttons(tablet,
				      device,
				      time,
				      tool,
				      LIBINPUT_BUTTON_STATE_PRESSED);
		tablet_unset_status(tablet, TABLET_BUTTONS_PRESSED);
	}
}

static void
tablet_send_events(struct tablet_dispatch *tablet,
		   struct libinput_tablet_tool *tool,
		   struct evdev_device *device,
		   uint64_t time)
{
	struct tablet_axes axes = { 0 };

	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY)) {
		/* Tool is leaving proximity, we can't rely on the last axis
		 * information (it'll be mostly 0), so we just get the
		 * current state and skip over updating the axes.
		 */
		axes = tablet->axes;

		/* Don't send an axis event, but we may have a tip event
		 * update */
		tablet_unset_status(tablet, TABLET_AXES_UPDATED);
	} else {
		if (tablet_check_notify_axes(tablet, device, tool, &axes, time))
			tablet_update_touch_device_rect(tablet, &axes, time);
	}

	assert(tablet->axes.delta.x == 0);
	assert(tablet->axes.delta.y == 0);

	tablet_send_proximity_in(tablet, tool, device, &axes, time);
	if (!tablet_send_tip(tablet, tool, device, &axes, time))
		tablet_send_axes(tablet, tool, device, &axes, time);

	tablet_unset_status(tablet, TABLET_TOOL_ENTERING_CONTACT);
	tablet_reset_changed_axes(tablet);

	tablet_send_buttons(tablet, tool, device, time);

	tablet_send_proximity_out(tablet, tool, device, &axes, time);
}

static void
tablet_update_tool_state(struct tablet_dispatch *tablet,
			 struct evdev_device *device,
			 uint64_t time)
{
	enum libinput_tablet_tool_type type;
	uint32_t changed;
	int state;

	if (tablet->tool_state == tablet->prev_tool_state)
		return;

	changed = tablet->tool_state ^ tablet->prev_tool_state;
	type = ffs(changed) - 1;
	state = !!(tablet->tool_state & bit(type));

	tablet_update_tool(tablet, device, type, state);

	tablet->prev_tool_state = tablet->tool_state;
}

static struct libinput_tablet_tool *
tablet_get_current_tool(struct tablet_dispatch *tablet)
{
	if (tablet->current_tool.type == LIBINPUT_TOOL_NONE)
		return NULL;

	return tablet_get_tool(tablet,
			       tablet->current_tool.type,
			       tablet->current_tool.id,
			       tablet->current_tool.serial);
}

static void
update_pressure_range(struct tablet_dispatch *tablet,
		      struct evdev_device *device,
		      struct libinput_tablet_tool *tool)
{
	if (!libevdev_has_event_code(device->evdev, EV_ABS, ABS_PRESSURE))
		return;

	double min = tool->pressure.range.min;
	double max = tool->pressure.range.max;

	struct input_absinfo abs = *libevdev_get_abs_info(device->evdev, ABS_PRESSURE);

	int minimum = axis_range_percentage(&abs, min * 100.0);
	int maximum = axis_range_percentage(&abs, max * 100.0);

	abs.minimum = minimum;
	abs.maximum = maximum;

	/* Only use the quirk pressure range if we don't have a custom range */
	int hi, lo;
	if (tool->pressure.range.min != 0.0 || tool->pressure.range.max != 1.0 ||
	    !tablet_get_quirked_pressure_thresholds(tablet, &hi, &lo)) {
		/* 5 and 1% of the pressure range */
		hi = axis_range_percentage(&abs, 5);
		lo = axis_range_percentage(&abs, 1);
	}

	struct libinput_tablet_tool_pressure_threshold *threshold =
		tablet_tool_get_threshold(tablet, tool);
	threshold->abs_pressure = abs;
	threshold->threshold.upper = hi;
	threshold->threshold.lower = lo;

	if (threshold->has_offset)
		set_pressure_offset(threshold, threshold->offset);

	/* Disable any heuristics */
	if (tool->pressure.has_configured_range) {
		threshold->has_offset = true;
		threshold->heuristic_state = PRESSURE_HEURISTIC_STATE_DONE;
	}
}

static void
tablet_flush(struct tablet_dispatch *tablet, struct evdev_device *device, uint64_t time)
{
	struct libinput_tablet_tool *tool;

	tablet_update_tool_state(tablet, device, time);

	tool = tablet_get_current_tool(tablet);
	if (!tool)
		return; /* OOM */

	if (tool->type == LIBINPUT_TABLET_TOOL_TYPE_MOUSE ||
	    tool->type == LIBINPUT_TABLET_TOOL_TYPE_LENS)
		tablet_update_proximity_state(tablet, device, tool);

	if (tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY) ||
	    tablet_has_status(tablet, TABLET_TOOL_OUT_OF_RANGE))
		return;

	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY)) {
		/* Release all stylus buttons */
		memset(tablet->button_state.bits, 0, sizeof(tablet->button_state.bits));
		tablet_set_status(tablet, TABLET_BUTTONS_RELEASED);
		if (tablet_has_status(tablet, TABLET_TOOL_IN_CONTACT))
			tablet_set_status(tablet, TABLET_TOOL_LEAVING_CONTACT);
		apply_pressure_range_configuration(tablet, tool, false);
	} else if (!tablet_has_status(tablet, TABLET_TOOL_OUTSIDE_AREA)) {
		if (tablet_has_status(tablet, TABLET_TOOL_ENTERING_PROXIMITY)) {
			/* If we get into proximity outside the tablet area, we ignore
			 * that whole sequence of events even if we later move into
			 * the allowed area. This may be bad UX but it's complicated to
			 * implement so let's wait for someone to actually complain
			 * about it.
			 *
			 * We allow a margin of 3% (6mm on a 200mm tablet) to be
			 * "within" the area - there we clip to the area but do not
			 * ignore the sequence.
			 */
			const struct device_coords point = {
				device->abs.absinfo_x->value,
				device->abs.absinfo_y->value,
			};

			const double margin = 0.03;
			if (is_inside_area(tablet, &point, margin)) {
				tablet_mark_all_axes_changed(tablet, tool);
				update_pressure_range(tablet, device, tool);
				update_pressure_offset(tablet, device, tool);
				detect_pressure_offset(tablet, device, tool);
				detect_tool_contact(tablet, device, tool);
				sanitize_tablet_axes(tablet, tool);
			} else {
				tablet_set_status(tablet, TABLET_TOOL_OUTSIDE_AREA);
				tablet_unset_status(tablet,
						    TABLET_TOOL_ENTERING_PROXIMITY);
			}
		} else if (tablet_has_status(tablet, TABLET_AXES_UPDATED)) {
			update_pressure_offset(tablet, device, tool);
			detect_tool_contact(tablet, device, tool);
			sanitize_tablet_axes(tablet, tool);
		}
	}

	if (!tablet_has_status(tablet, TABLET_TOOL_OUTSIDE_AREA))
		tablet_send_events(tablet, tool, device, time);

	if (tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY)) {
		tablet_set_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY);
		tablet_unset_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY);
		tablet_unset_status(tablet, TABLET_TOOL_OUTSIDE_AREA);

		tablet_reset_changed_axes(tablet);

		tablet_change_to_left_handed(device);
		tablet_apply_rotation(device);
		tablet_change_area(device);
		tablet_history_reset(tablet);
		tablet_tool_apply_eraser_button(tablet, tool);
	}
}

static inline void
tablet_set_touch_device_enabled(struct tablet_dispatch *tablet,
				enum evdev_arbitration_state which,
				const struct phys_rect *rect,
				uint64_t time)
{
	struct evdev_device *touch_device = tablet->touch_device;
	struct evdev_dispatch *dispatch;

	if (touch_device == NULL)
		return;

	tablet->arbitration = which;

	dispatch = touch_device->dispatch;
	if (dispatch->interface->touch_arbitration_toggle)
		dispatch->interface->touch_arbitration_toggle(dispatch,
							      touch_device,
							      which,
							      rect,
							      time);
}

static inline void
tablet_toggle_touch_device(struct tablet_dispatch *tablet,
			   struct evdev_device *tablet_device,
			   uint64_t time)
{
	enum evdev_arbitration_state which;
	struct phys_rect r = { 0 };
	struct phys_rect *rect = NULL;

	if (tablet_has_status(tablet, TABLET_TOOL_OUT_OF_RANGE) ||
	    tablet_has_status(tablet, TABLET_NONE) ||
	    tablet_has_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY) ||
	    tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY)) {
		which = ARBITRATION_NOT_ACTIVE;
	} else if (tablet->axes.tilt.x == 0) {
		which = ARBITRATION_IGNORE_ALL;
	} else if (tablet->arbitration != ARBITRATION_IGNORE_RECT) {
		/* This enables rect-based arbitration, updates are sent
		 * elsewhere */
		r = tablet_calculate_arbitration_rect(tablet);
		rect = &r;
		which = ARBITRATION_IGNORE_RECT;
	} else {
		return;
	}

	tablet_set_touch_device_enabled(tablet, which, rect, time);
}

static inline void
tablet_reset_state(struct tablet_dispatch *tablet)
{
	struct button_state zero = { 0 };

	/* Update state */
	memcpy(&tablet->prev_button_state,
	       &tablet->button_state,
	       sizeof(tablet->button_state));
	tablet_unset_status(tablet, TABLET_TOOL_UPDATED);

	if (memcmp(&tablet->button_state, &zero, sizeof(zero)) == 0)
		tablet_unset_status(tablet, TABLET_BUTTONS_DOWN);
	else
		tablet_set_status(tablet, TABLET_BUTTONS_DOWN);
}

static void
tablet_process_event(struct evdev_dispatch *dispatch,
		     struct evdev_device *device,
		     struct evdev_event *e,
		     uint64_t time)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);

	uint16_t type = evdev_event_type(e);
	switch (type) {
	case EV_ABS:
		tablet_process_absolute(tablet, device, e, time);
		break;
	case EV_REL:
		tablet_process_relative(tablet, device, e, time);
		break;
	case EV_KEY:
		tablet_process_key(tablet, device, e, time);
		break;
	case EV_MSC:
		tablet_process_misc(tablet, device, e, time);
		break;
	case EV_SYN:
		tablet_flush(tablet, device, time);
		tablet_toggle_touch_device(tablet, device, time);
		tablet_reset_state(tablet);
		break;
	default:
		evdev_log_error(device,
				"Unexpected event type %s (%#x)\n",
				evdev_event_get_type_name(e),
				evdev_event_type(e));
		break;
	}
}

static void
tablet_process(struct evdev_dispatch *dispatch,
	       struct evdev_device *device,
	       struct evdev_frame *frame,
	       uint64_t time)
{
	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	for (size_t i = 0; i < nevents; i++) {
		tablet_process_event(dispatch, device, &events[i], time);
	}
}

static void
tablet_suspend(struct evdev_dispatch *dispatch, struct evdev_device *device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);
	struct libinput *li = tablet_libinput_context(tablet);
	uint64_t now = libinput_now(li);

	tablet_set_touch_device_enabled(tablet, ARBITRATION_NOT_ACTIVE, NULL, now);

	if (!tablet_has_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY)) {
		tablet_set_status(tablet, TABLET_TOOL_LEAVING_PROXIMITY);
		tablet_flush(tablet, device, libinput_now(li));
	}
}

static void
tablet_remove(struct evdev_dispatch *dispatch)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);
	struct libinput_device *device = &tablet->device->base;
	struct libinput *libinput = tablet_libinput_context(tablet);
	struct libinput_tablet_tool *tool;

	list_for_each_safe(tool, &tablet->tool_list, link) {
		if (tool->last_device == device) {
			libinput_device_unref(tool->last_device);
			tool->last_device = NULL;
		}
	}

	list_for_each_safe(tool, &libinput->tool_list, link) {
		if (tool->last_device == device) {
			libinput_device_unref(tool->last_device);
			tool->last_device = NULL;
		}
	}
}

static void
tablet_destroy(struct evdev_dispatch *dispatch)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);
	struct libinput_tablet_tool *tool;
	struct libinput *li = tablet_libinput_context(tablet);

	list_for_each_safe(tool, &tablet->tool_list, link) {
		libinput_tablet_tool_unref(tool);
	}

	libinput_libwacom_unref(li);

	free(tablet);
}

static void
tablet_setup_touch_arbitration(struct evdev_device *device,
			       struct evdev_device *new_device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);

	/* We enable touch arbitration with the first touch screen/external
	 * touchpad we see. This may be wrong in some cases, so we have some
	 * heuristics in case we find a "better" device.
	 */
	if (tablet->touch_device != NULL) {
		struct libinput_device_group *group1 =
			libinput_device_get_device_group(&device->base);
		struct libinput_device_group *group2 =
			libinput_device_get_device_group(&new_device->base);

		/* same phsical device? -> better, otherwise keep the one we have */
		if (group1 != group2)
			return;

		/* We found a better device, let's swap it out */
		struct libinput *li = tablet_libinput_context(tablet);
		tablet_set_touch_device_enabled(tablet,
						ARBITRATION_NOT_ACTIVE,
						NULL,
						libinput_now(li));
		evdev_log_debug(device,
				"touch-arbitration: removing pairing for %s<->%s\n",
				device->devname,
				tablet->touch_device->devname);
	}

	evdev_log_debug(device,
			"touch-arbitration: activated for %s<->%s\n",
			device->devname,
			new_device->devname);
	tablet->touch_device = new_device;
}

static void
tablet_setup_rotation(struct evdev_device *device, struct evdev_device *new_device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);
	struct libinput_device_group *group1 =
		libinput_device_get_device_group(&device->base);
	struct libinput_device_group *group2 =
		libinput_device_get_device_group(&new_device->base);

	if (tablet->rotation.touch_device == NULL && (group1 == group2)) {
		evdev_log_debug(device,
				"tablet-rotation: %s will rotate %s\n",
				device->devname,
				new_device->devname);
		tablet->rotation.touch_device = new_device;

		if (libinput_device_config_left_handed_get(&new_device->base)) {
			tablet->rotation.touch_device_left_handed_state = true;
			tablet_change_rotation(device, DO_NOTIFY);
		}
	}
}

static void
tablet_device_added(struct evdev_device *device, struct evdev_device *added_device)
{
	bool is_touchscreen, is_ext_touchpad;

	is_touchscreen =
		evdev_device_has_capability(added_device, LIBINPUT_DEVICE_CAP_TOUCH);
	is_ext_touchpad = evdev_device_has_capability(added_device,
						      LIBINPUT_DEVICE_CAP_POINTER) &&
			  (added_device->tags & EVDEV_TAG_EXTERNAL_TOUCHPAD);

	if (is_touchscreen || is_ext_touchpad)
		tablet_setup_touch_arbitration(device, added_device);

	if (is_ext_touchpad)
		tablet_setup_rotation(device, added_device);
}

static void
tablet_device_removed(struct evdev_device *device, struct evdev_device *removed_device)
{
	struct tablet_dispatch *tablet = tablet_dispatch(device->dispatch);

	if (tablet->touch_device == removed_device)
		tablet->touch_device = NULL;

	if (tablet->rotation.touch_device == removed_device) {
		tablet->rotation.touch_device = NULL;
		tablet->rotation.touch_device_left_handed_state = false;
		tablet_change_rotation(device, DO_NOTIFY);
	}
}

static void
tablet_check_initial_proximity(struct evdev_device *device,
			       struct evdev_dispatch *dispatch)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);
	int code, state;
	enum libinput_tablet_tool_type tool;

	for (tool = LIBINPUT_TABLET_TOOL_TYPE_PEN;
	     tool <= LIBINPUT_TABLET_TOOL_TYPE_MAX;
	     tool++) {
		code = tablet_tool_to_evcode(tool);

		/* we only expect one tool to be in proximity at a time */
		if (libevdev_fetch_event_value(device->evdev, EV_KEY, code, &state) &&
		    state) {
			tablet->tool_state = bit(tool);
			tablet->prev_tool_state = bit(tool);
			break;
		}
	}

	if (!tablet->tool_state)
		return;

	tablet_update_tool(tablet, device, tool, state);

	tablet->current_tool.id =
		libevdev_get_event_value(device->evdev, EV_ABS, ABS_MISC);

	/* we can't fetch MSC_SERIAL from the kernel, so we set the serial
	 * to 0 for now. On the first real event from the device we get the
	 * serial (if any) and that event will be converted into a proximity
	 * event */
	tablet->current_tool.serial = 0;
}

/* Called when the touchpad toggles to left-handed */
static void
tablet_left_handed_toggled(struct evdev_dispatch *dispatch,
			   struct evdev_device *device,
			   bool left_handed_enabled)
{
	struct tablet_dispatch *tablet = tablet_dispatch(dispatch);

	if (!tablet->rotation.touch_device)
		return;

	evdev_log_debug(device,
			"tablet-rotation: touchpad is %s\n",
			left_handed_enabled ? "left-handed" : "right-handed");

	/* Our left-handed config is independent even though rotation is
	 * locked. So we rotate when either device is left-handed. But it
	 * can only be actually changed when the device is in a neutral
	 * state, hence the want_rotate.
	 */
	tablet->rotation.touch_device_left_handed_state = left_handed_enabled;
	tablet_change_rotation(device, DONT_NOTIFY);
}

static struct evdev_dispatch_interface tablet_interface = {
	.process = tablet_process,
	.suspend = tablet_suspend,
	.remove = tablet_remove,
	.destroy = tablet_destroy,
	.device_added = tablet_device_added,
	.device_removed = tablet_device_removed,
	.device_suspended = NULL,
	.device_resumed = NULL,
	.post_added = tablet_check_initial_proximity,
	.touch_arbitration_toggle = NULL,
	.touch_arbitration_update_rect = NULL,
	.get_switch_state = NULL,
	.left_handed_toggle = tablet_left_handed_toggled,
};

static void
tablet_init_calibration(struct tablet_dispatch *tablet,
			struct evdev_device *device,
			bool is_display_tablet)
{
	if (is_display_tablet ||
	    libevdev_has_property(device->evdev, INPUT_PROP_DIRECT))
		evdev_init_calibration(device, &tablet->calibration);
}

static int
tablet_area_has_rectangle(struct libinput_device *device)
{
	return 1;
}

static enum libinput_config_status
tablet_area_set_rectangle(struct libinput_device *device,
			  const struct libinput_config_area_rectangle *rectangle)
{
	struct evdev_device *evdev = evdev_device(device);
	struct tablet_dispatch *tablet = tablet_dispatch(evdev->dispatch);

	if (rectangle->x1 >= rectangle->x2 || rectangle->y1 >= rectangle->y2)
		return LIBINPUT_CONFIG_STATUS_INVALID;

	if (rectangle->x1 < 0.0 || rectangle->x2 > 1.0 || rectangle->y1 < 0.0 ||
	    rectangle->y2 > 1.0)
		return LIBINPUT_CONFIG_STATUS_INVALID;

	tablet->area.want_rect = *rectangle;

	tablet_change_area(evdev);

	return LIBINPUT_CONFIG_STATUS_SUCCESS;
}

static struct libinput_config_area_rectangle
tablet_area_get_rectangle(struct libinput_device *device)
{
	struct evdev_device *evdev = evdev_device(device);
	struct tablet_dispatch *tablet = tablet_dispatch(evdev->dispatch);

	return tablet->area.rect;
}

static struct libinput_config_area_rectangle
tablet_area_get_default_rectangle(struct libinput_device *device)
{
	struct libinput_config_area_rectangle rect = {
		0.0,
		0.0,
		1.0,
		1.0,
	};
	return rect;
}

static void
tablet_init_area(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	tablet->area.rect = (struct libinput_config_area_rectangle){
		0.0,
		0.0,
		1.0,
		1.0,
	};
	tablet->area.want_rect = tablet->area.rect;
	tablet->area.x = *device->abs.absinfo_x;
	tablet->area.y = *device->abs.absinfo_y;

	if (!libevdev_has_property(device->evdev, INPUT_PROP_DIRECT)) {
		device->base.config.area = &tablet->area.config;
		tablet->area.config.has_rectangle = tablet_area_has_rectangle;
		tablet->area.config.set_rectangle = tablet_area_set_rectangle;
		tablet->area.config.get_rectangle = tablet_area_get_rectangle;
		tablet->area.config.get_default_rectangle =
			tablet_area_get_default_rectangle;
	}
}

static void
tablet_init_proximity_threshold(struct tablet_dispatch *tablet,
				struct evdev_device *device)
{
	/* This rules out most of the bamboos and other devices, we're
	 * pretty much down to
	 */
	if (!libevdev_has_event_code(device->evdev, EV_KEY, BTN_TOOL_MOUSE) &&
	    !libevdev_has_event_code(device->evdev, EV_KEY, BTN_TOOL_LENS))
		return;

	/* 42 is the default proximity threshold the xf86-input-wacom driver
	 * uses for Intuos/Cintiq models. Graphire models have a threshold
	 * of 10 but since they haven't been manufactured in ages and the
	 * intersection of users having a graphire, running libinput and
	 * wanting to use the mouse/lens cursor tool is small enough to not
	 * worry about it for now. If we need to, we can introduce a udev
	 * property later.
	 *
	 * Value is in device coordinates.
	 */
	tablet->cursor_proximity_threshold = 42;
}

static uint32_t
tablet_accel_config_get_profiles(struct libinput_device *libinput_device)
{
	return LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
}

static enum libinput_config_status
tablet_accel_config_set_profile(struct libinput_device *libinput_device,
				enum libinput_config_accel_profile profile)
{
	return LIBINPUT_CONFIG_STATUS_UNSUPPORTED;
}

static enum libinput_config_accel_profile
tablet_accel_config_get_profile(struct libinput_device *libinput_device)
{
	return LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
}

static enum libinput_config_accel_profile
tablet_accel_config_get_default_profile(struct libinput_device *libinput_device)
{
	return LIBINPUT_CONFIG_ACCEL_PROFILE_NONE;
}

static int
tablet_init_accel(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	const struct input_absinfo *x, *y;
	struct motion_filter *filter;

	x = device->abs.absinfo_x;
	y = device->abs.absinfo_y;

	filter = create_pointer_accelerator_filter_tablet(x->resolution, y->resolution);
	if (!filter)
		return -1;

	evdev_device_init_pointer_acceleration(device, filter);

	/* we override the profile hooks for accel configuration with hooks
	 * that don't allow selection of profiles */
	device->pointer.config.get_profiles = tablet_accel_config_get_profiles;
	device->pointer.config.set_profile = tablet_accel_config_set_profile;
	device->pointer.config.get_profile = tablet_accel_config_get_profile;
	device->pointer.config.get_default_profile =
		tablet_accel_config_get_default_profile;

	return 0;
}

static void
tablet_init_left_handed(struct evdev_device *device, WacomDevice *wacom)
{
	bool has_left_handed = true;

#ifdef HAVE_LIBWACOM
	has_left_handed = !wacom || libwacom_is_reversible(wacom);
#endif
	if (has_left_handed)
		evdev_init_left_handed(device, tablet_change_to_left_handed);
}

static inline bool
tablet_is_display_tablet(WacomDevice *wacom)
{
#ifdef HAVE_LIBWACOM
	return !wacom ||
	       (libwacom_get_integration_flags(wacom) &
		(WACOM_DEVICE_INTEGRATED_SYSTEM | WACOM_DEVICE_INTEGRATED_DISPLAY));
#else
	return true;
#endif
}

static inline bool
tablet_is_aes(struct evdev_device *device, WacomDevice *wacom)
{
#ifdef HAVE_LIBWACOM
	int vid = evdev_device_get_id_vendor(device);
	/* Wacom-specific check for whether smoothing is required:
	 * libwacom keeps all the AES pens in a single group, so any device
	 * that supports AES pens will list all AES pens. 0x11 is one of the
	 * lenovo pens so we use that as the flag of whether the tablet
	 * is an AES tablet
	 */
	if (wacom && vid == VENDOR_ID_WACOM) {
		int nstyli;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		const int *stylus_ids = libwacom_get_supported_styli(wacom, &nstyli);
#pragma GCC diagnostic pop
		for (int i = 0; i < nstyli; i++) {
			if (stylus_ids[i] == 0x11) {
				return true;
			}
		}
	}
#endif

	return false;
}

static void
tablet_init_smoothing(struct evdev_device *device,
		      struct tablet_dispatch *tablet,
		      bool is_aes,
		      bool is_virtual)
{
	size_t history_size = ARRAY_LENGTH(tablet->history.samples);
	bool use_smoothing = true;

	/* By default, always enable smoothing except on AES or uinput devices.
	 * AttrTabletSmoothing can override this, if necessary.
	 */
	_unref_(quirks) *q = libinput_device_get_quirks(&device->base);
	if (!q || !quirks_get_bool(q, QUIRK_ATTR_TABLET_SMOOTHING, &use_smoothing))
		use_smoothing = !is_aes && !is_virtual;

	/* Setting the history size to 1 means we never do any actual smoothing. */
	if (!use_smoothing)
		history_size = 1;

	tablet->history.size = history_size;
}

static bool
tablet_reject_device(struct evdev_device *device)
{
	struct libevdev *evdev = device->evdev;
	double w, h;
	bool has_xy, has_pen, has_btn_stylus, has_size;

	has_xy = libevdev_has_event_code(evdev, EV_ABS, ABS_X) &&
		 libevdev_has_event_code(evdev, EV_ABS, ABS_Y);
	has_pen = libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN);
	has_btn_stylus = libevdev_has_event_code(evdev, EV_KEY, BTN_STYLUS);
	has_size = evdev_device_get_size(device, &w, &h) == 0;

	if (has_xy && (has_pen || has_btn_stylus) && has_size)
		return false;

	evdev_log_bug_libinput(device,
			       "missing tablet capabilities:%s%s%s%s. "
			       "Ignoring this device.\n",
			       has_xy ? "" : " xy",
			       has_pen ? "" : " pen",
			       has_btn_stylus ? "" : " btn-stylus",
			       has_size ? "" : " resolution");
	return true;
}

static void
tablet_fix_tilt(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	struct libevdev *evdev = device->evdev;

	if (libevdev_has_event_code(evdev, EV_ABS, ABS_TILT_X) !=
	    libevdev_has_event_code(evdev, EV_ABS, ABS_TILT_Y)) {
		libevdev_disable_event_code(evdev, EV_ABS, ABS_TILT_X);
		libevdev_disable_event_code(evdev, EV_ABS, ABS_TILT_Y);
		return;
	}

	if (!libevdev_has_event_code(evdev, EV_ABS, ABS_TILT_X))
		return;

	/* Wacom has three types of devices:
	 * - symmetrical: [-90, 90], like the ISDv4 524c
	 * - asymmetrical: [-64, 63], like the Cintiq l3HDT
	 * - zero-based: [0, 127], like the Cintiq 12WX
	 *
	 * Note how the latter two cases have an even range and thus do
	 * not have a logical center value. But this is tilt and at
	 * least in the asymmetrical case we assume that hardware zero
	 * means vertical. So we cheat and adjust the range depending
	 * on whether it's odd, then use the center value.
	 *
	 * Since it's always the max that's one too low let's go with that and
	 * fix it if we run into a device where that isn't the case.
	 */
	for (unsigned int axis = ABS_TILT_X; axis <= ABS_TILT_Y; axis++) {
		struct input_absinfo abs = *libevdev_get_abs_info(evdev, axis);

		/* Don't touch axes reporting radians */
		if (abs.resolution != 0)
			continue;

		if ((int)absinfo_range(&abs) % 2 == 1)
			continue;

		abs.maximum += 1;
		libevdev_set_abs_info(evdev, axis, &abs);

		evdev_log_debug(device,
				"Adjusting %s range to [%d, %d]\n",
				libevdev_event_code_get_name(EV_ABS, axis),
				abs.minimum,
				abs.maximum);
	}
}

static int
tablet_init(struct tablet_dispatch *tablet, struct evdev_device *device)
{
	static unsigned int tablet_ids = 0;
	struct libinput *li = evdev_libinput_context(device);
	struct libevdev *evdev = device->evdev;
	enum libinput_tablet_tool_axis axis;
	int rc = -1;
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

	tablet->tablet_id = ++tablet_ids;
	tablet->base.dispatch_type = DISPATCH_TABLET;
	tablet->base.interface = &tablet_interface;
	tablet->device = device;
	tablet->status = TABLET_NONE;
	tablet->current_tool.type = LIBINPUT_TOOL_NONE;
	list_init(&tablet->tool_list);

	if (tablet_reject_device(device))
		goto out;

	bool is_aes = tablet_is_aes(device, wacom);
	bool is_virtual = evdev_device_is_virtual(device);
	bool is_display_tablet = tablet_is_display_tablet(wacom);

	if (!libevdev_has_event_code(evdev, EV_KEY, BTN_TOOL_PEN)) {
		libevdev_enable_event_code(evdev, EV_KEY, BTN_TOOL_PEN, NULL);
	}

	/* Our rotation code only works with Wacoms, let's wait until
	 * someone shouts */
	if (evdev_device_get_id_vendor(device) != VENDOR_ID_WACOM) {
		libevdev_disable_event_code(evdev, EV_KEY, BTN_TOOL_MOUSE);
		libevdev_disable_event_code(evdev, EV_KEY, BTN_TOOL_LENS);
	}

	tablet_fix_tilt(tablet, device);
	tablet_init_calibration(tablet, device, is_display_tablet);
	tablet_init_area(tablet, device);
	tablet_init_proximity_threshold(tablet, device);
	rc = tablet_init_accel(tablet, device);
	if (rc != 0)
		goto out;

	evdev_init_sendevents(device, &tablet->base);
	tablet_init_left_handed(device, wacom);
	tablet_init_smoothing(device, tablet, is_aes, is_virtual);

	for (axis = LIBINPUT_TABLET_TOOL_AXIS_X; axis <= LIBINPUT_TABLET_TOOL_AXIS_MAX;
	     axis++) {
		if (tablet_device_has_axis(tablet, axis))
			set_bit(tablet->axis_caps, axis);
	}

	tablet_set_status(tablet, TABLET_TOOL_OUT_OF_PROXIMITY);

	rc = 0;
out:
#ifdef HAVE_LIBWACOM
	if (wacom)
		libwacom_destroy(wacom);
	if (db)
		libinput_libwacom_unref(li);
#endif
	return rc;
}

struct evdev_dispatch *
evdev_tablet_create(struct evdev_device *device)
{
	struct tablet_dispatch *tablet;
	struct libinput *li = evdev_libinput_context(device);

	libinput_libwacom_ref(li);

	tablet = zalloc(sizeof *tablet);

	if (tablet_init(tablet, device) != 0) {
		tablet_destroy(&tablet->base);
		return NULL;
	}

	return &tablet->base;
}
