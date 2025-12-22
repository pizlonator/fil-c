/* GStreamer
 * Copyright (C) 2021 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-vadeinterlace
 * @title: vadeinterlace
 * @short_description: A VA-API base video deinterlace filter
 *
 * vadeinterlace deinterlaces interlaced video frames to progressive
 * video frames. This element and its deinterlacing methods depend on
 * the installed and chosen [VA-API](https://01.org/linuxmedia/vaapi)
 * driver, but it's usually avaialble with bob (linear) method.
 *
 * This element doesn't change the caps features, it only negotiates
 * the same dowstream and upstream.
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 filesrc location=interlaced_video.mp4 ! parsebin ! vah264dec ! vadeinterlace ! vapostproc ! autovideosink
 * ```
 *
 * Since: 1.20
 *
 */

/* ToDo:
 *
 * + field property to select only one field and keep the same framerate
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstvadeinterlace.h"

#include <gst/va/gstva.h>
#include <gst/video/video.h>
#include <va/va_drmcommon.h>

#include "gstvabasetransform.h"
#include "gstvacaps.h"
#include "gstvadisplay_priv.h"
#include "gstvafilter.h"
#include "gstvapluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_va_deinterlace_debug);
#define GST_CAT_DEFAULT gst_va_deinterlace_debug

#define GST_VA_DEINTERLACE(obj)           ((GstVaDeinterlace *) obj)
#define GST_VA_DEINTERLACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_FROM_INSTANCE (obj), GstVaDeinterlaceClass))
#define GST_VA_DEINTERLACE_CLASS(klass)    ((GstVaDeinterlaceClass *) klass)

typedef struct _GstVaDeinterlace GstVaDeinterlace;
typedef struct _GstVaDeinterlaceClass GstVaDeinterlaceClass;

enum CurrField
{
  UNKNOWN_FIELD,
  FIRST_FIELD,
  SECOND_FIELD,
  FINISHED,
};

struct _GstVaDeinterlaceClass
{
  /* GstVideoFilter overlaps functionality */
  GstVaBaseTransformClass parent_class;
};

struct _GstVaDeinterlace
{
  GstVaBaseTransform parent;

  gboolean rebuild_filters;
  VAProcDeinterlacingType method;

  guint num_backward_references;

  GstBuffer *history[8];
  gint hcount;
  gint hdepth;
  gint hcurr;
  enum CurrField curr_field;

  /* Calculated buffer duration by using upstream framerate */
  GstClockTime default_duration;
};

static GstElementClass *parent_class = NULL;

struct CData
{
  gchar *render_device_path;
  gchar *description;
};

/* *INDENT-OFF* */
static const gchar *caps_str =
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VA,
        "{ NV12, I420, YV12, YUY2, RGBA, BGRA, P010_10LE, ARGB, ABGR }") " ;"
    GST_VIDEO_CAPS_MAKE ("{ VUYA, GRAY8, NV12, NV21, YUY2, UYVY, YV12, "
        "I420, P010_10LE, RGBA, BGRA, ARGB, ABGR  }");
/* *INDENT-ON* */

static void
_reset_history (GstVaDeinterlace * self)
{
  gint i;

  for (i = 0; i < self->hcount; i++)
    gst_buffer_unref (self->history[i]);
  self->hcount = 0;
}

static void
gst_va_deinterlace_dispose (GObject * object)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (object);

  _reset_history (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_va_deinterlace_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (object);
  guint method;

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case GST_VA_FILTER_PROP_DEINTERLACE_METHOD:
      method = g_value_get_enum (value);
      if (method != self->method) {
        self->method = method;
        g_atomic_int_set (&self->rebuild_filters, TRUE);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static void
gst_va_deinterlace_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (object);

  GST_OBJECT_LOCK (object);
  switch (prop_id) {
    case GST_VA_FILTER_PROP_DEINTERLACE_METHOD:{
      g_value_set_enum (value, self->method);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (object);
}

static GstFlowReturn
gst_va_deinterlace_submit_input_buffer (GstBaseTransform * trans,
    gboolean is_discont, GstBuffer * input)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstBuffer *buf, *inbuf;
  GstFlowReturn ret;
  gint i;

  /* Let baseclass handle QoS first */
  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->submit_input_buffer (trans,
      is_discont, input);
  if (ret != GST_FLOW_OK)
    return ret;

  if (gst_base_transform_is_passthrough (trans))
    return ret;

  /* at this moment, baseclass must hold queued_buf */
  g_assert (trans->queued_buf != NULL);

  /* Check if we can use this buffer directly. If not, copy this into
   * our fallback buffer */
  buf = trans->queued_buf;
  trans->queued_buf = NULL;

  ret = gst_va_base_transform_import_buffer (btrans, buf, &inbuf);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_buffer_unref (buf);

  if (self->hcount < self->hdepth) {
    self->history[self->hcount++] = inbuf;
  } else {
    gst_clear_buffer (&self->history[0]);
    for (i = 0; i + 1 < self->hcount; i++)
      self->history[i] = self->history[i + 1];
    self->history[i] = inbuf;
  }

  if (self->history[self->hcurr])
    self->curr_field = FIRST_FIELD;

  return ret;
}

static void
_build_filter (GstVaDeinterlace * self)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);
  guint i, num_caps;
  VAProcFilterCapDeinterlacing *caps;
  guint32 num_forward_references;

  caps = gst_va_filter_get_filter_caps (btrans->filter,
      VAProcFilterDeinterlacing, &num_caps);
  if (!caps)
    return;

  for (i = 0; i < num_caps; i++) {
    if (caps[i].type != self->method)
      continue;

    if (gst_va_filter_add_deinterlace_buffer (btrans->filter, self->method,
            &num_forward_references, &self->num_backward_references)) {
      self->hdepth = num_forward_references + self->num_backward_references + 1;
      if (self->hdepth > 8) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            ("Pipeline requires too many references: (%u forward, %u backward)",
                num_forward_references, self->num_backward_references), (NULL));
      }
      GST_INFO_OBJECT (self, "References for method: %u forward / %u backward",
          num_forward_references, self->num_backward_references);
      self->hcurr = num_forward_references;
      return;
    }
  }

  GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS,
      ("Invalid deinterlacing method: %d", self->method), (NULL));
}

static void
gst_va_deinterlace_rebuild_filters (GstVaDeinterlace * self)
{
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (self);

  if (!g_atomic_int_get (&self->rebuild_filters))
    return;

  _reset_history (self);
  gst_va_filter_drop_filter_buffers (btrans->filter);
  _build_filter (self);

  /* extra number of buffers for propose_allocation */
  if (self->hdepth > btrans->extra_min_buffers) {
    btrans->extra_min_buffers = self->hdepth;
    gst_base_transform_reconfigure_sink (GST_BASE_TRANSFORM (self));
  }

  g_atomic_int_set (&self->rebuild_filters, FALSE);
}

static gboolean
gst_va_deinterlace_set_info (GstVaBaseTransform * btrans, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (btrans);
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (btrans);

  switch (GST_VIDEO_INFO_INTERLACE_MODE (in_info)) {
    case GST_VIDEO_INTERLACE_MODE_PROGRESSIVE:
      /* Nothing to do */
      gst_base_transform_set_passthrough (trans, TRUE);
      return TRUE;
      break;
    case GST_VIDEO_INTERLACE_MODE_ALTERNATE:
    case GST_VIDEO_INTERLACE_MODE_FIELDS:
      GST_ERROR_OBJECT (self, "Unsupported interlace mode.");
      return FALSE;
      break;
    default:
      break;
  }

  /* Calculate expected buffer duration. We might need to reference this value
   * when buffer duration is unknown */
  if (GST_VIDEO_INFO_FPS_N (in_info) > 0 && GST_VIDEO_INFO_FPS_D (in_info) > 0) {
    self->default_duration =
        gst_util_uint64_scale_int (GST_SECOND, GST_VIDEO_INFO_FPS_D (in_info),
        GST_VIDEO_INFO_FPS_N (in_info));
  } else {
    /* Assume 25 fps. We need this for reporting latency at least  */
    self->default_duration = gst_util_uint64_scale_int (GST_SECOND, 1, 25);
  }

  if (gst_va_filter_set_video_info (btrans->filter, in_info, out_info)) {
    g_atomic_int_set (&self->rebuild_filters, TRUE);
    gst_base_transform_set_passthrough (trans, FALSE);
    gst_va_deinterlace_rebuild_filters (self);

    return TRUE;
  }

  return FALSE;
}

static void
gst_va_deinterlace_before_transform (GstBaseTransform * trans,
    GstBuffer * inbuf)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstClockTime ts, stream_time;

  ts = GST_BUFFER_TIMESTAMP (inbuf);
  stream_time =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, ts);

  GST_TRACE_OBJECT (self, "sync to %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (self), stream_time);

  gst_va_deinterlace_rebuild_filters (self);
}

static void
_set_field (GstVaDeinterlace * self, guint32 * surface_flags)
{
  GstBaseTransform *trans = GST_BASE_TRANSFORM (self);

  if (trans->segment.rate < 0) {
    if ((self->curr_field == FIRST_FIELD
            && (*surface_flags & VA_TOP_FIELD_FIRST))
        || (self->curr_field == SECOND_FIELD
            && (*surface_flags & VA_BOTTOM_FIELD_FIRST))) {
      *surface_flags |= VA_BOTTOM_FIELD;
    } else {
      *surface_flags |= VA_TOP_FIELD;
    }
  } else {
    if ((self->curr_field == FIRST_FIELD
            && (*surface_flags & VA_BOTTOM_FIELD_FIRST))
        || (self->curr_field == SECOND_FIELD
            && (*surface_flags & VA_TOP_FIELD_FIRST))) {
      *surface_flags |= VA_BOTTOM_FIELD;
    } else {
      *surface_flags |= VA_TOP_FIELD;
    }
  }
}

static GstFlowReturn
gst_va_deinterlace_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  GstFlowReturn res = GST_FLOW_OK;
  GstVaSample src, dst;
  GstVideoInfo *info = &btrans->in_info;
  VASurfaceID forward_references[8], backward_references[8];
  guint i, surface_flags;

  if (G_UNLIKELY (!btrans->negotiated))
    goto unknown_format;

  g_assert (self->curr_field == FIRST_FIELD
      || self->curr_field == SECOND_FIELD);

  surface_flags = gst_va_buffer_get_surface_flags (inbuf, info);
  if (surface_flags != VA_FRAME_PICTURE)
    _set_field (self, &surface_flags);

  GST_TRACE_OBJECT (self, "Processing %d field (flags = %u): %" GST_PTR_FORMAT,
      self->curr_field, surface_flags, inbuf);

  for (i = 0; i < self->hcurr; i++) {
    forward_references[i] =
        gst_va_buffer_get_surface (self->history[self->hcurr - i - 1]);
  }
  for (i = 0; i < self->num_backward_references; i++) {
    backward_references[i] =
        gst_va_buffer_get_surface (self->history[self->hcurr + i + 1]);
  }

  /* *INDENT-OFF* */
  src = (GstVaSample) {
    .buffer = inbuf,
    .flags = surface_flags,
    .forward_references = forward_references,
    .num_forward_references = self->hcurr,
    .backward_references = backward_references,
    .num_backward_references = self->num_backward_references,
  };
  dst = (GstVaSample) {
    .buffer = outbuf,
  };
  /* *INDENT-ON* */

  if (!gst_va_filter_process (btrans->filter, &src, &dst)) {
    gst_buffer_set_flags (outbuf, GST_BUFFER_FLAG_CORRUPTED);
    res = GST_BASE_TRANSFORM_FLOW_DROPPED;
  }

  return res;

  /* ERRORS */
unknown_format:
  {
    GST_ELEMENT_ERROR (self, CORE, NOT_IMPLEMENTED, (NULL), ("unknown format"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static GstFlowReturn
gst_va_deinterlace_generate_output (GstBaseTransform * trans,
    GstBuffer ** outbuf)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstFlowReturn ret;
  GstBuffer *inbuf, *buf = NULL;

  if (gst_base_transform_is_passthrough (trans)) {
    return GST_BASE_TRANSFORM_CLASS (parent_class)->generate_output (trans,
        outbuf);
  }

  *outbuf = NULL;

  if (self->curr_field == FINISHED)
    return GST_FLOW_OK;

  inbuf = self->history[self->hcurr];
  if (!inbuf)
    return GST_FLOW_OK;

  if (!self->history[self->hdepth - 1])
    return GST_FLOW_OK;

  ret = GST_BASE_TRANSFORM_CLASS (parent_class)->prepare_output_buffer (trans,
      inbuf, &buf);
  if (ret != GST_FLOW_OK || !buf) {
    GST_WARNING_OBJECT (self, "Could not get buffer from pool: %s",
        gst_flow_get_name (ret));
    return ret;
  }

  ret = gst_va_deinterlace_transform (trans, inbuf, buf);
  if (ret != GST_FLOW_OK) {
    gst_buffer_unref (buf);
    return ret;
  }

  if (!GST_BUFFER_PTS_IS_VALID (inbuf)) {
    GST_LOG_OBJECT (self, "Input buffer timestamp is unknown");
  } else {
    GstClockTime duration;

    if (GST_BUFFER_DURATION_IS_VALID (inbuf))
      duration = GST_BUFFER_DURATION (inbuf) / 2;
    else
      duration = self->default_duration / 2;

    GST_BUFFER_DURATION (buf) = duration;
    if (self->curr_field == SECOND_FIELD)
      GST_BUFFER_PTS (buf) = GST_BUFFER_PTS (buf) + duration;
  }

  *outbuf = buf;

  GST_TRACE_OBJECT (self, "Pushing %" GST_PTR_FORMAT, buf);

  if (self->curr_field == FIRST_FIELD)
    self->curr_field = SECOND_FIELD;
  else if (self->curr_field == SECOND_FIELD)
    self->curr_field = FINISHED;

  return ret;
}

static GstCaps *
gst_va_deinterlace_remove_interlace (GstCaps * caps)
{
  GstStructure *st;
  gint i, n;
  GstCaps *res;
  GstCapsFeatures *f;

  res = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    st = gst_caps_get_structure (caps, i);
    f = gst_caps_get_features (caps, i);

    /* If this is already expressed by the existing caps
     * skip this structure */
    if (i > 0 && gst_caps_is_subset_structure_full (res, st, f))
      continue;

    st = gst_structure_copy (st);
    gst_structure_remove_fields (st, "interlace-mode", "field-order",
        "framerate", NULL);

    gst_caps_append_structure_full (res, st, gst_caps_features_copy (f));
  }

  return res;
}

static GstCaps *
gst_va_deinterlace_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstVaBaseTransform *btrans = GST_VA_BASE_TRANSFORM (trans);
  GstCaps *ret, *filter_caps;

  GST_DEBUG_OBJECT (self,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  filter_caps = gst_va_base_transform_get_filter_caps (btrans);
  if (filter_caps && !gst_caps_can_intersect (caps, filter_caps)) {
    ret = gst_caps_ref (caps);
    goto bail;
  }

  ret = gst_va_deinterlace_remove_interlace (caps);

bail:
  if (filter) {
    GstCaps *intersection;

    intersection =
        gst_caps_intersect_full (filter, ret, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (ret);
    ret = intersection;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

static GstCaps *
gst_va_deinterlace_fixate_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * othercaps)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);
  GstCapsFeatures *out_f;
  GstStructure *in_s, *out_s;
  gint fps_n, fps_d;
  const gchar *in_interlace_mode, *out_interlace_mode;

  GST_DEBUG_OBJECT (self,
      "trying to fixate othercaps %" GST_PTR_FORMAT " based on caps %"
      GST_PTR_FORMAT, othercaps, caps);

  othercaps = gst_caps_truncate (othercaps);
  othercaps = gst_caps_make_writable (othercaps);

  if (direction == GST_PAD_SRC) {
    othercaps = gst_caps_fixate (othercaps);
    goto bail;
  }

  in_s = gst_caps_get_structure (caps, 0);
  in_interlace_mode = gst_structure_get_string (in_s, "interlace-mode");

  out_s = gst_caps_get_structure (othercaps, 0);

  if (g_strcmp0 ("progressive", in_interlace_mode) == 0) {
    /* Just forward interlace-mode=progressive and framerate
     * By this way, basetransform will enable passthrough for non-interlaced
     * stream */
    const GValue *framerate = gst_structure_get_value (in_s, "framerate");
    gst_structure_set_value (out_s, "framerate", framerate);
    gst_structure_set (out_s, "interlace-mode", G_TYPE_STRING, "progressive",
        NULL);

    goto bail;
  }

  out_f = gst_caps_get_features (othercaps, 0);
  out_interlace_mode = gst_structure_get_string (out_s, "interlace-mode");

  if ((!out_interlace_mode
          || (g_strcmp0 ("progressive", out_interlace_mode) == 0))
      && (gst_caps_features_contains (out_f, GST_CAPS_FEATURE_MEMORY_VA)
          || gst_caps_features_contains (out_f, GST_CAPS_FEATURE_MEMORY_DMABUF)
          || gst_caps_features_contains (out_f,
              GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY))) {
    gst_structure_set (out_s, "interlace-mode", G_TYPE_STRING, "progressive",
        NULL);

    if (gst_structure_get_fraction (in_s, "framerate", &fps_n, &fps_d)) {
      fps_n *= 2;
      gst_structure_set (out_s, "framerate", GST_TYPE_FRACTION, fps_n, fps_d,
          NULL);
    }
  } else {
    /* if caps features aren't supported, just forward interlace-mode
     * and framerate */
    const GValue *framerate = gst_structure_get_value (in_s, "framerate");
    gst_structure_set_value (out_s, "framerate", framerate);
    gst_structure_set (out_s, "interlace-mode", G_TYPE_STRING,
        in_interlace_mode, NULL);
  }

bail:
  GST_DEBUG_OBJECT (self, "fixated othercaps to %" GST_PTR_FORMAT, othercaps);

  return othercaps;
}

static gboolean
gst_va_deinterlace_query (GstBaseTransform * trans, GstPadDirection direction,
    GstQuery * query)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (trans);

  if (direction == GST_PAD_SRC && GST_QUERY_TYPE (query) == GST_QUERY_LATENCY
      && !gst_base_transform_is_passthrough (trans)) {
    GstPad *peer;
    GstClockTime latency, min, max;
    gboolean res = FALSE;
    gboolean live;

    peer = gst_pad_get_peer (GST_BASE_TRANSFORM_SINK_PAD (trans));
    if (!peer)
      return FALSE;

    res = gst_pad_query (peer, query);
    gst_object_unref (peer);
    if (!res)
      return FALSE;

    gst_query_parse_latency (query, &live, &min, &max);

    GST_DEBUG_OBJECT (self, "Peer latency: min %" GST_TIME_FORMAT " max %"
        GST_TIME_FORMAT, GST_TIME_ARGS (min), GST_TIME_ARGS (max));

    /* add our own latency: number of fields + history depth */
    latency = (2 + self->hdepth) * self->default_duration;

    GST_DEBUG_OBJECT (self, "Our latency: min %" GST_TIME_FORMAT ", max %"
        GST_TIME_FORMAT, GST_TIME_ARGS (latency), GST_TIME_ARGS (latency));

    min += latency;
    if (max != GST_CLOCK_TIME_NONE)
      max += latency;

    GST_DEBUG_OBJECT (self, "Calculated total latency : min %" GST_TIME_FORMAT
        " max %" GST_TIME_FORMAT, GST_TIME_ARGS (min), GST_TIME_ARGS (max));

    gst_query_set_latency (query, live, min, max);

    return TRUE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static void
gst_va_deinterlace_class_init (gpointer g_class, gpointer class_data)
{
  GstCaps *doc_caps, *sink_caps = NULL, *src_caps = NULL;
  GstPadTemplate *sink_pad_templ, *src_pad_templ;
  GObjectClass *object_class = G_OBJECT_CLASS (g_class);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (g_class);
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstVaBaseTransformClass *btrans_class = GST_VA_BASE_TRANSFORM_CLASS (g_class);
  GstVaDisplay *display;
  GstVaFilter *filter;
  struct CData *cdata = class_data;
  gchar *long_name;

  parent_class = g_type_class_peek_parent (g_class);

  btrans_class->render_device_path = g_strdup (cdata->render_device_path);

  if (cdata->description) {
    long_name = g_strdup_printf ("VA-API Deinterlacer in %s",
        cdata->description);
  } else {
    long_name = g_strdup ("VA-API Deinterlacer");
  }

  gst_element_class_set_metadata (element_class, long_name,
      "Filter/Effect/Video/Deinterlace",
      "VA-API based deinterlacer", "Víctor Jáquez <vjaquez@igalia.com>");

  display = gst_va_display_platform_new (btrans_class->render_device_path);
  filter = gst_va_filter_new (display);

  if (gst_va_filter_open (filter)) {
    src_caps = gst_va_filter_get_caps (filter);
    /* adds any to enable passthrough */
    {
      GstCaps *any_caps = gst_caps_new_empty_simple ("video/x-raw");
      gst_caps_set_features_simple (any_caps, gst_caps_features_new_any ());
      src_caps = gst_caps_merge (src_caps, any_caps);
    }
  } else {
    src_caps = gst_caps_from_string (caps_str);
  }

  sink_caps = gst_va_deinterlace_remove_interlace (src_caps);

  doc_caps = gst_caps_from_string (caps_str);

  sink_pad_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      sink_caps);
  gst_element_class_add_pad_template (element_class, sink_pad_templ);
  gst_pad_template_set_documentation_caps (sink_pad_templ,
      gst_caps_ref (doc_caps));

  src_pad_templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      src_caps);
  gst_element_class_add_pad_template (element_class, src_pad_templ);
  gst_pad_template_set_documentation_caps (src_pad_templ,
      gst_caps_ref (doc_caps));
  gst_caps_unref (doc_caps);

  gst_caps_unref (src_caps);
  gst_caps_unref (sink_caps);

  object_class->dispose = gst_va_deinterlace_dispose;
  object_class->set_property = gst_va_deinterlace_set_property;
  object_class->get_property = gst_va_deinterlace_get_property;

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_va_deinterlace_transform_caps);
  trans_class->fixate_caps = GST_DEBUG_FUNCPTR (gst_va_deinterlace_fixate_caps);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_va_deinterlace_before_transform);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_va_deinterlace_transform);
  trans_class->submit_input_buffer =
      GST_DEBUG_FUNCPTR (gst_va_deinterlace_submit_input_buffer);
  trans_class->generate_output =
      GST_DEBUG_FUNCPTR (gst_va_deinterlace_generate_output);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_va_deinterlace_query);

  trans_class->transform_ip_on_passthrough = FALSE;

  btrans_class->set_info = GST_DEBUG_FUNCPTR (gst_va_deinterlace_set_info);

  gst_va_filter_install_deinterlace_properties (filter, object_class);

  g_free (long_name);
  g_free (cdata->description);
  g_free (cdata->render_device_path);
  g_free (cdata);
  gst_object_unref (filter);
  gst_object_unref (display);
}

static void
gst_va_deinterlace_init (GTypeInstance * instance, gpointer g_class)
{
  GstVaDeinterlace *self = GST_VA_DEINTERLACE (instance);
  GParamSpec *pspec;

  pspec = g_object_class_find_property (g_class, "method");
  g_assert (pspec);
  self->method = g_value_get_enum (g_param_spec_get_default_value (pspec));
}

static gpointer
_register_debug_category (gpointer data)
{
  GST_DEBUG_CATEGORY_INIT (gst_va_deinterlace_debug, "vadeinterlace", 0,
      "VA Video Deinterlace");

  return NULL;
}

gboolean
gst_va_deinterlace_register (GstPlugin * plugin, GstVaDevice * device,
    guint rank)
{
  static GOnce debug_once = G_ONCE_INIT;
  GType type;
  GTypeInfo type_info = {
    .class_size = sizeof (GstVaDeinterlaceClass),
    .class_init = gst_va_deinterlace_class_init,
    .instance_size = sizeof (GstVaDeinterlace),
    .instance_init = gst_va_deinterlace_init,
  };
  struct CData *cdata;
  gboolean ret;
  gchar *type_name, *feature_name;

  g_return_val_if_fail (GST_IS_PLUGIN (plugin), FALSE);
  g_return_val_if_fail (GST_IS_VA_DEVICE (device), FALSE);

  cdata = g_new (struct CData, 1);
  cdata->description = NULL;
  cdata->render_device_path = g_strdup (device->render_device_path);

  type_info.class_data = cdata;

  gst_va_create_feature_name (device, "GstVaDeinterlace", "GstVa%sDeinterlace",
      &type_name, "vadeinterlace", "va%sdeinterlace", &feature_name,
      &cdata->description, &rank);

  g_once (&debug_once, _register_debug_category, NULL);

  type = g_type_register_static (GST_TYPE_VA_BASE_TRANSFORM, type_name,
      &type_info, 0);

  ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
