/* GStreamer
 * Copyright (C) 2016 Alessandro Decina <alessandro.d@gmail.com>
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

#ifndef _GST_GL_CONTEXT_HELPER_H_
#define _GST_GL_CONTEXT_HELPER_H_

#include <gst/gst.h>
#include <gst/gl/gl.h>

typedef struct _GstGLContextHelper
{
  GstElement *element;
  GstGLDisplay *display;
  GstGLContext *context;
  GstGLContext *other_context;
} GstGLContextHelper;

GstGLContextHelper * gst_gl_context_helper_new (GstElement *element);
void gst_gl_context_helper_free (GstGLContextHelper *ctxh);
void gst_gl_context_helper_ensure_context (GstGLContextHelper *ctxh);

#endif

