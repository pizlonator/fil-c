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

#include <config.h>

#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef HAVE_LIBWACOM
#include <libwacom/libwacom.h>
#endif

#include "libinput-util.h"
#include "litest.h"

START_TEST(pad_cap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert(
		libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TABLET_PAD));
}
END_TEST

START_TEST(pad_no_cap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert(!libinput_device_has_capability(device,
						      LIBINPUT_DEVICE_CAP_TABLET_PAD));
}
END_TEST

START_TEST(pad_time)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	unsigned int code;
	uint64_t time, time_usec, oldtime;
	bool has_buttons = false;

	litest_drain_events(li);

	for (code = BTN_0; code < BTN_DIGI; code++) {
		if (!libevdev_has_event_code(dev->evdev, EV_KEY, code))
			continue;

		has_buttons = true;

		litest_button_click(dev, code, 1);
		litest_button_click(dev, code, 0);
		litest_dispatch(li);

		break;
	}

	if (!has_buttons)
		return LITEST_NOT_APPLICABLE;

	ev = libinput_get_event(li);
	litest_assert_notnull(ev);
	litest_assert_event_type(ev, LIBINPUT_EVENT_TABLET_PAD_BUTTON);
	pev = libinput_event_get_tablet_pad_event(ev);
	time = libinput_event_tablet_pad_get_time(pev);
	time_usec = libinput_event_tablet_pad_get_time_usec(pev);

	litest_assert(time != 0);
	litest_assert(time == time_usec / 1000);

	libinput_event_destroy(ev);

	litest_drain_events(li);
	msleep(10);

	litest_button_click(dev, code, 1);
	litest_button_click(dev, code, 0);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	litest_assert_event_type(ev, LIBINPUT_EVENT_TABLET_PAD_BUTTON);
	pev = libinput_event_get_tablet_pad_event(ev);

	oldtime = time;
	time = libinput_event_tablet_pad_get_time(pev);
	time_usec = libinput_event_tablet_pad_get_time_usec(pev);

	litest_assert(time > oldtime);
	litest_assert(time != 0);
	litest_assert(time == time_usec / 1000);

	libinput_event_destroy(ev);
}
END_TEST

START_TEST(pad_num_buttons_libwacom)
{
#ifdef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	WacomDeviceDatabase *db = NULL;
	WacomDevice *wacom = NULL;
	unsigned int nb_lw, nb;

	db = libwacom_database_new();
	litest_assert_notnull(db);

	wacom = libwacom_new_from_usbid(db,
					libevdev_get_id_vendor(dev->evdev),
					libevdev_get_id_product(dev->evdev),
					NULL);
	litest_assert_notnull(wacom);

	nb_lw = libwacom_get_num_buttons(wacom);
	nb = libinput_device_tablet_pad_get_num_buttons(device);

	litest_assert_int_eq(nb, nb_lw);

	libwacom_destroy(wacom);
	libwacom_database_destroy(db);
#endif
}
END_TEST

START_TEST(pad_num_buttons)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	unsigned int code;
	int nbuttons = 0;

	for (code = BTN_0; code < KEY_OK; code++) {
		/* BTN_STYLUS is set for compatibility reasons but not
		 * actually hooked up */
		if (code == BTN_STYLUS)
			continue;

		if (libevdev_has_event_code(dev->evdev, EV_KEY, code))
			nbuttons++;
	}

	litest_assert_int_eq(libinput_device_tablet_pad_get_num_buttons(device),
			     nbuttons);
}
END_TEST

START_TEST(pad_button_intuos)
{
#ifndef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	unsigned int code;
	unsigned int expected_number = 0;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	unsigned int count = 0;

	/* Intuos button mapping is sequential up from BTN_0 and continues
	 * with BTN_A */
	if (!libevdev_has_event_code(dev->evdev, EV_KEY, BTN_0))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	for (code = BTN_0; code < BTN_DIGI; code++) {
		/* Skip over the BTN_MOUSE and BTN_JOYSTICK range */
		if ((code >= BTN_MOUSE && code < BTN_JOYSTICK) || (code >= BTN_DIGI)) {
			litest_assert(
				!libevdev_has_event_code(dev->evdev, EV_KEY, code));
			continue;
		}

		if (!libevdev_has_event_code(dev->evdev, EV_KEY, code))
			continue;

		litest_button_click(dev, code, 1);
		litest_button_click(dev, code, 0);
		litest_dispatch(li);

		count++;

		ev = libinput_get_event(li);
		pev = litest_is_pad_button_event(ev,
						 expected_number,
						 LIBINPUT_BUTTON_STATE_PRESSED);
		ev = libinput_event_tablet_pad_get_base_event(pev);
		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		pev = litest_is_pad_button_event(ev,
						 expected_number,
						 LIBINPUT_BUTTON_STATE_RELEASED);
		ev = libinput_event_tablet_pad_get_base_event(pev);
		libinput_event_destroy(ev);

		expected_number++;
	}

	litest_assert_empty_queue(li);

	litest_assert_int_ge(count, 1U);
#endif
}
END_TEST

START_TEST(pad_button_bamboo)
{
#ifndef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	unsigned int code;
	unsigned int expected_number = 0;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	unsigned int count = 0;

	if (!libevdev_has_event_code(dev->evdev, EV_KEY, BTN_LEFT))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	for (code = BTN_LEFT; code < BTN_JOYSTICK; code++) {
		if (!libevdev_has_event_code(dev->evdev, EV_KEY, code))
			continue;

		litest_button_click(dev, code, 1);
		litest_button_click(dev, code, 0);
		litest_dispatch(li);

		count++;

		ev = libinput_get_event(li);
		pev = litest_is_pad_button_event(ev,
						 expected_number,
						 LIBINPUT_BUTTON_STATE_PRESSED);
		ev = libinput_event_tablet_pad_get_base_event(pev);
		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		pev = litest_is_pad_button_event(ev,
						 expected_number,
						 LIBINPUT_BUTTON_STATE_RELEASED);
		ev = libinput_event_tablet_pad_get_base_event(pev);
		libinput_event_destroy(ev);

		expected_number++;
	}

	litest_assert_empty_queue(li);

	litest_assert_int_gt(count, 3U);
#endif
}
END_TEST

START_TEST(pad_button_libwacom)
{
#ifdef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	WacomDeviceDatabase *db = NULL;
	WacomDevice *wacom = NULL;

	db = libwacom_database_new();
	assert(db);

	wacom = libwacom_new_from_usbid(db,
					libevdev_get_id_vendor(dev->evdev),
					libevdev_get_id_product(dev->evdev),
					NULL);
	assert(wacom);

	litest_drain_events(li);

	for (int i = 0; i < libwacom_get_num_buttons(wacom); i++) {
		unsigned int code;

		code = libwacom_get_button_evdev_code(wacom, 'A' + i);

		litest_button_click(dev, code, 1);
		litest_button_click(dev, code, 0);
		litest_dispatch(li);

		litest_assert_pad_button_event(li, i, LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_pad_button_event(li, i, LIBINPUT_BUTTON_STATE_RELEASED);
	}

	libwacom_destroy(wacom);
	libwacom_database_destroy(db);
#endif
}
END_TEST

START_TEST(pad_button_mode_groups)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	unsigned int expected_mode = 0;
	int evdev_codes[KEY_OK - BTN_0] = { 0 };
#ifdef HAVE_LIBWACOM
	WacomDeviceDatabase *db = NULL;
	WacomDevice *wacom = NULL;

	db = libwacom_database_new();
	litest_assert_notnull(db);

	wacom = libwacom_new_from_usbid(db,
					libevdev_get_id_vendor(dev->evdev),
					libevdev_get_id_product(dev->evdev),
					NULL);
	if (wacom) {
		for (size_t i = 0; i < ARRAY_LENGTH(evdev_codes); i++) {
			evdev_codes[i] = libwacom_get_button_evdev_code(wacom, 'A' + i);
		}
	}

	libwacom_destroy(wacom);
	libwacom_database_destroy(db);
#else
	for (size_t i = 0; i < ARRAY_LENGTH(evdev_codes); i++) {
		evdev_codes[i] = BTN_0 + 1;
	}
#endif

	litest_drain_events(li);

	for (unsigned int b = 0; b < ARRAY_LENGTH(evdev_codes) && evdev_codes[b] != 0;
	     b++) {
		unsigned int code = evdev_codes[b];
		unsigned int mode, index;
		struct libinput_tablet_pad_mode_group *group;

		if (!libevdev_has_event_code(dev->evdev, EV_KEY, code))
			continue;

		litest_button_click(dev, code, 1);
		litest_button_click(dev, code, 0);
		litest_dispatch(li);

		switch (code) {
		case BTN_STYLUS:
			litest_assert_empty_queue(li);
			continue;
		default:
			break;
		}

		ev = libinput_get_event(li);
		litest_assert_event_type(ev, LIBINPUT_EVENT_TABLET_PAD_BUTTON);
		pev = libinput_event_get_tablet_pad_event(ev);

		group = libinput_event_tablet_pad_get_mode_group(pev);
#ifdef HAVE_LIBWACOM_BUTTON_MODESWITCH_MODE
		/* One of the few devices that has very specific mode switch buttons
		 * so we hardcode these here in the test */
		if (dev->which == LITEST_WACOM_MOBILESTUDIO_PRO_16_PAD) {
			switch (code) {
			case BTN_9:
				expected_mode = 0;
				break;
			case BTN_A:
				expected_mode = 1;
				break;
			case BTN_B:
				expected_mode = 2;
				break;
			case BTN_C:
				expected_mode = 3;
				break;
			default:
				break;
			}
		} else
#endif
			if (libinput_tablet_pad_mode_group_button_is_toggle(group, b)) {
			int num_modes =
				libinput_tablet_pad_mode_group_get_num_modes(group);
			expected_mode = (expected_mode + 1) % num_modes;
		}
		mode = libinput_event_tablet_pad_get_mode(pev);
		litest_assert_int_eq(mode, expected_mode);

		index = libinput_tablet_pad_mode_group_get_index(group);
		litest_assert_int_eq(index, 0U);

		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		litest_assert_event_type(ev, LIBINPUT_EVENT_TABLET_PAD_BUTTON);
		pev = libinput_event_get_tablet_pad_event(ev);

		mode = libinput_event_tablet_pad_get_mode(pev);
		litest_assert_int_eq(mode, expected_mode);
		group = libinput_event_tablet_pad_get_mode_group(pev);
		index = libinput_tablet_pad_mode_group_get_index(group);
		litest_assert_int_eq(index, 0U);
		libinput_event_destroy(ev);
	}

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(pad_has_ring)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	int nrings;

	nrings = libinput_device_tablet_pad_get_num_rings(device);
	litest_assert_int_ge(nrings, 1);
}
END_TEST

START_TEST(pad_ring)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	int val;
	double degrees, expected;
	int min, max;
	int step_size;
	int nevents = 0;

	litest_pad_ring_start(dev, 10);

	litest_drain_events(li);

	/* Wacom's 0 value is at 275 degrees */
	expected = 270;

	min = libevdev_get_abs_minimum(dev->evdev, ABS_WHEEL);
	max = libevdev_get_abs_maximum(dev->evdev, ABS_WHEEL);
	step_size = 360 / (max - min + 1);

	/* This is a bit strange because we rely on kernel filtering here.
	   The litest_*() functions take a percentage, but mapping this to
	   the pads 72 or 36 range pad ranges is lossy and a bit
	   unpredictable. So instead we increase by a small percentage,
	   expecting *most* events to be filtered by the kernel because they
	   resolve to the same integer value as the previous event. Whenever
	   an event gets through, we expect that to be the next integer
	   value in the range and thus the next step on the circle.
	 */
	for (val = 0; val < 100.0; val += 1) {
		litest_pad_ring_change(dev, val);
		litest_dispatch(li);

		ev = libinput_get_event(li);
		if (!ev)
			continue;

		nevents++;
		pev = litest_is_pad_ring_event(ev,
					       0,
					       LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER);

		degrees = libinput_event_tablet_pad_get_ring_position(pev);
		litest_assert_double_ge(degrees, 0.0);
		litest_assert_double_lt(degrees, 360.0);

		litest_assert_double_eq(degrees, expected);

		libinput_event_destroy(ev);
		expected = fmod(degrees + step_size, 360);
	}

	litest_assert_int_eq(nevents, 360 / step_size - 1);

	litest_pad_ring_end(dev);
}
END_TEST

START_TEST(pad_ring_finger_up)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	double degrees;

	litest_pad_ring_start(dev, 10);

	litest_drain_events(li);

	litest_pad_ring_end(dev);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	pev = litest_is_pad_ring_event(ev, 0, LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER);

	degrees = libinput_event_tablet_pad_get_ring_position(pev);
	litest_assert_double_eq(degrees, -1.0);
	libinput_event_destroy(ev);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(pad_has_dial)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	int ndials;
	int expected_ndials = 1;

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_HWHEEL))
		expected_ndials = 2;

	ndials = libinput_device_tablet_pad_get_num_dials(device);
	litest_assert_int_eq(ndials, expected_ndials);
}
END_TEST

START_TEST(pad_dial_low_res)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	unsigned int code = 0;

	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL))
		code = REL_WHEEL;
	if (libevdev_has_event_code(dev->evdev, EV_REL, REL_DIAL))
		code = REL_DIAL;

	litest_drain_events(li);

	for (int i = 0; i < 10; i++) {
		int direction = -1 + 2 * i % 2;
		litest_event(dev, EV_REL, code, direction);
		if (code == REL_WHEEL)
			litest_event(dev, EV_REL, REL_WHEEL_HI_RES, direction * 120);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		litest_dispatch(li);

		struct libinput_event *ev = libinput_get_event(li);
		struct libinput_event_tablet_pad *pev = litest_is_pad_dial_event(ev, 0);

		double v120 = libinput_event_tablet_pad_get_dial_delta_v120(pev);
		switch (code) {
		case REL_WHEEL: /* inverted */
			litest_assert_double_eq(v120, -120.0 * direction);
			break;
		case REL_DIAL:
			litest_assert_double_eq(v120, 120.0 * direction);
			break;
		default:
			litest_abort_msg("Invalid dial code");
		}
		libinput_event_destroy(ev);
	}
}
END_TEST

START_TEST(pad_dial_hi_res)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	const int increment = 30;
	int accumulated = 0;

	if (!libevdev_has_event_code(dev->evdev, EV_REL, REL_WHEEL_HI_RES))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	for (int i = 0; i < 10; i++) {
		litest_event(dev, EV_REL, REL_WHEEL_HI_RES, increment);
		accumulated += increment;
		if (accumulated % 120 == 0)
			litest_event(dev, EV_REL, REL_WHEEL, 1);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		litest_dispatch(li);

		struct libinput_event *ev = libinput_get_event(li);
		struct libinput_event_tablet_pad *pev = litest_is_pad_dial_event(ev, 0);

		double v120 = libinput_event_tablet_pad_get_dial_delta_v120(pev);
		litest_assert_double_eq(v120, -increment); /* REL_WHEEL is inverted */
		libinput_event_destroy(ev);
	}
}
END_TEST

START_TEST(pad_has_strip)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	int nstrips;

	nstrips = libinput_device_tablet_pad_get_num_strips(device);
	litest_assert_int_ge(nstrips, 1);
}
END_TEST

START_TEST(pad_strip)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	int val;
	double pos, expected;

	litest_pad_strip_start(dev, 10);

	litest_drain_events(li);

	expected = 0;

	/* 9.5 works with the generic axis scaling without jumping over a
	 * value. */
	for (val = 0; val < 100; val += 9.5) {
		litest_pad_strip_change(dev, val);
		litest_dispatch(li);

		ev = libinput_get_event(li);
		pev = litest_is_pad_strip_event(
			ev,
			0,
			LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER);

		pos = libinput_event_tablet_pad_get_strip_position(pev);
		litest_assert_double_ge(pos, 0.0);
		litest_assert_double_lt(pos, 1.0);

		/* rounding errors, mostly caused by small physical range */
		litest_assert_double_ge(pos, expected - 0.02);
		litest_assert_double_le(pos, expected + 0.02);

		libinput_event_destroy(ev);

		expected = pos + 0.08;
	}

	litest_pad_strip_change(dev, 100);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	pev = litest_is_pad_strip_event(ev, 0, LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER);
	pos = libinput_event_tablet_pad_get_strip_position(pev);
	litest_assert_double_eq(pos, 1.0);
	libinput_event_destroy(ev);

	litest_pad_strip_end(dev);
}
END_TEST

START_TEST(pad_strip_finger_up)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	double pos;

	litest_pad_strip_start(dev, 10);
	litest_drain_events(li);

	litest_pad_strip_end(dev);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	pev = litest_is_pad_strip_event(ev, 0, LIBINPUT_TABLET_PAD_STRIP_SOURCE_FINGER);

	pos = libinput_event_tablet_pad_get_strip_position(pev);
	litest_assert_double_eq(pos, -1.0);
	libinput_event_destroy(ev);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(pad_left_handed_default)
{
#ifdef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	enum libinput_config_status status;

	litest_assert(libinput_device_config_left_handed_is_available(device));

	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);
	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 0);

	status = libinput_device_config_left_handed_set(dev->libinput_device, 1);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 1);
	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);

	status = libinput_device_config_left_handed_set(dev->libinput_device, 0);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 0);
	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);

#endif
}
END_TEST

START_TEST(pad_no_left_handed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	/* Without libwacom we default to left-handed being available */
#ifdef HAVE_LIBWACOM
	litest_assert(!libinput_device_config_left_handed_is_available(device));
#else
	litest_assert(libinput_device_config_left_handed_is_available(device));
#endif

	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);
	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 0);

#ifdef HAVE_LIBWACOM
	enum libinput_config_status status;
	status = libinput_device_config_left_handed_set(dev->libinput_device, 1);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);

	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 0);
	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);

	status = libinput_device_config_left_handed_set(dev->libinput_device, 0);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);

	litest_assert_int_eq(libinput_device_config_left_handed_get(device), 0);
	litest_assert_int_eq(libinput_device_config_left_handed_get_default(device), 0);
#endif
}
END_TEST

START_TEST(pad_left_handed_ring)
{
#ifdef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_tablet_pad *pev;
	int val;
	double degrees, expected;

	libinput_device_config_left_handed_set(dev->libinput_device, 1);

	litest_pad_ring_start(dev, 10);

	litest_drain_events(li);

	/* Wacom's 0 value is at 275 degrees -> 90 in left-handed mode*/
	expected = 90;

	for (val = 0; val < 100; val += 10) {
		litest_pad_ring_change(dev, val);
		litest_dispatch(li);

		ev = libinput_get_event(li);
		pev = litest_is_pad_ring_event(ev,
					       0,
					       LIBINPUT_TABLET_PAD_RING_SOURCE_FINGER);

		degrees = libinput_event_tablet_pad_get_ring_position(pev);
		litest_assert_double_ge(degrees, 0.0);
		litest_assert_double_lt(degrees, 360.0);

		/* rounding errors, mostly caused by small physical range */
		litest_assert_double_ge(degrees, expected - 2);
		litest_assert_double_le(degrees, expected + 2);

		libinput_event_destroy(ev);

		expected = fmod(degrees + 36, 360);
	}

	litest_pad_ring_end(dev);
#endif
}
END_TEST

static bool
pad_has_groups(struct litest_device *dev)
{
	bool rc = false;
#ifdef HAVE_LIBWACOM
	WacomDeviceDatabase *db = NULL;
	WacomDevice *wacom = NULL;

	db = libwacom_database_new();
	litest_assert_notnull(db);

	wacom = libwacom_new_from_usbid(db,
					libevdev_get_id_vendor(dev->evdev),
					libevdev_get_id_product(dev->evdev),
					NULL);
	if (wacom && (libwacom_get_ring_num_modes(wacom) != 0 ||
		      libwacom_get_ring2_num_modes(wacom) != 0 ||
		      libwacom_get_strips_num_modes(wacom) != 0))
		rc = true;

	libwacom_destroy(wacom);
	libwacom_database_destroy(db);
#endif
	return rc;
}

START_TEST(pad_mode_groups)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int ngroups;
	unsigned int i;

	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);
	litest_assert_int_ge(ngroups, 1);

	for (i = 0; i < (unsigned int)ngroups; i++) {
		group = libinput_device_tablet_pad_get_mode_group(device, i);
		litest_assert_notnull(group);
		litest_assert_int_eq(libinput_tablet_pad_mode_group_get_index(group),
				     i);
	}

	group = libinput_device_tablet_pad_get_mode_group(device, ngroups);
	litest_assert(group == NULL);
	group = libinput_device_tablet_pad_get_mode_group(device, ngroups + 1);
	litest_assert(group == NULL);
}
END_TEST

START_TEST(pad_mode_groups_userdata)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int rc;
	void *userdata = &rc;

	group = libinput_device_tablet_pad_get_mode_group(device, 0);
	litest_assert(libinput_tablet_pad_mode_group_get_user_data(group) == NULL);
	libinput_tablet_pad_mode_group_set_user_data(group, userdata);
	litest_assert(libinput_tablet_pad_mode_group_get_user_data(group) == &rc);

	libinput_tablet_pad_mode_group_set_user_data(group, NULL);
	litest_assert(libinput_tablet_pad_mode_group_get_user_data(group) == NULL);
}
END_TEST

START_TEST(pad_mode_groups_ref)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group, *g;

	group = libinput_device_tablet_pad_get_mode_group(device, 0);
	g = libinput_tablet_pad_mode_group_ref(group);
	litest_assert_ptr_eq(g, group);

	/* We don't expect this to be freed. Any leaks should be caught by
	 * valgrind. */
	g = libinput_tablet_pad_mode_group_unref(group);
	litest_assert_ptr_eq(g, group);
}
END_TEST

START_TEST(pad_mode_group_mode)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int ngroups;
	unsigned int nmodes, mode;

	if (pad_has_groups(dev))
		return LITEST_NOT_APPLICABLE;

	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);
	litest_assert_int_ge(ngroups, 1);

	group = libinput_device_tablet_pad_get_mode_group(device, 0);

	nmodes = libinput_tablet_pad_mode_group_get_num_modes(group);
	litest_assert_int_eq(nmodes, 1U);

	mode = libinput_tablet_pad_mode_group_get_mode(group);
	litest_assert_int_lt(mode, nmodes);
}
END_TEST

START_TEST(pad_mode_group_has)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int ngroups, nbuttons, nrings, nstrips;
	int i, b, r, s;

	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);
	litest_assert_int_ge(ngroups, 1);

	nbuttons = libinput_device_tablet_pad_get_num_buttons(device);
	nrings = libinput_device_tablet_pad_get_num_rings(device);
	nstrips = libinput_device_tablet_pad_get_num_strips(device);

	for (b = 0; b < nbuttons; b++) {
		bool found = false;
		for (i = 0; i < ngroups; i++) {
			group = libinput_device_tablet_pad_get_mode_group(device, i);
			if (libinput_tablet_pad_mode_group_has_button(group, b)) {
				litest_assert(!found);
				found = true;
			}
		}
		litest_assert(found);
	}

	for (s = 0; s < nstrips; s++) {
		bool found = false;
		for (i = 0; i < ngroups; i++) {
			group = libinput_device_tablet_pad_get_mode_group(device, i);
			if (libinput_tablet_pad_mode_group_has_strip(group, s)) {
				litest_assert(!found);
				found = true;
			}
		}
		litest_assert(found);
	}

	for (r = 0; r < nrings; r++) {
		bool found = false;
		for (i = 0; i < ngroups; i++) {
			group = libinput_device_tablet_pad_get_mode_group(device, i);
			if (libinput_tablet_pad_mode_group_has_ring(group, r)) {
				litest_assert(!found);
				found = true;
			}
		}
		litest_assert(found);
	}
}
END_TEST

START_TEST(pad_mode_group_has_invalid)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int ngroups, nbuttons, nrings, nstrips;
	int i;
	int rc;

	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);
	litest_assert_int_ge(ngroups, 1);

	nbuttons = libinput_device_tablet_pad_get_num_buttons(device);
	nrings = libinput_device_tablet_pad_get_num_rings(device);
	nstrips = libinput_device_tablet_pad_get_num_strips(device);

	for (i = 0; i < ngroups; i++) {
		group = libinput_device_tablet_pad_get_mode_group(device, i);
		rc = libinput_tablet_pad_mode_group_has_button(group, nbuttons);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_button(group, nbuttons + 1);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_button(group, 0x1000000);
		litest_assert_int_eq(rc, 0);
	}

	for (i = 0; i < ngroups; i++) {
		group = libinput_device_tablet_pad_get_mode_group(device, i);
		rc = libinput_tablet_pad_mode_group_has_strip(group, nstrips);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_strip(group, nstrips + 1);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_strip(group, 0x1000000);
		litest_assert_int_eq(rc, 0);
	}

	for (i = 0; i < ngroups; i++) {
		group = libinput_device_tablet_pad_get_mode_group(device, i);
		rc = libinput_tablet_pad_mode_group_has_ring(group, nrings);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_ring(group, nrings + 1);
		litest_assert_int_eq(rc, 0);
		rc = libinput_tablet_pad_mode_group_has_ring(group, 0x1000000);
		litest_assert_int_eq(rc, 0);
	}
}
END_TEST

START_TEST(pad_mode_group_has_no_toggle)
{
#ifdef HAVE_LIBWACOM
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libinput_tablet_pad_mode_group *group;
	int ngroups, nbuttons;
	int i, b;

	if (pad_has_groups(dev))
		return LITEST_NOT_APPLICABLE;

	ngroups = libinput_device_tablet_pad_get_num_mode_groups(device);
	litest_assert_int_eq(ngroups, 1);

	/* Button must not be toggle buttons */
	nbuttons = libinput_device_tablet_pad_get_num_buttons(device);
	for (i = 0; i < ngroups; i++) {
		group = libinput_device_tablet_pad_get_mode_group(device, i);
		for (b = 0; b < nbuttons; b++) {
			litest_assert(
				!libinput_tablet_pad_mode_group_button_is_toggle(group,
										 b));
		}
	}
#endif
}
END_TEST

static bool
pad_has_keys(struct litest_device *dev)
{
	struct libevdev *evdev = dev->evdev;

	return (libevdev_has_event_code(evdev, EV_KEY, KEY_BUTTONCONFIG) ||
		libevdev_has_event_code(evdev, EV_KEY, KEY_ONSCREEN_KEYBOARD) ||
		libevdev_has_event_code(evdev, EV_KEY, KEY_CONTROLPANEL));
}

static void
pad_key_down(struct litest_device *dev, unsigned int which)
{
	litest_event(dev, EV_ABS, ABS_MISC, 15);
	litest_event(dev, EV_KEY, which, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
}

static void
pad_key_up(struct litest_device *dev, unsigned int which)
{
	litest_event(dev, EV_ABS, ABS_MISC, 0);
	litest_event(dev, EV_KEY, which, 0);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
}

START_TEST(pad_keys)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	unsigned int key;

	if (!pad_has_keys(dev))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	key = KEY_BUTTONCONFIG;
	pad_key_down(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_PRESSED);

	pad_key_up(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_RELEASED);

	key = KEY_ONSCREEN_KEYBOARD;
	pad_key_down(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_PRESSED);

	pad_key_up(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_RELEASED);

	key = KEY_CONTROLPANEL;
	pad_key_down(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_PRESSED);

	pad_key_up(dev, key);
	litest_dispatch(li);
	litest_assert_pad_key_event(li, key, LIBINPUT_KEY_STATE_RELEASED);
}
END_TEST

START_TEST(pad_send_events_disabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	enum libinput_config_status status;

	litest_drain_events(li);

	status = libinput_device_config_send_events_set_mode(
		dev->libinput_device,
		LIBINPUT_CONFIG_SEND_EVENTS_DISABLED);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);

	litest_pad_strip_start(dev, 10);
	litest_assert_empty_queue(li);
	litest_pad_strip_change(dev, 100);
	litest_assert_empty_queue(li);
	litest_pad_strip_end(dev);
	litest_assert_empty_queue(li);

	litest_pad_ring_start(dev, 10);
	litest_assert_empty_queue(li);
	litest_pad_ring_change(dev, 100);
	litest_assert_empty_queue(li);
	litest_pad_ring_end(dev);
	litest_assert_empty_queue(li);

	pad_key_down(dev, KEY_BUTTONCONFIG);
	litest_assert_empty_queue(li);
	pad_key_up(dev, KEY_BUTTONCONFIG);
	litest_assert_empty_queue(li);
}
END_TEST

TEST_COLLECTION(pad)
{
	/* clang-format off */
	litest_add(pad_cap, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_no_cap, LITEST_ANY, LITEST_TABLET_PAD);

	litest_add(pad_time, LITEST_TABLET_PAD, LITEST_ANY);

	litest_add(pad_num_buttons, LITEST_TABLET_PAD, LITEST_ANY);
	/* None of our dial devices have libwacom entries */
	litest_add(pad_num_buttons_libwacom, LITEST_TABLET_PAD, LITEST_DIAL);
	litest_add(pad_button_intuos, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_button_bamboo, LITEST_TABLET_PAD, LITEST_ANY);
	/* None of our dial devices have libwacom entries */
	litest_add(pad_button_libwacom, LITEST_TABLET_PAD, LITEST_DIAL);
	litest_add(pad_button_mode_groups, LITEST_TABLET_PAD, LITEST_ANY);

	litest_add(pad_has_ring, LITEST_RING, LITEST_ANY);
	litest_add(pad_ring, LITEST_RING, LITEST_ANY);
	litest_add(pad_ring_finger_up, LITEST_RING, LITEST_ANY);

	litest_add(pad_has_strip, LITEST_STRIP, LITEST_ANY);
	litest_add(pad_strip, LITEST_STRIP, LITEST_ANY);
	litest_add(pad_strip_finger_up, LITEST_STRIP, LITEST_ANY);

	litest_add(pad_has_dial, LITEST_DIAL, LITEST_ANY);
	litest_add(pad_dial_low_res, LITEST_DIAL, LITEST_ANY);
	litest_add(pad_dial_hi_res, LITEST_DIAL, LITEST_ANY);

	litest_add_for_device(pad_left_handed_default, LITEST_WACOM_INTUOS5_PAD);
	litest_add_for_device(pad_no_left_handed, LITEST_WACOM_INTUOS3_PAD);
	litest_add_for_device(pad_left_handed_ring, LITEST_WACOM_INTUOS5_PAD);
	/* None of the current strip tablets are left-handed */

	litest_add(pad_mode_groups, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_groups_userdata, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_groups_ref, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_group_mode, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_group_has, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_group_has_invalid, LITEST_TABLET_PAD, LITEST_ANY);
	litest_add(pad_mode_group_has_no_toggle, LITEST_TABLET_PAD, LITEST_ANY);

	litest_add(pad_keys, LITEST_TABLET_PAD, LITEST_ANY);

	litest_add(pad_send_events_disabled, LITEST_TABLET_PAD, LITEST_ANY);
	/* clang-format on */
}
