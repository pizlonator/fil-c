/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-win32.c: Win32 specific OpenGL wrappers
 *
 * Copyright © 2014  Chun-wei Fan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GDKWIN32_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdkwin32.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_GL_CONTEXT		(gdk_win32_gl_context_get_type ())
#define GDK_WIN32_GL_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WIN32_GL_CONTEXT, GdkWin32GLContext))
#define GDK_WIN32_IS_GL_CONTEXT(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WIN32_GL_CONTEXT))

typedef struct _GdkWin32GLContext		GdkWin32GLContext;
typedef struct _GdkWin32GLContextClass	GdkWin32GLContextClass;

GDK_AVAILABLE_IN_ALL
GType gdk_win32_gl_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
gboolean        gdk_win32_display_get_wgl_version (GdkDisplay *display,
                                                   int        *major,
                                                   int        *minor);

G_END_DECLS

