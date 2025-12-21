/*
 * GStreamer
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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

#ifndef __GL_IOS_UTILS_H__
#define __GL_IOS_UTILS_H__

#include <gst/gst.h>
#include <UIKit/UIKit.h>

#include "gstglwindow_eagl.h"

G_BEGIN_DECLS

@interface GstGLUIView : UIView
- (void) setGstWindow:(GstGLWindowEagl *)window_eagl;
@end

typedef void (*GstGLWindowEaglFunc) (gpointer data);

G_GNUC_INTERNAL
void _gl_invoke_on_main (GstGLWindowEaglFunc func, gpointer data, GDestroyNotify notify);

G_GNUC_INTERNAL
gpointer gst_gl_window_eagl_get_layer (GstGLWindowEagl * window_eagl);

G_END_DECLS

#endif
