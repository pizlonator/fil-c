/* GStreamer
 * Copyright (C) 2008 David Schleef <ds@schleef.org>
 * Copyright (C) 2012 Collabora Ltd.
 *	Author : Edward Hervey <edward@collabora.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>
#include "gstvideoutilsprivate.h"

/*
 * Takes caps and copies its video fields to tmpl_caps
 */
static GstCaps *
__gst_video_element_proxy_caps (GstElement * element, GstCaps * templ_caps,
    GstCaps * caps)
{
  GstCaps *result = gst_caps_new_empty ();
  gint i, j;
  gint templ_caps_size = gst_caps_get_size (templ_caps);
  gint caps_size = gst_caps_get_size (caps);

  for (i = 0; i < templ_caps_size; i++) {
    GQuark q_name =
        gst_structure_get_name_id (gst_caps_get_structure (templ_caps, i));
    GstCapsFeatures *features = gst_caps_get_features (templ_caps, i);

    for (j = 0; j < caps_size; j++) {
      const GstStructure *caps_s = gst_caps_get_structure (caps, j);
      const GValue *val;
      GstStructure *s;
      GstCaps *tmp = gst_caps_new_empty ();

      s = gst_structure_new_id_empty (q_name);
      if ((val = gst_structure_get_value (caps_s, "width")))
        gst_structure_set_value (s, "width", val);
      if ((val = gst_structure_get_value (caps_s, "height")))
        gst_structure_set_value (s, "height", val);
      if ((val = gst_structure_get_value (caps_s, "framerate")))
        gst_structure_set_value (s, "framerate", val);
      if ((val = gst_structure_get_value (caps_s, "pixel-aspect-ratio")))
        gst_structure_set_value (s, "pixel-aspect-ratio", val);
      if ((val = gst_structure_get_value (caps_s, "colorimetry")))
        gst_structure_set_value (s, "colorimetry", val);
      if ((val = gst_structure_get_value (caps_s, "chroma-site")))
        gst_structure_set_value (s, "chroma-site", val);

      gst_caps_append_structure_full (tmp, s,
          gst_caps_features_copy (features));
      result = gst_caps_merge (result, tmp);
    }
  }

  return result;
}

/**
 * __gst_video_element_proxy_getcaps:
 * @element: a #GstElement
 * @sinkpad: the element's sink #GstPad
 * @srcpad: the element's source #GstPad
 * @initial_caps: initial caps
 * @filter: filter caps
 *
 * Returns caps that express @initial_caps (or sink template caps if
 * @initial_caps == NULL) restricted to resolution/format/...
 * combinations supported by downstream elements (e.g. muxers).
 *
 * Returns: a #GstCaps owned by caller
 */
GstCaps *
__gst_video_element_proxy_getcaps (GstElement * element, GstPad * sinkpad,
    GstPad * srcpad, GstCaps * initial_caps, GstCaps * filter)
{
  GstCaps *templ_caps, *src_templ_caps;
  GstCaps *peer_caps;
  GstCaps *allowed;
  GstCaps *fcaps, *filter_caps;

  /* Allow downstream to specify width/height/framerate/PAR constraints
   * and forward them upstream for video converters to handle
   */
  templ_caps = initial_caps ? gst_caps_ref (initial_caps) :
      gst_pad_get_pad_template_caps (sinkpad);
  src_templ_caps = gst_pad_get_pad_template_caps (srcpad);
  if (filter && !gst_caps_is_any (filter)) {
    GstCaps *proxy_filter =
        __gst_video_element_proxy_caps (element, src_templ_caps, filter);

    peer_caps = gst_pad_peer_query_caps (srcpad, proxy_filter);
    gst_caps_unref (proxy_filter);
  } else {
    peer_caps = gst_pad_peer_query_caps (srcpad, NULL);
  }

  allowed = gst_caps_intersect_full (peer_caps, src_templ_caps,
      GST_CAPS_INTERSECT_FIRST);

  gst_caps_unref (src_templ_caps);
  gst_caps_unref (peer_caps);

  if (!allowed || gst_caps_is_any (allowed)) {
    fcaps = templ_caps;
    goto done;
  } else if (gst_caps_is_empty (allowed)) {
    fcaps = gst_caps_ref (allowed);
    goto done;
  }

  GST_LOG_OBJECT (element, "template caps %" GST_PTR_FORMAT, templ_caps);
  GST_LOG_OBJECT (element, "allowed caps %" GST_PTR_FORMAT, allowed);

  filter_caps = __gst_video_element_proxy_caps (element, templ_caps, allowed);

  fcaps = gst_caps_intersect (filter_caps, templ_caps);
  gst_caps_unref (filter_caps);
  gst_caps_unref (templ_caps);

  if (filter) {
    GST_LOG_OBJECT (element, "intersecting with %" GST_PTR_FORMAT, filter);
    filter_caps = gst_caps_intersect (fcaps, filter);
    gst_caps_unref (fcaps);
    fcaps = filter_caps;
  }

done:
  gst_caps_replace (&allowed, NULL);

  GST_LOG_OBJECT (element, "proxy caps %" GST_PTR_FORMAT, fcaps);

  return fcaps;
}

gboolean
__gst_video_rawvideo_convert (GstVideoCodecState * state,
    GstFormat src_format, gint64 src_value,
    GstFormat * dest_format, gint64 * dest_value)
{
  gboolean res = FALSE;
  guint vidsize;
  guint fps_n, fps_d;

  g_return_val_if_fail (dest_format != NULL, FALSE);
  g_return_val_if_fail (dest_value != NULL, FALSE);

  if (src_format == *dest_format || src_value == 0 || src_value == -1) {
    *dest_value = src_value;
    return TRUE;
  }

  vidsize = GST_VIDEO_INFO_SIZE (&state->info);
  fps_n = GST_VIDEO_INFO_FPS_N (&state->info);
  fps_d = GST_VIDEO_INFO_FPS_D (&state->info);

  if (src_format == GST_FORMAT_BYTES &&
      *dest_format == GST_FORMAT_DEFAULT && vidsize) {
    /* convert bytes to frames */
    *dest_value = gst_util_uint64_scale_int (src_value, 1, vidsize);
    res = TRUE;
  } else if (src_format == GST_FORMAT_DEFAULT &&
      *dest_format == GST_FORMAT_BYTES && vidsize) {
    /* convert bytes to frames */
    *dest_value = src_value * vidsize;
    res = TRUE;
  } else if (src_format == GST_FORMAT_DEFAULT &&
      *dest_format == GST_FORMAT_TIME && fps_n) {
    /* convert frames to time */
    *dest_value = gst_util_uint64_scale (src_value, GST_SECOND * fps_d, fps_n);
    res = TRUE;
  } else if (src_format == GST_FORMAT_TIME &&
      *dest_format == GST_FORMAT_DEFAULT && fps_d) {
    /* convert time to frames */
    *dest_value = gst_util_uint64_scale (src_value, fps_n, GST_SECOND * fps_d);
    res = TRUE;
  } else if (src_format == GST_FORMAT_TIME &&
      *dest_format == GST_FORMAT_BYTES && fps_d && vidsize) {
    /* convert time to bytes */
    *dest_value = gst_util_uint64_scale (src_value,
        fps_n * (guint64) vidsize, GST_SECOND * fps_d);
    res = TRUE;
  } else if (src_format == GST_FORMAT_BYTES &&
      *dest_format == GST_FORMAT_TIME && fps_n && vidsize) {
    /* convert bytes to time */
    *dest_value = gst_util_uint64_scale (src_value,
        GST_SECOND * fps_d, fps_n * (guint64) vidsize);
    res = TRUE;
  }

  return res;
}

gboolean
__gst_video_encoded_video_convert (gint64 bytes, gint64 time,
    GstFormat src_format, gint64 src_value, GstFormat * dest_format,
    gint64 * dest_value)
{
  gboolean res = FALSE;

  g_return_val_if_fail (dest_format != NULL, FALSE);
  g_return_val_if_fail (dest_value != NULL, FALSE);

  if (G_UNLIKELY (src_format == *dest_format || src_value == 0 ||
          src_value == -1)) {
    if (dest_value)
      *dest_value = src_value;
    return TRUE;
  }

  if (bytes <= 0 || time <= 0) {
    GST_DEBUG ("not enough metadata yet to convert");
    goto exit;
  }

  switch (src_format) {
    case GST_FORMAT_BYTES:
      switch (*dest_format) {
        case GST_FORMAT_TIME:
          *dest_value = gst_util_uint64_scale (src_value, time, bytes);
          res = TRUE;
          break;
        default:
          res = FALSE;
      }
      break;
    case GST_FORMAT_TIME:
      switch (*dest_format) {
        case GST_FORMAT_BYTES:
          *dest_value = gst_util_uint64_scale (src_value, bytes, time);
          res = TRUE;
          break;
        default:
          res = FALSE;
      }
      break;
    default:
      GST_DEBUG ("unhandled conversion from %d to %d", src_format,
          *dest_format);
      res = FALSE;
  }

exit:
  return res;
}
