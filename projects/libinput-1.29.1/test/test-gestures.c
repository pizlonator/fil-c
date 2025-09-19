/*
 * Copyright © 2015 Red Hat, Inc.
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

#include <libinput.h>
#include <valgrind/valgrind.h>

#include "libinput-util.h"
#include "litest.h"

enum cardinal { N, NE, E, SE, S, SW, W, NW, NCARDINALS };

enum hold_gesture_behaviour {
	HOLD_GESTURE_IGNORE,
	HOLD_GESTURE_REQUIRE,
};

static void
test_gesture_swipe_3fg(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) < 3)
		return;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 40);
	litest_touch_down(dev, 1, 50, 40);
	litest_touch_down(dev, 2, 60, 40);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	litest_touch_move_three_touches(dev, 40, 40, 50, 40, 60, 40, dir_x, dir_y, 10);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 3);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 3);
	}

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 3);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE,
						 3);

		dx = libinput_event_gesture_get_dx(gevent);
		dy = libinput_event_gesture_get_dy(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		dx = libinput_event_gesture_get_dx_unaccelerated(gevent);
		dy = libinput_event_gesture_get_dy_unaccelerated(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		libinput_event_destroy(event);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 2);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_END, 3);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_swipe_4fg(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int cardinals[NCARDINALS][2] = {
		{ 0, 3 },  { 3, 3 },   { 3, 0 },  { 3, -3 },
		{ 0, -3 }, { -3, -3 }, { -3, 0 }, { -3, 3 },
	};
	int i;

	if (litest_slot_count(dev) < 4)
		return;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 40);
	litest_touch_down(dev, 1, 50, 40);
	litest_touch_down(dev, 2, 60, 40);
	litest_touch_down(dev, 3, 70, 40);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	for (i = 0; i < 8; i++) {
		litest_push_event_frame(dev);

		dir_x += cardinals[cardinal][0];
		dir_y += cardinals[cardinal][1];

		litest_touch_move(dev, 0, 40 + dir_x, 40 + dir_y);
		litest_touch_move(dev, 1, 50 + dir_x, 40 + dir_y);
		litest_touch_move(dev, 2, 60 + dir_x, 40 + dir_y);
		litest_touch_move(dev, 3, 70 + dir_x, 40 + dir_y);
		litest_pop_event_frame(dev);
		litest_dispatch(li);
	}

	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 4);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 4);
	}

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 4);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE,
						 4);

		dx = libinput_event_gesture_get_dx(gevent);
		dy = libinput_event_gesture_get_dy(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		dx = libinput_event_gesture_get_dx_unaccelerated(gevent);
		dy = libinput_event_gesture_get_dy_unaccelerated(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		libinput_event_destroy(event);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 2);
	litest_touch_up(dev, 3);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_END, 4);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_pinch_2fg(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int i;
	double scale, oldscale;
	double angle;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) < 2 ||
	    !libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return;

	/* If the device is too small to provide a finger spread wide enough
	 * to avoid the scroll bias, skip the test */
	if (cardinal == E || cardinal == W) {
		double w = 0, h = 0;
		libinput_device_get_size(dev->libinput_device, &w, &h);
		/* 0.6 because the code below gives us points like 20/y and
		 * 80/y. 45 because the threshold in the code is 40mm */
		if (w * 0.6 < 45)
			return;
	}

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50 + dir_x, 50 + dir_y);
	litest_touch_down(dev, 1, 50 - dir_x, 50 - dir_y);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	for (i = 0; i < 8; i++) {
		litest_push_event_frame(dev);
		if (dir_x > 0.0)
			dir_x -= 2;
		else if (dir_x < 0.0)
			dir_x += 2;
		if (dir_y > 0.0)
			dir_y -= 2;
		else if (dir_y < 0.0)
			dir_y += 2;
		litest_touch_move(dev, 0, 50 + dir_x, 50 + dir_y);
		litest_touch_move(dev, 1, 50 - dir_x, 50 - dir_y);
		litest_pop_event_frame(dev);
		litest_dispatch(li);
	}

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 2);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 2);
	}

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_BEGIN, 2);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	scale = libinput_event_gesture_get_scale(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	litest_assert(scale == 1.0);

	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_PINCH_UPDATE,
						 2);

		oldscale = scale;
		scale = libinput_event_gesture_get_scale(gevent);

		litest_assert(scale < oldscale);

		angle = libinput_event_gesture_get_angle_delta(gevent);
		litest_assert_double_le(fabs(angle), 1.0);

		libinput_event_destroy(event);
		litest_dispatch(li);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_END, 2);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_pinch_3fg(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int i;
	double scale, oldscale;
	double angle;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) < 3)
		return;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50 + dir_x, 50 + dir_y);
	litest_touch_down(dev, 1, 50 - dir_x, 50 - dir_y);
	litest_touch_down(dev, 2, 51 - dir_x, 51 - dir_y);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	for (i = 0; i < 8; i++) {
		litest_push_event_frame(dev);
		if (dir_x > 0.0)
			dir_x -= 2;
		else if (dir_x < 0.0)
			dir_x += 2;
		if (dir_y > 0.0)
			dir_y -= 2;
		else if (dir_y < 0.0)
			dir_y += 2;
		litest_touch_move(dev, 0, 50 + dir_x, 50 + dir_y);
		litest_touch_move(dev, 1, 50 - dir_x, 50 - dir_y);
		litest_touch_move(dev, 2, 51 - dir_x, 51 - dir_y);
		litest_pop_event_frame(dev);
		litest_dispatch(li);
	}

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 3);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 3);
	}
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_BEGIN, 3);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	scale = libinput_event_gesture_get_scale(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	litest_assert(scale == 1.0);

	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_PINCH_UPDATE,
						 3);

		oldscale = scale;
		scale = libinput_event_gesture_get_scale(gevent);

		litest_assert(scale < oldscale);

		angle = libinput_event_gesture_get_angle_delta(gevent);
		litest_assert_double_le(fabs(angle), 1.0);

		libinput_event_destroy(event);
		litest_dispatch(li);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 2);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_END, 3);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_pinch_4fg(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int i;
	double scale, oldscale;
	double angle;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) < 4)
		return;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50 + dir_x, 50 + dir_y);
	litest_touch_down(dev, 1, 50 - dir_x, 50 - dir_y);
	litest_touch_down(dev, 2, 51 - dir_x, 51 - dir_y);
	litest_touch_down(dev, 3, 52 - dir_x, 52 - dir_y);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	for (i = 0; i < 7; i++) {
		litest_push_event_frame(dev);
		if (dir_x > 0.0)
			dir_x -= 2;
		else if (dir_x < 0.0)
			dir_x += 2;
		if (dir_y > 0.0)
			dir_y -= 2;
		else if (dir_y < 0.0)
			dir_y += 2;
		litest_touch_move(dev, 0, 50 + dir_x, 50 + dir_y);
		litest_touch_move(dev, 1, 50 - dir_x, 50 - dir_y);
		litest_touch_move(dev, 2, 51 - dir_x, 51 - dir_y);
		litest_touch_move(dev, 3, 52 - dir_x, 52 - dir_y);
		litest_pop_event_frame(dev);
		litest_dispatch(li);
	}

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 4);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 4);
	}

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_BEGIN, 4);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	scale = libinput_event_gesture_get_scale(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	litest_assert(scale == 1.0);

	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_PINCH_UPDATE,
						 4);

		oldscale = scale;
		scale = libinput_event_gesture_get_scale(gevent);

		litest_assert(scale < oldscale);

		angle = libinput_event_gesture_get_angle_delta(gevent);
		litest_assert_double_le(fabs(angle), 1.0);

		libinput_event_destroy(event);
		litest_dispatch(li);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 2);
	litest_touch_up(dev, 3);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_END, 4);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_spread(enum cardinal cardinal, enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	double dir_x, dir_y;
	int i;
	double scale, oldscale;
	double angle;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) < 2 ||
	    !libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return;

	/* If the device is too small to provide a finger spread wide enough
	 * to avoid the scroll bias, skip the test */
	if (cardinal == E || cardinal == W) {
		double w = 0, h = 0;
		libinput_device_get_size(dev->libinput_device, &w, &h);
		/* 0.6 because the code below gives us points like 20/y and
		 * 80/y. 45 because the threshold in the code is 40mm */
		if (w * 0.6 < 45)
			return;
	}

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50 + dir_x, 50 + dir_y);
	litest_touch_down(dev, 1, 50 - dir_x, 50 - dir_y);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	for (i = 0; i < 15; i++) {
		litest_push_event_frame(dev);
		if (dir_x > 0.0)
			dir_x += 1;
		else if (dir_x < 0.0)
			dir_x -= 1;
		if (dir_y > 0.0)
			dir_y += 1;
		else if (dir_y < 0.0)
			dir_y -= 1;
		litest_touch_move(dev, 0, 50 + dir_x, 50 + dir_y);
		litest_touch_move(dev, 1, 50 - dir_x, 50 - dir_y);
		litest_pop_event_frame(dev);
		litest_dispatch(li);
	}

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 2);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 2);
	}

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_BEGIN, 2);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	scale = libinput_event_gesture_get_scale(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	litest_assert(scale == 1.0);

	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_PINCH_UPDATE,
						 2);
		oldscale = scale;
		scale = libinput_event_gesture_get_scale(gevent);
		litest_assert(scale > oldscale);

		angle = libinput_event_gesture_get_angle_delta(gevent);
		litest_assert_double_le(fabs(angle), 1.0);

		libinput_event_destroy(event);
		litest_dispatch(li);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_PINCH_END, 2);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}

static void
test_gesture_3fg_buttonarea_scroll(enum hold_gesture_behaviour hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (litest_slot_count(dev) < 3)
		return;

	litest_enable_buttonareas(dev);
	litest_enable_2fg_scroll(dev);
	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 20);
	litest_touch_down(dev, 1, 30, 20);
	/* third finger in btnarea */
	litest_touch_down(dev, 2, 50, 99);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE)
		litest_timeout_gesture_hold(li);

	litest_touch_move_two_touches(dev, 40, 20, 30, 20, 0, 40, 10);

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);

	if (hold == HOLD_GESTURE_REQUIRE) {
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 2);
		litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 2);
	}

	litest_assert_scroll(li,
			     LIBINPUT_EVENT_POINTER_SCROLL_FINGER,
			     LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
			     4);
}

static void
test_gesture_hold(int nfingers)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (litest_slot_count(dev) < nfingers)
		return;

	litest_drain_events(li);

	switch (nfingers) {
	case 4:
		litest_touch_down(dev, 3, 70, 30);
		_fallthrough_;
	case 3:
		litest_touch_down(dev, 2, 60, 30);
		_fallthrough_;
	case 2:
		litest_touch_down(dev, 1, 50, 30);
		_fallthrough_;
	case 1:
		litest_touch_down(dev, 0, 40, 30);
		break;
	}

	litest_timeout_gesture_hold(li);

	if (libinput_device_has_capability(dev->libinput_device,
					   LIBINPUT_DEVICE_CAP_GESTURE)) {
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_BEGIN,
					    nfingers);
	} else {
		litest_assert_empty_queue(li);
	}

	switch (nfingers) {
	case 4:
		litest_touch_up(dev, 3);
		_fallthrough_;
	case 3:
		litest_touch_up(dev, 2);
		_fallthrough_;
	case 2:
		litest_touch_up(dev, 1);
		_fallthrough_;
	case 1:
		litest_touch_up(dev, 0);
		break;
	}

	litest_dispatch(li);
	if (libinput_device_has_capability(dev->libinput_device,
					   LIBINPUT_DEVICE_CAP_GESTURE)) {
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_END,
					    nfingers);
	}

	litest_assert_empty_queue(li);
}

static void
test_gesture_hold_cancel(int nfingers)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int last_finger = (nfingers - 1);

	if (litest_slot_count(dev) < nfingers)
		return;

	litest_drain_events(li);

	switch (nfingers) {
	case 4:
		litest_touch_down(dev, 3, 70, 30);
		_fallthrough_;
	case 3:
		litest_touch_down(dev, 2, 60, 30);
		_fallthrough_;
	case 2:
		litest_touch_down(dev, 1, 50, 30);
		_fallthrough_;
	case 1:
		litest_touch_down(dev, 0, 40, 30);
		break;
	}

	litest_timeout_gesture_hold(li);

	litest_touch_up(dev, last_finger);

	if (libinput_device_has_capability(dev->libinput_device,
					   LIBINPUT_DEVICE_CAP_GESTURE)) {
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_BEGIN,
					    nfingers);
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_END,
					    nfingers);
	}

	litest_assert_empty_queue(li);
}

START_TEST(gestures_cap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	if (libevdev_has_property(dev->evdev, INPUT_PROP_SEMI_MT))
		litest_assert(
			!libinput_device_has_capability(device,
							LIBINPUT_DEVICE_CAP_GESTURE));
	else
		litest_assert(
			libinput_device_has_capability(device,
						       LIBINPUT_DEVICE_CAP_GESTURE));
}
END_TEST

START_TEST(gestures_nocap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert(
		!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_GESTURE));
}
END_TEST

START_TEST(gestures_swipe_3fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_swipe_3fg(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_swipe_3fg_btntool)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	double dir_x, dir_y;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) > 2 ||
	    !libevdev_has_event_code(dev->evdev, EV_KEY, BTN_TOOL_TRIPLETAP) ||
	    !libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 40);
	litest_touch_down(dev, 1, 50, 40);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_dispatch(li);
	litest_touch_move_two_touches(dev, 40, 40, 50, 40, dir_x, dir_y, 10);
	litest_dispatch(li);

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 3);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE,
						 3);

		dx = libinput_event_gesture_get_dx(gevent);
		dy = libinput_event_gesture_get_dy(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		dx = libinput_event_gesture_get_dx_unaccelerated(gevent);
		dy = libinput_event_gesture_get_dy_unaccelerated(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		libinput_event_destroy(event);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_END, 3);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}
END_TEST

START_TEST(gestures_swipe_3fg_btntool_pinch_like)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;

	if (litest_slot_count(dev) > 2 ||
	    !libevdev_has_event_code(dev->evdev, EV_KEY, BTN_TOOL_TRIPLETAP) ||
	    !libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	/* Technically a pinch position + pinch movement, but expect swipe
	 * for nfingers > nslots */
	litest_touch_down(dev, 0, 20, 60);
	litest_touch_down(dev, 1, 50, 20);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_dispatch(li);
	litest_touch_move_to(dev, 0, 20, 60, 10, 80, 20);
	litest_dispatch(li);

	event = libinput_get_event(li);
	litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 3);
	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE, 3);
		libinput_event_destroy(event);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_END, 3);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}
END_TEST

START_TEST(gestures_swipe_4fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_swipe_4fg(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_swipe_4fg_btntool)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	double dx, dy;
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	double dir_x, dir_y;
	int cardinals[NCARDINALS][2] = {
		{ 0, 30 },  { 30, 30 },   { 30, 0 },  { 30, -30 },
		{ 0, -30 }, { -30, -30 }, { -30, 0 }, { -30, 30 },
	};

	if (litest_slot_count(dev) > 2 ||
	    !libevdev_has_event_code(dev->evdev, EV_KEY, BTN_TOOL_QUADTAP) ||
	    !libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	dir_x = cardinals[cardinal][0];
	dir_y = cardinals[cardinal][1];

	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 40);
	litest_touch_down(dev, 1, 50, 40);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_QUADTAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);

	litest_dispatch(li);
	litest_touch_move_two_touches(dev, 40, 40, 50, 40, dir_x, dir_y, 10);
	litest_dispatch(li);

	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 4);
	dx = libinput_event_gesture_get_dx(gevent);
	dy = libinput_event_gesture_get_dy(gevent);
	litest_assert(dx == 0.0);
	litest_assert(dy == 0.0);
	libinput_event_destroy(event);

	while ((event = libinput_get_event(li)) != NULL) {
		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE,
						 4);

		dx = libinput_event_gesture_get_dx(gevent);
		dy = libinput_event_gesture_get_dy(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		dx = libinput_event_gesture_get_dx_unaccelerated(gevent);
		dy = libinput_event_gesture_get_dy_unaccelerated(gevent);
		if (dir_x == 0.0)
			litest_assert(dx == 0.0);
		else if (dir_x < 0.0)
			litest_assert(dx < 0.0);
		else if (dir_x > 0.0)
			litest_assert(dx > 0.0);

		if (dir_y == 0.0)
			litest_assert(dy == 0.0);
		else if (dir_y < 0.0)
			litest_assert(dy < 0.0);
		else if (dir_y > 0.0)
			litest_assert(dy > 0.0);

		libinput_event_destroy(event);
	}

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_END, 4);
	litest_assert(!libinput_event_gesture_get_cancelled(gevent));
	libinput_event_destroy(event);
}
END_TEST

START_TEST(gestures_pinch)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_2fg(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_pinch_3fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_3fg(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_pinch_4fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_4fg(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_spread)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_spread(cardinal, HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_time_usec)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	struct libinput_event_gesture *gevent;
	uint64_t time_usec;

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 40, 40);
	litest_touch_down(dev, 1, 50, 40);
	litest_touch_down(dev, 2, 60, 40);
	litest_dispatch(li);
	litest_touch_move_three_touches(dev, 40, 40, 50, 40, 60, 40, 0, 30, 30);

	litest_dispatch(li);
	event = libinput_get_event(li);
	gevent = litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 3);
	time_usec = libinput_event_gesture_get_time_usec(gevent);
	litest_assert_int_eq(libinput_event_gesture_get_time(gevent),
			     (uint32_t)(time_usec / 1000));
	libinput_event_destroy(event);
}
END_TEST

START_TEST(gestures_3fg_buttonarea_scroll)
{
	test_gesture_3fg_buttonarea_scroll(HOLD_GESTURE_IGNORE);
}
END_TEST

START_TEST(gestures_3fg_buttonarea_scroll_btntool)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (litest_slot_count(dev) > 2)
		return LITEST_NOT_APPLICABLE;

	litest_enable_buttonareas(dev);
	litest_enable_2fg_scroll(dev);
	litest_drain_events(li);

	/* first finger in btnarea */
	litest_touch_down(dev, 0, 20, 99);
	litest_touch_down(dev, 1, 30, 20);
	litest_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, 0);
	litest_event(dev, EV_KEY, BTN_TOOL_TRIPLETAP, 1);
	litest_event(dev, EV_SYN, SYN_REPORT, 0);
	litest_dispatch(li);
	litest_touch_move_to(dev, 1, 30, 20, 30, 70, 10);

	litest_touch_up(dev, 1);
	litest_dispatch(li);
	litest_assert_scroll(li,
			     LIBINPUT_EVENT_POINTER_SCROLL_FINGER,
			     LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
			     4);
}
END_TEST

START_TEST(gestures_swipe_3fg_unaccel)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	struct libinput_event *event;
	double reference_ux = 0, reference_uy = 0;

	/**
	 * This magic number is an artifact of the acceleration code.
	 * The maximum factor in the touchpad accel profile is 4.8 times the
	 * speed setting (1.000875 at default setting 0). The factor
	 * applied to the const acceleration is the 0.9 baseline.
	 * So our two sets of coordinates are:
	 * accel = 4.8 * delta * normalize_magic
	 * unaccel = 0.9 * delta * normalize_magic
	 *
	 * Since delta and the normalization magic are the same for both,
	 * our accelerated deltas can be a maximum of 4.8/0.9 bigger than
	 * the unaccelerated deltas.
	 *
	 * If any of the accel methods numbers change, this will have to
	 * change here too.
	 */
	const double max_factor = 5.34;

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);
	litest_touch_down(dev, 0, 40, 20);
	litest_touch_down(dev, 1, 50, 20);
	litest_touch_down(dev, 2, 60, 20);
	litest_dispatch(li);
	litest_touch_move_three_touches(dev, 40, 20, 50, 20, 60, 20, 30, 40, 10);
	litest_dispatch(li);

	event = libinput_get_event(li);
	litest_is_gesture_event(event, LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN, 3);
	libinput_event_destroy(event);
	event = libinput_get_event(li);
	do {
		struct libinput_event_gesture *gevent;
		double dx, dy;
		double ux, uy;

		gevent = litest_is_gesture_event(event,
						 LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE,
						 3);
		dx = libinput_event_gesture_get_dx(gevent);
		dy = libinput_event_gesture_get_dy(gevent);
		ux = libinput_event_gesture_get_dx_unaccelerated(gevent);
		uy = libinput_event_gesture_get_dy_unaccelerated(gevent);

		litest_assert_double_ne(ux, 0.0);
		litest_assert_double_ne(uy, 0.0);

		if (!reference_ux)
			reference_ux = ux;
		if (!reference_uy)
			reference_uy = uy;

		/* The unaccelerated delta should be the same for every
		 * event, but we have rounding errors since we only control
		 * input data as percentage of the touchpad size.
		 * so we just eyeball it */
		litest_assert_double_gt(ux, reference_ux - 2);
		litest_assert_double_lt(ux, reference_ux + 2);
		litest_assert_double_gt(uy, reference_uy - 2);
		litest_assert_double_lt(uy, reference_uy + 2);

		/* All our touchpads are large enough to make this is a fast
		 * swipe, we don't expect deceleration, unaccel should
		 * always be less than accel delta */
		litest_assert_double_lt(ux, dx);
		litest_assert_double_lt(ux, dx);

		/* Check our accelerated delta is within the expected
		 * maximum. */
		litest_assert_double_lt(dx, ux * max_factor);
		litest_assert_double_lt(dy, uy * max_factor);

		libinput_event_destroy(event);
	} while ((event = libinput_get_event(li)));

	litest_touch_up(dev, 0);
	litest_touch_up(dev, 1);
	litest_touch_up(dev, 2);
}
END_TEST

START_TEST(gestures_hold_config_default_disabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_config_gesture_hold_is_available(device),
			     0);
	litest_assert_enum_eq(
		libinput_device_config_gesture_get_hold_default_enabled(device),
		LIBINPUT_CONFIG_HOLD_DISABLED);
	litest_assert_enum_eq(
		libinput_device_config_gesture_get_hold_default_enabled(device),
		LIBINPUT_CONFIG_HOLD_DISABLED);
}
END_TEST

START_TEST(gestures_hold_config_default_enabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_config_gesture_hold_is_available(device),
			     1);
	litest_assert_enum_eq(
		libinput_device_config_gesture_get_hold_default_enabled(device),
		LIBINPUT_CONFIG_HOLD_ENABLED);
	litest_assert_enum_eq(libinput_device_config_gesture_get_hold_enabled(device),
			      LIBINPUT_CONFIG_HOLD_ENABLED);
}
END_TEST

START_TEST(gestures_hold_config_set_invalid)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_enum_eq(
		libinput_device_config_gesture_set_hold_enabled(device, -1),
		LIBINPUT_CONFIG_STATUS_INVALID);
	litest_assert_enum_eq(
		libinput_device_config_gesture_set_hold_enabled(device, 2),
		LIBINPUT_CONFIG_STATUS_INVALID);
}
END_TEST

START_TEST(gestures_hold_config_is_available)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_config_gesture_hold_is_available(device),
			     1);
	litest_assert_enum_eq(libinput_device_config_gesture_get_hold_enabled(device),
			      LIBINPUT_CONFIG_HOLD_ENABLED);
	litest_assert_enum_eq(libinput_device_config_gesture_set_hold_enabled(
				      device,
				      LIBINPUT_CONFIG_HOLD_DISABLED),
			      LIBINPUT_CONFIG_STATUS_SUCCESS);
	litest_assert_enum_eq(libinput_device_config_gesture_get_hold_enabled(device),
			      LIBINPUT_CONFIG_HOLD_DISABLED);
}
END_TEST

START_TEST(gestures_hold_config_is_not_available)
{
	struct litest_device *dev = litest_current_device();
	struct libinput_device *device = dev->libinput_device;

	litest_assert_int_eq(libinput_device_config_gesture_hold_is_available(device),
			     0);
	litest_assert_enum_eq(libinput_device_config_gesture_get_hold_enabled(device),
			      LIBINPUT_CONFIG_HOLD_DISABLED);
	litest_assert_enum_eq(libinput_device_config_gesture_set_hold_enabled(
				      device,
				      LIBINPUT_CONFIG_HOLD_ENABLED),
			      LIBINPUT_CONFIG_STATUS_UNSUPPORTED);
	litest_assert_enum_eq(libinput_device_config_gesture_set_hold_enabled(
				      device,
				      LIBINPUT_CONFIG_HOLD_DISABLED),
			      LIBINPUT_CONFIG_STATUS_SUCCESS);
}
END_TEST

START_TEST(gestures_hold)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int nfingers = litest_test_param_get_i32(test_env->params, "fingers");

	litest_disable_tap(dev->libinput_device);
	litest_drain_events(li);

	test_gesture_hold(nfingers);
}
END_TEST

START_TEST(gestures_hold_tap_enabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int nfingers = litest_test_param_get_i32(test_env->params, "fingers");

	litest_enable_tap(dev->libinput_device);
	litest_drain_events(li);

	test_gesture_hold(nfingers);
}
END_TEST

START_TEST(gestures_hold_cancel)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int nfingers = litest_test_param_get_i32(test_env->params, "fingers");

	litest_disable_tap(dev->libinput_device);
	litest_drain_events(li);

	test_gesture_hold_cancel(nfingers);
}
END_TEST

START_TEST(gestures_hold_cancel_tap_enabled)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int nfingers = litest_test_param_get_i32(test_env->params, "fingers");

	litest_enable_tap(dev->libinput_device);
	litest_drain_events(li);

	test_gesture_hold_cancel(nfingers);
}
END_TEST

START_TEST(gestures_hold_then_swipe_3fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_swipe_3fg(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_swipe_4fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_swipe_4fg(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_pinch_2fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_2fg(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_pinch_3fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_3fg(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_pinch_4fg)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_pinch_4fg(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_spread)
{
	enum cardinal cardinal =
		litest_test_param_get_i32(test_env->params, "direction");
	test_gesture_spread(cardinal, HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_then_3fg_buttonarea_scroll)
{
	test_gesture_3fg_buttonarea_scroll(HOLD_GESTURE_REQUIRE);
}
END_TEST

START_TEST(gestures_hold_once_on_double_tap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	litest_enable_tap(dev->libinput_device);
	litest_drain_events(li);

	/* First tap, a hold gesture must be generated */
	litest_touch_down(dev, 0, 50, 50);
	litest_timeout_gesture_quick_hold(li);
	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 1);
	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 1);
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_empty_queue(li);

	/* Double tap, don't generate an extra hold gesture */
	litest_touch_down(dev, 0, 50, 50);
	litest_touch_up(dev, 0);
	litest_timeout_gesture_quick_hold(li);

	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(gestures_hold_once_tap_n_drag)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int nfingers = litest_test_param_get_i32(test_env->params, "fingers");
	unsigned int button = 0;

	if (nfingers > litest_slot_count(dev))
		return LITEST_NOT_APPLICABLE;

	if (!libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	litest_enable_tap(dev->libinput_device);
	litest_disable_drag_lock(dev->libinput_device);
	litest_drain_events(li);

	switch (nfingers) {
	case 1:
		button = BTN_LEFT;
		break;
	case 2:
		button = BTN_RIGHT;
		break;
	case 3:
		button = BTN_MIDDLE;
		break;
	default:
		litest_assert_not_reached();
	}

	switch (nfingers) {
	case 3:
		litest_touch_down(dev, 2, 60, 30);
		_fallthrough_;
	case 2:
		litest_touch_down(dev, 1, 50, 30);
		_fallthrough_;
	case 1:
		litest_touch_down(dev, 0, 40, 30);
		break;
	}
	litest_timeout_gesture_quick_hold(li);

	switch (nfingers) {
	case 3:
		litest_touch_up(dev, 2);
		_fallthrough_;
	case 2:
		litest_touch_up(dev, 1);
		_fallthrough_;
	case 1:
		litest_touch_up(dev, 0);
		break;
	}
	litest_dispatch(li);

	/* "Quick" hold gestures are only generated when using 1 or 2 fingers */
	if (nfingers == 1 || nfingers == 2) {
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_BEGIN,
					    nfingers);
		litest_assert_gesture_event(li,
					    LIBINPUT_EVENT_GESTURE_HOLD_END,
					    nfingers);
	}

	/* Tap and drag, don't generate an extra hold gesture */
	litest_touch_down(dev, 0, 50, 50);
	litest_touch_move_to(dev, 0, 50, 50, 80, 80, 20);
	litest_dispatch(li);

	litest_assert_button_event(li, button, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_assert_button_event(li, button, LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(gestures_hold_and_motion_before_timeout)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_dispatch(li);

	litest_touch_move_to(dev, 0, 50, 50, 51, 51, 1);
	litest_touch_move_to(dev, 0, 51, 51, 50, 50, 1);

	litest_timeout_gesture_quick_hold(li);

	litest_drain_events_of_type(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 1);

	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 1);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(gestures_hold_and_motion_after_timeout)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	if (!libinput_device_has_capability(dev->libinput_device,
					    LIBINPUT_DEVICE_CAP_GESTURE))
		return LITEST_NOT_APPLICABLE;

	litest_drain_events(li);

	litest_touch_down(dev, 0, 50, 50);
	litest_timeout_gesture_quick_hold(li);

	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_BEGIN, 1);

	litest_touch_move_to(dev, 0, 50, 50, 51, 51, 1);
	litest_touch_move_to(dev, 0, 51, 51, 50, 50, 1);
	litest_dispatch(li);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_assert_gesture_event(li, LIBINPUT_EVENT_GESTURE_HOLD_END, 1);
	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(gestures_3fg_drag)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	uint32_t finger_count = litest_test_param_get_u32(test_env->params, "fingers");
	bool tap_enabled = litest_test_param_get_bool(test_env->params, "tap-enabled");

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    (int)finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);
	if (tap_enabled)
		litest_enable_tap(dev->libinput_device);
	else
		litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	double y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	litest_dispatch(li);

	litest_drain_events_of_type(li,
				    LIBINPUT_EVENT_GESTURE_HOLD_BEGIN,
				    LIBINPUT_EVENT_GESTURE_HOLD_END);

	if (tap_enabled) {
		litest_checkpoint(
			"Expecting no immediate button press as tapping is enabled");
		litest_assert_empty_queue(li);
	} else {
		litest_checkpoint(
			"Expecting immediate button press as tapping is disabled");
		litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	}

	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}

	if (tap_enabled) {
		litest_checkpoint("Expecting late button press as tapping is enabled");
		litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	}
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_timeout_3fg_drag(li);

	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(gestures_3fg_drag_lock_resume_3fg_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	uint32_t finger_count = litest_test_param_get_u32(test_env->params, "fingers");
	bool tap_enabled = litest_test_param_get_bool(test_env->params, "tap-enabled");
	bool wait_for_timeout = litest_test_param_get_bool(test_env->params, "wait");

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    (int)finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);
	if (tap_enabled)
		litest_enable_tap(dev->libinput_device);
	else
		litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_checkpoint("Putting three fingers down + movement)");
	double y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	litest_dispatch(li);

	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_checkpoint("Releasing all fingers");
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Putting three fingers down (no movement)");
	y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_dispatch(li);

	litest_checkpoint("Waiting past finger switch timeout");
	litest_timeout_finger_switch(li);

	if (wait_for_timeout) {
		litest_checkpoint("Waiting past tap/3fg drag timeout");
		litest_timeout_3fg_drag(li);
		litest_assert_empty_queue(li);
	}

	litest_checkpoint("Moving three fingers");
	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_checkpoint("Releasing three fingers");
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_timeout_3fg_drag(li);

	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
}
END_TEST

START_TEST(gestures_3fg_drag_lock_resume_3fg_release_no_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	uint32_t finger_count = litest_test_param_get_u32(test_env->params, "fingers");
	bool tap_enabled = litest_test_param_get_bool(test_env->params, "tap-enabled");
	bool wait_for_timeout = litest_test_param_get_bool(test_env->params, "wait");

	/* tap-enabled for 4fg finger count doesn't make a difference */
	bool expect_tap = finger_count <= 3 && tap_enabled && !wait_for_timeout;

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    (int)finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);

	if (tap_enabled)
		litest_enable_tap(dev->libinput_device);
	else
		litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_checkpoint("Putting 3 fingers down with motion for drag");
	double y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	litest_dispatch(li);
	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Putting 3 fingers down again (no motion)");
	y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Waiting past finger switch timeout");
	litest_timeout_finger_switch(li);

	if (wait_for_timeout) {
		litest_checkpoint("Waiting past tap/3fg drag timeout");
		litest_timeout_3fg_drag(li);
		litest_assert_empty_queue(li);
	}

	litest_checkpoint("Releasing three fingers");
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);
	litest_dispatch(li);

	if (expect_tap) {
		/* If we're not waiting and tapping is enabled, this is
		 * the equivalent of a 3fg tap within the drag timeout */
		litest_checkpoint("Expecting 3fg drag release");
		litest_assert_button_event(li,
					   BTN_LEFT,
					   LIBINPUT_BUTTON_STATE_RELEASED);
		litest_checkpoint("Expecting 3fg tap");
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_PRESSED);
		litest_assert_button_event(li,
					   BTN_MIDDLE,
					   LIBINPUT_BUTTON_STATE_RELEASED);
	}

	litest_assert_empty_queue(li);
	litest_timeout_3fg_drag(li);

	if (!expect_tap)
		litest_assert_button_event(li,
					   BTN_LEFT,
					   LIBINPUT_BUTTON_STATE_RELEASED);

	litest_assert_empty_queue(li);
}
END_TEST

START_TEST(gestures_3fg_drag_lock_resume_1fg_motion)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	uint32_t finger_count = litest_test_param_get_u32(test_env->params, "fingers");
	bool tap_enabled = litest_test_param_get_bool(test_env->params, "tap-enabled");

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    (int)finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);

	if (tap_enabled)
		litest_enable_tap(dev->libinput_device);
	else
		litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_checkpoint("Putting 3 fingers down + motion to trigger 3fg drag");
	double y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_checkpoint("Releasing 3 fingers");
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Putting 1 finger down and moving it");
	/* fingers are up, now let's put one finger down and move it */
	y = 30.0;
	litest_touch_down(dev, 0, 10, y);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	/* We need to wait until the gesture code accepts this is one finger only */
	litest_timeout_finger_switch(li);

	while (y < 60.0) {
		y += 2;
		litest_touch_move(dev, 0, 10, y);
		litest_dispatch(li);
	}

	litest_checkpoint("Expecting drag button release and motion");
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_touch_up(dev, 0);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_timeout_3fg_drag(li);
}
END_TEST

START_TEST(gestures_3fg_drag_lock_resume_2fg_scroll)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;

	uint32_t finger_count = litest_test_param_get_u32(test_env->params, "fingers");
	bool tap_enabled = litest_test_param_get_bool(test_env->params, "tap-enabled");

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    (int)finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);

	if (tap_enabled)
		litest_enable_tap(dev->libinput_device);
	else
		litest_disable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_checkpoint("Putting 3 fingers down + motion to trigger 3fg drag");
	double y = 30.0;
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	while (y < 60.0) {
		y += 2;
		for (uint32_t i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_checkpoint("Releasing 3 fingers");
	for (uint32_t i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Putting 2 fingers down and moving them");
	y = 30.0;
	litest_touch_down(dev, 0, 10, y);
	litest_touch_down(dev, 1, 20, y);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_timeout_finger_switch(li);

	while (y < 60.0) {
		y += 2;
		litest_touch_move(dev, 0, 10, y);
		litest_touch_move(dev, 1, 20, y);
		litest_dispatch(li);
	}

	litest_checkpoint("Expecting drag button release and scroll");
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_only_axis_events(li, LIBINPUT_EVENT_POINTER_SCROLL_FINGER);

	litest_touch_up(dev, 0);
	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_timeout_3fg_drag(li);
}
END_TEST

START_TEST(gestures_3fg_drag_lock_resume_1fg_tap)
{
	struct litest_device *dev = litest_current_device();
	struct libinput *li = dev->libinput;
	int finger_count = litest_test_param_get_i32(test_env->params, "fingers");

	if (litest_slot_count(dev) < 3)
		return LITEST_NOT_APPLICABLE;

	if (libinput_device_config_3fg_drag_get_finger_count(dev->libinput_device) <
	    finger_count)
		return LITEST_NOT_APPLICABLE;

	litest_enable_3fg_drag(dev->libinput_device, finger_count);
	litest_enable_tap(dev->libinput_device);

	litest_drain_events(li);

	litest_checkpoint("Putting 3 fingers down for 3fg drag");
	double y = 30.0;
	for (int i = 0; i < finger_count; i++)
		litest_touch_down(dev, i, 10 + i, y);

	litest_dispatch(li);

	while (y < 60.0) {
		y += 2;
		for (int i = 0; i < finger_count; i++)
			litest_touch_move(dev, i, 10 + i, y);
		litest_dispatch(li);
	}
	litest_drain_events_of_type(li,
				    LIBINPUT_EVENT_GESTURE_HOLD_BEGIN,
				    LIBINPUT_EVENT_GESTURE_HOLD_END,
				    -1);
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_only_typed_events(li, LIBINPUT_EVENT_POINTER_MOTION);

	litest_checkpoint("Releasing 3 fingers");
	for (int i = 0; i < finger_count; i++)
		litest_touch_up(dev, i);

	litest_dispatch(li);
	litest_assert_empty_queue(li);

	litest_checkpoint("Tapping with 1 finger");
	/* fingers are up, now let's tap with one finger */
	y = 30.0;
	litest_touch_down(dev, 0, 10, y);
	litest_dispatch(li);
	litest_assert_empty_queue(li);
	litest_touch_up(dev, 0);
	litest_dispatch(li);

	litest_timeout_tap(li);

	litest_checkpoint("Expecting drag release followed by 1fg tap");

	/* 3fg drag lock must be cancelled */
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
	/* And a 1fg tap */
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_PRESSED);
	litest_assert_button_event(li, BTN_LEFT, LIBINPUT_BUTTON_STATE_RELEASED);
	litest_assert_empty_queue(li);

	litest_timeout_3fg_drag(li);
}
END_TEST

TEST_COLLECTION(gestures)
{
	/* clang-format off */
	litest_add(gestures_cap, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	litest_add(gestures_nocap, LITEST_ANY, LITEST_TOUCHPAD);

	litest_add(gestures_swipe_3fg_btntool_pinch_like, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);

	litest_add(gestures_3fg_buttonarea_scroll, LITEST_CLICKPAD, LITEST_SINGLE_TOUCH);
	litest_add(gestures_3fg_buttonarea_scroll_btntool, LITEST_CLICKPAD, LITEST_SINGLE_TOUCH);

	litest_add(gestures_time_usec, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);

	litest_add(gestures_hold_config_default_disabled, LITEST_TOUCHPAD|LITEST_SEMI_MT, LITEST_ANY);
	litest_add(gestures_hold_config_default_enabled, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH|LITEST_SEMI_MT);
	litest_add(gestures_hold_config_set_invalid, LITEST_TOUCHPAD, LITEST_ANY);
	litest_add(gestures_hold_config_is_available, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH|LITEST_SEMI_MT);
	litest_add(gestures_hold_config_is_not_available, LITEST_TOUCHPAD|LITEST_SEMI_MT, LITEST_ANY);

	litest_with_parameters(params, "fingers", 'i', 4, 1, 2, 3, 4) {
		litest_add_parametrized(gestures_hold, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_tap_enabled, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_cancel, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_cancel_tap_enabled, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	litest_with_parameters(params, "direction", 'I', 8, litest_named_i32(N), litest_named_i32(NE),
							    litest_named_i32(E), litest_named_i32(SE),
							    litest_named_i32(S), litest_named_i32(SW),
							    litest_named_i32(W), litest_named_i32(NW)) {
		litest_add_parametrized(gestures_swipe_3fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_swipe_3fg_btntool, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_swipe_4fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_swipe_4fg_btntool, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_pinch, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_pinch_3fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_pinch_4fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_spread, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);

		litest_add_parametrized(gestures_hold_then_swipe_3fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_then_swipe_4fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_then_pinch_2fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_then_pinch_3fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_then_pinch_4fg, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_hold_then_spread, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	litest_add(gestures_hold_then_3fg_buttonarea_scroll, LITEST_CLICKPAD, LITEST_SINGLE_TOUCH);

	litest_add(gestures_hold_once_on_double_tap, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	litest_with_parameters(params, "fingers", 'i', 3, 1, 2, 3) {
		litest_add_parametrized(gestures_hold_once_tap_n_drag, LITEST_TOUCHPAD, LITEST_ANY, params);
	}

	litest_add(gestures_hold_and_motion_before_timeout, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	litest_add(gestures_hold_and_motion_after_timeout, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);

	litest_with_parameters(params,
			       "fingers", 'u', 2, 3, 4,
			       "tap-enabled", 'b') {
		litest_add_parametrized(gestures_3fg_drag, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	litest_with_parameters(params,
			       "fingers", 'u', 2, 3, 4,
			       "tap-enabled", 'b',
			       "wait", 'b') {
		litest_add_parametrized(gestures_3fg_drag_lock_resume_3fg_motion, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_3fg_drag_lock_resume_3fg_release_no_motion, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	litest_with_parameters(params,
			       "fingers", 'u', 2, 3, 4,
			       "tap-enabled", 'b') {
		litest_add_parametrized(gestures_3fg_drag_lock_resume_1fg_motion, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
		litest_add_parametrized(gestures_3fg_drag_lock_resume_2fg_scroll, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	litest_with_parameters(params, "fingers", 'i', 2, 3, 4) {
		litest_add_parametrized(gestures_3fg_drag_lock_resume_1fg_tap, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH, params);
	}

	/* Timing-sensitive test, valgrind is too slow */
	if (!RUNNING_ON_VALGRIND)
		litest_add(gestures_swipe_3fg_unaccel, LITEST_TOUCHPAD, LITEST_SINGLE_TOUCH);
	/* clang-format on */
}
