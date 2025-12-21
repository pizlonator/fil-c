/*
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
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

#ifndef __GST_GL_EFFECTS_H__
#define __GST_GL_EFFECTS_H__

#include <gst/gl/gstglfilter.h>
#include <gst/gl/gstglfuncs.h>

#include "effects/gstgleffectssources.h"

G_BEGIN_DECLS

#define GST_TYPE_GL_EFFECTS            (gst_gl_effects_get_type())
#define GST_GL_EFFECTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GL_EFFECTS,GstGLEffects))
#define GST_IS_GL_EFFECTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GL_EFFECTS))
#define GST_GL_EFFECTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) , GST_TYPE_GL_EFFECTS,GstGLEffectsClass))
#define GST_IS_GL_EFFECTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) , GST_TYPE_GL_EFFECTS))
#define GST_GL_EFFECTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) , GST_TYPE_GL_EFFECTS,GstGLEffectsClass))

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

typedef struct _GstGLEffects GstGLEffects;
typedef struct _GstGLEffectsClass GstGLEffectsClass;

typedef struct {
  gint effect;
  guint supported_properties;
  const gchar *filter_name;
  const gchar *filter_longname;
} GstGLEffectsFilterDescriptor;

typedef void (* GstGLEffectProcessFunc) (GstGLEffects *effects);

#define NEEDED_TEXTURES 5

enum {
  GST_GL_EFFECTS_CURVE_HEAT,
  GST_GL_EFFECTS_CURVE_SEPIA,
  GST_GL_EFFECTS_CURVE_XPRO,
  GST_GL_EFFECTS_CURVE_LUMA_XPRO,
  GST_GL_EFFECTS_CURVE_XRAY,
  GST_GL_EFFECTS_N_CURVES
};

struct _GstGLEffects
{
  GstGLFilter filter;

  GstGLEffectProcessFunc effect;
  gint current_effect;

  GstGLMemory *intexture;
  GstGLMemory *midtexture[NEEDED_TEXTURES];
  GstGLMemory *outtexture;

  GLuint curve[GST_GL_EFFECTS_N_CURVES];

  GHashTable *shaderstable;

  gboolean horizontal_swap; /* switch left to right */
  gboolean invert; /* colours */
};

struct _GstGLEffectsClass
{
  GstGLFilterClass filter_class;
  const GstGLEffectsFilterDescriptor *filter_descriptor;
};

GType gst_gl_effects_get_type (void);
gboolean gst_gl_effects_register_filters (GstPlugin *, GstRank);
GstGLShader* gst_gl_effects_get_fragment_shader (GstGLEffects *effects,
    const gchar * shader_name, const gchar * shader_source_gles2);

void gst_gl_effects_identity (GstGLEffects *effects);
void gst_gl_effects_mirror (GstGLEffects *effects);
void gst_gl_effects_squeeze (GstGLEffects *effects);
void gst_gl_effects_stretch (GstGLEffects *effects);
void gst_gl_effects_tunnel (GstGLEffects *effects);
void gst_gl_effects_fisheye (GstGLEffects *effects);
void gst_gl_effects_twirl (GstGLEffects *effects);
void gst_gl_effects_bulge (GstGLEffects *effects);
void gst_gl_effects_square (GstGLEffects *effects);
void gst_gl_effects_heat (GstGLEffects *effects);
void gst_gl_effects_sepia (GstGLEffects *effects);
void gst_gl_effects_xpro (GstGLEffects *effects);
void gst_gl_effects_xray (GstGLEffects *effects);
void gst_gl_effects_luma_xpro (GstGLEffects *effects);
void gst_gl_effects_sin (GstGLEffects *effects);
void gst_gl_effects_glow (GstGLEffects *effects);
void gst_gl_effects_sobel (GstGLEffects *effects);
void gst_gl_effects_blur (GstGLEffects *effects);
void gst_gl_effects_laplacian (GstGLEffects *effects);

G_END_DECLS

#endif /*__GST_GL_EFFECTS_H__ */
