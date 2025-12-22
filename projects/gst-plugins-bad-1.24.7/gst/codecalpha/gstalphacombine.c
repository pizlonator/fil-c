/* GStreamer
 * Copyright (C) <2021> Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-alphacombine
 * @title: Alpha Combiner
 *
 * This element can combine a Luma plane from one stream as being the alpha
 * plane of another stream. This element can only work with planar formats
 * that have an equivalent format with an alpha plane. This is notably used to
 * combine VP8/VP9 alpha streams from WebM container.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v videotestsrc ! c. videotestsrc pattern=ball ! c.
 *     alphacombine name=c ! compositor ! videoconvert ! autovideosink
 * ]| This pipeline uses luma of a ball test pattern as alpha, combined with
 * default test pattern and renders the resulting moving ball on a checker
 * board.
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/video/video.h>

#include "gstalphacombine.h"


#define SUPPORTED_SINK_FORMATS "{ I420, I420_10LE, NV12 }"
#define SUPPORTED_ALPHA_FORMATS "{ GRAY8, I420, I420_10LE, NV12 }"
#define SUPPORTED_SRC_FORMATS "{ A420, A420_10LE, AV12 }"

/* *INDENT-OFF* */
struct {
  GstVideoFormat sink;
  GstVideoFormat alpha;
  GstVideoFormat src;
} format_map[] = {
  {
    .sink = GST_VIDEO_FORMAT_I420,
    .alpha = GST_VIDEO_FORMAT_I420,
    .src = GST_VIDEO_FORMAT_A420
  },{
    .sink = GST_VIDEO_FORMAT_I420,
    .alpha = GST_VIDEO_FORMAT_GRAY8,
    .src = GST_VIDEO_FORMAT_A420
 },{
    .sink = GST_VIDEO_FORMAT_I420,
    .alpha = GST_VIDEO_FORMAT_NV12,
    .src = GST_VIDEO_FORMAT_A420
  }, {
    .sink = GST_VIDEO_FORMAT_NV12,
    .alpha = GST_VIDEO_FORMAT_NV12,
    .src = GST_VIDEO_FORMAT_AV12,
  }, {
    .sink = GST_VIDEO_FORMAT_NV12,
    .alpha = GST_VIDEO_FORMAT_GRAY8,
    .src = GST_VIDEO_FORMAT_AV12
 },{
    .sink = GST_VIDEO_FORMAT_NV12,
    .alpha = GST_VIDEO_FORMAT_I420,
    .src = GST_VIDEO_FORMAT_AV12
 },{
    .sink = GST_VIDEO_FORMAT_I420_10LE,
    .alpha = GST_VIDEO_FORMAT_I420_10LE,
    .src = GST_VIDEO_FORMAT_A420_10LE
  },
};
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (alphacombine_debug);
#define GST_CAT_DEFAULT (alphacombine_debug)

struct _GstAlphaCombine
{
  GstElement parent;

  GstPad *sink_pad;
  GstPad *alpha_pad;
  GstPad *src_pad;

  /* protected by sink_pad stream lock */
  GstBuffer *last_alpha_buffer;
  GstFlowReturn last_flow_ret;

  GMutex buffer_lock;
  GCond buffer_cond;
  GstBuffer *alpha_buffer;
  /* Ref-counted flushing state */
  guint flushing;

  GstVideoInfo sink_vinfo;
  GstVideoInfo alpha_vinfo;
  GstVideoFormat src_format;

  guint sink_format_cookie;
  guint alpha_format_cookie;
};

#define gst_alpha_combine_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAlphaCombine, gst_alpha_combine,
    GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (alphacombine_debug, "alphacombine", 0,
        "Alpha Combiner"));

GST_ELEMENT_REGISTER_DEFINE (alpha_combine, "alphacombine",
    GST_RANK_NONE, GST_TYPE_ALPHA_COMBINE);

static GstStaticPadTemplate gst_alpha_combine_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (SUPPORTED_SINK_FORMATS))
    );

static GstStaticPadTemplate gst_alpha_combine_alpha_template =
GST_STATIC_PAD_TEMPLATE ("alpha",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (SUPPORTED_ALPHA_FORMATS))
    );

static GstStaticPadTemplate gst_alpha_combine_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (SUPPORTED_SRC_FORMATS))
    );

static void
gst_alpha_combine_unlock (GstAlphaCombine * self)
{
  g_mutex_lock (&self->buffer_lock);
  self->flushing++;
  g_cond_broadcast (&self->buffer_cond);
  g_mutex_unlock (&self->buffer_lock);
}

static void
gst_alpha_combine_unlock_stop (GstAlphaCombine * self)
{
  g_mutex_lock (&self->buffer_lock);
  g_assert (self->flushing);
  self->flushing--;

  /* Reset the format cookies to ensure they are equal */
  if (!self->flushing) {
    self->sink_format_cookie = 0;
    self->alpha_format_cookie = 0;
  }

  g_mutex_unlock (&self->buffer_lock);
}

static void
gst_alpha_combine_reset (GstAlphaCombine * self)
{
  gst_buffer_replace (&self->alpha_buffer, NULL);
  gst_buffer_replace (&self->last_alpha_buffer, NULL);
  self->last_flow_ret = GST_FLOW_OK;
}

/*
 * gst_alpha_combine_negotiate:
 * @self: #GstAlphaCombine pointer
 *
 * Verify that the stream and alpha stream format are compatible and fail
 * otherwise. There is no effort in helping upstream to dynamically negotiate
 * a valid combination to keep the complexity low, and because this would be a
 * very atypical usage.
 */
static gboolean
gst_alpha_combine_negotiate (GstAlphaCombine * self)
{
  gint i;
  GstVideoFormat src_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoFormat sink_format = GST_VIDEO_INFO_FORMAT (&self->sink_vinfo);
  GstVideoFormat alpha_format = GST_VIDEO_INFO_FORMAT (&self->alpha_vinfo);

  if (self->src_format != GST_VIDEO_FORMAT_UNKNOWN)
    return TRUE;

  for (i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (format_map[i].sink == sink_format
        && format_map[i].alpha == alpha_format) {
      src_format = format_map[i].src;
      break;
    }
  }

  if (src_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, ("Unsupported formats."),
        ("Cannot combined '%s' and '%s' into any supported transparent format",
            gst_video_format_to_string (sink_format),
            gst_video_format_to_string (alpha_format)));
    return FALSE;
  }

  if (GST_VIDEO_INFO_COLORIMETRY (&self->sink_vinfo).range !=
      GST_VIDEO_INFO_COLORIMETRY (&self->alpha_vinfo).range) {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, ("Color range mismatch"),
        ("We can only combine buffers if they have the same color range."));
    return FALSE;
  }

  self->src_format = src_format;
  return TRUE;
}

static GstFlowReturn
gst_alpha_combine_peek_alpha_buffer (GstAlphaCombine * self,
    GstBuffer ** alpha_buffer)
{
  g_mutex_lock (&self->buffer_lock);

  while (!self->alpha_buffer && !self->flushing)
    g_cond_wait (&self->buffer_cond, &self->buffer_lock);

  if (self->flushing) {
    g_mutex_unlock (&self->buffer_lock);
    return GST_FLOW_FLUSHING;
  }

  /* Now is a good time to validate the formats, as the alpha_vinfo won't be
   * updated until we signal this alpha_buffer_as being consumed */
  if (!gst_alpha_combine_negotiate (self)) {
    g_mutex_unlock (&self->buffer_lock);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  *alpha_buffer = gst_buffer_ref (self->alpha_buffer);
  g_mutex_unlock (&self->buffer_lock);

  if (GST_BUFFER_FLAG_IS_SET (*alpha_buffer, GST_BUFFER_FLAG_GAP)) {
    if (!self->last_alpha_buffer) {
      GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE,
          ("Cannot handle streams without an initial alpha buffer."), (NULL));
      gst_clear_buffer (alpha_buffer);
      return GST_FLOW_ERROR;
    }

    /* Re-use the last alpha buffer if one is gone missing */
    gst_buffer_replace (alpha_buffer, self->last_alpha_buffer);
  }

  return GST_FLOW_OK;
}

static void
gst_alpha_combine_pop_alpha_buffer (GstAlphaCombine * self,
    GstFlowReturn flow_ret)
{
  g_mutex_lock (&self->buffer_lock);
  self->last_flow_ret = flow_ret;
  gst_clear_buffer (&self->alpha_buffer);
  g_cond_broadcast (&self->buffer_cond);
  g_mutex_unlock (&self->buffer_lock);
}

static GstFlowReturn
gst_alpha_combine_push_alpha_buffer (GstAlphaCombine * self, GstBuffer * buffer)
{
  GstFlowReturn ret;

  g_mutex_lock (&self->buffer_lock);

  /* We wait for the alpha_buffer to be consumed and store the buffer for the
   * sink_chain to pick it up */
  while (self->alpha_buffer && !self->flushing)
    g_cond_wait (&self->buffer_cond, &self->buffer_lock);

  if (self->flushing) {
    gst_buffer_unref (buffer);
    g_mutex_unlock (&self->buffer_lock);
    return GST_FLOW_FLUSHING;
  }

  self->alpha_buffer = buffer;
  GST_DEBUG_OBJECT (self, "Stored pending alpha buffer %p", buffer);
  g_cond_signal (&self->buffer_cond);
  ret = self->last_flow_ret;
  g_mutex_unlock (&self->buffer_lock);

  return ret;
}

static GstFlowReturn
gst_alpha_combine_sink_chain (GstPad * pad, GstObject * object,
    GstBuffer * src_buffer)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);
  GstFlowReturn ret;
  GstVideoMeta *vmeta;
  GstBuffer *alpha_buffer;
  GstMemory *alpha_mem = NULL;
  gsize alpha_skip = 0;
  gint alpha_stride;
  GstBuffer *buffer;
  guint alpha_plane_idx;

  ret = gst_alpha_combine_peek_alpha_buffer (self, &alpha_buffer);
  if (ret != GST_FLOW_OK)
    return ret;

  GST_DEBUG_OBJECT (self, "Combining buffer %p with alpha buffer %p",
      src_buffer, alpha_buffer);

  vmeta = gst_buffer_get_video_meta (alpha_buffer);
  if (vmeta) {
    guint idx, length;
    if (gst_buffer_find_memory (alpha_buffer, vmeta->offset[GST_VIDEO_COMP_Y],
            1, &idx, &length, &alpha_skip)) {
      alpha_mem = gst_buffer_get_memory (alpha_buffer, idx);
    }

    alpha_stride = vmeta->stride[GST_VIDEO_COMP_Y];
  } else {
    alpha_mem = gst_buffer_get_memory (alpha_buffer, 0);
    alpha_stride = self->alpha_vinfo.stride[GST_VIDEO_COMP_Y];
  }

  if (!alpha_mem) {
    gst_buffer_unref (alpha_buffer);
    gst_buffer_unref (src_buffer);
    GST_ELEMENT_ERROR (self, STREAM, WRONG_TYPE,
        ("Invalid alpha video frame."), ("Could not find the plane"));
    return GST_FLOW_ERROR;
  }

  /* FIXME use some GstBuffer cache to reduce run-time allocation */
  buffer = gst_buffer_copy (src_buffer);
  vmeta = gst_buffer_get_video_meta (buffer);
  if (!vmeta)
    vmeta = gst_buffer_add_video_meta (buffer, 0,
        GST_VIDEO_INFO_FORMAT (&self->sink_vinfo),
        GST_VIDEO_INFO_WIDTH (&self->sink_vinfo),
        GST_VIDEO_INFO_HEIGHT (&self->sink_vinfo));

  alpha_skip += gst_buffer_get_size (buffer);
  gst_buffer_append_memory (buffer, alpha_mem);

  alpha_plane_idx = GST_VIDEO_INFO_N_PLANES (&self->sink_vinfo);
  vmeta->offset[alpha_plane_idx] = alpha_skip;
  vmeta->stride[alpha_plane_idx] = alpha_stride;

  vmeta->format = self->src_format;
  vmeta->n_planes = alpha_plane_idx + 1;

  /* Keep the origina GstBuffer alive to make this buffer pool friendly */
  gst_buffer_add_parent_buffer_meta (buffer, src_buffer);
  gst_buffer_add_parent_buffer_meta (buffer, alpha_buffer);

  gst_buffer_replace (&self->last_alpha_buffer, alpha_buffer);
  gst_buffer_unref (src_buffer);
  gst_buffer_unref (alpha_buffer);

  ret = gst_pad_push (self->src_pad, buffer);
  gst_alpha_combine_pop_alpha_buffer (self, ret);

  return ret;
}

static GstFlowReturn
gst_alpha_combine_alpha_chain (GstPad * pad, GstObject * object,
    GstBuffer * buffer)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);

  return gst_alpha_combine_push_alpha_buffer (self, buffer);
}

static gboolean
gst_alpha_combine_set_sink_format (GstAlphaCombine * self, GstCaps * caps)
{
  GstVideoFormat sink_format, src_format = GST_VIDEO_FORMAT_UNKNOWN;
  GstEvent *event;
  gint i;
  gboolean ret;

  if (!gst_video_info_from_caps (&self->sink_vinfo, caps)) {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, ("Invalid video format"), (NULL));
    return FALSE;
  }

  sink_format = GST_VIDEO_INFO_FORMAT (&self->sink_vinfo);

  /* The sink format determines the src format, though we cannot fully validate
   * the negotiation here, since we don't have the alpha format yet. */
  for (i = 0; i < G_N_ELEMENTS (format_map); i++) {
    if (format_map[i].sink == sink_format) {
      src_format = format_map[i].src;
      break;
    }
  }

  if (src_format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, ("Unsupported formats."),
        ("Sink format '%s' not supported.",
            gst_video_format_to_string (sink_format)));
    return FALSE;
  }

  caps = gst_caps_copy (caps);
  gst_caps_set_simple (caps, "format", G_TYPE_STRING,
      gst_video_format_to_string (src_format), NULL);
  event = gst_event_new_caps (caps);
  gst_caps_unref (caps);

  ret = gst_pad_push_event (self->src_pad, event);

  /* signal the sink format change */
  g_mutex_lock (&self->buffer_lock);
  self->sink_format_cookie++;
  g_cond_signal (&self->buffer_cond);
  g_mutex_unlock (&self->buffer_lock);

  return ret;
}

static gboolean
gst_alpha_combine_set_alpha_format (GstAlphaCombine * self, GstCaps * caps)
{
  /* We wait for the alpha_buffer to be consumed, so that we don't pick the
   * caps too soon */
  g_mutex_lock (&self->buffer_lock);

  /* We wait for the alpha_buffer to be consumed and store the buffer for the
   * sink_chain to pick it up */
  while (self->alpha_buffer && !self->flushing)
    g_cond_wait (&self->buffer_cond, &self->buffer_lock);

  if (self->flushing) {
    g_mutex_unlock (&self->buffer_lock);
    return GST_FLOW_FLUSHING;
  }

  if (!gst_video_info_from_caps (&self->alpha_vinfo, caps)) {
    g_mutex_unlock (&self->buffer_lock);
    GST_ELEMENT_ERROR (self, STREAM, FORMAT, ("Invalid video format"), (NULL));
    return FALSE;
  }

  self->alpha_format_cookie++;

  /* wait for the matching format change on the sink pad */
  while (self->alpha_format_cookie != self->sink_format_cookie &&
      !self->flushing)
    g_cond_wait (&self->buffer_cond, &self->buffer_lock);

  g_mutex_unlock (&self->buffer_lock);

  return TRUE;
}

static void
gst_alpha_combine_handle_gap (GstAlphaCombine * self)
{
  GstBuffer *gap_buffer = gst_buffer_new ();
  GST_BUFFER_FLAG_SET (gap_buffer, GST_BUFFER_FLAG_GAP);
  gst_alpha_combine_push_alpha_buffer (self, gap_buffer);
}

static gboolean
gst_alpha_combine_sink_event (GstPad * pad, GstObject * object,
    GstEvent * event)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);

  switch (event->type) {
    case GST_EVENT_FLUSH_START:
      gst_alpha_combine_unlock (self);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_alpha_combine_unlock_stop (self);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gboolean ret;

      gst_event_parse_caps (event, &caps);
      ret = gst_alpha_combine_set_sink_format (self, caps);
      gst_event_unref (event);

      return ret;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, object, event);
}

static gboolean
gst_alpha_combine_alpha_event (GstPad * pad, GstObject * object,
    GstEvent * event)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);

  switch (event->type) {
    case GST_EVENT_FLUSH_START:
      gst_alpha_combine_unlock (self);
      break;
    case GST_EVENT_FLUSH_STOP:
      gst_alpha_combine_unlock_stop (self);
      gst_alpha_combine_reset (self);
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      gst_event_parse_caps (event, &caps);
      gst_alpha_combine_set_alpha_format (self, caps);
      break;
    }
    case GST_EVENT_GAP:
    {
      gst_alpha_combine_handle_gap (self);
      break;
    }
    default:
      break;
  }

  /* Events are being duplicated, over both branches, so let's just drop this
   * secondary stream and use the one from the main stream. */
  gst_event_unref (event);
  return TRUE;
}

static gboolean
gst_alpha_combine_sink_query (GstPad * pad, GstObject * object,
    GstQuery * query)
{
  switch (query->type) {
    case GST_QUERY_ALLOCATION:
    {
      int i;

      if (!gst_pad_query_default (pad, object, query))
        return FALSE;

      /* Ensure NULL pool because it cannot be shared between the 2 decoders.
       * Ideally, we should cache the downstream query and use it for both
       * decoders, but it is hard to know when we should refresh it */
      for (i = 0; i < gst_query_get_n_allocation_pools (query); i++) {
        guint size = 0, min = 0, max = 0;
        gst_query_parse_nth_allocation_pool (query, i, NULL, &size, &min, &max);
        gst_query_set_nth_allocation_pool (query, i, NULL, size, min, max);
      }

      return TRUE;
      break;
    }
    default:
      break;
  }

  return gst_pad_query_default (pad, object, query);
}

static GstStateChangeReturn
gst_alpha_combine_change_state (GstElement * element, GstStateChange transition)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_alpha_combine_unlock_stop (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_alpha_combine_unlock (self);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_alpha_combine_reset (self);
      self->src_format = GST_VIDEO_FORMAT_UNKNOWN;
      gst_video_info_init (&self->sink_vinfo);
      gst_video_info_init (&self->alpha_vinfo);
      self->sink_format_cookie = 0;
      self->alpha_format_cookie = 0;
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_alpha_combine_dispose (GObject * object)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);

  g_clear_object (&self->sink_pad);
  g_clear_object (&self->alpha_pad);
  g_clear_object (&self->src_pad);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_alpha_combine_finalize (GObject * object)
{
  GstAlphaCombine *self = GST_ALPHA_COMBINE (object);

  g_mutex_clear (&self->buffer_lock);
  g_cond_clear (&self->buffer_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_alpha_combine_class_init (GstAlphaCombineClass * klass)
{
  GstElementClass *element_class = (GstElementClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  gst_element_class_set_static_metadata (element_class,
      "Alpha Combiner", "Codec/Demuxer",
      "Use luma from an opaque stream as alpha plane on another",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_alpha_combine_sink_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_alpha_combine_alpha_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_alpha_combine_src_template);

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_alpha_combine_change_state);

  object_class->dispose = GST_DEBUG_FUNCPTR (gst_alpha_combine_dispose);
  object_class->finalize = GST_DEBUG_FUNCPTR (gst_alpha_combine_finalize);
}

static void
gst_alpha_combine_init (GstAlphaCombine * self)
{
  gst_element_create_all_pads (GST_ELEMENT (self));
  self->sink_pad = gst_element_get_static_pad (GST_ELEMENT (self), "sink");
  self->alpha_pad = gst_element_get_static_pad (GST_ELEMENT (self), "alpha");
  self->src_pad = gst_element_get_static_pad (GST_ELEMENT (self), "src");
  self->flushing = 1;

  g_mutex_init (&self->buffer_lock);
  g_cond_init (&self->buffer_cond);

  GST_PAD_SET_PROXY_SCHEDULING (self->sink_pad);
  GST_PAD_SET_PROXY_SCHEDULING (self->src_pad);

  GST_PAD_SET_PROXY_ALLOCATION (self->sink_pad);
  GST_PAD_SET_PROXY_ALLOCATION (self->alpha_pad);

  gst_pad_set_chain_function (self->sink_pad, gst_alpha_combine_sink_chain);
  gst_pad_set_chain_function (self->alpha_pad, gst_alpha_combine_alpha_chain);

  gst_pad_set_event_function (self->sink_pad, gst_alpha_combine_sink_event);
  gst_pad_set_event_function (self->alpha_pad, gst_alpha_combine_alpha_event);

  gst_pad_set_query_function (self->sink_pad, gst_alpha_combine_sink_query);
  gst_pad_set_query_function (self->alpha_pad, gst_alpha_combine_sink_query);
}
