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

#include <inttypes.h>

#include "util-libinput.h"
#include "util-mem.h"
#include "util-strings.h"

#include "libevdev/libevdev.h"

static const char *
event_type_to_str(enum libinput_event_type evtype)
{
	const char *type;

	switch (evtype) {
	case LIBINPUT_EVENT_NONE:
		abort();
	case LIBINPUT_EVENT_DEVICE_ADDED:
		type = "DEVICE_ADDED";
		break;
	case LIBINPUT_EVENT_DEVICE_REMOVED:
		type = "DEVICE_REMOVED";
		break;
	case LIBINPUT_EVENT_KEYBOARD_KEY:
		type = "KEYBOARD_KEY";
		break;
	case LIBINPUT_EVENT_POINTER_MOTION:
		type = "POINTER_MOTION";
		break;
	case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
		type = "POINTER_MOTION_ABSOLUTE";
		break;
	case LIBINPUT_EVENT_POINTER_BUTTON:
		type = "POINTER_BUTTON";
		break;
	case LIBINPUT_EVENT_POINTER_AXIS:
		type = "POINTER_AXIS";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		type = "POINTER_SCROLL_WHEEL";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		type = "POINTER_SCROLL_FINGER";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		type = "POINTER_SCROLL_CONTINUOUS";
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
		type = "TOUCH_DOWN";
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		type = "TOUCH_MOTION";
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		type = "TOUCH_UP";
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		type = "TOUCH_CANCEL";
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
		type = "TOUCH_FRAME";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		type = "GESTURE_SWIPE_BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		type = "GESTURE_SWIPE_UPDATE";
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
		type = "GESTURE_SWIPE_END";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		type = "GESTURE_PINCH_BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		type = "GESTURE_PINCH_UPDATE";
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
		type = "GESTURE_PINCH_END";
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		type = "GESTURE_HOLD_BEGIN";
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_END:
		type = "GESTURE_HOLD_END";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
		type = "TABLET_TOOL_AXIS";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
		type = "TABLET_TOOL_PROXIMITY";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_TIP:
		type = "TABLET_TOOL_TIP";
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
		type = "TABLET_TOOL_BUTTON";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
		type = "TABLET_PAD_BUTTON";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_RING:
		type = "TABLET_PAD_RING";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_STRIP:
		type = "TABLET_PAD_STRIP";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_KEY:
		type = "TABLET_PAD_KEY";
		break;
	case LIBINPUT_EVENT_TABLET_PAD_DIAL:
		type = "TABLET_PAD_DIAL";
		break;
	case LIBINPUT_EVENT_SWITCH_TOGGLE:
		type = "SWITCH_TOGGLE";
		break;
	}

	return type;
}

static char *
print_event_header(struct libinput_event *ev, size_t event_count)
{
	/* use for pointer value only, do not dereference */
	static void *last_device = NULL;
	struct libinput_device *dev = libinput_event_get_device(ev);
	const char *type = event_type_to_str(libinput_event_get_type(ev));
	char count[10];

	if (event_count > 1)
		snprintf(count, sizeof(count), "%3zd ", event_count);
	else
		snprintf(count, sizeof(count), "    ");

	char prefix = (last_device != dev) ? '-' : ' ';
	last_device = dev;

	return strdup_printf("%c%-7s  %-23s %s",
			     prefix,
			     libinput_device_get_sysname(dev),
			     type,
			     count);
}

static void
print_event_time(char buf[16], uint32_t start_time, uint32_t time)
{
	snprintf(buf, 16, "%+6.3fs", start_time ? (time - start_time) / 1000.0 : 0);
}

static inline char *
print_device_options(struct libinput_device *dev)
{
	uint32_t scroll_methods, click_methods;
	_autofree_ char *tap = NULL;
	_autofree_ char *scroll = NULL;
	_autofree_ char *clickm = NULL;
	_autofree_ char *dwt = NULL;
	_autofree_ char *dwtp = NULL;
	_autofree_ char *pad = NULL;

	if (libinput_device_config_tap_get_finger_count(dev)) {
		tap = strdup_printf(
			" tap (dl %s)",
			onoff(libinput_device_config_tap_get_drag_lock_enabled(dev)));
	}

	scroll_methods = libinput_device_config_scroll_get_methods(dev);
	if (scroll_methods != LIBINPUT_CONFIG_SCROLL_NO_SCROLL) {
		scroll = strdup_printf(
			" scroll%s%s%s",
			(scroll_methods & LIBINPUT_CONFIG_SCROLL_2FG) ? "-2fg" : "",
			(scroll_methods & LIBINPUT_CONFIG_SCROLL_EDGE) ? "-edge" : "",
			(scroll_methods & LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN)
				? "-button"
				: "");
	}

	click_methods = libinput_device_config_click_get_methods(dev);
	if (click_methods != LIBINPUT_CONFIG_CLICK_METHOD_NONE) {
		clickm = strdup_printf(
			" click%s%s",
			(click_methods & LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS)
				? "-buttonareas"
				: "",
			(click_methods & LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER)
				? "-clickfinger"
				: "");
	}

	if (libinput_device_config_dwt_is_available(dev)) {
		dwt = strdup_printf(" dwt-%s",
				    onoff(libinput_device_config_dwt_get_enabled(dev) ==
					  LIBINPUT_CONFIG_DWT_ENABLED));
	}

	if (libinput_device_config_dwtp_is_available(dev)) {
		dwtp = strdup_printf(
			" dwtp-%s",
			onoff(libinput_device_config_dwtp_get_enabled(dev) ==
			      LIBINPUT_CONFIG_DWTP_ENABLED));
	}

	if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_PAD)) {
		pad = strdup_printf(
			" buttons:%d strips:%d rings:%d mode groups:%d",
			libinput_device_tablet_pad_get_num_buttons(dev),
			libinput_device_tablet_pad_get_num_strips(dev),
			libinput_device_tablet_pad_get_num_rings(dev),
			libinput_device_tablet_pad_get_num_mode_groups(dev));
	}

	return strdup_printf(
		"%s%s%s%s%s%s%s%s%s",
		tap ? tap : "",
		libinput_device_config_left_handed_is_available(dev) ? " left" : "",
		libinput_device_config_scroll_has_natural_scroll(dev) ? " scroll-nat"
								      : "",
		libinput_device_config_calibration_has_matrix(dev) ? " calib" : "",
		scroll ? scroll : "",
		clickm ? clickm : "",
		dwt ? dwt : "",
		dwtp ? dwtp : "",
		pad ? pad : "");
}

static char *
print_device_notify(struct libinput_event *ev)
{
	struct libinput_device *dev = libinput_event_get_device(ev);
	struct libinput_seat *seat = libinput_device_get_seat(dev);
	struct libinput_device_group *group;
	double w, h;
	static int next_group_id = 0;
	intptr_t group_id;
	_autofree_ char *size = NULL;
	_autofree_ char *ntouches = NULL;
	_autofree_ char *options = NULL;

	group = libinput_device_get_device_group(dev);
	group_id = (intptr_t)libinput_device_group_get_user_data(group);
	if (!group_id) {
		group_id = ++next_group_id;
		libinput_device_group_set_user_data(group, (void *)group_id);
	}

	if (libinput_device_get_size(dev, &w, &h) == 0)
		size = strdup_printf("  size %.0fx%.0fmm", w, h);

	if (libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TOUCH))
		ntouches = strdup_printf(" ntouches %d",
					 libinput_device_touch_get_touch_count(dev));

	if (libinput_event_get_type(ev) == LIBINPUT_EVENT_DEVICE_ADDED)
		options = print_device_options(dev);

	return strdup_printf(
		"%-33s %5s %7s group%-2d cap:%s%s%s%s%s%s%s%s%s%s",
		libinput_device_get_name(dev),
		libinput_seat_get_physical_name(seat),
		libinput_seat_get_logical_name(seat),
		(int)group_id,
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_KEYBOARD) ? "k"
										  : "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_POINTER) ? "p"
										 : "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TOUCH) ? "t"
									       : "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_GESTURE) ? "g"
										 : "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_TOOL)
			? "T"
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_TABLET_PAD)
			? "P"
			: "",
		libinput_device_has_capability(dev, LIBINPUT_DEVICE_CAP_SWITCH) ? "S"
										: "",
		size ? size : "",
		ntouches ? ntouches : "",
		options ? options : "");
}

static char *
print_key_event(struct libinput_event *ev, const struct libinput_print_options *opts)
{
	struct libinput_event_keyboard *k = libinput_event_get_keyboard_event(ev);
	enum libinput_key_state state;
	uint32_t key;
	const char *keyname;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_keyboard_get_time(k));
	state = libinput_event_keyboard_get_key_state(k);

	key = libinput_event_keyboard_get_key(k);
	if (!opts->show_keycodes && (key >= KEY_ESC && key < KEY_ZENKAKUHANKAKU)) {
		keyname = "***";
		key = -1;
	} else {
		keyname = libevdev_event_code_get_name(EV_KEY, key);
		keyname = keyname ? keyname : "???";
	}
	return strdup_printf("%s\t%s (%d) %s",
			     time,
			     keyname,
			     key,
			     state == LIBINPUT_KEY_STATE_PRESSED ? "pressed"
								 : "released");
}

static char *
print_motion_event(struct libinput_event *ev, const struct libinput_print_options *opts)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	double x = libinput_event_pointer_get_dx(p);
	double y = libinput_event_pointer_get_dy(p);
	double ux = libinput_event_pointer_get_dx_unaccelerated(p);
	double uy = libinput_event_pointer_get_dy_unaccelerated(p);
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_pointer_get_time(p));

	return strdup_printf("%s\t%6.2f/%6.2f (%+6.2f/%+6.2f)", time, x, y, ux, uy);
}

static char *
print_absmotion_event(struct libinput_event *ev,
		      const struct libinput_print_options *opts)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	double x =
		libinput_event_pointer_get_absolute_x_transformed(p,
								  opts->screen_width);
	double y =
		libinput_event_pointer_get_absolute_y_transformed(p,
								  opts->screen_height);
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_pointer_get_time(p));
	return strdup_printf("%s\t%6.2f/%6.2f", time, x, y);
}

static char *
print_pointer_button_event(struct libinput_event *ev,
			   const struct libinput_print_options *opts)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	enum libinput_button_state state;
	const char *buttonname;
	int button;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_pointer_get_time(p));

	button = libinput_event_pointer_get_button(p);
	buttonname = libevdev_event_code_get_name(EV_KEY, button);

	state = libinput_event_pointer_get_button_state(p);
	return strdup_printf("%s\t%s (%d) %s, seat count: %u",
			     time,
			     buttonname ? buttonname : "???",
			     button,
			     state == LIBINPUT_BUTTON_STATE_PRESSED ? "pressed"
								    : "released",
			     libinput_event_pointer_get_seat_button_count(p));
}

static char *
print_tablet_axes(struct libinput_event_tablet_tool *t)
{
	struct libinput_tablet_tool *tool = libinput_event_tablet_tool_get_tool(t);
	double x, y;
	_autofree_ char *tilt = NULL;
	_autofree_ char *distance = NULL;
	_autofree_ char *rot = NULL;
	_autofree_ char *whl = NULL;
	_autofree_ char *sld = NULL;
	_autofree_ char *size = NULL;

#define changed_sym(ev, ax) \
	(libinput_event_tablet_tool_##ax##_has_changed(ev) ? "*" : " ")

	if (libinput_tablet_tool_has_tilt(tool)) {
		x = libinput_event_tablet_tool_get_tilt_x(t);
		y = libinput_event_tablet_tool_get_tilt_y(t);
		tilt = strdup_printf("\ttilt: %.2f%s/%.2f%s",
				     x,
				     changed_sym(t, tilt_x),
				     y,
				     changed_sym(t, tilt_y));
	}

	if (libinput_tablet_tool_has_distance(tool) ||
	    libinput_tablet_tool_has_pressure(tool)) {
		double dist = libinput_event_tablet_tool_get_distance(t);
		double pressure = libinput_event_tablet_tool_get_pressure(t);
		if (dist)
			distance = strdup_printf("\tdistance: %.2f%s",
						 dist,
						 changed_sym(t, distance));
		else
			distance = strdup_printf("\tpressure: %.2f%s",
						 pressure,
						 changed_sym(t, pressure));
	}

	if (libinput_tablet_tool_has_rotation(tool)) {
		double rotation = libinput_event_tablet_tool_get_rotation(t);
		rot = strdup_printf("\trotation: %6.2f%s",
				    rotation,
				    changed_sym(t, rotation));
	}

	if (libinput_tablet_tool_has_slider(tool)) {
		double slider = libinput_event_tablet_tool_get_slider_position(t);
		sld = strdup_printf("\tslider: %.2f%s", slider, changed_sym(t, slider));
	}

	if (libinput_tablet_tool_has_wheel(tool)) {
		double wheel = libinput_event_tablet_tool_get_wheel_delta(t);
		double delta = libinput_event_tablet_tool_get_wheel_delta_discrete(t);
		whl = strdup_printf("\twheel: %.2f%s (%d)",
				    wheel,
				    changed_sym(t, wheel),
				    (int)delta);
	}

	if (libinput_tablet_tool_has_size(tool)) {
		double major = libinput_event_tablet_tool_get_size_major(t);
		double minor = libinput_event_tablet_tool_get_size_minor(t);
		size = strdup_printf("\tsize: %.2f%s/%.2f%s",
				     major,
				     changed_sym(t, size_major),
				     minor,
				     changed_sym(t, size_minor));
	}

	x = libinput_event_tablet_tool_get_x(t);
	y = libinput_event_tablet_tool_get_y(t);
	return strdup_printf("\t%.2f%s/%.2f%s%s%s%s%s%s%s",
			     x,
			     changed_sym(t, x),
			     y,
			     changed_sym(t, y),
			     tilt ? tilt : "",
			     distance ? distance : "",
			     rot ? rot : "",
			     whl ? whl : "",
			     sld ? sld : "",
			     size ? size : "");
}

static char *
print_tablet_tip_event(struct libinput_event *ev,
		       const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_tool *t = libinput_event_get_tablet_tool_event(ev);
	enum libinput_tablet_tool_tip_state state;
	char time[16];

	print_event_time(time,
			 opts->start_time,
			 libinput_event_tablet_tool_get_time(t));

	_autofree_ char *axes = print_tablet_axes(t);

	state = libinput_event_tablet_tool_get_tip_state(t);
	return strdup_printf("%s\t%s %s",
			     time,
			     axes,
			     state == LIBINPUT_TABLET_TOOL_TIP_DOWN ? "down" : "up");
}

static char *
print_tablet_button_event(struct libinput_event *ev,
			  const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_tool *p = libinput_event_get_tablet_tool_event(ev);
	enum libinput_button_state state;
	const char *buttonname;
	int button;
	char time[16];

	print_event_time(time,
			 opts->start_time,
			 libinput_event_tablet_tool_get_time(p));

	button = libinput_event_tablet_tool_get_button(p);
	buttonname = libevdev_event_code_get_name(EV_KEY, button);

	state = libinput_event_tablet_tool_get_button_state(p);
	return strdup_printf("%s\ts%3d (%s) %s, seat count: %u",
			     time,
			     button,
			     buttonname ? buttonname : "???",
			     state == LIBINPUT_BUTTON_STATE_PRESSED ? "pressed"
								    : "released",
			     libinput_event_tablet_tool_get_seat_button_count(p));
}

static char *
print_legacy_pointer_axis_event(struct libinput_event *ev,
				const struct libinput_print_options *opts)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	double v = 0, h = 0;
	int v_discrete = 0, h_discrete = 0;
	const char *have_vert = "", *have_horiz = "";
	const char *source = NULL;
	enum libinput_pointer_axis axis;
	char time[16];

	switch (libinput_event_pointer_get_axis_source(p)) {
	case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL:
		source = "wheel";
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_FINGER:
		source = "finger";
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS:
		source = "continuous";
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL_TILT:
		/* No device uses wheel tilt */
		abort();
		break;
	default:
		abort();
		break;
	}

	axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
	if (libinput_event_pointer_has_axis(p, axis)) {
		v = libinput_event_pointer_get_axis_value(p, axis);
		v_discrete = libinput_event_pointer_get_axis_value_discrete(p, axis);
		have_vert = "*";
	}
	axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
	if (libinput_event_pointer_has_axis(p, axis)) {
		h = libinput_event_pointer_get_axis_value(p, axis);
		h_discrete = libinput_event_pointer_get_axis_value_discrete(p, axis);
		have_horiz = "*";
	}

	print_event_time(time, opts->start_time, libinput_event_pointer_get_time(p));
	return strdup_printf("%s\tvert %.2f/%d%s horiz %.2f/%d%s (%s)",
			     time,
			     v,
			     v_discrete,
			     have_vert,
			     h,
			     h_discrete,
			     have_horiz,
			     source);
}

static char *
print_pointer_axis_event(struct libinput_event *ev,
			 const struct libinput_print_options *opts)
{
	struct libinput_event_pointer *p = libinput_event_get_pointer_event(ev);
	double v = 0, h = 0, v120 = 0, h120 = 0;
	const char *have_vert = "", *have_horiz = "";
	const char *source = NULL;
	enum libinput_pointer_axis axis;
	enum libinput_event_type type;
	char time[16];

	type = libinput_event_get_type(ev);

	switch (type) {
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		source = "wheel";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		source = "finger";
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		source = "continuous";
		break;
	default:
		abort();
		break;
	}

	axis = LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL;
	if (libinput_event_pointer_has_axis(p, axis)) {
		v = libinput_event_pointer_get_scroll_value(p, axis);
		if (type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL)
			v120 = libinput_event_pointer_get_scroll_value_v120(p, axis);
		have_vert = "*";
	}
	axis = LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL;
	if (libinput_event_pointer_has_axis(p, axis)) {
		h = libinput_event_pointer_get_scroll_value(p, axis);
		if (type == LIBINPUT_EVENT_POINTER_SCROLL_WHEEL)
			h120 = libinput_event_pointer_get_scroll_value_v120(p, axis);
		have_horiz = "*";
	}

	print_event_time(time, opts->start_time, libinput_event_pointer_get_time(p));
	return strdup_printf("%s\tvert %.2f/%.1f%s horiz %.2f/%.1f%s (%s)",
			     time,
			     v,
			     v120,
			     have_vert,
			     h,
			     h120,
			     have_horiz,
			     source);
}

static char *
print_tablet_axis_event(struct libinput_event *ev,
			const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_tool *t = libinput_event_get_tablet_tool_event(ev);
	char time[16];

	print_event_time(time,
			 opts->start_time,
			 libinput_event_tablet_tool_get_time(t));
	_autofree_ char *axes = print_tablet_axes(t);

	return strdup_printf("%s\t%s", time, axes);
}

static char *
print_proximity_event(struct libinput_event *ev,
		      const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_tool *t = libinput_event_get_tablet_tool_event(ev);
	struct libinput_tablet_tool *tool = libinput_event_tablet_tool_get_tool(t);
	enum libinput_tablet_tool_proximity_state state;
	const char *tool_str, *state_str;
	char time[16];
	_autofree_ char *axes = NULL;
	_autofree_ char *proxin = NULL;

	switch (libinput_tablet_tool_get_type(tool)) {
	case LIBINPUT_TABLET_TOOL_TYPE_PEN:
		tool_str = "pen";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_ERASER:
		tool_str = "eraser";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_BRUSH:
		tool_str = "brush";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_PENCIL:
		tool_str = "pencil";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_AIRBRUSH:
		tool_str = "airbrush";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_MOUSE:
		tool_str = "mouse";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_LENS:
		tool_str = "lens";
		break;
	case LIBINPUT_TABLET_TOOL_TYPE_TOTEM:
		tool_str = "totem";
		break;
	default:
		abort();
	}

	state = libinput_event_tablet_tool_get_proximity_state(t);

	print_event_time(time,
			 opts->start_time,
			 libinput_event_tablet_tool_get_time(t));

	if (state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN) {
		axes = print_tablet_axes(t);
		state_str = "proximity-in";
	} else if (state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_OUT) {
		axes = print_tablet_axes(t);
		state_str = "proximity-out";
	} else {
		abort();
	}

	if (state == LIBINPUT_TABLET_TOOL_PROXIMITY_STATE_IN) {
		proxin = strdup_printf(
			"\taxes:%s%s%s%s%s%s\tbtn:%s%s%s%s%s%s%s%s%s%s",
			libinput_tablet_tool_has_distance(tool) ? "d" : "",
			libinput_tablet_tool_has_pressure(tool) ? "p" : "",
			libinput_tablet_tool_has_tilt(tool) ? "t" : "",
			libinput_tablet_tool_has_rotation(tool) ? "r" : "",
			libinput_tablet_tool_has_slider(tool) ? "s" : "",
			libinput_tablet_tool_has_wheel(tool) ? "w" : "",
			libinput_tablet_tool_has_size(tool) ? "S" : "",
			libinput_tablet_tool_has_button(tool, BTN_TOUCH) ? "T" : "",
			libinput_tablet_tool_has_button(tool, BTN_STYLUS) ? "S" : "",
			libinput_tablet_tool_has_button(tool, BTN_STYLUS2) ? "S2" : "",
			libinput_tablet_tool_has_button(tool, BTN_LEFT) ? "L" : "",
			libinput_tablet_tool_has_button(tool, BTN_MIDDLE) ? "M" : "",
			libinput_tablet_tool_has_button(tool, BTN_RIGHT) ? "R" : "",
			libinput_tablet_tool_has_button(tool, BTN_SIDE) ? "Sd" : "",
			libinput_tablet_tool_has_button(tool, BTN_EXTRA) ? "Ex" : "",
			libinput_tablet_tool_has_button(tool, BTN_0) ? "0" : "");
	}

	return strdup_printf("%s\t%s\t%-8s (%#" PRIx64 ", id %#" PRIx64 ") %s%s",
			     time,
			     axes ? axes : "",
			     tool_str,
			     libinput_tablet_tool_get_serial(tool),
			     libinput_tablet_tool_get_tool_id(tool),
			     state_str,
			     proxin ? proxin : "");
}

static char *
print_touch_event(struct libinput_event *ev, const struct libinput_print_options *opts)
{
	struct libinput_event_touch *t = libinput_event_get_touch_event(ev);
	enum libinput_event_type type = libinput_event_get_type(ev);
	char time[16];
	_autofree_ char *slot = NULL;
	_autofree_ char *pos = NULL;

	print_event_time(time, opts->start_time, libinput_event_touch_get_time(t));

	if (type != LIBINPUT_EVENT_TOUCH_FRAME) {
		slot = strdup_printf("%d (%d)",
				     libinput_event_touch_get_slot(t),
				     libinput_event_touch_get_seat_slot(t));
	}

	if (type == LIBINPUT_EVENT_TOUCH_DOWN || type == LIBINPUT_EVENT_TOUCH_MOTION) {
		double x =
			libinput_event_touch_get_x_transformed(t, opts->screen_width);
		double y =
			libinput_event_touch_get_y_transformed(t, opts->screen_height);
		double xmm = libinput_event_touch_get_x(t);
		double ymm = libinput_event_touch_get_y(t);

		pos = strdup_printf(" %5.2f/%5.2f (%5.2f/%5.2fmm)", x, y, xmm, ymm);
	}

	return strdup_printf("%s\t%s%s", time, slot ? slot : "", pos ? pos : "");
}

static char *
print_gesture_event_without_coords(struct libinput_event *ev,
				   const struct libinput_print_options *opts)
{
	struct libinput_event_gesture *t = libinput_event_get_gesture_event(ev);
	int finger_count = libinput_event_gesture_get_finger_count(t);
	int cancelled = 0;
	enum libinput_event_type type;
	char time[16];

	type = libinput_event_get_type(ev);

	if (type == LIBINPUT_EVENT_GESTURE_SWIPE_END ||
	    type == LIBINPUT_EVENT_GESTURE_PINCH_END ||
	    type == LIBINPUT_EVENT_GESTURE_HOLD_END)
		cancelled = libinput_event_gesture_get_cancelled(t);

	print_event_time(time, opts->start_time, libinput_event_gesture_get_time(t));
	return strdup_printf("%s\t%d%s",
			     time,
			     finger_count,
			     cancelled ? " cancelled" : "");
}

static char *
print_gesture_event_with_coords(struct libinput_event *ev,
				const struct libinput_print_options *opts)
{
	struct libinput_event_gesture *t = libinput_event_get_gesture_event(ev);
	double dx = libinput_event_gesture_get_dx(t);
	double dy = libinput_event_gesture_get_dy(t);
	double dx_unaccel = libinput_event_gesture_get_dx_unaccelerated(t);
	double dy_unaccel = libinput_event_gesture_get_dy_unaccelerated(t);
	char time[16];
	_autofree_ char *pinch = NULL;

	print_event_time(time, opts->start_time, libinput_event_gesture_get_time(t));

	if (libinput_event_get_type(ev) == LIBINPUT_EVENT_GESTURE_PINCH_UPDATE) {
		double scale = libinput_event_gesture_get_scale(t);
		double angle = libinput_event_gesture_get_angle_delta(t);

		pinch = strdup_printf(" %5.2f @ %5.2f", scale, angle);
	}

	return strdup_printf("%s\t%d %5.2f/%5.2f (%5.2f/%5.2f unaccelerated)%s",
			     time,
			     libinput_event_gesture_get_finger_count(t),
			     dx,
			     dy,
			     dx_unaccel,
			     dy_unaccel,
			     pinch ? pinch : "");
}

static char *
print_tablet_pad_button_event(struct libinput_event *ev,
			      const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_pad *p = libinput_event_get_tablet_pad_event(ev);
	struct libinput_tablet_pad_mode_group *group;
	enum libinput_button_state state;
	unsigned int button, mode;
	char time[16];
	const char *toggle = NULL;

	print_event_time(time, opts->start_time, libinput_event_tablet_pad_get_time(p));

	button = libinput_event_tablet_pad_get_button_number(p),
	state = libinput_event_tablet_pad_get_button_state(p);
	mode = libinput_event_tablet_pad_get_mode(p);

	group = libinput_event_tablet_pad_get_mode_group(p);
	if (libinput_tablet_pad_mode_group_button_is_toggle(group, button))
		toggle = " <mode toggle>";

	return strdup_printf("%3d %s (mode %d)%s",
			     button,
			     state == LIBINPUT_BUTTON_STATE_PRESSED ? "pressed"
								    : "released",
			     mode,
			     toggle ? toggle : "");
}

static char *
print_tablet_pad_ring_event(struct libinput_event *ev,
			    const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_pad *p = libinput_event_get_tablet_pad_event(ev);
	const char *source = NULL;
	unsigned int mode;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_tablet_pad_get_time(p));

	switch (libinput_event_tablet_pad_get_ring_source(p)) {
	case LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER:
		source = "finger";
		break;
	case LIBINPUT_TABLET_PAD_RING_SOURCE_UNKNOWN:
		source = "unknown";
		break;
	}

	mode = libinput_event_tablet_pad_get_mode(p);
	return strdup_printf("%s\tring %d position %.2f (source %s) (mode %d)",
			     time,
			     libinput_event_tablet_pad_get_ring_number(p),
			     libinput_event_tablet_pad_get_ring_position(p),
			     source,
			     mode);
}

static char *
print_tablet_pad_strip_event(struct libinput_event *ev,
			     const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_pad *p = libinput_event_get_tablet_pad_event(ev);
	const char *source = NULL;
	unsigned int mode;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_tablet_pad_get_time(p));

	switch (libinput_event_tablet_pad_get_strip_source(p)) {
	case LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER:
		source = "finger";
		break;
	case LIBINPUT_TABLET_PAD_STRIP_SOURCE_UNKNOWN:
		source = "unknown";
		break;
	}

	mode = libinput_event_tablet_pad_get_mode(p);
	return strdup_printf("%s\tstrip %d position %.2f (source %s) (mode %d)",
			     time,
			     libinput_event_tablet_pad_get_strip_number(p),
			     libinput_event_tablet_pad_get_strip_position(p),
			     source,
			     mode);
}

static char *
print_tablet_pad_key_event(struct libinput_event *ev,
			   const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_pad *p = libinput_event_get_tablet_pad_event(ev);
	enum libinput_key_state state;
	uint32_t key;
	const char *keyname;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_tablet_pad_get_time(p));

	key = libinput_event_tablet_pad_get_key(p);
	if (!opts->show_keycodes && (key >= KEY_ESC && key < KEY_ZENKAKUHANKAKU)) {
		keyname = "***";
		key = -1;
	} else {
		keyname = libevdev_event_code_get_name(EV_KEY, key);
		keyname = keyname ? keyname : "???";
	}
	state = libinput_event_tablet_pad_get_key_state(p);
	return strdup_printf("%s\t%s (%d) %s",
			     time,
			     keyname,
			     key,
			     state == LIBINPUT_KEY_STATE_PRESSED ? "pressed"
								 : "released");
}

static char *
print_tablet_pad_dial_event(struct libinput_event *ev,
			    const struct libinput_print_options *opts)
{
	struct libinput_event_tablet_pad *p = libinput_event_get_tablet_pad_event(ev);
	unsigned int mode;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_tablet_pad_get_time(p));

	mode = libinput_event_tablet_pad_get_mode(p);
	return strdup_printf("%s\tdial %d delta %.2f (mode %d)",
			     time,
			     libinput_event_tablet_pad_get_dial_number(p),
			     libinput_event_tablet_pad_get_dial_delta_v120(p),
			     mode);
}

static char *
print_switch_event(struct libinput_event *ev, const struct libinput_print_options *opts)
{
	struct libinput_event_switch *sw = libinput_event_get_switch_event(ev);
	enum libinput_switch_state state;
	const char *which;
	char time[16];

	print_event_time(time, opts->start_time, libinput_event_switch_get_time(sw));

	switch (libinput_event_switch_get_switch(sw)) {
	case LIBINPUT_SWITCH_LID:
		which = "lid";
		break;
	case LIBINPUT_SWITCH_TABLET_MODE:
		which = "tablet-mode";
		break;
	default:
		abort();
	}

	state = libinput_event_switch_get_switch_state(sw);

	return strdup_printf("%s\tswitch %s state %d", time, which, state);
}

char *
libinput_event_to_str(struct libinput_event *ev,
		      size_t event_repeat_count,
		      const struct libinput_print_options *options)
{
	enum libinput_event_type type = libinput_event_get_type(ev);
	_autofree_ char *event_header = print_event_header(ev, event_repeat_count);
	_autofree_ char *event_str = NULL;

	struct libinput_print_options opts = {
		.start_time = options ? options->start_time : 0,
		.show_keycodes = options ? options->show_keycodes : true,
		.screen_width = (options && options->screen_width > 0)
					? options->screen_width
					: 100,
		.screen_height = (options && options->screen_height > 0)
					 ? options->screen_height
					 : 100,

	};

	switch (type) {
	case LIBINPUT_EVENT_NONE:
		abort();
	case LIBINPUT_EVENT_DEVICE_ADDED:
		event_str = print_device_notify(ev);
		break;
	case LIBINPUT_EVENT_DEVICE_REMOVED:
		event_str = print_device_notify(ev);
		break;
	case LIBINPUT_EVENT_KEYBOARD_KEY:
		event_str = print_key_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_POINTER_MOTION:
		event_str = print_motion_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
		event_str = print_absmotion_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_POINTER_BUTTON:
		event_str = print_pointer_button_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_POINTER_AXIS:
		event_str = print_legacy_pointer_axis_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		event_str = print_pointer_axis_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
	case LIBINPUT_EVENT_TOUCH_MOTION:
	case LIBINPUT_EVENT_TOUCH_UP:
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		event_str = print_touch_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
		event_str = print_touch_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		event_str = print_gesture_event_with_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		event_str = print_gesture_event_with_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_END:
		event_str = print_gesture_event_without_coords(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
		event_str = print_tablet_axis_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
		event_str = print_proximity_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_TIP:
		event_str = print_tablet_tip_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
		event_str = print_tablet_button_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
		event_str = print_tablet_pad_button_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_RING:
		event_str = print_tablet_pad_ring_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_STRIP:
		event_str = print_tablet_pad_strip_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_KEY:
		event_str = print_tablet_pad_key_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_DIAL:
		event_str = print_tablet_pad_dial_event(ev, &opts);
		break;
	case LIBINPUT_EVENT_SWITCH_TOGGLE:
		event_str = print_switch_event(ev, &opts);
		break;
	}

	return strdup_printf("%s %s", event_header, event_str);
}
