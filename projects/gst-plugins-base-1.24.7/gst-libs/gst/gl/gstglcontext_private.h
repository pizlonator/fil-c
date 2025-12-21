/* GStreamer
 * Copyright (C) 2012 Matthew Waters <ystreet00@gmail.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_GL_CONTEXT_PRIVATE_H__
#define __GST_GL_CONTEXT_PRIVATE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL extern GstDebugCategory *gst_gl_context_debug;

G_GNUC_INTERNAL
gboolean            _gst_gl_context_debug_is_enabled            (GstGLContext * context);

G_GNUC_INTERNAL
void                gst_gl_context_apply_quirks                 (GstGLContext * context);

#define GST_GL_CONTEXT_WRAPPED_GL_CONFIG_NAME "gst.gl.context.wrapped.config"

#define GST_TYPE_GL_WRAPPED_CONTEXT (gst_gl_wrapped_context_get_type())
G_GNUC_INTERNAL
GType gst_gl_wrapped_context_get_type (void);

#define GST_GL_WRAPPED_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_TYPE_GL_WRAPPED_CONTEXT, GstGLWrappedContext))
#define GST_GL_WRAPPED_CONTEXT_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_TYPE_GL_CONTEXT, GstGLContextClass))
#define GST_IS_GL_WRAPPED_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_TYPE_GL_WRAPPED_CONTEXT))
#define GST_IS_GL_WRAPPED_CONTEXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_TYPE_GL_WRAPPED_CONTEXT))
#define GST_GL_WRAPPED_CONTEXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_TYPE_GL_WRAPPED_CONTEXT, GstGLWrappedContextClass))

G_END_DECLS

#endif /* __GST_GL_CONTEXT_PRIVATE_H__ */
