/* GStreamer Stream Splitter
 * Copyright (C) 2010 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009 Nokia Corporation
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
#include "gststreamsplitter.h"

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src_%u", GST_PAD_SRC, GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_stream_splitter_debug);
#define GST_CAT_DEFAULT gst_stream_splitter_debug

G_DEFINE_TYPE (GstStreamSplitter, gst_stream_splitter, GST_TYPE_ELEMENT);

#define STREAMS_LOCK(obj) (g_mutex_lock(&obj->lock))
#define STREAMS_UNLOCK(obj) (g_mutex_unlock(&obj->lock))

static void gst_stream_splitter_dispose (GObject * object);
static void gst_stream_splitter_finalize (GObject * object);

static gboolean gst_stream_splitter_sink_setcaps (GstPad * pad, GstCaps * caps);

static GstPad *gst_stream_splitter_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_stream_splitter_release_pad (GstElement * element,
    GstPad * pad);

static void
gst_stream_splitter_class_init (GstStreamSplitterClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *gstelement_klass;

  gobject_klass = (GObjectClass *) klass;
  gstelement_klass = (GstElementClass *) klass;

  gobject_klass->dispose = gst_stream_splitter_dispose;
  gobject_klass->finalize = gst_stream_splitter_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_stream_splitter_debug, "streamsplitter", 0,
      "Stream Splitter");

  gst_element_class_add_static_pad_template (gstelement_klass, &src_template);
  gst_element_class_add_static_pad_template (gstelement_klass, &sink_template);

  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_stream_splitter_request_new_pad);
  gstelement_klass->release_pad =
      GST_DEBUG_FUNCPTR (gst_stream_splitter_release_pad);

  gst_element_class_set_static_metadata (gstelement_klass,
      "streamsplitter", "Generic",
      "Splits streams based on their media type",
      "Edward Hervey <edward.hervey@collabora.co.uk>");
}

static void
gst_stream_splitter_dispose (GObject * object)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) object;

  g_list_foreach (stream_splitter->pending_events, (GFunc) gst_event_unref,
      NULL);
  g_list_free (stream_splitter->pending_events);
  stream_splitter->pending_events = NULL;

  G_OBJECT_CLASS (gst_stream_splitter_parent_class)->dispose (object);
}

static void
gst_stream_splitter_finalize (GObject * object)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) object;

  g_mutex_clear (&stream_splitter->lock);

  G_OBJECT_CLASS (gst_stream_splitter_parent_class)->finalize (object);
}

static void
gst_stream_splitter_push_pending_events (GstStreamSplitter * splitter,
    GstPad * srcpad)
{
  GList *tmp;
  GST_DEBUG_OBJECT (srcpad, "Pushing out pending events");

  for (tmp = splitter->pending_events; tmp; tmp = tmp->next) {
    GstEvent *event = (GstEvent *) tmp->data;
    gst_pad_push_event (srcpad, event);
  }
  g_list_free (splitter->pending_events);
  splitter->pending_events = NULL;
}

static GstFlowReturn
gst_stream_splitter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) parent;
  GstFlowReturn res;
  GstPad *srcpad = NULL;

  STREAMS_LOCK (stream_splitter);
  if (stream_splitter->current)
    srcpad = gst_object_ref (stream_splitter->current);
  STREAMS_UNLOCK (stream_splitter);

  if (G_UNLIKELY (srcpad == NULL))
    goto nopad;

  if (G_UNLIKELY (stream_splitter->pending_events))
    gst_stream_splitter_push_pending_events (stream_splitter, srcpad);

  /* Forward to currently activated stream */
  res = gst_pad_push (srcpad, buf);
  gst_object_unref (srcpad);

  return res;

nopad:
  GST_WARNING_OBJECT (stream_splitter, "No output pad was configured");
  return GST_FLOW_ERROR;
}

static GList *
_flush_events (GstPad * pad, GList * events)
{
  GList *tmp;

  for (tmp = events; tmp; tmp = tmp->next) {
    if (GST_EVENT_TYPE (tmp->data) != GST_EVENT_EOS &&
        GST_EVENT_TYPE (tmp->data) != GST_EVENT_SEGMENT &&
        GST_EVENT_IS_STICKY (tmp->data) && pad != NULL) {
      gst_pad_store_sticky_event (pad, GST_EVENT_CAST (tmp->data));
    }
    gst_event_unref (tmp->data);
  }
  g_list_free (events);

  return NULL;
}

static gboolean
gst_stream_splitter_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) parent;
  gboolean res = TRUE;
  gboolean toall = FALSE;
  gboolean store = FALSE;
  /* FLUSH_START/STOP : forward to all
   * INBAND events : store to send in chain function to selected chain
   * OUT_OF_BAND events : send to all
   */

  GST_DEBUG_OBJECT (stream_splitter, "Got event %s",
      GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
      toall = TRUE;
      break;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      res = gst_stream_splitter_sink_setcaps (pad, caps);

      store = TRUE;
      break;
    }
    case GST_EVENT_FLUSH_STOP:
      toall = TRUE;
      STREAMS_LOCK (stream_splitter);
      stream_splitter->pending_events = _flush_events (stream_splitter->current,
          stream_splitter->pending_events);
      STREAMS_UNLOCK (stream_splitter);
      break;
    case GST_EVENT_FLUSH_START:
      toall = TRUE;
      break;
    case GST_EVENT_EOS:

      if (G_UNLIKELY (stream_splitter->pending_events)) {
        GstPad *srcpad = NULL;

        STREAMS_LOCK (stream_splitter);
        if (stream_splitter->current)
          srcpad = gst_object_ref (stream_splitter->current);
        STREAMS_UNLOCK (stream_splitter);

        if (srcpad) {
          gst_stream_splitter_push_pending_events (stream_splitter, srcpad);
          gst_object_unref (srcpad);
        }
      }

      toall = TRUE;
      break;
    default:
      if (GST_EVENT_TYPE (event) & GST_EVENT_TYPE_SERIALIZED)
        store = TRUE;
  }

  if (store) {
    stream_splitter->pending_events =
        g_list_append (stream_splitter->pending_events, event);
  } else if (toall) {
    GList *tmp;
    guint32 cookie;

    /* Send to all pads */
    STREAMS_LOCK (stream_splitter);
  resync:
    if (G_UNLIKELY (stream_splitter->srcpads == NULL)) {
      STREAMS_UNLOCK (stream_splitter);
      /* No source pads */
      gst_event_unref (event);
      res = FALSE;
      goto beach;
    }
    tmp = stream_splitter->srcpads;
    cookie = stream_splitter->cookie;
    while (tmp) {
      GstPad *srcpad = (GstPad *) tmp->data;
      STREAMS_UNLOCK (stream_splitter);
      gst_event_ref (event);
      res = gst_pad_push_event (srcpad, event);
      STREAMS_LOCK (stream_splitter);
      if (G_UNLIKELY (cookie != stream_splitter->cookie))
        goto resync;
      tmp = tmp->next;
    }
    STREAMS_UNLOCK (stream_splitter);
    gst_event_unref (event);
  } else {
    GstPad *pad;

    /* Only send to current pad */

    STREAMS_LOCK (stream_splitter);
    pad = stream_splitter->current;
    STREAMS_UNLOCK (stream_splitter);
    if (pad)
      res = gst_pad_push_event (pad, event);
    else {
      gst_event_unref (event);
      res = FALSE;
    }
  }

beach:
  return res;
}

static GstCaps *
gst_stream_splitter_sink_getcaps (GstPad * pad, GstCaps * filter)
{
  GstStreamSplitter *stream_splitter =
      (GstStreamSplitter *) GST_PAD_PARENT (pad);
  guint32 cookie;
  GList *tmp;
  GstCaps *res = NULL;

  /* Return the combination of all downstream caps */

  STREAMS_LOCK (stream_splitter);

resync:
  if (G_UNLIKELY (stream_splitter->srcpads == NULL)) {
    res = (filter ? gst_caps_ref (filter) : gst_caps_new_any ());
    goto beach;
  }

  res = NULL;
  cookie = stream_splitter->cookie;
  tmp = stream_splitter->srcpads;

  while (tmp) {
    GstPad *srcpad = (GstPad *) tmp->data;

    /* Ensure srcpad doesn't get destroyed while we query peer */
    gst_object_ref (srcpad);
    STREAMS_UNLOCK (stream_splitter);
    if (res) {
      GstCaps *peercaps = gst_pad_peer_query_caps (srcpad, filter);
      if (peercaps)
        res = gst_caps_merge (res, peercaps);
    } else {
      res = gst_pad_peer_query_caps (srcpad, filter);
    }
    STREAMS_LOCK (stream_splitter);
    gst_object_unref (srcpad);

    if (G_UNLIKELY (cookie != stream_splitter->cookie)) {
      if (res)
        gst_caps_unref (res);
      goto resync;
    }
    tmp = tmp->next;
  }

beach:
  STREAMS_UNLOCK (stream_splitter);
  return res;
}

static gboolean
gst_stream_splitter_sink_acceptcaps (GstPad * pad, GstCaps * caps)
{
  GstStreamSplitter *stream_splitter =
      (GstStreamSplitter *) GST_PAD_PARENT (pad);
  guint32 cookie;
  GList *tmp;
  gboolean res = FALSE;

  /* check if one of the downstream elements accepts the caps */
  STREAMS_LOCK (stream_splitter);

resync:
  res = FALSE;

  if (G_UNLIKELY (stream_splitter->srcpads == NULL))
    goto beach;

  cookie = stream_splitter->cookie;
  tmp = stream_splitter->srcpads;

  while (tmp) {
    GstPad *srcpad = (GstPad *) tmp->data;

    /* Ensure srcpad doesn't get destroyed while we query peer */
    gst_object_ref (srcpad);
    STREAMS_UNLOCK (stream_splitter);

    res = gst_pad_peer_query_accept_caps (srcpad, caps);

    STREAMS_LOCK (stream_splitter);
    gst_object_unref (srcpad);

    if (G_UNLIKELY (cookie != stream_splitter->cookie))
      goto resync;

    if (res)
      break;

    tmp = tmp->next;
  }

beach:
  STREAMS_UNLOCK (stream_splitter);
  return res;
}

static gboolean
gst_stream_splitter_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
    {
      GstCaps *filter, *caps;

      gst_query_parse_caps (query, &filter);
      caps = gst_stream_splitter_sink_getcaps (pad, filter);
      gst_query_set_caps_result (query, caps);
      gst_caps_unref (caps);
      res = TRUE;
      break;
    }
    case GST_QUERY_ACCEPT_CAPS:
    {
      GstCaps *caps;
      gboolean result;

      gst_query_parse_accept_caps (query, &caps);
      result = gst_stream_splitter_sink_acceptcaps (pad, caps);
      gst_query_set_accept_caps_result (query, result);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
  return res;
}

static gboolean
gst_stream_splitter_sink_setcaps (GstPad * pad, GstCaps * caps)
{
  GstPad *prev = NULL;
  GstStreamSplitter *stream_splitter =
      (GstStreamSplitter *) GST_PAD_PARENT (pad);
  guint32 cookie;
  GList *tmp;
  gboolean res;

  GST_DEBUG_OBJECT (stream_splitter, "caps %" GST_PTR_FORMAT, caps);

  /* Try on all pads, choose the one that succeeds as the current stream */
  STREAMS_LOCK (stream_splitter);

resync:
  if (G_UNLIKELY (stream_splitter->srcpads == NULL)) {
    res = FALSE;
    goto beach;
  }

  res = FALSE;
  prev =
      stream_splitter->
      current ? gst_object_ref (stream_splitter->current) : NULL;
  tmp = stream_splitter->srcpads;
  cookie = stream_splitter->cookie;

  while (tmp) {
    GstPad *srcpad = (GstPad *) tmp->data;
    GstCaps *peercaps;

    // GST_ERROR_OBJECT (srcpad, "Checking");
    STREAMS_UNLOCK (stream_splitter);
    peercaps = gst_pad_peer_query_caps (srcpad, NULL);
    if (peercaps) {
      res = gst_caps_can_intersect (caps, peercaps);
      gst_caps_unref (peercaps);
    }
    STREAMS_LOCK (stream_splitter);

    if (G_UNLIKELY (cookie != stream_splitter->cookie))
      goto resync;

    if (res) {
      GST_DEBUG_OBJECT (srcpad, "Setting caps on this pad was successful");
      stream_splitter->current = srcpad;
      if (prev && prev != srcpad) {
        /* When switching branches, we need to ensure that the branch it was
         * pushing to before is drainned and the encoder pushes all its internal
         * data, we do it by first letting downstream streamcombiner that we are
         * working on it, and then send an EOS to ensure all the data is
         * processed and cleanup the encoder with a flush start/flush stop
         * dance. Those events are discarded in the stream combiner */
        /* FIXME We should theoretically be able to use a DRAIN query here
         * instead but the encoders do not react properly to them */
        gst_pad_push_event (prev,
            gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM,
                gst_structure_new_empty ("start-draining-encoder")));
        gst_pad_push_event (prev, gst_event_new_eos ());
        gst_pad_push_event (prev, gst_event_new_flush_start ());
        gst_pad_push_event (prev, gst_event_new_flush_stop (FALSE));
      }
      goto beach;
    }
    tmp = tmp->next;
  }

beach:
  gst_clear_object (&prev);
  STREAMS_UNLOCK (stream_splitter);
  return res;
}

static gboolean
gst_stream_splitter_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) parent;

  if (gst_video_event_is_force_key_unit (event)) {
    guint32 seqnum = gst_event_get_seqnum (event);
    STREAMS_LOCK (stream_splitter);
    if (seqnum == stream_splitter->keyunit_seqnum) {
      STREAMS_UNLOCK (stream_splitter);
      GST_TRACE_OBJECT (pad,
          "Drop duplicated force-key-unit event %" G_GUINT32_FORMAT, seqnum);
      gst_event_unref (event);
      return TRUE;
    }
    stream_splitter->keyunit_seqnum = seqnum;
    STREAMS_UNLOCK (stream_splitter);
  }

  return gst_pad_event_default (pad, parent, event);
}

static void
gst_stream_splitter_init (GstStreamSplitter * stream_splitter)
{
  stream_splitter->sinkpad =
      gst_pad_new_from_static_template (&sink_template, "sink");
  gst_pad_set_chain_function (stream_splitter->sinkpad,
      gst_stream_splitter_chain);
  gst_pad_set_event_function (stream_splitter->sinkpad,
      gst_stream_splitter_sink_event);
  gst_pad_set_query_function (stream_splitter->sinkpad,
      gst_stream_splitter_sink_query);
  gst_element_add_pad (GST_ELEMENT (stream_splitter), stream_splitter->sinkpad);

  g_mutex_init (&stream_splitter->lock);
}

static GstPad *
gst_stream_splitter_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) element;
  GstPad *srcpad;

  srcpad = gst_pad_new_from_static_template (&src_template, name);

  STREAMS_LOCK (stream_splitter);
  stream_splitter->srcpads = g_list_append (stream_splitter->srcpads, srcpad);
  gst_pad_set_active (srcpad, TRUE);
  gst_element_add_pad (element, srcpad);
  stream_splitter->cookie++;
  gst_pad_set_event_function (srcpad, gst_stream_splitter_src_event);
  STREAMS_UNLOCK (stream_splitter);

  return srcpad;
}

static void
gst_stream_splitter_release_pad (GstElement * element, GstPad * pad)
{
  GstStreamSplitter *stream_splitter = (GstStreamSplitter *) element;
  GList *tmp;

  STREAMS_LOCK (stream_splitter);
  tmp = g_list_find (stream_splitter->srcpads, pad);
  if (tmp) {
    GstPad *pad = (GstPad *) tmp->data;

    stream_splitter->srcpads =
        g_list_delete_link (stream_splitter->srcpads, tmp);
    stream_splitter->cookie++;

    if (pad == stream_splitter->current) {
      /* Deactivate current flow */
      GST_DEBUG_OBJECT (element, "Removed pad was the current one");
      stream_splitter->current = NULL;
    }

    gst_element_remove_pad (element, pad);
  }
  STREAMS_UNLOCK (stream_splitter);

  return;
}
