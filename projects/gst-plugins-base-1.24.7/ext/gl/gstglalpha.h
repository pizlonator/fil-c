/* GStreamer
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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


#ifndef __GST_GL_ALPHA_H__
#define __GST_GL_ALPHA_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_ALPHA \
  (gst_gl_alpha_get_type())
#define GST_GL_ALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_ALPHA,GstGLAlpha))
#define GST_GL_ALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_ALPHA,GstGLAlphaClass))
#define GST_IS_GL_ALPHA(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_ALPHA))
#define GST_IS_GL_ALPHA_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_ALPHA))

typedef struct _GstGLAlpha GstGLAlpha;
typedef struct _GstGLAlphaClass GstGLAlphaClass;

/**
 * GstGLAlphaMethod:
 * @ALPHA_METHOD_SET: Set/adjust alpha channel
 * @ALPHA_METHOD_GREEN: Chroma Key green
 * @ALPHA_METHOD_BLUE: Chroma Key blue
 * @ALPHA_METHOD_CUSTOM: Chroma Key on target_r/g/b
 */
typedef enum
{
  ALPHA_METHOD_SET,
  ALPHA_METHOD_GREEN,
  ALPHA_METHOD_BLUE,
  ALPHA_METHOD_CUSTOM,
}
GstGLAlphaMethod;

/**
 * GstGLAlpha:
 *
 * Opaque data structure.
 */
struct _GstGLAlpha {
  GstGLFilter videofilter;

  GstGLShader *alpha_shader;
  GstGLShader *chroma_key_shader;

  /* properties */
  gdouble alpha;

  guint target_r;
  guint target_g;
  guint target_b;

  GstGLAlphaMethod method;

  gfloat angle;
  gfloat noise_level;
  guint black_sensitivity;
  guint white_sensitivity;

  /* precalculated values for chroma keying */
  gfloat cb, cr;
  gfloat kg;
  gfloat accept_angle_tg;
  gfloat accept_angle_ctg;
  gfloat one_over_kc;
  gfloat kfgy_scale;
  gfloat noise_level2;
};

struct _GstGLAlphaClass {
  GstGLFilterClass parent_class;
};

GType gst_gl_alpha_get_type(void);

G_END_DECLS

#endif /* __GST_GL_ALPHA_H__ */
