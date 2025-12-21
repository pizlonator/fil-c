/* GStreamer Stream Combiner
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
#include "gststreamcombiner.h"
#include "gststreamcombinerpad.h"

static GstStaticPadTemplate src_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (gst_stream_combiner_debug);
#define GST_CAT_DEFAULT gst_stream_combiner_debug

G_DEFINE_TYPE (GstStreamCombiner, gst_stream_combiner, GST_TYPE_ELEMENT);

G_DEFINE_TYPE (GstStreamCombinerPad, gst_stream_combiner_pad, GST_TYPE_PAD);

#define STREAMS_LOCK(obj) (g_mutex_lock(&obj->lock))
#define STREAMS_UNLOCK(obj) (g_mutex_unlock(&obj->lock))

static void gst_stream_combiner_finalize (GObject * object);

static GstPad *gst_stream_combiner_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_stream_combiner_release_pad (GstElement * element,
    GstPad * pad);

static void
gst_stream_combiner_pad_class_init (GstStreamCombinerPadClass * klass)
{
  return;
}

static void
gst_stream_combiner_pad_init (GstStreamCombinerPad * mixerpad)
{
  mixerpad->is_eos = FALSE;
}

static void
gst_stream_combiner_class_init (GstStreamCombinerClass * klass)
{
  GObjectClass *gobject_klass;
  GstElementClass *gstelement_klass;

  gobject_klass = (GObjectClass *) klass;
  gstelement_klass = (GstElementClass *) klass;

  gobject_klass->finalize = gst_stream_combiner_finalize;

  GST_DEBUG_CATEGORY_INIT (gst_stream_combiner_debug, "streamcombiner", 0,
      "Stream Combiner");

  gst_element_class_add_static_pad_template (gstelement_klass, &src_template);
  gst_element_class_add_static_pad_template (gstelement_klass, &sink_template);

  gstelement_klass->request_new_pad =
      GST_DEBUG_FUNCPTR (gst_stream_combiner_request_new_pad);
  gstelement_klass->release_pad =
      GST_DEBUG_FUNCPTR (gst_stream_combiner_release_pad);

  gst_element_class_set_static_metadata (gstelement_klass,
      "streamcombiner", "Generic",
      "Recombines streams split by the streamsplitter element",
      "Edward Hervey <edward.hervey@collabora.co.uk>");
}

static void
gst_stream_combiner_finalize (GObject * object)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) object;

  g_mutex_clear (&stream_combiner->lock);

  G_OBJECT_CLASS (gst_stream_combiner_parent_class)->finalize (object);
}

static GstFlowReturn
gst_stream_combiner_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) parent;
  /* FIXME : IMPLEMENT */

  /* with lock taken, check if we're the active stream, if not drop */
  return gst_pad_push (stream_combiner->srcpad, buf);
}

static gboolean
_all_sink_pads_eos (GstStreamCombiner * combiner)
{
  GList *tmp;

  for (tmp = combiner->sinkpads; tmp; tmp = tmp->next) {
    if (!(GST_STREAM_COMBINER_PAD (tmp->data))->is_eos)
      return FALSE;
  }

  return TRUE;
}

static gboolean
gst_stream_combiner_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) parent;
  GstStreamCombinerPad *combiner_pad = GST_STREAM_COMBINER_PAD (pad);
  /* FIXME : IMPLEMENT */

  GST_DEBUG_OBJECT (pad, "Got event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
      STREAMS_LOCK (stream_combiner);
      if (gst_structure_has_name (gst_event_get_structure (event),
              "start-draining-encoder")) {
        GST_INFO_OBJECT (pad, "Starting to drain the encoder");
        stream_combiner->draining_encoder = TRUE;
      }
      STREAMS_UNLOCK (stream_combiner);
      break;
    case GST_EVENT_FLUSH_START:
      STREAMS_LOCK (stream_combiner);
      if (stream_combiner->draining_encoder) {
        GST_INFO_OBJECT (pad, "Discarding FLUSH_START as draining encoder");
        gst_clear_event (&event);
      }
      STREAMS_UNLOCK (stream_combiner);
      break;
    case GST_EVENT_FLUSH_STOP:
      STREAMS_LOCK (stream_combiner);
      if (stream_combiner->draining_encoder) {
        gst_clear_event (&event);
        GST_INFO_OBJECT (stream_combiner, "Done draining the encoder.");
      }
      stream_combiner->draining_encoder = FALSE;
      STREAMS_UNLOCK (stream_combiner);
      break;
    case GST_EVENT_EOS:
      STREAMS_LOCK (stream_combiner);
      if (stream_combiner->draining_encoder) {
        STREAMS_UNLOCK (stream_combiner);
        GST_INFO_OBJECT (stream_combiner, "Discarding EOS as draining encoder");
        gst_clear_event (&event);
        break;
      }
      combiner_pad->is_eos = TRUE;
      if (!_all_sink_pads_eos (stream_combiner)) {
        gst_event_unref (event);
        event = NULL;
      } else {
        GST_DEBUG_OBJECT (stream_combiner, "All sink pads eos, pushing eos");
      }
      STREAMS_UNLOCK (stream_combiner);
      break;
    default:
      break;
  }

  /* SEGMENT : lock, wait for other stream to EOS, select stream, unlock, push */
  /* FLUSH_START : lock, mark as flushing, unlock. if wasn't flushing forward */
  /* FLUSH_STOP : lock, unmark as flushing, unlock, if was flushing forward */
  /* OTHER : if selected pad forward */
  if (event)
    return gst_pad_push_event (stream_combiner->srcpad, event);
  return FALSE;
}

static gboolean
gst_stream_combiner_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) parent;

  return gst_pad_peer_query (stream_combiner->srcpad, query);
}

static gboolean
gst_stream_combiner_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) parent;
  GstPad *sinkpad = NULL;

  /* Forward force-key-unit event to all sinkpads */
  if (gst_video_event_is_force_key_unit (event))
    return gst_pad_event_default (pad, parent, event);

  STREAMS_LOCK (stream_combiner);
  if (stream_combiner->current)
    sinkpad = stream_combiner->current;
  else if (stream_combiner->sinkpads)
    sinkpad = (GstPad *) stream_combiner->sinkpads->data;
  STREAMS_UNLOCK (stream_combiner);

  if (sinkpad)
    /* Forward upstream as is */
    return gst_pad_push_event (sinkpad, event);

  return FALSE;
}

static gboolean
gst_stream_combiner_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) parent;
  GstPad *sinkpad = NULL;
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CAPS:
      ret = gst_pad_query_default (pad, parent, query);
      break;
    default:
      STREAMS_LOCK (stream_combiner);
      if (stream_combiner->current)
        sinkpad = stream_combiner->current;
      else if (stream_combiner->sinkpads)
        sinkpad = (GstPad *) stream_combiner->sinkpads->data;
      STREAMS_UNLOCK (stream_combiner);

      if (sinkpad)
        /* Forward upstream as is */
        ret = gst_pad_peer_query (sinkpad, query);
      break;
  }
  return ret;
}

static void
gst_stream_combiner_init (GstStreamCombiner * stream_combiner)
{
  stream_combiner->srcpad =
      gst_pad_new_from_static_template (&src_template, "src");
  gst_pad_set_event_function (stream_combiner->srcpad,
      gst_stream_combiner_src_event);
  gst_pad_set_query_function (stream_combiner->srcpad,
      gst_stream_combiner_src_query);
  gst_element_add_pad (GST_ELEMENT (stream_combiner), stream_combiner->srcpad);

  g_mutex_init (&stream_combiner->lock);
}

static GstPad *
gst_stream_combiner_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) element;
  GstStreamCombinerPad *combiner_pad;
  GstPad *sinkpad;
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (element);
  GstPadTemplate *template =
      gst_element_class_get_pad_template (klass, "sink_%u");

  GST_DEBUG_OBJECT (element, "templ:%p, name:%s", templ, name);

  combiner_pad = g_object_new (GST_TYPE_STREAM_COMBINER_PAD, "name", name,
      "template", template, "direction", template->direction, NULL);

  sinkpad = GST_PAD_CAST (combiner_pad);
  gst_pad_set_chain_function (sinkpad, gst_stream_combiner_chain);
  gst_pad_set_event_function (sinkpad, gst_stream_combiner_sink_event);
  gst_pad_set_query_function (sinkpad, gst_stream_combiner_sink_query);

  STREAMS_LOCK (stream_combiner);
  stream_combiner->sinkpads =
      g_list_append (stream_combiner->sinkpads, sinkpad);
  gst_pad_set_active (sinkpad, TRUE);
  gst_element_add_pad (element, sinkpad);
  stream_combiner->cookie++;
  STREAMS_UNLOCK (stream_combiner);

  GST_DEBUG_OBJECT (element, "Returning pad %p", sinkpad);

  return sinkpad;
}

static void
gst_stream_combiner_release_pad (GstElement * element, GstPad * pad)
{
  GstStreamCombiner *stream_combiner = (GstStreamCombiner *) element;
  GList *tmp;

  GST_DEBUG_OBJECT (element, "pad %s:%s", GST_DEBUG_PAD_NAME (pad));

  STREAMS_LOCK (stream_combiner);
  tmp = g_list_find (stream_combiner->sinkpads, pad);
  if (tmp) {
    GstPad *pad = (GstPad *) tmp->data;

    stream_combiner->sinkpads =
        g_list_delete_link (stream_combiner->sinkpads, tmp);
    stream_combiner->cookie++;

    if (pad == stream_combiner->current) {
      /* Deactivate current flow */
      GST_DEBUG_OBJECT (element, "Removed pad was the current one");
      stream_combiner->current = NULL;
    }
    GST_DEBUG_OBJECT (element, "Removing pad from ourself");
    gst_element_remove_pad (element, pad);
  }
  STREAMS_UNLOCK (stream_combiner);

  return;
}
