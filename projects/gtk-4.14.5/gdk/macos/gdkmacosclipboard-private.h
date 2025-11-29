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

#include "gdkclipboardprivate.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacospasteboard-private.h"

G_BEGIN_DECLS

#define GDK_TYPE_MACOS_CLIPBOARD (_gdk_macos_clipboard_get_type())

G_DECLARE_FINAL_TYPE (GdkMacosClipboard, _gdk_macos_clipboard, GDK, MACOS_CLIPBOARD, GdkClipboard)

GdkClipboard      *_gdk_macos_clipboard_new                       (GdkMacosDisplay      *display);
void               _gdk_macos_clipboard_check_externally_modified (GdkMacosClipboard    *self);
NSPasteboardType   _gdk_macos_clipboard_to_ns_type                (const char           *mime_type,
                                                                   NSPasteboardType     *alternate);
const char        *_gdk_macos_clipboard_from_ns_type              (NSPasteboardType      ns_type);
void               _gdk_macos_clipboard_register_drag_types       (NSWindow             *window);

G_END_DECLS

