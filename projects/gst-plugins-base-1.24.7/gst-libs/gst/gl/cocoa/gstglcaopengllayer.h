/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef __GST_GL_CA_OPENGL_LAYER__
#define __GST_GL_CA_OPENGL_LAYER__

#include <gst/gst.h>
#include <gst/gl/gl.h>
#include <Cocoa/Cocoa.h>

G_BEGIN_DECLS

@interface GstGLCAOpenGLLayer : CAOpenGLLayer {
@public
  GWeakRef gst_gl_window_ref;
  GstGLContext *gst_gl_context_ref;
  CGLContextObj gl_context;

@private
  GstGLContext *draw_context;
  CGRect last_bounds;
  gint expected_dims[4];

  GstGLWindowCB draw_cb;
  gpointer draw_data;
  GDestroyNotify draw_notify;

  GstGLWindowResizeCB resize_cb;
  gpointer resize_data;
  GDestroyNotify resize_notify;

  gint can_draw;
  gboolean queue_resize;
}
- (void) setDrawCallback:(GstGLWindowCB)cb data:(gpointer)a notify:(GDestroyNotify)notify;
- (void) setResizeCallback:(GstGLWindowResizeCB)cb data:(gpointer)a notify:(GDestroyNotify)notify;
- (void) queueResize;
- (GstGLContext *) getGLContext;
- (id) initWithGstGLWindow:(GstGLWindow *)window;
- (id) initWithGstGLContext: (GstGLContext *)context;
@end

G_END_DECLS

#endif /* __GST_GL_CA_OPENGL_LAYER__ */
