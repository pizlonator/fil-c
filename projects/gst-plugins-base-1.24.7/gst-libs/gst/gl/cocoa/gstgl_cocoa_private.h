/*
 * GStreamer
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

#ifndef __GST_GL_COCOA_PRIVATE_H__
#define __GST_GL_COCOA_PRIVATE_H__

#include <gst/gst.h>
#include <gst/gl/gstglfuncs.h>

#include <Cocoa/Cocoa.h>

#include "gstglwindow_cocoa.h"
#include "gstglcontext_cocoa.h"
#include "gstglcaopengllayer.h"

G_BEGIN_DECLS

struct _GstGLContextCocoaPrivate
{
  CGLPixelFormatObj pixel_format;
  CGLContextObj gl_context;
  CGLContextObj external_gl_context;

  GstGLAPI context_api;

  gint source_id;
};


/* =============================================================*/
/*                                                              */
/*                  GstGLNSView declaration               */
/*                                                              */
/* =============================================================*/

@interface GstGLNSView: NSView {
@public
  GstGLWindowCocoa *window_cocoa;
  GstGLCAOpenGLLayer *layer;
}
- (id) initWithFrameLayer:(GstGLWindowCocoa *)window rect:(NSRect)contentRect layer:(CALayer *)layerContent;
@end

gboolean gst_gl_window_cocoa_create_window (GstGLWindowCocoa *window_cocoa);

G_GNUC_INTERNAL
void _gst_gl_invoke_on_main (GstGLWindowCB func, gpointer data, GDestroyNotify notify);

G_END_DECLS

#endif /* __GST_GL_COCOA_PRIVATE_H__ */
