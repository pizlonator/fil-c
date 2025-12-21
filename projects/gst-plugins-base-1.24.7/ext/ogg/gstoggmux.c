/* OGG muxer plugin for GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 * Copyright (C) 2006 Thomas Vander Stichele <thomas at apestaart dot org>
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
 * SECTION:element-oggmux
 * @title: oggmux
 * @see_also: <link linkend="gst-plugins-base-plugins-oggdemux">oggdemux</link>
 *
 * This element merges streams (audio and video) into ogg files.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 v4l2src num-buffers=500 ! video/x-raw,width=320,height=240 ! videoconvert ! videorate ! theoraenc ! oggmux ! filesink location=video.ogg
 * ]|
 * Encodes a video stream captured from a v4l2-compatible camera to Ogg/Theora
 * (the encoding will stop automatically after 500 frames)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbytewriter.h>
#include <gst/audio/audio.h>
#include <gst/tag/tag.h>

#include "gstoggelements.h"
#include "gstoggmux.h"

/* memcpy - if someone knows a way to get rid of it, please speak up
 * note: the ogg docs even say you need this... */
#include <string.h>
#include <time.h>
#include <stdlib.h>             /* rand, srand, atoi */

GST_DEBUG_CATEGORY_STATIC (gst_ogg_mux_debug);
#define GST_CAT_DEFAULT gst_ogg_mux_debug

/* This isn't generally what you'd want with an end-time macro, because
   technically the end time of a buffer with invalid duration is invalid. But
   for sorting ogg pages this is what we want. */
#define GST_BUFFER_END_TIME(buf) \
    (GST_BUFFER_DURATION_IS_VALID (buf) \
    ? GST_BUFFER_TIMESTAMP (buf) + GST_BUFFER_DURATION (buf) \
    : GST_BUFFER_TIMESTAMP (buf))

#define GST_GP_FORMAT "[gp %8" G_GINT64_FORMAT "]"
#define GST_GP_CAST(_gp) ((gint64) _gp)

/* set to 0.5 seconds by default */
#define DEFAULT_MAX_DELAY       G_GINT64_CONSTANT(500000000)
#define DEFAULT_MAX_PAGE_DELAY  G_GINT64_CONSTANT(500000000)
#define DEFAULT_MAX_TOLERANCE   G_GINT64_CONSTANT(40000000)
#define DEFAULT_SKELETON        FALSE

enum
{
  ARG_0,
  ARG_MAX_DELAY,
  ARG_MAX_PAGE_DELAY,
  ARG_MAX_TOLERANCE,
  ARG_SKELETON
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ogg; audio/ogg; video/ogg")
    );

static GstStaticPadTemplate video_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("video_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("video/x-theora; "
        "application/x-ogm-video; video/x-dirac; "
        "video/x-smoke; video/x-vp8; video/x-daala")
    );

static GstStaticPadTemplate audio_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("audio_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS
    ("audio/x-vorbis; audio/x-flac; audio/x-speex; audio/x-celt; "
        "application/x-ogm-audio; audio/x-opus")
    );

static GstStaticPadTemplate subtitle_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("subtitle_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("text/x-cmml, encoded = (boolean) TRUE; "
        "subtitle/x-kate; application/x-kate")
    );

static void gst_ogg_mux_finalize (GObject * object);

static GstFlowReturn gst_ogg_mux_collected (GstCollectPads * pads,
    GstOggMux * ogg_mux);
static gboolean gst_ogg_mux_sink_event (GstCollectPads * pads,
    GstCollectData * pad, GstEvent * event, gpointer user_data);
static gboolean gst_ogg_mux_handle_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstPad *gst_ogg_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps);
static void gst_ogg_mux_release_pad (GstElement * element, GstPad * pad);
static void gst_ogg_pad_data_reset (GstOggMux * ogg_mux,
    GstOggPadData * pad_data);

static void gst_ogg_mux_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_ogg_mux_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstStateChangeReturn gst_ogg_mux_change_state (GstElement * element,
    GstStateChange transition);

/*static guint gst_ogg_mux_signals[LAST_SIGNAL] = { 0 }; */


#define gst_ogg_mux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstOggMux, gst_ogg_mux, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (oggmux, "oggmux", GST_RANK_PRIMARY,
    GST_TYPE_OGG_MUX, GST_DEBUG_CATEGORY_INIT (gst_ogg_mux_debug, "oggmux", 0,
        "ogg muxer"));

static void
gst_ogg_mux_class_init (GstOggMuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->finalize = gst_ogg_mux_finalize;
  gobject_class->get_property = gst_ogg_mux_get_property;
  gobject_class->set_property = gst_ogg_mux_set_property;

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &video_sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &audio_sink_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &subtitle_sink_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "Ogg muxer", "Codec/Muxer",
      "mux ogg streams (info about ogg: http://xiph.org)",
      "Wim Taymans <wim@fluendo.com>");

  gstelement_class->request_new_pad = gst_ogg_mux_request_new_pad;
  gstelement_class->release_pad = gst_ogg_mux_release_pad;

  g_object_class_install_property (gobject_class, ARG_MAX_DELAY,
      g_param_spec_uint64 ("max-delay", "Max delay",
          "Maximum delay in multiplexing streams", 0, G_MAXUINT64,
          DEFAULT_MAX_DELAY,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_MAX_PAGE_DELAY,
      g_param_spec_uint64 ("max-page-delay", "Max page delay",
          "Maximum delay for sending out a page", 0, G_MAXUINT64,
          DEFAULT_MAX_PAGE_DELAY,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_MAX_TOLERANCE,
      g_param_spec_uint64 ("max-tolerance", "Max time tolerance",
          "Maximum timestamp difference for maintaining perfect granules",
          0, G_MAXUINT64, DEFAULT_MAX_TOLERANCE,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, ARG_SKELETON,
      g_param_spec_boolean ("skeleton", "Skeleton",
          "Whether to include a Skeleton track",
          DEFAULT_SKELETON,
          (GParamFlags) G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_ogg_mux_change_state;

}

static void
gst_ogg_mux_clear (GstOggMux * ogg_mux)
{
  ogg_mux->pulling = NULL;
  ogg_mux->need_headers = TRUE;
  ogg_mux->need_start_events = TRUE;
  ogg_mux->delta_pad = NULL;
  ogg_mux->offset = 0;
  ogg_mux->next_ts = 0;
  ogg_mux->last_ts = GST_CLOCK_TIME_NONE;
}

static void
gst_ogg_mux_init (GstOggMux * ogg_mux)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (ogg_mux);

  ogg_mux->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  gst_pad_set_event_function (ogg_mux->srcpad, gst_ogg_mux_handle_src_event);
  gst_element_add_pad (GST_ELEMENT (ogg_mux), ogg_mux->srcpad);

  /* seed random number generator for creation of serial numbers */
  srand (time (NULL));

  ogg_mux->collect = gst_collect_pads_new ();
  gst_collect_pads_set_function (ogg_mux->collect,
      (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (gst_ogg_mux_collected),
      ogg_mux);
  gst_collect_pads_set_event_function (ogg_mux->collect,
      (GstCollectPadsEventFunction) GST_DEBUG_FUNCPTR (gst_ogg_mux_sink_event),
      ogg_mux);

  ogg_mux->max_delay = DEFAULT_MAX_DELAY;
  ogg_mux->max_page_delay = DEFAULT_MAX_PAGE_DELAY;
  ogg_mux->max_tolerance = DEFAULT_MAX_TOLERANCE;

  gst_ogg_mux_clear (ogg_mux);
}

static void
gst_ogg_mux_finalize (GObject * object)
{
  GstOggMux *ogg_mux;

  ogg_mux = GST_OGG_MUX (object);

  if (ogg_mux->collect) {
    gst_object_unref (ogg_mux->collect);
    ogg_mux->collect = NULL;
  }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ogg_mux_ogg_pad_destroy_notify (GstCollectData * data)
{
  GstOggPadData *oggpad = (GstOggPadData *) data;
  GstBuffer *buf;

  ogg_stream_clear (&oggpad->map.stream);
  gst_caps_replace (&oggpad->map.caps, NULL);

  if (oggpad->pagebuffers) {
    while ((buf = g_queue_pop_head (oggpad->pagebuffers)) != NULL) {
      gst_buffer_unref (buf);
    }
    g_queue_free (oggpad->pagebuffers);
    oggpad->pagebuffers = NULL;
  }
}

static GstPadLinkReturn
gst_ogg_mux_sinkconnect (GstPad * pad, GstObject * parent, GstPad * peer)
{
  GstOggMux *ogg_mux;

  ogg_mux = GST_OGG_MUX (parent);

  GST_DEBUG_OBJECT (ogg_mux, "sinkconnect triggered on %s", GST_PAD_NAME (pad));

  return GST_PAD_LINK_OK;
}

static void
gst_ogg_mux_flush (GstOggMux * ogg_mux)
{
  GSList *walk;

  for (walk = ogg_mux->collect->data; walk; walk = g_slist_next (walk)) {
    GstOggPadData *pad;

    pad = (GstOggPadData *) walk->data;

    gst_ogg_pad_data_reset (ogg_mux, pad);
  }

  gst_ogg_mux_clear (ogg_mux);
}

static gboolean
gst_ogg_mux_sink_event (GstCollectPads * pads, GstCollectData * pad,
    GstEvent * event, gpointer user_data)
{
  GstOggMux *ogg_mux = GST_OGG_MUX (user_data);
  GstOggPadData *ogg_pad = (GstOggPadData *) pad;

  GST_DEBUG_OBJECT (pad->pad, "Got %s event", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);

      /* We don't support non time NEWSEGMENT events */
      if (segment->format != GST_FORMAT_TIME) {
        gst_event_unref (event);
        event = NULL;
        break;
      }

      gst_segment_copy_into (segment, &ogg_pad->segment);
      break;
    }
    case GST_EVENT_FLUSH_STOP:{
      /* only a single flush-stop is forwarded from collect pads */
      gst_ogg_mux_flush (ogg_mux);
      break;
    }
    case GST_EVENT_TAG:{
      GstTagList *tags;

      gst_event_parse_tag (event, &tags);
      tags = gst_tag_list_merge (ogg_pad->tags, tags, GST_TAG_MERGE_APPEND);
      if (ogg_pad->tags)
        gst_tag_list_unref (ogg_pad->tags);
      ogg_pad->tags = tags;

      GST_DEBUG_OBJECT (ogg_mux, "Got tags %" GST_PTR_FORMAT, ogg_pad->tags);
      break;
    }
    default:
      break;
  }

  /* now GstCollectPads can take care of the rest, e.g. EOS */
  if (event != NULL)
    return gst_collect_pads_event_default (pads, pad, event, FALSE);

  return TRUE;
}

static gboolean
gst_ogg_mux_is_serialno_present (GstOggMux * ogg_mux, guint32 serialno)
{
  GSList *walk;

  walk = ogg_mux->collect->data;
  while (walk) {
    GstOggPadData *pad = (GstOggPadData *) walk->data;
    if (pad->map.serialno == serialno)
      return TRUE;
    walk = walk->next;
  }

  return FALSE;
}

static void
gst_ogg_pad_data_reset (GstOggMux * ogg_mux, GstOggPadData * oggpad)
{
  oggpad->packetno = 0;
  oggpad->pageno = 0;
  oggpad->eos = FALSE;

  /* we assume there will be some control data first for this pad */
  oggpad->state = GST_OGG_PAD_STATE_CONTROL;
  oggpad->new_page = TRUE;
  oggpad->first_delta = FALSE;
  oggpad->prev_delta = FALSE;
  oggpad->data_pushed = FALSE;
  oggpad->map.headers = NULL;
  oggpad->map.queued = NULL;
  oggpad->next_granule = 0;
  oggpad->keyframe_granule = -1;
  ogg_stream_clear (&oggpad->map.stream);
  ogg_stream_init (&oggpad->map.stream, oggpad->map.serialno);

  if (oggpad->pagebuffers) {
    GstBuffer *buf;

    while ((buf = g_queue_pop_head (oggpad->pagebuffers)) != NULL) {
      gst_buffer_unref (buf);
    }
  } else if (GST_STATE (ogg_mux) > GST_STATE_READY) {
    /* This will be initialized in init_collectpads when going from ready
     * paused state */
    oggpad->pagebuffers = g_queue_new ();
  }

  gst_segment_init (&oggpad->segment, GST_FORMAT_TIME);
}

static guint32
gst_ogg_mux_generate_serialno (GstOggMux * ogg_mux)
{
  guint32 serialno;

  do {
    serialno = g_random_int_range (0, G_MAXINT32);
  } while (gst_ogg_mux_is_serialno_present (ogg_mux, serialno));

  return serialno;
}

static GstPad *
gst_ogg_mux_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstOggMux *ogg_mux;
  GstPad *newpad;
  GstElementClass *klass;

  g_return_val_if_fail (templ != NULL, NULL);

  if (templ->direction != GST_PAD_SINK)
    goto wrong_direction;

  g_return_val_if_fail (GST_IS_OGG_MUX (element), NULL);
  ogg_mux = GST_OGG_MUX (element);

  klass = GST_ELEMENT_GET_CLASS (element);

  if (templ != gst_element_class_get_pad_template (klass, "video_%u") &&
      templ != gst_element_class_get_pad_template (klass, "audio_%u") &&
      templ != gst_element_class_get_pad_template (klass, "subtitle_%u")) {
    goto wrong_template;
  }

  {
    guint32 serial;
    gchar *name = NULL;

    if (req_name == NULL || strlen (req_name) < 6) {
      /* no name given when requesting the pad, use random serial number */
      serial = gst_ogg_mux_generate_serialno (ogg_mux);
    } else {
      /* parse serial number from requested padname */
      unsigned long long_serial;
      char *endptr = NULL;
      long_serial = strtoul (&req_name[5], &endptr, 10);
      if ((endptr && *endptr) || (long_serial & ~0xffffffff)) {
        GST_WARNING_OBJECT (ogg_mux, "Invalid serial number specification: %s",
            req_name + 5);
        return NULL;
      }
      serial = (guint32) long_serial;
    }
    /* create new pad with the name */
    GST_DEBUG_OBJECT (ogg_mux, "Creating new pad for serial %d", serial);

    if (templ == gst_element_class_get_pad_template (klass, "video_%u")) {
      name = g_strdup_printf ("video_%u", serial);
    } else if (templ == gst_element_class_get_pad_template (klass, "audio_%u")) {
      name = g_strdup_printf ("audio_%u", serial);
    } else if (templ == gst_element_class_get_pad_template (klass,
            "subtitle_%u")) {
      name = g_strdup_printf ("subtitle_%u", serial);
    }
    newpad = gst_pad_new_from_template (templ, name);
    g_free (name);

    /* construct our own wrapper data structure for the pad to
     * keep track of its status */
    {
      GstOggPadData *oggpad;

      oggpad = (GstOggPadData *)
          gst_collect_pads_add_pad (ogg_mux->collect, newpad,
          sizeof (GstOggPadData), gst_ogg_mux_ogg_pad_destroy_notify, FALSE);
      ogg_mux->active_pads++;

      oggpad->map.serialno = serial;
      gst_ogg_pad_data_reset (ogg_mux, oggpad);
    }
  }

  /* setup some pad functions */
  gst_pad_set_link_function (newpad, gst_ogg_mux_sinkconnect);

  /* dd the pad to the element */
  gst_element_add_pad (element, newpad);

  return newpad;

  /* ERRORS */
wrong_direction:
  {
    g_warning ("ogg_mux: request pad that is not a SINK pad\n");
    return NULL;
  }
wrong_template:
  {
    g_warning ("ogg_mux: this is not our template!\n");
    return NULL;
  }
}

static void
gst_ogg_mux_release_pad (GstElement * element, GstPad * pad)
{
  GstOggMux *ogg_mux;

  ogg_mux = GST_OGG_MUX (gst_pad_get_parent (pad));

  gst_collect_pads_remove_pad (ogg_mux->collect, pad);
  gst_element_remove_pad (element, pad);

  gst_object_unref (ogg_mux);
}

/* handle events */
static gboolean
gst_ogg_mux_handle_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean res = FALSE;
  GstOggMux *ogg_mux = GST_OGG_MUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:{
      GstSeekFlags flags;

      gst_event_parse_seek (event, NULL, NULL, &flags, NULL, NULL, NULL, NULL);
      if (!ogg_mux->need_headers && (flags & GST_SEEK_FLAG_FLUSH) != 0) {
        /* don't allow flushing seeks once we started */
        gst_event_unref (event);
        event = NULL;
      }
      break;
    }
    default:
      break;
  }

  if (event != NULL)
    res = gst_pad_event_default (pad, parent, event);

  return res;
}

static GstBuffer *
gst_ogg_mux_buffer_from_page (GstOggMux * mux, ogg_page * page, gboolean delta)
{
  GstBuffer *buffer;

  /* allocate space for header and body */
  buffer = gst_buffer_new_and_alloc (page->header_len + page->body_len);
  gst_buffer_fill (buffer, 0, page->header, page->header_len);
  gst_buffer_fill (buffer, page->header_len, page->body, page->body_len);

  /* Here we set granulepos as our OFFSET_END to give easy direct access to
   * this value later. Before we push it, we reset this to OFFSET + SIZE
   * (see gst_ogg_mux_push_buffer). */
  GST_BUFFER_OFFSET_END (buffer) = ogg_page_granulepos (page);
  if (delta)
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  GST_LOG_OBJECT (mux, GST_GP_FORMAT
      " created buffer %p from ogg page",
      GST_GP_CAST (ogg_page_granulepos (page)), buffer);

  return buffer;
}

static GstFlowReturn
gst_ogg_mux_push_buffer (GstOggMux * mux, GstBuffer * buffer,
    GstOggPadData * oggpad)
{
  /* fix up OFFSET and OFFSET_END again */
  GST_BUFFER_OFFSET (buffer) = mux->offset;
  mux->offset += gst_buffer_get_size (buffer);
  GST_BUFFER_OFFSET_END (buffer) = mux->offset;

  /* Ensure we have monotonically increasing timestamps in the output. */
  if (GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    gint64 run_time = GST_BUFFER_TIMESTAMP (buffer);
    if (mux->last_ts != GST_CLOCK_TIME_NONE && run_time < mux->last_ts)
      GST_BUFFER_TIMESTAMP (buffer) = mux->last_ts;
    else
      mux->last_ts = run_time;
  }

  GST_LOG_OBJECT (mux->srcpad, "pushing %p, last_ts=%" GST_TIME_FORMAT,
      buffer, GST_TIME_ARGS (mux->last_ts));

  return gst_pad_push (mux->srcpad, buffer);
}

/* if all queues have at least one page, dequeue the page with the lowest
 * timestamp */
static gboolean
gst_ogg_mux_dequeue_page (GstOggMux * mux, GstFlowReturn * flowret)
{
  GSList *walk;
  GstOggPadData *opad = NULL;   /* "oldest" pad */
  GstClockTime oldest = GST_CLOCK_TIME_NONE;
  GstBuffer *buf = NULL;
  gboolean ret = FALSE;

  *flowret = GST_FLOW_OK;

  walk = mux->collect->data;
  while (walk) {
    GstOggPadData *pad = (GstOggPadData *) walk->data;

    /* We need each queue to either be at EOS, or have one or more pages
     * available with a set granulepos (i.e. not -1), otherwise we don't have
     * enough data yet to determine which stream needs to go next for correct
     * time ordering. */
    if (pad->pagebuffers->length == 0) {
      if (pad->eos) {
        GST_LOG_OBJECT (pad->collect.pad,
            "pad is EOS, skipping for dequeue decision");
      } else {
        GST_LOG_OBJECT (pad->collect.pad,
            "no pages in this queue, can't dequeue");
        return FALSE;
      }
    } else {
      /* We then need to check for a non-negative granulepos */
      gboolean valid = FALSE;
      GList *l;

      for (l = pad->pagebuffers->head; l != NULL; l = l->next) {
        buf = l->data;
        /* Here we check the OFFSET_END, which is actually temporarily the
         * granulepos value for this buffer */
        if (GST_BUFFER_OFFSET_END_IS_VALID (buf)) {
          valid = TRUE;
          break;
        }
      }
      if (!valid) {
        GST_LOG_OBJECT (pad->collect.pad,
            "No page timestamps in queue, can't dequeue");
        return FALSE;
      }
    }

    walk = g_slist_next (walk);
  }

  walk = mux->collect->data;
  while (walk) {
    GstOggPadData *pad = (GstOggPadData *) walk->data;

    /* any page with a granulepos of -1 can be pushed immediately.
     * TODO: it CAN be, but it seems silly to do so? */
    buf = g_queue_peek_head (pad->pagebuffers);
    while (buf && GST_BUFFER_OFFSET_END (buf) == -1) {
      GST_LOG_OBJECT (pad->collect.pad, "[gp        -1] pushing page");
      g_queue_pop_head (pad->pagebuffers);
      *flowret = gst_ogg_mux_push_buffer (mux, buf, pad);
      buf = g_queue_peek_head (pad->pagebuffers);
      ret = TRUE;
    }

    if (buf) {
      /* if no oldest buffer yet, take this one */
      if (oldest == GST_CLOCK_TIME_NONE) {
        GST_LOG_OBJECT (mux, "no oldest yet, taking buffer %p from pad %"
            GST_PTR_FORMAT " with gp time %" GST_TIME_FORMAT,
            buf, pad->collect.pad, GST_TIME_ARGS (GST_BUFFER_OFFSET (buf)));
        oldest = GST_BUFFER_OFFSET (buf);
        opad = pad;
      } else {
        /* if we have an oldest, compare with this one */
        if (GST_BUFFER_OFFSET (buf) < oldest) {
          GST_LOG_OBJECT (mux, "older buffer %p, taking from pad %"
              GST_PTR_FORMAT " with gp time %" GST_TIME_FORMAT,
              buf, pad->collect.pad, GST_TIME_ARGS (GST_BUFFER_OFFSET (buf)));
          oldest = GST_BUFFER_OFFSET (buf);
          opad = pad;
        }
      }
    }
    walk = g_slist_next (walk);
  }

  if (oldest != GST_CLOCK_TIME_NONE) {
    g_assert (opad);
    buf = g_queue_pop_head (opad->pagebuffers);
    GST_LOG_OBJECT (opad->collect.pad,
        GST_GP_FORMAT " pushing oldest page buffer %p (granulepos time %"
        GST_TIME_FORMAT ")", GST_BUFFER_OFFSET_END (buf), buf,
        GST_TIME_ARGS (GST_BUFFER_OFFSET (buf)));
    *flowret = gst_ogg_mux_push_buffer (mux, buf, opad);
    ret = TRUE;
  }

  return ret;
}

/* put the given ogg page on a per-pad queue, timestamping it correctly.
 * after that, dequeue and push as many pages as possible.
 * Caller should make sure:
 * pad->timestamp     was set with the timestamp of the first packet put
 *                    on the page
 * pad->timestamp_end was set with the timestamp + duration of the last packet
 *                    put on the page
 * pad->gp_time       was set with the time matching the gp of the last
 *                    packet put on the page
 *
 * will also reset timestamp and timestamp_end, so caller func can restart
 * counting.
 */
static GstFlowReturn
gst_ogg_mux_pad_queue_page (GstOggMux * mux, GstOggPadData * pad,
    ogg_page * page, gboolean delta)
{
  GstFlowReturn ret;
  GstBuffer *buffer = gst_ogg_mux_buffer_from_page (mux, page, delta);

  /* take the timestamp of the first packet on this page */
  GST_BUFFER_TIMESTAMP (buffer) = pad->timestamp;
  GST_BUFFER_DURATION (buffer) = pad->timestamp_end - pad->timestamp;
  /* take the gp time of the last completed packet on this page */
  GST_BUFFER_OFFSET (buffer) = pad->gp_time;

  /* the next page will start where the current page's end time leaves off */
  pad->timestamp = pad->timestamp_end;

  g_queue_push_tail (pad->pagebuffers, buffer);
  GST_LOG_OBJECT (pad->collect.pad, GST_GP_FORMAT
      " queued buffer page %p (gp time %"
      GST_TIME_FORMAT ", timestamp %" GST_TIME_FORMAT
      "), %d page buffers queued", GST_GP_CAST (ogg_page_granulepos (page)),
      buffer, GST_TIME_ARGS (GST_BUFFER_OFFSET (buffer)),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
      g_queue_get_length (pad->pagebuffers));

  while (gst_ogg_mux_dequeue_page (mux, &ret)) {
    if (ret != GST_FLOW_OK)
      break;
  }

  return ret;
}

/*
 * Given two pads, compare the buffers queued on it.
 * Returns:
 *  0 if they have an equal priority
 * -1 if the first is better
 *  1 if the second is better
 * Priority decided by: a) validity, b) older timestamp, c) smaller number
 * of muxed pages
 */
static gint
gst_ogg_mux_compare_pads (GstOggMux * ogg_mux, GstOggPadData * first,
    GstOggPadData * second)
{
  guint64 firsttime, secondtime;

  /* if the first pad doesn't contain anything or is even NULL, return
   * the second pad as best candidate and vice versa */
  if (first == NULL)
    return 1;
  if (second == NULL)
    return -1;

  /* no timestamp on first buffer, it must go first */
  firsttime = GST_BUFFER_TIMESTAMP (first->buffer);
  if (firsttime == GST_CLOCK_TIME_NONE)
    return -1;

  /* no timestamp on second buffer, it must go first */
  secondtime = GST_BUFFER_TIMESTAMP (second->buffer);
  if (secondtime == GST_CLOCK_TIME_NONE)
    return 1;

  /* first buffer has higher timestamp, second one should go first */
  if (secondtime < firsttime)
    return 1;
  /* second buffer has higher timestamp, first one should go first */
  else if (secondtime > firsttime)
    return -1;
  else {
    /* buffers with equal timestamps, prefer the pad that has the
     * least number of pages muxed */
    if (second->pageno < first->pageno)
      return 1;
    else if (second->pageno > first->pageno)
      return -1;
  }

  /* same priority if all of the above failed */
  return 0;
}

static GstBuffer *
gst_ogg_mux_decorate_buffer (GstOggMux * ogg_mux, GstOggPadData * pad,
    GstBuffer * buf)
{
  GstClockTime time, end_time;
  gint64 duration, granule, limit;
  GstClockTime next_time;
  GstClockTimeDiff diff;
  GstMapInfo map;
  ogg_packet packet;
  gboolean end_clip = TRUE;
  GstAudioClippingMeta *meta;

  /* ensure messing with metadata is ok */
  buf = gst_buffer_make_writable (buf);

  /* convert time to running time, so we need no longer bother about that */
  time = GST_BUFFER_TIMESTAMP (buf);
  if (G_LIKELY (GST_CLOCK_TIME_IS_VALID (time))) {
    time = gst_segment_to_running_time (&pad->segment, GST_FORMAT_TIME, time);
    if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (time))) {
      gst_buffer_unref (buf);
      return NULL;
    } else {
      GST_BUFFER_TIMESTAMP (buf) = time;
    }
  }

  /* now come up with granulepos stuff corresponding to time */
  if (!pad->have_type ||
      pad->map.granulerate_n <= 0 || pad->map.granulerate_d <= 0)
    goto no_granule;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;

  gst_ogg_stream_update_stats (&pad->map, &packet);

  duration = gst_ogg_stream_get_packet_duration (&pad->map, &packet);

  gst_buffer_unmap (buf, &map);

  /* give up if no duration can be determined, relying on upstream */
  if (G_UNLIKELY (duration < 0)) {
    /* well, if some day we really could handle sparse input ... */
    if (pad->map.is_sparse) {
      granule = 0;
      limit = 1;
      diff = 2;
      goto resync;
    }
    GST_WARNING_OBJECT (pad->collect.pad,
        "failed to determine packet duration");
    goto no_granule;
  }

  /* The last packet may have clipped samples. We need to test against
   * the segment to ensure we do not use a granpos that encompasses those.
   */
  if (pad->map.audio_clipping) {
    GstAudioClippingMeta *cmeta = gst_buffer_get_audio_clipping_meta (buf);

    g_assert (!cmeta || cmeta->format == GST_FORMAT_DEFAULT);
    if (cmeta && cmeta->end && cmeta->end < duration) {
      GST_DEBUG_OBJECT (pad->collect.pad,
          "Clipping %" G_GUINT64_FORMAT " samples at the end", cmeta->end);
      duration -= cmeta->end;
      end_clip = FALSE;
    }
  }

  if (end_clip) {
    end_time =
        gst_ogg_stream_granule_to_time (&pad->map,
        pad->next_granule + duration);
    meta = gst_buffer_get_audio_clipping_meta (buf);
    if (meta && meta->end) {
      if (meta->format == GST_FORMAT_DEFAULT) {
        if (meta->end > duration) {
          GST_WARNING_OBJECT (pad->collect.pad,
              "Clip meta tries to clip more sample than exist in the buffer, clipping all");
          duration = 0;
        } else {
          duration -= meta->end;
        }
      } else {
        GST_WARNING_OBJECT (pad->collect.pad,
            "Unsupported format in clip meta");
      }
    }
    if (end_time > pad->segment.stop
        && !GST_CLOCK_TIME_IS_VALID (gst_segment_to_running_time (&pad->segment,
                GST_FORMAT_TIME, pad->segment.start + end_time))) {
      gint64 actual_duration =
          gst_util_uint64_scale_round (pad->segment.stop - time,
          pad->map.granulerate_n,
          GST_SECOND * pad->map.granulerate_d);
      GST_INFO_OBJECT (ogg_mux,
          "Got clipped last packet of duration %" G_GINT64_FORMAT " (%"
          G_GINT64_FORMAT " clipped)", actual_duration,
          duration - actual_duration);
      duration = actual_duration;
    }
  }

  GST_LOG_OBJECT (pad->collect.pad, "buffer ts %" GST_TIME_FORMAT
      ", duration %" GST_TIME_FORMAT ", granule duration %" G_GINT64_FORMAT,
      GST_TIME_ARGS (time), GST_TIME_ARGS (GST_BUFFER_DURATION (buf)),
      duration);

  /* determine granule corresponding to time,
   * using the inverse of oggdemux' granule -> time */

  /* see if interpolated granule matches good enough */
  granule = pad->next_granule;
  next_time = gst_ogg_stream_granule_to_time (&pad->map, pad->next_granule);
  diff = GST_CLOCK_DIFF (next_time, time);

  /* we tolerate deviation up to configured or within granule granularity */
  limit = gst_ogg_stream_granule_to_time (&pad->map, 1) / 2;
  limit = MAX (limit, ogg_mux->max_tolerance);

  GST_LOG_OBJECT (pad->collect.pad, "expected granule %" G_GINT64_FORMAT " == "
      "time %" GST_TIME_FORMAT " --> ts diff %" GST_STIME_FORMAT
      " < tolerance %" GST_TIME_FORMAT " (?)",
      granule, GST_TIME_ARGS (next_time), GST_STIME_ARGS (diff),
      GST_TIME_ARGS (limit));

resync:
  /* if not good enough, determine granule based on time */
  if (diff > limit || diff < -limit) {
    granule = gst_util_uint64_scale_round (time, pad->map.granulerate_n,
        GST_SECOND * pad->map.granulerate_d);
    GST_DEBUG_OBJECT (pad->collect.pad,
        "resyncing to determined granule %" G_GINT64_FORMAT, granule);
  }

  if (pad->map.is_ogm || pad->map.is_sparse) {
    pad->next_granule = granule;
  } else {
    granule += duration;
    pad->next_granule = granule;
  }

  /* track previous keyframe */
  if (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT))
    pad->keyframe_granule = granule;

  /* determine corresponding time and granulepos */
  GST_BUFFER_OFFSET (buf) = gst_ogg_stream_granule_to_time (&pad->map, granule);
  GST_BUFFER_OFFSET_END (buf) =
      gst_ogg_stream_granule_to_granulepos (&pad->map, granule,
      pad->keyframe_granule);

  GST_LOG_OBJECT (pad->collect.pad,
      GST_GP_FORMAT " decorated buffer %p (granulepos time %" GST_TIME_FORMAT
      ")", GST_BUFFER_OFFSET_END (buf), buf,
      GST_TIME_ARGS (GST_BUFFER_OFFSET (buf)));

  return buf;

  /* ERRORS */
no_granule:
  {
    GST_DEBUG_OBJECT (pad->collect.pad, "could not determine granulepos, "
        "falling back to upstream provided metadata");
    return buf;
  }
}


/* make sure at least one buffer is queued on all pads, two if possible
 *
 * if pad->buffer == NULL, pad->next_buffer !=  NULL, then
 *   we do not know if the buffer is the last or not
 * if pad->buffer != NULL, pad->next_buffer != NULL, then
 *   pad->buffer is not the last buffer for the pad
 * if pad->buffer != NULL, pad->next_buffer == NULL, then
 *   pad->buffer if the last buffer for the pad
 *
 * returns a pointer to an oggpad that holds the best buffer, or
 * NULL when no pad was usable. "best" means the buffer marked
 * with the lowest timestamp. If best->buffer == NULL then either
 * we're at EOS (popped = FALSE), or a buffer got dropped, so retry. */
static GstOggPadData *
gst_ogg_mux_queue_pads (GstOggMux * ogg_mux, gboolean * popped)
{
  GstOggPadData *bestpad = NULL;
  GSList *walk;

  *popped = FALSE;

  /* try to make sure we have a buffer from each usable pad first */
  walk = ogg_mux->collect->data;
  while (walk) {
    GstOggPadData *pad;
    GstCollectData *data;

    data = (GstCollectData *) walk->data;
    pad = (GstOggPadData *) data;

    walk = g_slist_next (walk);

    GST_LOG_OBJECT (data->pad, "looking at pad for buffer");

    /* try to get a new buffer for this pad if needed and possible */
    if (pad->buffer == NULL) {
      GstBuffer *buf;

      buf = gst_collect_pads_pop (ogg_mux->collect, data);
      GST_LOG_OBJECT (data->pad, "popped buffer %" GST_PTR_FORMAT, buf);

      /* On EOS we get a NULL buffer */
      if (buf != NULL) {
        *popped = TRUE;

        if (ogg_mux->delta_pad == NULL &&
            GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT))
          ogg_mux->delta_pad = pad;

        /* if we need headers */
        if (pad->state == GST_OGG_PAD_STATE_CONTROL) {
          /* and we have one */
          ogg_packet packet;
          gboolean is_header;
          GstMapInfo map;

          gst_buffer_map (buf, &map, GST_MAP_READ);
          packet.packet = map.data;
          packet.bytes = map.size;

          /* if we're not yet in data mode, ensure we're setup on the first packet */
          if (!pad->have_type) {
            GstCaps *caps;

            /* Use headers in caps, if any; this will allow us to be resilient
             * to starting streams on the fly, and some streams (like VP8
             * at least) do not send headers packets, as other muxers don't
             * expect/need them. */
            caps = gst_pad_get_current_caps (GST_PAD_CAST (data->pad));
            GST_DEBUG_OBJECT (data->pad, "checking caps: %" GST_PTR_FORMAT,
                caps);

            pad->have_type =
                gst_ogg_stream_setup_map_from_caps_headers (&pad->map, caps);

            if (!pad->have_type) {
              /* fallback on the packet */
              pad->have_type = gst_ogg_stream_setup_map (&pad->map, &packet);
            }
            if (!pad->have_type) {
              /* fallback 2 to try to get the mapping from the caps */
              pad->have_type =
                  gst_ogg_stream_setup_map_from_caps (&pad->map, caps);
            }
            if (!pad->have_type) {
              GST_ERROR_OBJECT (data->pad,
                  "mapper didn't recognise input stream " "(pad caps: %"
                  GST_PTR_FORMAT ")", caps);
            } else {
              GST_DEBUG_OBJECT (data->pad, "caps detected: %" GST_PTR_FORMAT,
                  pad->map.caps);

              if (pad->map.is_sparse) {
                GST_DEBUG_OBJECT (data->pad, "Pad is sparse, marking as such");
                gst_collect_pads_set_waiting (ogg_mux->collect,
                    (GstCollectData *) pad, FALSE);
              }

              if (pad->map.is_video && ogg_mux->delta_pad == NULL) {
                ogg_mux->delta_pad = pad;
                GST_INFO_OBJECT (data->pad, "selected delta pad");
              }
            }
            if (caps)
              gst_caps_unref (caps);
          }

          if (pad->have_type)
            is_header = gst_ogg_stream_packet_is_header (&pad->map, &packet);
          else                  /* fallback (FIXME 0.11: remove IN_CAPS hack) */
            is_header = GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_HEADER);

          gst_buffer_unmap (buf, &map);

          if (is_header) {
            GST_DEBUG_OBJECT (ogg_mux,
                "got header buffer in control state, ignoring");
            /* just ignore */
            pad->map.n_header_packets_seen++;
            gst_buffer_unref (buf);
            buf = NULL;
          } else {
            GST_DEBUG_OBJECT (ogg_mux,
                "got data buffer in control state, switching to data mode");
            /* this is a data buffer so switch to data state */
            pad->state = GST_OGG_PAD_STATE_DATA;

            /* check if this type of stream allows generating granulepos
             * metadata here, if not, upstream will have to provide */
            if (gst_ogg_stream_granule_to_granulepos (&pad->map, 1, 1) < 0) {
              GST_WARNING_OBJECT (data->pad, "can not generate metadata; "
                  "relying on upstream");
              /* disable metadata code path, otherwise not used anyway */
              pad->map.granulerate_n = 0;
            }
          }
        }

        /* so now we should have a real data packet;
         * see that it is properly decorated */
        if (G_LIKELY (buf)) {
          buf = gst_ogg_mux_decorate_buffer (ogg_mux, pad, buf);
          if (G_UNLIKELY (!buf))
            GST_DEBUG_OBJECT (data->pad, "buffer clipped");
        }
      }

      pad->buffer = buf;
    }

    /* we should have a buffer now, see if it is the best pad to
     * pull on. Our best pad can't be eos */
    if (pad->buffer && !pad->eos) {
      if (gst_ogg_mux_compare_pads (ogg_mux, bestpad, pad) > 0) {
        GST_LOG_OBJECT (data->pad,
            "new best pad, with buffer %" GST_PTR_FORMAT, pad->buffer);

        bestpad = pad;
      }
    }
  }

  return bestpad;
}

static GList *
gst_ogg_mux_get_headers (GstOggPadData * pad)
{
  GList *res = NULL;
  GstStructure *structure;
  GstCaps *caps;
  const GValue *streamheader;
  GstPad *thepad;
  GstBuffer *header;

  thepad = pad->collect.pad;

  GST_LOG_OBJECT (thepad, "getting headers");

  caps = gst_pad_get_current_caps (thepad);
  if (caps == NULL) {
    GST_INFO_OBJECT (thepad, "got empty caps as negotiated format");
    return NULL;
  }

  structure = gst_caps_get_structure (caps, 0);
  streamheader = gst_structure_get_value (structure, "streamheader");
  if (streamheader != NULL) {
    GST_LOG_OBJECT (thepad, "got header");
    if (G_VALUE_TYPE (streamheader) == GST_TYPE_ARRAY) {
      GArray *bufarr = g_value_peek_pointer (streamheader);
      gint i;

      GST_LOG_OBJECT (thepad, "got fixed list");

      for (i = 0; i < bufarr->len; i++) {
        GValue *bufval = &g_array_index (bufarr, GValue, i);

        GST_LOG_OBJECT (thepad, "item %d", i);
        if (G_VALUE_TYPE (bufval) == GST_TYPE_BUFFER) {
          GstBuffer *buf = g_value_peek_pointer (bufval);

          GST_LOG_OBJECT (thepad, "adding item %d to header list", i);

          gst_buffer_ref (buf);
          res = g_list_append (res, buf);
        }
      }
    } else {
      GST_LOG_OBJECT (thepad, "streamheader is not fixed list");
    }

  } else if (gst_structure_has_name (structure, "video/x-dirac")) {
    res = g_list_append (res, pad->buffer);
    pad->buffer = NULL;
  } else if (pad->have_type
      && (header = gst_ogg_stream_get_headers (&pad->map))) {
    res = g_list_append (res, header);
  } else {
    GST_LOG_OBJECT (thepad, "caps don't have streamheader");
  }
  gst_caps_unref (caps);

  return res;
}

static GstCaps *
gst_ogg_mux_set_header_on_caps (GstCaps * caps, GList * buffers)
{
  GstStructure *structure;
  GValue array = { 0 };
  GList *walk = buffers;

  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  /* put buffers in a fixed list */
  g_value_init (&array, GST_TYPE_ARRAY);

  while (walk) {
    GstBuffer *buf = GST_BUFFER (walk->data);
    GValue value = { 0 };

    walk = walk->next;

    /* mark buffer */
    GST_LOG ("Setting HEADER on buffer of length %" G_GSIZE_FORMAT,
        gst_buffer_get_size (buf));
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);

    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_set_buffer (&value, buf);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
  }
  gst_structure_take_value (structure, "streamheader", &array);

  return caps;
}

static void
gst_ogg_mux_create_header_packet_with_flags (ogg_packet * packet,
    gboolean bos, gboolean eos)
{
  packet->granulepos = 0;
  /* mark BOS and packet number */
  packet->b_o_s = bos;
  /* mark EOS */
  packet->e_o_s = eos;
}

static void
gst_ogg_mux_create_header_packet (ogg_packet * packet, GstOggPadData * pad)
{
  gst_ogg_mux_create_header_packet_with_flags (packet, pad->packetno == 0, 0);
  packet->packetno = pad->packetno++;
}

static void
gst_ogg_mux_submit_skeleton_header_packet (GstOggMux * mux,
    ogg_stream_state * os, GstBuffer * buf, gboolean bos, gboolean eos)
{
  ogg_packet packet;
  GstMapInfo map;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  packet.packet = map.data;
  packet.bytes = map.size;
  gst_ogg_mux_create_header_packet_with_flags (&packet, bos, eos);
  ogg_stream_packetin (os, &packet);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unref (buf);
}

static void
gst_ogg_mux_make_fishead (GstOggMux * mux, ogg_stream_state * os)
{
  GstByteWriter bw;
  GstBuffer *fishead;
  gboolean handled = TRUE;

  GST_DEBUG_OBJECT (mux, "Creating fishead");

  gst_byte_writer_init_with_size (&bw, 64, TRUE);
  handled &= gst_byte_writer_put_string_utf8 (&bw, "fishead");
  handled &= gst_byte_writer_put_int16_le (&bw, 3);     /* version major */
  handled &= gst_byte_writer_put_int16_le (&bw, 0);     /* version minor */
  handled &= gst_byte_writer_put_int64_le (&bw, 0);     /* presentation time numerator */
  handled &= gst_byte_writer_put_int64_le (&bw, 1000);  /* ...and denominator */
  handled &= gst_byte_writer_put_int64_le (&bw, 0);     /* base time numerator */
  handled &= gst_byte_writer_put_int64_le (&bw, 1000);  /* ...and denominator */
  handled &= gst_byte_writer_fill (&bw, ' ', 20);       /* UTC time */
  g_assert (handled && gst_byte_writer_get_pos (&bw) == 64);
  fishead = gst_byte_writer_reset_and_get_buffer (&bw);
  gst_ogg_mux_submit_skeleton_header_packet (mux, os, fishead, 1, 0);
}

static void
gst_ogg_mux_byte_writer_put_string_utf8 (GstByteWriter * bw, const char *s)
{
  if (!gst_byte_writer_put_data (bw, (const guint8 *) s, strlen (s)))
    GST_ERROR ("put_data failed");
}

static void
gst_ogg_mux_add_fisbone_message_header (GstOggMux * mux, GstByteWriter * bw,
    const char *tag, const char *value)
{
  /* It is valid to pass NULL as the value to omit the tag */
  if (!value)
    return;
  GST_DEBUG_OBJECT (mux, "Adding fisbone message header %s: %s", tag, value);
  gst_ogg_mux_byte_writer_put_string_utf8 (bw, tag);
  gst_ogg_mux_byte_writer_put_string_utf8 (bw, ": ");
  gst_ogg_mux_byte_writer_put_string_utf8 (bw, value);
  gst_ogg_mux_byte_writer_put_string_utf8 (bw, "\r\n");
}

static void
gst_ogg_mux_add_fisbone_message_header_from_tags (GstOggMux * mux,
    GstByteWriter * bw, const char *header, const char *tag,
    const GstTagList * tags)
{
  GString *s;
  guint size = gst_tag_list_get_tag_size (tags, tag), n;
  GST_DEBUG_OBJECT (mux, "Found %u tags for name %s", size, tag);
  if (size == 0)
    return;
  s = g_string_new ("");
  for (n = 0; n < size; ++n) {
    gchar *tmp;
    if (n)
      g_string_append (s, ", ");
    if (gst_tag_list_get_string_index (tags, tag, n, &tmp)) {
      g_string_append (s, tmp);
      g_free (tmp);
    } else {
      GST_WARNING_OBJECT (mux, "Tag %s index %u was not found (%u total)", tag,
          n, size);
    }
  }
  gst_ogg_mux_add_fisbone_message_header (mux, bw, header, s->str);
  g_string_free (s, TRUE);
}

/* This is a basic placeholder to generate roles for the tracks.
   For tracks with more than one video, both video tracks will get
   tagged with a "video/main" role, but we have no way of knowing
   which one is the main one, if any. We could just pick one. For
   audio, it's more complicated as we don't know which is music,
   which is dubbing, etc. For kate, we could take a pretty good
   guess based on the category, as role essentially is category.
   For now, leave this as is. */
static const char *
gst_ogg_mux_get_default_role (GstOggPadData * pad)
{
  const char *type = gst_ogg_stream_get_media_type (&pad->map);
  if (type) {
    if (!strncmp (type, "video/", strlen ("video/")))
      return "video/main";
    if (!strncmp (type, "audio/", strlen ("audio/")))
      return "audio/main";
    if (!strcmp (type + strlen (type) - strlen ("kate"), "kate"))
      return "text/caption";
  }
  return NULL;
}

static void
gst_ogg_mux_make_fisbone (GstOggMux * mux, ogg_stream_state * os,
    GstOggPadData * pad)
{
  GstByteWriter bw;
  gboolean handled = TRUE;

  GST_DEBUG_OBJECT (mux,
      "Creating %s fisbone for serial %08x",
      gst_ogg_stream_get_media_type (&pad->map), pad->map.serialno);

  gst_byte_writer_init (&bw);
  handled &= gst_byte_writer_put_string_utf8 (&bw, "fisbone");
  handled &= gst_byte_writer_put_int32_le (&bw, 44);    /* offset to message headers */
  handled &= gst_byte_writer_put_uint32_le (&bw, pad->map.serialno);
  handled &= gst_byte_writer_put_uint32_le (&bw, pad->map.n_header_packets);
  handled &= gst_byte_writer_put_uint64_le (&bw, pad->map.granulerate_n);
  handled &= gst_byte_writer_put_uint64_le (&bw, pad->map.granulerate_d);
  handled &= gst_byte_writer_put_uint64_le (&bw, 0);    /* base granule */
  handled &= gst_byte_writer_put_uint32_le (&bw, pad->map.preroll);
  handled &= gst_byte_writer_put_uint8 (&bw, pad->map.granuleshift);
  handled &= gst_byte_writer_fill (&bw, 0, 3);  /* padding */
  /* message header fields - MIME type for now */
  gst_ogg_mux_add_fisbone_message_header (mux, &bw, "Content-Type",
      gst_ogg_stream_get_media_type (&pad->map));
  gst_ogg_mux_add_fisbone_message_header (mux, &bw, "Role",
      gst_ogg_mux_get_default_role (pad));
  gst_ogg_mux_add_fisbone_message_header_from_tags (mux, &bw, "Language",
      GST_TAG_LANGUAGE_CODE, pad->tags);
  gst_ogg_mux_add_fisbone_message_header_from_tags (mux, &bw, "Title",
      GST_TAG_TITLE, pad->tags);

  if (G_UNLIKELY (!handled))
    GST_WARNING_OBJECT (mux, "Error writing fishbon");

  gst_ogg_mux_submit_skeleton_header_packet (mux, os,
      gst_byte_writer_reset_and_get_buffer (&bw), 0, 0);
}

static void
gst_ogg_mux_make_fistail (GstOggMux * mux, ogg_stream_state * os)
{
  GST_DEBUG_OBJECT (mux, "Creating fistail");

  gst_ogg_mux_submit_skeleton_header_packet (mux, os,
      gst_buffer_new_and_alloc (0), 0, 1);
}

/*
 * For each pad we need to write out one (small) header in one
 * page that allows decoders to identify the type of the stream.
 * After that we need to write out all extra info for the decoders.
 * In the case of a codec that also needs data as configuration, we can
 * find that info in the streamcaps.
 * After writing the headers we must start a new page for the data.
 */
static GstFlowReturn
gst_ogg_mux_send_headers (GstOggMux * mux)
{
  GSList *walk;
  GList *hbufs, *hwalk;
  GstCaps *caps;
  GstFlowReturn ret;
  ogg_page page;
  ogg_stream_state skeleton_stream;

  hbufs = NULL;
  ret = GST_FLOW_OK;

  GST_LOG_OBJECT (mux, "collecting headers");

  walk = mux->collect->data;
  while (walk) {
    GstOggPadData *pad;
    GstPad *thepad;

    pad = (GstOggPadData *) walk->data;
    thepad = pad->collect.pad;

    walk = g_slist_next (walk);

    GST_LOG_OBJECT (mux, "looking at pad %s:%s", GST_DEBUG_PAD_NAME (thepad));

    /* if the pad has no buffer and is not sparse, we don't care */
    if (pad->buffer == NULL && !pad->map.is_sparse)
      continue;

    /* now figure out the headers */
    pad->map.headers = gst_ogg_mux_get_headers (pad);
  }

  GST_LOG_OBJECT (mux, "creating BOS pages");
  walk = mux->collect->data;
  while (walk) {
    GstOggPadData *pad;
    GstBuffer *buf;
    ogg_packet packet;
    GstPad *thepad;
    GstBuffer *hbuf;
    GstMapInfo map;
    GstCaps *caps;
    const gchar *mime_type = "";

    pad = (GstOggPadData *) walk->data;
    thepad = pad->collect.pad;
    walk = walk->next;

    pad->packetno = 0;

    GST_LOG_OBJECT (thepad, "looping over headers");

    if (pad->map.headers) {
      buf = GST_BUFFER (pad->map.headers->data);
      pad->map.headers = g_list_remove (pad->map.headers, buf);
    } else if (pad->buffer) {
      buf = pad->buffer;
      gst_buffer_ref (buf);
    } else {
      /* fixme -- should be caught in the previous list traversal. */
      GST_OBJECT_LOCK (thepad);
      g_critical ("No headers or buffers on pad %s:%s",
          GST_DEBUG_PAD_NAME (thepad));
      GST_OBJECT_UNLOCK (thepad);
      continue;
    }

    if ((caps = gst_pad_get_current_caps (thepad))) {
      GstStructure *structure = gst_caps_get_structure (caps, 0);
      mime_type = gst_structure_get_name (structure);
    } else {
      GST_INFO_OBJECT (thepad, "got empty caps as negotiated format");
    }

    /* create a packet from the buffer */
    gst_buffer_map (buf, &map, GST_MAP_READ);
    packet.packet = map.data;
    packet.bytes = map.size;

    gst_ogg_mux_create_header_packet (&packet, pad);

    /* swap the packet in */
    ogg_stream_packetin (&pad->map.stream, &packet);

    gst_buffer_unmap (buf, &map);
    gst_buffer_unref (buf);

    GST_LOG_OBJECT (thepad, "flushing out BOS page");
    if (!ogg_stream_flush (&pad->map.stream, &page))
      g_critical ("Could not flush BOS page");

    hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);

    GST_LOG_OBJECT (mux, "swapped out page with mime type '%s'", mime_type);

    /* quick hack: put video pages at the front.
     * Ideally, we would have a settable enum for which Ogg
     * profile we work with, and order based on that.
     * (FIXME: if there is more than one video stream, shouldn't we only put
     * one's BOS into the first page, followed by an audio stream's BOS, and
     * only then followed by the remaining video and audio streams?) */
    if (pad->map.is_video) {
      GST_DEBUG_OBJECT (thepad, "putting %s page at the front", mime_type);
      hbufs = g_list_prepend (hbufs, hbuf);
    } else {
      hbufs = g_list_append (hbufs, hbuf);
    }

    if (caps) {
      gst_caps_unref (caps);
    }
  }

  /* The Skeleton BOS goes first - even before the video that went first before */
  if (mux->use_skeleton) {
    ogg_stream_init (&skeleton_stream, gst_ogg_mux_generate_serialno (mux));
    gst_ogg_mux_make_fishead (mux, &skeleton_stream);
    while (ogg_stream_flush (&skeleton_stream, &page) > 0) {
      GstBuffer *hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);
      hbufs = g_list_append (hbufs, hbuf);
    }
  }

  GST_LOG_OBJECT (mux, "creating next headers");
  walk = mux->collect->data;
  while (walk) {
    GstOggPadData *pad;
    GstPad *thepad;

    pad = (GstOggPadData *) walk->data;
    thepad = pad->collect.pad;

    walk = walk->next;

    if (mux->use_skeleton)
      gst_ogg_mux_make_fisbone (mux, &skeleton_stream, pad);

    GST_LOG_OBJECT (mux, "looping over headers for pad %s:%s",
        GST_DEBUG_PAD_NAME (thepad));

    hwalk = pad->map.headers;
    while (hwalk) {
      GstBuffer *buf = GST_BUFFER (hwalk->data);
      ogg_packet packet;
      ogg_page page;
      GstMapInfo map;

      hwalk = hwalk->next;

      /* create a packet from the buffer */
      gst_buffer_map (buf, &map, GST_MAP_READ);
      packet.packet = map.data;
      packet.bytes = map.size;

      gst_ogg_mux_create_header_packet (&packet, pad);

      /* swap the packet in */
      ogg_stream_packetin (&pad->map.stream, &packet);
      gst_buffer_unmap (buf, &map);
      gst_buffer_unref (buf);

      /* if last header, flush page */
      if (hwalk == NULL) {
        GST_LOG_OBJECT (mux,
            "flushing page as packet %" G_GUINT64_FORMAT " is first or "
            "last packet", (guint64) packet.packetno);
        while (ogg_stream_flush (&pad->map.stream, &page)) {
          GstBuffer *hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);

          GST_LOG_OBJECT (mux, "swapped out page");
          hbufs = g_list_append (hbufs, hbuf);
        }
      } else {
        GST_LOG_OBJECT (mux, "try to swap out page");
        /* just try to swap out a page then */
        while (ogg_stream_pageout (&pad->map.stream, &page) > 0) {
          GstBuffer *hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);

          GST_LOG_OBJECT (mux, "swapped out page");
          hbufs = g_list_append (hbufs, hbuf);
        }
      }
    }
    g_list_free (pad->map.headers);
    pad->map.headers = NULL;
  }

  if (mux->use_skeleton) {
    /* flush accumulated fisbones, the fistail must be on a separate page */
    while (ogg_stream_flush (&skeleton_stream, &page) > 0) {
      GstBuffer *hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);
      hbufs = g_list_append (hbufs, hbuf);
    }
    gst_ogg_mux_make_fistail (mux, &skeleton_stream);
    while (ogg_stream_flush (&skeleton_stream, &page) > 0) {
      GstBuffer *hbuf = gst_ogg_mux_buffer_from_page (mux, &page, FALSE);
      hbufs = g_list_append (hbufs, hbuf);
    }
    ogg_stream_clear (&skeleton_stream);
  }

  /* hbufs holds all buffers for the headers now */

  /* create caps with the buffers */
  /* FIXME: should prefer media type audio/ogg, video/ogg, etc. depending on
   * what we create, if acceptable downstream (instead of defaulting to
   * application/ogg because that's the first in the template caps) */
  caps = gst_pad_get_allowed_caps (mux->srcpad);
  if (caps) {
    if (!gst_caps_is_fixed (caps))
      caps = gst_caps_fixate (caps);
  }
  if (!caps)
    caps = gst_caps_new_empty_simple ("application/ogg");

  caps = gst_ogg_mux_set_header_on_caps (caps, hbufs);
  gst_pad_set_caps (mux->srcpad, caps);
  gst_caps_unref (caps);

  /* Send segment event */
  {
    GstSegment segment;
    gst_segment_init (&segment, GST_FORMAT_TIME);
    gst_pad_push_event (mux->srcpad, gst_event_new_segment (&segment));
  }

  /* and send the buffers */
  while (hbufs != NULL) {
    GstBuffer *buf = GST_BUFFER (hbufs->data);

    hbufs = g_list_delete_link (hbufs, hbufs);

    if ((ret = gst_ogg_mux_push_buffer (mux, buf, NULL)) != GST_FLOW_OK)
      break;
  }
  /* free any remaining nodes/buffers in case we couldn't push them */
  g_list_foreach (hbufs, (GFunc) gst_mini_object_unref, NULL);
  g_list_free (hbufs);

  return ret;
}

/* this function is called to process data on the best pending pad.
 *
 * basic idea:
 *
 * 1) store the selected pad and keep on pulling until we fill a
 *    complete ogg page or the ogg page is filled above the max-delay
 *    threshold. This is needed because the ogg spec says that
 *    you should fill a complete page with data from the same logical
 *    stream. When the page is filled, go back to 1).
 * 2) before filling a page, read ahead one more buffer to see if this
 *    packet is the last of the stream. We need to do this because the ogg
 *    spec mandates that the last packet should have the EOS flag set before
 *    sending it to ogg. if pad->buffer is NULL we need to wait to find out
 *    whether there are any more buffers.
 * 3) pages get queued on a per-pad queue. Every time a page is queued, a
 *    dequeue is called, which will dequeue the oldest page on any pad, provided
 *    that ALL pads have at least one marked page in the queue (or remaining
 *    pads are at EOS)
 */
static GstFlowReturn
gst_ogg_mux_process_best_pad (GstOggMux * ogg_mux, GstOggPadData * best)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean delta_unit;
  gint64 granulepos = 0;
  GstClockTime timestamp, gp_time;
  GstBuffer *next_buf;

  GST_LOG_OBJECT (ogg_mux, "best pad %" GST_PTR_FORMAT
      ", currently pulling from %" GST_PTR_FORMAT, best->collect.pad,
      ogg_mux->pulling ? ogg_mux->pulling->collect.pad : NULL);

  if (ogg_mux->pulling) {
    next_buf = gst_collect_pads_peek (ogg_mux->collect,
        &ogg_mux->pulling->collect);
    if (next_buf) {
      ogg_mux->pulling->eos = FALSE;
      gst_buffer_unref (next_buf);
    } else if (!ogg_mux->pulling->map.is_sparse) {
      GST_DEBUG_OBJECT (ogg_mux->pulling->collect.pad, "setting eos to true");
      ogg_mux->pulling->eos = TRUE;
    }
  }

  /* We could end up pushing from the best pad instead, so check that
   * as well */
  if (best && best != ogg_mux->pulling) {
    next_buf = gst_collect_pads_peek (ogg_mux->collect, &best->collect);
    if (next_buf) {
      best->eos = FALSE;
      gst_buffer_unref (next_buf);
    } else if (!best->map.is_sparse) {
      GST_DEBUG_OBJECT (best->collect.pad, "setting eos to true");
      best->eos = TRUE;
    }
  }

  /* if we were already pulling from one pad, but the new "best" buffer is
   * from another pad, we need to check if we have reason to flush a page
   * for the pad we were pulling from before */
  if (ogg_mux->pulling && best &&
      ogg_mux->pulling != best && ogg_mux->pulling->buffer) {
    GstOggPadData *pad = ogg_mux->pulling;
    GstClockTime last_ts = GST_BUFFER_END_TIME (pad->buffer);

    /* if the next packet in the current page is going to make the page
     * too long, we need to flush */
    if (last_ts > ogg_mux->next_ts + ogg_mux->max_delay) {
      ogg_page page;

      GST_LOG_OBJECT (pad->collect.pad,
          GST_GP_FORMAT " stored packet %" G_GINT64_FORMAT
          " will make page too long, flushing",
          GST_BUFFER_OFFSET_END (pad->buffer),
          (gint64) pad->map.stream.packetno);

      while (ogg_stream_flush (&pad->map.stream, &page)) {
        /* end time of this page is the timestamp of the next buffer */
        ogg_mux->pulling->timestamp_end = GST_BUFFER_TIMESTAMP (pad->buffer);
        /* Place page into the per-pad queue */
        ret = gst_ogg_mux_pad_queue_page (ogg_mux, pad, &page,
            pad->first_delta);
        /* increment the page number counter */
        pad->pageno++;
        /* mark other pages as delta */
        pad->first_delta = TRUE;
      }
      pad->new_page = TRUE;
      ogg_mux->pulling = NULL;
    }
  }

  /* if we don't know which pad to pull on, use the best one */
  if (ogg_mux->pulling == NULL) {
    ogg_mux->pulling = best;
    GST_LOG_OBJECT (ogg_mux->pulling->collect.pad, "pulling from best pad");

    /* remember timestamp and gp time of first buffer for this new pad */
    if (ogg_mux->pulling != NULL) {
      ogg_mux->next_ts = GST_BUFFER_TIMESTAMP (ogg_mux->pulling->buffer);
      GST_LOG_OBJECT (ogg_mux->pulling->collect.pad, "updated times, next ts %"
          GST_TIME_FORMAT, GST_TIME_ARGS (ogg_mux->next_ts));
    } else {
      GST_LOG_OBJECT (ogg_mux->srcpad, "sending EOS");
      /* no pad to pull on, send EOS */
      gst_pad_push_event (ogg_mux->srcpad, gst_event_new_eos ());
      return GST_FLOW_FLUSHING;
    }
  }

  if (ogg_mux->need_headers) {
    ret = gst_ogg_mux_send_headers (ogg_mux);
    ogg_mux->need_headers = FALSE;
  }

  /* we are pulling from a pad, continue to do so until a page
   * has been filled and queued */
  if (ogg_mux->pulling != NULL) {
    ogg_packet packet;
    ogg_page page;
    GstBuffer *buf, *tmpbuf;
    GstOggPadData *pad = ogg_mux->pulling;
    gint64 duration;
    gboolean force_flush;
    GstMapInfo map;

    GST_LOG_OBJECT (ogg_mux->pulling->collect.pad, "pulling from pad");

    /* now see if we have a buffer */
    buf = pad->buffer;
    if (buf == NULL) {
      GST_DEBUG_OBJECT (ogg_mux, "pad was EOS");
      ogg_mux->pulling = NULL;
      return GST_FLOW_OK;
    }

    delta_unit = GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
    duration = GST_BUFFER_DURATION (buf);

    /* if the current "next timestamp" on the pad is unset, then this is the
     * first packet on the new page.  Update our pad's page timestamp */
    if (ogg_mux->pulling->timestamp == GST_CLOCK_TIME_NONE) {
      ogg_mux->pulling->timestamp = GST_BUFFER_TIMESTAMP (buf);
      GST_LOG_OBJECT (ogg_mux->pulling->collect.pad,
          "updated pad timestamp to %" GST_TIME_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));
    }
    /* create a packet from the buffer */
    gst_buffer_map (buf, &map, GST_MAP_READ);
    packet.packet = map.data;
    packet.bytes = map.size;
    packet.granulepos = GST_BUFFER_OFFSET_END (buf);
    if (packet.granulepos == -1)
      packet.granulepos = 0;
    /* mark BOS and packet number */
    packet.b_o_s = (pad->packetno == 0);
    packet.packetno = pad->packetno++;
    GST_LOG_OBJECT (pad->collect.pad, GST_GP_FORMAT
        " packet %" G_GINT64_FORMAT " (%ld bytes) created from buffer",
        GST_GP_CAST (packet.granulepos), (gint64) packet.packetno,
        packet.bytes);

    packet.e_o_s = ogg_mux->pulling->eos ? 1 : 0;
    tmpbuf = NULL;

    /* we flush when we see a new keyframe */
    force_flush = (pad->prev_delta && !delta_unit)
        || pad->map.always_flush_page;
    if (duration != -1) {
      pad->duration += duration;
      /* if page duration exceeds max, flush page */
      if (pad->duration > ogg_mux->max_page_delay) {
        force_flush = TRUE;
        pad->duration = 0;
      }
    }

    if (GST_BUFFER_IS_DISCONT (buf)) {
      if (pad->data_pushed) {
        GST_LOG_OBJECT (pad->collect.pad, "got discont");
        packet.packetno++;
        /* No public API for this; hack things in */
        pad->map.stream.pageno++;
        force_flush = TRUE;
      } else {
        GST_LOG_OBJECT (pad->collect.pad, "discont at stream start");
      }
    }

    /* flush the currently built page if necessary */
    if (force_flush) {
      GST_LOG_OBJECT (pad->collect.pad,
          GST_GP_FORMAT " forced flush of page before this packet",
          GST_BUFFER_OFFSET_END (pad->buffer));
      while (ogg_stream_flush (&pad->map.stream, &page)) {
        /* end time of this page is the timestamp of the next buffer */
        ogg_mux->pulling->timestamp_end = GST_BUFFER_TIMESTAMP (pad->buffer);
        ret = gst_ogg_mux_pad_queue_page (ogg_mux, pad, &page,
            pad->first_delta);

        /* increment the page number counter */
        pad->pageno++;
        /* mark other pages as delta */
        pad->first_delta = TRUE;
      }
      pad->new_page = TRUE;
    }

    /* if this is the first packet of a new page figure out the delta flag */
    if (pad->new_page) {
      if (delta_unit) {
        /* mark the page as delta */
        pad->first_delta = TRUE;
      } else {
        /* got a keyframe */
        if (ogg_mux->delta_pad == pad) {
          /* if we get it on the pad with deltaunits,
           * we mark the page as non delta */
          pad->first_delta = FALSE;
        } else if (ogg_mux->delta_pad != NULL) {
          /* if there are pads with delta frames, we
           * must mark this one as delta */
          pad->first_delta = TRUE;
        } else {
          pad->first_delta = FALSE;
        }
      }
      pad->new_page = FALSE;
    }

    /* save key unit to track delta->key unit transitions */
    pad->prev_delta = delta_unit;

    /* swap the packet in */
    if (packet.e_o_s == 1)
      GST_DEBUG_OBJECT (pad->collect.pad, "swapping in EOS packet");
    if (packet.b_o_s == 1)
      GST_DEBUG_OBJECT (pad->collect.pad, "swapping in BOS packet");

    ogg_stream_packetin (&pad->map.stream, &packet);
    gst_buffer_unmap (buf, &map);
    pad->data_pushed = TRUE;

    gp_time = GST_BUFFER_OFFSET (pad->buffer);
    granulepos = GST_BUFFER_OFFSET_END (pad->buffer);
    timestamp = GST_BUFFER_TIMESTAMP (pad->buffer);

    GST_LOG_OBJECT (pad->collect.pad,
        GST_GP_FORMAT " packet %" G_GINT64_FORMAT ", gp time %"
        GST_TIME_FORMAT ", timestamp %" GST_TIME_FORMAT " packetin'd",
        granulepos, (gint64) packet.packetno, GST_TIME_ARGS (gp_time),
        GST_TIME_ARGS (timestamp));
    /* don't need the old buffer anymore */
    gst_buffer_unref (pad->buffer);
    /* store new readahead buffer */
    pad->buffer = tmpbuf;

    /* let ogg write out the pages now. The packet we got could end
     * up in more than one page so we need to write them all */
    if (ogg_stream_pageout (&pad->map.stream, &page) > 0) {
      /* we have a new page, so we need to timestamp it correctly.
       * if this fresh packet ends on this page, then the page's granulepos
       * comes from that packet, and we should set this buffer's timestamp */

      GST_LOG_OBJECT (pad->collect.pad,
          GST_GP_FORMAT " packet %" G_GINT64_FORMAT ", time %"
          GST_TIME_FORMAT ") caused new page",
          granulepos, (gint64) packet.packetno, GST_TIME_ARGS (timestamp));
      GST_LOG_OBJECT (pad->collect.pad,
          GST_GP_FORMAT " new page %ld",
          GST_GP_CAST (ogg_page_granulepos (&page)), pad->map.stream.pageno);

      if (ogg_page_granulepos (&page) == granulepos) {
        /* the packet we streamed in finishes on the current page,
         * because the page's granulepos is the granulepos of the last
         * packet completed on that page,
         * so update the timestamp that we will give to the page */
        GST_LOG_OBJECT (pad->collect.pad,
            GST_GP_FORMAT
            " packet finishes on current page, updating gp time to %"
            GST_TIME_FORMAT, granulepos, GST_TIME_ARGS (gp_time));
        pad->gp_time = gp_time;
      } else {
        GST_LOG_OBJECT (pad->collect.pad,
            GST_GP_FORMAT
            " packet spans beyond current page, keeping old gp time %"
            GST_TIME_FORMAT, granulepos, GST_TIME_ARGS (pad->gp_time));
      }

      /* push the page */
      /* end time of this page is the timestamp of the next buffer */
      pad->timestamp_end = timestamp;
      ret = gst_ogg_mux_pad_queue_page (ogg_mux, pad, &page, pad->first_delta);
      pad->pageno++;
      /* mark next pages as delta */
      pad->first_delta = TRUE;

      /* use an inner loop here to flush the remaining pages and
       * mark them as delta frames as well */
      while (ogg_stream_pageout (&pad->map.stream, &page) > 0) {
        if (ogg_page_granulepos (&page) == granulepos) {
          /* the page has taken up the new packet completely, which means
           * the packet ends the page and we can update the gp time
           * before pushing out */
          pad->gp_time = gp_time;
        }

        /* we have a complete page now, we can push the page
         * and make sure to pull on a new pad the next time around */
        ret = gst_ogg_mux_pad_queue_page (ogg_mux, pad, &page,
            pad->first_delta);
        /* increment the page number counter */
        pad->pageno++;
      }
      /* need a new page as well */
      pad->new_page = TRUE;
      pad->duration = 0;
      /* we're done pulling on this pad, make sure to choose a new
       * pad for pulling in the next iteration */
      ogg_mux->pulling = NULL;
    }

    /* Update the gp time, if necessary, since any future page will have at
     * least this gp time.
     */
    if (pad->gp_time < gp_time) {
      pad->gp_time = gp_time;
      GST_LOG_OBJECT (pad->collect.pad,
          "Updated running gp time of pad %" GST_PTR_FORMAT
          " to %" GST_TIME_FORMAT, pad->collect.pad, GST_TIME_ARGS (gp_time));
    }
  }

  return ret;
}

/* all_pads_eos:
 *
 * Checks if all pads are EOS'd by peeking.
 *
 * Returns TRUE if all pads are EOS.
 */
static gboolean
all_pads_eos (GstCollectPads * pads)
{
  GSList *walk;

  walk = pads->data;
  while (walk) {
    GstOggPadData *oggpad = (GstOggPadData *) walk->data;

    GST_DEBUG_OBJECT (oggpad->collect.pad,
        "oggpad %p eos %d", oggpad, oggpad->eos);

    if (!oggpad->eos)
      return FALSE;

    walk = g_slist_next (walk);
  }

  return TRUE;
}

static void
gst_ogg_mux_send_start_events (GstOggMux * ogg_mux, GstCollectPads * pads)
{
  gchar s_id[32];

  /* stream-start (FIXME: create id based on input ids) and
   * also do something with the group id */
  g_snprintf (s_id, sizeof (s_id), "oggmux-%08x", g_random_int ());
  gst_pad_push_event (ogg_mux->srcpad, gst_event_new_stream_start (s_id));

  /* we'll send caps later, need to collect all headers first */
}

/* This function is called when there is data on all pads.
 *
 * It finds a pad to pull on, this is done by looking at the buffers
 * to decide which one to use, and using the 'oldest' one first. It then calls
 * gst_ogg_mux_process_best_pad() to process as much data as possible.
 *
 * If all the pads have received EOS, it flushes out all data by continually
 * getting the best pad and calling gst_ogg_mux_process_best_pad() until they
 * are all empty, and then sends EOS.
 */
static GstFlowReturn
gst_ogg_mux_collected (GstCollectPads * pads, GstOggMux * ogg_mux)
{
  GstOggPadData *best;
  GstFlowReturn ret;
  gboolean popped;

  GST_LOG_OBJECT (ogg_mux, "collected");

  if (ogg_mux->need_start_events) {
    gst_ogg_mux_send_start_events (ogg_mux, pads);
    ogg_mux->need_start_events = FALSE;
  }

  /* queue buffers on all pads; find a buffer with the lowest timestamp */
  best = gst_ogg_mux_queue_pads (ogg_mux, &popped);

  if (popped)
    return GST_FLOW_OK;

  if (best == NULL) {
    /* No data, assume EOS */
    goto eos;
  }

  /* This is not supposed to happen */
  g_return_val_if_fail (best->buffer != NULL, GST_FLOW_ERROR);

  ret = gst_ogg_mux_process_best_pad (ogg_mux, best);

  if (best->eos && all_pads_eos (pads))
    goto eos;

  /* We might have used up a cached pad->buffer. If all streams
   * have a buffer ready in collectpads, collectpads will block at
   * next chain, and will never call collected again. So we make a
   * last call to _queue_pads now, to ensure that collectpads can
   * push to at least one pad (mostly for streams with a single
   * logical stream). */
  gst_ogg_mux_queue_pads (ogg_mux, &popped);

  return ret;

eos:
  {
    GST_DEBUG_OBJECT (ogg_mux, "no data available, must be EOS");
    gst_pad_push_event (ogg_mux->srcpad, gst_event_new_eos ());
    return GST_FLOW_EOS;
  }
}

static void
gst_ogg_mux_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOggMux *ogg_mux;

  ogg_mux = GST_OGG_MUX (object);

  switch (prop_id) {
    case ARG_MAX_DELAY:
      g_value_set_uint64 (value, ogg_mux->max_delay);
      break;
    case ARG_MAX_PAGE_DELAY:
      g_value_set_uint64 (value, ogg_mux->max_page_delay);
      break;
    case ARG_MAX_TOLERANCE:
      g_value_set_uint64 (value, ogg_mux->max_tolerance);
      break;
    case ARG_SKELETON:
      g_value_set_boolean (value, ogg_mux->use_skeleton);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ogg_mux_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOggMux *ogg_mux;

  ogg_mux = GST_OGG_MUX (object);

  switch (prop_id) {
    case ARG_MAX_DELAY:
      ogg_mux->max_delay = g_value_get_uint64 (value);
      break;
    case ARG_MAX_PAGE_DELAY:
      ogg_mux->max_page_delay = g_value_get_uint64 (value);
      break;
    case ARG_MAX_TOLERANCE:
      ogg_mux->max_tolerance = g_value_get_uint64 (value);
      break;
    case ARG_SKELETON:
      ogg_mux->use_skeleton = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* reset all variables in the ogg pads. */
static void
gst_ogg_mux_init_collectpads (GstCollectPads * collect)
{
  GSList *walk;

  walk = collect->data;
  while (walk) {
    GstOggPadData *oggpad = (GstOggPadData *) walk->data;

    ogg_stream_clear (&oggpad->map.stream);
    ogg_stream_init (&oggpad->map.stream, oggpad->map.serialno);
    oggpad->packetno = 0;
    oggpad->pageno = 0;
    oggpad->eos = FALSE;
    /* we assume there will be some control data first for this pad */
    oggpad->state = GST_OGG_PAD_STATE_CONTROL;
    oggpad->new_page = TRUE;
    oggpad->first_delta = FALSE;
    oggpad->prev_delta = FALSE;
    oggpad->data_pushed = FALSE;
    oggpad->pagebuffers = g_queue_new ();

    gst_segment_init (&oggpad->segment, GST_FORMAT_TIME);

    walk = g_slist_next (walk);
  }
}

/* Clear all buffers from the collectpads object */
static void
gst_ogg_mux_clear_collectpads (GstCollectPads * collect)
{
  GSList *walk;

  for (walk = collect->data; walk; walk = g_slist_next (walk)) {
    GstOggPadData *oggpad = (GstOggPadData *) walk->data;
    GstBuffer *buf;

    ogg_stream_clear (&oggpad->map.stream);

    while ((buf = g_queue_pop_head (oggpad->pagebuffers)) != NULL) {
      GST_LOG ("flushing buffer : %p", buf);
      gst_buffer_unref (buf);
    }
    g_queue_free (oggpad->pagebuffers);
    oggpad->pagebuffers = NULL;

    if (oggpad->buffer) {
      gst_buffer_unref (oggpad->buffer);
      oggpad->buffer = NULL;
    }

    if (oggpad->tags) {
      gst_tag_list_unref (oggpad->tags);
      oggpad->tags = NULL;
    }

    gst_segment_init (&oggpad->segment, GST_FORMAT_TIME);
  }
}

static GstStateChangeReturn
gst_ogg_mux_change_state (GstElement * element, GstStateChange transition)
{
  GstOggMux *ogg_mux;
  GstStateChangeReturn ret;

  ogg_mux = GST_OGG_MUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_ogg_mux_clear (ogg_mux);
      gst_ogg_mux_init_collectpads (ogg_mux->collect);
      gst_collect_pads_start (ogg_mux->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (ogg_mux->collect);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_ogg_mux_clear_collectpads (ogg_mux->collect);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}
