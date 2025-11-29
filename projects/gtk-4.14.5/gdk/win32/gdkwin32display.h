/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <windows.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkWin32Display GdkWin32Display;
#else
typedef GdkDisplay GdkWin32Display;
#endif
typedef struct _GdkWin32DisplayClass GdkWin32DisplayClass;

#define GDK_TYPE_WIN32_DISPLAY              (gdk_win32_display_get_type())
#define GDK_WIN32_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_DISPLAY, GdkWin32Display))
#define GDK_WIN32_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_DISPLAY, GdkWin32DisplayClass))
#define GDK_IS_WIN32_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_DISPLAY))
#define GDK_IS_WIN32_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_DISPLAY))
#define GDK_WIN32_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_DISPLAY, GdkWin32DisplayClass))

GDK_AVAILABLE_IN_ALL
GType      gdk_win32_display_get_type            (void);

GDK_AVAILABLE_IN_ALL
void       gdk_win32_display_set_cursor_theme    (GdkDisplay  *display,
                                                  const char *name,
                                                  int          size);

/**
 * GdkWin32MessageFilterReturn:
 * @GDK_WIN32_MESSAGE_FILTER_CONTINUE: event not handled, continue processing.
 * @GDK_WIN32_MESSAGE_FILTER_REMOVE: event handled, terminate processing.
 *
 * Specifies the result of applying a `GdkWin32MessageFilterFunc` to a Windows message.
 */
typedef enum {
  GDK_WIN32_MESSAGE_FILTER_CONTINUE,	  /* Message not handled, continue processing */
  GDK_WIN32_MESSAGE_FILTER_REMOVE	  /* Terminate processing, removing message */
} GdkWin32MessageFilterReturn;

/**
 * GdkWin32MessageFilterFunc:
 * @msg: the Windows message to filter.
 * @return_value: a location to store the return value for the message
 * @data: (closure): user data set when the filter was installed.
 *
 * Specifies the type of function used to filter Windows messages before they are
 * processed by GDK Win32 backend.
 *
 * The @return_value must be set, if this function returns
 * `GDK_WIN32_MESSAGE_FILTER_REMOVE`, otherwise it is ignored.
 *
 * The event translation and message filtering are orthogonal -
 * if a filter wants a GDK event queued, it should do that itself.
 *
 * Returns: a `GdkWin32MessageFilterReturn` value.
 */
typedef GdkWin32MessageFilterReturn (*GdkWin32MessageFilterFunc) (GdkWin32Display *display,
                                                                  MSG             *message,
                                                                  int             *return_value,
                                                                  gpointer         data);

GDK_AVAILABLE_IN_ALL
void       gdk_win32_display_add_filter          (GdkWin32Display                 *display,
                                                  GdkWin32MessageFilterFunc        function,
                                                  gpointer                         data);

GDK_AVAILABLE_IN_ALL
void       gdk_win32_display_remove_filter       (GdkWin32Display                 *display,
                                                  GdkWin32MessageFilterFunc        function,
                                                  gpointer                         data);

GDK_AVAILABLE_IN_ALL
GdkMonitor * gdk_win32_display_get_primary_monitor (GdkDisplay *display);

GDK_AVAILABLE_IN_4_4
gpointer gdk_win32_display_get_egl_display         (GdkDisplay  *display);


G_END_DECLS

