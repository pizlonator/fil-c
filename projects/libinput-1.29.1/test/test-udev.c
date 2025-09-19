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
#include <libinput-util.h>
#include <libinput.h>
#include <libudev.h>
#include <unistd.h>

#include "litest.h"

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

START_TEST(udev_create_NULL)
{
	_unref_(udev) *udev = udev_new();

	_unref_(libinput) *li = libinput_udev_create_context(NULL, NULL, NULL);
	litest_assert(li == NULL);

	li = libinput_udev_create_context(&simple_interface, NULL, NULL);
	litest_assert(li == NULL);

	li = libinput_udev_create_context(NULL, NULL, udev);
	litest_assert(li == NULL);

	li = libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, NULL), -1);
}
END_TEST

START_TEST(udev_create_seat0)
{
	struct libinput_event *event;
	int fd;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	fd = libinput_get_fd(li);
	litest_assert_int_ge(fd, 0);

	/* expect at least one event */
	litest_dispatch(li);
	event = libinput_get_event(li);
	litest_assert_notnull(event);

	libinput_event_destroy(event);
}
END_TEST

START_TEST(udev_create_empty_seat)
{
	struct libinput_event *event;
	int fd;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	/* expect a libinput reference, but no events */
	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seatdoesntexist"), 0);

	fd = libinput_get_fd(li);
	litest_assert_int_ge(fd, 0);

	litest_dispatch(li);
	event = libinput_get_event(li);
	litest_assert(event == NULL);

	libinput_event_destroy(event);
}
END_TEST

START_TEST(udev_create_seat_too_long)
{
	char seatname[258];

	memset(seatname, 'a', sizeof(seatname) - 1);
	seatname[sizeof(seatname) - 1] = '\0';

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_set_log_handler_bug(li);

	litest_assert_int_eq(libinput_udev_assign_seat(li, seatname), -1);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(udev_set_user_data)
{
	int data1, data2;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, &data1, udev);
	litest_assert_notnull(li);
	litest_assert(libinput_get_user_data(li) == &data1);
	libinput_set_user_data(li, &data2);
	litest_assert(libinput_get_user_data(li) == &data2);
}
END_TEST

START_TEST(udev_added_seat_default)
{
	struct libinput_event *event;
	struct libinput_device *device;
	struct libinput_seat *seat;
	const char *seat_name;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);
	litest_dispatch(li);

	/* Drop any events from other devices */
	litest_drain_events(li);

	/* Now create our own device, it should be in the "default"
	 * logical seat. This test may fail if there is a local rule changing
	 * that, but it'll be fine for the 99% case. */
	_unused_ _destroy_(litest_device) *dev =
		litest_create(LITEST_MOUSE, NULL, NULL, NULL, NULL);
	litest_wait_for_event_of_type(li, LIBINPUT_EVENT_DEVICE_ADDED);
	event = libinput_get_event(li);
	device = libinput_event_get_device(event);
	seat = libinput_device_get_seat(device);
	litest_assert_notnull(seat);

	seat_name = libinput_seat_get_logical_name(seat);
	litest_assert_str_eq(seat_name, "default");
	libinput_event_destroy(event);
}
END_TEST

START_TEST(udev_change_seat)
{
	const char *seat2_name = "new seat";

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);
	litest_dispatch(li);

	/* Drop any events from other devices */
	litest_drain_events(li);

	const char *devname = "udev-change-seat device";

	/* Now create our own device, it should be in the "default"
	 * logical seat. This test may fail if there is a local rule changing
	 * that, but it'll be fine for the 99% case. */
	_unused_ _destroy_(litest_device) *dev =
		litest_create(LITEST_MOUSE, devname, NULL, NULL, NULL);

	_unref_(libinput_device) *device = NULL;
	_autofree_ char *seat1_name = NULL;
	while (true) {
		litest_wait_for_event_of_type(li, LIBINPUT_EVENT_DEVICE_ADDED);

		_destroy_(libinput_event) *event = libinput_get_event(li);
		struct libinput_device *d = libinput_event_get_device(event);
		const char *name = libinput_device_get_name(d);
		if (strendswith(name, devname)) {
			device = libinput_device_ref(d);
			struct libinput_seat *seat = libinput_device_get_seat(device);
			seat1_name = safe_strdup(libinput_seat_get_logical_name(seat));
			break;
		}
	}
	litest_drain_events(li);

	/* Changing the logical seat name will remove and re-add the device */
	int rc = libinput_device_set_seat_logical_name(device, seat2_name);
	litest_assert_int_eq(rc, 0);

	while (true) {
		litest_wait_for_event_of_type(li, LIBINPUT_EVENT_DEVICE_REMOVED);
		_destroy_(libinput_event) *event = libinput_get_event(li);
		litest_assert_event_type(event, LIBINPUT_EVENT_DEVICE_REMOVED);
		if (libinput_event_get_device(event) == device)
			break;
	}

	_unref_(libinput_seat) *seat2 = NULL;
	while (true) {
		litest_wait_for_event_of_type(li, LIBINPUT_EVENT_DEVICE_ADDED);

		_destroy_(libinput_event) *event = libinput_get_event(li);
		litest_assert_event_type(event, LIBINPUT_EVENT_DEVICE_ADDED);
		struct libinput_device *d = libinput_event_get_device(event);
		const char *name = libinput_device_get_name(d);
		if (strendswith(name, devname)) {
			seat2 = libinput_device_get_seat(d);
			libinput_seat_ref(seat2);
			break;
		}
	}

	litest_assert_str_ne(libinput_seat_get_logical_name(seat2), seat1_name);
	litest_assert_str_eq(libinput_seat_get_logical_name(seat2), seat2_name);
}
END_TEST

START_TEST(udev_double_suspend)
{
	struct libinput_event *event;
	int fd;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	fd = libinput_get_fd(li);
	litest_assert_int_ge(fd, 0);

	/* expect at least one event */
	litest_assert_int_ge(litest_dispatch(li), 0);
	event = libinput_get_event(li);
	litest_assert_notnull(event);

	libinput_suspend(li);
	libinput_suspend(li);
	libinput_resume(li);

	libinput_event_destroy(event);
}
END_TEST

START_TEST(udev_double_resume)
{
	struct libinput_event *event;
	int fd;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	fd = libinput_get_fd(li);
	litest_assert_int_ge(fd, 0);

	/* expect at least one event */
	litest_assert_int_ge(litest_dispatch(li), 0);
	event = libinput_get_event(li);
	litest_assert_notnull(event);

	libinput_suspend(li);
	libinput_resume(li);
	libinput_resume(li);

	libinput_event_destroy(event);
}
END_TEST

static void
process_events_count_devices(struct libinput *li, int *device_count)
{
	struct libinput_event *event;

	while ((event = libinput_get_event(li))) {
		switch (libinput_event_get_type(event)) {
		case LIBINPUT_EVENT_DEVICE_ADDED:
			(*device_count)++;
			break;
		case LIBINPUT_EVENT_DEVICE_REMOVED:
			(*device_count)--;
			break;
		default:
			break;
		}
		libinput_event_destroy(event);
	}
}

START_TEST(udev_suspend_resume)
{
	int fd;
	int num_devices = 0;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	fd = libinput_get_fd(li);
	litest_assert_int_ge(fd, 0);

	/* Check that at least one device was discovered after creation. */
	litest_assert_int_ge(litest_dispatch(li), 0);
	process_events_count_devices(li, &num_devices);
	litest_assert_int_gt(num_devices, 0);

	/* Check that after a suspend, no devices are left. */
	libinput_suspend(li);
	litest_assert_int_ge(litest_dispatch(li), 0);
	process_events_count_devices(li, &num_devices);
	litest_assert_int_eq(num_devices, 0);

	/* Check that after a resume, at least one device is discovered. */
	libinput_resume(li);
	litest_assert_int_ge(litest_dispatch(li), 0);
	process_events_count_devices(li, &num_devices);
	litest_assert_int_gt(num_devices, 0);
}
END_TEST

START_TEST(udev_resume_before_seat)
{
	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);

	int rc = libinput_resume(li);
	litest_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(udev_suspend_resume_before_seat)
{
	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);

	libinput_suspend(li);
	int rc = libinput_resume(li);
	litest_assert_int_eq(rc, 0);
}
END_TEST

START_TEST(udev_device_sysname)
{
	struct libinput_event *ev;
	struct libinput_device *device;
	const char *sysname;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	litest_dispatch(li);

	while ((ev = libinput_get_event(li))) {
		if (libinput_event_get_type(ev) != LIBINPUT_EVENT_DEVICE_ADDED) {
			libinput_event_destroy(ev);
			continue;
		}

		device = libinput_event_get_device(ev);
		sysname = libinput_device_get_sysname(device);
		litest_assert_notnull(sysname);
		litest_assert_int_gt(strlen(sysname), 1U);
		litest_assert(strchr(sysname, '/') == NULL);
		litest_assert(strstartswith(sysname, "event"));
		libinput_event_destroy(ev);
	}
}
END_TEST

START_TEST(udev_seat_recycle)
{
	struct libinput_event *ev;
	struct libinput_device *device;
	struct libinput_seat *saved_seat = NULL;
	struct libinput_seat *seat;
	int data = 0;
	int found = 0;
	void *user_data;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	litest_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_DEVICE_ADDED:
			if (saved_seat)
				break;

			device = libinput_event_get_device(ev);
			litest_assert_notnull(device);
			saved_seat = libinput_device_get_seat(device);
			libinput_seat_set_user_data(saved_seat, &data);
			libinput_seat_ref(saved_seat);
			break;
		default:
			break;
		}

		libinput_event_destroy(ev);
	}

	litest_assert_notnull(saved_seat);

	libinput_suspend(li);

	litest_drain_events(li);

	libinput_resume(li);

	litest_dispatch(li);
	while ((ev = libinput_get_event(li))) {
		switch (libinput_event_get_type(ev)) {
		case LIBINPUT_EVENT_DEVICE_ADDED:
			device = libinput_event_get_device(ev);
			litest_assert_notnull(device);

			seat = libinput_device_get_seat(device);
			user_data = libinput_seat_get_user_data(seat);
			if (user_data == &data) {
				found = 1;
				litest_assert(seat == saved_seat);
			}
			break;
		default:
			break;
		}

		libinput_event_destroy(ev);
	}

	litest_assert(found == 1);
}
END_TEST

START_TEST(udev_path_add_device)
{
	struct libinput_device *device;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);

	litest_set_log_handler_bug(li);
	device = libinput_path_add_device(li, "/dev/input/event0");
	litest_assert(device == NULL);
	litest_restore_log_handler(li);
}
END_TEST

START_TEST(udev_path_remove_device)
{
	struct libinput_device *device;
	struct libinput_event *event;

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);
	litest_dispatch(li);

	litest_wait_for_event_of_type(li, LIBINPUT_EVENT_DEVICE_ADDED);
	event = libinput_get_event(li);
	device = libinput_event_get_device(event);
	litest_assert_notnull(device);

	/* no effect bug a bug log msg */
	litest_set_log_handler_bug(li);
	libinput_path_remove_device(device);
	litest_restore_log_handler(li);

	libinput_event_destroy(event);
}
END_TEST

START_TEST(udev_ignore_device)
{
	struct libinput_device *device;
	struct libinput_event *event;
	const char *devname;

	_destroy_(litest_device) *dev =
		litest_create(LITEST_IGNORED_MOUSE, NULL, NULL, NULL, NULL);
	devname = libevdev_get_name(dev->evdev);

	_unref_(udev) *udev = udev_new();
	litest_assert_notnull(udev);

	_unref_(libinput) *li =
		libinput_udev_create_context(&simple_interface, NULL, udev);
	litest_assert_notnull(li);
	litest_restore_log_handler(li);

	litest_assert_int_eq(libinput_udev_assign_seat(li, "seat0"), 0);
	litest_dispatch(li);

	event = libinput_get_event(li);
	litest_assert_notnull(event);
	while (event) {
		if (libinput_event_get_type(event) == LIBINPUT_EVENT_DEVICE_ADDED) {
			const char *name;

			device = libinput_event_get_device(event);
			name = libinput_device_get_name(device);
			litest_assert_str_ne(devname, name);
		}
		libinput_event_destroy(event);
		litest_dispatch(li);
		event = libinput_get_event(li);
	}
}
END_TEST

TEST_COLLECTION(udev)
{
	/* clang-format off */
	litest_add_no_device(udev_create_NULL);
	litest_add_no_device(udev_create_seat0);
	litest_add_no_device(udev_create_empty_seat);
	litest_add_no_device(udev_create_seat_too_long);
	litest_add_no_device(udev_set_user_data);

	litest_add_no_device(udev_added_seat_default);
	litest_add_no_device(udev_change_seat);

	litest_add_for_device(udev_double_suspend, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_double_resume, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_suspend_resume, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_resume_before_seat, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_suspend_resume_before_seat, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_device_sysname, LITEST_SYNAPTICS_CLICKPAD_X220);
	litest_add_for_device(udev_seat_recycle, LITEST_SYNAPTICS_CLICKPAD_X220);

	litest_add_no_device(udev_path_add_device);
	litest_add_for_device(udev_path_remove_device, LITEST_SYNAPTICS_CLICKPAD_X220);

	litest_add_no_device(udev_ignore_device);
	/* clang-format on */
}
