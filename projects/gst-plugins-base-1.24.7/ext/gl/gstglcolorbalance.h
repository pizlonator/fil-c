/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_GL_COLOR_BALANCE_H__
#define __GST_GL_COLOR_BALANCE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_COLOR_BALANCE \
  (gst_gl_color_balance_get_type())
#define GST_GL_COLOR_BALANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_COLOR_BALANCE,GstGLColorBalance))
#define GST_GL_COLOR_BALANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_COLOR_BALANCE,GstGLColorBalanceClass))
#define GST_IS_GL_COLOR_BALANCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_COLOR_BALANCE))
#define GST_IS_GL_COLOR_BALANCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_COLOR_BALANCE))

typedef struct _GstGLColorBalance GstGLColorBalance;
typedef struct _GstGLColorBalanceClass GstGLColorBalanceClass;

/**
 * GstGLColorBalance:
 *
 * Opaque data structure.
 */
struct _GstGLColorBalance {
  GstGLFilter videofilter;

  GstGLShader *shader;

  /* channels for interface */
  GList *channels;

  /* cached matrix for speeding up the operations within the shader */
  gfloat yuva_balance_matrix[4*4];
  gfloat yuva_balance_constant[4];

  /* properties */
  gdouble contrast;
  gdouble brightness;
  gdouble hue;
  gdouble saturation;
};

struct _GstGLColorBalanceClass {
  GstGLFilterClass parent_class;
};

GType gst_gl_color_balance_get_type(void);

G_END_DECLS

#endif /* __GST_GL_COLOR_BALANCE_H__ */
