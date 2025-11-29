/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc.
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

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkWin32DisplayManager GdkWin32DisplayManager;
#else
typedef GdkDisplayManager GdkWin32DisplayManager;
#endif
typedef struct _GdkWin32DisplayManagerClass GdkWin32DisplayManagerClass;

#define GDK_TYPE_WIN32_DISPLAY_MANAGER              (gdk_win32_display_manager_get_type())
#define GDK_WIN32_DISPLAY_MANAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_DISPLAY_MANAGER, GdkWin32DisplayManager))
#define GDK_WIN32_DISPLAY_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_DISPLAY_MANAGER, GdkWin32DisplayManagerClass))
#define GDK_IS_WIN32_DISPLAY_MANAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_DISPLAY_MANAGER))
#define GDK_IS_WIN32_DISPLAY_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_DISPLAY_MANAGER))
#define GDK_WIN32_DISPLAY_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_DISPLAY_MANAGER, GdkWin32DisplayManagerClass))

GDK_AVAILABLE_IN_ALL
GType      gdk_win32_display_manager_get_type            (void);

G_END_DECLS

