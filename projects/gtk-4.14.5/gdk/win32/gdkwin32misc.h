/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 2013 Chun-wei Fan
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#pragma once

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

#ifndef STRICT
#define STRICT			/* We want strict type checks */
#endif
#include <windows.h>
#include <commctrl.h>

G_BEGIN_DECLS

#ifdef INSIDE_GDK_WIN32

#include "gdkprivate-win32.h"

#define GDK_SURFACE_HWND(win)          (GDK_WIN32_SURFACE(win)->handle)
#else
/* definition for exported 'internals' go here */
#define GDK_SURFACE_HWND(d) (gdk_win32_surface_get_handle (d))

#endif /* INSIDE_GDK_WIN32 */

/* These need to be here so gtkstatusicon.c can pick them up if needed. */
#ifndef WM_XBUTTONDOWN
#define WM_XBUTTONDOWN 0x020B
#endif
#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP 0x020C
#endif
#ifndef GET_XBUTTON_WPARAM
#define GET_XBUTTON_WPARAM(w) (HIWORD(w))
#endif
#ifndef XBUTTON1
#define XBUTTON1 1
#endif
#ifndef XBUTTON2
#define XBUTTON2 2
#endif

GDK_DEPRECATED_IN_4_8_FOR(GDK_IS_WIN32_SURFACE)
gboolean      gdk_win32_surface_is_win32 (GdkSurface *surface);
GDK_DEPRECATED_IN_4_8_FOR(gdk_win32_surface_get_handle)
HWND          gdk_win32_surface_get_impl_hwnd (GdkSurface *surface);

GDK_DEPRECATED_IN_4_8
gpointer      gdk_win32_handle_table_lookup (HWND handle);

/* Translate from window to Windows handle */
GDK_AVAILABLE_IN_ALL
HWND          gdk_win32_surface_get_handle (GdkSurface *surface);

GDK_AVAILABLE_IN_ALL
GdkSurface *   gdk_win32_surface_lookup_for_display (GdkDisplay *display,
                                                     HWND        anid);

G_END_DECLS

