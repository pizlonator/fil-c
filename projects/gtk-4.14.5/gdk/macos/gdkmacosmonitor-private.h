/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay.h"
#include "gdkmacosmonitor.h"
#include "gdkmacossurface.h"

#include "gdkmonitorprivate.h"

G_BEGIN_DECLS

char              *_gdk_macos_monitor_get_localized_name    (NSScreen          *screen);
char              *_gdk_macos_monitor_get_connector_name    (CGDirectDisplayID  screen_id);
GdkMacosMonitor   *_gdk_macos_monitor_new                   (GdkMacosDisplay   *display,
                                                             CGDirectDisplayID  screen_id);
CGDirectDisplayID  _gdk_macos_monitor_get_screen_id         (GdkMacosMonitor   *self);
gboolean           _gdk_macos_monitor_reconfigure           (GdkMacosMonitor   *self);
CGColorSpaceRef    _gdk_macos_monitor_copy_colorspace       (GdkMacosMonitor   *self);
void               _gdk_macos_monitor_add_frame_callback    (GdkMacosMonitor   *self,
                                                             GdkMacosSurface   *surface);
void               _gdk_macos_monitor_remove_frame_callback (GdkMacosMonitor   *self,
                                                             GdkMacosSurface   *surface);
void               _gdk_macos_monitor_clamp                 (GdkMacosMonitor   *self,
                                                             GdkRectangle      *area);

G_END_DECLS

