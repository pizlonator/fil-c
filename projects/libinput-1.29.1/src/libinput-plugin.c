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

#include <stdbool.h>

#include "util-files.h"
#include "util-list.h"

#include "evdev-frame.h"
#include "evdev-plugin.h"
#include "libinput-plugin-button-debounce.h"
#include "libinput-plugin-mouse-wheel-lowres.h"
#include "libinput-plugin-mouse-wheel.h"
#include "libinput-plugin-mtdev.h"
#include "libinput-plugin-private.h"
#include "libinput-plugin-tablet-double-tool.h"
#include "libinput-plugin-tablet-eraser-button.h"
#include "libinput-plugin-tablet-forced-tool.h"
#include "libinput-plugin-tablet-proximity-timer.h"
#include "libinput-plugin.h"
#include "libinput-private.h"
#include "libinput-util.h"
#include "timer.h"

struct libinput_plugin {
	struct libinput *libinput;
	size_t index; /* sequential index of all plugins */
	char *name;
	int refcount;
	struct list link;
	void *user_data;

	bool registered;

	const struct libinput_plugin_interface *interface;

	struct list timers;

	struct {
		struct list *after;
		struct list *before;
	} event_queue;
};

struct libinput_plugin_timer {
	int refcount;
	struct list link;
	struct libinput_plugin *plugin;
	struct libinput_timer timer;
	void (*func)(struct libinput_plugin *plugin, uint64_t now, void *user_data);
	void *user_data;
};

LIBINPUT_ATTRIBUTE_PRINTF(3, 4)
void
plugin_log_msg(struct libinput_plugin *plugin,
	       enum libinput_log_priority priority,
	       const char *format,
	       ...)
{

	if (!log_is_logged(plugin->libinput, priority))
		return;

	_autofree_ char *prefix = strdup_printf("Plugin:%-15s - ", plugin->name);
	va_list args;
	va_start(args, format);
	_autofree_ char *message = strdup_vprintf(format, args);
	va_end(args);

	log_msg(plugin->libinput, priority, "%s%s", prefix, message);
}

struct libinput_plugin *
libinput_plugin_new(struct libinput *libinput,
		    const char *name,
		    const struct libinput_plugin_interface *interface,
		    void *user_data)
{
	struct libinput_plugin *plugin = zalloc(sizeof(*plugin));
	plugin->index = libinput->plugin_system.next_plugin_index++;
	plugin->registered = true;
	plugin->libinput = libinput;
	plugin->refcount = 1;
	plugin->interface = interface;
	plugin->user_data = user_data;
	plugin->name = strdup(name);
	list_init(&plugin->timers);

	if (plugin->index >= 32) {
		log_bug_libinput(libinput, "Too many plugins, maximum is 32\n");
	}

	libinput_plugin_system_register_plugin(&libinput->plugin_system, plugin);

	return plugin;
}

void
libinput_plugin_unregister(struct libinput_plugin *plugin)
{
	struct libinput *libinput = plugin->libinput;
	if (!plugin->registered)
		return;

	plugin->registered = false;

	libinput_plugin_system_unregister_plugin(&libinput->plugin_system, plugin);
}

struct libinput_plugin *
libinput_plugin_ref(struct libinput_plugin *plugin)
{
	assert(plugin->refcount > 0);
	++plugin->refcount;
	return plugin;
}

struct libinput_plugin *
libinput_plugin_unref(struct libinput_plugin *plugin)
{
	assert(plugin->refcount > 0);
	if (--plugin->refcount == 0) {
		struct libinput_plugin_timer *timer;
		list_for_each_safe(timer, &plugin->timers, link) {
			libinput_plugin_timer_cancel(timer);
			libinput_plugin_timer_unref(timer);
		}

		list_remove(&plugin->link);
		if (plugin->interface->destroy)
			plugin->interface->destroy(plugin);
		free(plugin->name);
		free(plugin);
	}
	return NULL;
}

void
libinput_plugin_set_user_data(struct libinput_plugin *plugin, void *user_data)
{
	plugin->user_data = user_data;
}

void *
libinput_plugin_get_user_data(struct libinput_plugin *plugin)
{
	return plugin->user_data;
}

const char *
libinput_plugin_get_name(struct libinput_plugin *plugin)
{
	return plugin->name;
}

struct libinput *
libinput_plugin_get_context(struct libinput_plugin *plugin)
{
	return plugin->libinput;
}

void
libinput_plugin_enable_device_event_frame(struct libinput_plugin *plugin,
					  struct libinput_device *device,
					  bool enable)
{
	if (enable) {
		bitmask_set_bit(&device->plugin_frame_callbacks, plugin->index);
	} else {
		bitmask_clear_bit(&device->plugin_frame_callbacks, plugin->index);
	}
}

struct plugin_queued_event {
	struct list link;
	struct evdev_frame *frame;      /* owns a ref */
	struct libinput_device *device; /* owns a ref */
};

static void
plugin_queued_event_destroy(struct plugin_queued_event *event)
{
	evdev_frame_unref(event->frame);
	libinput_device_unref(event->device);
	list_remove(&event->link);
	free(event);
}

static inline struct plugin_queued_event *
plugin_queued_event_new(struct evdev_frame *frame, struct libinput_device *device)
{
	struct plugin_queued_event *event = zalloc(sizeof(*event));

	event->frame = evdev_frame_ref(frame);
	event->device = libinput_device_ref(device);

	return event;
}

static void
libinput_plugin_queue_evdev_frame(struct list *queue,
				  const char *func,
				  struct libinput_plugin *plugin,
				  struct libinput_device *device,
				  struct evdev_frame *frame)
{
	if (queue == NULL) {
		plugin_log_bug(plugin,
			       "%s() called outside evdev_frame processing\n",
			       func);
		libinput_plugin_unregister(plugin);
		return;
	}

	_unref_(evdev_frame) *clone = evdev_frame_clone(frame);
	struct plugin_queued_event *event = plugin_queued_event_new(clone, device);
	list_take_append(queue, event, link);
}

void
libinput_plugin_append_evdev_frame(struct libinput_plugin *plugin,
				   struct libinput_device *device,
				   struct evdev_frame *frame)
{
	libinput_plugin_queue_evdev_frame(plugin->event_queue.after,
					  __func__,
					  plugin,
					  device,
					  frame);
}

void
libinput_plugin_prepend_evdev_frame(struct libinput_plugin *plugin,
				    struct libinput_device *device,
				    struct evdev_frame *frame)
{
	libinput_plugin_queue_evdev_frame(plugin->event_queue.before,
					  __func__,
					  plugin,
					  device,
					  frame);
}

void
libinput_plugin_inject_evdev_frame(struct libinput_plugin *plugin,
				   struct libinput_device *device,
				   struct evdev_frame *frame)
{
	if (device->inject_evdev_frame)
		device->inject_evdev_frame(device, frame);
}

void
libinput_plugin_run(struct libinput_plugin *plugin)
{
	if (plugin->interface->run)
		plugin->interface->run(plugin);
}

void
libinput_plugin_notify_device_new(struct libinput_plugin *plugin,
				  struct libinput_device *device,
				  struct libevdev *evdev,
				  struct udev_device *udev_device)
{
	if (plugin->interface->device_new)
		plugin->interface->device_new(plugin, device, evdev, udev_device);
}

void
libinput_plugin_notify_device_added(struct libinput_plugin *plugin,
				    struct libinput_device *device)
{
	if (plugin->interface->device_added)
		plugin->interface->device_added(plugin, device);
}

void
libinput_plugin_notify_device_ignored(struct libinput_plugin *plugin,
				      struct libinput_device *device)
{
	if (plugin->interface->device_ignored)
		plugin->interface->device_ignored(plugin, device);
}

void
libinput_plugin_notify_device_removed(struct libinput_plugin *plugin,
				      struct libinput_device *device)
{
	if (plugin->interface->device_removed)
		plugin->interface->device_removed(plugin, device);
}

void
libinput_plugin_system_run(struct libinput_plugin_system *system)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_run(plugin);
	}
}

void
libinput_plugin_system_register_plugin(struct libinput_plugin_system *system,
				       struct libinput_plugin *plugin)
{
	libinput_plugin_ref(plugin);
	list_append(&system->plugins, &plugin->link);
}

void
libinput_plugin_system_unregister_plugin(struct libinput_plugin_system *system,
					 struct libinput_plugin *plugin)
{
	struct libinput_plugin *p;
	list_for_each(p, &system->plugins, link) {
		if (p == plugin) {
			list_remove(&plugin->link);
			list_append(&system->removed_plugins, &plugin->link);
			return;
		}
	}
}

static void
libinput_plugin_system_drop_unregistered_plugins(struct libinput_plugin_system *system)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->removed_plugins, link) {
		list_remove(&plugin->link);
		list_init(&plugin->link); /* allow list_remove in unref */
		libinput_plugin_unref(plugin);
	}
}

void
libinput_plugin_system_init(struct libinput_plugin_system *system)
{
	system->loaded = false;
	list_init(&system->plugins);
	list_init(&system->removed_plugins);
}

void
libinput_plugin_system_load_internal_plugins(struct libinput *libinput,
					     struct libinput_plugin_system *system)
{
	if (system->loaded)
		return;

	system->loaded = true;

#ifdef HAVE_MTDEV
	libinput_mtdev_plugin(libinput);
#endif
	libinput_tablet_plugin_forced_tool(libinput);
	libinput_tablet_plugin_double_tool(libinput);
	libinput_tablet_plugin_proximity_timer(libinput);
	libinput_tablet_plugin_eraser_button(libinput);
	libinput_debounce_plugin(libinput);
	libinput_mouse_plugin_wheel_lowres(libinput);
	libinput_mouse_plugin_wheel(libinput);

	/* Our own event dispatch is implemented as mini-plugin,
	 * guarantee this one to always be last (and after any
	 * other plugins have run so none of the devices are
	 * actually connected to anything yet */
	libinput_evdev_dispatch_plugin(libinput);
}
void
libinput_plugin_system_destroy(struct libinput_plugin_system *system)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_unregister(plugin);
	}

	libinput_plugin_system_drop_unregistered_plugins(system);

	strv_free(system->directories);
}

void
libinput_plugin_system_notify_device_new(struct libinput_plugin_system *system,
					 struct libinput_device *device,
					 struct libevdev *evdev,
					 struct udev_device *udev_device)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_notify_device_new(plugin, device, evdev, udev_device);
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

void
libinput_plugin_system_notify_device_added(struct libinput_plugin_system *system,
					   struct libinput_device *device)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_notify_device_added(plugin, device);
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

void
libinput_plugin_system_notify_device_removed(struct libinput_plugin_system *system,
					     struct libinput_device *device)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_notify_device_removed(plugin, device);
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

void
libinput_plugin_system_notify_device_ignored(struct libinput_plugin_system *system,
					     struct libinput_device *device)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		libinput_plugin_notify_device_ignored(plugin, device);
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

void
libinput_plugin_system_notify_tablet_tool_configured(
	struct libinput_plugin_system *system,
	struct libinput_tablet_tool *tool)
{
	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		if (plugin->interface->tool_configured)
			plugin->interface->tool_configured(plugin, tool);
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

static void
libinput_plugin_process_frame(struct libinput_plugin *plugin,
			      struct libinput_device *device,
			      struct evdev_frame *frame,
			      struct list *queued_events)
{
	struct list before_events = LIST_INIT(before_events);
	struct list after_events = LIST_INIT(after_events);

	plugin->event_queue.before = &before_events;
	plugin->event_queue.after = &after_events;

	if (plugin->interface->evdev_frame)
		plugin->interface->evdev_frame(plugin, device, frame);

	plugin->event_queue.before = NULL;
	plugin->event_queue.after = NULL;

	list_chain(queued_events, &before_events);

	if (!evdev_frame_is_empty(frame)) {
		struct plugin_queued_event *event =
			plugin_queued_event_new(frame, device);
		list_take_append(queued_events, event, link);
	}

	list_chain(queued_events, &after_events);
}

_unused_ static inline void
print_frame(struct libinput *libinput, struct evdev_frame *frame, const char *prefix)
{
	static uint32_t offset = 0;
	static uint32_t last_time = 0;
	uint32_t time = evdev_frame_get_time(frame) / 1000;

	if (offset == 0) {
		offset = time;
		last_time = time - offset;
	}

	time -= offset;

	size_t nevents;
	struct evdev_event *events = evdev_frame_get_events(frame, &nevents);

	for (size_t i = 0; i < nevents; i++) {
		struct evdev_event e = events[i];
		enum evdev_usage usage = evdev_usage_enum(e.usage);

		if ((usage > EVDEV_KEY_ESC && usage < EVDEV_KEY_CAPSLOCK) ||
		    (usage >= EVDEV_KEY_KP7 && usage <= EVDEV_KEY_KPDOT)) {
			e.usage = evdev_usage_from(EVDEV_KEY_A);
		} else if (usage == EVDEV_MSC_SCAN) {
			e.value = 30; /* KEY_A scancode */
		}

		switch (evdev_usage_enum(e.usage)) {
		case EVDEV_SYN_REPORT:
			log_debug(
				libinput,
				"%s%u.%03u ----------------- EV_SYN ----------------- +%ums\n",
				prefix,
				time / 1000,
				time % 1000,
				time - last_time);

			last_time = time;
			break;
		case EVDEV_MSC_SERIAL:
			log_debug(libinput,
				  "%s%u.%03u %-16s %-16s %#010x\n",
				  prefix,
				  time / 1000,
				  time % 1000,
				  evdev_event_get_type_name(&e),
				  evdev_event_get_code_name(&e),
				  e.value);
			break;
		default:
			log_debug(libinput,
				  "%s%u.%03u %-16s %-20s %4d\n",
				  prefix,
				  time / 1000,
				  time % 1000,
				  evdev_event_get_type_name(&e),
				  evdev_event_get_code_name(&e),
				  e.value);
			break;
		}
	}
}

static void
plugin_system_notify_evdev_frame(struct libinput_plugin_system *system,
				 struct libinput_device *device,
				 struct evdev_frame *frame,
				 struct libinput_plugin *sender_plugin)
{
	/* This is messy because a single event frame may cause
	 * *each* plugin to generate multiple event frames for potentially
	 * different devices and replaying is basically breadth-first traversal.
	 *
	 * So we have our event (passed in as 'frame') and we create a queue.
	 * Each plugin then creates a new event list from each frame in the
	 * queue.
	 */
	struct plugin_queued_event *our_event = plugin_queued_event_new(frame, device);

	struct list queued_events = LIST_INIT(queued_events);
	list_take_insert(&queued_events, our_event, link);

	uint64_t frame_time = evdev_frame_get_time(frame);

	bool delay = !!sender_plugin;

	struct libinput_plugin *plugin;
	list_for_each_safe(plugin, &system->plugins, link) {
		/* We start processing *after* the sender plugin. sender_plugin
		 * is only set if we're queuing (not injecting) events from
		 * a plugin timer func
		 */
		if (delay) {
			delay = plugin != sender_plugin;
			continue;
		}

		/* The list of queued events for the *next* plugin */
		struct list next_events = LIST_INIT(next_events);

		/* Iterate through the current list of queued events, pass
		 * each through to the plugin and remove it from the current
		 * list. The plugin may generate a new event list (possibly
		 * containing our frame but not our queued_event directly)
		 * and that list becomes the event list for the next plugin.
		 */
		struct plugin_queued_event *event;
		list_for_each_safe(event, &queued_events, link) {
			struct list next = LIST_INIT(next);

			if (evdev_frame_get_time(event->frame) == 0)
				evdev_frame_set_time(event->frame, frame_time);

			if (!bitmask_bit_is_set(device->plugin_frame_callbacks,
						plugin->index)) {
				list_remove(&event->link);
				list_append(&next_events, &event->link);
				continue;
			}
#ifdef EVENT_DEBUGGING
			_autofree_ char *prefix = strdup_printf(
				"%7s: plugin %-22s - ",
				libinput_device_get_sysname(event->device),
				plugin->name);
			print_frame(libinput_device_get_context(device),
				    event->frame,
				    prefix);
#endif

			libinput_plugin_process_frame(plugin,
						      event->device,
						      event->frame,
						      &next);

			list_chain(&next_events, &next);
			plugin_queued_event_destroy(event);
		}
		assert(list_empty(&queued_events));
		list_chain(&queued_events, &next_events);
		if (list_empty(&queued_events)) {
#ifdef EVENT_DEBUGGING
			if (list_last_entry_by_type(&system->plugins,
						    struct libinput_plugin,
						    link) != plugin) {
				log_debug(
					libinput_device_get_context(device),
					"%s: --- empty frame queue - end of events ---\n",
					plugin->name);
			}
#endif
			/* No more events to process, stop here */
			break;
		}
	}

	/* Our own evdev plugin is last and discards the event for us */
	if (!list_empty(&queued_events)) {
		log_bug_libinput(libinput_device_get_context(device),
				 "Events left over to replay after last plugin\n");
	}
	libinput_plugin_system_drop_unregistered_plugins(system);
}

void
libinput_plugin_system_notify_evdev_frame(struct libinput_plugin_system *system,
					  struct libinput_device *device,
					  struct evdev_frame *frame)
{
	plugin_system_notify_evdev_frame(system, device, frame, NULL);
}

static void
plugin_timer_func(uint64_t now, void *data)
{
	struct libinput_plugin_timer *timer = data;
	struct libinput_plugin *plugin = timer->plugin;
	struct libinput *libinput = plugin->libinput;

	if (!timer->func)
		return;

	struct list before_events = LIST_INIT(before_events);
	struct list after_events = LIST_INIT(after_events);

	plugin->event_queue.before = &before_events;
	plugin->event_queue.after = &after_events;
	timer->func(plugin, now, timer->user_data);
	plugin->event_queue.before = NULL;
	plugin->event_queue.after = NULL;

	list_chain(&before_events, &after_events);

	struct plugin_queued_event *event;
	list_for_each_safe(event, &before_events, link) {
		plugin_system_notify_evdev_frame(&libinput->plugin_system,
						 event->device,
						 event->frame,
						 plugin);
		plugin_queued_event_destroy(event);
	}
}

struct libinput_plugin_timer *
libinput_plugin_timer_new(struct libinput_plugin *plugin,
			  const char *name,
			  void (*func)(struct libinput_plugin *plugin,
				       uint64_t now,
				       void *data),
			  void *data)
{
	struct libinput_plugin_timer *timer = zalloc(sizeof(*timer));

	_autofree_ char *timer_name = strdup_printf("%s-%s", plugin->name, name);

	timer->plugin = plugin;
	timer->refcount = 2; /* one for the caller, one for our list */
	timer->func = func;
	timer->user_data = data;

	libinput_timer_init(&timer->timer,
			    plugin->libinput,
			    timer_name,
			    plugin_timer_func,
			    timer);

	list_append(&plugin->timers, &timer->link);

	return timer;
}

void
libinput_plugin_timer_set_user_data(struct libinput_plugin_timer *timer,
				    void *user_data)
{
	timer->user_data = user_data;
}

void *
libinput_plugin_timer_get_user_data(struct libinput_plugin_timer *timer)
{
	return timer->user_data;
}

struct libinput_plugin_timer *
libinput_plugin_timer_ref(struct libinput_plugin_timer *timer)
{
	assert(timer->refcount > 0);
	++timer->refcount;
	return timer;
}

struct libinput_plugin_timer *
libinput_plugin_timer_unref(struct libinput_plugin_timer *timer)
{
	assert(timer->refcount > 0);
	if (--timer->refcount == 0) {
		libinput_timer_cancel(&timer->timer);
		libinput_timer_destroy(&timer->timer);
		list_remove(&timer->link);
		free(timer);
	}
	return NULL;
}

/* Set timer expire time, in absolute us CLOCK_MONOTONIC */
void
libinput_plugin_timer_set(struct libinput_plugin_timer *timer, uint64_t expire)
{
	libinput_timer_set(&timer->timer, expire);
}

void
libinput_plugin_timer_cancel(struct libinput_plugin_timer *timer)
{
	libinput_timer_cancel(&timer->timer);
}
