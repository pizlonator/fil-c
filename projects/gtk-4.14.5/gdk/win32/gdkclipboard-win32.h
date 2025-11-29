/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdk/gdkclipboard.h"

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_CLIPBOARD    (gdk_win32_clipboard_get_type ())
#define GDK_WIN32_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_CLIPBOARD, GdkWin32Clipboard))
#define GDK_IS_WIN32_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_CLIPBOARD))

typedef struct _GdkWin32Clipboard GdkWin32Clipboard;

GType                   gdk_win32_clipboard_get_type            (void) G_GNUC_CONST;

GdkClipboard *          gdk_win32_clipboard_new                 (GdkDisplay *display);

void                    gdk_win32_clipboard_claim_remote        (GdkWin32Clipboard *cb);

G_END_DECLS

