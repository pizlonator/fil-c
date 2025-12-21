/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2001 Thomas <thomas@apestaart.org>
 *               2005,2006 Wim Taymans <wim@fluendo.com>
 *
 * adder.c: Adder element, N in, one out, samples are added
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
 * SECTION:element-adder
 * @title: adder
 *
 * The adder allows to mix several streams into one by adding the data.
 * Mixed data is clamped to the min/max values of the data format.
 *
 * The adder currently mixes all data received on the sinkpads as soon as
 * possible without trying to synchronize the streams.
 *
 * Check out the audiomixer element in gst-plugins-bad for a better-behaving
 * audio mixing element: It will sync input streams correctly and also handle
 * live inputs properly.
 *
 * Caps negotiation is inherently racy with the adder element. You can set
 * the "caps" property to force adder to operate in a specific audio
 * format, sample rate and channel count. In this case you may also need
 * audioconvert and/or audioresample elements for each input stream before the
 * adder element to make sure the input branch can produce the forced format.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc freq=100 ! adder name=mix ! audioconvert ! autoaudiosink audiotestsrc freq=500 ! mix.
 * ]|
 *  This pipeline produces two sine waves mixed together.
 *
 */
/* Element-Checklist-Version: 5 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstadder.h"
#include <gst/audio/audio.h>
#include <string.h>             /* strcmp */
#include "gstadderorc.h"

#define GST_CAT_DEFAULT gst_adder_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define DEFAULT_PAD_VOLUME (1.0)
#define DEFAULT_PAD_MUTE (FALSE)

/* some defines for audio processing */
/* the volume factor is a range from 0.0 to (arbitrary) VOLUME_MAX_DOUBLE = 10.0
 * we map 1.0 to VOLUME_UNITY_INT*
 */
#define VOLUME_UNITY_INT8            8  /* internal int for unity 2^(8-5) */
#define VOLUME_UNITY_INT8_BIT_SHIFT  3  /* number of bits to shift for unity */
#define VOLUME_UNITY_INT16           2048       /* internal int for unity 2^(16-5) */
#define VOLUME_UNITY_INT16_BIT_SHIFT 11 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT24           524288     /* internal int for unity 2^(24-5) */
#define VOLUME_UNITY_INT24_BIT_SHIFT 19 /* number of bits to shift for unity */
#define VOLUME_UNITY_INT32           134217728  /* internal int for unity 2^(32-5) */
#define VOLUME_UNITY_INT32_BIT_SHIFT 27

enum
{
  PROP_PAD_0,
  PROP_PAD_VOLUME,
  PROP_PAD_MUTE
};

G_DEFINE_TYPE (GstAdderPad, gst_adder_pad, GST_TYPE_PAD);

static void
gst_adder_pad_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAdderPad *pad = GST_ADDER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_VOLUME:
      g_value_set_double (value, pad->volume);
      break;
    case PROP_PAD_MUTE:
      g_value_set_boolean (value, pad->mute);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_adder_pad_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAdderPad *pad = GST_ADDER_PAD (object);

  switch (prop_id) {
    case PROP_PAD_VOLUME:
      GST_OBJECT_LOCK (pad);
      pad->volume = g_value_get_double (value);
      pad->volume_i8 = pad->volume * VOLUME_UNITY_INT8;
      pad->volume_i16 = pad->volume * VOLUME_UNITY_INT16;
      pad->volume_i32 = pad->volume * VOLUME_UNITY_INT32;
      GST_OBJECT_UNLOCK (pad);
      break;
    case PROP_PAD_MUTE:
      GST_OBJECT_LOCK (pad);
      pad->mute = g_value_get_boolean (value);
      GST_OBJECT_UNLOCK (pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_adder_pad_class_init (GstAdderPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_adder_pad_set_property;
  gobject_class->get_property = gst_adder_pad_get_property;

  g_object_class_install_property (gobject_class, PROP_PAD_VOLUME,
      g_param_spec_double ("volume", "Volume", "Volume of this pad",
          0.0, 10.0, DEFAULT_PAD_VOLUME,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PAD_MUTE,
      g_param_spec_boolean ("mute", "Mute", "Mute this pad",
          DEFAULT_PAD_MUTE,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS));
}

static void
gst_adder_pad_init (GstAdderPad * pad)
{
  pad->volume = DEFAULT_PAD_VOLUME;
  pad->mute = DEFAULT_PAD_MUTE;
}

enum
{
  PROP_0,
  PROP_FILTER_CAPS
};

/* elementfactory information */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32LE, U32LE, S16LE, U16LE, S8, U8, F32LE, F64LE }") \
  ", layout = (string) { interleaved }"
#else
#define CAPS \
  GST_AUDIO_CAPS_MAKE ("{ S32BE, U32BE, S16BE, U16BE, S8, U8, F32BE, F64BE }") \
  ", layout = (string) { interleaved }"
#endif

static GstStaticPadTemplate gst_adder_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS)
    );

static GstStaticPadTemplate gst_adder_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS (CAPS)
    );

static void gst_adder_child_proxy_init (gpointer g_iface, gpointer iface_data);

#define gst_adder_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAdder, gst_adder, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_CHILD_PROXY, gst_adder_child_proxy_init));
GST_ELEMENT_REGISTER_DEFINE (adder, "adder", GST_RANK_NONE, GST_TYPE_ADDER);

static void gst_adder_dispose (GObject * object);
static void gst_adder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_adder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_adder_setcaps (GstAdder * adder, GstPad * pad,
    GstCaps * caps);
static gboolean gst_adder_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_adder_sink_query (GstCollectPads * pads,
    GstCollectData * pad, GstQuery * query, gpointer user_data);
static gboolean gst_adder_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_adder_sink_event (GstCollectPads * pads,
    GstCollectData * pad, GstEvent * event, gpointer user_data);

static GstPad *gst_adder_request_new_pad (GstElement * element,
    GstPadTemplate * temp, const gchar * unused, const GstCaps * caps);
static void gst_adder_release_pad (GstElement * element, GstPad * pad);

static GstStateChangeReturn gst_adder_change_state (GstElement * element,
    GstStateChange transition);

static GstFlowReturn gst_adder_do_clip (GstCollectPads * pads,
    GstCollectData * data, GstBuffer * buffer, GstBuffer ** out,
    gpointer user_data);
static GstFlowReturn gst_adder_collected (GstCollectPads * pads,
    gpointer user_data);

/* we can only accept caps that we and downstream can handle.
 * if we have filtercaps set, use those to constrain the target caps.
 */
static GstCaps *
gst_adder_sink_getcaps (GstPad * pad, GstCaps * filter)
{
  GstAdder *adder;
  GstCaps *result, *peercaps, *current_caps, *filter_caps;
  GstStructure *s;
  gint i, n;

  adder = GST_ADDER (GST_PAD_PARENT (pad));

  GST_OBJECT_LOCK (adder);
  /* take filter */
  if ((filter_caps = adder->filter_caps)) {
    if (filter)
      filter_caps =
          gst_caps_intersect_full (filter, filter_caps,
          GST_CAPS_INTERSECT_FIRST);
    else
      gst_caps_ref (filter_caps);
  } else {
    filter_caps = filter ? gst_caps_ref (filter) : NULL;
  }
  GST_OBJECT_UNLOCK (adder);

  if (filter_caps && gst_caps_is_empty (filter_caps)) {
    GST_WARNING_OBJECT (pad, "Empty filter caps");
    return filter_caps;
  }

  /* get the downstream possible caps */
  peercaps = gst_pad_peer_query_caps (adder->srcpad, filter_caps);

  /* get the allowed caps on this sinkpad */
  GST_OBJECT_LOCK (adder);
  current_caps =
      adder->current_caps ? gst_caps_ref (adder->current_caps) : NULL;
  if (current_caps == NULL) {
    current_caps = gst_pad_get_pad_template_caps (pad);
    if (!current_caps)
      current_caps = gst_caps_new_any ();
  }
  GST_OBJECT_UNLOCK (adder);

  if (peercaps) {
    /* if the peer has caps, intersect */
    GST_DEBUG_OBJECT (adder, "intersecting peer and our caps");
    result =
        gst_caps_intersect_full (peercaps, current_caps,
        GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (peercaps);
    gst_caps_unref (current_caps);
  } else {
    /* the peer has no caps (or there is no peer), just use the allowed caps
     * of this sinkpad. */
    /* restrict with filter-caps if any */
    if (filter_caps) {
      GST_DEBUG_OBJECT (adder, "no peer caps, using filtered caps");
      result =
          gst_caps_intersect_full (filter_caps, current_caps,
          GST_CAPS_INTERSECT_FIRST);
      gst_caps_unref (current_caps);
    } else {
      GST_DEBUG_OBJECT (adder, "no peer caps, using our caps");
      result = current_caps;
    }
  }

  result = gst_caps_make_writable (result);

  n = gst_caps_get_size (result);
  for (i = 0; i < n; i++) {
    GstStructure *sref;

    s = gst_caps_get_structure (result, i);
    sref = gst_structure_copy (s);
    gst_structure_set (sref, "channels", GST_TYPE_INT_RANGE, 0, 2, NULL);
    if (gst_structure_is_subset (s, sref)) {
      /* This field is irrelevant when in mono or stereo */
      gst_structure_remove_field (s, "channel-mask");
    }
    gst_structure_free (sref);
  }

  if (filter_caps)
    gst_caps_unref (filter_caps);

  GST_LOG_OBJECT (adder, "getting caps on pad %p,%s to %" GST_PTR_FORMAT, pad,
      GST_PAD_NAME (pad), result);

  return result;
}

static gboolean
gst_adder_sink_query (GstCollectPads * pads, GstCollectData * pad,
    GstQuery * query, gpointer user_data)
{
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_adder_sink_getcaps (pad->pad, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    default:
      res = gst_collect_pads_query_default (pads, pad, query, FALSE);
      break;
  }

  return res;
}

/* the first caps we receive on any of the sinkpads will define the caps for all
 * the other sinkpads because we can only mix streams with the same caps.
 */
static gboolean
gst_adder_setcaps (GstAdder * adder, GstPad * pad, GstCaps * orig_caps)
{
  GstCaps *caps;
  GstAudioInfo info;
  GstStructure *s;
  gint channels;

  caps = gst_caps_copy (orig_caps);

  s = gst_caps_get_structure (caps, 0);
  if (gst_structure_get_int (s, "channels", &channels))
    if (channels <= 2)
      gst_structure_remove_field (s, "channel-mask");

  if (!gst_audio_info_from_caps (&info, caps))
    goto invalid_format;

  GST_OBJECT_LOCK (adder);
  /* don't allow reconfiguration for now; there's still a race between the
   * different upstream threads doing query_caps + accept_caps + sending
   * (possibly different) CAPS events, but there's not much we can do about
   * that, upstream needs to deal with it. */
  if (adder->current_caps != NULL) {
    if (gst_audio_info_is_equal (&info, &adder->info)) {
      GST_OBJECT_UNLOCK (adder);
      gst_caps_unref (caps);
      return TRUE;
    } else {
      GST_DEBUG_OBJECT (pad, "got input caps %" GST_PTR_FORMAT ", but "
          "current caps are %" GST_PTR_FORMAT, caps, adder->current_caps);
      GST_OBJECT_UNLOCK (adder);
      gst_pad_push_event (pad, gst_event_new_reconfigure ());
      gst_caps_unref (caps);
      return FALSE;
    }
  }

  GST_INFO_OBJECT (pad, "setting caps to %" GST_PTR_FORMAT, caps);
  adder->current_caps = gst_caps_ref (caps);

  memcpy (&adder->info, &info, sizeof (info));
  GST_OBJECT_UNLOCK (adder);
  /* send caps event later, after stream-start event */

  GST_INFO_OBJECT (pad, "handle caps change to %" GST_PTR_FORMAT, caps);

  gst_caps_unref (caps);

  return TRUE;

  /* ERRORS */
invalid_format:
  {
    gst_caps_unref (caps);
    GST_WARNING_OBJECT (adder, "invalid format set as caps");
    return FALSE;
  }
}

/* FIXME, the duration query should reflect how long you will produce
 * data, that is the amount of stream time until you will emit EOS.
 *
 * For synchronized mixing this is always the max of all the durations
 * of upstream since we emit EOS when all of them finished.
 *
 * We don't do synchronized mixing so this really depends on where the
 * streams where punched in and what their relative offsets are against
 * each other which we can get from the first timestamps we see.
 *
 * When we add a new stream (or remove a stream) the duration might
 * also become invalid again and we need to post a new DURATION
 * message to notify this fact to the parent.
 * For now we take the max of all the upstream elements so the simple
 * cases work at least somewhat.
 */
static gboolean
gst_adder_query_duration (GstAdder * adder, GstQuery * query)
{
  gint64 max;
  gboolean res;
  GstFormat format;
  GstIterator *it;
  gboolean done;
  GValue item = { 0, };

  /* parse format */
  gst_query_parse_duration (query, &format, NULL);

  max = -1;
  res = TRUE;
  done = FALSE;

  it = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (adder));
  while (!done) {
    GstIteratorResult ires;

    ires = gst_iterator_next (it, &item);
    switch (ires) {
      case GST_ITERATOR_DONE:
        done = TRUE;
        break;
      case GST_ITERATOR_OK:
      {
        GstPad *pad = g_value_get_object (&item);
        gint64 duration;

        /* ask sink peer for duration */
        res &= gst_pad_peer_query_duration (pad, format, &duration);
        /* take max from all valid return values */
        if (res) {
          /* valid unknown length, stop searching */
          if (duration == -1) {
            max = duration;
            done = TRUE;
          }
          /* else see if bigger than current max */
          else if (duration > max)
            max = duration;
        }
        g_value_reset (&item);
        break;
      }
      case GST_ITERATOR_RESYNC:
        max = -1;
        res = TRUE;
        gst_iterator_resync (it);
        break;
      default:
        res = FALSE;
        done = TRUE;
        break;
    }
  }
  g_value_unset (&item);
  gst_iterator_free (it);

  if (res) {
    /* and store the max */
    GST_DEBUG_OBJECT (adder, "Total duration in format %s: %"
        GST_TIME_FORMAT, gst_format_get_name (format), GST_TIME_ARGS (max));
    gst_query_set_duration (query, format, max);
  }

  return res;
}

static gboolean
gst_adder_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstAdder *adder = GST_ADDER (parent);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;

      gst_query_parse_position (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_TIME:
          /* FIXME, bring to stream time, might be tricky */
          gst_query_set_position (query, format, adder->segment.position);
          res = TRUE;
          break;
        case GST_FORMAT_DEFAULT:
          gst_query_set_position (query, format, adder->offset);
          res = TRUE;
          break;
        default:
          break;
      }
      break;
    }
    case GST_QUERY_DURATION:
      res = gst_adder_query_duration (adder, query);
      break;
    default:
      /* FIXME, needs a custom query handler because we have multiple
       * sinkpads */
      res = gst_pad_query_default (pad, parent, query);
      break;
  }

  return res;
}

/* event handling */

typedef struct
{
  GstEvent *event;
  gboolean flush;
} EventData;

static gboolean
forward_event_func (const GValue * val, GValue * ret, EventData * data)
{
  GstPad *pad = g_value_get_object (val);
  GstEvent *event = data->event;
  GstPad *peer;

  gst_event_ref (event);
  GST_LOG_OBJECT (pad, "About to send event %s", GST_EVENT_TYPE_NAME (event));
  peer = gst_pad_get_peer (pad);
  /* collect pad might have been set flushing,
   * so bypass core checking that and send directly to peer */
  if (!peer || !gst_pad_send_event (peer, event)) {
    if (!peer)
      gst_event_unref (event);
    GST_WARNING_OBJECT (pad, "Sending event  %p (%s) failed.",
        event, GST_EVENT_TYPE_NAME (event));
    /* quick hack to unflush the pads, ideally we need a way to just unflush
     * this single collect pad */
    if (data->flush)
      gst_pad_send_event (pad, gst_event_new_flush_stop (TRUE));
  } else {
    g_value_set_boolean (ret, TRUE);
    GST_LOG_OBJECT (pad, "Sent event  %p (%s).",
        event, GST_EVENT_TYPE_NAME (event));
  }
  if (peer)
    gst_object_unref (peer);

  /* continue on other pads, even if one failed */
  return TRUE;
}

/* forwards the event to all sinkpads, takes ownership of the
 * event
 *
 * Returns: TRUE if the event could be forwarded on all
 * sinkpads.
 */
static gboolean
forward_event (GstAdder * adder, GstEvent * event, gboolean flush)
{
  gboolean ret;
  GstIterator *it;
  GstIteratorResult ires;
  GValue vret = { 0 };
  EventData data;

  GST_LOG_OBJECT (adder, "Forwarding event %p (%s)", event,
      GST_EVENT_TYPE_NAME (event));

  data.event = event;
  data.flush = flush;

  g_value_init (&vret, G_TYPE_BOOLEAN);
  g_value_set_boolean (&vret, FALSE);
  it = gst_element_iterate_sink_pads (GST_ELEMENT_CAST (adder));
  while (TRUE) {
    ires =
        gst_iterator_fold (it, (GstIteratorFoldFunction) forward_event_func,
        &vret, &data);
    switch (ires) {
      case GST_ITERATOR_RESYNC:
        GST_WARNING ("resync");
        gst_iterator_resync (it);
        g_value_set_boolean (&vret, TRUE);
        break;
      case GST_ITERATOR_OK:
      case GST_ITERATOR_DONE:
        ret = g_value_get_boolean (&vret);
        goto done;
      default:
        ret = FALSE;
        goto done;
    }
  }
done:
  gst_iterator_free (it);
  GST_LOG_OBJECT (adder, "Forwarded event %p (%s), ret=%d", event,
      GST_EVENT_TYPE_NAME (event), ret);
  gst_event_unref (event);

  return ret;
}

static gboolean
gst_adder_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAdder *adder;
  gboolean result;

  adder = GST_ADDER (parent);

  GST_DEBUG_OBJECT (pad, "Got %s event on src pad: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
    {
      GstSeekFlags flags;
      gdouble rate;
      GstSeekType start_type, stop_type;
      gint64 start, stop;
      GstFormat seek_format, dest_format;
      gboolean flush;

      /* parse the seek parameters */
      gst_event_parse_seek (event, &rate, &seek_format, &flags, &start_type,
          &start, &stop_type, &stop);

      if ((start_type != GST_SEEK_TYPE_NONE)
          && (start_type != GST_SEEK_TYPE_SET)) {
        result = FALSE;
        GST_DEBUG_OBJECT (adder,
            "seeking failed, unhandled seek type for start: %d", start_type);
        goto done;
      }
      if ((stop_type != GST_SEEK_TYPE_NONE) && (stop_type != GST_SEEK_TYPE_SET)) {
        result = FALSE;
        GST_DEBUG_OBJECT (adder,
            "seeking failed, unhandled seek type for end: %d", stop_type);
        goto done;
      }

      dest_format = adder->segment.format;
      if (seek_format != dest_format) {
        result = FALSE;
        GST_DEBUG_OBJECT (adder,
            "seeking failed, unhandled seek format: %d", seek_format);
        goto done;
      }

      flush = (flags & GST_SEEK_FLAG_FLUSH) == GST_SEEK_FLAG_FLUSH;

      /* check if we are flushing */
      if (flush) {
        /* flushing seek, start flush downstream, the flush will be done
         * when all pads received a FLUSH_STOP.
         * Make sure we accept nothing anymore and return WRONG_STATE.
         * We send a flush-start before, to ensure no streaming is done
         * as we need to take the stream lock.
         */
        gst_pad_push_event (adder->srcpad, gst_event_new_flush_start ());
        gst_collect_pads_set_flushing (adder->collect, TRUE);

        /* We can't send FLUSH_STOP here since upstream could start pushing data
         * after we unlock adder->collect.
         * We set flush_stop_pending to TRUE instead and send FLUSH_STOP after
         * forwarding the seek upstream or from gst_adder_collected,
         * whichever happens first.
         */
        GST_COLLECT_PADS_STREAM_LOCK (adder->collect);
        adder->flush_stop_pending = TRUE;
        GST_COLLECT_PADS_STREAM_UNLOCK (adder->collect);
        GST_DEBUG_OBJECT (adder, "mark pending flush stop event");
      }
      GST_DEBUG_OBJECT (adder, "handling seek event: %" GST_PTR_FORMAT, event);

      /* now wait for the collected to be finished and mark a new
       * segment. After we have the lock, no collect function is running and no
       * new collect function will be called for as long as we're flushing. */
      GST_COLLECT_PADS_STREAM_LOCK (adder->collect);

      /* clip position and update our segment */
      if (adder->segment.stop != -1) {
        adder->segment.position = adder->segment.stop;
      }
      gst_segment_do_seek (&adder->segment, rate, seek_format, flags,
          start_type, start, stop_type, stop, NULL);

      if (flush) {
        /* Yes, we need to call _set_flushing again *WHEN* the streaming threads
         * have stopped so that the cookie gets properly updated. */
        gst_collect_pads_set_flushing (adder->collect, TRUE);
      }
      GST_COLLECT_PADS_STREAM_UNLOCK (adder->collect);
      GST_DEBUG_OBJECT (adder, "forwarding seek event: %" GST_PTR_FORMAT,
          event);
      GST_DEBUG_OBJECT (adder, "updated segment: %" GST_SEGMENT_FORMAT,
          &adder->segment);

      /* we're forwarding seek to all upstream peers and wait for one to reply
       * with a newsegment-event before we send a newsegment-event downstream */
      g_atomic_int_set (&adder->new_segment_pending, TRUE);
      result = forward_event (adder, event, flush);
      if (!result) {
        /* seek failed. maybe source is a live source. */
        GST_DEBUG_OBJECT (adder, "seeking failed");
      }
      if (g_atomic_int_compare_and_exchange (&adder->flush_stop_pending,
              TRUE, FALSE)) {
        GST_DEBUG_OBJECT (adder, "pending flush stop");
        if (!gst_pad_push_event (adder->srcpad,
                gst_event_new_flush_stop (TRUE))) {
          GST_WARNING_OBJECT (adder, "Sending flush stop event failed");
        }
      }
      break;
    }
    case GST_EVENT_QOS:
      /* QoS might be tricky */
      result = FALSE;
      gst_event_unref (event);
      break;
    case GST_EVENT_NAVIGATION:
      /* navigation is rather pointless. */
      result = FALSE;
      gst_event_unref (event);
      break;
    default:
      /* just forward the rest for now */
      GST_DEBUG_OBJECT (adder, "forward unhandled event: %s",
          GST_EVENT_TYPE_NAME (event));
      result = forward_event (adder, event, FALSE);
      break;
  }

done:

  return result;
}

static gboolean
gst_adder_sink_event (GstCollectPads * pads, GstCollectData * pad,
    GstEvent * event, gpointer user_data)
{
  GstAdder *adder = GST_ADDER (user_data);
  gboolean res = TRUE, discard = FALSE;

  GST_DEBUG_OBJECT (pad->pad, "Got %s event on sink pad",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_adder_setcaps (adder, pad->pad, caps);
      gst_event_unref (event);
      event = NULL;
      break;
    }
    case GST_EVENT_FLUSH_START:
      /* ensure that we will send a flush stop */
      res = gst_collect_pads_event_default (pads, pad, event, discard);
      event = NULL;
      GST_COLLECT_PADS_STREAM_LOCK (adder->collect);
      adder->flush_stop_pending = TRUE;
      GST_COLLECT_PADS_STREAM_UNLOCK (adder->collect);
      break;
    case GST_EVENT_FLUSH_STOP:
      /* we received a flush-stop. We will only forward it when
       * flush_stop_pending is set, and we will unset it then.
       */
      g_atomic_int_set (&adder->new_segment_pending, TRUE);
      GST_COLLECT_PADS_STREAM_LOCK (adder->collect);
      if (adder->flush_stop_pending) {
        GST_DEBUG_OBJECT (pad->pad, "forwarding flush stop");
        res = gst_collect_pads_event_default (pads, pad, event, discard);
        adder->flush_stop_pending = FALSE;
        event = NULL;
      } else {
        discard = TRUE;
        GST_DEBUG_OBJECT (pad->pad, "eating flush stop");
      }
      GST_COLLECT_PADS_STREAM_UNLOCK (adder->collect);
      /* Clear pending tags */
      if (adder->pending_events) {
        g_list_foreach (adder->pending_events, (GFunc) gst_event_unref, NULL);
        g_list_free (adder->pending_events);
        adder->pending_events = NULL;
      }
      break;
    case GST_EVENT_TAG:
      /* collect tags here so we can push them out when we collect data */
      adder->pending_events = g_list_append (adder->pending_events, event);
      event = NULL;
      break;
    case GST_EVENT_SEGMENT:{
      const GstSegment *segment;
      gst_event_parse_segment (event, &segment);
      if (segment->rate != adder->segment.rate) {
        GST_ERROR_OBJECT (pad->pad,
            "Got segment event with wrong rate %lf, expected %lf",
            segment->rate, adder->segment.rate);
        res = FALSE;
        gst_event_unref (event);
        event = NULL;
      }
      discard = TRUE;
      break;
    }
    default:
      break;
  }

  if (G_LIKELY (event))
    return gst_collect_pads_event_default (pads, pad, event, discard);
  else
    return res;
}

static void
gst_adder_class_init (GstAdderClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_adder_set_property;
  gobject_class->get_property = gst_adder_get_property;
  gobject_class->dispose = gst_adder_dispose;

  g_object_class_install_property (gobject_class, PROP_FILTER_CAPS,
      g_param_spec_boxed ("caps", "Target caps",
          "Set target format for mixing (NULL means ANY). "
          "Setting this property takes a reference to the supplied GstCaps "
          "object.", GST_TYPE_CAPS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_adder_src_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_adder_sink_template);
  gst_element_class_set_static_metadata (gstelement_class, "Adder",
      "Generic/Audio", "Add N audio channels together",
      "Thomas Vander Stichele <thomas at apestaart dot org>");

  gstelement_class->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_adder_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_adder_release_pad);
  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_adder_change_state);
}

static void
gst_adder_init (GstAdder * adder)
{
  GstPadTemplate *template;

  template = gst_static_pad_template_get (&gst_adder_src_template);
  adder->srcpad = gst_pad_new_from_template (template, "src");
  gst_object_unref (template);

  gst_pad_set_query_function (adder->srcpad,
      GST_DEBUG_FUNCPTR (gst_adder_src_query));
  gst_pad_set_event_function (adder->srcpad,
      GST_DEBUG_FUNCPTR (gst_adder_src_event));
  GST_PAD_SET_PROXY_CAPS (adder->srcpad);
  gst_element_add_pad (GST_ELEMENT (adder), adder->srcpad);

  adder->current_caps = NULL;
  gst_audio_info_init (&adder->info);
  adder->padcount = 0;

  adder->filter_caps = NULL;

  /* keep track of the sinkpads requested */
  adder->collect = gst_collect_pads_new ();
  gst_collect_pads_set_function (adder->collect,
      GST_DEBUG_FUNCPTR (gst_adder_collected), adder);
  gst_collect_pads_set_clip_function (adder->collect,
      GST_DEBUG_FUNCPTR (gst_adder_do_clip), adder);
  gst_collect_pads_set_event_function (adder->collect,
      GST_DEBUG_FUNCPTR (gst_adder_sink_event), adder);
  gst_collect_pads_set_query_function (adder->collect,
      GST_DEBUG_FUNCPTR (gst_adder_sink_query), adder);
}

static void
gst_adder_dispose (GObject * object)
{
  GstAdder *adder = GST_ADDER (object);

  if (adder->collect) {
    gst_object_unref (adder->collect);
    adder->collect = NULL;
  }
  gst_caps_replace (&adder->filter_caps, NULL);
  gst_caps_replace (&adder->current_caps, NULL);

  if (adder->pending_events) {
    g_list_foreach (adder->pending_events, (GFunc) gst_event_unref, NULL);
    g_list_free (adder->pending_events);
    adder->pending_events = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_adder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAdder *adder = GST_ADDER (object);

  switch (prop_id) {
    case PROP_FILTER_CAPS:{
      GstCaps *new_caps = NULL;
      GstCaps *old_caps;
      const GstCaps *new_caps_val = gst_value_get_caps (value);

      if (new_caps_val != NULL) {
        new_caps = (GstCaps *) new_caps_val;
        gst_caps_ref (new_caps);
      }

      GST_OBJECT_LOCK (adder);
      old_caps = adder->filter_caps;
      adder->filter_caps = new_caps;
      GST_OBJECT_UNLOCK (adder);

      if (old_caps)
        gst_caps_unref (old_caps);

      GST_DEBUG_OBJECT (adder, "set new caps %" GST_PTR_FORMAT, new_caps);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_adder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAdder *adder = GST_ADDER (object);

  switch (prop_id) {
    case PROP_FILTER_CAPS:
      GST_OBJECT_LOCK (adder);
      gst_value_set_caps (value, adder->filter_caps);
      GST_OBJECT_UNLOCK (adder);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static GstPad *
gst_adder_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * unused, const GstCaps * caps)
{
  gchar *name;
  GstAdder *adder;
  GstPad *newpad;
  gint padcount;

  if (templ->direction != GST_PAD_SINK)
    goto not_sink;

  adder = GST_ADDER (element);

  /* increment pad counter */
  padcount = g_atomic_int_add (&adder->padcount, 1);

  name = g_strdup_printf ("sink_%u", padcount);
  newpad = g_object_new (GST_TYPE_ADDER_PAD, "name", name, "direction",
      templ->direction, "template", templ, NULL);
  GST_DEBUG_OBJECT (adder, "request new pad %s", name);
  g_free (name);

  gst_collect_pads_add_pad (adder->collect, newpad, sizeof (GstCollectData),
      NULL, TRUE);

  /* takes ownership of the pad */
  if (!gst_element_add_pad (GST_ELEMENT (adder), newpad))
    goto could_not_add;

  gst_child_proxy_child_added (GST_CHILD_PROXY (adder), G_OBJECT (newpad),
      GST_OBJECT_NAME (newpad));

  return newpad;

  /* errors */
not_sink:
  {
    g_warning ("gstadder: request new pad that is not a SINK pad\n");
    return NULL;
  }
could_not_add:
  {
    GST_DEBUG_OBJECT (adder, "could not add pad");
    gst_collect_pads_remove_pad (adder->collect, newpad);
    gst_object_unref (newpad);
    return NULL;
  }
}

static void
gst_adder_release_pad (GstElement * element, GstPad * pad)
{
  GstAdder *adder;

  adder = GST_ADDER (element);

  GST_DEBUG_OBJECT (adder, "release pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  gst_child_proxy_child_removed (GST_CHILD_PROXY (adder), G_OBJECT (pad),
      GST_OBJECT_NAME (pad));
  if (adder->collect)
    gst_collect_pads_remove_pad (adder->collect, pad);
  gst_element_remove_pad (element, pad);
}

static GstFlowReturn
gst_adder_do_clip (GstCollectPads * pads, GstCollectData * data,
    GstBuffer * buffer, GstBuffer ** out, gpointer user_data)
{
  GstAdder *adder = GST_ADDER (user_data);
  gint rate, bpf;

  rate = GST_AUDIO_INFO_RATE (&adder->info);
  bpf = GST_AUDIO_INFO_BPF (&adder->info);

  buffer = gst_audio_buffer_clip (buffer, &data->segment, rate, bpf);

  *out = buffer;
  return GST_FLOW_OK;
}

/*
 * gst_adder_collected:
 *
 * Combine audio streams by adding data values.
 * basic algorithm :
 * - this function is called when all pads have a buffer
 * - get available bytes on all pads.
 * - repeat for each input pad :
 *   - read available bytes, copy or add to target buffer
 *   - if there's an EOS event, remove the input channel
 * - push out the output buffer
 *
 * Note: this code will run in one of the upstream threads.
 *
 * TODO: it would be nice to have a mixing mode, instead of only adding
 * - for float we could downscale after collect loop
 * - for int we need to downscale each input to avoid clipping or
 *   mix into a temp (float) buffer and scale afterwards as well
 */
static GstFlowReturn
gst_adder_collected (GstCollectPads * pads, gpointer user_data)
{
  GstAdder *adder;
  GSList *collected, *next = NULL;
  GstFlowReturn ret;
  GstBuffer *outbuf = NULL, *gapbuf = NULL;
  GstMapInfo outmap = { NULL };
  guint outsize;
  gint64 next_offset;
  gint64 next_timestamp;
  gint rate, bps, bpf;
  gboolean had_mute = FALSE;
  gboolean is_eos = TRUE;
  gboolean is_discont = FALSE;

  adder = GST_ADDER (user_data);

  /* this is fatal */
  if (G_UNLIKELY (adder->info.finfo->format == GST_AUDIO_FORMAT_UNKNOWN))
    goto not_negotiated;

  if (adder->flush_stop_pending) {
    GST_INFO_OBJECT (adder->srcpad, "send pending flush stop event");
    if (!gst_pad_push_event (adder->srcpad, gst_event_new_flush_stop (TRUE))) {
      GST_WARNING_OBJECT (adder->srcpad, "Sending flush stop event failed");
    }

    adder->flush_stop_pending = FALSE;
  }

  if (adder->send_stream_start) {
    gchar s_id[32];
    GstEvent *event;

    GST_INFO_OBJECT (adder->srcpad, "send pending stream start event");
    /* FIXME: create id based on input ids, we can't use
     * gst_pad_create_stream_id() though as that only handles 0..1 sink-pad
     */
    g_snprintf (s_id, sizeof (s_id), "adder-%08x", g_random_int ());
    event = gst_event_new_stream_start (s_id);
    gst_event_set_group_id (event, gst_util_group_id_next ());

    if (!gst_pad_push_event (adder->srcpad, event)) {
      GST_WARNING_OBJECT (adder->srcpad, "Sending stream start event failed");
    }
    adder->send_stream_start = FALSE;
  }

  if (adder->send_caps) {
    GstEvent *caps_event;

    caps_event = gst_event_new_caps (adder->current_caps);
    GST_INFO_OBJECT (adder->srcpad, "send pending caps event %" GST_PTR_FORMAT,
        caps_event);
    if (!gst_pad_push_event (adder->srcpad, caps_event)) {
      GST_WARNING_OBJECT (adder->srcpad, "Sending caps event failed");
    }
    adder->send_caps = FALSE;
  }

  rate = GST_AUDIO_INFO_RATE (&adder->info);
  bps = GST_AUDIO_INFO_BPS (&adder->info);
  bpf = GST_AUDIO_INFO_BPF (&adder->info);

  if (g_atomic_int_compare_and_exchange (&adder->new_segment_pending, TRUE,
          FALSE)) {
    GstEvent *event;

    /*
     * When seeking we set the start and stop positions as given in the seek
     * event. We also adjust offset & timestamp accordingly.
     * This basically ignores all newsegments sent by upstream.
     */
    event = gst_event_new_segment (&adder->segment);
    if (adder->segment.rate > 0.0) {
      adder->segment.position = adder->segment.start;
    } else {
      adder->segment.position = adder->segment.stop;
    }
    adder->offset = gst_util_uint64_scale (adder->segment.position,
        rate, GST_SECOND);

    GST_INFO_OBJECT (adder->srcpad, "sending pending new segment event %"
        GST_SEGMENT_FORMAT, &adder->segment);
    if (event) {
      if (!gst_pad_push_event (adder->srcpad, event)) {
        GST_WARNING_OBJECT (adder->srcpad, "Sending new segment event failed");
      }
    } else {
      GST_WARNING_OBJECT (adder->srcpad, "Creating new segment event for "
          "start:%" G_GINT64_FORMAT ", end:%" G_GINT64_FORMAT " failed",
          adder->segment.start, adder->segment.stop);
    }
    is_discont = TRUE;
  }

  /* get available bytes for reading, this can be 0 which could mean empty
   * buffers or EOS, which we will catch when we loop over the pads. */
  outsize = gst_collect_pads_available (pads);

  GST_LOG_OBJECT (adder,
      "starting to cycle through channels, %d bytes available (bps = %d, bpf = %d)",
      outsize, bps, bpf);

  for (collected = pads->data; collected; collected = next) {
    GstCollectData *collect_data;
    GstBuffer *inbuf;
    gboolean is_gap;
    GstAdderPad *pad;
    GstClockTime timestamp, stream_time;

    /* take next to see if this is the last collectdata */
    next = g_slist_next (collected);

    collect_data = (GstCollectData *) collected->data;
    pad = GST_ADDER_PAD (collect_data->pad);

    /* get a buffer of size bytes, if we get a buffer, it is at least outsize
     * bytes big. */
    inbuf = gst_collect_pads_take_buffer (pads, collect_data, outsize);

    if (!GST_COLLECT_PADS_STATE_IS_SET (collect_data,
            GST_COLLECT_PADS_STATE_EOS))
      is_eos = FALSE;

    /* NULL means EOS or an empty buffer so we still need to flush in
     * case of an empty buffer. */
    if (inbuf == NULL) {
      GST_LOG_OBJECT (adder, "channel %p: no bytes available", collect_data);
      continue;
    }

    timestamp = GST_BUFFER_TIMESTAMP (inbuf);
    stream_time =
        gst_segment_to_stream_time (&collect_data->segment, GST_FORMAT_TIME,
        timestamp);

    /* sync object properties on stream time */
    if (GST_CLOCK_TIME_IS_VALID (stream_time))
      gst_object_sync_values (GST_OBJECT (pad), stream_time);

    GST_OBJECT_LOCK (pad);
    if (pad->mute || pad->volume < G_MINDOUBLE) {
      had_mute = TRUE;
      GST_DEBUG_OBJECT (adder, "channel %p: skipping muted pad", collect_data);
      gst_buffer_unref (inbuf);
      GST_OBJECT_UNLOCK (pad);
      continue;
    }

    is_gap = GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_GAP);

    /* Try to make an output buffer */
    if (outbuf == NULL) {
      /* if this is a gap buffer but we have some more pads to check, skip it.
       * If we are at the last buffer, take it, regardless if it is a GAP
       * buffer or not. */
      if (is_gap && next) {
        GST_DEBUG_OBJECT (adder, "skipping, non-last GAP buffer");
        /* we keep the GAP buffer, if we don't have anymore buffers (all pads
         * EOS, we can use this one as the output buffer. */
        if (gapbuf == NULL)
          gapbuf = inbuf;
        else
          gst_buffer_unref (inbuf);
        GST_OBJECT_UNLOCK (pad);
        continue;
      }

      GST_LOG_OBJECT (adder, "channel %p: preparing output buffer of %d bytes",
          collect_data, outsize);

      /* make data and metadata writable, can simply return the inbuf when we
       * are the only one referencing this buffer. If this is the last (and
       * only) GAP buffer, it will automatically copy the GAP flag. */
      outbuf = gst_buffer_make_writable (inbuf);
      gst_buffer_map (outbuf, &outmap, GST_MAP_READWRITE);

      if (pad->volume != 1.0) {
        switch (adder->info.finfo->format) {
          case GST_AUDIO_FORMAT_U8:
            adder_orc_volume_u8 ((gpointer) outmap.data, pad->volume_i8,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_S8:
            adder_orc_volume_s8 ((gpointer) outmap.data, pad->volume_i8,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_U16:
            adder_orc_volume_u16 ((gpointer) outmap.data, pad->volume_i16,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_S16:
            adder_orc_volume_s16 ((gpointer) outmap.data, pad->volume_i16,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_U32:
            adder_orc_volume_u32 ((gpointer) outmap.data, pad->volume_i32,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_S32:
            adder_orc_volume_s32 ((gpointer) outmap.data, pad->volume_i32,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_F32:
            adder_orc_volume_f32 ((gpointer) outmap.data, pad->volume,
                outmap.size / bps);
            break;
          case GST_AUDIO_FORMAT_F64:
            adder_orc_volume_f64 ((gpointer) outmap.data, pad->volume,
                outmap.size / bps);
            break;
          default:
            g_assert_not_reached ();
            break;
        }
      }
    } else {
      if (!is_gap) {
        /* we had a previous output buffer, mix this non-GAP buffer */
        GstMapInfo inmap;

        gst_buffer_map (inbuf, &inmap, GST_MAP_READ);

        /* all buffers should have outsize, there are no short buffers because we
         * asked for the max size above */
        g_assert (inmap.size == outmap.size);

        GST_LOG_OBJECT (adder, "channel %p: mixing %" G_GSIZE_FORMAT " bytes"
            " from data %p", collect_data, inmap.size, inmap.data);

        /* further buffers, need to add them */
        if (pad->volume == 1.0) {
          switch (adder->info.finfo->format) {
            case GST_AUDIO_FORMAT_U8:
              adder_orc_add_u8 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S8:
              adder_orc_add_s8 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_U16:
              adder_orc_add_u16 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S16:
              adder_orc_add_s16 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_U32:
              adder_orc_add_u32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S32:
              adder_orc_add_s32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_F32:
              adder_orc_add_f32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_F64:
              adder_orc_add_f64 ((gpointer) outmap.data,
                  (gpointer) inmap.data, inmap.size / bps);
              break;
            default:
              g_assert_not_reached ();
              break;
          }
        } else {
          switch (adder->info.finfo->format) {
            case GST_AUDIO_FORMAT_U8:
              adder_orc_add_volume_u8 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i8, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S8:
              adder_orc_add_volume_s8 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i8, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_U16:
              adder_orc_add_volume_u16 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i16, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S16:
              adder_orc_add_volume_s16 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i16, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_U32:
              adder_orc_add_volume_u32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i32, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_S32:
              adder_orc_add_volume_s32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume_i32, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_F32:
              adder_orc_add_volume_f32 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume, inmap.size / bps);
              break;
            case GST_AUDIO_FORMAT_F64:
              adder_orc_add_volume_f64 ((gpointer) outmap.data,
                  (gpointer) inmap.data, pad->volume, inmap.size / bps);
              break;
            default:
              g_assert_not_reached ();
              break;
          }
        }
        gst_buffer_unmap (inbuf, &inmap);
      } else {
        /* skip gap buffer */
        GST_LOG_OBJECT (adder, "channel %p: skipping GAP buffer", collect_data);
      }
      gst_buffer_unref (inbuf);
    }
    GST_OBJECT_UNLOCK (pad);
  }

  if (outbuf)
    gst_buffer_unmap (outbuf, &outmap);

  if (is_eos)
    goto eos;

  if (outbuf == NULL) {
    /* no output buffer, reuse one of the GAP buffers then if we have one */
    if (gapbuf) {
      GST_LOG_OBJECT (adder, "reusing GAP buffer %p", gapbuf);
      outbuf = gapbuf;
    } else if (had_mute) {
      GstMapInfo map;

      /* Means we had all pads muted, create some silence */
      outbuf = gst_buffer_new_allocate (NULL, outsize, NULL);
      gst_buffer_map (outbuf, &map, GST_MAP_WRITE);
      gst_audio_format_info_fill_silence (adder->info.finfo, map.data, outsize);
      gst_buffer_unmap (outbuf, &map);
      GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_GAP);
    } else {
      /* assume EOS otherwise, this should not happen, really */
      goto eos;
    }
  } else if (gapbuf) {
    /* we had an output buffer, unref the gapbuffer we kept */
    gst_buffer_unref (gapbuf);
  }

  if (G_UNLIKELY (adder->pending_events)) {
    GList *tmp = adder->pending_events;

    while (tmp) {
      GstEvent *ev = (GstEvent *) tmp->data;

      gst_pad_push_event (adder->srcpad, ev);
      tmp = g_list_next (tmp);
    }
    g_list_free (adder->pending_events);
    adder->pending_events = NULL;
  }

  /* for the next timestamp, use the sample counter, which will
   * never accumulate rounding errors */
  if (adder->segment.rate > 0.0) {
    next_offset = adder->offset + outsize / bpf;
  } else {
    next_offset = adder->offset - outsize / bpf;
  }
  next_timestamp = gst_util_uint64_scale (next_offset, GST_SECOND, rate);

  /* set timestamps on the output buffer */
  GST_BUFFER_DTS (outbuf) = GST_CLOCK_TIME_NONE;
  if (adder->segment.rate > 0.0) {
    GST_BUFFER_PTS (outbuf) = adder->segment.position;
    GST_BUFFER_OFFSET (outbuf) = adder->offset;
    GST_BUFFER_OFFSET_END (outbuf) = next_offset;
    GST_BUFFER_DURATION (outbuf) = next_timestamp - adder->segment.position;
  } else {
    GST_BUFFER_PTS (outbuf) = next_timestamp;
    GST_BUFFER_OFFSET (outbuf) = next_offset;
    GST_BUFFER_OFFSET_END (outbuf) = adder->offset;
    GST_BUFFER_DURATION (outbuf) = adder->segment.position - next_timestamp;
  }
  if (is_discont) {
    GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
  } else {
    GST_BUFFER_FLAG_UNSET (outbuf, GST_BUFFER_FLAG_DISCONT);
  }

  adder->offset = next_offset;
  adder->segment.position = next_timestamp;

  /* send it out */
  GST_LOG_OBJECT (adder, "pushing outbuf %p, timestamp %" GST_TIME_FORMAT
      " offset %" G_GINT64_FORMAT, outbuf,
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (outbuf)),
      GST_BUFFER_OFFSET (outbuf));
  ret = gst_pad_push (adder->srcpad, outbuf);

  GST_LOG_OBJECT (adder, "pushed outbuf, result = %s", gst_flow_get_name (ret));

  return ret;

  /* ERRORS */
not_negotiated:
  {
    GST_ELEMENT_ERROR (adder, STREAM, FORMAT, (NULL),
        ("Unknown data received, not negotiated"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
eos:
  {
    GST_DEBUG_OBJECT (adder, "no data available, must be EOS");
    gst_pad_push_event (adder->srcpad, gst_event_new_eos ());
    return GST_FLOW_EOS;
  }
}

static GstStateChangeReturn
gst_adder_change_state (GstElement * element, GstStateChange transition)
{
  GstAdder *adder;
  GstStateChangeReturn ret;

  adder = GST_ADDER (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      adder->offset = 0;
      adder->flush_stop_pending = FALSE;
      adder->new_segment_pending = TRUE;
      adder->send_stream_start = TRUE;
      adder->send_caps = TRUE;
      gst_caps_replace (&adder->current_caps, NULL);
      gst_segment_init (&adder->segment, GST_FORMAT_TIME);
      gst_collect_pads_start (adder->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      /* need to unblock the collectpads before calling the
       * parent change_state so that streaming can finish */
      gst_collect_pads_stop (adder->collect);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    default:
      break;
  }

  return ret;
}

/* GstChildProxy implementation */
static GObject *
gst_adder_child_proxy_get_child_by_index (GstChildProxy * child_proxy,
    guint index)
{
  GstAdder *adder = GST_ADDER (child_proxy);
  GObject *obj = NULL;

  GST_OBJECT_LOCK (adder);
  obj = g_list_nth_data (GST_ELEMENT_CAST (adder)->sinkpads, index);
  if (obj)
    gst_object_ref (obj);
  GST_OBJECT_UNLOCK (adder);
  return obj;
}

static guint
gst_adder_child_proxy_get_children_count (GstChildProxy * child_proxy)
{
  guint count = 0;
  GstAdder *adder = GST_ADDER (child_proxy);

  GST_OBJECT_LOCK (adder);
  count = GST_ELEMENT_CAST (adder)->numsinkpads;
  GST_OBJECT_UNLOCK (adder);
  GST_INFO_OBJECT (adder, "Children Count: %d", count);
  return count;
}

static void
gst_adder_child_proxy_init (gpointer g_iface, gpointer iface_data)
{
  GstChildProxyInterface *iface = g_iface;

  GST_INFO ("initializing child proxy interface");
  iface->get_child_by_index = gst_adder_child_proxy_get_child_by_index;
  iface->get_children_count = gst_adder_child_proxy_get_children_count;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  gboolean ret = FALSE;
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "adder", 0,
      "audio channel mixing element");

  ret |= GST_ELEMENT_REGISTER (adder, plugin);

  return ret;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    adder,
    "Adds multiple streams",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
