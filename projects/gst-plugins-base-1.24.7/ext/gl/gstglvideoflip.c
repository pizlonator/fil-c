/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-glvideo_flip
 * @title: glvideo_flip
 *
 * Transforms video on the GPU.
 *
 * ## Examples
 * |[
 * gst-launch-1.0 videotestsrc ! glupload ! glvideoflip method=clockwise ! glimagesinkelement
 * ]| This pipeline flips the test image 90 degrees clockwise.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstglelements.h"
#include "gstglvideoflip.h"

#define GST_CAT_DEFAULT gst_gl_video_flip_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEFAULT_METHOD GST_VIDEO_ORIENTATION_IDENTITY

enum
{
  PROP_0,
  PROP_METHOD,
  PROP_VIDEO_DIRECTION
};

static GstStaticPadTemplate _sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ", "
        "texture-target = (string) 2D"));

static GstStaticPadTemplate _src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw(" GST_CAPS_FEATURE_MEMORY_GL_MEMORY "), "
        "format = (string) RGBA, "
        "width = " GST_VIDEO_SIZE_RANGE ", "
        "height = " GST_VIDEO_SIZE_RANGE ", "
        "framerate = " GST_VIDEO_FPS_RANGE ", "
        "texture-target = (string) 2D"));

#define GST_TYPE_GL_VIDEO_FLIP_METHOD (gst_video_flip_method_get_type())
static const GEnumValue video_flip_methods[] = {
  {GST_VIDEO_ORIENTATION_IDENTITY, "Identity (no rotation)", "none"},
  {GST_VIDEO_ORIENTATION_90R, "Rotate clockwise 90 degrees", "clockwise"},
  {GST_VIDEO_ORIENTATION_180, "Rotate 180 degrees", "rotate-180"},
  {GST_VIDEO_ORIENTATION_90L, "Rotate counter-clockwise 90 degrees",
      "counterclockwise"},
  {GST_VIDEO_ORIENTATION_HORIZ, "Flip horizontally", "horizontal-flip"},
  {GST_VIDEO_ORIENTATION_VERT, "Flip vertically", "vertical-flip"},
  {GST_VIDEO_ORIENTATION_UL_LR,
      "Flip across upper left/lower right diagonal", "upper-left-diagonal"},
  {GST_VIDEO_ORIENTATION_UR_LL,
      "Flip across upper right/lower left diagonal", "upper-right-diagonal"},
  {GST_VIDEO_ORIENTATION_AUTO,
      "Select flip method based on image-orientation tag", "automatic"},
  {0, NULL, NULL},
};

static GType
gst_video_flip_method_get_type (void)
{
  static GType video_flip_method_type = 0;

  if (!video_flip_method_type) {
    video_flip_method_type = g_enum_register_static ("GstGLVideoFlipMethod",
        video_flip_methods);
  }
  return video_flip_method_type;
}

static void gst_gl_video_flip_finalize (GObject * object);
static void gst_gl_video_flip_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_video_flip_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_gl_video_flip_set_method (GstGLVideoFlip * vf,
    GstVideoOrientationMethod method, gboolean from_tag);

static GstPadProbeReturn _input_sink_probe (GstPad * pad,
    GstPadProbeInfo * info, gpointer user_data);
static GstPadProbeReturn _trans_src_probe (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data);

static void
gst_gl_video_flip_video_direction_interface_init (GstVideoDirectionInterface
    * iface);

#define gst_gl_video_flip_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGLVideoFlip, gst_gl_video_flip,
    GST_TYPE_BIN, GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "glvideoflip", 0, "glvideoflip element");
    G_IMPLEMENT_INTERFACE (GST_TYPE_VIDEO_DIRECTION,
        gst_gl_video_flip_video_direction_interface_init););
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (glvideoflip, "glvideoflip",
    GST_RANK_NONE, GST_TYPE_GL_VIDEO_FLIP, gl_element_init (plugin));

static void
gst_gl_video_flip_video_direction_interface_init (GstVideoDirectionInterface
    * iface)
{
  /* We implement the video-direction property */
}

static void
gst_gl_video_flip_constructed (GObject * object)
{
  GstGLVideoFlip *self = GST_GL_VIDEO_FLIP (object);

  if (self->method == (GstVideoOrientationMethod) DEFAULT_METHOD) {
    gst_gl_video_flip_set_method (self,
        (GstVideoOrientationMethod) DEFAULT_METHOD, FALSE);
  }
}

static void
gst_gl_video_flip_class_init (GstGLVideoFlipClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_gl_video_flip_finalize;
  gobject_class->set_property = gst_gl_video_flip_set_property;
  gobject_class->get_property = gst_gl_video_flip_get_property;
  gobject_class->constructed = gst_gl_video_flip_constructed;

  g_object_class_install_property (gobject_class, PROP_METHOD,
      g_param_spec_enum ("method", "method",
          "method (deprecated, use video-direction instead)",
          GST_TYPE_GL_VIDEO_FLIP_METHOD, DEFAULT_METHOD,
          GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_override_property (gobject_class, PROP_VIDEO_DIRECTION,
      "video-direction");

  gst_element_class_add_static_pad_template (element_class, &_src_template);
  gst_element_class_add_static_pad_template (element_class, &_sink_template);

  gst_element_class_set_metadata (element_class, "OpenGL video flip filter",
      "Filter/Effect/Video", "Flip video on the GPU",
      "Matthew Waters <matthew@centricular.com>");

  gst_type_mark_as_plugin_api (GST_TYPE_GL_VIDEO_FLIP_METHOD, 0);
}

static void
gst_gl_video_flip_init (GstGLVideoFlip * flip)
{
  gboolean res = TRUE;
  GstPad *pad;

  /* We initialize to the default and call set_method() from constructed
   * if the value hasn't changed, this ensures set_method() does get called
   * even if the non-construct method / direction properties aren't set
   */
  flip->method = (GstVideoOrientationMethod) DEFAULT_METHOD;

  flip->aspect = 1.0;

  flip->input_capsfilter = gst_element_factory_make ("capsfilter", NULL);
  res &= gst_bin_add (GST_BIN (flip), flip->input_capsfilter);

  flip->transformation = gst_element_factory_make ("gltransformation", NULL);
  g_object_set (flip->transformation, "ortho", TRUE, NULL);
  res &= gst_bin_add (GST_BIN (flip), flip->transformation);

  flip->output_capsfilter = gst_element_factory_make ("capsfilter", NULL);
  res &= gst_bin_add (GST_BIN (flip), flip->output_capsfilter);

  res &=
      gst_element_link_pads (flip->input_capsfilter, "src",
      flip->transformation, "sink");
  res &=
      gst_element_link_pads (flip->transformation, "src",
      flip->output_capsfilter, "sink");

  pad = gst_element_get_static_pad (flip->input_capsfilter, "sink");
  if (!pad) {
    res = FALSE;
  } else {
    GST_DEBUG_OBJECT (flip, "setting target sink pad %" GST_PTR_FORMAT, pad);
    flip->sinkpad = gst_ghost_pad_new ("sink", pad);
    flip->sink_probe = gst_pad_add_probe (flip->sinkpad,
        GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM |
        GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
        (GstPadProbeCallback) _input_sink_probe, flip, NULL);
    gst_element_add_pad (GST_ELEMENT_CAST (flip), flip->sinkpad);
    gst_object_unref (pad);
  }

  pad = gst_element_get_static_pad (flip->transformation, "src");
  flip->src_probe = gst_pad_add_probe (pad,
      GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM,
      (GstPadProbeCallback) _trans_src_probe, flip, NULL);
  gst_object_unref (pad);

  pad = gst_element_get_static_pad (flip->output_capsfilter, "src");
  if (!pad) {
    res = FALSE;
  } else {
    GST_DEBUG_OBJECT (flip, "setting target sink pad %" GST_PTR_FORMAT, pad);
    flip->srcpad = gst_ghost_pad_new ("src", pad);
    gst_element_add_pad (GST_ELEMENT_CAST (flip), flip->srcpad);
    gst_object_unref (pad);
  }

  if (!res) {
    GST_WARNING_OBJECT (flip, "Failed to add/connect the necessary machinery");
  }
}

static void
gst_gl_video_flip_finalize (GObject * object)
{
  GstGLVideoFlip *flip = GST_GL_VIDEO_FLIP (object);

  gst_caps_replace (&flip->input_caps, NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Caps negotiation happens like this:
 *
 * 1. caps/accept-caps queries bypass the capsfilters on either side of the
 *    transformation element so the fixed caps don't get in the way.
 * 2. Receiving a caps event on the sink pad will set fixed caps on either side
 *    of the transformation element.
 */
static GstCaps *
_transform_caps (GstGLVideoFlip * vf, GstPadDirection direction, GstCaps * caps)
{
  GstCaps *output = gst_caps_copy (caps);
  gint i;

  for (i = 0; i < gst_caps_get_size (output); i++) {
    GstStructure *structure = gst_caps_get_structure (output, i);
    gint width, height;
    gint par_n, par_d;

    if (gst_structure_get_int (structure, "width", &width) &&
        gst_structure_get_int (structure, "height", &height)) {

      switch (vf->active_method) {
        case GST_VIDEO_ORIENTATION_90R:
        case GST_VIDEO_ORIENTATION_90L:
        case GST_VIDEO_ORIENTATION_UL_LR:
        case GST_VIDEO_ORIENTATION_UR_LL:
          gst_structure_set (structure, "width", G_TYPE_INT, height,
              "height", G_TYPE_INT, width, NULL);
          if (gst_structure_get_fraction (structure, "pixel-aspect-ratio",
                  &par_n, &par_d)) {
            if (par_n != 1 || par_d != 1) {
              GValue val = { 0, };

              g_value_init (&val, GST_TYPE_FRACTION);
              gst_value_set_fraction (&val, par_d, par_n);
              gst_structure_set_value (structure, "pixel-aspect-ratio", &val);
              g_value_unset (&val);
            }
          }
          break;
        case GST_VIDEO_ORIENTATION_IDENTITY:
        case GST_VIDEO_ORIENTATION_180:
        case GST_VIDEO_ORIENTATION_HORIZ:
        case GST_VIDEO_ORIENTATION_VERT:
          break;
        default:
          g_assert_not_reached ();
          break;
      }
    }
  }

  return output;
}

/* with object lock */
static void
_set_active_method (GstGLVideoFlip * vf, GstVideoOrientationMethod method,
    GstCaps * caps)
{
  gfloat rot_z = 0., scale_x = 1.0, scale_y = 1.0;
  GstCaps *output_caps, *templ;
  GstPad *srcpad;

  switch (method) {
    case GST_VIDEO_ORIENTATION_IDENTITY:
      break;
    case GST_VIDEO_ORIENTATION_90R:
      scale_x *= vf->aspect;
      scale_y *= 1. / vf->aspect;
      rot_z = 90.;
      break;
    case GST_VIDEO_ORIENTATION_180:
      rot_z = 180.;
      break;
    case GST_VIDEO_ORIENTATION_90L:
      scale_x *= vf->aspect;
      scale_y *= 1. / vf->aspect;
      rot_z = 270.;
      break;
    case GST_VIDEO_ORIENTATION_HORIZ:
      scale_x *= -1.;
      break;
    case GST_VIDEO_ORIENTATION_UR_LL:
      scale_x *= -vf->aspect;
      scale_y *= 1. / vf->aspect;
      rot_z = 90.;
      break;
    case GST_VIDEO_ORIENTATION_VERT:
      scale_x *= -1.;
      rot_z = 180.;
      break;
    case GST_VIDEO_ORIENTATION_UL_LR:
      scale_x *= -vf->aspect;
      scale_y *= 1. / vf->aspect;
      rot_z = 270.;
      break;
    default:
      break;
  }
  vf->active_method = method;

  output_caps = _transform_caps (vf, GST_PAD_SINK, caps);
  gst_caps_replace (&vf->input_caps, caps);

  srcpad = gst_element_get_static_pad (vf->transformation, "src");
  templ = gst_pad_get_pad_template_caps (srcpad);
  gst_object_unref (srcpad);

  gst_caps_append (output_caps, gst_caps_ref (templ));
  GST_OBJECT_UNLOCK (vf);

  g_object_set (vf->input_capsfilter, "caps", caps, NULL);
  g_object_set (vf->output_capsfilter, "caps", output_caps, NULL);
  g_object_set (vf->transformation, "rotation-z", rot_z, "scale-x", scale_x,
      "scale-y", scale_y, NULL);

  gst_caps_unref (output_caps);

  GST_OBJECT_LOCK (vf);
}

static void
gst_gl_video_flip_set_method (GstGLVideoFlip * vf,
    GstVideoOrientationMethod method, gboolean from_tag)
{
  GST_OBJECT_LOCK (vf);

  if (method == GST_VIDEO_ORIENTATION_CUSTOM) {
    GST_WARNING_OBJECT (vf, "unsupported custom orientation");
    GST_OBJECT_UNLOCK (vf);
    return;
  }

  /* Store updated method */
  if (from_tag)
    vf->tag_method = method;
  else
    vf->method = method;

  /* Get the new method */
  if (vf->method == GST_VIDEO_ORIENTATION_AUTO)
    method = vf->tag_method;
  else
    method = vf->method;

  if (vf->input_caps)
    _set_active_method (vf, method, vf->input_caps);
  else {
    /* just store the configured method here. The actual transform configuration
     * will be done once caps are configured. See caps handling in
     * _input_sink_probe. */
    vf->active_method = method;
  }

  GST_OBJECT_UNLOCK (vf);
}

static void
gst_gl_video_flip_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLVideoFlip *vf = GST_GL_VIDEO_FLIP (object);

  switch (prop_id) {
    case PROP_METHOD:
    case PROP_VIDEO_DIRECTION:
      gst_gl_video_flip_set_method (vf, g_value_get_enum (value), FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_video_flip_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGLVideoFlip *vf = GST_GL_VIDEO_FLIP (object);

  switch (prop_id) {
    case PROP_METHOD:
    case PROP_VIDEO_DIRECTION:
      g_value_set_enum (value, vf->method);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstPadProbeReturn
_input_sink_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstGLVideoFlip *vf = GST_GL_VIDEO_FLIP (user_data);

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

    switch (GST_EVENT_TYPE (event)) {
      case GST_EVENT_TAG:{
        GstTagList *taglist;
        GstVideoOrientationMethod method;

        gst_event_parse_tag (event, &taglist);

        if (gst_video_orientation_from_tag (taglist, &method))
          gst_gl_video_flip_set_method (vf, method, TRUE);
        break;
      }
      case GST_EVENT_CAPS:{
        GstCaps *caps;
        GstVideoInfo v_info;

        gst_event_parse_caps (event, &caps);
        GST_OBJECT_LOCK (vf);
        if (gst_video_info_from_caps (&v_info, caps))
          vf->aspect =
              (gfloat) GST_VIDEO_INFO_WIDTH (&v_info) /
              (gfloat) GST_VIDEO_INFO_HEIGHT (&v_info);
        else
          vf->aspect = 1.0;
        _set_active_method (vf, vf->active_method, caps);
        GST_OBJECT_UNLOCK (vf);
        break;
      }
      default:
        break;
    }
  } else if (GST_PAD_PROBE_INFO_TYPE (info) &
      GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

    switch (GST_QUERY_TYPE (query)) {
        /* bypass the capsfilter */
      case GST_QUERY_CAPS:
      case GST_QUERY_ACCEPT_CAPS:{
        GstPad *pad = gst_element_get_static_pad (vf->transformation, "sink");
        if (gst_pad_query (pad, query)) {
          gst_object_unref (pad);
          return GST_PAD_PROBE_HANDLED;
        } else {
          gst_object_unref (pad);
          return GST_PAD_PROBE_DROP;
        }
      }
      default:
        break;
    }
  }

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
_trans_src_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstGLVideoFlip *vf = GST_GL_VIDEO_FLIP (user_data);

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM) {
    GstQuery *query = GST_PAD_PROBE_INFO_QUERY (info);

    switch (GST_QUERY_TYPE (query)) {
        /* bypass the capsfilter */
      case GST_QUERY_CAPS:
      case GST_QUERY_ACCEPT_CAPS:{
        if (gst_pad_peer_query (vf->srcpad, query))
          return GST_PAD_PROBE_HANDLED;
        else
          return GST_PAD_PROBE_DROP;
      }
      default:
        break;
    }
  }

  return GST_PAD_PROBE_OK;
}
