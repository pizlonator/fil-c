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

#include "util-list.h"

#include "evdev-frame.h"
#include "libinput.h"

struct libinput;
struct libinput_plugin;

struct libinput_plugin_system {
	char **directories; /* NULL once loaded == true */

	bool loaded;

	struct list plugins;
	struct list removed_plugins;

	size_t next_plugin_index; /* sequential index of all plugins */
};

void
libinput_plugin_system_init(struct libinput_plugin_system *system);

void
libinput_plugin_system_load_internal_plugins(struct libinput *libinput,
					     struct libinput_plugin_system *system);

void
libinput_plugin_system_destroy(struct libinput_plugin_system *system);

void
libinput_plugin_system_run(struct libinput_plugin_system *system);

void
libinput_plugin_system_register_plugin(struct libinput_plugin_system *system,
				       struct libinput_plugin *plugin);
void
libinput_plugin_system_unregister_plugin(struct libinput_plugin_system *system,
					 struct libinput_plugin *plugin);

void
libinput_plugin_system_notify_device_new(struct libinput_plugin_system *system,
					 struct libinput_device *device,
					 struct libevdev *evdev,
					 struct udev_device *udev);

void
libinput_plugin_system_notify_device_added(struct libinput_plugin_system *system,
					   struct libinput_device *device);

void
libinput_plugin_system_notify_device_removed(struct libinput_plugin_system *system,
					     struct libinput_device *device);

void
libinput_plugin_system_notify_device_ignored(struct libinput_plugin_system *system,
					     struct libinput_device *device);

void
libinput_plugin_system_notify_tablet_tool_configured(
	struct libinput_plugin_system *system,
	struct libinput_tablet_tool *tool);

void
libinput_plugin_system_notify_evdev_frame(struct libinput_plugin_system *system,
					  struct libinput_device *device,
					  struct evdev_frame *frame);
