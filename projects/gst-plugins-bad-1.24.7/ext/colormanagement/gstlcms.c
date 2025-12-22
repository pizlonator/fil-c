/*
 * GStreamer gstreamer-lcms
 * Copyright (C) 2016 Andreas Frisch <fraxinas@dreambox.guru>
 *
 * gstlcms.c
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
 * SECTION:element-lcms
 * @short_description: Uses LittleCMS 2 to perform ICC profile correction
 *
 * This is a color management plugin that uses LittleCMS 2 to correct
 * frames using the given ICC (International Color Consortium) profiles.
 * Falls back to internal sRGB profile if no ICC file is specified in property.
 *
 * ## Example launch line
 *
 * (write everything in one line, without the backslash characters)
 * |[
 * gst-launch-1.0 filesrc location=photo_camera.png ! pngdec ! \
 * videoconvert ! lcms input-profile=sRGB.icc dest-profile=printer.icc ! \
 * pngenc ! filesink location=photo_print.png
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstlcms.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <stdlib.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (lcms_debug);
#define GST_CAT_DEFAULT lcms_debug

/* GstLcms properties */
enum
{
  PROP_0,
  PROP_INTENT,
  PROP_LOOKUP_METHOD,
  PROP_SRC_FILE,
  PROP_DST_FILE,
  PROP_PRESERVE_BLACK,
  PROP_EMBEDDED_PROFILE
};

GType
gst_lcms_intent_get_type (void)
{
  static gsize intent_type = 0;
  static const GEnumValue intent[] = {
    {GST_LCMS_INTENT_PERCEPTUAL, "Perceptual",
        "perceptual"},
    {GST_LCMS_INTENT_RELATIVE_COLORIMETRIC, "Relative Colorimetric",
        "relative"},
    {GST_LCMS_INTENT_SATURATION, "Saturation",
        "saturation"},
    {GST_LCMS_INTENT_ABSOLUTE_COLORIMETRIC, "Absolute Colorimetric",
        "absolute"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&intent_type)) {
    GType tmp = g_enum_register_static ("GstLcmsIntent", intent);
    g_once_init_leave (&intent_type, tmp);
  }
  return (GType) intent_type;
}

static GType
gst_lcms_lookup_method_get_type (void)
{
  static gsize lookup_method_type = 0;
  static const GEnumValue lookup_method[] = {
    {GST_LCMS_LOOKUP_METHOD_UNCACHED,
          "Uncached, calculate every pixel on the fly (very slow playback)",
        "uncached"},
    {GST_LCMS_LOOKUP_METHOD_PRECALCULATED,
          "Precalculate lookup table (takes a long time getting READY)",
        "precalculated"},
    {GST_LCMS_LOOKUP_METHOD_CACHED,
          "Calculate and cache color replacement values on first occurrence",
        "cached"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&lookup_method_type)) {
    GType tmp = g_enum_register_static ("GstLcmsLookupMethod", lookup_method);
    g_once_init_leave (&lookup_method_type, tmp);
  }
  return (GType) lookup_method_type;
}

#define DEFAULT_INTENT  GST_LCMS_INTENT_PERCEPTUAL
#define DEFAULT_LOOKUP_METHOD     GST_LCMS_LOOKUP_METHOD_CACHED
#define DEFAULT_PRESERVE_BLACK    FALSE
#define DEFAULT_EMBEDDED_PROFILE  TRUE

static GstStaticPadTemplate gst_lcms_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ "
            "ARGB, BGRA, ABGR, RGBA, xRGB," "RGBx, xBGR, BGRx, RGB, BGR }"))
    );

static GstStaticPadTemplate gst_lcms_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ "
            "ARGB, BGRA, ABGR, RGBA, xRGB," "RGBx, xBGR, BGRx, RGB, BGR }"))
    );

static void gst_lcms_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_lcms_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_lcms_finalize (GObject * object);
static GstStateChangeReturn gst_lcms_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_lcms_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static gboolean gst_lcms_sink_event (GstBaseTransform * trans,
    GstEvent * event);
static void gst_lcms_handle_tags (GstLcms * lcms, GstTagList * taglist);
static void gst_lcms_handle_tag_sample (GstLcms * lcms, GstSample * sample);
static GstFlowReturn gst_lcms_transform_frame (GstVideoFilter * vfilter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstFlowReturn gst_lcms_transform_frame_ip (GstVideoFilter * vfilter,
    GstVideoFrame * frame);

static void gst_lcms_get_ready (GstLcms * lcms);
static void gst_lcms_create_transform (GstLcms * lcms);
static void gst_lcms_cleanup_cms (GstLcms * lcms);
static void gst_lcms_init_lookup_table (GstLcms * lcms);
static void gst_lcms_process_rgb (GstLcms * lcms, GstVideoFrame * inframe,
    GstVideoFrame * outframe);

G_DEFINE_TYPE (GstLcms, gst_lcms, GST_TYPE_VIDEO_FILTER);
GST_ELEMENT_REGISTER_DEFINE (lcms, "lcms", GST_RANK_NONE, GST_TYPE_LCMS);

static void
gst_lcms_class_init (GstLcmsClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *element_class = (GstElementClass *) klass;
  GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;
  GstVideoFilterClass *vfilter_class = (GstVideoFilterClass *) klass;

  GST_DEBUG_CATEGORY_INIT (lcms_debug, "lcms", 0, "lcms");

  gobject_class->set_property = gst_lcms_set_property;
  gobject_class->get_property = gst_lcms_get_property;
  gobject_class->finalize = gst_lcms_finalize;

  g_object_class_install_property (gobject_class, PROP_INTENT,
      g_param_spec_enum ("intent", "Rendering intent",
          "Select the rendering intent of the color correction",
          GST_TYPE_LCMS_INTENT, DEFAULT_INTENT,
          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_SRC_FILE,
      g_param_spec_string ("input-profile", "Input ICC profile file",
          "Specify the input ICC profile file to apply", NULL,
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DST_FILE,
      g_param_spec_string ("dest-profile", "Destination ICC profile file",
          "Specify the destination ICC profile file to apply", NULL,
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_LOOKUP_METHOD,
      g_param_spec_enum ("lookup", "Lookup method",
          "Select the caching method for the color compensation calculations",
          GST_TYPE_LCMS_LOOKUP_METHOD, DEFAULT_LOOKUP_METHOD,
          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_PRESERVE_BLACK,
      g_param_spec_boolean ("preserve-black", "Preserve black",
          "Select whether purely black pixels should be preserved",
          DEFAULT_PRESERVE_BLACK,
          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_EMBEDDED_PROFILE,
      g_param_spec_boolean ("embedded-profile", "Embedded Profile",
          "Extract and use source profiles embedded in images",
          DEFAULT_EMBEDDED_PROFILE,
          (G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gst_element_class_set_static_metadata (element_class,
      "LCMS2 ICC correction", "Filter/Effect/Video",
      "Uses LittleCMS 2 to perform ICC profile correction",
      "Andreas Frisch <fraxinas@opendreambox.org>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_lcms_sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_lcms_src_template));

  element_class->change_state = GST_DEBUG_FUNCPTR (gst_lcms_change_state);

  trans_class->sink_event = GST_DEBUG_FUNCPTR (gst_lcms_sink_event);

  vfilter_class->set_info = GST_DEBUG_FUNCPTR (gst_lcms_set_info);
  vfilter_class->transform_frame_ip =
      GST_DEBUG_FUNCPTR (gst_lcms_transform_frame_ip);
  vfilter_class->transform_frame = GST_DEBUG_FUNCPTR (gst_lcms_transform_frame);

  gst_type_mark_as_plugin_api (GST_TYPE_LCMS_INTENT, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_LCMS_LOOKUP_METHOD, 0);
}

static void
gst_lcms_init (GstLcms * lcms)
{
  lcms->color_lut = NULL;
  lcms->cms_inp_profile = NULL;
  lcms->cms_dst_profile = NULL;
  lcms->cms_transform = NULL;
}

static void
gst_lcms_finalize (GObject * object)
{
  GstLcms *lcms = GST_LCMS (object);
  if (lcms->color_lut)
    g_free (lcms->color_lut);
  g_free (lcms->inp_profile_filename);
  g_free (lcms->dst_profile_filename);
  G_OBJECT_CLASS (gst_lcms_parent_class)->finalize (object);
}

static void
gst_lcms_set_intent (GstLcms * lcms, GstLcmsIntent intent)
{
  GEnumValue *val =
      g_enum_get_value (G_ENUM_CLASS (g_type_class_ref
          (GST_TYPE_LCMS_INTENT)), intent);
  const gchar *value_nick;

  g_return_if_fail (GST_IS_LCMS (lcms));
  if (!val) {
    GST_ERROR_OBJECT (lcms, "no such rendering intent %i!", intent);
    return;
  }
  value_nick = val->value_nick;

  GST_OBJECT_LOCK (lcms);
  lcms->intent = intent;
  GST_OBJECT_UNLOCK (lcms);

  GST_DEBUG_OBJECT (lcms, "successfully set rendering intent to %s (%i)",
      value_nick, intent);
  return;
}

static GstLcmsIntent
gst_lcms_get_intent (GstLcms * lcms)
{
  g_return_val_if_fail (GST_IS_LCMS (lcms), (GstLcmsIntent) - 1);
  return lcms->intent;
}

static void
gst_lcms_set_lookup_method (GstLcms * lcms, GstLcmsLookupMethod method)
{
  GEnumValue *val =
      g_enum_get_value (G_ENUM_CLASS (g_type_class_ref
          (GST_TYPE_LCMS_LOOKUP_METHOD)), method);
  const gchar *value_nick;

  g_return_if_fail (GST_IS_LCMS (lcms));
  if (!val) {
    GST_ERROR_OBJECT (lcms, "no such lookup method %i!", method);
    return;
  }
  value_nick = val->value_nick;

  GST_OBJECT_LOCK (lcms);
  lcms->lookup_method = method;
  GST_OBJECT_UNLOCK (lcms);

  GST_DEBUG_OBJECT (lcms, "successfully set lookup method to %s (%i)",
      value_nick, method);
  return;
}

static GstLcmsLookupMethod
gst_lcms_get_lookup_method (GstLcms * lcms)
{
  g_return_val_if_fail (GST_IS_LCMS (lcms), (GstLcmsLookupMethod) - 1);
  return lcms->lookup_method;
}

static void
gst_lcms_set_property (GObject * object, guint prop_id, const GValue * value,
    GParamSpec * pspec)
{
  const gchar *filename;
  GstLcms *lcms = GST_LCMS (object);

  switch (prop_id) {
    case PROP_SRC_FILE:
    {
      GST_OBJECT_LOCK (lcms);
      filename = g_value_get_string (value);
      if (filename
          && g_file_test (filename,
              (GFileTest) (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        if (lcms->inp_profile_filename)
          g_free (lcms->inp_profile_filename);
        lcms->inp_profile_filename = g_strdup (filename);
      } else {
        GST_WARNING_OBJECT (lcms, "Input profile file '%s' not found!",
            filename);
      }
      GST_OBJECT_UNLOCK (lcms);
      break;
    }
    case PROP_DST_FILE:
    {
      GST_OBJECT_LOCK (lcms);
      filename = g_value_get_string (value);
      if (g_file_test (filename,
              (GFileTest) (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))) {
        if (lcms->dst_profile_filename)
          g_free (lcms->dst_profile_filename);
        lcms->dst_profile_filename = g_strdup (filename);
      } else {
        GST_WARNING_OBJECT (lcms, "Destination profile file '%s' not found!",
            filename);
      }
      GST_OBJECT_UNLOCK (lcms);
      break;
    }
    case PROP_INTENT:
      gst_lcms_set_intent (lcms, (GstLcmsIntent) g_value_get_enum (value));
      break;
    case PROP_LOOKUP_METHOD:
      gst_lcms_set_lookup_method (lcms,
          (GstLcmsLookupMethod) g_value_get_enum (value));
      break;
    case PROP_PRESERVE_BLACK:
      lcms->preserve_black = g_value_get_boolean (value);
      break;
    case PROP_EMBEDDED_PROFILE:
      lcms->embeddedprofiles = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_lcms_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstLcms *lcms = GST_LCMS (object);

  switch (prop_id) {
    case PROP_SRC_FILE:
      g_value_set_string (value, lcms->inp_profile_filename);
      break;
    case PROP_DST_FILE:
      g_value_set_string (value, lcms->dst_profile_filename);
      break;
    case PROP_INTENT:
      g_value_set_enum (value, gst_lcms_get_intent (lcms));
      break;
    case PROP_LOOKUP_METHOD:
      g_value_set_enum (value, gst_lcms_get_lookup_method (lcms));
      break;
    case PROP_PRESERVE_BLACK:
      g_value_set_boolean (value, lcms->preserve_black);
      break;
    case PROP_EMBEDDED_PROFILE:
      g_value_set_boolean (value, lcms->embeddedprofiles);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_lcms_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstLcms *lcms = GST_LCMS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      GST_DEBUG_OBJECT (lcms, "GST_STATE_CHANGE_NULL_TO_READY");
      gst_lcms_get_ready (lcms);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
      if (!lcms->cms_inp_profile) {
        if (!lcms->cms_dst_profile) {
          GST_WARNING_OBJECT (lcms,
              "No input or output ICC profile specified, falling back to passthrough!");
          gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (lcms), TRUE);
          GST_BASE_TRANSFORM_CLASS (GST_LCMS_GET_CLASS
              (lcms))->transform_ip_on_passthrough = lcms->embeddedprofiles;
          return GST_STATE_CHANGE_SUCCESS;
        }
        lcms->cms_inp_profile = cmsCreate_sRGBProfile ();
        GST_INFO_OBJECT (lcms,
            "No input profile specified, falling back to sRGB");
      }
    }

    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_SUCCESS)
    ret =
        GST_ELEMENT_CLASS (gst_lcms_parent_class)->change_state (element,
        transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_lcms_cleanup_cms (lcms);
    default:
      break;
  }
  return ret;
}

static void
gst_lcms_get_ready (GstLcms * lcms)
{
  if (lcms->inp_profile_filename) {
    lcms->cms_inp_profile =
        cmsOpenProfileFromFile (lcms->inp_profile_filename, "r");
    if (!lcms->cms_inp_profile)
      GST_ERROR_OBJECT (lcms, "Couldn't parse input ICC profile '%s'",
          lcms->inp_profile_filename);
    else
      GST_DEBUG_OBJECT (lcms, "Successfully opened input ICC profile '%s'",
          lcms->inp_profile_filename);
  }

  if (lcms->dst_profile_filename) {
    lcms->cms_dst_profile =
        cmsOpenProfileFromFile (lcms->dst_profile_filename, "r");
    if (!lcms->cms_dst_profile)
      GST_ERROR_OBJECT (lcms,
          "Couldn't parse destination ICC profile '%s'",
          lcms->dst_profile_filename);
    else
      GST_DEBUG_OBJECT (lcms, "Successfully opened output ICC profile '%s'",
          lcms->dst_profile_filename);
  }

  if (lcms->lookup_method != GST_LCMS_LOOKUP_METHOD_UNCACHED) {
    gst_lcms_init_lookup_table (lcms);
  }
}

static void
gst_lcms_cleanup_cms (GstLcms * lcms)
{
  if (lcms->cms_inp_profile) {
    cmsCloseProfile (lcms->cms_inp_profile);
    lcms->cms_inp_profile = NULL;
  }
  if (lcms->cms_dst_profile) {
    cmsCloseProfile (lcms->cms_dst_profile);
    lcms->cms_dst_profile = NULL;
  }
  if (lcms->cms_transform) {
    cmsDeleteTransform (lcms->cms_transform);
    lcms->cms_transform = NULL;
  }
}

static void
gst_lcms_init_lookup_table (GstLcms * lcms)
{
  guint32 p;
  const guint32 color_max = 0x01000000;

  if (lcms->color_lut)
    g_free (lcms->color_lut);

  lcms->color_lut = g_new (guint32, color_max);

  if (lcms->color_lut == NULL) {
    GST_ELEMENT_ERROR (lcms, RESOURCE, FAILED, ("LUT alloc failed"),
        ("Unable to open allocate memory for lookup table!"));
    return;
  }

  if (lcms->lookup_method == GST_LCMS_LOOKUP_METHOD_PRECALCULATED) {
    cmsHTRANSFORM hTransform;
    hTransform =
        cmsCreateTransform (lcms->cms_inp_profile, TYPE_RGB_8,
        lcms->cms_dst_profile, TYPE_RGB_8, lcms->intent, 0);
    /*FIXME use cmsFLAGS_COPY_ALPHA when new lcms2 2.8 release is available */
    for (p = 0; p < color_max; p++)
      cmsDoTransform (hTransform, (const cmsUInt32Number *) &p,
          &lcms->color_lut[p], 1);
    cmsDeleteTransform (hTransform);
    GST_DEBUG_OBJECT (lcms, "writing lookup table finished");
  } else if (lcms->lookup_method == GST_LCMS_LOOKUP_METHOD_CACHED) {
    memset (lcms->color_lut, 0xAA, color_max * sizeof (guint32));
    GST_DEBUG_OBJECT (lcms, "initialized empty lookup table for caching");
  }
  if (lcms->preserve_black)
    lcms->color_lut[0] = 0x000000;
}

static cmsUInt32Number
gst_lcms_cms_format_from_gst (GstVideoFormat gst_format)
{
  cmsUInt32Number cms_format = 0;
  switch (gst_format) {
    case GST_VIDEO_FORMAT_ARGB:
    case GST_VIDEO_FORMAT_xRGB:
      cms_format = TYPE_ARGB_8;
      break;
    case GST_VIDEO_FORMAT_xBGR:
    case GST_VIDEO_FORMAT_ABGR:
      cms_format = TYPE_ABGR_8;
      break;
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_BGRx:
      cms_format = TYPE_BGRA_8;
      break;
    case GST_VIDEO_FORMAT_BGR:
      cms_format = TYPE_BGR_8;
      break;
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_RGBx:
      cms_format = TYPE_RGBA_8;
      break;
    case GST_VIDEO_FORMAT_RGB:
      cms_format = TYPE_RGB_8;
      break;
    default:
      break;
  }
  return cms_format;
}

static gboolean
gst_lcms_set_info (GstVideoFilter * vfilter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstLcms *lcms = GST_LCMS (vfilter);

  GST_DEBUG_OBJECT (lcms,
      "setting caps: in %" GST_PTR_FORMAT " out %" GST_PTR_FORMAT, incaps,
      outcaps);

  lcms->cms_inp_format =
      gst_lcms_cms_format_from_gst (GST_VIDEO_INFO_FORMAT (in_info));
  lcms->cms_dst_format =
      gst_lcms_cms_format_from_gst (GST_VIDEO_INFO_FORMAT (out_info));

  if (gst_base_transform_is_passthrough (GST_BASE_TRANSFORM (lcms)))
    return TRUE;

  if (!lcms->cms_inp_format || !lcms->cms_dst_format)
    goto invalid_caps;

  if (lcms->cms_inp_format == lcms->cms_dst_format
      && lcms->lookup_method != GST_LCMS_LOOKUP_METHOD_UNCACHED) {
    gst_base_transform_set_in_place (GST_BASE_TRANSFORM (lcms), TRUE);
  } else
    gst_base_transform_set_in_place (GST_BASE_TRANSFORM (lcms), FALSE);

  gst_lcms_create_transform (lcms);
  lcms->process = gst_lcms_process_rgb;

  return TRUE;

invalid_caps:
  {
    GST_ERROR_OBJECT (lcms, "Invalid caps: %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }
}

static void
gst_lcms_create_transform (GstLcms * lcms)
{
  if (!lcms->cms_dst_profile) {
    lcms->cms_dst_profile = cmsCreate_sRGBProfile ();
    GST_INFO_OBJECT (lcms, "No output profile specified, falling back to sRGB");
  }
  lcms->cms_transform =
      cmsCreateTransform (lcms->cms_inp_profile, lcms->cms_inp_format,
      lcms->cms_dst_profile, lcms->cms_dst_format, lcms->intent, 0);
  if (lcms->cms_transform) {
    GST_DEBUG_OBJECT (lcms, "created transformation format=%i->%i",
        lcms->cms_inp_format, lcms->cms_dst_format);
  } else {
    GST_WARNING_OBJECT (lcms,
        "couldn't create transformation format=%i->%i, fallback to passthrough!",
        lcms->cms_inp_format, lcms->cms_dst_format);
    gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (lcms), TRUE);
  }
}

static gboolean
gst_lcms_sink_event (GstBaseTransform * trans, GstEvent * event)
{
  gboolean ret = FALSE;
  GstLcms *lcms = GST_LCMS (trans);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
    {
      if (lcms->embeddedprofiles) {
        GstTagList *taglist = NULL;
        /* icc profiles might be embedded in attachments */
        gst_event_parse_tag (event, &taglist);
        gst_lcms_handle_tags (lcms, taglist);
      }
      break;
    }
    default:
      break;
  }
  ret =
      GST_BASE_TRANSFORM_CLASS (gst_lcms_parent_class)->sink_event (trans,
      event);
  return ret;
}

static void
gst_lcms_handle_tag_sample (GstLcms * lcms, GstSample * sample)
{
  GstBuffer *buf;
  const GstStructure *structure;

  buf = gst_sample_get_buffer (sample);
  structure = gst_sample_get_info (sample);

  if (!buf || !structure)
    return;

  if (gst_structure_has_name (structure, "application/vnd.iccprofile")) {
    if (!lcms->inp_profile_filename
        && lcms->lookup_method != GST_LCMS_LOOKUP_METHOD_UNCACHED) {
      GstMapInfo map;
      const gchar *icc_name;
      icc_name = gst_structure_get_string (structure, "icc-name");
      gst_buffer_map (buf, &map, GST_MAP_READ);
      lcms->cms_inp_profile = cmsOpenProfileFromMem (map.data, map.size);
      gst_buffer_unmap (buf, &map);
      if (!lcms->cms_inp_profile)
        GST_WARNING_OBJECT (lcms,
            "Couldn't parse embedded input ICC profile '%s'", icc_name);
      else {
        GST_DEBUG_OBJECT (lcms,
            "Successfully opened embedded input ICC profile '%s'", icc_name);
        if (lcms->cms_inp_format) {
          gst_lcms_create_transform (lcms);
          gst_lcms_init_lookup_table (lcms);
        }
      }
    } else {
      GST_DEBUG_OBJECT (lcms,
          "disregarding embedded ICC profile because input profile file was explicitly specified");
    }
  } else
    GST_DEBUG_OBJECT (lcms, "attachment is not an ICC profile");
}

static void
gst_lcms_handle_tags (GstLcms * lcms, GstTagList * taglist)
{
  guint tag_size;

  if (!taglist)
    return;

  tag_size = gst_tag_list_get_tag_size (taglist, GST_TAG_ATTACHMENT);
  if (tag_size > 0) {
    guint index;
    GstSample *sample;
    for (index = 0; index < tag_size; index++) {
      if (gst_tag_list_get_sample_index (taglist, GST_TAG_ATTACHMENT, index,
              &sample)) {
        gst_lcms_handle_tag_sample (lcms, sample);
        gst_sample_unref (sample);
      }
    }
  }
}

static GstFlowReturn
gst_lcms_transform_frame_ip (GstVideoFilter * vfilter, GstVideoFrame * inframe)
{
  GstLcms *lcms = GST_LCMS (vfilter);
  if (!gst_base_transform_is_passthrough (GST_BASE_TRANSFORM (lcms)))
    lcms->process (lcms, inframe, NULL);
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_lcms_transform_frame (GstVideoFilter * vfilter, GstVideoFrame * inframe,
    GstVideoFrame * outframe)
{
  GstLcms *lcms = GST_LCMS (vfilter);
  if (!gst_base_transform_is_passthrough (GST_BASE_TRANSFORM (lcms)))
    lcms->process (lcms, inframe, outframe);
  return GST_FLOW_OK;
}

static void
gst_lcms_process_rgb (GstLcms * lcms, GstVideoFrame * inframe,
    GstVideoFrame * outframe)
{
  gint height;
  gint width, in_stride, out_stride;
  gint in_pixel_stride, out_pixel_stride;
  gint in_offsets[4], out_offsets[4];
  guint8 *in_data, *out_data;
  gint i, j;
  gint in_row_wrap, out_row_wrap;
  guint8 alpha = 0;

  in_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (inframe, 0);
  in_stride = GST_VIDEO_FRAME_PLANE_STRIDE (inframe, 0);
  width = GST_VIDEO_FRAME_COMP_WIDTH (inframe, 0);
  height = GST_VIDEO_FRAME_COMP_HEIGHT (inframe, 0);
  in_pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (inframe, 0);

  in_offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 0);
  in_offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 1);
  in_offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 2);
  in_offsets[3] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 3);

  if (outframe) {
    if (width != GST_VIDEO_FRAME_COMP_WIDTH (outframe, 0)
        || height != GST_VIDEO_FRAME_COMP_HEIGHT (outframe, 0)) {
      GST_WARNING_OBJECT (lcms,
          "can't transform, input dimensions != output dimensions!");
      return;
    }
    out_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (outframe, 0);
    out_stride = GST_VIDEO_FRAME_PLANE_STRIDE (outframe, 0);
    out_pixel_stride = GST_VIDEO_FRAME_COMP_PSTRIDE (outframe, 0);
    out_offsets[0] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 0);
    out_offsets[1] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 1);
    out_offsets[2] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 2);
    out_offsets[3] = GST_VIDEO_FRAME_COMP_OFFSET (inframe, 3);
    GST_LOG_OBJECT (lcms,
        "transforming frame (%ix%i) stride=%i->%i pixel_stride=%i->%i format=%s->%s",
        width, height, in_stride, out_stride, in_pixel_stride, out_pixel_stride,
        gst_video_format_to_string (inframe->info.finfo->format),
        gst_video_format_to_string (outframe->info.finfo->format));
  } else {                      /* in-place transformation */
    GST_LOG_OBJECT (lcms,
        "transforming frame IN-PLACE (%ix%i) pixel_stride=%i format=%s", width,
        height, in_pixel_stride,
        gst_video_format_to_string (inframe->info.finfo->format));
    out_data = in_data;
    out_stride = in_stride;
    out_pixel_stride = in_pixel_stride;
    out_offsets[0] = in_offsets[0];
    out_offsets[1] = in_offsets[1];
    out_offsets[2] = in_offsets[2];
    out_offsets[3] = in_offsets[3];
  }

  in_row_wrap = in_stride - in_pixel_stride * width;
  out_row_wrap = out_stride - out_pixel_stride * width;

  if (lcms->lookup_method == GST_LCMS_LOOKUP_METHOD_UNCACHED) {
    if (!GST_VIDEO_FORMAT_INFO_HAS_ALPHA (inframe->info.finfo)
        && !lcms->preserve_black) {
      GST_DEBUG_OBJECT (lcms,
          "GST_LCMS_LOOKUP_METHOD_UNCACHED WITHOUT alpha AND WITHOUT preserve-black -> picture-at-once transformation!");
      cmsDoTransformStride (lcms->cms_transform, in_data, out_data,
          height * width, out_pixel_stride);
    } else {
      GST_DEBUG_OBJECT (lcms,
          "GST_LCMS_LOOKUP_METHOD_UNCACHED WITH alpha or preserve-black -> pixel-by-pixel transformation!");
      for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
          if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (inframe->info.finfo))
            alpha = in_data[in_offsets[3]];
          if (lcms->preserve_black && (in_data[in_offsets[0]] == 0x00)
              && (in_data[in_offsets[1]] == 0x00)
              && (in_data[in_offsets[2]] == 0x0))
            out_data[out_offsets[0]] = out_data[out_offsets[1]] =
                out_data[out_offsets[2]] = 0x00;
          else
            cmsDoTransformStride (lcms->cms_transform, in_data, out_data, 1,
                in_pixel_stride);
          if (alpha)
            out_data[in_offsets[3]] = alpha;
          in_data += in_pixel_stride;
          out_data += out_pixel_stride;
        }
        in_data += in_row_wrap;
        out_data += out_row_wrap;
      }
    }
  } else if (lcms->lookup_method == GST_LCMS_LOOKUP_METHOD_PRECALCULATED) {
    guint32 color, new_color;
    GST_LOG_OBJECT (lcms, "GST_LCMS_LOOKUP_METHOD_PRECALCULATED");
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        color =
            in_data[in_offsets[0]] |
            in_data[in_offsets[1]] << 0x08 | in_data[in_offsets[2]] << 0x10;
        new_color = lcms->color_lut[color];
        out_data[out_offsets[0]] = (new_color & 0x0000FF) >> 0x00;
        out_data[out_offsets[1]] = (new_color & 0x00FF00) >> 0x08;
        out_data[out_offsets[2]] = (new_color & 0xFF0000) >> 0x10;
        GST_TRACE_OBJECT (lcms,
            "(%i:%i)@%p original color 0x%08X (dest was 0x%08X)", i, j, in_data,
            color, new_color);
        if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (inframe->info.finfo)) {
          out_data[in_offsets[3]] = in_data[out_offsets[3]];
        }
        in_data += in_pixel_stride;
        out_data += out_pixel_stride;
      }
      in_data += in_row_wrap;
      out_data += out_row_wrap;
    }
  } else if (lcms->lookup_method == GST_LCMS_LOOKUP_METHOD_CACHED) {
    guint32 color, new_color;
    GST_LOG_OBJECT (lcms, "GST_LCMS_LOOKUP_METHOD_CACHED");
    for (i = 0; i < height; i++) {
      for (j = 0; j < width; j++) {
        if (GST_VIDEO_FORMAT_INFO_HAS_ALPHA (inframe->info.finfo))
          alpha = in_data[in_offsets[3]];
        color =
            in_data[in_offsets[0]] |
            in_data[in_offsets[1]] << 0x08 | in_data[in_offsets[2]] << 0x10;
        new_color = lcms->color_lut[color];
        if (new_color == 0xAAAAAAAA) {
          cmsDoTransform (lcms->cms_transform, in_data, out_data, 1);
          new_color =
              out_data[out_offsets[0]] |
              out_data[out_offsets[1]] << 0x08 |
              out_data[out_offsets[2]] << 0x10;
          GST_OBJECT_LOCK (lcms);
          lcms->color_lut[color] = new_color;
          GST_OBJECT_UNLOCK (lcms);
          GST_TRACE_OBJECT (lcms, "cached color 0x%08X -> 0x%08X", color,
              new_color);
        } else {
          out_data[out_offsets[0]] = (new_color & 0x0000FF) >> 0x00;
          out_data[out_offsets[1]] = (new_color & 0x00FF00) >> 0x08;
          out_data[out_offsets[2]] = (new_color & 0xFF0000) >> 0x10;
        }
        if (alpha) {
          out_data[in_offsets[3]] = alpha;
        }
        in_data += in_pixel_stride;
        out_data += out_pixel_stride;
      }
      in_data += in_row_wrap;
      out_data += out_row_wrap;
    }
  }
}
