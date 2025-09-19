/*
 * Copyright © 2013 Red Hat, Inc.
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
#include <libevdev/libevdev.h>
#include <libinput.h>
#include <unistd.h>

#include "libinput-util.h"
#include "litest.h"

START_TEST(touch_frame_events)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	int have_frame_event = 0;

	litest_drain_events(dev->libinput);

	litest_touch_down(dev, 0, 10, 10);
	litest_dispatch(li);

	while ((event = libinput_get_event(li))) {
		if (libinput_event_get_type(event) == LIBINPUT_EVENT_TOUCH_FRAME)
			have_frame_event++;
		libinput_event_destroy(event);
	}
	litest_assert_int_eq(have_frame_event, 1);

	litest_touch_down(dev, 1, 10, 10);
	litest_dispatch(li);

	while ((event = libinput_get_event(li))) {
		if (libinput_event_get_type(event) == LIBINPUT_EVENT_TOUCH_FRAME)
			have_frame_event++;
		libinput_event_destroy(event);
	}
	litest_assert_int_eq(have_frame_event, 2);
}
END_TEST

START_TEST(touch_downup_no_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 10, 10);
	litest_dispatch(li);

	litest_assert_touch_down_frame(li);

	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_assert_touch_up_frame(li);
}
END_TEST

START_TEST(touch_abs_transform)
{
	struct litest_device *dev;
	struct libinput *libinput;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	double fx, fy;
	bool tested = false;

	struct input_absinfo abs[] = {
		{ ABS_X, 0, 32767, 75, 0, 10 },
		{ ABS_Y, 0, 32767, 129, 0, 9 },
		{ ABS_MT_POSITION_X, 0, 32767, 0, 0, 10 },
		{ ABS_MT_POSITION_Y, 0, 32767, 0, 0, 9 },
		{ .value = -1 },
	};

	dev = litest_create_device_with_overrides(LITEST_WACOM_ISDV4_E6_FINGER,
						  "litest Highres touch device",
						  NULL,
						  abs,
						  NULL);

	libinput = dev->libinput;

	litest_touch_down(dev, 0, 100, 100);

	litest_dispatch(libinput);

	while ((ev = libinput_get_event(libinput))) {
		if (libinput_event_get_type(ev) != LIBINPUT_EVENT_TOUCH_DOWN) {
			libinput_event_destroy(ev);
			continue;
		}

		tev = libinput_event_get_touch_event(ev);
		fx = libinput_event_touch_get_x_transformed(tev, 1920);
		litest_assert_double_eq_epsilon(fx, 1920.0, 0.1);
		litest_assert_double_lt(fx, 1920.0);
		fy = libinput_event_touch_get_y_transformed(tev, 720);
		litest_assert_double_eq_epsilon(fy, 720.0, 0.1);
		litest_assert_double_lt(fy, 720.0);

		tested = true;

		libinput_event_destroy(ev);
	}

	litest_assert(tested);

	litest_device_destroy(dev);
}
END_TEST

static inline void
touch_assert_seat_slot(struct libinput *li,
		       enum libinput_event_type type,
		       unsigned int slot,
		       unsigned int seat_slot)
{
	struct libinput_event *ev;
	struct libinput_event_touch *tev;

	litest_dispatch(li);
	ev = libinput_get_event(li);
	tev = litest_is_touch_event(ev, type);
	slot = libinput_event_touch_get_slot(tev);
	litest_assert_int_eq(slot, slot);
	slot = libinput_event_touch_get_seat_slot(tev);
	litest_assert_int_eq(slot, seat_slot);
	libinput_event_destroy(ev);

	ev = libinput_get_event(li);
	litest_assert_event_type(ev, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(ev);
}

START_TEST(touch_seat_slot)
{
	struct litest_device *dev1 = litest_current_device();
	struct litest_device *dev2;
	struct libinput *li = dev1->libinput;

	dev2 = litest_add_device(li, LITEST_WACOM_ISDV4_E6_FINGER);

	litest_drain_events(li);

	litest_touch_down(dev1, 0, 50, 50);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_DOWN, 0, 0);

	litest_touch_down(dev2, 0, 50, 50);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_DOWN, 0, 1);

	litest_touch_down(dev2, 1, 60, 50);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_DOWN, 1, 2);

	litest_touch_down(dev1, 1, 60, 50);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_DOWN, 1, 3);

	litest_touch_move_to(dev1, 0, 50, 50, 60, 70, 10);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_MOTION, 0, 0);
	litest_drain_events(li);

	litest_touch_move_to(dev2, 1, 50, 50, 60, 70, 10);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_MOTION, 1, 2);
	litest_drain_events(li);

	litest_touch_up(dev1, 0);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_UP, 0, 0);

	litest_touch_up(dev2, 0);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_UP, 0, 1);

	litest_touch_up(dev2, 1);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_UP, 1, 2);

	litest_touch_up(dev1, 1);
	touch_assert_seat_slot(li, LIBINPUT_EVENT_TOUCH_UP, 1, 3);

	litest_device_destroy(dev2);
}
END_TEST

START_TEST(touch_many_slots)
{
	struct libinput *libinput;
	struct litest_device *dev;
	struct libinput_event *ev;
	int slot;
	const int num_tps = 100;
	int slot_count = 0;
	enum libinput_event_type type;

	struct input_absinfo abs[] = {
		{ ABS_MT_SLOT, 0, num_tps - 1, 0, 0, 0 },
		{ .value = -1 },
	};

	dev = litest_create_device_with_overrides(LITEST_WACOM_ISDV4_E6_FINGER,
						  "litest Multi-touch device",
						  NULL,
						  abs,
						  NULL);
	libinput = dev->libinput;

	for (slot = 0; slot < num_tps; ++slot)
		litest_touch_down(dev, slot, 0, 0);
	for (slot = 0; slot < num_tps; ++slot)
		litest_touch_up(dev, slot);

	litest_dispatch(libinput);
	while ((ev = libinput_get_event(libinput))) {
		type = libinput_event_get_type(ev);

		if (type == LIBINPUT_EVENT_TOUCH_DOWN)
			slot_count++;
		else if (type == LIBINPUT_EVENT_TOUCH_UP)
			break;

		libinput_event_destroy(ev);
		litest_dispatch(libinput);
	}

	litest_assert_notnull(ev);
	litest_assert_int_gt(slot_count, 0);

	litest_dispatch(libinput);
	do {
		type = libinput_event_get_type(ev);
		litest_assert_enum_ne(type, LIBINPUT_EVENT_TOUCH_DOWN);
		if (type == LIBINPUT_EVENT_TOUCH_UP)
			slot_count--;

		libinput_event_destroy(ev);
		litest_dispatch(libinput);
	} while ((ev = libinput_get_event(libinput)));

	litest_assert_int_eq(slot_count, 0);

	litest_device_destroy(dev);
}
END_TEST

START_TEST(touch_double_touch_down_up)
{
	struct libinput *libinput;
	struct litest_device *dev;
	struct libinput_event *ev;
	bool got_down = false;
	bool got_up = false;

	dev = litest_current_device();
	libinput = dev->libinput;

	litest_disable_log_handler(libinput);
	/* note: this test is a false negative, libevdev will filter
	 * tracking IDs re-used in the same slot. */
	litest_touch_down(dev, 0, 0, 0);
	litest_touch_down(dev, 0, 0, 0);
	litest_touch_up(dev, 0);
	litest_touch_up(dev, 0);
	litest_dispatch(libinput);
	litest_restore_log_handler(libinput);

	while ((ev = libinput_get_event(libinput))) {
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_TOUCH_DOWN:
			litest_assert(!got_down);
			got_down = true;
			break;
		case LIBINPUT_EVENT_TOUCH_UP:
			litest_assert(got_down);
			litest_assert(!got_up);
			got_up = true;
			break;
		default:
			break;
		}

		libinput_event_destroy(ev);
		litest_dispatch(libinput);
	}

	litest_assert(got_down);
	litest_assert(got_up);
}
END_TEST

START_TEST(touch_calibration_scale)
{
	struct libinput *li;
	struct litest_device *dev;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	float matrix[6] = { 1, 0, 0, 0, 1, 0 };

	float calibration;
	double x, y;
	const int width = 640, height = 480;

	dev = litest_current_device();
	li = dev->libinput;

	for (calibration = 0.1; calibration < 1; calibration += 0.1) {
		libinput_device_config_calibration_set_matrix(dev->libinput_device,
							      matrix);
		litest_drain_events(li);

		litest_touch_down(dev, 0, 100, 100);
		litest_touch_up(dev, 0);
		litest_dispatch(li);

		ev = libinput_get_event(li);
		tev = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);

		x = libinput_event_touch_get_x_transformed(tev, width);
		y = libinput_event_touch_get_y_transformed(tev, height);

		litest_assert_int_eq(round(x), round(width * matrix[0]));
		litest_assert_int_eq(round(y), round(height * matrix[4]));

		libinput_event_destroy(ev);
		litest_drain_events(li);

		matrix[0] = calibration;
		matrix[4] = 1 - calibration;
	}
}
END_TEST

START_TEST(touch_calibration_rotation)
{
	struct libinput *li;
	struct litest_device *dev;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	float matrix[6];
	int i;
	double x, y;
	int width = 1024, height = 480;

	dev = litest_current_device();
	li = dev->libinput;

	for (i = 0; i < 4; i++) {
		float angle = i * M_PI / 2;

		/* [ cos -sin  tx ]
		   [ sin  cos  ty ]
		   [  0    0   1  ] */
		matrix[0] = cos(angle);
		matrix[1] = -sin(angle);
		matrix[3] = sin(angle);
		matrix[4] = cos(angle);

		switch (i) {
		case 0: /* 0 deg */
			matrix[2] = 0;
			matrix[5] = 0;
			break;
		case 1: /* 90 deg cw */
			matrix[2] = 1;
			matrix[5] = 0;
			break;
		case 2: /* 180 deg cw */
			matrix[2] = 1;
			matrix[5] = 1;
			break;
		case 3: /* 270 deg cw */
			matrix[2] = 0;
			matrix[5] = 1;
			break;
		}

		libinput_device_config_calibration_set_matrix(dev->libinput_device,
							      matrix);
		litest_drain_events(li);

		litest_touch_down(dev, 0, 80, 20);
		litest_touch_up(dev, 0);
		litest_dispatch(li);
		ev = libinput_get_event(li);
		tev = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);

		x = libinput_event_touch_get_x_transformed(tev, width);
		y = libinput_event_touch_get_y_transformed(tev, height);

		/* rounding errors... */
		switch (i) {
		case 0: /* 0 deg */
			litest_assert_double_eq_epsilon(x, width * 0.8, 1.0);
			litest_assert_double_eq_epsilon(y, height * 0.2, 1.0);
			break;
		case 1: /* 90 deg cw */
			litest_assert_double_eq_epsilon(x, width * 0.8, 1.0);
			litest_assert_double_eq_epsilon(y, height * 0.8, 1.0);
			break;
		case 2: /* 180 deg cw */
			litest_assert_double_eq_epsilon(x, width * 0.2, 1.0);
			litest_assert_double_eq_epsilon(y, height * 0.8, 1.0);
			break;
		case 3: /* 270 deg cw */
			litest_assert_double_eq_epsilon(x, width * 0.2, 1.0);
			litest_assert_double_eq_epsilon(y, height * 0.2, 1.0);
			break;
		}

		libinput_event_destroy(ev);
		litest_drain_events(li);
	}
}
END_TEST

START_TEST(touch_calibration_translation)
{
	struct libinput *li;
	struct litest_device *dev;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	float matrix[6] = { 1, 0, 0, 0, 1, 0 };

	float translate;
	double x, y;
	const int width = 640, height = 480;

	dev = litest_current_device();
	li = dev->libinput;

	/* translating from 0 up to 1 device width/height */
	for (translate = 0.1; translate <= 1; translate += 0.1) {
		libinput_device_config_calibration_set_matrix(dev->libinput_device,
							      matrix);
		litest_drain_events(li);

		litest_touch_down(dev, 0, 100, 100);
		litest_touch_up(dev, 0);

		litest_dispatch(li);
		ev = libinput_get_event(li);
		tev = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);

		x = libinput_event_touch_get_x_transformed(tev, width);
		y = libinput_event_touch_get_y_transformed(tev, height);

		/* sigh. rounding errors */
		litest_assert_int_ge(round(x), width + round(width * matrix[2]) - 1);
		litest_assert_int_ge(round(y), height + round(height * matrix[5]) - 1);
		litest_assert_int_le(round(x), width + round(width * matrix[2]) + 1);
		litest_assert_int_le(round(y), height + round(height * matrix[5]) + 1);

		libinput_event_destroy(ev);
		litest_drain_events(li);

		matrix[2] = translate;
		matrix[5] = 1 - translate;
	}
}
END_TEST

START_TEST(touch_calibrated_screen_path)
{
	struct litest_device *dev = litest_current_device();
	float matrix[6];
	int rc;

	rc = libinput_device_config_calibration_has_matrix(dev->libinput_device);
	litest_assert_int_eq(rc, 1);

	rc = libinput_device_config_calibration_get_matrix(dev->libinput_device,
							   matrix);
	litest_assert_int_eq(rc, 1);

	litest_assert_double_eq(matrix[0], 1.2);
	litest_assert_double_eq(matrix[1], 3.4);
	litest_assert_double_eq(matrix[2], 5.6);
	litest_assert_double_eq(matrix[3], 7.8);
	litest_assert_double_eq(matrix[4], 9.10);
	litest_assert_double_eq(matrix[5], 11.12);
}
END_TEST

START_TEST(touch_calibration_config)
{
	struct litest_device *dev = litest_current_device();
	float identity[6] = { 1, 0, 0, 0, 1, 0 };
	float nonidentity[6] = { 1, 2, 3, 4, 5, 6 };
	float matrix[6];
	enum libinput_config_status status;
	int rc;

	rc = libinput_device_config_calibration_has_matrix(dev->libinput_device);
	litest_assert_int_eq(rc, 1);

	/* Twice so we have every to-fro combination */
	for (int i = 0; i < 2; i++) {
		status = libinput_device_config_calibration_set_matrix(
			dev->libinput_device,
			identity);
		litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);
		libinput_device_config_calibration_get_matrix(dev->libinput_device,
							      matrix);
		litest_assert_int_eq(memcmp(matrix, identity, sizeof(matrix)), 0);

		status = libinput_device_config_calibration_set_matrix(
			dev->libinput_device,
			nonidentity);
		litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_SUCCESS);
		libinput_device_config_calibration_get_matrix(dev->libinput_device,
							      matrix);
		litest_assert_int_eq(memcmp(matrix, nonidentity, sizeof(matrix)), 0);
	}
}
END_TEST

static int
open_restricted(const char *path, int flags, void *data)
{
	int fd;
	fd = open(path, flags);
	return fd < 0 ? -errno : fd;
}
static void
close_restricted(int fd, void *data)
{
	close(fd);
}

static const struct libinput_interface simple_interface = {
	.open_restricted = open_restricted,
	.close_restricted = close_restricted,
};

START_TEST(touch_calibrated_screen_udev)
{
	struct libinput *li;
	struct libinput_event *ev;
	struct libinput_device *device = NULL;
	struct udev *udev;
	float matrix[6];
	int rc;

	udev = udev_new();
	litest_assert_notnull(udev);

	li = libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	litest_dispatch(li);

	while ((ev = libinput_get_event(li))) {
		struct libinput_device *d;

		if (libinput_event_get_type(ev) != LIBINPUT_EVENT_DEVICE_ADDED) {
			libinput_event_destroy(ev);
			continue;
		}

		d = libinput_event_get_device(ev);

		if (libinput_device_get_id_vendor(d) == 0x22 &&
		    libinput_device_get_id_product(d) == 0x33) {
			device = libinput_device_ref(d);
			litest_drain_events(li);
		}
		libinput_event_destroy(ev);
	}

	litest_drain_events(li);

	litest_assert_notnull(device);
	rc = libinput_device_config_calibration_has_matrix(device);
	litest_assert_int_eq(rc, 1);

	rc = libinput_device_config_calibration_get_matrix(device, matrix);
	litest_assert_int_eq(rc, 1);

	litest_assert_double_eq(matrix[0], 1.2);
	litest_assert_double_eq(matrix[1], 3.4);
	litest_assert_double_eq(matrix[2], 5.6);
	litest_assert_double_eq(matrix[3], 7.8);
	litest_assert_double_eq(matrix[4], 9.10);
	litest_assert_double_eq(matrix[5], 11.12);

	libinput_device_unref(device);

	libinput_unref(li);
	udev_unref(udev);
}
END_TEST

START_TEST(touch_no_left_handed)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *d = dev->libinput_device;
	enum libinput_config_status status;
	int rc;

	rc = libinput_device_config_left_handed_is_available(d);
	litest_assert_int_eq(rc, 0);

	rc = libinput_device_config_left_handed_get(d);
	litest_assert_int_eq(rc, 0);

	rc = libinput_device_config_left_handed_get_default(d);
	litest_assert_int_eq(rc, 0);

	status = libinput_device_config_left_handed_set(d, 0);
	litest_assert_enum_eq(status, LIBINPUT_CONFIG_STATUS_UNSUPPORTED);
}
END_TEST

START_TEST(fake_mt_exists)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_device *device;

	litest_dispatch(li);

	event = libinput_get_event(li);
	device = libinput_event_get_device(event);

	litest_assert(
		!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH));

	/* This test may need fixing if we add other fake-mt devices that
	 * have different capabilities */
	litest_assert(
		libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER));

	libinput_event_destroy(event);
}
END_TEST

START_TEST(fake_mt_no_touch_events)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 5);
	litest_touch_up(dev, 0);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_down(dev, 1, 70, 70);
	litest_touch_move_to(dev, 0, 50, 50, 90, 40, 10);
	litest_touch_move_to(dev, 0, 70, 70, 40, 50, 10);
	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);

	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE);
}
END_TEST

START_TEST(touch_protocol_a_init)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_device *device = dev->libinput_device;

	litest_wait_for_event(li);

	litest_assert(
		libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH));
}
END_TEST

START_TEST(touch_protocol_a_touch)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	double x, y, oldx, oldy;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 5, 95);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	tev = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);

	oldx = libinput_event_touch_get_x(tev);
	oldy = libinput_event_touch_get_y(tev);

	libinput_event_destroy(ev);

	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(ev);

	litest_touch_move_to(dev, 0, 10, 90, 90, 10, 20);
	litest_dispatch(li);

	while ((ev = libinput_get_event(li))) {
		if (libinput_event_get_type(ev) == LIBINPUT_EVENT_TOUCH_FRAME) {
			libinput_event_destroy(ev);
			continue;
		}

		litest_assert_event_type(ev, LIBINPUT_EVENT_TOUCH_MOTION);

		tev = libinput_event_get_touch_event(ev);
		x = libinput_event_touch_get_x(tev);
		y = libinput_event_touch_get_y(tev);

		litest_assert_int_gt(x, oldx);
		litest_assert_int_lt(y, oldy);

		oldx = x;
		oldy = y;

		libinput_event_destroy(ev);
	}

	litest_touch_up(dev, 0);
	litest_dispatch(li);
	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_UP);
	libinput_event_destroy(ev);
}
END_TEST

START_TEST(touch_protocol_a_2fg_touch)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_touch *tev;
	int pos;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 5, 95);
	litest_touch_down(dev, 1, 95, 5);

	litest_dispatch(li);
	litest_assert_touch_down_frame(li);

	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);
	libinput_event_destroy(ev);

	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(ev);

	for (pos = 10; pos < 100; pos += 10) {
		litest_touch_move(dev, 0, pos, 100 - pos);
		litest_touch_move(dev, 1, 100 - pos, pos);
		litest_dispatch(li);

		ev = libinput_get_event(li);
		tev = libinput_event_get_touch_event(ev);
		litest_assert_int_eq(libinput_event_touch_get_slot(tev), 0);
		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_FRAME);
		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		tev = libinput_event_get_touch_event(ev);
		litest_assert_int_eq(libinput_event_touch_get_slot(tev), 1);
		libinput_event_destroy(ev);

		ev = libinput_get_event(li);
		litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_FRAME);
		libinput_event_destroy(ev);
	}

	litest_touch_up(dev, 0);
	litest_dispatch(li);
	litest_assert_touch_up_frame(li);

	litest_touch_up(dev, 1);
	litest_dispatch(li);
	litest_assert_touch_up_frame(li);
}
END_TEST

START_TEST(touch_initial_state)
{
	struct litest_device *dev;
	struct libinput *libinput1;
	struct libinput_event *ev1 = NULL;
	struct libinput_event *ev2 = NULL;
	struct libinput_event_touch *t1, *t2;
	struct libinput_device *device1, *device2;
	int axis = litest_test_param_get_i32(test_env->params, "axis");

	dev = litest_current_device();
	device1 = dev->libinput_device;
	libinput_device_config_tap_set_enabled(device1, LIBINPUT_CONFIG_TAP_DISABLED);

	libinput1 = dev->libinput;
	litest_touch_down(dev, 0, 40, 60);
	litest_touch_up(dev, 0);

	/* device is now on some x/y value */
	litest_drain_events(libinput1);

	_litest_context_destroy_ struct libinput *libinput2 = litest_create_context();
	device2 = libinput_path_add_device(libinput2,
					   libevdev_uinput_get_devnode(dev->uinput));
	libinput_device_config_tap_set_enabled(device2, LIBINPUT_CONFIG_TAP_DISABLED);
	litest_drain_events(libinput2);

	if (axis == ABS_X)
		litest_touch_down(dev, 0, 40, 70);
	else
		litest_touch_down(dev, 0, 70, 60);
	litest_touch_up(dev, 0);

	litest_dispatch(libinput1);
	litest_dispatch(libinput2);

	while (libinput_next_event_type(libinput1)) {
		ev1 = libinput_get_event(libinput1);
		ev2 = libinput_get_event(libinput2);

		t1 = litest_is_touch_event(ev1, 0);
		t2 = litest_is_touch_event(ev2, 0);

		litest_assert_int_eq(libinput_event_get_type(ev1),
				     libinput_event_get_type(ev2));

		if (libinput_event_get_type(ev1) == LIBINPUT_EVENT_TOUCH_UP ||
		    libinput_event_get_type(ev1) == LIBINPUT_EVENT_TOUCH_FRAME)
			break;

		litest_assert_double_eq(libinput_event_touch_get_x(t1),
					libinput_event_touch_get_x(t2));
		litest_assert_double_eq(libinput_event_touch_get_y(t1),
					libinput_event_touch_get_y(t2));

		libinput_event_destroy(ev1);
		libinput_event_destroy(ev2);
		ev1 = NULL;
		ev2 = NULL;
	}

	libinput_event_destroy(ev1);
	libinput_event_destroy(ev2);
}
END_TEST

START_TEST(touch_time_usec)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_touch *tev;
	uint64_t time_usec;

	litest_drain_events(dev->libinput);

	litest_touch_down(dev, 0, 10, 10);
	litest_dispatch(li);

	event = libinput_get_event(li);
	tev = litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_DOWN);
	time_usec = libinput_event_touch_get_time_usec(tev);
	litest_assert_int_eq(libinput_event_touch_get_time(tev),
			     (uint32_t)(time_usec / 1000));
	libinput_event_destroy(event);
}
END_TEST

START_TEST(touch_fuzz)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	int i;
	int x = 700, y = 300;

	litest_drain_events(dev->libinput);

	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 30);
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_dispatch(li);

	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_DOWN);
	libinput_event_destroy(event);
	event = libinput_get_event(li);
	litest_is_touch_event(event, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(event);

	litest_drain_events(li);

	for (i = 0; i < 50; i++) {
		if (i % 2) {
			x++;
			y--;
		} else {
			x--;
			y++;
		}
		litest_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
		litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		litest_dispatch(li);
		litest_assert_empty_queue(li);
	}
}
END_TEST

START_TEST(touch_fuzz_property)
{
	struct litest_device *dev = litest_current_device();
	struct udev_device *d;
	const char *prop;
	int fuzz = 0;

	litest_assert_int_eq(libevdev_get_abs_fuzz(dev->evdev, ABS_X), 0);
	litest_assert_int_eq(libevdev_get_abs_fuzz(dev->evdev, ABS_Y), 0);

	d = libinput_device_get_udev_device(dev->libinput_device);
	prop = udev_device_get_property_value(d, "LIBINPUT_FUZZ_00");
	litest_assert_notnull(prop);
	litest_assert(safe_atoi(prop, &fuzz));
	litest_assert_int_eq(fuzz, 10); /* device-specific */

	prop = udev_device_get_property_value(d, "LIBINPUT_FUZZ_01");
	litest_assert_notnull(prop);
	litest_assert(safe_atoi(prop, &fuzz));
	litest_assert_int_eq(fuzz, 12); /* device-specific */

	udev_device_unref(d);
}
END_TEST

START_TEST(touch_release_on_unplug)
{
	struct litest_device *dev;
	struct libinput_event *ev;

	_litest_context_destroy_ struct libinput *li = litest_create_context();
	dev = litest_add_device(li, LITEST_GENERIC_MULTITOUCH_SCREEN);
	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_drain_events(li);

	/* Touch is still down when device is removed, expect a release */
	litest_device_destroy(dev);
	litest_dispatch(li);

	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_CANCEL);
	libinput_event_destroy(ev);

	ev = libinput_get_event(li);
	litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_FRAME);
	libinput_event_destroy(ev);

	ev = libinput_get_event(li);
	litest_assert_event_type(ev, LIBINPUT_EVENT_DEVICE_REMOVED);
	libinput_event_destroy(ev);
}
END_TEST

START_TEST(touch_invalid_range_over)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_touch *t;
	double x, y;

	litest_drain_events(li);

	/* Touch outside the valid area */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	litest_event(dev, EV_ABS, ABS_X, 4000);
	litest_event(dev, EV_ABS, ABS_Y, 5000);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, 4000);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 5000);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_dispatch(li);

	/* Expect the mm to be correct regardless */
	ev = libinput_get_event(li);
	t = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);
	x = libinput_event_touch_get_x(t);
	y = libinput_event_touch_get_y(t);
	litest_assert_double_eq(x, 300); /* device has resolution 10 */
	litest_assert_double_eq(y, 300); /* device has resolution 10 */

	/* Expect the percentage to be correct too, even if > 100% */
	x = libinput_event_touch_get_x_transformed(t, 100);
	y = libinput_event_touch_get_y_transformed(t, 100);
	litest_assert_double_eq(round(x), 200);
	litest_assert_double_eq(round(y), 120);

	libinput_event_destroy(ev);
}
END_TEST

START_TEST(touch_invalid_range_under)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *ev;
	struct libinput_event_touch *t;
	double x, y;

	litest_drain_events(li);

	/* Touch outside the valid area */
	litest_event(dev, EV_ABS, ABS_MT_SLOT, 0);
	litest_event(dev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	litest_event(dev, EV_ABS, ABS_X, -500);
	litest_event(dev, EV_ABS, ABS_Y, 1000);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_X, -500);
	litest_event(dev, EV_ABS, ABS_MT_POSITION_Y, 1000);
	litest_event(dev, EV_KEY, BTN_TOUCH, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_dispatch(li);

	/* Expect the mm to be correct regardless */
	ev = libinput_get_event(li);
	t = litest_is_touch_event(ev, LIBINPUT_EVENT_TOUCH_DOWN);
	x = libinput_event_touch_get_x(t);
	y = libinput_event_touch_get_y(t);
	litest_assert_double_eq(x, -150); /* device has resolution 10 */
	litest_assert_double_eq(y, -100); /* device has resolution 10 */

	/* Expect the percentage to be correct too, even if > 100% */
	x = libinput_event_touch_get_x_transformed(t, 100);
	y = libinput_event_touch_get_y_transformed(t, 100);
	litest_assert_double_eq(round(x), -100);
	litest_assert_double_eq(round(y), -40);

	libinput_event_destroy(ev);
}
END_TEST

START_TEST(touch_count_st)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_touch_get_touch_count(device), 1);
}
END_TEST

START_TEST(touch_count_mt)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;
	struct libevdev *evdev = dev->evdev;

	litest_assert_int_eq(libinput_device_touch_get_touch_count(device),
			     libevdev_get_num_slots(evdev));
}
END_TEST

START_TEST(touch_count_mtdev)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	/* We hardcode this to 10 */
	litest_assert_int_eq(libinput_device_touch_get_touch_count(device), 10);
}
END_TEST

START_TEST(touch_count_invalid)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_touch_get_touch_count(device), -1);
}
END_TEST

static inline bool
touch_has_tool_palm(struct litest_device *dev)
{
	return libevdev_has_event_code(dev->evdev, EV_ABS, ABS_MT_TOOL_TYPE);
}

START_TEST(touch_palm_detect_tool_palm)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_drain_events(li);

	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_dispatch(li);
	litest_assert_touch_cancel(li);

	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touch_palm_detect_tool_palm_on_off)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_drain_events(li);

	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_dispatch(li);
	litest_assert_touch_cancel(li);

	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_assert_empty_queue(li);

	litest_axis_set_value(axes, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
	litest_touch_move_to_extended(dev, 0, 50, 40, 70, 70, axes, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touch_palm_detect_tool_palm_keep_type)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_touch_up(dev, 0);
	litest_drain_events(li);

	/* ABS_MT_TOOL_TYPE never reset to finger, so a new touch
	   should be ignored outright */
	litest_touch_down_extended(dev, 0, 50, 50, axes);

	/* Test the revert to finger case too */
	litest_axis_set_value(axes, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touch_palm_detect_tool_palm_2fg)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_down(dev, 1, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_drain_events(li);

	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_dispatch(li);
	litest_assert_touch_cancel(li);

	litest_touch_move_to(dev, 1, 50, 50, 70, 70, 10);
	litest_dispatch(li);
	litest_assert_touch_motion_frame(li);

	litest_touch_up(dev, 1);
	litest_dispatch(li);
	litest_assert_touch_up_frame(li);

	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touch_palm_detect_tool_palm_on_off_2fg)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_down(dev, 1, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_drain_events(li);

	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_dispatch(li);
	litest_assert_touch_cancel(li);

	litest_touch_move_to(dev, 1, 50, 50, 70, 70, 10);
	litest_dispatch(li);
	litest_assert_touch_motion_frame(li);

	litest_axis_set_value(axes, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
	litest_touch_move_to_extended(dev, 0, 50, 40, 70, 70, axes, 10);
	litest_assert_empty_queue(li);

	litest_touch_move_to(dev, 1, 70, 70, 50, 40, 10);
	litest_dispatch(li);
	litest_assert_touch_motion_frame(li);

	litest_touch_up(dev, 1);
	litest_dispatch(li);
	litest_assert_touch_up_frame(li);

	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(touch_palm_detect_tool_palm_keep_type_2fg)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct axis_replacement axes[] = {
		{ ABS_MT_TOOL_TYPE, MT_TOOL_PALM },
		{ -1, 0 },
	};

	if (!touch_has_tool_palm(dev))
		return LITEST_NOT_APPLICABLE;

	litest_touch_down(dev, 0, 50, 50);
	litest_touch_down(dev, 1, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 70, 70, 10);
	litest_touch_move_to_extended(dev, 0, 50, 50, 70, 70, axes, 10);
	litest_touch_up(dev, 0);
	litest_drain_events(li);

	litest_touch_move_to(dev, 1, 50, 50, 70, 70, 10);
	litest_dispatch(li);
	litest_assert_touch_motion_frame(li);

	/* ABS_MT_TOOL_TYPE never reset to finger, so a new touch
	   should be ignored outright */
	litest_touch_down_extended(dev, 0, 50, 50, axes);

	/* Test the revert to finger case too */
	litest_axis_set_value(axes, ABS_MT_TOOL_TYPE, MT_TOOL_FINGER);
	litest_touch_move_to(dev, 0, 70, 70, 50, 40, 10);
	litest_touch_up(dev, 0);

	litest_assert_empty_queue(li);

	litest_touch_up(dev, 1);
	litest_dispatch(li);
	litest_assert_touch_up_frame(li);
}
END_TEST

TEST_COLLECTION(touch)
{
	/* clang-format off */
	litest_add(touch_frame_events, LITEST_TOUCH, LITEST_ANY);
	litest_add(touch_downup_no_motion, LITEST_TOUCH, LITEST_ANY);
	litest_add(touch_downup_no_motion, LITEST_SINGLE_TOUCH, LITEST_TOUCHPAD);
	litest_add_no_device(touch_abs_transform);
	litest_add(touch_seat_slot, LITEST_TOUCH, LITEST_TOUCHPAD);
	litest_add_no_device(touch_many_slots);
	litest_add(touch_double_touch_down_up, LITEST_TOUCH, LITEST_PROTOCOL_A);
	litest_add(touch_calibration_scale, LITEST_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_calibration_scale, LITEST_SINGLE_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_calibration_rotation, LITEST_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_calibration_rotation, LITEST_SINGLE_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_calibration_translation, LITEST_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_calibration_translation, LITEST_SINGLE_TOUCH, LITEST_TOUCHPAD);
	litest_add_for_device(touch_calibrated_screen_path, LITEST_CALIBRATED_TOUCHSCREEN);
	litest_add_for_device(touch_calibrated_screen_udev, LITEST_CALIBRATED_TOUCHSCREEN);
	litest_add(touch_calibration_config, LITEST_TOUCH, LITEST_ANY);

	litest_add(touch_no_left_handed, LITEST_TOUCH, LITEST_ANY);

	litest_add(fake_mt_exists, LITEST_FAKE_MT, LITEST_ANY);
	litest_add(fake_mt_no_touch_events, LITEST_FAKE_MT, LITEST_ANY);

#ifdef HAVE_MTDEV
	litest_add(touch_protocol_a_init, LITEST_PROTOCOL_A, LITEST_ANY);
	litest_add(touch_protocol_a_touch, LITEST_PROTOCOL_A, LITEST_ANY);
	litest_add(touch_protocol_a_2fg_touch, LITEST_PROTOCOL_A, LITEST_ANY);
#endif

	litest_with_parameters(params, "axis", 'I', 2, litest_named_i32(ABS_X), litest_named_i32(ABS_Y)) {
		litest_add_parametrized(touch_initial_state, LITEST_TOUCH, LITEST_PROTOCOL_A, params);
	}

	litest_add(touch_time_usec, LITEST_TOUCH, LITEST_TOUCHPAD);

	litest_add_for_device(touch_fuzz, LITEST_MULTITOUCH_FUZZ_SCREEN);
	litest_add_for_device(touch_fuzz_property, LITEST_MULTITOUCH_FUZZ_SCREEN);

	litest_add_no_device(touch_release_on_unplug);

	litest_add_for_device(touch_invalid_range_over, LITEST_TOUCHSCREEN_INVALID_RANGE);
	litest_add_for_device(touch_invalid_range_under, LITEST_TOUCHSCREEN_INVALID_RANGE);

	litest_add(touch_count_st, LITEST_SINGLE_TOUCH, LITEST_TOUCHPAD);
	litest_add(touch_count_mt, LITEST_TOUCH, LITEST_SINGLE_TOUCH|LITEST_PROTOCOL_A);
#ifdef HAVE_MTDEV
	litest_add(touch_count_mtdev, LITEST_PROTOCOL_A, LITEST_ANY);
	litest_add(touch_count_invalid, LITEST_ANY, LITEST_TOUCH|LITEST_SINGLE_TOUCH|LITEST_PROTOCOL_A);
#endif

	litest_add(touch_palm_detect_tool_palm, LITEST_TOUCH, LITEST_ANY);
	litest_add(touch_palm_detect_tool_palm_on_off, LITEST_TOUCH, LITEST_ANY);
	litest_add(touch_palm_detect_tool_palm_keep_type, LITEST_TOUCH, LITEST_ANY);
	litest_add(touch_palm_detect_tool_palm_2fg, LITEST_TOUCH, LITEST_SINGLE_TOUCH);
	litest_add(touch_palm_detect_tool_palm_on_off_2fg, LITEST_TOUCH, LITEST_SINGLE_TOUCH);
	litest_add(touch_palm_detect_tool_palm_keep_type_2fg, LITEST_TOUCH, LITEST_ANY);
	/* clang-format on */
}
