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

#pragma once

#include "config.h"

#include <libevdev/libevdev.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations instead of #includes to make
 * this header self-contained (bindgen, etc.) */
struct evdev_frame;
struct libinput;
struct libinput_device;
struct libinput_tablet_tool;
struct libinput_plugin;
enum libinput_log_priority;

#define plugin_log_debug(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_DEBUG, __VA_ARGS__)
#define plugin_log_info(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_INFO, __VA_ARGS__)
#define plugin_log_error(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_ERROR, __VA_ARGS__)
#define plugin_log_bug_kernel(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_ERROR, "kernel bug: " __VA_ARGS__)
#define plugin_log_bug_libinput(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_ERROR, "libinput bug: " __VA_ARGS__)
#define plugin_log_bug_client(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_ERROR, "client bug: " __VA_ARGS__)
#define plugin_log_bug(p_, ...) plugin_log_msg((p_), LIBINPUT_LOG_PRIORITY_ERROR, "plugin bug: " __VA_ARGS__)

void
plugin_log_msg(struct libinput_plugin *plugin,
	       enum libinput_log_priority priority,
	       const char *format,
	       ...);

struct libinput_plugin_interface {
	void (*run)(struct libinput_plugin *plugin);
	/**
	 * Notification that the plugin is about to be destroyed.
	 * When this function is called, the plugin has already
	 * been unregistered. The plugin should free any
	 * resources allocated but not the struct libinput_plugin
	 * itself.
	 */
	void (*destroy)(struct libinput_plugin *plugin);
	/**
	 * Notification about a newly added device that has **not** yet
	 * been added by libinput as struct libinput_device.
	 */
	void (*device_new)(struct libinput_plugin *plugin,
			   struct libinput_device *device,
			   struct libevdev *evdev,
			   struct udev_device *udev_device);
	/**
	 * Notification that a device (previously announced with device_new)
	 * was ignored by libinput and was **never** added as struct
	 * libinput_device.
	 *
	 * If a device was added (device_added) then this callback will
	 * not be called for that device.
	 */
	void (*device_ignored)(struct libinput_plugin *plugin,
			       struct libinput_device *device);
	/**
	 * Notification that a device was added to libinput. Called
	 * after the device_new callback if the device matches libinput's
	 * expectations.
	 */
	void (*device_added)(struct libinput_plugin *plugin,
			     struct libinput_device *device);
	/**
	 * Notification that a previously added device was removed.
	 */
	void (*device_removed)(struct libinput_plugin *plugin,
			       struct libinput_device *device);
	/**
	 * Notification that a device submitted a frame event.
	 */
	void (*evdev_frame)(struct libinput_plugin *plugin,
			    struct libinput_device *device,
			    struct evdev_frame *frame);

	/**
	 * Notification that a configuration option on a tool
	 * was modified.
	 */
	void (*tool_configured)(struct libinput_plugin *plugin,
				struct libinput_tablet_tool *tool);
};

/**
 * Returns a new plugin with the given interface and, optionally,
 * the user data. The returned plugin has a refcount of at least 1
 * and must be unref'd by the caller.
 * Should an error occur, the plugin must be unregistered by
 * the caller:
 *
 * ```
 * struct libinput_plugin *plugin = libinput_plugin_new(libinput, ...);
 * if (some_error_condition) {
 *     libinput_plugin_unregister(plugin);
 * }
 * libinput_plugin_unref(plugin);
 * ```
 */
struct libinput_plugin *
libinput_plugin_new(struct libinput *libinput,
		    const char *name,
		    const struct libinput_plugin_interface *interface,
		    void *user_data);

const char *
libinput_plugin_get_name(struct libinput_plugin *plugin);

struct libinput *
libinput_plugin_get_context(struct libinput_plugin *plugin);

void
libinput_plugin_unregister(struct libinput_plugin *plugin);

void
libinput_plugin_set_user_data(struct libinput_plugin *plugin, void *user_data);
void *
libinput_plugin_get_user_data(struct libinput_plugin *plugin);

struct libinput_plugin *
libinput_plugin_ref(struct libinput_plugin *plugin);

struct libinput_plugin *
libinput_plugin_unref(struct libinput_plugin *plugin);

#ifdef DEFINE_UNREF_CLEANUP_FUNC
DEFINE_UNREF_CLEANUP_FUNC(libinput_plugin);
#endif

void
libinput_plugin_enable_device_event_frame(struct libinput_plugin *plugin,
					  struct libinput_device *device,
					  bool enable);

/**
 * Inject a new event frame from the given plugin. This
 * frame is treated as if it was just sent by the kernel's
 * event node and is processed immediately, interrupting
 * any other processing from this device.
 *
 * This function can be called any time but unlike
 * libinput_plugin_append_evdev_frame() and
 * libinput_plugin_prepend_evdev_frame() it starts
 * processing the frame at the bottom of the plugin
 * stack.
 *
 * The injected event will also be sent to
 * the current plugin, a guard needs to be in
 * place to prevent recursion.
 *
 * Injecting events may cause other plugins to
 * behave unexpectedly (see below). In almost all cases
 * it is better to use libinput_plugin_append_evdev_frame()
 * or libinput_plugin_prepend_evdev_frame() to only
 * affect the plugins logically *after* this
 * plugin.
 *
 * Assuming several plugins P1, P2, and P3 and
 * an event frame E, and P2 calling this function
 * with an injected event I:
 * - P1: receives E, optionally modifies E
 * - P2: receives E, injects I
 *   - P1: receives I, optionally modifies I
 *   - P2: receives I, optionally modifies I
 *   - P3: receives I, optionally modifies I
 * - P2: continues processing E, optionally modifies E
 * - P3: receives E, optionally modifies E
 *
 * For P1 the injected event I is received after E,
 * for P3 the injected event I is received before E.
 *
 * An example for event injection being harmful:
 * A plugin may monitor tablet proximity state and prepent
 * proximity-in events if the tablet does not send proximity-in
 * events. This plugin stops monitoring events once it sees correct
 * proximity-in events.
 * If another plugin were to inject a proximity event (e.g. to fake a
 * different tool coming into proximity), the plugin would stop
 * monitoring. Future proximity events from the tablet will then
 * have the wrong proximity.
 * This can be avoided by appending or prepending the events instead
 * of injecting them.
 */
void
libinput_plugin_inject_evdev_frame(struct libinput_plugin *libinput,
				   struct libinput_device *device,
				   struct evdev_frame *frame);

/**
 * Queue an event frame for the next plugin in sequence, after
 * the current event frame being processed.
 *
 * This function can only be called from within the evdev_frame()
 * callback or within a plugin's timer func and may be used to
 * queue new frames. If called multiple times with frames F1, F2, ...
 * while processing the current frame C, frame sequence to be passed
 * to the next plugin is C, F1, F2, ...
 *
 * Assuming several plugins P1, P2, P3, and P4 and an event frame E,
 * and P2 and P3 calling this function with an queued event frame Q1 and Q2,
 * respectively.
 *
 * - P1: receives E, optionally modifies E
 * - P2: receives E, appends Q1, optionally modifies E
 * - P3: receives E, appends Q2, optionally modifies E
 * - P3: receives Q1, optionally modifies Q1
 * - P4: receives E, optionally modifies E
 * - P4: receives Q2, optionally modifies Q2
 * - P4: receives Q1, optionally modifies Q1
 *
 * Once plugin processing is complete, the event sequence passed
 * back to libinput is thus [E, Q2, Q1].
 *
 * To discard the original event frame, the plugin needs to
 * call evdev_frame_reset() on the frame passed to it.
 *
 * It is a plugin bug to call this function from outside the
 * evdev_frame() callback or a timer callback.
 *
 * If called within a plugin's timer callback, any frames generated by
 * the plugin will only be seen by plugins after this plugin. These
 * frames will be processed in the usual evdev_fame() callback and there
 * is no indication that the events were queued from within a timer
 * callback. Using the above example:
 *
 * - P1: <idle>
 * - P2: timer callback invoked, appends Q1
 * - P3: receives Q1, appends Q2, optionally modifies Q1
 * - P4: receives Q2, optionally modifies Q2
 * - P4: receives Q1, optionally modifies Q1
 *
 * Because there is no current frame during a timer callback
 * libinput_plugin_append_evdev_frame() and
 * libinput_plugin_prepend_evdev_frame() are functionally equivalent.
 * If both functions are used, all events from
 * libinput_plugin_prepend_evdev_frame() will be queued before
 * events from libinput_plugin_append_evdev_frame().
 */
void
libinput_plugin_append_evdev_frame(struct libinput_plugin *libinput,
				   struct libinput_device *device,
				   struct evdev_frame *frame);

/**
 * Identical to libinput_plugin_append_evdev_frame(), but prepends
 * the event frame to the current event frame being processed.
 * If called multiple times with frames F1, F2, ... while processing
 * the current frame C, frame sequence to be passed to the next
 * plugin is F1, F2, ..., C
 */
void
libinput_plugin_prepend_evdev_frame(struct libinput_plugin *libinput,
				    struct libinput_device *device,
				    struct evdev_frame *frame);

/**
 * Create a new timer for the given plugin.
 *
 * The timer needs to be set with libinput_plugin_timer_set()
 * before it can be used.
 *
 * The refcount of the returned timer is at least 1, and the caller
 * must call libinput_plugin_timer_unref() to release it.
 *
 * The func given is the callback invoked when the timer expires.
 * It is passed the user_data given here or in a subsequent
 * call to libinput_plugin_timer_set_user_data().
 *
 * Note that event generating inside a timer is subject to
 * specific behaviors, see the documentation
 * to libinput_plugin_append_evdev_frame(), libinput_plugin_prepend_evdev_frame()
 * and libinput_plugin_inject_evdev_frame() for details.
 */
struct libinput_plugin_timer *
libinput_plugin_timer_new(struct libinput_plugin *plugin,
			  const char *name,
			  void (*func)(struct libinput_plugin *plugin,
				       uint64_t now,
				       void *user_data),
			  void *user_data);

struct libinput_plugin_timer *
libinput_plugin_timer_ref(struct libinput_plugin_timer *timer);

struct libinput_plugin_timer *
libinput_plugin_timer_unref(struct libinput_plugin_timer *timer);

#ifdef DEFINE_UNREF_CLEANUP_FUNC
DEFINE_UNREF_CLEANUP_FUNC(libinput_plugin_timer);
#endif

/* Set timer expire time, in absolute us CLOCK_MONOTONIC */
void
libinput_plugin_timer_set(struct libinput_plugin_timer *timer, uint64_t expire);

void
libinput_plugin_timer_set_user_data(struct libinput_plugin_timer *timer,
				    void *user_data);
void *
libinput_plugin_timer_get_user_data(struct libinput_plugin_timer *timer);

void
libinput_plugin_timer_cancel(struct libinput_plugin_timer *timer);
