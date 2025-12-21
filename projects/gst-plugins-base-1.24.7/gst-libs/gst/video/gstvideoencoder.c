/* GStreamer
 * Copyright (C) 2008 David Schleef <ds@schleef.org>
 * Copyright (C) 2011 Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>.
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
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

/**
 * SECTION:gstvideoencoder
 * @title: GstVideoEncoder
 * @short_description: Base class for video encoders
 *
 * This base class is for video encoders turning raw video into
 * encoded video data.
 *
 * GstVideoEncoder and subclass should cooperate as follows.
 *
 * ## Configuration
 *
 *   * Initially, GstVideoEncoder calls @start when the encoder element
 *     is activated, which allows subclass to perform any global setup.
 *   * GstVideoEncoder calls @set_format to inform subclass of the format
 *     of input video data that it is about to receive.  Subclass should
 *     setup for encoding and configure base class as appropriate
 *     (e.g. latency). While unlikely, it might be called more than once,
 *     if changing input parameters require reconfiguration.  Baseclass
 *     will ensure that processing of current configuration is finished.
 *   * GstVideoEncoder calls @stop at end of all processing.
 *
 * ## Data processing
 *
 *     * Base class collects input data and metadata into a frame and hands
 *       this to subclass' @handle_frame.
 *
 *     * If codec processing results in encoded data, subclass should call
 *       @gst_video_encoder_finish_frame to have encoded data pushed
 *       downstream.
 *
 *     * If implemented, baseclass calls subclass @pre_push just prior to
 *       pushing to allow subclasses to modify some metadata on the buffer.
 *       If it returns GST_FLOW_OK, the buffer is pushed downstream.
 *
 *     * GstVideoEncoderClass will handle both srcpad and sinkpad events.
 *       Sink events will be passed to subclass if @event callback has been
 *       provided.
 *
 * ## Shutdown phase
 *
 *   * GstVideoEncoder class calls @stop to inform the subclass that data
 *     parsing will be stopped.
 *
 * Subclass is responsible for providing pad template caps for
 * source and sink pads. The pads need to be named "sink" and "src". It should
 * also be able to provide fixed src pad caps in @getcaps by the time it calls
 * @gst_video_encoder_finish_frame.
 *
 * Things that subclass need to take care of:
 *
 *   * Provide pad templates
 *   * Provide source pad caps before pushing the first buffer
 *   * Accept data in @handle_frame and provide encoded results to
 *      @gst_video_encoder_finish_frame.
 *
 *
 * The #GstVideoEncoder:qos property will enable the Quality-of-Service
 * features of the encoder which gather statistics about the real-time
 * performance of the downstream elements. If enabled, subclasses can
 * use gst_video_encoder_get_max_encode_time() to check if input frames
 * are already late and drop them right away to give a chance to the
 * pipeline to catch up.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* TODO
 *
 * * Calculate actual latency based on input/output timestamp/frame_number
 *   and if it exceeds the recorded one, save it and emit a GST_MESSAGE_LATENCY
 */

#include <gst/video/video.h>
#include "gstvideoencoder.h"
#include "gstvideoutils.h"
#include "gstvideoutilsprivate.h"

#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <string.h>

GST_DEBUG_CATEGORY (videoencoder_debug);
#define GST_CAT_DEFAULT videoencoder_debug

/* properties */

#define DEFAULT_QOS                 FALSE
#define DEFAULT_MIN_FORCE_KEY_UNIT_INTERVAL 0

enum
{
  PROP_0,
  PROP_QOS,
  PROP_MIN_FORCE_KEY_UNIT_INTERVAL,
  PROP_LAST
};

struct _GstVideoEncoderPrivate
{
  guint64 presentation_frame_number;
  int distance_from_sync;

  /* FIXME : (and introduce a context ?) */
  gboolean drained;

  gint64 min_latency;
  gint64 max_latency;

  /* Tracks whether the latency message was posted at least once */
  gboolean posted_latency_msg;

  /* FIXME 2.0: Use a GQueue or similar, see GstVideoCodecFrame::events */
  GList *current_frame_events;

  GList *headers;
  gboolean new_headers;         /* Whether new headers were just set */

  GQueue force_key_unit;        /* List of pending forced keyunits */
  GstClockTime min_force_key_unit_interval;
  GstClockTime last_force_key_unit_request;
  GstClockTime last_key_unit;

  guint32 system_frame_number;

  GQueue frames;                /* Protected with OBJECT_LOCK */
  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
  gboolean output_state_changed;

  gint64 bytes;
  gint64 time;

  GstAllocator *allocator;
  GstAllocationParams params;

  /* upstream stream tags (global tags are passed through as-is) */
  GstTagList *upstream_tags;

  /* subclass tags */
  GstTagList *tags;
  GstTagMergeMode tags_merge_mode;

  gboolean tags_changed;

  GstClockTime min_pts;
  /* adjustment needed on pts, dts, segment start and stop to accommodate
   * min_pts */
  GstClockTime time_adjustment;

  /* QoS properties */
  gint qos_enabled;             /* ATOMIC */
  gdouble proportion;           /* OBJECT_LOCK */
  GstClockTime earliest_time;   /* OBJECT_LOCK */
  GstClockTime qos_frame_duration;      /* OBJECT_LOCK */
  /* qos messages: frames dropped/processed */
  guint dropped;
  guint processed;
};

typedef struct _ForcedKeyUnitEvent ForcedKeyUnitEvent;
struct _ForcedKeyUnitEvent
{
  GstClockTime running_time;
  gboolean pending;             /* TRUE if this was requested already */
  gboolean all_headers;
  guint count;
  guint32 frame_id;
};

static void
forced_key_unit_event_free (ForcedKeyUnitEvent * evt)
{
  g_free (evt);
}

static ForcedKeyUnitEvent *
forced_key_unit_event_new (GstClockTime running_time, gboolean all_headers,
    guint count)
{
  ForcedKeyUnitEvent *evt = g_new0 (ForcedKeyUnitEvent, 1);

  evt->running_time = running_time;
  evt->all_headers = all_headers;
  evt->count = count;

  return evt;
}

static gint
forced_key_unit_event_compare (const ForcedKeyUnitEvent * a,
    const ForcedKeyUnitEvent * b, gpointer user_data)
{
  if (a->running_time == b->running_time) {
    /* Sort pending ones before non-pending ones */
    if (a->pending && !b->pending)
      return -1;
    if (!a->pending && b->pending)
      return 1;
    return 0;
  }

  if (a->running_time == GST_CLOCK_TIME_NONE)
    return -1;
  if (b->running_time == GST_CLOCK_TIME_NONE)
    return 1;
  if (a->running_time < b->running_time)
    return -1;
  return 1;
}

static GstElementClass *parent_class = NULL;
static gint private_offset = 0;

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

static void gst_video_encoder_class_init (GstVideoEncoderClass * klass);
static void gst_video_encoder_init (GstVideoEncoder * enc,
    GstVideoEncoderClass * klass);

static void gst_video_encoder_finalize (GObject * object);

static gboolean gst_video_encoder_setcaps (GstVideoEncoder * enc,
    GstCaps * caps);
static GstCaps *gst_video_encoder_sink_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);
static gboolean gst_video_encoder_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_video_encoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_video_encoder_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buf);
static GstStateChangeReturn gst_video_encoder_change_state (GstElement *
    element, GstStateChange transition);
static gboolean gst_video_encoder_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_video_encoder_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static GstVideoCodecFrame *gst_video_encoder_new_frame (GstVideoEncoder *
    encoder, GstBuffer * buf, GstClockTime pts, GstClockTime dts,
    GstClockTime duration);

static gboolean gst_video_encoder_sink_event_default (GstVideoEncoder * encoder,
    GstEvent * event);
static gboolean gst_video_encoder_src_event_default (GstVideoEncoder * encoder,
    GstEvent * event);
static gboolean gst_video_encoder_decide_allocation_default (GstVideoEncoder *
    encoder, GstQuery * query);
static gboolean gst_video_encoder_propose_allocation_default (GstVideoEncoder *
    encoder, GstQuery * query);
static gboolean gst_video_encoder_negotiate_default (GstVideoEncoder * encoder);
static gboolean gst_video_encoder_negotiate_unlocked (GstVideoEncoder *
    encoder);

static gboolean gst_video_encoder_sink_query_default (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_video_encoder_src_query_default (GstVideoEncoder * encoder,
    GstQuery * query);

static gboolean gst_video_encoder_transform_meta_default (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame, GstMeta * meta);

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_video_encoder_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstVideoEncoderClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_video_encoder_class_init,
      NULL,
      NULL,
      sizeof (GstVideoEncoder),
      0,
      (GInstanceInitFunc) gst_video_encoder_init,
    };
    const GInterfaceInfo preset_interface_info = {
      NULL,                     /* interface_init */
      NULL,                     /* interface_finalize */
      NULL                      /* interface_data */
    };

    _type = g_type_register_static (GST_TYPE_ELEMENT,
        "GstVideoEncoder", &info, G_TYPE_FLAG_ABSTRACT);
    private_offset =
        g_type_add_instance_private (_type, sizeof (GstVideoEncoderPrivate));
    g_type_add_interface_static (_type, GST_TYPE_PRESET,
        &preset_interface_info);
    g_once_init_leave (&type, _type);
  }
  return type;
}

static inline GstVideoEncoderPrivate *
gst_video_encoder_get_instance_private (GstVideoEncoder * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static void
gst_video_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVideoEncoder *sink = GST_VIDEO_ENCODER (object);

  switch (prop_id) {
    case PROP_QOS:
      gst_video_encoder_set_qos_enabled (sink, g_value_get_boolean (value));
      break;
    case PROP_MIN_FORCE_KEY_UNIT_INTERVAL:
      gst_video_encoder_set_min_force_key_unit_interval (sink,
          g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_encoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVideoEncoder *sink = GST_VIDEO_ENCODER (object);

  switch (prop_id) {
    case PROP_QOS:
      g_value_set_boolean (value, gst_video_encoder_is_qos_enabled (sink));
      break;
    case PROP_MIN_FORCE_KEY_UNIT_INTERVAL:
      g_value_set_uint64 (value,
          gst_video_encoder_get_min_force_key_unit_interval (sink));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_video_encoder_class_init (GstVideoEncoderClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (videoencoder_debug, "videoencoder", 0,
      "Base Video Encoder");

  parent_class = g_type_class_peek_parent (klass);

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  gobject_class->set_property = gst_video_encoder_set_property;
  gobject_class->get_property = gst_video_encoder_get_property;
  gobject_class->finalize = gst_video_encoder_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_video_encoder_change_state);

  klass->sink_event = gst_video_encoder_sink_event_default;
  klass->src_event = gst_video_encoder_src_event_default;
  klass->propose_allocation = gst_video_encoder_propose_allocation_default;
  klass->decide_allocation = gst_video_encoder_decide_allocation_default;
  klass->negotiate = gst_video_encoder_negotiate_default;
  klass->sink_query = gst_video_encoder_sink_query_default;
  klass->src_query = gst_video_encoder_src_query_default;
  klass->transform_meta = gst_video_encoder_transform_meta_default;

  g_object_class_install_property (gobject_class, PROP_QOS,
      g_param_spec_boolean ("qos", "Qos",
          "Handle Quality-of-Service events from downstream", DEFAULT_QOS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstVideoEncoder:min-force-key-unit-interval:
   *
   * Minimum interval between force-keyunit requests in nanoseconds. See
   * gst_video_encoder_set_min_force_key_unit_interval() for more details.
   *
   * Since: 1.18
   **/
  g_object_class_install_property (gobject_class,
      PROP_MIN_FORCE_KEY_UNIT_INTERVAL,
      g_param_spec_uint64 ("min-force-key-unit-interval",
          "Minimum Force Keyunit Interval",
          "Minimum interval between force-keyunit requests in nanoseconds", 0,
          G_MAXUINT64, DEFAULT_MIN_FORCE_KEY_UNIT_INTERVAL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static GList *
_flush_events (GstPad * pad, GList * events)
{
  GList *tmp;

  for (tmp = events; tmp; tmp = tmp->next) {
    if (GST_EVENT_TYPE (tmp->data) != GST_EVENT_EOS &&
        GST_EVENT_TYPE (tmp->data) != GST_EVENT_SEGMENT &&
        GST_EVENT_IS_STICKY (tmp->data)) {
      gst_pad_store_sticky_event (pad, GST_EVENT_CAST (tmp->data));
    }
    gst_event_unref (tmp->data);
  }
  g_list_free (events);

  return NULL;
}

static gboolean
gst_video_encoder_reset (GstVideoEncoder * encoder, gboolean hard)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  gboolean ret = TRUE;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

  priv->presentation_frame_number = 0;
  priv->distance_from_sync = 0;

  g_queue_clear_full (&priv->force_key_unit,
      (GDestroyNotify) forced_key_unit_event_free);
  priv->last_force_key_unit_request = GST_CLOCK_TIME_NONE;
  priv->last_key_unit = GST_CLOCK_TIME_NONE;

  priv->drained = TRUE;

  GST_OBJECT_LOCK (encoder);
  priv->bytes = 0;
  priv->time = 0;
  GST_OBJECT_UNLOCK (encoder);

  priv->time_adjustment = GST_CLOCK_TIME_NONE;

  if (hard) {
    gst_segment_init (&encoder->input_segment, GST_FORMAT_TIME);
    gst_segment_init (&encoder->output_segment, GST_FORMAT_TIME);

    if (priv->input_state)
      gst_video_codec_state_unref (priv->input_state);
    priv->input_state = NULL;
    if (priv->output_state)
      gst_video_codec_state_unref (priv->output_state);
    priv->output_state = NULL;

    if (priv->upstream_tags) {
      gst_tag_list_unref (priv->upstream_tags);
      priv->upstream_tags = NULL;
    }
    if (priv->tags)
      gst_tag_list_unref (priv->tags);
    priv->tags = NULL;
    priv->tags_merge_mode = GST_TAG_MERGE_APPEND;
    priv->tags_changed = FALSE;

    g_list_foreach (priv->headers, (GFunc) gst_event_unref, NULL);
    g_list_free (priv->headers);
    priv->headers = NULL;
    priv->new_headers = FALSE;

    if (priv->allocator) {
      gst_object_unref (priv->allocator);
      priv->allocator = NULL;
    }

    g_list_foreach (priv->current_frame_events, (GFunc) gst_event_unref, NULL);
    g_list_free (priv->current_frame_events);
    priv->current_frame_events = NULL;

    GST_OBJECT_LOCK (encoder);
    priv->proportion = 0.5;
    priv->earliest_time = GST_CLOCK_TIME_NONE;
    priv->qos_frame_duration = 0;
    GST_OBJECT_UNLOCK (encoder);

    priv->dropped = 0;
    priv->processed = 0;

    priv->posted_latency_msg = FALSE;
  } else {
    GList *l;

    for (l = priv->frames.head; l; l = l->next) {
      GstVideoCodecFrame *frame = l->data;

      frame->events = _flush_events (encoder->srcpad, frame->events);
    }
    priv->current_frame_events = _flush_events (encoder->srcpad,
        encoder->priv->current_frame_events);
  }

  g_queue_clear_full (&priv->frames,
      (GDestroyNotify) gst_video_codec_frame_unref);

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return ret;
}

/* Always call reset() in one way or another after this */
static gboolean
gst_video_encoder_flush (GstVideoEncoder * encoder)
{
  GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);
  gboolean ret = TRUE;

  if (klass->flush)
    ret = klass->flush (encoder);

  return ret;
}

static void
gst_video_encoder_init (GstVideoEncoder * encoder, GstVideoEncoderClass * klass)
{
  GstVideoEncoderPrivate *priv;
  GstPadTemplate *pad_template;
  GstPad *pad;

  GST_DEBUG_OBJECT (encoder, "gst_video_encoder_init");

  priv = encoder->priv = gst_video_encoder_get_instance_private (encoder);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_return_if_fail (pad_template != NULL);

  encoder->sinkpad = pad = gst_pad_new_from_template (pad_template, "sink");

  gst_pad_set_chain_function (pad, GST_DEBUG_FUNCPTR (gst_video_encoder_chain));
  gst_pad_set_event_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_encoder_sink_event));
  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_encoder_sink_query));
  gst_element_add_pad (GST_ELEMENT (encoder), encoder->sinkpad);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "src");
  g_return_if_fail (pad_template != NULL);

  encoder->srcpad = pad = gst_pad_new_from_template (pad_template, "src");

  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_encoder_src_query));
  gst_pad_set_event_function (pad,
      GST_DEBUG_FUNCPTR (gst_video_encoder_src_event));
  gst_element_add_pad (GST_ELEMENT (encoder), encoder->srcpad);

  gst_segment_init (&encoder->input_segment, GST_FORMAT_TIME);
  gst_segment_init (&encoder->output_segment, GST_FORMAT_TIME);

  g_rec_mutex_init (&encoder->stream_lock);

  priv->headers = NULL;
  priv->new_headers = FALSE;

  g_queue_init (&priv->frames);
  g_queue_init (&priv->force_key_unit);

  priv->min_latency = 0;
  priv->max_latency = 0;
  priv->min_pts = GST_CLOCK_TIME_NONE;
  priv->time_adjustment = GST_CLOCK_TIME_NONE;

  gst_video_encoder_reset (encoder, TRUE);
}

/**
 * gst_video_encoder_set_headers:
 * @encoder: a #GstVideoEncoder
 * @headers: (transfer full) (element-type GstBuffer): a list of #GstBuffer containing the codec header
 *
 * Set the codec headers to be sent downstream whenever requested.
 */
void
gst_video_encoder_set_headers (GstVideoEncoder * video_encoder, GList * headers)
{
  GST_VIDEO_ENCODER_STREAM_LOCK (video_encoder);

  GST_DEBUG_OBJECT (video_encoder, "new headers %p", headers);
  if (video_encoder->priv->headers) {
    g_list_foreach (video_encoder->priv->headers, (GFunc) gst_buffer_unref,
        NULL);
    g_list_free (video_encoder->priv->headers);
  }
  video_encoder->priv->headers = headers;
  video_encoder->priv->new_headers = TRUE;

  GST_VIDEO_ENCODER_STREAM_UNLOCK (video_encoder);
}

static GstVideoCodecState *
_new_output_state (GstCaps * caps, GstVideoCodecState * reference)
{
  GstVideoCodecState *state;

  state = g_new0 (GstVideoCodecState, 1);
  state->ref_count = 1;
  gst_video_info_init (&state->info);

  if (!gst_video_info_set_format (&state->info, GST_VIDEO_FORMAT_ENCODED, 0, 0)) {
    g_free (state);
    return NULL;
  }

  state->caps = caps;

  if (reference) {
    GstVideoInfo *tgt, *ref;

    tgt = &state->info;
    ref = &reference->info;

    /* Copy over extra fields from reference state */
    tgt->interlace_mode = ref->interlace_mode;
    tgt->flags = ref->flags;
    tgt->width = ref->width;
    tgt->height = ref->height;
    tgt->chroma_site = ref->chroma_site;
    tgt->colorimetry = ref->colorimetry;
    tgt->par_n = ref->par_n;
    tgt->par_d = ref->par_d;
    tgt->fps_n = ref->fps_n;
    tgt->fps_d = ref->fps_d;

    GST_VIDEO_INFO_FIELD_ORDER (tgt) = GST_VIDEO_INFO_FIELD_ORDER (ref);

    GST_VIDEO_INFO_MULTIVIEW_MODE (tgt) = GST_VIDEO_INFO_MULTIVIEW_MODE (ref);
    GST_VIDEO_INFO_MULTIVIEW_FLAGS (tgt) = GST_VIDEO_INFO_MULTIVIEW_FLAGS (ref);

    if (reference->mastering_display_info) {
      state->mastering_display_info =
          g_memdup2 (reference->mastering_display_info,
          sizeof (GstVideoMasteringDisplayInfo));
    }
    if (reference->content_light_level) {
      state->content_light_level =
          g_memdup2 (reference->content_light_level,
          sizeof (GstVideoContentLightLevel));
    }
  }

  return state;
}

static GstVideoCodecState *
_new_input_state (GstCaps * caps)
{
  GstVideoCodecState *state;
  GstStructure *c_struct;
  const gchar *s;

  state = g_new0 (GstVideoCodecState, 1);
  state->ref_count = 1;
  gst_video_info_init (&state->info);
  if (G_UNLIKELY (!gst_video_info_from_caps (&state->info, caps)))
    goto parse_fail;
  state->caps = gst_caps_ref (caps);

  c_struct = gst_caps_get_structure (caps, 0);

  if ((s = gst_structure_get_string (c_struct, "mastering-display-info"))) {
    state->mastering_display_info = g_new (GstVideoMasteringDisplayInfo, 1);
    gst_video_mastering_display_info_from_string (state->mastering_display_info,
        s);
  }
  if ((s = gst_structure_get_string (c_struct, "content-light-level"))) {
    state->content_light_level = g_new (GstVideoContentLightLevel, 1);
    gst_video_content_light_level_from_string (state->content_light_level, s);
  }

  return state;

parse_fail:
  {
    g_free (state);
    return NULL;
  }
}

static gboolean
gst_video_encoder_setcaps (GstVideoEncoder * encoder, GstCaps * caps)
{
  GstVideoEncoderClass *encoder_class;
  GstVideoCodecState *state;
  gboolean ret = TRUE;

  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_DEBUG_OBJECT (encoder, "setcaps %" GST_PTR_FORMAT, caps);

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

  if (encoder->priv->input_state) {
    GST_DEBUG_OBJECT (encoder,
        "Checking if caps changed old %" GST_PTR_FORMAT " new %" GST_PTR_FORMAT,
        encoder->priv->input_state->caps, caps);
    if (gst_caps_is_equal (encoder->priv->input_state->caps, caps))
      goto caps_not_changed;
  }

  state = _new_input_state (caps);
  if (G_UNLIKELY (!state))
    goto parse_fail;

  if (encoder->priv->input_state
      && gst_video_info_is_equal (&state->info,
          &encoder->priv->input_state->info)) {
    gst_video_codec_state_unref (state);
    goto caps_not_changed;
  }

  if (encoder_class->reset) {
    GST_FIXME_OBJECT (encoder, "GstVideoEncoder::reset() is deprecated");
    encoder_class->reset (encoder, TRUE);
  }

  /* and subclass should be ready to configure format at any time around */
  if (encoder_class->set_format != NULL)
    ret = encoder_class->set_format (encoder, state);

  if (ret) {
    if (encoder->priv->input_state)
      gst_video_codec_state_unref (encoder->priv->input_state);
    encoder->priv->input_state = state;
  } else {
    gst_video_codec_state_unref (state);
  }

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  if (!ret)
    GST_WARNING_OBJECT (encoder, "rejected caps %" GST_PTR_FORMAT, caps);

  return ret;

caps_not_changed:
  {
    GST_DEBUG_OBJECT (encoder, "Caps did not change - ignore");
    GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
    return TRUE;
  }

  /* ERRORS */
parse_fail:
  {
    GST_WARNING_OBJECT (encoder, "Failed to parse caps");
    GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
    return FALSE;
  }
}

/**
 * gst_video_encoder_proxy_getcaps:
 * @enc: a #GstVideoEncoder
 * @caps: (nullable): initial caps
 * @filter: (nullable): filter caps
 *
 * Returns caps that express @caps (or sink template caps if @caps == NULL)
 * restricted to resolution/format/... combinations supported by downstream
 * elements (e.g. muxers).
 *
 * Returns: (transfer full): a #GstCaps owned by caller
 */
GstCaps *
gst_video_encoder_proxy_getcaps (GstVideoEncoder * encoder, GstCaps * caps,
    GstCaps * filter)
{
  return __gst_video_element_proxy_getcaps (GST_ELEMENT_CAST (encoder),
      GST_VIDEO_ENCODER_SINK_PAD (encoder),
      GST_VIDEO_ENCODER_SRC_PAD (encoder), caps, filter);
}

static GstCaps *
gst_video_encoder_sink_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstVideoEncoderClass *klass;
  GstCaps *caps;

  klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  if (klass->getcaps)
    caps = klass->getcaps (encoder, filter);
  else
    caps = gst_video_encoder_proxy_getcaps (encoder, NULL, filter);

  GST_LOG_OBJECT (encoder, "Returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_video_encoder_decide_allocation_default (GstVideoEncoder * encoder,
    GstQuery * query)
{
  GstAllocator *allocator = NULL;
  GstAllocationParams params;
  gboolean update_allocator;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    /* try the allocator */
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    update_allocator = TRUE;
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
    update_allocator = FALSE;
  }

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);
  if (allocator)
    gst_object_unref (allocator);

  return TRUE;
}

static gboolean
gst_video_encoder_propose_allocation_default (GstVideoEncoder * encoder,
    GstQuery * query)
{
  GstCaps *caps;
  GstVideoInfo info;
  GstBufferPool *pool;
  guint size;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstStructure *structure;
    GstAllocator *allocator = NULL;
    GstAllocationParams params = { 0, 15, 0, 0 };

    if (gst_query_get_n_allocation_params (query) > 0)
      gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    else
      gst_query_add_allocation_param (query, allocator, &params);

    pool = gst_video_buffer_pool_new ();

    structure = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (structure, caps, size, 0, 0);
    gst_buffer_pool_config_set_allocator (structure, allocator, &params);

    if (allocator)
      gst_object_unref (allocator);

    if (!gst_buffer_pool_set_config (pool, structure))
      goto config_failed;

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
    gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  }

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (encoder, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static gboolean
gst_video_encoder_sink_query_default (GstVideoEncoder * encoder,
    GstQuery * query)
{
  GstPad *pad = GST_VIDEO_ENCODER_SINK_PAD (encoder);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_video_encoder_sink_getcaps (encoder, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      GST_DEBUG_OBJECT (encoder, "convert query");

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      GST_OBJECT_LOCK (encoder);
      if (encoder->priv->input_state != NULL)
        res = __gst_video_rawvideo_convert (encoder->priv->input_state,
            src_fmt, src_val, &dest_fmt, &dest_val);
      else
        res = FALSE;
      GST_OBJECT_UNLOCK (encoder);
      if (!res)
        goto error;
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    case GST_QUERY_ALLOCATION:
    {
      GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);

      if (klass->propose_allocation)
        res = klass->propose_allocation (encoder, query);
      break;
    }
    default:
      res = gst_pad_query_default (pad, GST_OBJECT (encoder), query);
      break;
  }
  return res;

error:
  GST_DEBUG_OBJECT (encoder, "query failed");
  return res;
}

static gboolean
gst_video_encoder_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstVideoEncoder *encoder;
  GstVideoEncoderClass *encoder_class;
  gboolean ret = FALSE;

  encoder = GST_VIDEO_ENCODER (parent);
  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_DEBUG_OBJECT (encoder, "received query %d, %s", GST_QUERY_TYPE (query),
      GST_QUERY_TYPE_NAME (query));

  if (encoder_class->sink_query)
    ret = encoder_class->sink_query (encoder, query);

  return ret;
}

static void
gst_video_encoder_finalize (GObject * object)
{
  GstVideoEncoder *encoder;

  GST_DEBUG_OBJECT (object, "finalize");

  encoder = GST_VIDEO_ENCODER (object);
  g_rec_mutex_clear (&encoder->stream_lock);

  if (encoder->priv->allocator) {
    gst_object_unref (encoder->priv->allocator);
    encoder->priv->allocator = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_video_encoder_push_event (GstVideoEncoder * encoder, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

      gst_event_copy_segment (event, &segment);

      GST_DEBUG_OBJECT (encoder, "segment %" GST_SEGMENT_FORMAT, &segment);

      if (segment.format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (encoder, "received non TIME segment");
        GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
        break;
      }

      if (encoder->priv->time_adjustment != GST_CLOCK_TIME_NONE) {
        segment.start += encoder->priv->time_adjustment;
        if (GST_CLOCK_TIME_IS_VALID (segment.position)) {
          segment.position += encoder->priv->time_adjustment;
        }
        if (GST_CLOCK_TIME_IS_VALID (segment.stop)) {
          segment.stop += encoder->priv->time_adjustment;
        }
      }

      encoder->output_segment = segment;
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

      gst_event_unref (event);
      event = gst_event_new_segment (&encoder->output_segment);

      break;
    }
    default:
      break;
  }

  return gst_pad_push_event (encoder->srcpad, event);
}

static GstEvent *
gst_video_encoder_create_merged_tags_event (GstVideoEncoder * enc)
{
  GstTagList *merged_tags;

  GST_LOG_OBJECT (enc, "upstream : %" GST_PTR_FORMAT, enc->priv->upstream_tags);
  GST_LOG_OBJECT (enc, "encoder  : %" GST_PTR_FORMAT, enc->priv->tags);
  GST_LOG_OBJECT (enc, "mode     : %d", enc->priv->tags_merge_mode);

  merged_tags =
      gst_tag_list_merge (enc->priv->upstream_tags, enc->priv->tags,
      enc->priv->tags_merge_mode);

  GST_DEBUG_OBJECT (enc, "merged   : %" GST_PTR_FORMAT, merged_tags);

  if (merged_tags == NULL)
    return NULL;

  if (gst_tag_list_is_empty (merged_tags)) {
    gst_tag_list_unref (merged_tags);
    return NULL;
  }

  return gst_event_new_tag (merged_tags);
}

static inline void
gst_video_encoder_check_and_push_tags (GstVideoEncoder * encoder)
{
  if (encoder->priv->tags_changed) {
    GstEvent *tags_event;

    tags_event = gst_video_encoder_create_merged_tags_event (encoder);

    if (tags_event != NULL)
      gst_video_encoder_push_event (encoder, tags_event);

    encoder->priv->tags_changed = FALSE;
  }
}

static gboolean
gst_video_encoder_sink_event_default (GstVideoEncoder * encoder,
    GstEvent * event)
{
  GstVideoEncoderClass *encoder_class;
  gboolean ret = FALSE;

  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      ret = gst_video_encoder_setcaps (encoder, caps);

      gst_event_unref (event);
      event = NULL;
      break;
    }
    case GST_EVENT_EOS:
    {
      GstFlowReturn flow_ret;

      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

      if (encoder_class->finish) {
        flow_ret = encoder_class->finish (encoder);
      } else {
        flow_ret = GST_FLOW_OK;
      }

      if (encoder->priv->current_frame_events) {
        GList *l;

        for (l = g_list_last (encoder->priv->current_frame_events); l;
            l = g_list_previous (l)) {
          GstEvent *event = GST_EVENT (l->data);

          gst_video_encoder_push_event (encoder, event);
        }
      }
      g_list_free (encoder->priv->current_frame_events);
      encoder->priv->current_frame_events = NULL;

      gst_video_encoder_check_and_push_tags (encoder);

      ret = (flow_ret == GST_FLOW_OK);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
      break;
    }
    case GST_EVENT_SEGMENT:
    {
      GstSegment segment;

      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

      gst_event_copy_segment (event, &segment);

      GST_DEBUG_OBJECT (encoder, "segment %" GST_SEGMENT_FORMAT, &segment);

      if (segment.format != GST_FORMAT_TIME) {
        GST_DEBUG_OBJECT (encoder, "received non TIME newsegment");
        GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
        break;
      }

      encoder->input_segment = segment;
      ret = TRUE;
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
      break;
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      if (gst_video_event_is_force_key_unit (event)) {
        GstClockTime running_time;
        gboolean all_headers;
        guint count;

        if (gst_video_event_parse_downstream_force_key_unit (event,
                NULL, NULL, &running_time, &all_headers, &count)) {
          ForcedKeyUnitEvent *fevt;

          GST_OBJECT_LOCK (encoder);
          fevt = forced_key_unit_event_new (running_time, all_headers, count);
          g_queue_insert_sorted (&encoder->priv->force_key_unit, fevt,
              (GCompareDataFunc) forced_key_unit_event_compare, NULL);
          GST_OBJECT_UNLOCK (encoder);

          GST_DEBUG_OBJECT (encoder,
              "force-key-unit event: running-time %" GST_TIME_FORMAT
              ", all_headers %d, count %u",
              GST_TIME_ARGS (running_time), all_headers, count);
        }
        gst_event_unref (event);
        event = NULL;
        ret = TRUE;
      }
      break;
    }
    case GST_EVENT_STREAM_START:
    {
      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
      /* Flush upstream tags after a STREAM_START */
      GST_DEBUG_OBJECT (encoder, "STREAM_START, clearing upstream tags");
      if (encoder->priv->upstream_tags) {
        gst_tag_list_unref (encoder->priv->upstream_tags);
        encoder->priv->upstream_tags = NULL;
        encoder->priv->tags_changed = TRUE;
      }
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
      break;
    }
    case GST_EVENT_TAG:
    {
      GstTagList *tags;

      gst_event_parse_tag (event, &tags);

      if (gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_STREAM) {
        GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
        if (encoder->priv->upstream_tags != tags) {
          tags = gst_tag_list_copy (tags);

          /* FIXME: make generic based on GST_TAG_FLAG_ENCODED */
          gst_tag_list_remove_tag (tags, GST_TAG_CODEC);
          gst_tag_list_remove_tag (tags, GST_TAG_AUDIO_CODEC);
          gst_tag_list_remove_tag (tags, GST_TAG_VIDEO_CODEC);
          gst_tag_list_remove_tag (tags, GST_TAG_SUBTITLE_CODEC);
          gst_tag_list_remove_tag (tags, GST_TAG_CONTAINER_FORMAT);
          gst_tag_list_remove_tag (tags, GST_TAG_BITRATE);
          gst_tag_list_remove_tag (tags, GST_TAG_NOMINAL_BITRATE);
          gst_tag_list_remove_tag (tags, GST_TAG_MAXIMUM_BITRATE);
          gst_tag_list_remove_tag (tags, GST_TAG_MINIMUM_BITRATE);
          gst_tag_list_remove_tag (tags, GST_TAG_ENCODER);
          gst_tag_list_remove_tag (tags, GST_TAG_ENCODER_VERSION);

          if (encoder->priv->upstream_tags)
            gst_tag_list_unref (encoder->priv->upstream_tags);
          encoder->priv->upstream_tags = tags;
          GST_INFO_OBJECT (encoder, "upstream tags: %" GST_PTR_FORMAT, tags);
        }
        gst_event_unref (event);
        event = gst_video_encoder_create_merged_tags_event (encoder);
        GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
        if (!event)
          ret = TRUE;
      }
      break;
    }
    case GST_EVENT_FLUSH_STOP:{
      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
      gst_video_encoder_flush (encoder);
      gst_segment_init (&encoder->input_segment, GST_FORMAT_TIME);
      gst_segment_init (&encoder->output_segment, GST_FORMAT_TIME);
      gst_video_encoder_reset (encoder, FALSE);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
      break;
    }
    default:
      break;
  }

  /* Forward non-serialized events and EOS/FLUSH_STOP immediately.
   * For EOS this is required because no buffer or serialized event
   * will come after EOS and nothing could trigger another
   * _finish_frame() call.   *
   * If the subclass handles sending of EOS manually it can simply
   * not chain up to the parent class' event handler
   *
   * For FLUSH_STOP this is required because it is expected
   * to be forwarded immediately and no buffers are queued anyway.
   */
  if (event) {
    if (!GST_EVENT_IS_SERIALIZED (event)
        || GST_EVENT_TYPE (event) == GST_EVENT_EOS
        || GST_EVENT_TYPE (event) == GST_EVENT_FLUSH_STOP) {
      ret = gst_video_encoder_push_event (encoder, event);
    } else {
      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
      encoder->priv->current_frame_events =
          g_list_prepend (encoder->priv->current_frame_events, event);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
      ret = TRUE;
    }
  }

  return ret;
}

static gboolean
gst_video_encoder_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstVideoEncoder *enc;
  GstVideoEncoderClass *klass;
  gboolean ret = TRUE;

  enc = GST_VIDEO_ENCODER (parent);
  klass = GST_VIDEO_ENCODER_GET_CLASS (enc);

  GST_DEBUG_OBJECT (enc, "received event %d, %s", GST_EVENT_TYPE (event),
      GST_EVENT_TYPE_NAME (event));

  if (klass->sink_event)
    ret = klass->sink_event (enc, event);

  return ret;
}

static gboolean
gst_video_encoder_src_event_default (GstVideoEncoder * encoder,
    GstEvent * event)
{
  gboolean ret = FALSE;
  GstVideoEncoderPrivate *priv = encoder->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      if (gst_video_event_is_force_key_unit (event)) {
        GstClockTime running_time;
        gboolean all_headers;
        guint count;

        if (gst_video_event_parse_upstream_force_key_unit (event,
                &running_time, &all_headers, &count)) {
          ForcedKeyUnitEvent *fevt;

          GST_OBJECT_LOCK (encoder);
          fevt = forced_key_unit_event_new (running_time, all_headers, count);
          g_queue_insert_sorted (&encoder->priv->force_key_unit, fevt,
              (GCompareDataFunc) forced_key_unit_event_compare, NULL);
          GST_OBJECT_UNLOCK (encoder);

          GST_DEBUG_OBJECT (encoder,
              "force-key-unit event: running-time %" GST_TIME_FORMAT
              ", all_headers %d, count %u",
              GST_TIME_ARGS (running_time), all_headers, count);
        }
        gst_event_unref (event);
        event = NULL;
        ret = TRUE;
      }
      break;
    }
    case GST_EVENT_QOS:
    {
      GstQOSType type;
      gdouble proportion;
      GstClockTimeDiff diff;
      GstClockTime timestamp;

      if (!g_atomic_int_get (&priv->qos_enabled))
        break;

      gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp);

      GST_OBJECT_LOCK (encoder);
      priv->proportion = proportion;
      if (G_LIKELY (GST_CLOCK_TIME_IS_VALID (timestamp))) {
        if (G_UNLIKELY (diff > 0)) {
          priv->earliest_time = timestamp + 2 * diff + priv->qos_frame_duration;
        } else {
          priv->earliest_time = timestamp + diff;
        }
      } else {
        priv->earliest_time = GST_CLOCK_TIME_NONE;
      }
      GST_OBJECT_UNLOCK (encoder);

      GST_DEBUG_OBJECT (encoder,
          "got QoS %" GST_TIME_FORMAT ", %" GST_STIME_FORMAT ", %g",
          GST_TIME_ARGS (timestamp), GST_STIME_ARGS (diff), proportion);

      ret = gst_pad_push_event (encoder->sinkpad, event);
      event = NULL;
      break;
    }
    default:
      break;
  }

  if (event)
    ret =
        gst_pad_event_default (encoder->srcpad, GST_OBJECT_CAST (encoder),
        event);

  return ret;
}

static gboolean
gst_video_encoder_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstVideoEncoder *encoder;
  GstVideoEncoderClass *klass;
  gboolean ret = FALSE;

  encoder = GST_VIDEO_ENCODER (parent);
  klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_LOG_OBJECT (encoder, "handling event: %" GST_PTR_FORMAT, event);

  if (klass->src_event)
    ret = klass->src_event (encoder, event);

  return ret;
}

static gboolean
gst_video_encoder_src_query_default (GstVideoEncoder * enc, GstQuery * query)
{
  GstPad *pad = GST_VIDEO_ENCODER_SRC_PAD (enc);
  GstVideoEncoderPrivate *priv;
  gboolean res;

  priv = enc->priv;

  GST_LOG_OBJECT (enc, "handling query: %" GST_PTR_FORMAT, query);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      GST_OBJECT_LOCK (enc);
      res =
          __gst_video_encoded_video_convert (priv->bytes, priv->time, src_fmt,
          src_val, &dest_fmt, &dest_val);
      GST_OBJECT_UNLOCK (enc);
      if (!res)
        goto error;
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      break;
    }
    case GST_QUERY_LATENCY:
    {
      gboolean live;
      GstClockTime min_latency, max_latency;

      res = gst_pad_peer_query (enc->sinkpad, query);
      if (res) {
        gst_query_parse_latency (query, &live, &min_latency, &max_latency);
        GST_DEBUG_OBJECT (enc, "Peer latency: live %d, min %"
            GST_TIME_FORMAT " max %" GST_TIME_FORMAT, live,
            GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

        GST_OBJECT_LOCK (enc);
        min_latency += priv->min_latency;
        if (max_latency == GST_CLOCK_TIME_NONE
            || enc->priv->max_latency == GST_CLOCK_TIME_NONE)
          max_latency = GST_CLOCK_TIME_NONE;
        else
          max_latency += enc->priv->max_latency;
        GST_OBJECT_UNLOCK (enc);

        gst_query_set_latency (query, live, min_latency, max_latency);
      }
    }
      break;
    default:
      res = gst_pad_query_default (pad, GST_OBJECT (enc), query);
  }
  return res;

error:
  GST_DEBUG_OBJECT (enc, "query failed");
  return res;
}

static gboolean
gst_video_encoder_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstVideoEncoder *encoder;
  GstVideoEncoderClass *encoder_class;
  gboolean ret = FALSE;

  encoder = GST_VIDEO_ENCODER (parent);
  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_DEBUG_OBJECT (encoder, "received query %d, %s", GST_QUERY_TYPE (query),
      GST_QUERY_TYPE_NAME (query));

  if (encoder_class->src_query)
    ret = encoder_class->src_query (encoder, query);

  return ret;
}

static GstVideoCodecFrame *
gst_video_encoder_new_frame (GstVideoEncoder * encoder, GstBuffer * buf,
    GstClockTime pts, GstClockTime dts, GstClockTime duration)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GstVideoCodecFrame *frame;

  frame = g_new0 (GstVideoCodecFrame, 1);

  frame->ref_count = 1;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  frame->system_frame_number = priv->system_frame_number;
  priv->system_frame_number++;

  frame->presentation_frame_number = priv->presentation_frame_number;
  priv->presentation_frame_number++;
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  frame->events = priv->current_frame_events;
  priv->current_frame_events = NULL;
  frame->input_buffer = buf;
  frame->pts = pts;
  frame->dts = dts;
  frame->duration = duration;
  frame->abidata.ABI.ts = pts;

  return frame;
}


static GstFlowReturn
gst_video_encoder_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstVideoEncoder *encoder;
  GstVideoEncoderPrivate *priv;
  GstVideoEncoderClass *klass;
  GstVideoCodecFrame *frame;
  GstClockTime pts, duration;
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 start, stop, cstart, cstop;

  encoder = GST_VIDEO_ENCODER (parent);
  priv = encoder->priv;
  klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  g_return_val_if_fail (klass->handle_frame != NULL, GST_FLOW_ERROR);

  if (!encoder->priv->input_state)
    goto not_negotiated;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

  pts = GST_BUFFER_PTS (buf);
  duration = GST_BUFFER_DURATION (buf);

  GST_LOG_OBJECT (encoder,
      "received buffer of size %" G_GSIZE_FORMAT " with PTS %" GST_TIME_FORMAT
      ", DTS %" GST_TIME_FORMAT ", duration %" GST_TIME_FORMAT,
      gst_buffer_get_size (buf), GST_TIME_ARGS (pts),
      GST_TIME_ARGS (GST_BUFFER_DTS (buf)), GST_TIME_ARGS (duration));

  start = pts;
  if (GST_CLOCK_TIME_IS_VALID (duration))
    stop = start + duration;
  else
    stop = GST_CLOCK_TIME_NONE;

  /* Drop buffers outside of segment */
  if (!gst_segment_clip (&encoder->input_segment,
          GST_FORMAT_TIME, start, stop, &cstart, &cstop)) {
    GST_DEBUG_OBJECT (encoder, "clipping to segment dropped frame");
    gst_buffer_unref (buf);
    goto done;
  }

  if (GST_CLOCK_TIME_IS_VALID (cstop))
    duration = cstop - cstart;
  else
    duration = GST_CLOCK_TIME_NONE;

  if (priv->min_pts != GST_CLOCK_TIME_NONE
      && priv->time_adjustment == GST_CLOCK_TIME_NONE) {
    if (cstart < priv->min_pts) {
      priv->time_adjustment = priv->min_pts - cstart;
    }
  }

  if (priv->time_adjustment != GST_CLOCK_TIME_NONE) {
    cstart += priv->time_adjustment;
  }

  /* incoming DTS is not really relevant and does not make sense anyway,
   * so pass along _NONE and maybe come up with something better later on */
  frame = gst_video_encoder_new_frame (encoder, buf, cstart,
      GST_CLOCK_TIME_NONE, duration);

  GST_OBJECT_LOCK (encoder);
  if (priv->force_key_unit.head) {
    GList *l;
    GstClockTime running_time;
    gboolean throttled, have_fevt = FALSE, have_pending_none_fevt = FALSE;
    GQueue matching_fevt = G_QUEUE_INIT;

    running_time =
        gst_segment_to_running_time (&encoder->output_segment, GST_FORMAT_TIME,
        cstart);

    throttled = (priv->min_force_key_unit_interval != 0 &&
        priv->min_force_key_unit_interval != GST_CLOCK_TIME_NONE &&
        ((priv->last_force_key_unit_request != GST_CLOCK_TIME_NONE &&
                priv->last_force_key_unit_request +
                priv->min_force_key_unit_interval > running_time)
            || (priv->last_key_unit != GST_CLOCK_TIME_NONE
                && priv->last_key_unit + priv->min_force_key_unit_interval >
                running_time)));

    for (l = priv->force_key_unit.head; l && (!throttled || !have_fevt);
        l = l->next) {
      ForcedKeyUnitEvent *fevt = l->data;

      /* Skip pending keyunits */
      if (fevt->pending) {
        if (fevt->running_time == GST_CLOCK_TIME_NONE)
          have_pending_none_fevt = TRUE;
        continue;
      }

      /* Simple case, keyunit ASAP */
      if (fevt->running_time == GST_CLOCK_TIME_NONE) {
        have_fevt = TRUE;
        if (!throttled)
          g_queue_push_tail (&matching_fevt, fevt);
        continue;
      }

      /* Event for before this frame */
      if (fevt->running_time <= running_time) {
        have_fevt = TRUE;
        if (!throttled)
          g_queue_push_tail (&matching_fevt, fevt);
        continue;
      }

      /* Otherwise all following events are in the future */
      break;
    }

    if (throttled && have_fevt) {
      GstClockTime last_time;

      if (priv->last_force_key_unit_request != GST_CLOCK_TIME_NONE &&
          priv->last_force_key_unit_request +
          priv->min_force_key_unit_interval > running_time) {
        last_time = priv->last_force_key_unit_request;
      } else {
        last_time = priv->last_key_unit;
      }

      GST_DEBUG_OBJECT (encoder,
          "Not requesting a new key unit yet due to throttling (%"
          GST_TIME_FORMAT " + %" GST_TIME_FORMAT " > %" GST_TIME_FORMAT,
          GST_TIME_ARGS (last_time),
          GST_TIME_ARGS (priv->min_force_key_unit_interval),
          GST_TIME_ARGS (running_time));
      g_queue_clear (&matching_fevt);
    }

    if (matching_fevt.length > 0) {
      ForcedKeyUnitEvent *fevt;
      gboolean all_headers = FALSE;
      gboolean force_keyunit = FALSE;

      while ((fevt = g_queue_pop_head (&matching_fevt))) {
        fevt->pending = TRUE;

        if ((fevt->running_time == GST_CLOCK_TIME_NONE
                && have_pending_none_fevt)
            || (priv->last_force_key_unit_request != GST_CLOCK_TIME_NONE
                && fevt->running_time != GST_CLOCK_TIME_NONE
                && fevt->running_time <= priv->last_force_key_unit_request) ||
            (priv->last_key_unit != GST_CLOCK_TIME_NONE
                && fevt->running_time != GST_CLOCK_TIME_NONE
                && fevt->running_time <= priv->last_key_unit)) {
          GST_DEBUG_OBJECT (encoder,
              "Not requesting another key unit at running time %"
              GST_TIME_FORMAT, GST_TIME_ARGS (fevt->running_time));
        } else {
          force_keyunit = TRUE;
          fevt->frame_id = frame->system_frame_number;
          if (fevt->all_headers)
            all_headers = TRUE;
        }
      }

      if (force_keyunit) {
        GST_DEBUG_OBJECT (encoder,
            "Forcing a key unit at running time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (running_time));

        GST_VIDEO_CODEC_FRAME_SET_FORCE_KEYFRAME (frame);
        if (all_headers)
          GST_VIDEO_CODEC_FRAME_SET_FORCE_KEYFRAME_HEADERS (frame);
        priv->last_force_key_unit_request = running_time;
      }
    }
  }
  GST_OBJECT_UNLOCK (encoder);

  g_queue_push_tail (&priv->frames, gst_video_codec_frame_ref (frame));

  /* new data, more finish needed */
  priv->drained = FALSE;

  GST_LOG_OBJECT (encoder, "passing frame pfn %d to subclass",
      frame->presentation_frame_number);

  frame->deadline =
      gst_segment_to_running_time (&encoder->input_segment, GST_FORMAT_TIME,
      frame->pts);

  ret = klass->handle_frame (encoder, frame);

done:
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return ret;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (encoder, CORE, NEGOTIATION, (NULL),
        ("encoder not initialized"));
    gst_buffer_unref (buf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static GstStateChangeReturn
gst_video_encoder_change_state (GstElement * element, GstStateChange transition)
{
  GstVideoEncoder *encoder;
  GstVideoEncoderClass *encoder_class;
  GstStateChangeReturn ret;

  encoder = GST_VIDEO_ENCODER (element);
  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      /* open device/library if needed */
      if (encoder_class->open && !encoder_class->open (encoder))
        goto open_failed;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
      gst_video_encoder_reset (encoder, TRUE);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

      /* Initialize device/library if needed */
      if (encoder_class->start && !encoder_class->start (encoder))
        goto start_failed;
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      gboolean stopped = TRUE;

      if (encoder_class->stop)
        stopped = encoder_class->stop (encoder);

      GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
      gst_video_encoder_reset (encoder, TRUE);
      GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

      if (!stopped)
        goto stop_failed;
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      /* close device/library if needed */
      if (encoder_class->close && !encoder_class->close (encoder))
        goto close_failed;
      break;
    default:
      break;
  }

  return ret;

  /* Errors */

open_failed:
  {
    GST_ELEMENT_ERROR (encoder, LIBRARY, INIT, (NULL),
        ("Failed to open encoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

start_failed:
  {
    GST_ELEMENT_ERROR (encoder, LIBRARY, INIT, (NULL),
        ("Failed to start encoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

stop_failed:
  {
    GST_ELEMENT_ERROR (encoder, LIBRARY, INIT, (NULL),
        ("Failed to stop encoder"));
    return GST_STATE_CHANGE_FAILURE;
  }

close_failed:
  {
    GST_ELEMENT_ERROR (encoder, LIBRARY, INIT, (NULL),
        ("Failed to close encoder"));
    return GST_STATE_CHANGE_FAILURE;
  }
}

static gboolean
gst_video_encoder_negotiate_default (GstVideoEncoder * encoder)
{
  GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);
  GstAllocator *allocator;
  GstAllocationParams params;
  gboolean ret = TRUE;
  GstVideoCodecState *state = encoder->priv->output_state;
  GstVideoInfo *info = &state->info;
  GstQuery *query = NULL;
  GstVideoCodecFrame *frame;
  GstCaps *prevcaps;
  gchar *colorimetry;

  g_return_val_if_fail (state->caps != NULL, FALSE);

  if (encoder->priv->output_state_changed) {
    GstStructure *out_struct;

    state->caps = gst_caps_make_writable (state->caps);

    /* Fill caps */
    gst_caps_set_simple (state->caps, "width", G_TYPE_INT, info->width,
        "height", G_TYPE_INT, info->height,
        "pixel-aspect-ratio", GST_TYPE_FRACTION,
        info->par_n, info->par_d, NULL);
    if (info->flags & GST_VIDEO_FLAG_VARIABLE_FPS && info->fps_n != 0) {
      /* variable fps with a max-framerate */
      gst_caps_set_simple (state->caps, "framerate", GST_TYPE_FRACTION, 0, 1,
          "max-framerate", GST_TYPE_FRACTION, info->fps_n, info->fps_d, NULL);
    } else {
      /* no variable fps or no max-framerate */
      gst_caps_set_simple (state->caps, "framerate", GST_TYPE_FRACTION,
          info->fps_n, info->fps_d, NULL);
    }
    if (state->codec_data)
      gst_caps_set_simple (state->caps, "codec_data", GST_TYPE_BUFFER,
          state->codec_data, NULL);

    gst_caps_set_simple (state->caps, "interlace-mode", G_TYPE_STRING,
        gst_video_interlace_mode_to_string (info->interlace_mode), NULL);
    if (info->interlace_mode == GST_VIDEO_INTERLACE_MODE_INTERLEAVED &&
        GST_VIDEO_INFO_FIELD_ORDER (info) != GST_VIDEO_FIELD_ORDER_UNKNOWN)
      gst_caps_set_simple (state->caps, "field-order", G_TYPE_STRING,
          gst_video_field_order_to_string (GST_VIDEO_INFO_FIELD_ORDER (info)),
          NULL);

    colorimetry = gst_video_colorimetry_to_string (&info->colorimetry);
    if (colorimetry)
      gst_caps_set_simple (state->caps, "colorimetry", G_TYPE_STRING,
          colorimetry, NULL);
    g_free (colorimetry);

    if (info->chroma_site != GST_VIDEO_CHROMA_SITE_UNKNOWN) {
      gchar *chroma_site = gst_video_chroma_site_to_string (info->chroma_site);

      if (!chroma_site) {
        GST_WARNING ("Couldn't convert chroma-site 0x%x to string",
            info->chroma_site);
      } else {
        gst_caps_set_simple (state->caps,
            "chroma-site", G_TYPE_STRING, chroma_site, NULL);
        g_free (chroma_site);
      }
    }

    if (GST_VIDEO_INFO_MULTIVIEW_MODE (info) != GST_VIDEO_MULTIVIEW_MODE_NONE) {
      const gchar *caps_mview_mode =
          gst_video_multiview_mode_to_caps_string (GST_VIDEO_INFO_MULTIVIEW_MODE
          (info));

      gst_caps_set_simple (state->caps, "multiview-mode", G_TYPE_STRING,
          caps_mview_mode, "multiview-flags", GST_TYPE_VIDEO_MULTIVIEW_FLAGSET,
          GST_VIDEO_INFO_MULTIVIEW_FLAGS (info), GST_FLAG_SET_MASK_EXACT, NULL);
    }

    out_struct = gst_caps_get_structure (state->caps, 0);

    /* forward upstream mastering display info and content light level
     * if subclass didn't set */
    if (state->mastering_display_info &&
        !gst_structure_has_field (out_struct, "mastering-display-info")) {
      gst_video_mastering_display_info_add_to_caps
          (state->mastering_display_info, state->caps);
    }

    if (state->content_light_level &&
        !gst_structure_has_field (out_struct, "content-light-level")) {
      gst_video_content_light_level_add_to_caps (state->content_light_level,
          state->caps);
    }

    encoder->priv->output_state_changed = FALSE;
  }

  if (state->allocation_caps == NULL)
    state->allocation_caps = gst_caps_ref (state->caps);

  /* Push all pending pre-caps events of the oldest frame before
   * setting caps */
  frame = encoder->priv->frames.head ? encoder->priv->frames.head->data : NULL;
  if (frame || encoder->priv->current_frame_events) {
    GList **events, *l;

    if (frame) {
      events = &frame->events;
    } else {
      events = &encoder->priv->current_frame_events;
    }

    for (l = g_list_last (*events); l;) {
      GstEvent *event = GST_EVENT (l->data);
      GList *tmp;

      if (GST_EVENT_TYPE (event) < GST_EVENT_CAPS) {
        gst_video_encoder_push_event (encoder, event);
        tmp = l;
        l = l->prev;
        *events = g_list_delete_link (*events, tmp);
      } else {
        l = l->prev;
      }
    }
  }

  prevcaps = gst_pad_get_current_caps (encoder->srcpad);
  if (!prevcaps || !gst_caps_is_equal (prevcaps, state->caps))
    ret = gst_pad_set_caps (encoder->srcpad, state->caps);
  else
    ret = TRUE;
  if (prevcaps)
    gst_caps_unref (prevcaps);

  if (!ret)
    goto done;

  query = gst_query_new_allocation (state->allocation_caps, TRUE);
  if (!gst_pad_peer_query (encoder->srcpad, query)) {
    GST_DEBUG_OBJECT (encoder, "didn't get downstream ALLOCATION hints");
  }

  g_assert (klass->decide_allocation != NULL);
  ret = klass->decide_allocation (encoder, query);

  GST_DEBUG_OBJECT (encoder, "ALLOCATION (%d) params: %" GST_PTR_FORMAT, ret,
      query);

  if (!ret)
    goto no_decide_allocation;

  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
  }

  if (encoder->priv->allocator)
    gst_object_unref (encoder->priv->allocator);
  encoder->priv->allocator = allocator;
  encoder->priv->params = params;

done:
  if (query)
    gst_query_unref (query);

  return ret;

  /* Errors */
no_decide_allocation:
  {
    GST_WARNING_OBJECT (encoder, "Subclass failed to decide allocation");
    goto done;
  }
}

static gboolean
gst_video_encoder_negotiate_unlocked (GstVideoEncoder * encoder)
{
  GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);
  gboolean ret = TRUE;

  if (G_LIKELY (klass->negotiate))
    ret = klass->negotiate (encoder);

  return ret;
}

/**
 * gst_video_encoder_negotiate:
 * @encoder: a #GstVideoEncoder
 *
 * Negotiate with downstream elements to currently configured #GstVideoCodecState.
 * Unmark GST_PAD_FLAG_NEED_RECONFIGURE in any case. But mark it again if
 * negotiate fails.
 *
 * Returns: %TRUE if the negotiation succeeded, else %FALSE.
 */
gboolean
gst_video_encoder_negotiate (GstVideoEncoder * encoder)
{
  GstVideoEncoderClass *klass;
  gboolean ret = TRUE;

  g_return_val_if_fail (GST_IS_VIDEO_ENCODER (encoder), FALSE);
  g_return_val_if_fail (encoder->priv->output_state, FALSE);

  klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  gst_pad_check_reconfigure (encoder->srcpad);
  if (klass->negotiate) {
    ret = klass->negotiate (encoder);
    if (!ret)
      gst_pad_mark_reconfigure (encoder->srcpad);
  }
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return ret;
}

/**
 * gst_video_encoder_allocate_output_buffer:
 * @encoder: a #GstVideoEncoder
 * @size: size of the buffer
 *
 * Helper function that allocates a buffer to hold an encoded video frame
 * for @encoder's current #GstVideoCodecState.
 *
 * Returns: (transfer full): allocated buffer
 */
GstBuffer *
gst_video_encoder_allocate_output_buffer (GstVideoEncoder * encoder, gsize size)
{
  GstBuffer *buffer;
  gboolean needs_reconfigure = FALSE;

  g_return_val_if_fail (size > 0, NULL);

  GST_DEBUG ("alloc src buffer");

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  needs_reconfigure = gst_pad_check_reconfigure (encoder->srcpad);
  if (G_UNLIKELY (encoder->priv->output_state_changed
          || (encoder->priv->output_state && needs_reconfigure))) {
    if (!gst_video_encoder_negotiate_unlocked (encoder)) {
      GST_DEBUG_OBJECT (encoder, "Failed to negotiate, fallback allocation");
      gst_pad_mark_reconfigure (encoder->srcpad);
      goto fallback;
    }
  }

  buffer =
      gst_buffer_new_allocate (encoder->priv->allocator, size,
      &encoder->priv->params);
  if (!buffer) {
    GST_INFO_OBJECT (encoder, "couldn't allocate output buffer");
    goto fallback;
  }

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return buffer;

fallback:
  buffer = gst_buffer_new_allocate (NULL, size, NULL);

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return buffer;
}

/**
 * gst_video_encoder_allocate_output_frame:
 * @encoder: a #GstVideoEncoder
 * @frame: a #GstVideoCodecFrame
 * @size: size of the buffer
 *
 * Helper function that allocates a buffer to hold an encoded video frame for @encoder's
 * current #GstVideoCodecState.  Subclass should already have configured video
 * state and set src pad caps.
 *
 * The buffer allocated here is owned by the frame and you should only
 * keep references to the frame, not the buffer.
 *
 * Returns: %GST_FLOW_OK if an output buffer could be allocated
 */
GstFlowReturn
gst_video_encoder_allocate_output_frame (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame, gsize size)
{
  gboolean needs_reconfigure = FALSE;

  g_return_val_if_fail (frame->output_buffer == NULL, GST_FLOW_ERROR);

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  needs_reconfigure = gst_pad_check_reconfigure (encoder->srcpad);
  if (G_UNLIKELY (encoder->priv->output_state_changed
          || (encoder->priv->output_state && needs_reconfigure))) {
    if (!gst_video_encoder_negotiate_unlocked (encoder)) {
      GST_DEBUG_OBJECT (encoder, "Failed to negotiate, fallback allocation");
      gst_pad_mark_reconfigure (encoder->srcpad);
    }
  }

  GST_LOG_OBJECT (encoder, "alloc buffer size %" G_GSIZE_FORMAT, size);

  frame->output_buffer =
      gst_buffer_new_allocate (encoder->priv->allocator, size,
      &encoder->priv->params);

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return frame->output_buffer ? GST_FLOW_OK : GST_FLOW_ERROR;
}

static void
gst_video_encoder_release_frame (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame)
{
  GList *link;

  /* unref once from the list */
  link = g_queue_find (&enc->priv->frames, frame);
  if (link) {
    gst_video_codec_frame_unref (frame);
    g_queue_delete_link (&enc->priv->frames, link);
  }
  /* unref because this function takes ownership */
  gst_video_codec_frame_unref (frame);
}

static gboolean
gst_video_encoder_transform_meta_default (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame, GstMeta * meta)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;
  const gchar *const supported_tags[] = {
    GST_META_TAG_VIDEO_STR,
    GST_META_TAG_VIDEO_ORIENTATION_STR,
    GST_META_TAG_VIDEO_SIZE_STR,
    NULL,
  };

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags)
    return TRUE;

  while (*tags) {
    if (!g_strv_contains (supported_tags, *tags))
      return FALSE;
    tags++;
  }

  return TRUE;
}

typedef struct
{
  GstVideoEncoder *encoder;
  GstVideoCodecFrame *frame;
} CopyMetaData;

static gboolean
foreach_metadata (GstBuffer * inbuf, GstMeta ** meta, gpointer user_data)
{
  CopyMetaData *data = user_data;
  GstVideoEncoder *encoder = data->encoder;
  GstVideoEncoderClass *klass = GST_VIDEO_ENCODER_GET_CLASS (encoder);
  GstVideoCodecFrame *frame = data->frame;
  const GstMetaInfo *info = (*meta)->info;
  gboolean do_copy = FALSE;

  if (gst_meta_api_type_has_tag (info->api, _gst_meta_tag_memory)
      || gst_meta_api_type_has_tag (info->api, _gst_meta_tag_memory_reference)) {
    /* never call the transform_meta with memory specific metadata */
    GST_DEBUG_OBJECT (encoder, "not copying memory specific metadata %s",
        g_type_name (info->api));
    do_copy = FALSE;
  } else if (klass->transform_meta) {
    do_copy = klass->transform_meta (encoder, frame, *meta);
    GST_DEBUG_OBJECT (encoder, "transformed metadata %s: copy: %d",
        g_type_name (info->api), do_copy);
  }

  /* we only copy metadata when the subclass implemented a transform_meta
   * function and when it returns %TRUE */
  if (do_copy && info->transform_func) {
    GstMetaTransformCopy copy_data = { FALSE, 0, -1 };
    GST_DEBUG_OBJECT (encoder, "copy metadata %s", g_type_name (info->api));
    /* simply copy then */
    info->transform_func (frame->output_buffer, *meta, inbuf,
        _gst_meta_transform_copy, &copy_data);
  }
  return TRUE;
}

static void
gst_video_encoder_drop_frame (GstVideoEncoder * enc, GstVideoCodecFrame * frame)
{
  GstVideoEncoderPrivate *priv = enc->priv;
  GstClockTime stream_time, jitter, earliest_time, qostime, timestamp;
  GstSegment *segment;
  GstMessage *qos_msg;
  gdouble proportion;

  GST_DEBUG_OBJECT (enc, "dropping frame %" GST_TIME_FORMAT,
      GST_TIME_ARGS (frame->pts));

  priv->dropped++;

  /* post QoS message */
  GST_OBJECT_LOCK (enc);
  proportion = priv->proportion;
  earliest_time = priv->earliest_time;
  GST_OBJECT_UNLOCK (enc);

  timestamp = frame->pts;
  segment = &enc->output_segment;
  if (G_UNLIKELY (segment->format == GST_FORMAT_UNDEFINED))
    segment = &enc->input_segment;
  stream_time =
      gst_segment_to_stream_time (segment, GST_FORMAT_TIME, timestamp);
  qostime = gst_segment_to_running_time (segment, GST_FORMAT_TIME, timestamp);
  jitter = GST_CLOCK_DIFF (qostime, earliest_time);
  qos_msg =
      gst_message_new_qos (GST_OBJECT_CAST (enc), FALSE, qostime, stream_time,
      timestamp, GST_CLOCK_TIME_NONE);
  gst_message_set_qos_values (qos_msg, jitter, proportion, 1000000);
  gst_message_set_qos_stats (qos_msg, GST_FORMAT_BUFFERS,
      priv->processed, priv->dropped);
  gst_element_post_message (GST_ELEMENT_CAST (enc), qos_msg);
}

static GstFlowReturn
gst_video_encoder_can_push_unlocked (GstVideoEncoder * encoder)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  gboolean needs_reconfigure;

  needs_reconfigure = gst_pad_check_reconfigure (encoder->srcpad);
  if (G_UNLIKELY (priv->output_state_changed || (priv->output_state
              && needs_reconfigure))) {
    if (!gst_video_encoder_negotiate_unlocked (encoder)) {
      gst_pad_mark_reconfigure (encoder->srcpad);
      if (GST_PAD_IS_FLUSHING (encoder->srcpad))
        return GST_FLOW_FLUSHING;
      else
        return GST_FLOW_NOT_NEGOTIATED;
    }
  }

  if (G_UNLIKELY (priv->output_state == NULL)) {
    GST_ERROR_OBJECT (encoder, "Output state was not configured");
    GST_ELEMENT_ERROR (encoder, LIBRARY, FAILED,
        ("Output state was not configured"), (NULL));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static void
gst_video_encoder_push_pending_unlocked (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GList *l;

  /* Push all pending events that arrived before this frame */
  for (l = priv->frames.head; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;

    if (tmp->events) {
      GList *k;

      for (k = g_list_last (tmp->events); k; k = k->prev)
        gst_video_encoder_push_event (encoder, k->data);
      g_list_free (tmp->events);
      tmp->events = NULL;
    }

    if (tmp == frame)
      break;
  }

  gst_video_encoder_check_and_push_tags (encoder);
}

static void
gst_video_encoder_infer_dts_unlocked (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  /* DTS is expected to be monotonously increasing,
   * so a good guess is the lowest unsent PTS (all being OK) */
  GstVideoEncoderPrivate *priv = encoder->priv;
  GList *l;
  GstClockTime min_ts = GST_CLOCK_TIME_NONE;
  GstVideoCodecFrame *oframe = NULL;
  gboolean seen_none = FALSE;

  /* some maintenance regardless */
  for (l = priv->frames.head; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;

    if (!GST_CLOCK_TIME_IS_VALID (tmp->abidata.ABI.ts)) {
      seen_none = TRUE;
      continue;
    }

    if (!GST_CLOCK_TIME_IS_VALID (min_ts) || tmp->abidata.ABI.ts < min_ts) {
      min_ts = tmp->abidata.ABI.ts;
      oframe = tmp;
    }
  }
  /* save a ts if needed */
  if (oframe && oframe != frame) {
    oframe->abidata.ABI.ts = frame->abidata.ABI.ts;
  }

  /* and set if needed */
  if (!GST_CLOCK_TIME_IS_VALID (frame->dts) && !seen_none) {
    frame->dts = min_ts;
    GST_DEBUG_OBJECT (encoder,
        "no valid DTS, using oldest PTS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (frame->pts));
  }
}

static void
gst_video_encoder_send_header_unlocked (GstVideoEncoder * encoder,
    gboolean * discont, gboolean key_unit)
{
  GstVideoEncoderPrivate *priv = encoder->priv;

  if (G_UNLIKELY (priv->new_headers)) {
    GList *tmp;

    GST_DEBUG_OBJECT (encoder, "Sending headers");

    /* First make all buffers metadata-writable */
    for (tmp = priv->headers; tmp; tmp = tmp->next) {
      GstBuffer *tmpbuf = GST_BUFFER (tmp->data);

      tmp->data = tmpbuf = gst_buffer_make_writable (tmpbuf);

      GST_OBJECT_LOCK (encoder);
      priv->bytes += gst_buffer_get_size (tmpbuf);
      GST_OBJECT_UNLOCK (encoder);

      if (G_UNLIKELY (key_unit)) {
        key_unit = FALSE;
        GST_BUFFER_FLAG_UNSET (tmpbuf, GST_BUFFER_FLAG_DELTA_UNIT);
      } else {
        GST_BUFFER_FLAG_SET (tmpbuf, GST_BUFFER_FLAG_DELTA_UNIT);
      }

      if (G_UNLIKELY (*discont)) {
        GST_LOG_OBJECT (encoder, "marking discont");
        GST_BUFFER_FLAG_SET (tmpbuf, GST_BUFFER_FLAG_DISCONT);
        *discont = FALSE;
      } else {
        GST_BUFFER_FLAG_UNSET (tmpbuf, GST_BUFFER_FLAG_DISCONT);
      }

      gst_pad_push (encoder->srcpad, gst_buffer_ref (tmpbuf));
    }
    priv->new_headers = FALSE;
  }
}

static void
gst_video_encoder_transform_meta_unlocked (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoEncoderClass *encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  if (encoder_class->transform_meta) {
    if (G_LIKELY (frame->input_buffer)) {
      CopyMetaData data;

      data.encoder = encoder;
      data.frame = frame;
      gst_buffer_foreach_meta (frame->input_buffer, foreach_metadata, &data);
    } else {
      GST_FIXME_OBJECT (encoder,
          "Can't copy metadata because input frame disappeared");
    }
  }
}

static void
gst_video_encoder_send_key_unit_unlocked (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, gboolean * send_headers)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GstClockTime stream_time, running_time;
  GstEvent *ev;
  GList *l;
  GQueue matching_fevt = G_QUEUE_INIT;
  ForcedKeyUnitEvent *fevt;

  running_time =
      gst_segment_to_running_time (&encoder->output_segment, GST_FORMAT_TIME,
      frame->pts);

  GST_OBJECT_LOCK (encoder);
  for (l = priv->force_key_unit.head; l;) {
    fevt = l->data;

    /* Skip non-pending keyunits */
    if (!fevt->pending) {
      l = l->next;
      continue;
    }

    /* Exact match using the frame id */
    if (frame->system_frame_number == fevt->frame_id) {
      GList *next = l->next;
      g_queue_push_tail (&matching_fevt, fevt);
      g_queue_delete_link (&priv->force_key_unit, l);
      l = next;
      continue;
    }

    /* Simple case, keyunit ASAP */
    if (fevt->running_time == GST_CLOCK_TIME_NONE) {
      GList *next = l->next;
      g_queue_push_tail (&matching_fevt, fevt);
      g_queue_delete_link (&priv->force_key_unit, l);
      l = next;
      continue;
    }

    /* Event for before this frame */
    if (fevt->running_time <= running_time) {
      GList *next = l->next;
      g_queue_push_tail (&matching_fevt, fevt);
      g_queue_delete_link (&priv->force_key_unit, l);
      l = next;
      continue;
    }

    /* Otherwise all following events are in the future */
    break;
  }

  GST_OBJECT_UNLOCK (encoder);

  while ((fevt = g_queue_pop_head (&matching_fevt))) {
    stream_time =
        gst_segment_to_stream_time (&encoder->output_segment, GST_FORMAT_TIME,
        frame->pts);

    ev = gst_video_event_new_downstream_force_key_unit
        (frame->pts, stream_time, running_time, fevt->all_headers, fevt->count);

    gst_video_encoder_push_event (encoder, ev);

    if (fevt->all_headers)
      *send_headers = TRUE;

    GST_DEBUG_OBJECT (encoder,
        "Forced key unit: running-time %" GST_TIME_FORMAT
        ", all_headers %d, count %u",
        GST_TIME_ARGS (running_time), fevt->all_headers, fevt->count);
    forced_key_unit_event_free (fevt);
  }
}

/**
 * gst_video_encoder_finish_frame:
 * @encoder: a #GstVideoEncoder
 * @frame: (transfer full): an encoded #GstVideoCodecFrame
 *
 * @frame must have a valid encoded data buffer, whose metadata fields
 * are then appropriately set according to frame data or no buffer at
 * all if the frame should be dropped.
 * It is subsequently pushed downstream or provided to @pre_push.
 * In any case, the frame is considered finished and released.
 *
 * After calling this function the output buffer of the frame is to be
 * considered read-only. This function will also change the metadata
 * of the buffer.
 *
 * Returns: a #GstFlowReturn resulting from sending data downstream
 */
GstFlowReturn
gst_video_encoder_finish_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoEncoderClass *encoder_class;
  gboolean send_headers = FALSE;
  gboolean key_unit = FALSE;
  gboolean discont = FALSE;
  GstBuffer *buffer;

  g_return_val_if_fail (frame, GST_FLOW_ERROR);

  discont = (frame->presentation_frame_number == 0
      && frame->abidata.ABI.num_subframes == 0);

  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_LOG_OBJECT (encoder,
      "finish frame fpn %d sync point: %d", frame->presentation_frame_number,
      GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame));

  GST_LOG_OBJECT (encoder, "frame PTS %" GST_TIME_FORMAT
      ", DTS %" GST_TIME_FORMAT, GST_TIME_ARGS (frame->pts),
      GST_TIME_ARGS (frame->dts));

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);

  ret = gst_video_encoder_can_push_unlocked (encoder);
  if (ret != GST_FLOW_OK)
    goto done;

  if (frame->abidata.ABI.num_subframes == 0)
    gst_video_encoder_push_pending_unlocked (encoder, frame);

  /* no buffer data means this frame is skipped/dropped */
  if (!frame->output_buffer) {
    gst_video_encoder_drop_frame (encoder, frame);
    goto done;
  }

  priv->processed++;

  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame) && priv->force_key_unit.head)
    gst_video_encoder_send_key_unit_unlocked (encoder, frame, &send_headers);

  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)
      && frame->abidata.ABI.num_subframes == 0) {
    priv->distance_from_sync = 0;
    key_unit = TRUE;
    /* For keyframes, DTS = PTS, if encoder doesn't decide otherwise */
    if (!GST_CLOCK_TIME_IS_VALID (frame->dts)) {
      frame->dts = frame->pts;
    }
    priv->last_key_unit =
        gst_segment_to_running_time (&encoder->output_segment, GST_FORMAT_TIME,
        frame->pts);
  }

  gst_video_encoder_infer_dts_unlocked (encoder, frame);

  frame->distance_from_sync = priv->distance_from_sync;
  priv->distance_from_sync++;

  /* We need a writable buffer for the metadata changes below */
  frame->output_buffer = gst_buffer_make_writable (frame->output_buffer);

  GST_BUFFER_PTS (frame->output_buffer) = frame->pts;
  GST_BUFFER_DTS (frame->output_buffer) = frame->dts;
  GST_BUFFER_DURATION (frame->output_buffer) = frame->duration;

  /* At this stage we have a full frame in subframe use case ,
   * let's mark it to enabled some latency optimization
   *  in some uses cases like RTP. */

  GST_BUFFER_FLAG_SET (frame->output_buffer, GST_VIDEO_BUFFER_FLAG_MARKER);

  GST_OBJECT_LOCK (encoder);
  /* update rate estimate */
  priv->bytes += gst_buffer_get_size (frame->output_buffer);
  if (GST_CLOCK_TIME_IS_VALID (frame->duration)) {
    priv->time += frame->duration;
  } else {
    /* better none than nothing valid */
    priv->time = GST_CLOCK_TIME_NONE;
  }
  GST_OBJECT_UNLOCK (encoder);

  if (G_UNLIKELY (send_headers))
    priv->new_headers = TRUE;

  gst_video_encoder_send_header_unlocked (encoder, &discont, key_unit);

  if (key_unit) {
    GST_BUFFER_FLAG_UNSET (frame->output_buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  } else {
    GST_BUFFER_FLAG_SET (frame->output_buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  if (G_UNLIKELY (discont)) {
    GST_LOG_OBJECT (encoder, "marking discont");
    GST_BUFFER_FLAG_SET (frame->output_buffer, GST_BUFFER_FLAG_DISCONT);
  }

  if (encoder_class->pre_push)
    ret = encoder_class->pre_push (encoder, frame);

  gst_video_encoder_transform_meta_unlocked (encoder, frame);

  /* Get an additional ref to the buffer, which is going to be pushed
   * downstream, the original ref is owned by the frame */
  if (ret == GST_FLOW_OK)
    buffer = gst_buffer_ref (frame->output_buffer);

  /* Release frame so the buffer is writable when we push it downstream
   * if possible, i.e. if the subclass does not hold additional references
   * to the frame
   */
  gst_video_encoder_release_frame (encoder, frame);
  frame = NULL;

  if (ret == GST_FLOW_OK) {
    GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
    ret = gst_pad_push (encoder->srcpad, buffer);
    GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  }

done:
  /* handed out */
  if (frame)
    gst_video_encoder_release_frame (encoder, frame);

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return ret;
}

/**
 * gst_video_encoder_finish_subframe:
 * @encoder: a #GstVideoEncoder
 * @frame: (transfer none): a #GstVideoCodecFrame being encoded
 *
 * If multiple subframes are produced for one input frame then use this method
 * for each subframe, except for the last one. Before calling this function,
 * you need to fill frame->output_buffer with the encoded buffer to push.

 * You must call #gst_video_encoder_finish_frame() for the last sub-frame
 * to tell the encoder that the frame has been fully encoded.
 *
 * This function will change the metadata of @frame and frame->output_buffer
 * will be pushed downstream.
 *
 * Returns: a #GstFlowReturn resulting from pushing the buffer downstream.
 *
 * Since: 1.18
 */
GstFlowReturn
gst_video_encoder_finish_subframe (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GstVideoEncoderClass *encoder_class;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *subframe_buffer = NULL;
  gboolean discont = FALSE;
  gboolean send_headers = FALSE;
  gboolean key_unit = FALSE;

  g_return_val_if_fail (frame, GST_FLOW_ERROR);
  g_return_val_if_fail (frame->output_buffer, GST_FLOW_ERROR);

  subframe_buffer = frame->output_buffer;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  discont = (frame->presentation_frame_number == 0
      && frame->abidata.ABI.num_subframes == 0);

  encoder_class = GST_VIDEO_ENCODER_GET_CLASS (encoder);

  GST_LOG_OBJECT (encoder,
      "finish subframe %u of frame fpn %u PTS %" GST_TIME_FORMAT ", DTS %"
      GST_TIME_FORMAT " sync point: %d", frame->abidata.ABI.num_subframes,
      frame->presentation_frame_number, GST_TIME_ARGS (frame->pts),
      GST_TIME_ARGS (frame->dts), GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame));

  ret = gst_video_encoder_can_push_unlocked (encoder);
  if (ret != GST_FLOW_OK)
    goto done;

  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame) && priv->force_key_unit.head)
    gst_video_encoder_send_key_unit_unlocked (encoder, frame, &send_headers);

  /* Push pending events only for the first subframe ie segment event.
   * Push new incoming events on finish_frame otherwise.
   */
  if (frame->abidata.ABI.num_subframes == 0)
    gst_video_encoder_push_pending_unlocked (encoder, frame);

  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame)
      && frame->abidata.ABI.num_subframes == 0) {
    priv->distance_from_sync = 0;
    key_unit = TRUE;
    /* For keyframes, DTS = PTS, if encoder doesn't decide otherwise */
    if (!GST_CLOCK_TIME_IS_VALID (frame->dts)) {
      frame->dts = frame->pts;
    }
    priv->last_key_unit =
        gst_segment_to_running_time (&encoder->output_segment, GST_FORMAT_TIME,
        frame->pts);
  }

  gst_video_encoder_infer_dts_unlocked (encoder, frame);

  /* We need a writable buffer for the metadata changes below */
  subframe_buffer = gst_buffer_make_writable (subframe_buffer);

  GST_BUFFER_PTS (subframe_buffer) = frame->pts;
  GST_BUFFER_DTS (subframe_buffer) = frame->dts;
  GST_BUFFER_DURATION (subframe_buffer) = frame->duration;

  GST_OBJECT_LOCK (encoder);
  /* update rate estimate */
  priv->bytes += gst_buffer_get_size (subframe_buffer);
  GST_OBJECT_UNLOCK (encoder);

  if (G_UNLIKELY (send_headers))
    priv->new_headers = TRUE;

  gst_video_encoder_send_header_unlocked (encoder, &discont, key_unit);

  if (key_unit) {
    GST_BUFFER_FLAG_UNSET (subframe_buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  } else {
    GST_BUFFER_FLAG_SET (subframe_buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  }

  if (G_UNLIKELY (discont)) {
    GST_LOG_OBJECT (encoder, "marking discont buffer: %" GST_PTR_FORMAT,
        subframe_buffer);
    GST_BUFFER_FLAG_SET (subframe_buffer, GST_BUFFER_FLAG_DISCONT);
  }

  if (encoder_class->pre_push) {
    ret = encoder_class->pre_push (encoder, frame);
  }

  gst_video_encoder_transform_meta_unlocked (encoder, frame);

  if (ret == GST_FLOW_OK) {
    GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
    ret = gst_pad_push (encoder->srcpad, subframe_buffer);
    subframe_buffer = NULL;
    GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  }

done:
  frame->abidata.ABI.num_subframes++;
  if (subframe_buffer)
    gst_buffer_unref (subframe_buffer);
  frame->output_buffer = NULL;

  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return ret;
}

/**
 * gst_video_encoder_get_output_state:
 * @encoder: a #GstVideoEncoder
 *
 * Get the current #GstVideoCodecState
 *
 * Returns: (transfer full) (nullable): #GstVideoCodecState describing format of video data.
 */
GstVideoCodecState *
gst_video_encoder_get_output_state (GstVideoEncoder * encoder)
{
  GstVideoCodecState *state = NULL;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  if (encoder->priv->output_state)
    state = gst_video_codec_state_ref (encoder->priv->output_state);
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return state;
}

/**
 * gst_video_encoder_set_output_state:
 * @encoder: a #GstVideoEncoder
 * @caps: (transfer full): the #GstCaps to use for the output
 * @reference: (nullable) (transfer none): An optional reference @GstVideoCodecState
 *
 * Creates a new #GstVideoCodecState with the specified caps as the output state
 * for the encoder.
 * Any previously set output state on @encoder will be replaced by the newly
 * created one.
 *
 * The specified @caps should not contain any resolution, pixel-aspect-ratio,
 * framerate, codec-data, .... Those should be specified instead in the returned
 * #GstVideoCodecState.
 *
 * If the subclass wishes to copy over existing fields (like pixel aspect ratio,
 * or framerate) from an existing #GstVideoCodecState, it can be provided as a
 * @reference.
 *
 * If the subclass wishes to override some fields from the output state (like
 * pixel-aspect-ratio or framerate) it can do so on the returned #GstVideoCodecState.
 *
 * The new output state will only take effect (set on pads and buffers) starting
 * from the next call to #gst_video_encoder_finish_frame().
 *
 * Returns: (transfer full) (nullable): the newly configured output state.
 */
GstVideoCodecState *
gst_video_encoder_set_output_state (GstVideoEncoder * encoder, GstCaps * caps,
    GstVideoCodecState * reference)
{
  GstVideoEncoderPrivate *priv = encoder->priv;
  GstVideoCodecState *state;

  g_return_val_if_fail (caps != NULL, NULL);

  state = _new_output_state (caps, reference);
  if (!state)
    return NULL;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  if (priv->output_state)
    gst_video_codec_state_unref (priv->output_state);
  priv->output_state = gst_video_codec_state_ref (state);

  if (priv->output_state != NULL && priv->output_state->info.fps_n > 0) {
    priv->qos_frame_duration =
        gst_util_uint64_scale (GST_SECOND, priv->output_state->info.fps_d,
        priv->output_state->info.fps_n);
  } else {
    priv->qos_frame_duration = 0;
  }

  priv->output_state_changed = TRUE;
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return state;
}

/**
 * gst_video_encoder_set_latency:
 * @encoder: a #GstVideoEncoder
 * @min_latency: minimum latency
 * @max_latency: maximum latency
 *
 * Informs baseclass of encoding latency. If the provided values changed from
 * previously provided ones, this will also post a LATENCY message on the bus
 * so the pipeline can reconfigure its global latency.
 */
void
gst_video_encoder_set_latency (GstVideoEncoder * encoder,
    GstClockTime min_latency, GstClockTime max_latency)
{
  gboolean post_message = FALSE;

  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (min_latency));
  g_return_if_fail (max_latency >= min_latency);

  GST_DEBUG_OBJECT (encoder,
      "min_latency:%" GST_TIME_FORMAT " max_latency:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (min_latency), GST_TIME_ARGS (max_latency));

  GST_OBJECT_LOCK (encoder);
  if (encoder->priv->min_latency != min_latency) {
    encoder->priv->min_latency = min_latency;
    post_message = TRUE;
  }
  if (encoder->priv->max_latency != max_latency) {
    encoder->priv->max_latency = max_latency;
    post_message = TRUE;
  }
  if (!encoder->priv->posted_latency_msg) {
    encoder->priv->posted_latency_msg = TRUE;
    post_message = TRUE;
  }
  GST_OBJECT_UNLOCK (encoder);

  if (post_message)
    gst_element_post_message (GST_ELEMENT_CAST (encoder),
        gst_message_new_latency (GST_OBJECT_CAST (encoder)));
}

/**
 * gst_video_encoder_get_latency:
 * @encoder: a #GstVideoEncoder
 * @min_latency: (out) (optional): address of variable in which to store the
 *     configured minimum latency, or %NULL
 * @max_latency: (out) (optional): address of variable in which to store the
 *     configured maximum latency, or %NULL
 *
 * Query the configured encoding latency. Results will be returned via
 * @min_latency and @max_latency.
 */
void
gst_video_encoder_get_latency (GstVideoEncoder * encoder,
    GstClockTime * min_latency, GstClockTime * max_latency)
{
  GST_OBJECT_LOCK (encoder);
  if (min_latency)
    *min_latency = encoder->priv->min_latency;
  if (max_latency)
    *max_latency = encoder->priv->max_latency;
  GST_OBJECT_UNLOCK (encoder);
}

/**
 * gst_video_encoder_get_oldest_frame:
 * @encoder: a #GstVideoEncoder
 *
 * Get the oldest unfinished pending #GstVideoCodecFrame
 *
 * Returns: (transfer full) (nullable): oldest unfinished pending #GstVideoCodecFrame
 */
GstVideoCodecFrame *
gst_video_encoder_get_oldest_frame (GstVideoEncoder * encoder)
{
  GstVideoCodecFrame *frame = NULL;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  if (encoder->priv->frames.head)
    frame = gst_video_codec_frame_ref (encoder->priv->frames.head->data);
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return (GstVideoCodecFrame *) frame;
}

/**
 * gst_video_encoder_get_frame:
 * @encoder: a #GstVideoEncoder
 * @frame_number: system_frame_number of a frame
 *
 * Get a pending unfinished #GstVideoCodecFrame
 *
 * Returns: (transfer full) (nullable): pending unfinished #GstVideoCodecFrame identified by @frame_number.
 */
GstVideoCodecFrame *
gst_video_encoder_get_frame (GstVideoEncoder * encoder, int frame_number)
{
  GList *g;
  GstVideoCodecFrame *frame = NULL;

  GST_DEBUG_OBJECT (encoder, "frame_number : %d", frame_number);

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  for (g = encoder->priv->frames.head; g; g = g->next) {
    GstVideoCodecFrame *tmp = g->data;

    if (tmp->system_frame_number == frame_number) {
      frame = gst_video_codec_frame_ref (tmp);
      break;
    }
  }
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return frame;
}

/**
 * gst_video_encoder_get_frames:
 * @encoder: a #GstVideoEncoder
 *
 * Get all pending unfinished #GstVideoCodecFrame
 *
 * Returns: (transfer full) (element-type GstVideoCodecFrame): pending unfinished #GstVideoCodecFrame.
 */
GList *
gst_video_encoder_get_frames (GstVideoEncoder * encoder)
{
  GList *frames;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  frames =
      g_list_copy_deep (encoder->priv->frames.head,
      (GCopyFunc) gst_video_codec_frame_ref, NULL);
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  return frames;
}

/**
 * gst_video_encoder_merge_tags:
 * @encoder: a #GstVideoEncoder
 * @tags: (nullable): a #GstTagList to merge, or NULL to unset
 *     previously-set tags
 * @mode: the #GstTagMergeMode to use, usually #GST_TAG_MERGE_REPLACE
 *
 * Sets the video encoder tags and how they should be merged with any
 * upstream stream tags. This will override any tags previously-set
 * with gst_video_encoder_merge_tags().
 *
 * Note that this is provided for convenience, and the subclass is
 * not required to use this and can still do tag handling on its own.
 *
 * MT safe.
 */
void
gst_video_encoder_merge_tags (GstVideoEncoder * encoder,
    const GstTagList * tags, GstTagMergeMode mode)
{
  g_return_if_fail (GST_IS_VIDEO_ENCODER (encoder));
  g_return_if_fail (tags == NULL || GST_IS_TAG_LIST (tags));
  g_return_if_fail (tags == NULL || mode != GST_TAG_MERGE_UNDEFINED);

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  if (encoder->priv->tags != tags) {
    if (encoder->priv->tags) {
      gst_tag_list_unref (encoder->priv->tags);
      encoder->priv->tags = NULL;
      encoder->priv->tags_merge_mode = GST_TAG_MERGE_APPEND;
    }
    if (tags) {
      encoder->priv->tags = gst_tag_list_ref ((GstTagList *) tags);
      encoder->priv->tags_merge_mode = mode;
    }

    GST_DEBUG_OBJECT (encoder, "setting encoder tags to %" GST_PTR_FORMAT,
        tags);
    encoder->priv->tags_changed = TRUE;
  }
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);
}

/**
 * gst_video_encoder_get_allocator:
 * @encoder: a #GstVideoEncoder
 * @allocator: (out) (optional) (nullable) (transfer full): the #GstAllocator
 * used
 * @params: (out) (optional) (transfer full): the
 * #GstAllocationParams of @allocator
 *
 * Lets #GstVideoEncoder sub-classes to know the memory @allocator
 * used by the base class and its @params.
 *
 * Unref the @allocator after use it.
 */
void
gst_video_encoder_get_allocator (GstVideoEncoder * encoder,
    GstAllocator ** allocator, GstAllocationParams * params)
{
  g_return_if_fail (GST_IS_VIDEO_ENCODER (encoder));

  if (allocator)
    *allocator = encoder->priv->allocator ?
        gst_object_ref (encoder->priv->allocator) : NULL;

  if (params)
    *params = encoder->priv->params;
}

/**
 * gst_video_encoder_set_min_pts:
 * @encoder: a #GstVideoEncoder
 * @min_pts: minimal PTS that will be passed to handle_frame
 *
 * Request minimal value for PTS passed to handle_frame.
 *
 * For streams with reordered frames this can be used to ensure that there
 * is enough time to accommodate first DTS, which may be less than first PTS
 *
 * Since: 1.6
 */
void
gst_video_encoder_set_min_pts (GstVideoEncoder * encoder, GstClockTime min_pts)
{
  g_return_if_fail (GST_IS_VIDEO_ENCODER (encoder));
  encoder->priv->min_pts = min_pts;
  encoder->priv->time_adjustment = GST_CLOCK_TIME_NONE;
}

/**
 * gst_video_encoder_get_max_encode_time:
 * @encoder: a #GstVideoEncoder
 * @frame: a #GstVideoCodecFrame
 *
 * Determines maximum possible encoding time for @frame that will
 * allow it to encode and arrive in time (as determined by QoS events).
 * In particular, a negative result means encoding in time is no longer possible
 * and should therefore occur as soon/skippy as possible.
 *
 * If no QoS events have been received from downstream, or if
 * #GstVideoEncoder:qos is disabled this function returns #G_MAXINT64.
 *
 * Returns: max decoding time.
 * Since: 1.14
 */
GstClockTimeDiff
gst_video_encoder_get_max_encode_time (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame)
{
  GstClockTimeDiff deadline;
  GstClockTime earliest_time;

  if (!g_atomic_int_get (&encoder->priv->qos_enabled))
    return G_MAXINT64;

  GST_OBJECT_LOCK (encoder);
  earliest_time = encoder->priv->earliest_time;
  if (GST_CLOCK_TIME_IS_VALID (earliest_time)
      && GST_CLOCK_TIME_IS_VALID (frame->deadline))
    deadline = GST_CLOCK_DIFF (earliest_time, frame->deadline);
  else
    deadline = G_MAXINT64;

  GST_LOG_OBJECT (encoder, "earliest %" GST_TIME_FORMAT
      ", frame deadline %" GST_TIME_FORMAT ", deadline %" GST_STIME_FORMAT,
      GST_TIME_ARGS (earliest_time), GST_TIME_ARGS (frame->deadline),
      GST_STIME_ARGS (deadline));

  GST_OBJECT_UNLOCK (encoder);

  return deadline;
}

/**
 * gst_video_encoder_set_qos_enabled:
 * @encoder: the encoder
 * @enabled: the new qos value.
 *
 * Configures @encoder to handle Quality-of-Service events from downstream.
 * Since: 1.14
 */
void
gst_video_encoder_set_qos_enabled (GstVideoEncoder * encoder, gboolean enabled)
{
  g_return_if_fail (GST_IS_VIDEO_ENCODER (encoder));

  g_atomic_int_set (&encoder->priv->qos_enabled, enabled);
}

/**
 * gst_video_encoder_is_qos_enabled:
 * @encoder: the encoder
 *
 * Checks if @encoder is currently configured to handle Quality-of-Service
 * events from downstream.
 *
 * Returns: %TRUE if the encoder is configured to perform Quality-of-Service.
 * Since: 1.14
 */
gboolean
gst_video_encoder_is_qos_enabled (GstVideoEncoder * encoder)
{
  gboolean res;

  g_return_val_if_fail (GST_IS_VIDEO_ENCODER (encoder), FALSE);

  res = g_atomic_int_get (&encoder->priv->qos_enabled);

  return res;
}

/**
 * gst_video_encoder_set_min_force_key_unit_interval:
 * @encoder: the encoder
 * @interval: minimum interval
 *
 * Sets the minimum interval for requesting keyframes based on force-keyunit
 * events. Setting this to 0 will allow to handle every event, setting this to
 * %GST_CLOCK_TIME_NONE causes force-keyunit events to be ignored.
 *
 * Since: 1.18
 */
void
gst_video_encoder_set_min_force_key_unit_interval (GstVideoEncoder * encoder,
    GstClockTime interval)
{
  g_return_if_fail (GST_IS_VIDEO_ENCODER (encoder));

  GST_OBJECT_LOCK (encoder);
  encoder->priv->min_force_key_unit_interval = interval;
  GST_OBJECT_UNLOCK (encoder);
}

/**
 * gst_video_encoder_get_min_force_key_unit_interval:
 * @encoder: the encoder
 *
 * Returns the minimum force-keyunit interval, see gst_video_encoder_set_min_force_key_unit_interval()
 * for more details.
 *
 * Returns: the minimum force-keyunit interval
 *
 * Since: 1.18
 */
GstClockTime
gst_video_encoder_get_min_force_key_unit_interval (GstVideoEncoder * encoder)
{
  GstClockTime interval;

  g_return_val_if_fail (GST_IS_VIDEO_ENCODER (encoder), GST_CLOCK_TIME_NONE);

  GST_OBJECT_LOCK (encoder);
  interval = encoder->priv->min_force_key_unit_interval;
  GST_OBJECT_UNLOCK (encoder);

  return interval;
}
