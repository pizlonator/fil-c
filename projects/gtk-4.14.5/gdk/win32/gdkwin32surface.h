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

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_SURFACE              (gdk_win32_surface_get_type ())
#define GDK_WIN32_SURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WIN32_SURFACE, GdkWin32Surface))
#define GDK_WIN32_SURFACE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WIN32_SURFACE, GdkWin32SurfaceClass))
#define GDK_IS_WIN32_SURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WIN32_SURFACE))
#define GDK_IS_WIN32_SURFACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WIN32_SURFACE))
#define GDK_WIN32_SURFACE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WIN32_SURFACE, GdkWin32SurfaceClass))

#ifdef GTK_COMPILATION
typedef struct _GdkWin32Surface GdkWin32Surface;
#else
typedef GdkSurface GdkWin32Surface;
#endif
typedef struct _GdkWin32SurfaceClass GdkWin32SurfaceClass;

GDK_AVAILABLE_IN_ALL
GType    gdk_win32_surface_get_type          (void);

GDK_AVAILABLE_IN_ALL
void gdk_win32_surface_set_urgency_hint    (GdkSurface *surface,
                                            gboolean    urgent);

G_END_DECLS

