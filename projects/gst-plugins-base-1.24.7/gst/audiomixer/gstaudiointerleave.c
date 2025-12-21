/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *                    2007 Andy Wingo <wingo at pobox.com>
 *                    2008 Sebastian Dröge <slomo@circular-chaos.org>
 *                    2014 Collabora
 *                        Olivier Crete <olivier.crete@collabora.com>
 *
 * gstaudiointerleave.c: audiointerleave element, N in, one out,
 * samples are added
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
 * SECTION:element-audiointerleave
 * @title: audiointerleave
 *
 */

/* FIXME 0.11: suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstaudiomixerelements.h"
#include "gstaudiointerleave.h"

#define GST_CAT_DEFAULT gst_audio_interleave_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_PAD_0,
  PROP_PAD_CHANNEL
};

G_DEFINE_TYPE (GstAudioInterleavePad, gst_audio_interleave_pad,
    GST_TYPE_AUDIO_AGGREGATOR_PAD);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (audiointerleave, "audiointerleave",
    GST_RANK_NONE, GST_TYPE_AUDIO_INTERLEAVE, audiomixer_element_init (plugin));

static void
gst_audio_interleave_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioInterleavePad *pad = GST_AUDIO_INTERLEAVE_PAD (object);

  switch (prop_id) {
    case PROP_PAD_CHANNEL:
      g_value_set_uint (value, pad->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_audio_interleave_pad_class_init (GstAudioInterleavePadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gst_audio_interleave_pad_get_property;

  g_object_class_install_property (gobject_class,
      PROP_PAD_CHANNEL,
      g_param_spec_uint ("channel",
          "Channel number",
          "Number of the channel of this pad in the output", 0, G_MAXUINT, 0,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_audio_interleave_pad_init (GstAudioInterleavePad * pad)
{
}

enum
{
  PROP_0,
  PROP_CHANNEL_POSITIONS,
  PROP_CHANNEL_POSITIONS_FROM_INPUT
};

/* elementfactory information */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32LE, U32LE, S16LE, U16LE, S8, U8, F32LE, F64LE }") \
  ", layout = (string) { interleaved, non-interleaved }"
#else
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32BE, U32BE, S16BE, U16BE, S8, U8, F32BE, F64BE }") \
  ", layout = (string) { interleaved, non-interleaved }"
#endif

static GstStaticPadTemplate gst_audio_interleave_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("audio/x-raw, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) 1, "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) {non-interleaved, interleaved}")
    );

static GstStaticPadTemplate gst_audio_interleave_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "rate = (int) [ 1, MAX ], "
        "channels = (int) [ 1, MAX ], "
        "format = (string) " GST_AUDIO_FORMATS_ALL ", "
        "layout = (string) interleaved")
    );

static void gst_audio_interleave_child_proxy_init (gpointer g_iface,
    gpointer iface_data);

#define gst_audio_interleave_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAudioInterleave, gst_audio_interleave,
    GST_TYPE_AUDIO_AGGREGATOR, G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY,
        gst_audio_interleave_child_proxy_init));

static void gst_audio_interleave_finalize (GObject * object);
static void gst_audio_interleave_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_audio_interleave_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_audio_interleave_setcaps (GstAudioInterleave * self,
    GstPad * pad, GstCaps * caps);
static GstPad *gst_audio_interleave_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * req_name, const GstCaps * caps);
static void gst_audio_interleave_release_pad (GstElement * element,
    GstPad * pad);

static gboolean gst_audio_interleave_stop (GstAggregator * agg);

static gboolean
gst_audio_interleave_aggregate_one_buffer (GstAudioAggregator * aagg,
    GstAudioAggregatorPad * aaggpad, GstBuffer * inbuf, guint in_offset,
    GstBuffer * outbuf, guint out_offset, guint num_samples);


static void
__remove_channels (GstCaps * caps)
{
  GstStructure *s;
  gint i, size;

  size = gst_caps_get_size (caps);
  for (i = 0; i < size; i++) {
    s = gst_caps_get_structure (caps, i);
    gst_structure_remove_field (s, "channel-mask");
    gst_structure_remove_field (s, "channels");
  }
}

static void
__set_channels (GstCaps * caps, gint channels)
{
  GstStructure *s;
  gint i, size;

  size = gst_caps_get_size (caps);
  for (i = 0; i < size; i++) {
    s = gst_caps_get_structure (caps, i);
    if (channels > 0)
      gst_structure_set (s, "channels", G_TYPE_INT, channels, NULL);
    else
      gst_structure_set (s, "channels", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
  }
}

/* we can only accept caps that we and downstream can handle.
 * if we have filtercaps set, use those to constrain the target caps.
 */
static GstCaps *
gst_audio_interleave_sink_getcaps (GstAggregator * agg, GstPad * pad,
    GstCaps * filter)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (agg);
  GstCaps *result = NULL, *peercaps, *sinkcaps;

  GST_OBJECT_LOCK (self);
  /* If we already have caps on one of the sink pads return them */
  if (self->sinkcaps)
    result = gst_caps_copy (self->sinkcaps);
  GST_OBJECT_UNLOCK (self);

  if (result == NULL) {
    /* get the downstream possible caps */
    peercaps = gst_pad_peer_query_caps (agg->srcpad, NULL);

    /* get the allowed caps on this sinkpad */
    sinkcaps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
    __remove_channels (sinkcaps);
    if (peercaps) {
      peercaps = gst_caps_make_writable (peercaps);
      __remove_channels (peercaps);
      /* if the peer has caps, intersect */
      GST_DEBUG_OBJECT (pad, "intersecting peer and template caps");
      result = gst_caps_intersect (peercaps, sinkcaps);
      gst_caps_unref (peercaps);
      gst_caps_unref (sinkcaps);
    } else {
      /* the peer has no caps (or there is no peer), just use the allowed caps
       * of this sinkpad. */
      GST_DEBUG_OBJECT (pad, "no peer caps, using sinkcaps");
      result = sinkcaps;
    }
    __set_channels (result, 1);
  }

  if (filter != NULL) {
    GstCaps *caps = result;

    GST_LOG_OBJECT (pad, "intersecting filter caps %" GST_PTR_FORMAT " with "
        "preliminary result %" GST_PTR_FORMAT, filter, caps);

    result = gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
  }

  GST_DEBUG_OBJECT (pad, "Returning caps %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_audio_interleave_sink_query (GstAggregator * agg, GstAggregatorPad * aggpad,
    GstQuery * query)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_audio_interleave_sink_getcaps (agg, GST_PAD (aggpad), filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    default:
      res =
          GST_AGGREGATOR_CLASS (parent_class)->sink_query (agg, aggpad, query);
      break;
  }

  return res;
}

static gint
compare_positions (gconstpointer a, gconstpointer b, gpointer user_data)
{
  const gint i = *(const gint *) a;
  const gint j = *(const gint *) b;
  const gint *pos = (const gint *) user_data;

  if (pos[i] < pos[j])
    return -1;
  else if (pos[i] > pos[j])
    return 1;
  else
    return 0;
}

static gboolean
gst_audio_interleave_channel_positions_to_mask (GValueArray * positions,
    gint default_ordering_map[64], guint64 * mask)
{
  gint i;
  guint channels;
  GstAudioChannelPosition *pos;
  gboolean ret;

  channels = positions->n_values;
  pos = g_new (GstAudioChannelPosition, channels);

  for (i = 0; i < channels; i++) {
    GValue *val;

    val = g_value_array_get_nth (positions, i);
    pos[i] = g_value_get_enum (val);
  }

  /* sort the default ordering map according to the position order */
  for (i = 0; i < channels; i++) {
    default_ordering_map[i] = i;
  }
  g_qsort_with_data (default_ordering_map, channels,
      sizeof (*default_ordering_map), compare_positions, pos);

  ret = gst_audio_channel_positions_to_mask (pos, channels, FALSE, mask);
  g_free (pos);

  return ret;
}


/* Must be called with the object lock held */

static guint64
gst_audio_interleave_get_channel_mask (GstAudioInterleave * self)
{
  guint64 channel_mask = 0;

  if (self->channels <= 64 &&
      self->channel_positions != NULL &&
      self->channels == self->channel_positions->n_values) {
    if (!gst_audio_interleave_channel_positions_to_mask
        (self->channel_positions, self->default_channels_ordering_map,
            &channel_mask)) {
      GST_WARNING_OBJECT (self, "Invalid channel positions, using NONE");
      channel_mask = 0;
    }
  } else if (self->channels <= 64) {
    GST_WARNING_OBJECT (self, "Using NONE channel positions");
  }

  return channel_mask;
}


#define MAKE_FUNC(type) \
static void interleave_##type (guint##type *out, guint##type *in, \
    guint stride, guint nframes) \
{ \
  gint i; \
  \
  for (i = 0; i < nframes; i++) { \
    *out = in[i]; \
    out += stride; \
  } \
}

MAKE_FUNC (8);
MAKE_FUNC (16);
MAKE_FUNC (32);
MAKE_FUNC (64);

static void
interleave_24 (guint8 * out, guint8 * in, guint stride, guint nframes)
{
  gint i;

  for (i = 0; i < nframes; i++) {
    memcpy (out, in, 3);
    out += stride * 3;
    in += 3;
  }
}

static void
gst_audio_interleave_set_process_function (GstAudioInterleave * self,
    GstAudioInfo * info)
{
  switch (GST_AUDIO_INFO_WIDTH (info)) {
    case 8:
      self->func = (GstInterleaveFunc) interleave_8;
      break;
    case 16:
      self->func = (GstInterleaveFunc) interleave_16;
      break;
    case 24:
      self->func = (GstInterleaveFunc) interleave_24;
      break;
    case 32:
      self->func = (GstInterleaveFunc) interleave_32;
      break;
    case 64:
      self->func = (GstInterleaveFunc) interleave_64;
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}


/* the first caps we receive on any of the sinkpads will define the caps for all
 * the other sinkpads because we can only mix streams with the same caps.
 */
static gboolean
gst_audio_interleave_setcaps (GstAudioInterleave * self, GstPad * pad,
    GstCaps * caps)
{
  GstAudioAggregator *aagg = GST_AUDIO_AGGREGATOR (self);
  GstAudioInfo info;
  GValue *val;
  guint channel;
  gboolean new = FALSE;

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_format;

  GST_OBJECT_LOCK (self);
  if (self->sinkcaps && !gst_caps_is_subset (caps, self->sinkcaps))
    goto cannot_change_caps;

  if (!self->sinkcaps) {
    GstCaps *sinkcaps = gst_caps_copy (caps);
    GstStructure *s = gst_caps_get_structure (sinkcaps, 0);

    gst_structure_remove_field (s, "channel-mask");

    GST_DEBUG_OBJECT (self, "setting sinkcaps %" GST_PTR_FORMAT, sinkcaps);

    gst_caps_replace (&self->sinkcaps, sinkcaps);
    gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (aagg));

    gst_caps_unref (sinkcaps);
    new = TRUE;
  }

  if (self->channel_positions_from_input
      && GST_AUDIO_INFO_CHANNELS (&info) == 1) {
    channel = GST_AUDIO_INTERLEAVE_PAD (pad)->channel;
    val = g_value_array_get_nth (self->input_channel_positions, channel);
    g_value_set_enum (val, GST_AUDIO_INFO_POSITION (&info, 0));
  }
  GST_OBJECT_UNLOCK (self);

  gst_audio_aggregator_set_sink_caps (aagg, GST_AUDIO_AGGREGATOR_PAD (pad),
      caps);

  if (!new)
    return TRUE;

  GST_INFO_OBJECT (pad, "handle caps change to %" GST_PTR_FORMAT, caps);

  return TRUE;

  /* ERRORS */
invalid_format:
  {
    GST_WARNING_OBJECT (self, "invalid format set as caps: %" GST_PTR_FORMAT,
        caps);
    return FALSE;
  }
cannot_change_caps:
  {
    GST_OBJECT_UNLOCK (self);
    GST_WARNING_OBJECT (self, "caps of %" GST_PTR_FORMAT " already set, can't "
        "change", self->sinkcaps);
    return FALSE;
  }
}

static gboolean
gst_audio_interleave_sink_event (GstAggregator * agg, GstAggregatorPad * aggpad,
    GstEvent * event)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (agg);
  gboolean res = TRUE;

  GST_DEBUG_OBJECT (aggpad, "Got %s event on sink pad",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_audio_interleave_setcaps (self, GST_PAD_CAST (aggpad), caps);
      gst_event_unref (event);
      event = NULL;
      break;
    }
    default:
      break;
  }

  if (event != NULL)
    return GST_AGGREGATOR_CLASS (parent_class)->sink_event (agg, aggpad, event);

  return res;
}

static GstFlowReturn
gst_audio_interleave_update_src_caps (GstAggregator * agg, GstCaps * caps,
    GstCaps ** ret)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (agg);
  GstStructure *s;

  /* This means that either no caps have been set on the sink pad (if
   * sinkcaps is NULL) or that there is no sink pad (if channels == 0).
   */
  GST_OBJECT_LOCK (self);
  if (self->sinkcaps == NULL || self->channels == 0) {
    GST_OBJECT_UNLOCK (self);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  *ret = gst_caps_copy (self->sinkcaps);
  s = gst_caps_get_structure (*ret, 0);

  gst_structure_set (s, "channels", G_TYPE_INT, self->channels, "layout",
      G_TYPE_STRING, "interleaved", "channel-mask", GST_TYPE_BITMASK,
      gst_audio_interleave_get_channel_mask (self), NULL);

  GST_OBJECT_UNLOCK (self);

  return GST_FLOW_OK;
}

static gboolean
gst_audio_interleave_negotiated_src_caps (GstAggregator * agg, GstCaps * caps)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (agg);
  GstAudioAggregatorPad *srcpad = GST_AUDIO_AGGREGATOR_PAD (agg->srcpad);

  if (!GST_AGGREGATOR_CLASS (parent_class)->negotiated_src_caps (agg, caps))
    return FALSE;

  gst_audio_interleave_set_process_function (self, &srcpad->info);

  return TRUE;
}

static void
gst_audio_interleave_class_init (GstAudioInterleaveClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstAggregatorClass *agg_class = (GstAggregatorClass *) klass;
  GstAudioAggregatorClass *aagg_class = (GstAudioAggregatorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "audiointerleave", 0,
      "audio interleaving element");

  gobject_class->set_property = gst_audio_interleave_set_property;
  gobject_class->get_property = gst_audio_interleave_get_property;
  gobject_class->finalize = gst_audio_interleave_finalize;

  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_audio_interleave_src_template, GST_TYPE_AUDIO_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &gst_audio_interleave_sink_template, GST_TYPE_AUDIO_INTERLEAVE_PAD);
  gst_element_class_set_static_metadata (gstelement_class, "AudioInterleave",
      "Generic/Audio", "Mixes multiple audio streams",
      "Olivier Crete <olivier.crete@collabora.com>");

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_audio_interleave_request_new_pad);
  gstelement_class->release_pad =
      GST_DEBUG_FUNCPTR (gst_audio_interleave_release_pad);

  agg_class->sink_query = GST_DEBUG_FUNCPTR (gst_audio_interleave_sink_query);
  agg_class->sink_event = GST_DEBUG_FUNCPTR (gst_audio_interleave_sink_event);
  agg_class->stop = gst_audio_interleave_stop;
  agg_class->update_src_caps = gst_audio_interleave_update_src_caps;
  agg_class->negotiated_src_caps = gst_audio_interleave_negotiated_src_caps;

  aagg_class->aggregate_one_buffer = gst_audio_interleave_aggregate_one_buffer;

  /**
   * GstAudioInterleave:channel-positions:
   *
   * Channel positions: This property controls the channel positions
   * that are used on the src caps. The number of elements should be
   * the same as the number of sink pads and the array should contain
   * a valid list of channel positions. The n-th element of the array
   * is the position of the n-th sink pad.
   *
   * These channel positions will only be used if they're valid and the
   * number of elements is the same as the number of channels. If this
   * is not given a NONE layout will be used.
   *
   */
  g_object_class_install_property (gobject_class, PROP_CHANNEL_POSITIONS,
      g_param_spec_value_array ("channel-positions", "Channel positions",
          "Channel positions used on the output",
          g_param_spec_enum ("channel-position", "Channel position",
              "Channel position of the n-th input",
              GST_TYPE_AUDIO_CHANNEL_POSITION,
              GST_AUDIO_CHANNEL_POSITION_NONE,
              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS),
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstAudioInterleave:channel-positions-from-input:
   *
   * Channel positions from input: If this property is set to %TRUE the channel
   * positions will be taken from the input caps if valid channel positions for
   * the output can be constructed from them. If this is set to %TRUE setting the
   * channel-positions property overwrites this property again.
   *
   */
  g_object_class_install_property (gobject_class,
      PROP_CHANNEL_POSITIONS_FROM_INPUT,
      g_param_spec_boolean ("channel-positions-from-input",
          "Channel positions from input",
          "Take channel positions from the input", TRUE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_AUDIO_INTERLEAVE_PAD, 0);
}

static void
gst_audio_interleave_init (GstAudioInterleave * self)
{
  self->input_channel_positions = g_value_array_new (0);
  self->channel_positions_from_input = TRUE;
  self->channel_positions = self->input_channel_positions;
}

static void
gst_audio_interleave_finalize (GObject * object)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (object);

  if (self->channel_positions
      && self->channel_positions != self->input_channel_positions) {
    g_value_array_free (self->channel_positions);
    self->channel_positions = NULL;
  }

  if (self->input_channel_positions) {
    g_value_array_free (self->input_channel_positions);
    self->input_channel_positions = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_audio_interleave_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (object);

  switch (prop_id) {
    case PROP_CHANNEL_POSITIONS:
      g_return_if_fail (
          ((GValueArray *) g_value_get_boxed (value))->n_values > 0);

      if (self->channel_positions &&
          self->channel_positions != self->input_channel_positions)
        g_value_array_free (self->channel_positions);

      self->channel_positions = g_value_dup_boxed (value);
      self->channel_positions_from_input = FALSE;
      break;
    case PROP_CHANNEL_POSITIONS_FROM_INPUT:
      self->channel_positions_from_input = g_value_get_boolean (value);

      if (self->channel_positions_from_input) {
        if (self->channel_positions &&
            self->channel_positions != self->input_channel_positions)
          g_value_array_free (self->channel_positions);
        self->channel_positions = self->input_channel_positions;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_audio_interleave_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (object);

  switch (prop_id) {
    case PROP_CHANNEL_POSITIONS:
      g_value_set_boxed (value, self->channel_positions);
      break;
    case PROP_CHANNEL_POSITIONS_FROM_INPUT:
      g_value_set_boolean (value, self->channel_positions_from_input);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_audio_interleave_stop (GstAggregator * agg)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (agg);

  if (!GST_AGGREGATOR_CLASS (parent_class)->stop (agg))
    return FALSE;

  gst_caps_replace (&self->sinkcaps, NULL);

  return TRUE;
}

static GstPad *
gst_audio_interleave_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (element);
  GstAudioInterleavePad *newpad;
  gchar *pad_name;
  gint channel, padnumber;
  GValue val = { 0, };

  /* FIXME: We ignore req_name, this is evil! */

  GST_OBJECT_LOCK (self);
  padnumber = g_atomic_int_add (&self->padcounter, 1);
  channel = self->channels++;
  if (!self->channel_positions_from_input)
    channel = padnumber;
  GST_OBJECT_UNLOCK (self);

  pad_name = g_strdup_printf ("sink_%u", padnumber);
  newpad = (GstAudioInterleavePad *)
      GST_ELEMENT_CLASS (parent_class)->request_new_pad (element,
      templ, pad_name, caps);
  g_free (pad_name);
  if (newpad == NULL)
    goto could_not_create;

  newpad->channel = channel;
  gst_pad_use_fixed_caps (GST_PAD (newpad));

  gst_child_proxy_child_added (GST_CHILD_PROXY (element), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));


  g_value_init (&val, GST_TYPE_AUDIO_CHANNEL_POSITION);
  g_value_set_enum (&val, GST_AUDIO_CHANNEL_POSITION_NONE);
  self->input_channel_positions =
      g_value_array_append (self->input_channel_positions, &val);
  g_value_unset (&val);

  /* Update the src caps if we already have them */
  gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (self));

  return GST_PAD_CAST (newpad);

could_not_create:
  {
    GST_DEBUG_OBJECT (element, "could not create/add  pad");
    return NULL;
  }
}

static void
gst_audio_interleave_release_pad (GstElement * element, GstPad * pad)
{
  GstAudioInterleave *self;
  gint position;
  GList *l;

  self = GST_AUDIO_INTERLEAVE (element);

  /* Take lock to make sure we're not changing this when processing buffers */
  GST_OBJECT_LOCK (self);

  self->channels--;

  position = GST_AUDIO_INTERLEAVE_PAD (pad)->channel;
  g_value_array_remove (self->input_channel_positions, position);

  /* Update channel numbers */
  /* Taken above, GST_OBJECT_LOCK (self); */
  for (l = GST_ELEMENT_CAST (self)->sinkpads; l != NULL; l = l->next) {
    GstAudioInterleavePad *ipad = GST_AUDIO_INTERLEAVE_PAD (l->data);

    if (GST_AUDIO_INTERLEAVE_PAD (pad)->channel < ipad->channel)
      ipad->channel--;
  }

  gst_pad_mark_reconfigure (GST_AGGREGATOR_SRC_PAD (self));
  GST_OBJECT_UNLOCK (self);


  GST_DEBUG_OBJECT (self, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (self), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));

  GST_ELEMENT_CLASS (parent_class)->release_pad (element, pad);
}


/* Called with object lock and pad object lock held */
static gboolean
gst_audio_interleave_aggregate_one_buffer (GstAudioAggregator * aagg,
    GstAudioAggregatorPad * aaggpad, GstBuffer * inbuf, guint in_offset,
    GstBuffer * outbuf, guint out_offset, guint num_frames)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (aagg);
  GstAudioInterleavePad *pad = GST_AUDIO_INTERLEAVE_PAD (aaggpad);
  GstMapInfo inmap;
  GstMapInfo outmap;
  gint out_width, in_bpf, out_bpf, out_channels, channel;
  guint8 *outdata;
  GstAggregator *agg = GST_AGGREGATOR (aagg);
  GstAudioAggregatorPad *srcpad = GST_AUDIO_AGGREGATOR_PAD (agg->srcpad);

  GST_OBJECT_LOCK (aagg);
  GST_OBJECT_LOCK (aaggpad);

  out_width = GST_AUDIO_INFO_WIDTH (&srcpad->info) / 8;
  in_bpf = GST_AUDIO_INFO_BPF (&aaggpad->info);
  out_bpf = GST_AUDIO_INFO_BPF (&srcpad->info);
  out_channels = GST_AUDIO_INFO_CHANNELS (&srcpad->info);

  gst_buffer_map (outbuf, &outmap, GST_MAP_READWRITE);
  gst_buffer_map (inbuf, &inmap, GST_MAP_READ);
  GST_LOG_OBJECT (pad, "interleaves %u frames on channel %d/%d at offset %u"
      " from offset %u", num_frames, pad->channel, out_channels,
      out_offset * out_bpf, in_offset * in_bpf);

  if (self->channels > 64) {
    channel = pad->channel;
  } else {
    channel = self->default_channels_ordering_map[pad->channel];
  }

  outdata = outmap.data + (out_offset * out_bpf) + (out_width * channel);


  self->func (outdata, inmap.data + (in_offset * in_bpf), out_channels,
      num_frames);


  gst_buffer_unmap (inbuf, &inmap);
  gst_buffer_unmap (outbuf, &outmap);

  GST_OBJECT_UNLOCK (aaggpad);
  GST_OBJECT_UNLOCK (aagg);

  return TRUE;
}


/* GstChildProxy implementation */
static GObject *
gst_audio_interleave_child_proxy_get_child_by_index (GstChildProxy *
    child_proxy, guint index)
{
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (self);
  obj = g_list_nth_data (GST_ELEMENT_CAST (self)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (self);

  return obj;
}

static guint
gst_audio_interleave_child_proxy_get_children_count (GstChildProxy *
    child_proxy)
{
  guint count = 0;
  GstAudioInterleave *self = GST_AUDIO_INTERLEAVE (child_proxy);

  GST_OBJECT_LOCK (self);
  count = GST_ELEMENT_CAST (self)->numsinkpads;
  GST_OBJECT_UNLOCK (self);
  GST_INFO_OBJECT (self, "Children Count: %d", count);

  return count;
}

static void
gst_audio_interleave_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_INFO ("initializing child proxy interface");
  iface->get_child_by_index =
      gst_audio_interleave_child_proxy_get_child_by_index;
  iface->get_children_count =
      gst_audio_interleave_child_proxy_get_children_count;
}
