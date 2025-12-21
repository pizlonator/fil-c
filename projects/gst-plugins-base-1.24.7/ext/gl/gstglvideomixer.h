/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_VIDEO_MIXER_H_
#define _GST_GL_VIDEO_MIXER_H_

#include <gst/gl/gstglmixer.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_VIDEO_MIXER            (gst_gl_video_mixer_get_type())
#define GST_GL_VIDEO_MIXER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_VIDEO_MIXER,GstGLVideoMixer))
#define GST_IS_GL_VIDEO_MIXER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_VIDEO_MIXER))
#define GST_GL_VIDEO_MIXER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_VIDEO_MIXER,GstGLVideoMixerClass))
#define GST_IS_GL_VIDEO_MIXER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_VIDEO_MIXER))
#define GST_GL_VIDEO_MIXER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_VIDEO_MIXER,GstGLVideoMixerClass))

typedef struct _GstGLVideoMixer GstGLVideoMixer;
typedef struct _GstGLVideoMixerClass GstGLVideoMixerClass;

/**
 * GstGLVideoMixerBackground:
 * @GST_GL_VIDEO_MIXER_BACKGROUND_CHECKER: checker pattern background
 * @GST_GL_VIDEO_MIXER_BACKGROUND_BLACK: solid color black background
 * @GST_GL_VIDEO_MIXER_BACKGROUND_WHITE: solid color white background
 * @GST_GL_VIDEO_MIXER_BACKGROUND_TRANSPARENT: background is left transparent and layers are composited using "A OVER B" composition rules. This is only applicable to AYUV and ARGB (and variants) as it preserves the alpha channel and allows for further mixing.
 *
 * The different backgrounds compositor can blend over.
 */
typedef enum
{
  GST_GL_VIDEO_MIXER_BACKGROUND_CHECKER,
  GST_GL_VIDEO_MIXER_BACKGROUND_BLACK,
  GST_GL_VIDEO_MIXER_BACKGROUND_WHITE,
  GST_GL_VIDEO_MIXER_BACKGROUND_TRANSPARENT,
}
GstGLVideoMixerBackground;

/**
 * GstGLVideoMixerBlendEquation:
 * @GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD: Add the two results.
 * @GST_GL_VIDEO_MIXER_BLEND_EQUATION_SUBTRACT: Subtract component-wise the destination from the source (S - D).
 * @GST_GL_VIDEO_MIXER_BLEND_EQUATION_REVERSE_SUBTRACT: Subtract component-wise the source from the destination (D - S).
 *
 * The blending equation to use.  See the opengl specificition for
 * glBlendEquationSeparate
 */
typedef enum
{
  GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD,
  GST_GL_VIDEO_MIXER_BLEND_EQUATION_SUBTRACT,
  GST_GL_VIDEO_MIXER_BLEND_EQUATION_REVERSE_SUBTRACT,
}
GstGLVideoMixerBlendEquation;

/**
 * GstGLVideoMixerBlendFunction:
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ZERO: All components are zero
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE: All components are one
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_COLOR: Use the source color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_COLOR: One minus the source color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_COLOR: Use the destination color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_COLOR: One minus the destination color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA: All components are the source alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA: All components are one minus the source alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_ALPHA: All components are the destination alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_ALPHA: All components are one minus the destination alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_COLOR: Use the constant color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_COLOR: Use one minus the constant color/alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_ALPHA: All components are the constant alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_COLOR: All components are one minus the constant alpha
 * @GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE: All color components
 *     are the minimum of source alpha and one minus the destination alpha.
 *     Alpha is equal to one.
 *
 * The blending function to use.  See the opengl specificition for
 * glBlendFuncSeparate
 */
typedef enum
{
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ZERO,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_COLOR,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_ALPHA,
  GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE,
}
GstGLVideoMixerBlendFunction;

/**
 * GstGLVideoMixerSizingPolicy:
 * @GST_GL_VIDEO_MIXER_SIZING_POLICY_NONE: Scaling image without padding
 * @GST_GL_VIDEO_MIXER_SIZING_POLICY_KEEP_ASPECT_RATIO: Scaling image to fit destination
 *    resolution with preserving aspect ratio. Resulting image will be centered
 *    in the configured destination rectangle and it might have padding area
 *    if aspect ratio of destination rectangle is different from that of
 *    input image.
 *
 * Since: 1.24
 */
typedef enum
{
  GST_GL_VIDEO_MIXER_SIZING_POLICY_NONE,
  GST_GL_VIDEO_MIXER_SIZING_POLICY_KEEP_ASPECT_RATIO,
}
GstGLVideoMixerSizingPolicy;

struct _GstGLVideoMixer
{
    GstGLMixer mixer;

    GstGLVideoMixerBackground background;

    GstGLShader *shader;
    GstGLShader *checker;

    guint vao;
    guint vbo_indices;
    guint checker_vbo;
    GstGLMemory *out_tex;

    gboolean output_geo_change;
};

struct _GstGLVideoMixerClass
{
    GstGLMixerClass mixer_class;
};

GType gst_gl_video_mixer_get_type (void);
GType gst_gl_video_mixer_bin_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERCUBE_H_ */
