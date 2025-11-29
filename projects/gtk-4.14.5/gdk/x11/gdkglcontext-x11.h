/* GDK - The GIMP Drawing Kit
 *
 * gdkglcontext-x11.h: Private X11 specific OpenGL wrappers
 * 
 * Copyright © 2014  Emmanuele Bassi
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

#include "gdkx11glcontext.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#ifdef HAVE_XDAMAGE
#include <X11/extensions/Xdamage.h>
#endif

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include "gdkglcontextprivate.h"
#include "gdkdisplay-x11.h"
#include "gdksurface.h"

G_BEGIN_DECLS

#define GDK_X11_GL_CONTEXT_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_X11_GL_CONTEXT, GdkX11GLContextClass))
#define GDK_X11_GL_CONTEXT_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_X11_GL_CONTEXT, GdkX11GLContextClass))
#define GDK_X11_IS_GL_CONTEXT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_X11_GL_CONTEXT))

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

/* GLX */
#define GDK_TYPE_X11_GL_CONTEXT_GLX     (gdk_x11_gl_context_glx_get_type())
#define GDK_X11_GL_CONTEXT_GLX(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_X11_GL_CONTEXT_GLX, GdkX11GLContextGLX))
#define GDK_IS_X11_GL_CONTEXT_GLX(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_X11_GL_CONTEXT_GLX))

typedef struct _GdkX11GLContextGLX      GdkX11GLContextGLX;

gboolean                gdk_x11_display_init_glx                (GdkX11Display *display_x11,
                                                                 Visual       **out_visual,
                                                                 int           *out_depth,
                                                                 GError       **error);
void                    gdk_x11_surface_destroy_glx_drawable    (GdkX11Surface *self);

GType                   gdk_x11_gl_context_glx_get_type         (void) G_GNUC_CONST;


/* EGL */
#define GDK_TYPE_X11_GL_CONTEXT_EGL     (gdk_x11_gl_context_egl_get_type())
#define GDK_X11_GL_CONTEXT_EGL(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_X11_GL_CONTEXT_EGL, GdkX11GLContextEGL))
#define GDK_IS_X11_GL_CONTEXT_EGL(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_X11_GL_CONTEXT_EGL))

typedef struct _GdkX11GLContextEGL      GdkX11GLContextEGL;

GType                   gdk_x11_gl_context_egl_get_type         (void) G_GNUC_CONST;

G_END_DECLS

