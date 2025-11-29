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
#include "gdkmacoskeymap.h"

G_BEGIN_DECLS

GdkMacosKeymap *_gdk_macos_keymap_new            (GdkMacosDisplay *display);
GdkEventType    _gdk_macos_keymap_get_event_type (NSEvent         *event);
gboolean        _gdk_macos_keymap_is_modifier    (guint            keycode);
gunichar        _gdk_macos_keymap_get_equivalent (guint            key);

G_END_DECLS

