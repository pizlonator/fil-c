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

/**
 * SECTION:element-glvideomixer
 * @title: glvideomixer
 *
 * Composites a number of streams into a single output scene using OpenGL in
 * a similar fashion to compositor and videomixer. See the compositor plugin
 * for documentation about the #GstGLVideoMixerPad properties.
 *
 * ## Examples
 * |[
 * gst-launch-1.0  glvideomixer name=m ! glimagesink \
 *     videotestsrc ! video/x-raw, format=YUY2 ! glupload ! glcolorconvert ! m. \
 *     videotestsrc pattern=12 ! video/x-raw, format=I420, framerate=5/1, width=100, height=200 ! queue ! \
 *     glupload ! glcolorconvert ! m. \
 *     videotestsrc ! glupload ! gleffects effect=2 ! queue ! m.  \
 *     videotestsrc ! glupload ! glfiltercube ! queue ! m. \
 *     videotestsrc ! glupload ! gleffects effect=6 ! queue ! m.
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/controller/gstproxycontrolbinding.h>
#include <gst/gl/gstglfuncs.h>
#include <gst/video/gstvideoaffinetransformationmeta.h>

#include "gstglelements.h"
#include "gstglvideomixer.h"

#include "gstglmixerbin.h"
#include "gstglutils.h"

#define GST_CAT_DEFAULT gst_gl_video_mixer_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA"))
    );

#define GST_TYPE_GL_VIDEO_MIXER_BACKGROUND (gst_gl_video_mixer_background_get_type())
static GType
gst_gl_video_mixer_background_get_type (void)
{
  static GType mixer_background_type = 0;

  static const GEnumValue mixer_background[] = {
    {GST_GL_VIDEO_MIXER_BACKGROUND_CHECKER, "Checker pattern", "checker"},
    {GST_GL_VIDEO_MIXER_BACKGROUND_BLACK, "Black", "black"},
    {GST_GL_VIDEO_MIXER_BACKGROUND_WHITE, "White", "white"},
    {GST_GL_VIDEO_MIXER_BACKGROUND_TRANSPARENT,
        "Transparent Background to enable further compositing", "transparent"},
    {0, NULL, NULL},
  };

  if (!mixer_background_type) {
    mixer_background_type =
        g_enum_register_static ("GstGLVideoMixerBackground", mixer_background);
  }
  return mixer_background_type;
}

#define GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION (gst_gl_video_mixer_blend_equation_get_type())
static GType
gst_gl_video_mixer_blend_equation_get_type (void)
{
  static GType mixer_blend_equation_type = 0;

  static const GEnumValue mixer_blend_equations[] = {
    {GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD, "Add", "add"},
    {GST_GL_VIDEO_MIXER_BLEND_EQUATION_SUBTRACT, "Subtract", "subtract"},
    {GST_GL_VIDEO_MIXER_BLEND_EQUATION_REVERSE_SUBTRACT, "Reverse Subtract",
        "reverse-subtract"},
    {0, NULL, NULL},
  };

  if (!mixer_blend_equation_type) {
    mixer_blend_equation_type =
        g_enum_register_static ("GstGLVideoMixerBlendEquation",
        mixer_blend_equations);
  }
  return mixer_blend_equation_type;
}

#define GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION (gst_gl_video_mixer_blend_function_get_type())
static GType
gst_gl_video_mixer_blend_function_get_type (void)
{
  static GType mixer_blend_function_type = 0;

  static const GEnumValue mixer_blend_funcs[] = {
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ZERO, "Zero", "zero"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE, "One", "one"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_COLOR, "Source Color", "src-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_COLOR,
        "One Minus Source Color", "one-minus-src-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_COLOR, "Destination Color",
        "dst-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_COLOR,
        "One Minus Destination Color", "one-minus-dst-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA, "Source Alpha", "src-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA,
        "One Minus Source Alpha", "one-minus-src-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_ALPHA, "Destination Alpha",
        "dst-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_ALPHA,
        "One Minus Destination Alpha", "one-minus-dst-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_COLOR, "Constant Color",
        "constant-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_COLOR,
        "One Minus Constant Color", "one-minus-contant-color"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_ALPHA, "Constant Alpha",
        "constant-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_ALPHA,
        "One Minus Constant Alpha", "one-minus-contant-alpha"},
    {GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE,
        "Source Alpha Saturate", "src-alpha-saturate"},
    {0, NULL, NULL},
  };

  if (!mixer_blend_function_type) {
    mixer_blend_function_type =
        g_enum_register_static ("GstGLVideoMixerBlendFunction",
        mixer_blend_funcs);
  }
  return mixer_blend_function_type;
}

#define GST_TYPE_GL_VIDEO_MIXER_SIZING_POLICY (gst_gl_video_mixer_sizing_policy_get_type())
static GType
gst_gl_video_mixer_sizing_policy_get_type (void)
{

  static GType sizing_policy_type = 0;

  static const GEnumValue sizing_polices[] = {
    {GST_GL_VIDEO_MIXER_SIZING_POLICY_NONE,
        "None: Image is scaled to fill configured destination rectangle without "
          "padding or keeping the aspect ratio", "none"},
    {GST_GL_VIDEO_MIXER_SIZING_POLICY_KEEP_ASPECT_RATIO,
          "Keep Aspect Ratio: Image is scaled to fit destination rectangle "
          "specified by GstGLVideoMixerPad:{xpos, ypos, width, height} "
          "with preserved aspect ratio. The empty space of the resulting image "
          "will be distributed in the destination rectangle according to the "
          "GstGLVideoMixerPad:{xalign, yalign} values",
        "keep-aspect-ratio"},
    {0, NULL, NULL},
  };

  if (!sizing_policy_type) {
    sizing_policy_type =
        g_enum_register_static ("GstGLVideoMixerSizingPolicy", sizing_polices);
  }
  return sizing_policy_type;
}

#define DEFAULT_PAD_XPOS   0
#define DEFAULT_PAD_YPOS   0
#define DEFAULT_PAD_WIDTH  0
#define DEFAULT_PAD_HEIGHT 0
#define DEFAULT_PAD_ALPHA  1.0
#define DEFAULT_PAD_ZORDER 0
#define DEFAULT_PAD_REPEAT_AFTER_EOS FALSE
#define DEFAULT_PAD_BLEND_EQUATION_RGB GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD
#define DEFAULT_PAD_BLEND_EQUATION_ALPHA GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD
#define DEFAULT_PAD_BLEND_FUNCTION_SRC_RGB GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA
#define DEFAULT_PAD_BLEND_FUNCTION_SRC_ALPHA GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE
#define DEFAULT_PAD_BLEND_FUNCTION_DST_RGB GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA
#define DEFAULT_PAD_BLEND_FUNCTION_DST_ALPHA GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA
#define DEFAULT_PAD_CROP 0
#define DEFAULT_PAD_SIZING_POLICY GST_GL_VIDEO_MIXER_SIZING_POLICY_NONE
#define DEFAULT_PAD_XALIGN 0.5
#define DEFAULT_PAD_YALIGN 0.5

enum
{
  PROP_INPUT_0,
  PROP_INPUT_XPOS,
  PROP_INPUT_YPOS,
  PROP_INPUT_WIDTH,
  PROP_INPUT_HEIGHT,
  PROP_INPUT_ALPHA,
  PROP_INPUT_BLEND_EQUATION_RGB,
  PROP_INPUT_BLEND_EQUATION_ALPHA,
  PROP_INPUT_BLEND_FUNCTION_SRC_RGB,
  PROP_INPUT_BLEND_FUNCTION_SRC_ALPHA,
  PROP_INPUT_BLEND_FUNCTION_DST_RGB,
  PROP_INPUT_BLEND_FUNCTION_DST_ALPHA,
  PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_RED,
  PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_GREEN,
  PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_BLUE,
  PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA,
  PROP_INPUT_ZORDER,
  PROP_INPUT_REPEAT_AFTER_EOS,
  PROP_INPUT_CROP_LEFT,
  PROP_INPUT_CROP_RIGHT,
  PROP_INPUT_CROP_TOP,
  PROP_INPUT_CROP_BOTTOM,
  PROP_INPUT_SIZING_POLICY,
  PROP_INPUT_XALIGN,
  PROP_INPUT_YALIGN,
};

static void gst_gl_video_mixer_input_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_gl_video_mixer_input_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static gboolean gst_gl_video_mixer_src_event (GstAggregator * agg,
    GstEvent * event);
static void gst_gl_video_mixer_input_dispose (GObject * object);

typedef struct _GstGLVideoMixerInput GstGLVideoMixerInput;
typedef GstGhostPadClass GstGLVideoMixerInputClass;

struct _GstGLVideoMixerInput
{
  GstGhostPad parent;

  GstSegment segment;

  GstPad *mixer_pad;
};

GType gst_gl_video_mixer_input_get_type (void);
G_DEFINE_TYPE (GstGLVideoMixerInput, gst_gl_video_mixer_input,
    GST_TYPE_GHOST_PAD);

static void
gst_gl_video_mixer_input_init (GstGLVideoMixerInput * self)
{
}

static void
gst_gl_video_mixer_input_class_init (GstGLVideoMixerInputClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_gl_video_mixer_input_set_property;
  gobject_class->get_property = gst_gl_video_mixer_input_get_property;
  gobject_class->dispose = gst_gl_video_mixer_input_dispose;

  g_object_class_install_property (gobject_class, PROP_INPUT_ZORDER,
      g_param_spec_uint ("zorder", "Z-Order", "Z Order of the picture",
          0, 10000, DEFAULT_PAD_ZORDER,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_REPEAT_AFTER_EOS,
      g_param_spec_boolean ("repeat-after-eos", "Repeat After EOS",
          "Aggregate the last "
          "frame on pads that are EOS till they are released",
          DEFAULT_PAD_REPEAT_AFTER_EOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture", G_MININT,
          G_MAXINT, DEFAULT_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture", G_MININT,
          G_MAXINT, DEFAULT_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_PAD_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_BLEND_EQUATION_RGB,
      g_param_spec_enum ("blend-equation-rgb", "Blend Equation RGB",
          "Blend Equation for RGB", GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION,
          DEFAULT_PAD_BLEND_EQUATION_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_EQUATION_ALPHA,
      g_param_spec_enum ("blend-equation-alpha", "Blend Equation Alpha",
          "Blend Equation for Alpha", GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION,
          DEFAULT_PAD_BLEND_EQUATION_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_SRC_RGB,
      g_param_spec_enum ("blend-function-src-rgb", "Blend Function Source RGB",
          "Blend Function for Source RGB",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_SRC_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_SRC_ALPHA,
      g_param_spec_enum ("blend-function-src-alpha",
          "Blend Function Source Alpha", "Blend Function for Source Alpha",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_SRC_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_DST_RGB,
      g_param_spec_enum ("blend-function-dst-rgb",
          "Blend Function Destination RGB",
          "Blend Function for Destination RGB",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_DST_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_DST_ALPHA,
      g_param_spec_enum ("blend-function-dst-alpha",
          "Blend Function Destination Alpha",
          "Blend Function for Destination Alpha",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_DST_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_RED,
      g_param_spec_double ("blend-constant-color-red",
          "Blend Constant Color Red", "Blend Constant Color Red", 0.0, 1.0, 0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_GREEN,
      g_param_spec_double ("blend-constant-color-green",
          "Blend Constant Color Green", "Blend Constant Color Green", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_BLUE,
      g_param_spec_double ("blend-constant-color-blue",
          "Blend Constant Color Green", "Blend Constant Color Green", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA,
      g_param_spec_double ("blend-constant-color-alpha",
          "Blend Constant Color Alpha", "Blend Constant Color Alpha", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerInput:crop-left:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the left side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_CROP_LEFT,
      g_param_spec_int ("crop-left", "Crop Left", "Crop left of the picture", 0,
          G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerInput:crop-right:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the right side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_CROP_RIGHT,
      g_param_spec_int ("crop-right", "Crop Right", "Crop right of the picture",
          0, G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerInput:crop-top:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the top side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_CROP_TOP,
      g_param_spec_int ("crop-top", "Crop Top", "Crop top of the picture", 0,
          G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerInput:crop-bottom:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the bottom side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_CROP_BOTTOM,
      g_param_spec_int ("crop-bottom", "Crop Bottom",
          "Crop bottom of the picture", 0, G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerInput:sizing-policy:
   *
   * Specifies sizing policy to use. Depending on selected sizing policy,
   * scaled image might not fully cover the configured target rectangle area
   * (e.g., "keep-aspect-ratio"). In that case, any uncovered area will be
   * filled with background unless the uncovered area is drawn by other image.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_SIZING_POLICY,
      g_param_spec_enum ("sizing-policy", "Sizing policy",
          "Sizing policy to use for image scaling",
          GST_TYPE_GL_VIDEO_MIXER_SIZING_POLICY, DEFAULT_PAD_SIZING_POLICY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerInput:xalign:
   *
   * Defines the alignment of the input within the available horizontal space,
   * relative to #GstGLVideoMixerPad:width and #GstGLVideoMixerPad:height.
   * Values range from 0.0 (left) to 1.0 (right).
   *
   * The image is aligned in the available space as if the pivot point is
   * matching the alignment. For example, setting the `xalign` property to 0.0
   * will align the left edge of the image with the left edge of the bounding
   * box; 0.5 aligns the horizontal center of the image with the horizontal
   * center of the bounding box; 1.0 aligns the right edge of the image with the
   * right edge of the bounding box; and so it goes.
   *
   * This property is only effective when #GstGLVideoMixerInput:sizing-policy
   * is set to 'keep-aspect-ratio'.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_XALIGN,
      g_param_spec_double ("xalign", "X alignment",
          "X alignment of the picture", 0.0, 1.0, DEFAULT_PAD_XALIGN,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerInput:yalign:
   *
   * Defines the alignment of the input within the available vertical space,
   * relative to #GstGLVideoMixerPad:width and #GstGLVideoMixerPad:height.
   * Values range from 0.0 (top) to 1.0 (bottom).
   *
   * The image is aligned in the available space as if the pivot point is
   * matching the alignment. For example, setting the `xalign` property to 0.0
   * will align the left edge of the image with the left edge of the bounding
   * box; 0.5 aligns the horizontal center of the image with the horizontal
   * center of the bounding box; 1.0 aligns the right edge of the image with the
   * right edge of the bounding box; and so it goes.
   *
   * This property is only effective when #GstGLVideoMixerInput:sizing-policy
   * is set to 'keep-aspect-ratio'.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_INPUT_YALIGN,
      g_param_spec_double ("yalign", "Y alignment",
          "Y alignment of the picture", 0.0, 1.0, DEFAULT_PAD_YALIGN,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_video_mixer_input_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixerInput *self = (GstGLVideoMixerInput *) object;

  if (self->mixer_pad)
    g_object_get_property (G_OBJECT (self->mixer_pad), pspec->name, value);
}

static void
gst_gl_video_mixer_input_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixerInput *self = (GstGLVideoMixerInput *) object;

  if (self->mixer_pad)
    g_object_set_property (G_OBJECT (self->mixer_pad), pspec->name, value);
}

static void
gst_gl_video_mixer_input_dispose (GObject * object)
{
  GstGLVideoMixerInput *self = (GstGLVideoMixerInput *) object;

  gst_clear_object (&self->mixer_pad);
}

static GstGhostPad *
_create_video_mixer_input (GstGLMixerBin * self, GstPad * mixer_pad)
{
  GstGLVideoMixerInput *input =
      g_object_new (gst_gl_video_mixer_input_get_type (), "name",
      GST_OBJECT_NAME (mixer_pad), "direction", GST_PAD_DIRECTION (mixer_pad),
      NULL);

#define ADD_BINDING(obj,ref,prop) \
    gst_object_add_control_binding (GST_OBJECT (obj), \
        gst_proxy_control_binding_new (GST_OBJECT (obj), prop, \
            GST_OBJECT (ref), prop));
  ADD_BINDING (mixer_pad, input, "zorder");
  ADD_BINDING (mixer_pad, input, "xpos");
  ADD_BINDING (mixer_pad, input, "ypos");
  ADD_BINDING (mixer_pad, input, "width");
  ADD_BINDING (mixer_pad, input, "height");
  ADD_BINDING (mixer_pad, input, "alpha");
  ADD_BINDING (mixer_pad, input, "blend-equation-rgb");
  ADD_BINDING (mixer_pad, input, "blend-equation-alpha");
  ADD_BINDING (mixer_pad, input, "blend-function-src-rgb");
  ADD_BINDING (mixer_pad, input, "blend-function-src-alpha");
  ADD_BINDING (mixer_pad, input, "blend-function-dst-rgb");
  ADD_BINDING (mixer_pad, input, "blend-function-dst-alpha");
  ADD_BINDING (mixer_pad, input, "blend-constant-color-red");
  ADD_BINDING (mixer_pad, input, "blend-constant-color-green");
  ADD_BINDING (mixer_pad, input, "blend-constant-color-blue");
  ADD_BINDING (mixer_pad, input, "blend-constant-color-alpha");
  ADD_BINDING (mixer_pad, input, "sizing-policy");
  ADD_BINDING (mixer_pad, input, "xalign");
  ADD_BINDING (mixer_pad, input, "yalign");
#undef ADD_BINDING

  input->mixer_pad = gst_object_ref (mixer_pad);

  return GST_GHOST_PAD (input);
}

enum
{
  PROP_BIN_0,
  PROP_BIN_BACKGROUND,
};
#define DEFAULT_BACKGROUND GST_GL_VIDEO_MIXER_BACKGROUND_CHECKER

static void gst_gl_video_mixer_bin_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_gl_video_mixer_bin_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);

typedef GstGLMixerBin GstGLVideoMixerBin;
typedef GstGLMixerBinClass GstGLVideoMixerBinClass;

G_DEFINE_TYPE (GstGLVideoMixerBin, gst_gl_video_mixer_bin,
    GST_TYPE_GL_MIXER_BIN);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glvideomixer, "glvideomixer",
    GST_RANK_NONE, gst_gl_video_mixer_bin_get_type (),
    gl_element_init (plugin));

static void
gst_gl_video_mixer_bin_init (GstGLVideoMixerBin * self)
{
}

static void
gst_gl_video_mixer_bin_constructed (GObject * self)
{
  GstGLMixerBin *mix_bin = GST_GL_MIXER_BIN (self);

  gst_gl_mixer_bin_finish_init_with_element (mix_bin,
      g_object_new (GST_TYPE_GL_VIDEO_MIXER,
          "force-live", mix_bin->force_live,
          "latency", mix_bin->latency,
          "start-time-selection", mix_bin->start_time_selection,
          "start-time", mix_bin->start_time,
          "min-upstream-latency", mix_bin->min_upstream_latency, NULL));
}

static void
gst_gl_video_mixer_bin_class_init (GstGLVideoMixerBinClass * klass)
{
  GstGLMixerBinClass *mixer_class = GST_GL_MIXER_BIN_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstCaps *upload_caps;

  mixer_class->create_input_pad = _create_video_mixer_input;

  gobject_class->constructed = gst_gl_video_mixer_bin_constructed;
  gobject_class->set_property = gst_gl_video_mixer_bin_set_property;
  gobject_class->get_property = gst_gl_video_mixer_bin_get_property;

  g_object_class_install_property (gobject_class, PROP_BIN_BACKGROUND,
      g_param_spec_enum ("background", "Background", "Background type",
          GST_TYPE_GL_VIDEO_MIXER_BACKGROUND,
          DEFAULT_BACKGROUND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* override the sink_%u pad template from GstGLMixerBin.
   * We pass it the GType of our sink pad so it's properly documented when
   * inspecting the element. */
  upload_caps = gst_gl_upload_get_input_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new_with_gtype ("sink_%u", GST_PAD_SINK, GST_PAD_REQUEST,
          upload_caps, gst_gl_video_mixer_input_get_type ()));
  gst_caps_unref (upload_caps);

  gst_element_class_set_metadata (element_class, "OpenGL video_mixer bin",
      "Bin/Filter/Effect/Video/Compositor", "OpenGL video_mixer bin",
      "Matthew Waters <matthew@centricular.com>");

  gst_type_mark_as_plugin_api (gst_gl_video_mixer_input_get_type (), 0);
}

static void
gst_gl_video_mixer_bin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLMixerBin *self = GST_GL_MIXER_BIN (object);

  if (self->mixer)
    g_object_get_property (G_OBJECT (self->mixer), pspec->name, value);
}

static void
gst_gl_video_mixer_bin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLMixerBin *self = GST_GL_MIXER_BIN (object);

  if (self->mixer)
    g_object_set_property (G_OBJECT (self->mixer), pspec->name, value);
}

enum
{
  PROP_0,
  PROP_BACKGROUND,
};

static void gst_gl_video_mixer_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define DEBUG_INIT \
    GST_DEBUG_CATEGORY_INIT (gst_gl_video_mixer_debug, "glvideomixer", 0, "glvideomixer element");

#define gst_gl_video_mixer_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLVideoMixer, gst_gl_video_mixer, GST_TYPE_GL_MIXER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_gl_video_mixer_child_proxy_init); DEBUG_INIT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glvideomixerelement,
    "glvideomixerelement", GST_RANK_NONE, gst_gl_video_mixer_get_type (),
    gl_element_init (plugin));

static void gst_gl_video_mixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_video_mixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *_update_caps (GstVideoAggregator * vagg, GstCaps * caps);
static GstCaps *_fixate_caps (GstAggregator * agg, GstCaps * caps);
static gboolean gst_gl_video_mixer_propose_allocation (GstAggregator *
    agg, GstAggregatorPad * agg_pad, GstQuery * decide_query, GstQuery * query);
static gboolean gst_gl_video_mixer_gl_start (GstGLBaseMixer * base_mix);
static void gst_gl_video_mixer_gl_stop (GstGLBaseMixer * base_mix);

static gboolean gst_gl_video_mixer_process_textures (GstGLMixer * mixer,
    GstGLMemory * out_tex);
static gboolean gst_gl_video_mixer_callback (gpointer stuff);

/* *INDENT-OFF* */

/* fragment source */
static const gchar *video_mixer_f_src =
    "uniform sampler2D texture;                     \n"
    "uniform float alpha;\n"
    "varying vec2 v_texcoord;                            \n"
    "void main()                                         \n"
    "{                                                   \n"
    "  vec4 rgba = texture2D(texture, v_texcoord);\n"
    "  gl_FragColor = vec4(rgba.rgb, rgba.a * alpha);\n"
    "}                                                   \n";

/* checker vertex source */
static const gchar *checker_v_src =
    "attribute vec4 a_position;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = a_position;\n"
    "}\n";

/* checker fragment source */
static const gchar *checker_f_src =
    "const float blocksize = 8.0;\n"
    "void main ()\n"
    "{\n"
    "  vec4 high = vec4(0.667, 0.667, 0.667, 1.0);\n"
    "  vec4 low = vec4(0.333, 0.333, 0.333, 1.0);\n"
    "  if (mod(gl_FragCoord.x, blocksize * 2.0) >= blocksize) {\n"
    "    if (mod(gl_FragCoord.y, blocksize * 2.0) >= blocksize)\n"
    "      gl_FragColor = low;\n"
    "    else\n"
    "      gl_FragColor = high;\n"
    "  } else {\n"
    "    if (mod(gl_FragCoord.y, blocksize * 2.0) < blocksize)\n"
    "      gl_FragColor = low;\n"
    "    else\n"
    "      gl_FragColor = high;\n"
    "  }\n"
    "}\n";
/* *INDENT-ON* */

#define GST_TYPE_GL_VIDEO_MIXER_PAD (gst_gl_video_mixer_pad_get_type())
#define GST_GL_VIDEO_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_VIDEO_MIXER_PAD, GstGLVideoMixerPad))
#define GST_GL_VIDEO_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_VIDEO_MIXER_PAD, GstGLVideoMixerPadClass))
#define GST_IS_GL_VIDEO_MIXER_PAD(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_VIDEO_MIXER_PAD))
#define GST_IS_GL_VIDEO_MIXER_PAD_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_VIDEO_MIXER_PAD))

typedef struct _GstGLVideoMixerPad GstGLVideoMixerPad;
typedef struct _GstGLVideoMixerPadClass GstGLVideoMixerPadClass;
typedef struct _GstGLVideoMixerCollect GstGLVideoMixerCollect;

/**
 * GstGLVideoMixerPad:
 *
 * The opaque #GstGLVideoMixerPad structure.
 */
struct _GstGLVideoMixerPad
{
  GstGLMixerPad parent;

  /* properties */
  gint xpos, ypos;
  gint width, height;
  gdouble xalign, yalign;
  gdouble alpha;
  GstGLVideoMixerSizingPolicy sizing_policy;

  GstGLVideoMixerBlendEquation blend_equation_rgb;
  GstGLVideoMixerBlendEquation blend_equation_alpha;
  GstGLVideoMixerBlendFunction blend_function_src_rgb;
  GstGLVideoMixerBlendFunction blend_function_src_alpha;
  GstGLVideoMixerBlendFunction blend_function_dst_rgb;
  GstGLVideoMixerBlendFunction blend_function_dst_alpha;
  gdouble blend_constant_color_red;
  gdouble blend_constant_color_green;
  gdouble blend_constant_color_blue;
  gdouble blend_constant_color_alpha;

  gint crop_left, crop_right;
  gint crop_top, crop_bottom;

  gboolean geometry_change;
  GLuint vertex_buffer;
  gfloat m_matrix[16];
};

struct _GstGLVideoMixerPadClass
{
  GstGLMixerPadClass parent_class;
};

GType gst_gl_video_mixer_pad_get_type (void);
G_DEFINE_TYPE (GstGLVideoMixerPad, gst_gl_video_mixer_pad,
    GST_TYPE_GL_MIXER_PAD);

static void gst_gl_video_mixer_pad_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gl_video_mixer_pad_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

enum
{
  PROP_PAD_0,
  PROP_PAD_XPOS,
  PROP_PAD_YPOS,
  PROP_PAD_WIDTH,
  PROP_PAD_HEIGHT,
  PROP_PAD_ALPHA,
  PROP_PAD_BLEND_EQUATION_RGB,
  PROP_PAD_BLEND_EQUATION_ALPHA,
  PROP_PAD_BLEND_FUNCTION_SRC_RGB,
  PROP_PAD_BLEND_FUNCTION_SRC_ALPHA,
  PROP_PAD_BLEND_FUNCTION_DST_RGB,
  PROP_PAD_BLEND_FUNCTION_DST_ALPHA,
  PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_RED,
  PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_GREEN,
  PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_BLUE,
  PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA,
  PROP_PAD_CROP_LEFT,
  PROP_PAD_CROP_RIGHT,
  PROP_PAD_CROP_TOP,
  PROP_PAD_CROP_BOTTOM,
  PROP_PAD_SIZING_POLICY,
  PROP_PAD_XALIGN,
  PROP_PAD_YALIGN,
};

static void
gst_gl_video_mixer_pad_init (GstGLVideoMixerPad * pad)
{
  pad->alpha = DEFAULT_PAD_ALPHA;
  pad->blend_equation_rgb = DEFAULT_PAD_BLEND_EQUATION_RGB;
  pad->blend_equation_alpha = DEFAULT_PAD_BLEND_EQUATION_ALPHA;
  pad->blend_function_src_rgb = DEFAULT_PAD_BLEND_FUNCTION_SRC_RGB;
  pad->blend_function_src_alpha = DEFAULT_PAD_BLEND_FUNCTION_SRC_ALPHA;
  pad->blend_function_dst_rgb = DEFAULT_PAD_BLEND_FUNCTION_DST_RGB;
  pad->blend_function_dst_alpha = DEFAULT_PAD_BLEND_FUNCTION_DST_ALPHA;
  pad->sizing_policy = DEFAULT_PAD_SIZING_POLICY;
  pad->xalign = DEFAULT_PAD_XALIGN;
  pad->yalign = DEFAULT_PAD_YALIGN;
  memset (pad->m_matrix, 0, sizeof (gfloat) * 4 * 4);
  pad->m_matrix[0] = 1.0;
  pad->m_matrix[5] = 1.0;
  pad->m_matrix[10] = 1.0;
  pad->m_matrix[15] = 1.0;
}

static void
gst_gl_video_mixer_pad_class_init (GstGLVideoMixerPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_gl_video_mixer_pad_set_property;
  gobject_class->get_property = gst_gl_video_mixer_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_XPOS,
      g_param_spec_int ("xpos", "X Position", "X Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_XPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_YPOS,
      g_param_spec_int ("ypos", "Y Position", "Y Position of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_YPOS,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_WIDTH,
      g_param_spec_int ("width", "Width", "Width of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_WIDTH,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_HEIGHT,
      g_param_spec_int ("height", "Height", "Height of the picture",
          G_MININT, G_MAXINT, DEFAULT_PAD_HEIGHT,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_ALPHA,
      g_param_spec_double ("alpha", "Alpha", "Alpha of the picture", 0.0, 1.0,
          DEFAULT_PAD_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_INPUT_BLEND_EQUATION_RGB,
      g_param_spec_enum ("blend-equation-rgb", "Blend Equation RGB",
          "Blend Equation for RGB",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION,
          DEFAULT_PAD_BLEND_EQUATION_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_EQUATION_ALPHA,
      g_param_spec_enum ("blend-equation-alpha", "Blend Equation Alpha",
          "Blend Equation for Alpha", GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION,
          DEFAULT_PAD_BLEND_EQUATION_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_SRC_RGB,
      g_param_spec_enum ("blend-function-src-rgb", "Blend Function Source RGB",
          "Blend Function for Source RGB",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_SRC_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_SRC_ALPHA,
      g_param_spec_enum ("blend-function-src-alpha",
          "Blend Function Source Alpha", "Blend Function for Source Alpha",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_SRC_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_DST_RGB,
      g_param_spec_enum ("blend-function-dst-rgb",
          "Blend Function Destination RGB",
          "Blend Function for Destination RGB",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_DST_RGB,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_INPUT_BLEND_FUNCTION_DST_ALPHA,
      g_param_spec_enum ("blend-function-dst-alpha",
          "Blend Function Destination Alpha",
          "Blend Function for Destination Alpha",
          GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION,
          DEFAULT_PAD_BLEND_FUNCTION_DST_ALPHA,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_RED,
      g_param_spec_double ("blend-constant-color-red",
          "Blend Constant Color Red", "Blend Constant Color Red", 0.0, 1.0, 0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_GREEN,
      g_param_spec_double ("blend-constant-color-green",
          "Blend Constant Color Green", "Blend Constant Color Green", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_BLUE,
      g_param_spec_double ("blend-constant-color-blue",
          "Blend Constant Color Green", "Blend Constant Color Green", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA,
      g_param_spec_double ("blend-constant-color-alpha",
          "Blend Constant Color Alpha", "Blend Constant Color Alpha", 0.0, 1.0,
          0.0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerPad:crop-left:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the left side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_LEFT,
      g_param_spec_int ("crop-left", "Crop Left", "Crop left of the picture", 0,
          G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerPad:crop-right:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the right side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_RIGHT,
      g_param_spec_int ("crop-right", "Crop Right", "Crop right of the picture",
          0, G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerPad:crop-top:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the top side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_TOP,
      g_param_spec_int ("crop-top", "Crop Top", "Crop top of the picture", 0,
          G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  /**
   * GstGLVideoMixerPad:crop-bottom:
   *
   * Defines how many pixels of the input in input size should be cropped on
   * the bottom side.
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_PAD_CROP_BOTTOM,
      g_param_spec_int ("crop-bottom", "Crop Bottom",
          "Crop bottom of the picture", 0, G_MAXINT, DEFAULT_PAD_CROP,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerPad:sizing-policy:
   *
   * Specifies sizing policy to use. Depending on selected sizing policy,
   * scaled image might not fully cover the configured target rectangle area
   * (e.g., "keep-aspect-ratio"). In that case, any uncovered area will be
   * filled with background unless the uncovered area is drawn by other image.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_PAD_SIZING_POLICY,
      g_param_spec_enum ("sizing-policy", "Sizing policy",
          "Sizing policy to use for image scaling",
          GST_TYPE_GL_VIDEO_MIXER_SIZING_POLICY, DEFAULT_PAD_SIZING_POLICY,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerPad:xalign:
   *
   * Defines the alignment of the input within the available horizontal space,
   * relative to #GstGLVideoMixerPad:width and #GstGLVideoMixerPad:height.
   * Values range from 0.0 (left) to 1.0 (right).
   *
   * The image is aligned in the available space as if the pivot point is
   * matching the alignment. For example, setting the `xalign` property to 0.0
   * will align the left edge of the image with the left edge of the bounding
   * box; 0.5 aligns the horizontal center of the image with the horizontal
   * center of the bounding box; 1.0 aligns the right edge of the image with the
   * right edge of the bounding box; and so it goes.
   *
   * This property is only effective when #GstGLVideoMixerInput:sizing-policy
   * is set to 'keep-aspect-ratio'.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_PAD_XALIGN,
      g_param_spec_double ("xalign", "X alignment",
          "X alignment of the picture", 0.0, 1.0, DEFAULT_PAD_XALIGN,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstGLVideoMixerPad:yalign:
   *
   * Defines the alignment of the input within the available vertical space,
   * relative to #GstGLVideoMixerPad:width and #GstGLVideoMixerPad:height.
   * Values range from 0.0 (top) to 1.0 (bottom).
   *
   * The image is aligned in the available space as if the pivot point is
   * matching the alignment. For example, setting the `yalign` property to 0.0
   * will align the top edge of the image with the top edge of the bounding box;
   * 0.5 aligns the vertical center of the image with the vertical center of the
   * bounding box; 1.0 aligns the bottom edge of the image with the bottom edge
   * of the bounding box; and so it goes.
   *
   * This property is only effective when #GstGLVideoMixerInput:sizing-policy
   * is set to 'keep-aspect-ratio'.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_PAD_YALIGN,
      g_param_spec_double ("yalign", "Y alignment",
          "Y alignment of the picture", 0.0, 1.0, DEFAULT_PAD_YALIGN,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_gl_video_mixer_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixerPad *pad = GST_GL_VIDEO_MIXER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_XPOS:
      g_value_set_int (value, pad->xpos);
      break;
    case PROP_PAD_YPOS:
      g_value_set_int (value, pad->ypos);
      break;
    case PROP_PAD_WIDTH:
      g_value_set_int (value, pad->width);
      break;
    case PROP_PAD_HEIGHT:
      g_value_set_int (value, pad->height);
      break;
    case PROP_PAD_ALPHA:
      g_value_set_double (value, pad->alpha);
      break;
    case PROP_PAD_BLEND_EQUATION_RGB:
      g_value_set_enum (value, pad->blend_equation_rgb);
      break;
    case PROP_PAD_BLEND_EQUATION_ALPHA:
      g_value_set_enum (value, pad->blend_equation_alpha);
      break;
    case PROP_PAD_BLEND_FUNCTION_SRC_RGB:
      g_value_set_enum (value, pad->blend_function_src_rgb);
      break;
    case PROP_PAD_BLEND_FUNCTION_SRC_ALPHA:
      g_value_set_enum (value, pad->blend_function_src_alpha);
      break;
    case PROP_PAD_BLEND_FUNCTION_DST_RGB:
      g_value_set_enum (value, pad->blend_function_dst_rgb);
      break;
    case PROP_PAD_BLEND_FUNCTION_DST_ALPHA:
      g_value_set_enum (value, pad->blend_function_dst_alpha);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_RED:
      g_value_set_double (value, pad->blend_constant_color_red);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_GREEN:
      g_value_set_double (value, pad->blend_constant_color_green);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_BLUE:
      g_value_set_double (value, pad->blend_constant_color_blue);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA:
      g_value_set_double (value, pad->blend_constant_color_alpha);
      break;
    case PROP_PAD_CROP_LEFT:
      g_value_set_int (value, pad->crop_left);
      break;
    case PROP_PAD_CROP_RIGHT:
      g_value_set_int (value, pad->crop_right);
      break;
    case PROP_PAD_CROP_TOP:
      g_value_set_int (value, pad->crop_top);
      break;
    case PROP_PAD_CROP_BOTTOM:
      g_value_set_int (value, pad->crop_bottom);
      break;
    case PROP_PAD_SIZING_POLICY:
      g_value_set_enum (value, pad->sizing_policy);
      break;
    case PROP_PAD_XALIGN:
      g_value_set_double (value, pad->xalign);
      break;
    case PROP_PAD_YALIGN:
      g_value_set_double (value, pad->yalign);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_video_mixer_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixerPad *pad = GST_GL_VIDEO_MIXER_PAD (object);
  GstGLMixer *mix = GST_GL_MIXER (gst_pad_get_parent (GST_PAD (pad)));

  GST_OBJECT_LOCK (pad);
  switch (prop_id) {
    case PROP_PAD_XPOS:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->xpos;
      pad->xpos = val;
      break;
    }
    case PROP_PAD_YPOS:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->ypos;
      pad->ypos = val;
      break;
    }
    case PROP_PAD_WIDTH:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->width;
      pad->width = val;
      break;
    }
    case PROP_PAD_HEIGHT:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->height;
      pad->height = val;
    }
      break;
    case PROP_PAD_ALPHA:
      pad->alpha = g_value_get_double (value);
      break;
    case PROP_PAD_BLEND_EQUATION_RGB:
      pad->blend_equation_rgb = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_EQUATION_ALPHA:
      pad->blend_equation_alpha = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_SRC_RGB:
      pad->blend_function_src_rgb = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_SRC_ALPHA:
      pad->blend_function_src_alpha = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_DST_RGB:
      pad->blend_function_dst_rgb = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_DST_ALPHA:
      pad->blend_function_dst_alpha = g_value_get_enum (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_RED:
      pad->blend_constant_color_red = g_value_get_double (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_GREEN:
      pad->blend_constant_color_green = g_value_get_double (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_BLUE:
      pad->blend_constant_color_blue = g_value_get_double (value);
      break;
    case PROP_PAD_BLEND_FUNCTION_CONSTANT_COLOR_ALPHA:
      pad->blend_constant_color_alpha = g_value_get_double (value);
      break;
    case PROP_PAD_CROP_LEFT:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->crop_left;
      pad->crop_left = val;
      break;
    }
    case PROP_PAD_CROP_RIGHT:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->crop_right;
      pad->crop_right = val;
      break;
    }
    case PROP_PAD_CROP_TOP:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->crop_top;
      pad->crop_top = val;
      break;
    }
    case PROP_PAD_CROP_BOTTOM:{
      gint val = g_value_get_int (value);
      pad->geometry_change |= val != pad->crop_bottom;
      pad->crop_bottom = val;
      break;
    }
    case PROP_PAD_SIZING_POLICY:{
      GstGLVideoMixerSizingPolicy val = g_value_get_enum (value);
      pad->geometry_change |= val != pad->sizing_policy;
      pad->sizing_policy = val;
      break;
    }
    case PROP_PAD_XALIGN:{
      gdouble val = g_value_get_double (value);
      pad->geometry_change |= !G_APPROX_VALUE (val, pad->xalign, DBL_EPSILON);
      pad->xalign = val;
      break;
    }
    case PROP_PAD_YALIGN:{
      gdouble val = g_value_get_double (value);
      pad->geometry_change |= !G_APPROX_VALUE (val, pad->yalign, DBL_EPSILON);
      pad->yalign = val;
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (pad);

  gst_object_unref (mix);
}

static void
_del_buffer (GstGLContext * context, GLuint * pBuffer)
{
  context->gl_vtable->DeleteBuffers (1, pBuffer);
}

static GstPad *
gst_gl_video_mixer_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstPad *newpad;

  newpad = (GstPad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, req_name, caps);

  if (newpad == NULL)
    goto could_not_create;

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return newpad;

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add  pad");
    return NULL;
  }
}

static void
gst_gl_video_mixer_release_pad (GstElement * element, GstPad * p)
{
  GstGLVideoMixerPad *pad = GST_GL_VIDEO_MIXER_PAD (p);

  gst_child_proxy_child_removed (GST_CHILD_PROXY (element), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  /* we call the base class first as this will remove the pad from
   * the aggregator, thus stopping misc callbacks from being called,
   * one of which (process_textures) will recreate the vertex_buffer
   * if it is destroyed.  Calling the parent may release the last ref to the pad
   * so we need to keep the pad alive for the follow up clean up */
  gst_object_ref (pad);
  GST_ELEMENT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (element)))
      ->release_pad (element, p);

  if (pad->vertex_buffer) {
    GstGLBaseMixer *mix = GST_GL_BASE_MIXER (element);
    gst_gl_context_thread_add (mix->context, (GstGLContextThreadFunc)
        _del_buffer, &pad->vertex_buffer);
    pad->vertex_buffer = 0;
  }
  gst_object_unref (pad);
}

static void
gst_gl_video_mixer_class_init (GstGLVideoMixerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;
  GstVideoAggregatorClass *vagg_class = (GstVideoAggregatorClass *) klass;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);
  element_class->request_new_pad = gst_gl_video_mixer_request_new_pad;
  element_class->release_pad = gst_gl_video_mixer_release_pad;

  gobject_class->set_property = gst_gl_video_mixer_set_property;
  gobject_class->get_property = gst_gl_video_mixer_get_property;

  gst_element_class_set_metadata (element_class, "OpenGL video_mixer",
      "Filter/Effect/Video/Compositor", "OpenGL video_mixer",
      "Matthew Waters <matthew@centricular.com>");

  gst_gl_mixer_class_add_rgba_pad_templates (GST_GL_MIXER_CLASS (klass));
  gst_element_class_add_static_pad_template_with_gtype (element_class,
      &sink_factory, GST_TYPE_GL_VIDEO_MIXER_PAD);

  g_object_class_install_property (gobject_class, PROP_BACKGROUND,
      g_param_spec_enum ("background", "Background", "Background type",
          GST_TYPE_GL_VIDEO_MIXER_BACKGROUND,
          DEFAULT_BACKGROUND, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_GL_MIXER_CLASS (klass)->process_textures =
      gst_gl_video_mixer_process_textures;

  GST_GL_BASE_MIXER_CLASS (klass)->gl_stop = gst_gl_video_mixer_gl_stop;
  GST_GL_BASE_MIXER_CLASS (klass)->gl_start = gst_gl_video_mixer_gl_start;

  vagg_class->update_caps = _update_caps;

  agg_class->src_event = gst_gl_video_mixer_src_event;
  agg_class->fixate_src_caps = _fixate_caps;
  agg_class->propose_allocation = gst_gl_video_mixer_propose_allocation;

  GST_GL_BASE_MIXER_CLASS (klass)->supported_gl_api =
      GST_GL_API_OPENGL | GST_GL_API_OPENGL3 | GST_GL_API_GLES2;

  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_MIXER_BACKGROUND, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_MIXER_PAD, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_MIXER_BLEND_EQUATION, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_MIXER_BLEND_FUNCTION, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_MIXER_SIZING_POLICY, 0);
}

static void
gst_gl_video_mixer_init (GstGLVideoMixer * video_mixer)
{
  video_mixer->background = DEFAULT_BACKGROUND;
  video_mixer->shader = NULL;
}

static void
gst_gl_video_mixer_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixer *mixer = GST_GL_VIDEO_MIXER (object);

  switch (prop_id) {
    case PROP_BACKGROUND:
      mixer->background = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_video_mixer_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLVideoMixer *mixer = GST_GL_VIDEO_MIXER (object);

  switch (prop_id) {
    case PROP_BACKGROUND:
      g_value_set_enum (value, mixer->background);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gl_video_mixer_propose_allocation (GstAggregator * agg,
    GstAggregatorPad * agg_pad, GstQuery * decide_query, GstQuery * query)
{
  if (!GST_AGGREGATOR_CLASS (parent_class)->propose_allocation (agg,
          agg_pad, decide_query, query))
    return FALSE;

  gst_query_add_allocation_meta (query,
      GST_VIDEO_AFFINE_TRANSFORMATION_META_API_TYPE, 0);

  return TRUE;
}

static void
align_rect (const GstVideoRectangle * src,
    const GstVideoRectangle * dst, GstVideoRectangle * result, gdouble xalign,
    gdouble yalign)
{
  gdouble src_ratio, dst_ratio;

  g_return_if_fail (src->h != 0);
  g_return_if_fail (dst->h != 0);

  src_ratio = (gdouble) src->w / src->h;
  dst_ratio = (gdouble) dst->w / dst->h;

  if (src_ratio > dst_ratio) {
    result->w = dst->w;
    result->h = dst->w / src_ratio;
    result->x = dst->x;
    result->y = dst->y + (dst->h - result->h) * yalign;
  } else if (src_ratio < dst_ratio) {
    result->w = dst->h * src_ratio;
    result->h = dst->h;
    result->x = dst->x + (dst->w - result->w) * xalign;
    result->y = dst->y;
  } else {
    result->x = dst->x;
    result->y = dst->y;
    result->w = dst->w;
    result->h = dst->h;
  }

  GST_DEBUG ("source is %dx%d dest is %dx%d, result is %dx%d with x,y %dx%d",
      src->w, src->h, dst->w, dst->h,
      result->w, result->h, result->x, result->y);
}

static void
_mixer_pad_get_output_size (GstGLVideoMixer * mix,
    GstGLVideoMixerPad * mix_pad, gint out_par_n, gint out_par_d, gint * width,
    gint * height, gint * x_offset, gint * y_offset)
{
  GstVideoAggregatorPad *vagg_pad = GST_VIDEO_AGGREGATOR_PAD (mix_pad);
  gint pad_width, pad_height;
  guint dar_n, dar_d;

  *x_offset = 0;
  *y_offset = 0;
  *width = 0;
  *height = 0;

  /* FIXME: Anything better we can do here? */
  if (!vagg_pad->info.finfo
      || vagg_pad->info.finfo->format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_DEBUG_OBJECT (mix_pad, "Have no caps yet");
    return;
  }

  if (mix_pad->width <= 0) {
    gint crop = mix_pad->crop_left + mix_pad->crop_right;

    if (GST_VIDEO_INFO_WIDTH (&vagg_pad->info) > crop)
      pad_width = GST_VIDEO_INFO_WIDTH (&vagg_pad->info) - crop;
    else
      pad_width = 0;
  } else {
    pad_width = mix_pad->width;
  }

  if (mix_pad->height <= 0) {
    gint crop = mix_pad->crop_top + mix_pad->crop_bottom;

    if (GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) > crop)
      pad_height = GST_VIDEO_INFO_HEIGHT (&vagg_pad->info) - crop;
    else
      pad_height = 0;
  } else {
    pad_height = mix_pad->height;
  }

  if (!gst_video_calculate_display_ratio (&dar_n, &dar_d, pad_width, pad_height,
          GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
          GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d)) {
    GST_WARNING_OBJECT (mix_pad, "Cannot calculate display aspect ratio");
    *width = *height = 0;
    return;
  }
  GST_LOG_OBJECT (mix_pad, "scaling %ux%u by %u/%u (%u/%u / %u/%u)", pad_width,
      pad_height, dar_n, dar_d, GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
      GST_VIDEO_INFO_PAR_D (&vagg_pad->info), out_par_n, out_par_d);

  switch (mix_pad->sizing_policy) {
    case GST_GL_VIDEO_MIXER_SIZING_POLICY_NONE:
      /* Pick either height or width, whichever is an integer multiple of the
       * display aspect ratio. However, prefer preserving the height to account
       * for interlaced video. */
      if (pad_height % dar_n == 0) {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      } else if (pad_width % dar_d == 0) {
        pad_height = gst_util_uint64_scale_int (pad_width, dar_d, dar_n);
      } else {
        pad_width = gst_util_uint64_scale_int (pad_height, dar_n, dar_d);
      }
      break;
    case GST_GL_VIDEO_MIXER_SIZING_POLICY_KEEP_ASPECT_RATIO:
    {
      gint from_dar_n, from_dar_d, to_dar_n, to_dar_d, num, den;

      /* Calculate DAR again with actual video size */
      if (!gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (&vagg_pad->info),
              GST_VIDEO_INFO_HEIGHT (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_N (&vagg_pad->info),
              GST_VIDEO_INFO_PAR_D (&vagg_pad->info), &from_dar_n,
              &from_dar_d)) {
        from_dar_n = from_dar_d = -1;
      }

      if (!gst_util_fraction_multiply (pad_width, pad_height,
              out_par_n, out_par_d, &to_dar_n, &to_dar_d)) {
        to_dar_n = to_dar_d = -1;
      }

      if (from_dar_n != to_dar_n || from_dar_d != to_dar_d) {
        /* Calculate new output resolution */
        if (from_dar_n != -1 && from_dar_d != -1
            && gst_util_fraction_multiply (from_dar_n, from_dar_d,
                out_par_d, out_par_n, &num, &den)) {
          GstVideoRectangle src_rect, dst_rect;
          GstVideoRectangle rst_rect = { 0, 0, 0, 0 };

          src_rect.h = gst_util_uint64_scale_int (pad_width, den, num);
          if (src_rect.h == 0) {
            pad_width = 0;
            pad_height = 0;
            break;
          }

          src_rect.x = src_rect.y = 0;
          src_rect.w = pad_width;

          dst_rect.x = dst_rect.y = 0;
          dst_rect.w = pad_width;
          dst_rect.h = pad_height;

          /* Scale rect to be centered in destination rect */
          align_rect (&src_rect, &dst_rect, &rst_rect, mix_pad->xalign,
              mix_pad->yalign);

          GST_LOG_OBJECT (mix_pad,
              "Re-calculated size %dx%d -> %dx%d (x-offset %d, y-offset %d)",
              pad_width, pad_height, rst_rect.w, rst_rect.h, rst_rect.x,
              rst_rect.h);

          *x_offset = rst_rect.x;
          *y_offset = rst_rect.y;
          pad_width = rst_rect.w;
          pad_height = rst_rect.h;
        } else {
          GST_WARNING_OBJECT (mix_pad, "Failed to calculate output size");

          *x_offset = 0;
          *y_offset = 0;
          pad_width = 0;
          pad_height = 0;
        }
      }
      break;
    }
  }

  *width = pad_width;
  *height = pad_height;
}

static GstCaps *
_update_caps (GstVideoAggregator * vagg, GstCaps * caps)
{
  GstCaps *template_caps, *ret;
  GList *l;

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *vaggpad = l->data;

    if (!vaggpad->info.finfo)
      continue;

    if (GST_VIDEO_INFO_FORMAT (&vaggpad->info) == GST_VIDEO_FORMAT_UNKNOWN)
      continue;

    if (GST_VIDEO_INFO_MULTIVIEW_MODE (&vaggpad->info) !=
        GST_VIDEO_MULTIVIEW_MODE_NONE
        && GST_VIDEO_INFO_MULTIVIEW_MODE (&vaggpad->info) !=
        GST_VIDEO_MULTIVIEW_MODE_MONO) {
      GST_FIXME_OBJECT (vaggpad, "Multiview support is not implemented yet");
      GST_OBJECT_UNLOCK (vagg);
      return NULL;
    }
  }

  GST_OBJECT_UNLOCK (vagg);

  template_caps = gst_pad_get_pad_template_caps (GST_AGGREGATOR_SRC_PAD (vagg));
  ret = gst_caps_intersect (caps, template_caps);

  return ret;
}

static GstCaps *
_fixate_caps (GstAggregator * agg, GstCaps * caps)
{
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (agg);
  GstGLVideoMixer *mix = GST_GL_VIDEO_MIXER (vagg);
  gint best_width = 0, best_height = 0;
  gint best_fps_n = 0, best_fps_d = 0;
  gint par_n, par_d;
  gdouble best_fps = 0.;
  GstCaps *ret = NULL;
  GstStructure *s;
  GList *l;

  ret = gst_caps_make_writable (caps);

  /* we need this to calculate how large to make the output frame */
  s = gst_caps_get_structure (ret, 0);
  if (!gst_structure_has_field (s, "pixel-aspect-ratio")) {
    gst_structure_set (s, "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, NULL);
  }
  gst_structure_fixate_field_nearest_fraction (s, "pixel-aspect-ratio", 1, 1);
  gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d);

  GST_OBJECT_LOCK (vagg);
  for (l = GST_ELEMENT (vagg)->sinkpads; l; l = l->next) {
    GstVideoAggregatorPad *vaggpad = l->data;
    GstGLVideoMixerPad *mixer_pad = GST_GL_VIDEO_MIXER_PAD (vaggpad);
    gint this_width, this_height;
    gint width, height;
    gint offset_x, offset_y;
    gint fps_n, fps_d;
    gdouble cur_fps;

    fps_n = GST_VIDEO_INFO_FPS_N (&vaggpad->info);
    fps_d = GST_VIDEO_INFO_FPS_D (&vaggpad->info);
    _mixer_pad_get_output_size (mix, mixer_pad, par_n, par_d, &width, &height,
        &offset_x, &offset_y);

    if (width == 0 || height == 0)
      continue;

    this_width = width + MAX (mixer_pad->xpos + offset_x, 0);
    this_height = height + MAX (mixer_pad->ypos + offset_y, 0);

    if (best_width < this_width)
      best_width = this_width;
    if (best_height < this_height)
      best_height = this_height;

    if (fps_d == 0)
      cur_fps = 0.0;
    else
      gst_util_fraction_to_double (fps_n, fps_d, &cur_fps);

    if (best_fps < cur_fps) {
      best_fps = cur_fps;
      best_fps_n = fps_n;
      best_fps_d = fps_d;
    }
  }
  GST_OBJECT_UNLOCK (vagg);

  if (best_fps_n <= 0 || best_fps_d <= 0 || best_fps == 0.0) {
    best_fps_n = 25;
    best_fps_d = 1;
    best_fps = 25.0;
  }

  s = gst_caps_get_structure (ret, 0);
  gst_structure_fixate_field_nearest_int (s, "width", best_width);
  gst_structure_fixate_field_nearest_int (s, "height", best_height);
  gst_structure_fixate_field_nearest_fraction (s, "framerate", best_fps_n,
      best_fps_d);
  ret = gst_caps_fixate (ret);

  return ret;
}

static gboolean
_reset_pad_gl (GstElement * agg, GstPad * aggpad, gpointer udata)
{
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (agg)->context->gl_vtable;
  GstGLVideoMixerPad *pad = GST_GL_VIDEO_MIXER_PAD (aggpad);

  if (pad->vertex_buffer) {
    gl->DeleteBuffers (1, &pad->vertex_buffer);
    pad->vertex_buffer = 0;
  }

  return TRUE;
}

static void
_reset_gl (GstGLContext * context, GstGLVideoMixer * video_mixer)
{
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (video_mixer)->context->gl_vtable;

  if (video_mixer->vao) {
    gl->DeleteVertexArrays (1, &video_mixer->vao);
    video_mixer->vao = 0;
  }

  if (video_mixer->vbo_indices) {
    gl->DeleteBuffers (1, &video_mixer->vbo_indices);
    video_mixer->vbo_indices = 0;
  }

  if (video_mixer->checker_vbo) {
    gl->DeleteBuffers (1, &video_mixer->checker_vbo);
    video_mixer->checker_vbo = 0;
  }

  gst_element_foreach_sink_pad (GST_ELEMENT (video_mixer), _reset_pad_gl, NULL);
}

static gboolean
is_point_contained (const GstVideoRectangle rect, const gint px, const gint py)
{
  if ((px >= rect.x) && (px <= rect.x + rect.w) &&
      (py >= rect.y) && (py <= rect.y + rect.h))
    return TRUE;
  return FALSE;
}

static gboolean
src_pad_mouse_event (GstElement * element, GstPad * pad, gpointer user_data)
{
  GstGLVideoMixer *mix = GST_GL_VIDEO_MIXER (element);
  GstGLVideoMixerPad *mix_pad = GST_GL_VIDEO_MIXER_PAD (pad);
  GstCaps *caps = gst_pad_get_current_caps (pad);
  GstStructure *event_st, *caps_st;
  gint par_n = 1, par_d = 1;
  gdouble event_x, event_y;
  GstVideoRectangle rect;

  event_st =
      gst_structure_copy (gst_event_get_structure (GST_EVENT_CAST (user_data)));
  caps_st = gst_structure_copy (gst_caps_get_structure (caps, 0));

  gst_structure_get (event_st, "pointer_x", G_TYPE_DOUBLE, &event_x,
      "pointer_y", G_TYPE_DOUBLE, &event_y, NULL);

  /* Find output rectangle of this pad */
  gst_structure_get_fraction (caps_st, "pixel-aspect-ratio", &par_n, &par_d);
  _mixer_pad_get_output_size (mix, mix_pad, par_n, par_d, &(rect.w), &(rect.h),
      &(rect.x), &(rect.y));
  rect.x += mix_pad->xpos;
  rect.y += mix_pad->ypos;

  /* Translate coordinates and send event if it lies in this rectangle */
  if (is_point_contained (rect, event_x, event_y)) {
    GstVideoAggregatorPad *vpad = GST_VIDEO_AGGREGATOR_PAD_CAST (mix_pad);
    gdouble w, h, x, y;

    w = (gdouble) GST_VIDEO_INFO_WIDTH (&vpad->info);
    h = (gdouble) GST_VIDEO_INFO_HEIGHT (&vpad->info);
    x = (event_x - (gdouble) rect.x) * (w / (gdouble) rect.w);
    y = (event_y - (gdouble) rect.y) * (h / (gdouble) rect.h);

    gst_structure_set (event_st, "pointer_x", G_TYPE_DOUBLE, x,
        "pointer_y", G_TYPE_DOUBLE, y, NULL);
    gst_pad_push_event (pad, gst_event_new_navigation (event_st));
  } else {
    gst_structure_free (event_st);
  }

  gst_structure_free (caps_st);
  return TRUE;
}

static gboolean
gst_gl_video_mixer_src_event (GstAggregator * agg, GstEvent * event)
{
  GstNavigationEventType event_type;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NAVIGATION:
    {
      event_type = gst_navigation_event_get_type (event);
      switch (event_type) {
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_PRESS:
        case GST_NAVIGATION_EVENT_MOUSE_BUTTON_RELEASE:
        case GST_NAVIGATION_EVENT_MOUSE_MOVE:
        case GST_NAVIGATION_EVENT_MOUSE_SCROLL:
          gst_element_foreach_sink_pad (GST_ELEMENT_CAST (agg),
              src_pad_mouse_event, event);
          gst_event_unref (event);
          return FALSE;

        default:
          break;
      }
    }

    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->src_event (agg, event);
}

static void
gst_gl_video_mixer_gl_stop (GstGLBaseMixer * base_mix)
{
  GstGLVideoMixer *video_mixer = GST_GL_VIDEO_MIXER (base_mix);

  gst_clear_object (&video_mixer->shader);
  gst_clear_object (&video_mixer->checker);

  _reset_gl (base_mix->context, video_mixer);

  GST_GL_BASE_MIXER_CLASS (parent_class)->gl_stop (base_mix);
}

static gboolean
gst_gl_video_mixer_gl_start (GstGLBaseMixer * base_mix)
{
  GstGLVideoMixer *video_mixer = GST_GL_VIDEO_MIXER (base_mix);

  video_mixer->output_geo_change = TRUE;

  if (!video_mixer->shader) {
    gchar *frag_str = g_strdup_printf ("%s%s",
        gst_gl_shader_string_get_highest_precision (base_mix->context,
            GST_GLSL_VERSION_NONE,
            GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY),
        video_mixer_f_src);

    gst_gl_context_gen_shader (base_mix->context,
        gst_gl_shader_string_vertex_mat4_vertex_transform,
        frag_str, &video_mixer->shader);
    g_free (frag_str);
  }

  return GST_GL_BASE_MIXER_CLASS (parent_class)->gl_start (base_mix);
}

static void
_video_mixer_process_gl (GstGLContext * context, GstGLVideoMixer * video_mixer)
{
  GstGLMixer *mixer = GST_GL_MIXER (video_mixer);
  GstGLFramebuffer *fbo = gst_gl_mixer_get_framebuffer (mixer);

  gst_gl_framebuffer_draw_to_texture (fbo, video_mixer->out_tex,
      gst_gl_video_mixer_callback, video_mixer);

  gst_clear_object (&fbo);
}

static gboolean
gst_gl_video_mixer_process_textures (GstGLMixer * mix, GstGLMemory * out_tex)
{
  GstGLVideoMixer *video_mixer = GST_GL_VIDEO_MIXER (mix);
  GstGLContext *context = GST_GL_BASE_MIXER (mix)->context;

  video_mixer->out_tex = out_tex;

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _video_mixer_process_gl, video_mixer);

  return TRUE;
}

static const GLushort indices[] = { 0, 1, 2, 0, 2, 3 };

static void
_init_vbo_indices (GstGLVideoMixer * mixer)
{
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (mixer)->context->gl_vtable;

  if (!mixer->vbo_indices) {
    gl->GenBuffers (1, &mixer->vbo_indices);
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, mixer->vbo_indices);
    gl->BufferData (GL_ELEMENT_ARRAY_BUFFER, sizeof (indices), indices,
        GL_STATIC_DRAW);
  }
}

static gboolean
_draw_checker_background (GstGLVideoMixer * video_mixer)
{
  GstGLMixer *mixer = GST_GL_MIXER (video_mixer);
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (mixer)->context->gl_vtable;
  gint attr_position_loc = 0;

  /* *INDENT-OFF* */
  gfloat v_vertices[] = {
    -1.0,-1.0, 0.0f,
     1.0,-1.0, 0.0f,
     1.0, 1.0, 0.0f,
    -1.0, 1.0, 0.0f,
  };
  /* *INDENT-ON* */

  if (!video_mixer->checker) {
    gchar *frag_str;

    frag_str =
        g_strdup_printf ("%s%s",
        gst_gl_shader_string_get_highest_precision (GST_GL_BASE_MIXER
            (mixer)->context, GST_GLSL_VERSION_NONE,
            GST_GLSL_PROFILE_ES | GST_GLSL_PROFILE_COMPATIBILITY),
        checker_f_src);

    if (!gst_gl_context_gen_shader (GST_GL_BASE_MIXER (mixer)->context,
            checker_v_src, frag_str, &video_mixer->checker)) {
      g_free (frag_str);
      return FALSE;
    }
    g_free (frag_str);
  }

  gst_gl_shader_use (video_mixer->checker);
  attr_position_loc =
      gst_gl_shader_get_attribute_location (video_mixer->checker, "a_position");

  _init_vbo_indices (video_mixer);

  if (!video_mixer->checker_vbo) {
    gl->GenBuffers (1, &video_mixer->checker_vbo);
    gl->BindBuffer (GL_ARRAY_BUFFER, video_mixer->checker_vbo);
    gl->BufferData (GL_ARRAY_BUFFER, 4 * 3 * sizeof (GLfloat), v_vertices,
        GL_STATIC_DRAW);
  } else {
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, video_mixer->vbo_indices);
    gl->BindBuffer (GL_ARRAY_BUFFER, video_mixer->checker_vbo);
  }

  gl->VertexAttribPointer (attr_position_loc, 3, GL_FLOAT,
      GL_FALSE, 3 * sizeof (GLfloat), (void *) 0);

  gl->EnableVertexAttribArray (attr_position_loc);

  gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

  gl->DisableVertexAttribArray (attr_position_loc);
  gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
  gl->BindBuffer (GL_ARRAY_BUFFER, 0);

  return TRUE;
}

static gboolean
_draw_background (GstGLVideoMixer * video_mixer)
{
  GstGLMixer *mixer = GST_GL_MIXER (video_mixer);
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (mixer)->context->gl_vtable;

  switch (video_mixer->background) {
    case GST_GL_VIDEO_MIXER_BACKGROUND_BLACK:
      gl->ClearColor (0.0, 0.0, 0.0, 1.0);
      gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      break;
    case GST_GL_VIDEO_MIXER_BACKGROUND_WHITE:
      gl->ClearColor (1.0, 1.0, 1.0, 1.0);
      gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      break;
    case GST_GL_VIDEO_MIXER_BACKGROUND_TRANSPARENT:
      gl->ClearColor (0.0, 0.0, 0.0, 0.0);
      gl->Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      break;
    case GST_GL_VIDEO_MIXER_BACKGROUND_CHECKER:
      return _draw_checker_background (video_mixer);
      break;
    default:
      break;
  }

  return TRUE;
}

static guint
_blend_equation_to_gl (GstGLVideoMixerBlendEquation equation)
{
  switch (equation) {
    case GST_GL_VIDEO_MIXER_BLEND_EQUATION_ADD:
      return GL_FUNC_ADD;
    case GST_GL_VIDEO_MIXER_BLEND_EQUATION_SUBTRACT:
      return GL_FUNC_SUBTRACT;
    case GST_GL_VIDEO_MIXER_BLEND_EQUATION_REVERSE_SUBTRACT:
      return GL_FUNC_REVERSE_SUBTRACT;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static guint
_blend_function_to_gl (GstGLVideoMixerBlendFunction equation)
{
  switch (equation) {
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ZERO:
      return GL_ZERO;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE:
      return GL_ONE;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_COLOR:
      return GL_SRC_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_COLOR:
      return GL_ONE_MINUS_SRC_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_COLOR:
      return GL_DST_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_COLOR:
      return GL_ONE_MINUS_DST_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA:
      return GL_SRC_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_SRC_ALPHA:
      return GL_ONE_MINUS_SRC_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_DST_ALPHA:
      return GL_DST_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_DST_ALPHA:
      return GL_ONE_MINUS_DST_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_COLOR:
      return GL_CONSTANT_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_COLOR:
      return GL_ONE_MINUS_CONSTANT_COLOR;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_CONSTANT_ALPHA:
      return GL_CONSTANT_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_ONE_MINUS_CONSTANT_ALPHA:
      return GL_ONE_MINUS_CONSTANT_ALPHA;
    case GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE:
      return GL_SRC_ALPHA_SATURATE;
    default:
      g_assert_not_reached ();
      return 0;
  }
}

static gboolean
_set_blend_state (GstGLVideoMixer * video_mixer, GstGLVideoMixerPad * mix_pad)
{
  const GstGLFuncs *gl = GST_GL_BASE_MIXER (video_mixer)->context->gl_vtable;
  gboolean require_separate = FALSE;
  guint gl_func_src_rgb, gl_func_src_alpha, gl_func_dst_rgb, gl_func_dst_alpha;
  guint gl_equation_rgb, gl_equation_alpha;

  require_separate =
      mix_pad->blend_equation_rgb != mix_pad->blend_equation_alpha
      || mix_pad->blend_function_src_rgb != mix_pad->blend_function_src_alpha
      || mix_pad->blend_function_dst_rgb != mix_pad->blend_function_dst_alpha;

  if (require_separate && (!gl->BlendFuncSeparate
          || !gl->BlendEquationSeparate)) {
    GST_ERROR_OBJECT (mix_pad,
        "separated blend equations/functions requested however "
        "glBlendFuncSeparate or glBlendEquationSeparate not available");
    return FALSE;
  }

  if (mix_pad->blend_function_dst_rgb ==
      GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE) {
    GST_ERROR_OBJECT (mix_pad,
        "Destination RGB blend function cannot be \'SRC_ALPHA_SATURATE\'");
    return FALSE;
  }

  if (mix_pad->blend_function_dst_alpha ==
      GST_GL_VIDEO_MIXER_BLEND_FUNCTION_SRC_ALPHA_SATURATE) {
    GST_ERROR_OBJECT (mix_pad,
        "Destination alpha blend function cannot be \'SRC_ALPHA_SATURATE\'");
    return FALSE;
  }

  gl_equation_rgb = _blend_equation_to_gl (mix_pad->blend_equation_rgb);
  gl_equation_alpha = _blend_equation_to_gl (mix_pad->blend_equation_alpha);

  gl_func_src_rgb = _blend_function_to_gl (mix_pad->blend_function_src_rgb);
  gl_func_src_alpha = _blend_function_to_gl (mix_pad->blend_function_src_alpha);
  gl_func_dst_rgb = _blend_function_to_gl (mix_pad->blend_function_dst_rgb);
  gl_func_dst_alpha = _blend_function_to_gl (mix_pad->blend_function_dst_alpha);

  if (gl->BlendEquationSeparate)
    gl->BlendEquationSeparate (gl_equation_rgb, gl_equation_alpha);
  else
    gl->BlendEquation (gl_equation_rgb);

  if (gl->BlendFuncSeparate)
    gl->BlendFuncSeparate (gl_func_src_rgb, gl_func_dst_rgb, gl_func_src_alpha,
        gl_func_dst_alpha);
  else
    gl->BlendFunc (gl_func_src_rgb, gl_func_dst_rgb);

  gl->BlendColor (mix_pad->blend_constant_color_red,
      mix_pad->blend_constant_color_green, mix_pad->blend_constant_color_blue,
      mix_pad->blend_constant_color_alpha);

  return TRUE;
}

/* opengl scene, params: input texture (not the output mixer->texture) */
static gboolean
gst_gl_video_mixer_callback (gpointer stuff)
{
  GstGLVideoMixer *video_mixer = GST_GL_VIDEO_MIXER (stuff);
  GstVideoAggregator *vagg = GST_VIDEO_AGGREGATOR (stuff);
  GstGLMixer *mixer = GST_GL_MIXER (video_mixer);
  GstGLFuncs *gl = GST_GL_BASE_MIXER (mixer)->context->gl_vtable;
  GLint attr_position_loc = 0;
  GLint attr_texture_loc = 0;
  guint out_width, out_height;
  GList *walk;

  out_width = GST_VIDEO_INFO_WIDTH (&vagg->info);
  out_height = GST_VIDEO_INFO_HEIGHT (&vagg->info);

  gst_gl_context_clear_shader (GST_GL_BASE_MIXER (mixer)->context);
  gl->BindTexture (GL_TEXTURE_2D, 0);

  gl->Disable (GL_DEPTH_TEST);
  gl->Disable (GL_CULL_FACE);

  if (gl->GenVertexArrays) {
    if (!video_mixer->vao)
      gl->GenVertexArrays (1, &video_mixer->vao);
    gl->BindVertexArray (video_mixer->vao);
  }

  if (!_draw_background (video_mixer))
    return FALSE;

  gst_gl_shader_use (video_mixer->shader);

  attr_position_loc =
      gst_gl_shader_get_attribute_location (video_mixer->shader, "a_position");
  attr_texture_loc =
      gst_gl_shader_get_attribute_location (video_mixer->shader, "a_texcoord");

  gl->Enable (GL_BLEND);

  GST_OBJECT_LOCK (video_mixer);
  walk = GST_ELEMENT (video_mixer)->sinkpads;
  while (walk) {
    GstGLMixerPad *mix_pad = walk->data;
    GstGLVideoMixerPad *pad = walk->data;
    GstVideoAggregatorPad *vagg_pad = walk->data;
    GstVideoInfo *v_info;
    guint in_tex;
    guint in_width, in_height;

    v_info = &GST_VIDEO_AGGREGATOR_PAD (pad)->info;
    in_width = GST_VIDEO_INFO_WIDTH (v_info);
    in_height = GST_VIDEO_INFO_HEIGHT (v_info);

    if (!mix_pad->current_texture || in_width <= 0 || in_height <= 0
        || pad->alpha == 0.0f) {
      GST_DEBUG ("skipping texture:%u pad:%p width:%u height:%u alpha:%f",
          mix_pad->current_texture, pad, in_width, in_height, pad->alpha);
      walk = g_list_next (walk);
      continue;
    }

    if (!_set_blend_state (video_mixer, pad)) {
      GST_FIXME_OBJECT (pad, "skipping due to incorrect blend parameters");
      walk = g_list_next (walk);
      continue;
    }

    in_tex = mix_pad->current_texture;

    _init_vbo_indices (video_mixer);

    if (video_mixer->output_geo_change
        || pad->geometry_change || !pad->vertex_buffer) {
      gint pad_width, pad_height;
      gint pad_offset_x, pad_offset_y;
      gfloat w, h;
      /* *INDENT-OFF* */
      gfloat v_vertices[] = {
        -1.0,-1.0, 0.0f, 0.0f, 0.0f,
         1.0,-1.0, 0.0f, 1.0f, 0.0f,
         1.0, 1.0, 0.0f, 1.0f, 1.0f,
        -1.0, 1.0, 0.0f, 0.0f, 1.0f,
      };
      /* *INDENT-ON* */

      _mixer_pad_get_output_size (video_mixer, pad,
          GST_VIDEO_INFO_PAR_N (&vagg->info),
          GST_VIDEO_INFO_PAR_D (&vagg->info),
          &pad_width, &pad_height, &pad_offset_x, &pad_offset_y);

      w = ((gfloat) pad_width / (gfloat) out_width);
      h = ((gfloat) pad_height / (gfloat) out_height);

      pad->m_matrix[0] = w;
      pad->m_matrix[5] = h;
      pad->m_matrix[12] =
          2. * (gfloat) (pad->xpos + pad_offset_x) / (gfloat) out_width - (1. -
          w);
      pad->m_matrix[13] =
          2. * (gfloat) (pad->ypos + pad_offset_y) / (gfloat) out_height - (1. -
          h);

      v_vertices[0 * 5 + 3] = v_vertices[3 * 5 + 3] =
          pad->crop_left ? ((gfloat) pad->crop_left) /
          ((gfloat) in_width) : 0.0f;
      v_vertices[1 * 5 + 3] = v_vertices[2 * 5 + 3] =
          pad->crop_right ? 1.0 -
          ((gfloat) pad->crop_right) / ((gfloat) in_width) : 1.0f;
      v_vertices[0 * 5 + 4] = v_vertices[1 * 5 + 4] =
          pad->crop_top ? ((gfloat) pad->crop_top) /
          ((gfloat) in_height) : 0.0f;
      v_vertices[2 * 5 + 4] = v_vertices[3 * 5 + 4] =
          pad->crop_bottom ? 1.0 -
          ((gfloat) pad->crop_bottom) / ((gfloat) in_height) : 1.0f;

      GST_TRACE ("processing texture:%u dimensions:%ux%u with texture "
          "coordinates %f:%fx%f:%f, at %f,%f %fx%f with alpha:%f "
          "and crop: %d:%dx%d:%d", in_tex, in_width, in_height,
          v_vertices[0 * 5 + 3], v_vertices[1 * 5 + 3], v_vertices[0 * 5 + 4],
          v_vertices[1 * 5 + 4], pad->m_matrix[12], pad->m_matrix[13],
          pad->m_matrix[0], pad->m_matrix[5], pad->alpha, pad->crop_left,
          pad->crop_right, pad->crop_top, pad->crop_bottom);

      if (!pad->vertex_buffer)
        gl->GenBuffers (1, &pad->vertex_buffer);

      gl->BindBuffer (GL_ARRAY_BUFFER, pad->vertex_buffer);
      gl->BufferData (GL_ARRAY_BUFFER, 4 * 5 * sizeof (GLfloat), v_vertices,
          GL_STATIC_DRAW);

      pad->geometry_change = FALSE;
    } else {
      gl->BindBuffer (GL_ARRAY_BUFFER, pad->vertex_buffer);
    }
    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, video_mixer->vbo_indices);

    gl->ActiveTexture (GL_TEXTURE0);
    gl->BindTexture (GL_TEXTURE_2D, in_tex);
    gst_gl_shader_set_uniform_1i (video_mixer->shader, "texture", 0);
    gst_gl_shader_set_uniform_1f (video_mixer->shader, "alpha", pad->alpha);

    {
      GstVideoAffineTransformationMeta *af_meta;
      gfloat matrix[16];
      gfloat af_matrix[16];
      GstBuffer *buffer =
          gst_video_aggregator_pad_get_current_buffer (vagg_pad);

      af_meta = gst_buffer_get_video_affine_transformation_meta (buffer);
      gst_gl_get_affine_transformation_meta_as_ndc (af_meta, af_matrix);
      gst_gl_multiply_matrix4 (af_matrix, pad->m_matrix, matrix);
      gst_gl_shader_set_uniform_matrix_4fv (video_mixer->shader,
          "u_transformation", 1, FALSE, matrix);
    }

    gl->EnableVertexAttribArray (attr_position_loc);
    gl->EnableVertexAttribArray (attr_texture_loc);

    gl->VertexAttribPointer (attr_position_loc, 3, GL_FLOAT,
        GL_FALSE, 5 * sizeof (GLfloat), (void *) 0);

    gl->VertexAttribPointer (attr_texture_loc, 2, GL_FLOAT,
        GL_FALSE, 5 * sizeof (GLfloat), (void *) (3 * sizeof (GLfloat)));

    gl->DrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    walk = g_list_next (walk);
  }

  video_mixer->output_geo_change = FALSE;
  GST_OBJECT_UNLOCK (video_mixer);

  if (gl->GenVertexArrays) {
    gl->BindVertexArray (0);
  } else {
    gl->DisableVertexAttribArray (attr_position_loc);
    gl->DisableVertexAttribArray (attr_texture_loc);

    gl->BindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    gl->BindBuffer (GL_ARRAY_BUFFER, 0);
    gl->BindTexture (GL_TEXTURE_2D, 0);
  }

  gl->Disable (GL_BLEND);

  gst_gl_context_clear_shader (GST_GL_BASE_MIXER (mixer)->context);

  return TRUE;
}

/* GstChildProxy implementation */
static GObject *
gst_gl_video_mixer_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstGLVideoMixer *gl_video_mixer = GST_GL_VIDEO_MIXER (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (gl_video_mixer);
  obj = g_list_nth_data (GST_ELEMENT_CAST (gl_video_mixer)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (gl_video_mixer);

  return obj;
}

static guint
gst_gl_video_mixer_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstGLVideoMixer *gl_video_mixer = GST_GL_VIDEO_MIXER (child_proxy);

  GST_OBJECT_LOCK (gl_video_mixer);
  count = GST_ELEMENT_CAST (gl_video_mixer)->numsinkpads;
  GST_OBJECT_UNLOCK (gl_video_mixer);
  GST_INFO_OBJECT (gl_video_mixer, "Children Count: %d", count);

  return count;
}

static void
gst_gl_video_mixer_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  iface->get_child_by_index = gst_gl_video_mixer_child_proxy_get_child_by_index;
  iface->get_children_count = gst_gl_video_mixer_child_proxy_get_children_count;
}
