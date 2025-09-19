/*
 * Copyright © 2017-2025 Red Hat, Inc.
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
#include <libevdev/libevdev.h>

#include "util-mem.h"
#include "util-strings.h"

#include "evdev-frame.h"
#include "libinput-log.h"
#include "libinput-plugin-button-debounce.h"
#include "libinput-plugin.h"
#include "libinput-util.h"
#include "quirks.h"
#include "timer.h"

/* Debounce cases to handle
     P ... button press
     R ... button release
     ---|  timeout duration

     'normal' .... event sent when it happens
     'filtered' .. event is not sent (but may be sent later)
     'delayed' ... event is sent with wall-clock delay

   1) P---| R		P normal, R normal
   2) R---| P		R normal, P normal
   3) P---R--| P	P normal, R filtered, delayed, P normal
   4) R---P--| R	R normal, P filtered, delayed, R normal
   4.1) P---| R--P--|	P normal, R filtered
   5) P--R-P-| R	P normal, R filtered, P filtered, R normal
   6) R--P-R-| P	R normal, P filtered, R filtered, P normal
   7) P--R--|
	  ---P-|	P normal, R filtered, P filtered
   8) R--P--|
	  ---R-|	R normal, P filtered, R filtered

   1, 2 are the normal click cases without debouncing taking effect
   3, 4 are fast clicks where the second event is delivered with a delay
   5, 6 are contact bounces, fast
   7, 8 are contact bounces, slow

   4.1 is a special case with the same event sequence as 4 but we want to
   filter the *release* event out, it's a button losing contact while being
   held down.

   7 and 8 are cases where the first event happens within the first timeout
   but the second event is outside that timeout (but within the timeout of
   the second event). These cases are handled by restarting the timer on every
   event that could be part of a bouncing sequence, which makes these cases
   indistinguishable from 5 and 6.
*/

enum debounce_event {
	DEBOUNCE_EVENT_PRESS = 50,
	DEBOUNCE_EVENT_RELEASE,
	DEBOUNCE_EVENT_TIMEOUT,
	DEBOUNCE_EVENT_TIMEOUT_SHORT,
	DEBOUNCE_EVENT_OTHERBUTTON,
};

enum debounce_state {
	DEBOUNCE_STATE_IS_UP = 100,
	DEBOUNCE_STATE_IS_DOWN,
	DEBOUNCE_STATE_IS_DOWN_WAITING,
	DEBOUNCE_STATE_IS_UP_DELAYING,
	DEBOUNCE_STATE_IS_UP_DELAYING_SPURIOUS,
	DEBOUNCE_STATE_IS_UP_DETECTING_SPURIOUS,
	DEBOUNCE_STATE_IS_DOWN_DETECTING_SPURIOUS,
	DEBOUNCE_STATE_IS_UP_WAITING,
	DEBOUNCE_STATE_IS_DOWN_DELAYING,

	DEBOUNCE_STATE_DISABLED = 999,
};

static inline const char *
debounce_state_to_str(enum debounce_state state)
{
	switch (state) {
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_UP);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_DOWN);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_DOWN_WAITING);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_UP_DELAYING);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_UP_DELAYING_SPURIOUS);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_UP_DETECTING_SPURIOUS);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_DOWN_DETECTING_SPURIOUS);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_UP_WAITING);
	CASE_RETURN_STRING(DEBOUNCE_STATE_IS_DOWN_DELAYING);
	CASE_RETURN_STRING(DEBOUNCE_STATE_DISABLED);
	}

	return NULL;
}

static inline const char *
debounce_event_to_str(enum debounce_event event)
{
	switch (event) {
	CASE_RETURN_STRING(DEBOUNCE_EVENT_PRESS);
	CASE_RETURN_STRING(DEBOUNCE_EVENT_RELEASE);
	CASE_RETURN_STRING(DEBOUNCE_EVENT_TIMEOUT);
	CASE_RETURN_STRING(DEBOUNCE_EVENT_TIMEOUT_SHORT);
	CASE_RETURN_STRING(DEBOUNCE_EVENT_OTHERBUTTON);
	}
	return NULL;
}

struct plugin_device {
	struct list link;
	struct libinput_device *device;
	struct plugin_data *parent;

	evdev_usage_t button_usage;
	uint64_t button_time;
	enum debounce_state state;
	bool spurious_enabled;

	struct libinput_plugin_timer *timer;
	struct libinput_plugin_timer *timer_short;
};

static void
plugin_device_destroy(void *d)
{
	struct plugin_device *device = d;

	list_remove(&device->link);
	libinput_plugin_timer_cancel(device->timer);
	libinput_plugin_timer_unref(device->timer);
	libinput_plugin_timer_cancel(device->timer_short);
	libinput_plugin_timer_unref(device->timer_short);
	libinput_device_unref(device->device);

	free(device);
}

struct plugin_data {
	struct list devices;
	struct libinput_plugin *plugin;
};

static void
plugin_data_destroy(void *d)
{
	struct plugin_data *data = d;

	struct plugin_device *device;
	list_for_each_safe(device, &data->devices, link) {
		plugin_device_destroy(device);
	}

	free(data);
}

DEFINE_DESTROY_CLEANUP_FUNC(plugin_data);
static void
plugin_destroy(struct libinput_plugin *libinput_plugin)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	plugin_data_destroy(plugin);
}

static inline void
log_debounce_bug(struct plugin_device *device, enum debounce_event event)
{
	plugin_log_bug_libinput(device->parent->plugin,
				"invalid debounce event %s in state %s\n",
				debounce_event_to_str(event),
				debounce_state_to_str(device->state));
}

static inline void
debounce_set_state(struct plugin_device *device, enum debounce_state new_state)
{
	assert(new_state >= DEBOUNCE_STATE_IS_UP &&
	       new_state <= DEBOUNCE_STATE_IS_DOWN_DELAYING);

	device->state = new_state;
}

static inline void
debounce_set_timer(struct plugin_device *device, uint64_t time)
{
	const int DEBOUNCE_TIMEOUT_BOUNCE = ms2us(25);

	libinput_plugin_timer_set(device->timer, time + DEBOUNCE_TIMEOUT_BOUNCE);
}

static inline void
debounce_set_timer_short(struct plugin_device *device, uint64_t time)
{
	const int DEBOUNCE_TIMEOUT_SPURIOUS = ms2us(12);

	libinput_plugin_timer_set(device->timer_short,
				  time + DEBOUNCE_TIMEOUT_SPURIOUS);
}

static inline void
debounce_cancel_timer(struct plugin_device *device)
{
	libinput_plugin_timer_cancel(device->timer);
}

static inline void
debounce_cancel_timer_short(struct plugin_device *device)
{
	libinput_plugin_timer_cancel(device->timer_short);
}

static inline void
debounce_enable_spurious(struct plugin_device *device)
{
	if (device->spurious_enabled)
		plugin_log_bug(device->parent->plugin,
			       "tried to enable spurious debouncing twice\n");

	device->spurious_enabled = true;
	plugin_log_info(device->parent->plugin,
			"%s: enabling spurious button debouncing, "
			"see %s/button-debouncing.html for details\n",
			libinput_device_get_name(device->device),
			HTTP_DOC_LINK);
}

static void
debounce_notify_button(struct plugin_device *device,
		       struct evdev_frame *frame,
		       enum libinput_button_state state)
{
	_unref_(evdev_frame) *button_frame = NULL;
	if (frame == NULL) {
		button_frame = evdev_frame_new(2);
		frame = button_frame;
	}

	evdev_frame_append_one(frame,
			       device->button_usage,
			       state == LIBINPUT_BUTTON_STATE_PRESSED ? 1 : 0);
	evdev_frame_set_time(frame, device->button_time);

	libinput_plugin_prepend_evdev_frame(device->parent->plugin,
					    device->device,
					    frame);
}

static void
debounce_is_up_handle_event(struct plugin_device *device,
			    enum debounce_event event,
			    struct evdev_frame *frame,
			    uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		device->button_time = time;
		debounce_set_timer(device, time);
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN_WAITING);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_PRESSED);
		break;
	case DEBOUNCE_EVENT_RELEASE:
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		break;
	}
}

static void
debounce_is_down_handle_event(struct plugin_device *device,
			      enum debounce_event event,
			      struct evdev_frame *frame,
			      uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		/* If we lost the kernel button release event (e.g. something
		 * grabbed the device for a short while) we quietly ignore
		 * the next down event */
		break;
	case DEBOUNCE_EVENT_RELEASE:
		device->button_time = time;
		debounce_set_timer(device, time);
		debounce_set_timer_short(device, time);
		if (device->spurious_enabled) {
			debounce_set_state(device,
					   DEBOUNCE_STATE_IS_UP_DELAYING_SPURIOUS);
		} else {
			debounce_set_state(device,
					   DEBOUNCE_STATE_IS_UP_DETECTING_SPURIOUS);
			debounce_notify_button(device,
					       frame,
					       LIBINPUT_BUTTON_STATE_RELEASED);
		}
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		break;
	}
}

static void
debounce_is_down_waiting_handle_event(struct plugin_device *device,
				      enum debounce_event event,
				      struct evdev_frame *frame,
				      uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_RELEASE:
		debounce_set_timer(device, time);
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP_DELAYING);
		/* Note: In the debouncing RPR case, we use the last
		 * release's time stamp */
		device->button_time = time;
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		break;
	}
}

static void
debounce_is_up_delaying_handle_event(struct plugin_device *device,
				     enum debounce_event event,
				     struct evdev_frame *frame,
				     uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		debounce_set_timer(device, time);
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN_WAITING);
		break;
	case DEBOUNCE_EVENT_RELEASE:
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	}
}

static void
debounce_is_up_delaying_spurious_handle_event(struct plugin_device *device,
					      enum debounce_event event,
					      struct evdev_frame *frame,
					      uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		debounce_cancel_timer(device);
		debounce_cancel_timer_short(device);
		break;
	case DEBOUNCE_EVENT_RELEASE:
	case DEBOUNCE_EVENT_TIMEOUT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP_WAITING);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	}
}

static void
debounce_is_up_detecting_spurious_handle_event(struct plugin_device *device,
					       enum debounce_event event,
					       struct evdev_frame *frame,
					       uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		debounce_set_timer(device, time);
		debounce_set_timer_short(device, time);
		/* Note: in a bouncing PRP case, we use the last press
		 * event time */
		device->button_time = time;
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN_DETECTING_SPURIOUS);
		break;
	case DEBOUNCE_EVENT_RELEASE:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP_WAITING);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP);
		break;
	}
}

static void
debounce_is_down_detecting_spurious_handle_event(struct plugin_device *device,
						 enum debounce_event event,
						 struct evdev_frame *frame,
						 uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_RELEASE:
		debounce_set_timer(device, time);
		debounce_set_timer_short(device, time);
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP_DETECTING_SPURIOUS);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		debounce_cancel_timer(device);
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		debounce_enable_spurious(device);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_PRESSED);
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_PRESSED);
		break;
	}
}

static void
debounce_is_up_waiting_handle_event(struct plugin_device *device,
				    enum debounce_event event,
				    struct evdev_frame *frame,
				    uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		debounce_set_timer(device, time);
		/* Note: in a debouncing PRP case, we use the last press'
		 * time */
		device->button_time = time;
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN_DELAYING);
		break;
	case DEBOUNCE_EVENT_RELEASE:
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP);
		break;
	}
}

static void
debounce_is_down_delaying_handle_event(struct plugin_device *device,
				       enum debounce_event event,
				       struct evdev_frame *frame,
				       uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_RELEASE:
		debounce_set_timer(device, time);
		debounce_set_state(device, DEBOUNCE_STATE_IS_UP_WAITING);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_TIMEOUT:
	case DEBOUNCE_EVENT_OTHERBUTTON:
		debounce_set_state(device, DEBOUNCE_STATE_IS_DOWN);
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_PRESSED);
		break;
	}
}

static void
debounce_disabled_handle_event(struct plugin_device *device,
			       enum debounce_event event,
			       struct evdev_frame *frame,
			       uint64_t time)
{
	switch (event) {
	case DEBOUNCE_EVENT_PRESS:
		device->button_time = time;
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_PRESSED);
		break;
	case DEBOUNCE_EVENT_RELEASE:
		device->button_time = time;
		debounce_notify_button(device, frame, LIBINPUT_BUTTON_STATE_RELEASED);
		break;
	case DEBOUNCE_EVENT_TIMEOUT_SHORT:
	case DEBOUNCE_EVENT_TIMEOUT:
		log_debounce_bug(device, event);
		break;
	case DEBOUNCE_EVENT_OTHERBUTTON:
		break;
	}
}

static void
debounce_handle_event(struct plugin_device *device,
		      enum debounce_event event,
		      struct evdev_frame *frame,
		      uint64_t time)
{
	enum debounce_state current = device->state;

	if (event == DEBOUNCE_EVENT_OTHERBUTTON) {
		debounce_cancel_timer(device);
		debounce_cancel_timer_short(device);
	}

	switch (current) {
	case DEBOUNCE_STATE_IS_UP:
		debounce_is_up_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_IS_DOWN:
		debounce_is_down_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_IS_DOWN_WAITING:
		debounce_is_down_waiting_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_IS_UP_DELAYING:
		debounce_is_up_delaying_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_IS_UP_DELAYING_SPURIOUS:
		debounce_is_up_delaying_spurious_handle_event(device,
							      event,
							      frame,
							      time);
		break;
	case DEBOUNCE_STATE_IS_UP_DETECTING_SPURIOUS:
		debounce_is_up_detecting_spurious_handle_event(device,
							       event,
							       frame,
							       time);
		break;
	case DEBOUNCE_STATE_IS_DOWN_DETECTING_SPURIOUS:
		debounce_is_down_detecting_spurious_handle_event(device,
								 event,
								 frame,
								 time);
		break;
	case DEBOUNCE_STATE_IS_UP_WAITING:
		debounce_is_up_waiting_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_IS_DOWN_DELAYING:
		debounce_is_down_delaying_handle_event(device, event, frame, time);
		break;
	case DEBOUNCE_STATE_DISABLED:
		debounce_disabled_handle_event(device, event, frame, time);
		break;
	}

	plugin_log_debug(device->parent->plugin,
			 "debounce state: %s → %s → %s\n",
			 debounce_state_to_str(current),
			 debounce_event_to_str(event),
			 debounce_state_to_str(device->state));
}

static void
debounce_plugin_handle_frame(struct plugin_device *device,
			     struct evdev_frame *frame,
			     uint64_t time)
{
	size_t nchanged = 0;
	bool flushed = false;

	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	/* Strip out all button events from this frame (if any). Then
	 * append the button events to that stripped frame according
	 * to our state machine.
	 *
	 * We allow for a max of 16 buttons to be appended, if you press more
	 * than 16 buttons within the same frame good luck to you.
	 */
	_unref_(evdev_frame) *filtered_frame = evdev_frame_new(nevents + 16);
	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *e = &events[i];
		if (!evdev_usage_is_button(e->usage)) {
			evdev_frame_append(filtered_frame, e, 1);
			continue;
		}

		nchanged++;

		/* If we have more than one button this frame or a different button,
		 * flush the state machine with otherbutton */
		if (!flushed &&
		    (nchanged > 1 ||
		     evdev_usage_cmp(e->usage, device->button_usage) != 0)) {
			debounce_handle_event(device,
					      DEBOUNCE_EVENT_OTHERBUTTON,
					      NULL,
					      time);
			flushed = true;
		}
	}

	if (nchanged == 0)
		return;

	/* The state machine has some pre-conditions:
	 * - the IS_DOWN and IS_UP states are neutral entry states without
	 *   any timeouts
	 * - a OTHERBUTTON event always flushes the state to IS_DOWN or
	 *   IS_UP
	 */
	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event *e = &events[i];
		bool is_down = !!e->value;

		if (!evdev_usage_is_button(e->usage))
			continue;

		if (flushed && device->state != DEBOUNCE_STATE_DISABLED) {
			debounce_set_state(device,
					   !is_down ? DEBOUNCE_STATE_IS_DOWN
						    : DEBOUNCE_STATE_IS_UP);
			flushed = false;
		}

		device->button_usage = e->usage;
		debounce_handle_event(device,
				      is_down ? DEBOUNCE_EVENT_PRESS
					      : DEBOUNCE_EVENT_RELEASE,
				      filtered_frame,
				      time);

		/* if we have more than one event, we flush the state
		 * machine immediately after the event itself */
		if (nchanged > 1) {
			debounce_handle_event(device,
					      DEBOUNCE_EVENT_OTHERBUTTON,
					      filtered_frame,
					      time);
			flushed = true;
		}
	}

	evdev_frame_set(frame,
			evdev_frame_get_events(filtered_frame, NULL),
			evdev_frame_get_count(filtered_frame));
}

static void
debounce_plugin_evdev_frame(struct libinput_plugin *libinput_plugin,
			    struct libinput_device *device,
			    struct evdev_frame *frame)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd;

	list_for_each(pd, &plugin->devices, link) {
		if (pd->device == device) {
			debounce_plugin_handle_frame(pd, frame, frame->time);
			break;
		}
	}
}

static void
debounce_timeout(struct libinput_plugin *plugin, uint64_t now, void *data)
{
	struct plugin_device *device = data;

	debounce_handle_event(device, DEBOUNCE_EVENT_TIMEOUT, NULL, now);
}

static void
debounce_timeout_short(struct libinput_plugin *plugin, uint64_t now, void *data)
{
	struct plugin_device *device = data;

	debounce_handle_event(device, DEBOUNCE_EVENT_TIMEOUT_SHORT, NULL, now);
}

static void
debounce_plugin_device_added(struct libinput_plugin *libinput_plugin,
			     struct libinput_device *device)
{
	if (!libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER))
		return;

	_unref_(udev_device) *udev_device = libinput_device_get_udev_device(device);
	if (udev_device) {
		const char *prop = udev_device_get_property_value(udev_device,
								  "ID_INPUT_TOUCHPAD");
		bool val;
		if (parse_boolean_property(prop, &val) && val) {
			return;
		}
	}

	_unref_(quirks) *q = libinput_device_get_quirks(device);
	bool result = false;
	if (q && quirks_get_bool(q, QUIRK_MODEL_BOUNCING_KEYS, &result) && result) {
		return;
	}

	libinput_plugin_enable_device_event_frame(libinput_plugin, device, true);

	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *pd = zalloc(sizeof(*pd));
	pd->device = libinput_device_ref(device);
	pd->parent = plugin;
	pd->state = DEBOUNCE_STATE_IS_UP;

	_autofree_ char *timer1_name =
		strdup_printf("debounce-%s", libinput_device_get_sysname(device));
	_autofree_ char *timer2_name =
		strdup_printf("debounce-short-%s", libinput_device_get_sysname(device));
	pd->timer = libinput_plugin_timer_new(libinput_plugin,
					      timer1_name,
					      debounce_timeout,
					      pd);
	pd->timer_short = libinput_plugin_timer_new(libinput_plugin,
						    timer2_name,
						    debounce_timeout_short,
						    pd);

	list_take_append(&plugin->devices, pd, link);
}

static void
debounce_plugin_device_removed(struct libinput_plugin *libinput_plugin,
			       struct libinput_device *device)
{
	struct plugin_data *plugin = libinput_plugin_get_user_data(libinput_plugin);
	struct plugin_device *dev;
	list_for_each_safe(dev, &plugin->devices, link) {
		if (dev->device == device) {
			plugin_device_destroy(dev);
			return;
		}
	}
}

static const struct libinput_plugin_interface interface = {
	.run = NULL,
	.destroy = plugin_destroy,
	.device_new = NULL,
	.device_ignored = NULL,
	.device_added = debounce_plugin_device_added,
	.device_removed = debounce_plugin_device_removed,
	.evdev_frame = debounce_plugin_evdev_frame,
};

void
libinput_debounce_plugin(struct libinput *libinput)
{
	struct plugin_data *plugin = zalloc(sizeof(*plugin));
	list_init(&plugin->devices);

	_unref_(libinput_plugin) *p =
		libinput_plugin_new(libinput, "button-debounce", &interface, plugin);
	plugin->plugin = p;
}
