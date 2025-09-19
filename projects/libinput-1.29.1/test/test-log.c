/*
 * Copyright © 2014 Red Hat, Inc.
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
#include <stdarg.h>
#include <unistd.h>

#include "litest.h"

static int log_handler_called;
static struct libinput *log_handler_context;

static void
simple_log_handler(struct libinput *libinput,
		   enum libinput_log_priority priority,
		   const char *format,
		   va_list args)
{
	log_handler_called++;
	if (log_handler_context)
		litest_assert_ptr_eq(libinput, log_handler_context);
	litest_assert_notnull(format);
}

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

START_TEST(log_default_priority)
{
	enum libinput_log_priority pri;

	_unref_(libinput) *li = libinput_path_create_context(&simple_interface, NULL);
	pri = libinput_log_get_priority(li);

	litest_assert_enum_eq(pri, LIBINPUT_LOG_PRIORITY_ERROR);
}
END_TEST

START_TEST(log_handler_invoked)
{
	log_handler_context = NULL;
	log_handler_called = 0;

	_litest_context_destroy_ struct libinput *li = litest_create_context();

	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);
	libinput_log_set_handler(li, simple_log_handler);
	log_handler_context = li;

	libinput_path_add_device(li, "/tmp");

	litest_assert_int_gt(log_handler_called, 0);

	log_handler_context = NULL;
	log_handler_called = 0;
}
END_TEST

START_TEST(log_handler_NULL)
{
	log_handler_called = 0;

	_litest_context_destroy_ struct libinput *li = litest_create_context();
	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_DEBUG);
	libinput_log_set_handler(li, NULL);

	libinput_path_add_device(li, "/tmp");

	litest_assert_int_eq(log_handler_called, 0);

	log_handler_called = 0;
}
END_TEST

START_TEST(log_priority)
{
	log_handler_context = NULL;
	log_handler_called = 0;

	_litest_context_destroy_ struct libinput *li = litest_create_context();
	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_ERROR);
	libinput_log_set_handler(li, simple_log_handler);
	log_handler_context = li;

	libinput_path_add_device(li, "/tmp");

	litest_assert_int_eq(log_handler_called, 1);

	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_INFO);
	/* event0 exists on any box we care to run the test suite on and we
	 * currently prints *something* for each device */
	libinput_path_add_device(li, "/dev/input/event0");
	litest_assert_int_gt(log_handler_called, 1);

	log_handler_context = NULL;
	log_handler_called = 0;
}
END_TEST

static int axisrange_log_handler_called = 0;

static void
axisrange_warning_log_handler(struct libinput *libinput,
			      enum libinput_log_priority priority,
			      const char *format,
			      va_list args)
{
	axisrange_log_handler_called++;
	litest_assert_notnull(format);

	litest_assert_str_in("is outside expected range", format);
}

START_TEST(log_axisrange_warning)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	const struct input_absinfo *abs;
	int axis = litest_test_param_get_i32(test_env->params, "axis");

	litest_touch_down(dev, 0, 90, 100);
	litest_drain_events(li);

	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_INFO);
	libinput_log_set_handler(li, axisrange_warning_log_handler);

	abs = libevdev_get_abs_info(dev->evdev, axis);

	for (int i = 0; i < 100; i++) {
		litest_event(dev,
			     EV_ABS,
			     ABS_MT_POSITION_X + axis,
			     abs->maximum * 2 + i);
		litest_event(dev, EV_ABS, axis, abs->maximum * 2);
		litest_event(dev, EV_SYN, SYN_REPORT, 0);
		litest_dispatch(li);
	}

	/* Expect only one message per 5 min */
	litest_assert_int_eq(axisrange_log_handler_called, 1);

	libinput_log_set_priority(li, LIBINPUT_LOG_PRIORITY_ERROR);
	litest_restore_log_handler(li);
	axisrange_log_handler_called = 0;
}
END_TEST

TEST_COLLECTION(log)
{
	/* clang-format off */
	litest_add_deviceless(log_default_priority);
	litest_add_deviceless(log_handler_invoked);
	litest_add_deviceless(log_handler_NULL);
	litest_add_no_device(log_priority);

	litest_with_parameters(params, "axis", 'I', 2, litest_named_i32(ABS_X), litest_named_i32(ABS_Y)) {
		/* mtdev clips to axis ranges */
		litest_add_parametrized(log_axisrange_warning, LITEST_TOUCH, LITEST_PROTOCOL_A, params);
		litest_add_parametrized(log_axisrange_warning, LITEST_TOUCHPAD, LITEST_ANY, params);
	}
	/* clang-format on */
}
