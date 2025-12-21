/* GStreamer
 * Copyright (C) <2003> David A. Schleef <ds@schleef.org>
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

#ifndef __GL_TEST_SRC_H__
#define __GL_TEST_SRC_H__

#include <glib.h>

typedef struct _GstGLTestSrc GstGLTestSrc;

/**
 * GstGLTestSrcPattern:
 * @GST_GL_TEST_SRC_SMPTE: A standard SMPTE test pattern
 * @GST_GL_TEST_SRC_SNOW: Random noise
 * @GST_GL_TEST_SRC_BLACK: A black image
 * @GST_GL_TEST_SRC_WHITE: A white image
 * @GST_GL_TEST_SRC_RED: A red image
 * @GST_GL_TEST_SRC_GREEN: A green image
 * @GST_GL_TEST_SRC_BLUE: A blue image
 * @GST_GL_TEST_SRC_CHECKERS1: Checkers pattern (1px)
 * @GST_GL_TEST_SRC_CHECKERS2: Checkers pattern (2px)
 * @GST_GL_TEST_SRC_CHECKERS4: Checkers pattern (4px)
 * @GST_GL_TEST_SRC_CHECKERS8: Checkers pattern (8px)
 * @GST_GL_TEST_SRC_CIRCULAR: Circular pattern
 * @GST_GL_TEST_SRC_BLINK: Alternate between black and white
 *
 * The test pattern to produce.
 */
typedef enum {
    GST_GL_TEST_SRC_SMPTE,
    GST_GL_TEST_SRC_SNOW,
    GST_GL_TEST_SRC_BLACK,
    GST_GL_TEST_SRC_WHITE,
    GST_GL_TEST_SRC_RED,
    GST_GL_TEST_SRC_GREEN,
    GST_GL_TEST_SRC_BLUE,
    GST_GL_TEST_SRC_CHECKERS1,
    GST_GL_TEST_SRC_CHECKERS2,
    GST_GL_TEST_SRC_CHECKERS4,
    GST_GL_TEST_SRC_CHECKERS8,
    GST_GL_TEST_SRC_CIRCULAR,
    GST_GL_TEST_SRC_BLINK,
    GST_GL_TEST_SRC_MANDELBROT
} GstGLTestSrcPattern;

#include "gstgltestsrc.h"

struct BaseSrcImpl {
  GstGLTestSrc *src;
  GstGLContext *context;
  GstVideoInfo v_info;
};

struct SrcFuncs
{
  GstGLTestSrcPattern pattern;
  gpointer (*new) (GstGLTestSrc * src);
  gboolean (*init) (gpointer impl, GstGLContext * context, const GstVideoInfo * v_info);
  gboolean (*fill_bound_fbo) (gpointer impl);
  void (*free) (gpointer impl);
};

const struct SrcFuncs * gst_gl_test_src_get_src_funcs_for_pattern (GstGLTestSrcPattern pattern);

#endif
