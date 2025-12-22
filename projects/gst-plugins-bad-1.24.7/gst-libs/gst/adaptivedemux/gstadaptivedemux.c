/* GStreamer
 *
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
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
 * SECTION:gstadaptivedemux
 * @short_description: Base class for adaptive demuxers
 *
 * What is an adaptive demuxer?
 * Adaptive demuxers are special demuxers in the sense that they don't
 * actually demux data received from upstream but download the data
 * themselves.
 *
 * Adaptive formats (HLS, DASH, MSS) are composed of a manifest file and
 * a set of fragments. The manifest describes the available media and
 * the sequence of fragments to use. Each fragment contains a small
 * part of the media (typically only a few seconds). It is possible for
 * the manifest to have the same media available in different configurations
 * (bitrates for example) so that the client can select the one that
 * best suits its scenario (network fluctuation, hardware requirements...).
 * It is possible to switch from one representation of the media to another
 * during playback. That's why it is called 'adaptive', because it can be
 * adapted to the client's needs.
 *
 * Architectural overview:
 * The manifest is received by the demuxer in its sink pad and, upon receiving
 * EOS, it parses the manifest and exposes the streams available in it. For
 * each stream a source element will be created and will download the list
 * of fragments one by one. Once a fragment is finished downloading, the next
 * URI is set to the source element and it starts fetching it and pushing
 * through the stream's pad. This implies that each stream is independent from
 * each other as it runs on a separate thread.
 *
 * After downloading each fragment, the download rate of it is calculated and
 * the demuxer has a chance to switch to a different bitrate if needed. The
 * switch can be done by simply pushing a new caps before the next fragment
 * when codecs are the same, or by exposing a new pad group if it needs
 * a codec change.
 *
 * Extra features:
 * - Not linked streams: Streams that are not-linked have their download threads
 *                       interrupted to save network bandwidth. When they are
 *                       relinked a reconfigure event is received and the
 *                       stream is restarted.
 *
 * Subclasses:
 * While GstAdaptiveDemux is responsible for the workflow, it knows nothing
 * about the intrinsics of the subclass formats, so the subclasses are
 * responsible for maintaining the manifest data structures and stream
 * information.
 */

/*
MT safety.
The following rules were observed while implementing MT safety in adaptive demux:
1. If a variable is accessed from multiple threads and at least one thread
writes to it, then all the accesses needs to be done from inside a critical section.
2. If thread A wants to join thread B then at the moment it calls gst_task_join
it must not hold any mutexes that thread B might take.

Adaptive demux API can be called from several threads. More, adaptive demux
starts some threads to monitor the download of fragments. In order to protect
accesses to shared variables (demux and streams) all the API functions that
can be run in different threads will need to get a mutex (manifest_lock)
when they start and release it when they end. Because some of those functions
can indirectly call other API functions (eg they can generate events or messages
that are processed in the same thread) the manifest_lock must be recursive.

The manifest_lock will serialize the public API making access to shared
variables safe. But some of these functions will try at some moment to join
threads created by adaptive demux, or to change the state of src elements
(which will block trying to join the src element streaming thread). Because
of rule 2, those functions will need to release the manifest_lock during the
call of gst_task_join. During this time they can be interrupted by other API calls.
For example, during the precessing of a seek event, gst_adaptive_demux_stop_tasks
is called and this will join all threads. In order to prevent interruptions
during such period, all the API functions will also use a second lock: api_lock.
This will be taken at the beginning of the function and released at the end,
but this time this lock will not be temporarily released during join.
This lock will be used only by API calls (not by gst_adaptive_demux_stream_download_loop
or gst_adaptive_demux_updates_loop or _src_chain or _src_event) so it is safe
to hold it while joining the threads or changing the src element state. The
api_lock will serialise all external requests to adaptive demux. In order to
avoid deadlocks, if a function needs to acquire both manifest and api locks,
the api_lock will be taken first and the manifest_lock second.

By using the api_lock a thread is protected against other API calls. But when
temporarily dropping the manifest_lock, it will be vulnerable to changes from
threads that use only the manifest_lock and not the api_lock. These threads run
one of the following functions: gst_adaptive_demux_stream_download_loop,
gst_adaptive_demux_updates_loop, _src_chain, _src_event. In order to guarantee
that all operations during an API call are not impacted by other writes, the
above mentioned functions must check a cancelled flag every time they reacquire
the manifest_lock. If the flag is set, they must exit immediately, without
performing any changes on the shared data. In this way, an API call (eg seek
request) can set the cancel flag before releasing the manifest_lock and be sure
that the demux object and its streams are not changed by anybody else.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstadaptivedemux.h"
#include <glib/gi18n-lib.h>
#include <gst/base/gstadapter.h>

GST_DEBUG_CATEGORY (adaptivedemux_debug);
#define GST_CAT_DEFAULT adaptivedemux_debug

#define MAX_DOWNLOAD_ERROR_COUNT 3
#define DEFAULT_FAILED_COUNT 3
#define DEFAULT_CONNECTION_SPEED 0
#define DEFAULT_BITRATE_LIMIT 0.8f
#define SRC_QUEUE_MAX_BYTES 20 * 1024 * 1024    /* For safety. Large enough to hold a segment. */
#define NUM_LOOKBACK_FRAGMENTS 3

#define GST_MANIFEST_GET_LOCK(d) (&(GST_ADAPTIVE_DEMUX_CAST(d)->priv->manifest_lock))
#define GST_MANIFEST_LOCK(d) G_STMT_START { \
    GST_TRACE("Locking from thread %p", g_thread_self()); \
    g_rec_mutex_lock (GST_MANIFEST_GET_LOCK (d)); \
    GST_TRACE("Locked from thread %p", g_thread_self()); \
 } G_STMT_END

#define GST_MANIFEST_UNLOCK(d) G_STMT_START { \
    GST_TRACE("Unlocking from thread %p", g_thread_self()); \
    g_rec_mutex_unlock (GST_MANIFEST_GET_LOCK (d)); \
 } G_STMT_END

#define GST_API_GET_LOCK(d) (&(GST_ADAPTIVE_DEMUX_CAST(d)->priv->api_lock))
#define GST_API_LOCK(d)   g_mutex_lock (GST_API_GET_LOCK (d));
#define GST_API_UNLOCK(d) g_mutex_unlock (GST_API_GET_LOCK (d));

#define GST_ADAPTIVE_DEMUX_SEGMENT_GET_LOCK(d) (&GST_ADAPTIVE_DEMUX_CAST(d)->priv->segment_lock)
#define GST_ADAPTIVE_DEMUX_SEGMENT_LOCK(d) g_mutex_lock (GST_ADAPTIVE_DEMUX_SEGMENT_GET_LOCK (d))
#define GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK(d) g_mutex_unlock (GST_ADAPTIVE_DEMUX_SEGMENT_GET_LOCK (d))

enum
{
  PROP_0,
  PROP_CONNECTION_SPEED,
  PROP_BITRATE_LIMIT,
  PROP_LAST
};

/* Internal, so not using GST_FLOW_CUSTOM_SUCCESS_N */
#define GST_ADAPTIVE_DEMUX_FLOW_SWITCH (GST_FLOW_CUSTOM_SUCCESS_2 + 1)

struct _GstAdaptiveDemuxPrivate
{
  GstAdapter *input_adapter;    /* protected by manifest_lock */
  gint have_manifest;           /* MT safe */

  GList *old_streams;           /* protected by manifest_lock */

  GstTask *updates_task;        /* MT safe */
  GRecMutex updates_lock;
  GMutex updates_timed_lock;
  GCond updates_timed_cond;     /* protected by updates_timed_lock */
  gboolean stop_updates_task;   /* protected by updates_timed_lock */

  /* used only from updates_task, no need to protect it */
  gint update_failed_count;

  guint32 segment_seqnum;       /* protected by manifest_lock */

  /* main lock used to protect adaptive demux and all its streams.
   * It serializes the adaptive demux public API.
   */
  GRecMutex manifest_lock;

  /* condition to wait for manifest updates on a live stream.
   * In order to signal the manifest_cond, the caller needs to hold both
   * manifest_lock and manifest_update_lock (taken in this order)
   */
  GCond manifest_cond;
  GMutex manifest_update_lock;

  /* Lock and condition for prerolling streams before exposing */
  GMutex preroll_lock;
  GCond preroll_cond;
  gint preroll_pending;

  GMutex api_lock;

  /* Protects demux and stream segment information
   * Needed because seeks can update segment information
   * without needing to stop tasks when they just want to
   * update the segment boundaries */
  GMutex segment_lock;

  GstClockTime qos_earliest_time;
};

typedef struct _GstAdaptiveDemuxTimer
{
  gint ref_count;
  GCond *cond;
  GMutex *mutex;
  GstClockID clock_id;
  gboolean fired;
} GstAdaptiveDemuxTimer;

static GstBinClass *parent_class = NULL;
static gint private_offset = 0;

static void gst_adaptive_demux_class_init (GstAdaptiveDemuxClass * klass);
static void gst_adaptive_demux_init (GstAdaptiveDemux * dec,
    GstAdaptiveDemuxClass * klass);
static void gst_adaptive_demux_finalize (GObject * object);
static GstStateChangeReturn gst_adaptive_demux_change_state (GstElement *
    element, GstStateChange transition);

static void gst_adaptive_demux_handle_message (GstBin * bin, GstMessage * msg);

static gboolean gst_adaptive_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_adaptive_demux_sink_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buffer);
static gboolean gst_adaptive_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_adaptive_demux_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);

static gboolean
gst_adaptive_demux_push_src_event (GstAdaptiveDemux * demux, GstEvent * event);

static void gst_adaptive_demux_updates_loop (GstAdaptiveDemux * demux);
static void gst_adaptive_demux_stream_download_loop (GstAdaptiveDemuxStream *
    stream);
static void gst_adaptive_demux_reset (GstAdaptiveDemux * demux);
static gboolean gst_adaptive_demux_prepare_streams (GstAdaptiveDemux * demux,
    gboolean first_and_live);
static gboolean gst_adaptive_demux_expose_streams (GstAdaptiveDemux * demux);
static gboolean gst_adaptive_demux_is_live (GstAdaptiveDemux * demux);
static GstFlowReturn gst_adaptive_demux_stream_seek (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, gboolean forward, GstSeekFlags flags,
    GstClockTime ts, GstClockTime * final_ts);
static gboolean gst_adaptive_demux_stream_has_next_fragment (GstAdaptiveDemux *
    demux, GstAdaptiveDemuxStream * stream);
static gboolean gst_adaptive_demux_stream_select_bitrate (GstAdaptiveDemux *
    demux, GstAdaptiveDemuxStream * stream, guint64 bitrate);
static GstFlowReturn
gst_adaptive_demux_stream_update_fragment_info (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static gint64
gst_adaptive_demux_stream_get_fragment_waiting_time (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static GstFlowReturn gst_adaptive_demux_update_manifest (GstAdaptiveDemux *
    demux);
static GstFlowReturn
gst_adaptive_demux_update_manifest_default (GstAdaptiveDemux * demux);
static gboolean gst_adaptive_demux_has_next_period (GstAdaptiveDemux * demux);
static void gst_adaptive_demux_advance_period (GstAdaptiveDemux * demux);

static void gst_adaptive_demux_stream_free (GstAdaptiveDemuxStream * stream);
static gboolean
gst_adaptive_demux_stream_push_event (GstAdaptiveDemuxStream * stream,
    GstEvent * event);

static void gst_adaptive_demux_stop_manifest_update_task (GstAdaptiveDemux *
    demux);
static void gst_adaptive_demux_start_manifest_update_task (GstAdaptiveDemux *
    demux);

static void gst_adaptive_demux_start_tasks (GstAdaptiveDemux * demux,
    gboolean start_preroll_streams);
static void gst_adaptive_demux_stop_tasks (GstAdaptiveDemux * demux,
    gboolean stop_updates);
static GstFlowReturn gst_adaptive_demux_combine_flows (GstAdaptiveDemux *
    demux);
static void
gst_adaptive_demux_stream_fragment_download_finish (GstAdaptiveDemuxStream *
    stream, GstFlowReturn ret, GError * err);
static GstFlowReturn
gst_adaptive_demux_stream_data_received_default (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer);
static GstFlowReturn
gst_adaptive_demux_stream_finish_fragment_default (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static GstFlowReturn
gst_adaptive_demux_stream_advance_fragment_unlocked (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstClockTime duration);
static gboolean
gst_adaptive_demux_wait_until (GstClock * clock, GCond * cond, GMutex * mutex,
    GstClockTime end_time);
static gboolean gst_adaptive_demux_clock_callback (GstClock * clock,
    GstClockTime time, GstClockID id, gpointer user_data);
static gboolean
gst_adaptive_demux_requires_periodical_playlist_update_default (GstAdaptiveDemux
    * demux);

/* we can't use G_DEFINE_ABSTRACT_TYPE because we need the klass in the _init
 * method to get to the padtemplates */
GType
gst_adaptive_demux_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstAdaptiveDemuxClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_adaptive_demux_class_init,
      NULL,
      NULL,
      sizeof (GstAdaptiveDemux),
      0,
      (GInstanceInitFunc) gst_adaptive_demux_init,
    };

    _type = g_type_register_static (GST_TYPE_BIN,
        "GstAdaptiveDemux", &info, G_TYPE_FLAG_ABSTRACT);

    private_offset =
        g_type_add_instance_private (_type, sizeof (GstAdaptiveDemuxPrivate));

    g_once_init_leave (&type, _type);
  }
  return type;
}

static inline GstAdaptiveDemuxPrivate *
gst_adaptive_demux_get_instance_private (GstAdaptiveDemux * self)
{
  return (G_STRUCT_MEMBER_P (self, private_offset));
}

static void
gst_adaptive_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX (object);

  GST_API_LOCK (demux);
  GST_MANIFEST_LOCK (demux);

  switch (prop_id) {
    case PROP_CONNECTION_SPEED:
      demux->connection_speed = g_value_get_uint (value) * 1000;
      GST_DEBUG_OBJECT (demux, "Connection speed set to %u",
          demux->connection_speed);
      break;
    case PROP_BITRATE_LIMIT:
      demux->bitrate_limit = g_value_get_float (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_MANIFEST_UNLOCK (demux);
  GST_API_UNLOCK (demux);
}

static void
gst_adaptive_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX (object);

  GST_MANIFEST_LOCK (demux);

  switch (prop_id) {
    case PROP_CONNECTION_SPEED:
      g_value_set_uint (value, demux->connection_speed / 1000);
      break;
    case PROP_BITRATE_LIMIT:
      g_value_set_float (value, demux->bitrate_limit);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_MANIFEST_UNLOCK (demux);
}

static void
gst_adaptive_demux_class_init (GstAdaptiveDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbin_class = GST_BIN_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (adaptivedemux_debug, "adaptivedemux", 0,
      "Base Adaptive Demux");

  parent_class = g_type_class_peek_parent (klass);

  if (private_offset != 0)
    g_type_class_adjust_private_offset (klass, &private_offset);

  gobject_class->set_property = gst_adaptive_demux_set_property;
  gobject_class->get_property = gst_adaptive_demux_get_property;
  gobject_class->finalize = gst_adaptive_demux_finalize;

  g_object_class_install_property (gobject_class, PROP_CONNECTION_SPEED,
      g_param_spec_uint ("connection-speed", "Connection Speed",
          "Network connection speed in kbps (0 = calculate from downloaded"
          " fragments)", 0, G_MAXUINT / 1000, DEFAULT_CONNECTION_SPEED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* FIXME 2.0: rename this property to bandwidth-usage or any better name */
  g_object_class_install_property (gobject_class, PROP_BITRATE_LIMIT,
      g_param_spec_float ("bitrate-limit",
          "Bitrate limit in %",
          "Limit of the available bitrate to use when switching to alternates.",
          0, 1, DEFAULT_BITRATE_LIMIT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_adaptive_demux_change_state;

  gstbin_class->handle_message = gst_adaptive_demux_handle_message;

  klass->data_received = gst_adaptive_demux_stream_data_received_default;
  klass->finish_fragment = gst_adaptive_demux_stream_finish_fragment_default;
  klass->update_manifest = gst_adaptive_demux_update_manifest_default;
  klass->requires_periodical_playlist_update =
      gst_adaptive_demux_requires_periodical_playlist_update_default;

}

static void
gst_adaptive_demux_init (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxClass * klass)
{
  GstPadTemplate *pad_template;
  GstClockType clock_type = GST_CLOCK_TYPE_OTHER;
  GObjectClass *gobject_class;

  GST_DEBUG_OBJECT (demux, "gst_adaptive_demux_init");

  demux->priv = gst_adaptive_demux_get_instance_private (demux);
  demux->priv->input_adapter = gst_adapter_new ();
  demux->downloader = gst_uri_downloader_new ();
  gst_uri_downloader_set_parent (demux->downloader, GST_ELEMENT_CAST (demux));
  demux->stream_struct_size = sizeof (GstAdaptiveDemuxStream);
  demux->priv->segment_seqnum = gst_util_seqnum_next ();
  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;

  gst_segment_init (&demux->segment, GST_FORMAT_TIME);

  gst_bin_set_suppressed_flags (GST_BIN_CAST (demux),
      GST_ELEMENT_FLAG_SOURCE | GST_ELEMENT_FLAG_SINK);

  demux->realtime_clock = gst_system_clock_obtain ();
  g_assert (demux->realtime_clock != NULL);
  gobject_class = G_OBJECT_GET_CLASS (demux->realtime_clock);
  if (g_object_class_find_property (gobject_class, "clock-type")) {
    g_object_get (demux->realtime_clock, "clock-type", &clock_type, NULL);
  } else {
    GST_WARNING_OBJECT (demux,
        "System clock does not have clock-type property");
  }
  if (clock_type == GST_CLOCK_TYPE_REALTIME) {
    demux->clock_offset = 0;
  } else {
    GDateTime *utc_now;
    GstClockTime rtc_now;

    utc_now = g_date_time_new_now_utc ();
    rtc_now = gst_clock_get_time (demux->realtime_clock);
    demux->clock_offset =
        g_date_time_to_unix (utc_now) * G_TIME_SPAN_SECOND +
        g_date_time_get_microsecond (utc_now) - GST_TIME_AS_USECONDS (rtc_now);
    g_date_time_unref (utc_now);
  }
  g_rec_mutex_init (&demux->priv->updates_lock);
  demux->priv->updates_task =
      gst_task_new ((GstTaskFunction) gst_adaptive_demux_updates_loop,
      demux, NULL);
  gst_task_set_lock (demux->priv->updates_task, &demux->priv->updates_lock);

  g_mutex_init (&demux->priv->updates_timed_lock);
  g_cond_init (&demux->priv->updates_timed_cond);

  g_cond_init (&demux->priv->manifest_cond);
  g_mutex_init (&demux->priv->manifest_update_lock);

  g_rec_mutex_init (&demux->priv->manifest_lock);
  g_mutex_init (&demux->priv->api_lock);
  g_mutex_init (&demux->priv->segment_lock);

  g_cond_init (&demux->priv->preroll_cond);
  g_mutex_init (&demux->priv->preroll_lock);

  pad_template =
      gst_element_class_get_pad_template (GST_ELEMENT_CLASS (klass), "sink");
  g_return_if_fail (pad_template != NULL);

  demux->sinkpad = gst_pad_new_from_template (pad_template, "sink");
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_adaptive_demux_sink_event));
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_adaptive_demux_sink_chain));

  /* Properties */
  demux->bitrate_limit = DEFAULT_BITRATE_LIMIT;
  demux->connection_speed = DEFAULT_CONNECTION_SPEED;

  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);
}

static void
gst_adaptive_demux_finalize (GObject * object)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (object);
  GstAdaptiveDemuxPrivate *priv = demux->priv;

  GST_DEBUG_OBJECT (object, "finalize");

  g_object_unref (priv->input_adapter);
  g_object_unref (demux->downloader);

  g_mutex_clear (&priv->updates_timed_lock);
  g_cond_clear (&priv->updates_timed_cond);
  g_mutex_clear (&demux->priv->manifest_update_lock);
  g_cond_clear (&demux->priv->manifest_cond);
  g_object_unref (priv->updates_task);
  g_rec_mutex_clear (&priv->updates_lock);
  g_rec_mutex_clear (&demux->priv->manifest_lock);
  g_mutex_clear (&demux->priv->api_lock);
  g_mutex_clear (&demux->priv->segment_lock);
  if (demux->realtime_clock) {
    gst_object_unref (demux->realtime_clock);
    demux->realtime_clock = NULL;
  }

  g_cond_clear (&demux->priv->preroll_cond);
  g_mutex_clear (&demux->priv->preroll_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_adaptive_demux_change_state (GstElement * element,
    GstStateChange transition)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (element);
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      if (g_atomic_int_compare_and_exchange (&demux->running, TRUE, FALSE))
        GST_DEBUG_OBJECT (demux, "demuxer has stopped running");
      gst_uri_downloader_cancel (demux->downloader);

      GST_API_LOCK (demux);
      GST_MANIFEST_LOCK (demux);
      gst_adaptive_demux_reset (demux);
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_API_LOCK (demux);
      GST_MANIFEST_LOCK (demux);
      gst_adaptive_demux_reset (demux);
      /* Clear "cancelled" flag in uridownloader since subclass might want to
       * use uridownloader to fetch another manifest */
      gst_uri_downloader_reset (demux->downloader);
      if (g_atomic_int_get (&demux->priv->have_manifest))
        gst_adaptive_demux_start_manifest_update_task (demux);
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);
      if (g_atomic_int_compare_and_exchange (&demux->running, FALSE, TRUE))
        GST_DEBUG_OBJECT (demux, "demuxer has started running");
      break;
    default:
      break;
  }

  /* this must be run without MANIFEST_LOCK taken.
   * For PLAYING to PLAYING state changes, it will want to take a lock in
   * src element and that lock is held while the streaming thread is running.
   * The streaming thread will take the MANIFEST_LOCK, leading to a deadlock.
   */
  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  return result;
}

static gboolean
gst_adaptive_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (parent);
  gboolean ret;

  switch (event->type) {
    case GST_EVENT_FLUSH_STOP:{
      GST_API_LOCK (demux);
      GST_MANIFEST_LOCK (demux);

      gst_adaptive_demux_reset (demux);

      ret = gst_pad_event_default (pad, parent, event);

      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);

      return ret;
    }
    case GST_EVENT_EOS:{
      GstAdaptiveDemuxClass *demux_class;
      GstQuery *query;
      gboolean query_res;
      gboolean ret = TRUE;
      gsize available;
      GstBuffer *manifest_buffer;

      GST_API_LOCK (demux);
      GST_MANIFEST_LOCK (demux);

      demux_class = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

      available = gst_adapter_available (demux->priv->input_adapter);

      if (available == 0) {
        GST_WARNING_OBJECT (demux, "Received EOS without a manifest.");
        ret = gst_pad_event_default (pad, parent, event);

        GST_MANIFEST_UNLOCK (demux);
        GST_API_UNLOCK (demux);

        return ret;
      }

      GST_DEBUG_OBJECT (demux, "Got EOS on the sink pad: manifest fetched");

      /* Need to get the URI to use it as a base to generate the fragment's
       * uris */
      query = gst_query_new_uri ();
      query_res = gst_pad_peer_query (pad, query);
      if (query_res) {
        gchar *uri, *redirect_uri;
        gboolean permanent;

        gst_query_parse_uri (query, &uri);
        gst_query_parse_uri_redirection (query, &redirect_uri);
        gst_query_parse_uri_redirection_permanent (query, &permanent);

        if (permanent && redirect_uri) {
          demux->manifest_uri = redirect_uri;
          demux->manifest_base_uri = NULL;
          g_free (uri);
        } else {
          demux->manifest_uri = uri;
          demux->manifest_base_uri = redirect_uri;
        }

        GST_DEBUG_OBJECT (demux, "Fetched manifest at URI: %s (base: %s)",
            demux->manifest_uri, GST_STR_NULL (demux->manifest_base_uri));
      } else {
        GST_WARNING_OBJECT (demux, "Upstream URI query failed.");
      }
      gst_query_unref (query);

      /* Let the subclass parse the manifest */
      manifest_buffer =
          gst_adapter_take_buffer (demux->priv->input_adapter, available);
      if (!demux_class->process_manifest (demux, manifest_buffer)) {
        /* In most cases, this will happen if we set a wrong url in the
         * source element and we have received the 404 HTML response instead of
         * the manifest */
        GST_ELEMENT_ERROR (demux, STREAM, DECODE, ("Invalid manifest."),
            (NULL));
        ret = FALSE;
      } else {
        g_atomic_int_set (&demux->priv->have_manifest, TRUE);
      }
      gst_buffer_unref (manifest_buffer);

      gst_element_post_message (GST_ELEMENT_CAST (demux),
          gst_message_new_element (GST_OBJECT_CAST (demux),
              gst_structure_new (GST_ADAPTIVE_DEMUX_STATISTICS_MESSAGE_NAME,
                  "manifest-uri", G_TYPE_STRING,
                  demux->manifest_uri, "uri", G_TYPE_STRING,
                  demux->manifest_uri,
                  "manifest-download-start", GST_TYPE_CLOCK_TIME,
                  GST_CLOCK_TIME_NONE,
                  "manifest-download-stop", GST_TYPE_CLOCK_TIME,
                  gst_util_get_timestamp (), NULL)));

      if (ret) {
        /* Send duration message */
        if (!gst_adaptive_demux_is_live (demux)) {
          GstClockTime duration = demux_class->get_duration (demux);

          if (duration != GST_CLOCK_TIME_NONE) {
            GST_DEBUG_OBJECT (demux,
                "Sending duration message : %" GST_TIME_FORMAT,
                GST_TIME_ARGS (duration));
            gst_element_post_message (GST_ELEMENT (demux),
                gst_message_new_duration_changed (GST_OBJECT (demux)));
          } else {
            GST_DEBUG_OBJECT (demux,
                "media duration unknown, can not send the duration message");
          }
        }

        if (demux->next_streams) {
          gst_adaptive_demux_prepare_streams (demux,
              gst_adaptive_demux_is_live (demux));
          gst_adaptive_demux_start_tasks (demux, TRUE);
          gst_adaptive_demux_start_manifest_update_task (demux);
        } else {
          /* no streams */
          GST_WARNING_OBJECT (demux, "No streams created from manifest");
          GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
              (_("This file contains no playable streams.")),
              ("No known stream formats found at the Manifest"));
          ret = FALSE;
        }

      }
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);

      gst_event_unref (event);
      return ret;
    }
    case GST_EVENT_SEGMENT:
      /* Swallow newsegments, we'll push our own */
      gst_event_unref (event);
      return TRUE;
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static GstFlowReturn
gst_adaptive_demux_sink_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (parent);

  GST_MANIFEST_LOCK (demux);

  gst_adapter_push (demux->priv->input_adapter, buffer);

  GST_INFO_OBJECT (demux, "Received manifest buffer, total size is %i bytes",
      (gint) gst_adapter_available (demux->priv->input_adapter));

  GST_MANIFEST_UNLOCK (demux);
  return GST_FLOW_OK;
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_reset (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  GList *iter;
  GList *old_streams;
  GstEvent *eos;

  /* take ownership of old_streams before releasing the manifest_lock in
   * gst_adaptive_demux_stop_tasks
   */
  old_streams = demux->priv->old_streams;
  demux->priv->old_streams = NULL;

  gst_adaptive_demux_stop_tasks (demux, TRUE);

  if (klass->reset)
    klass->reset (demux);

  eos = gst_event_new_eos ();
  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    if (stream->pad) {
      gst_pad_push_event (stream->pad, gst_event_ref (eos));
      gst_pad_set_active (stream->pad, FALSE);

      gst_element_remove_pad (GST_ELEMENT_CAST (demux), stream->pad);
    }
    gst_adaptive_demux_stream_free (stream);
  }
  gst_event_unref (eos);
  g_list_free (demux->streams);
  demux->streams = NULL;
  if (demux->prepared_streams) {
    g_list_free_full (demux->prepared_streams,
        (GDestroyNotify) gst_adaptive_demux_stream_free);
    demux->prepared_streams = NULL;
  }
  if (demux->next_streams) {
    g_list_free_full (demux->next_streams,
        (GDestroyNotify) gst_adaptive_demux_stream_free);
    demux->next_streams = NULL;
  }

  if (old_streams) {
    g_list_free_full (old_streams,
        (GDestroyNotify) gst_adaptive_demux_stream_free);
  }

  if (demux->priv->old_streams) {
    g_list_free_full (demux->priv->old_streams,
        (GDestroyNotify) gst_adaptive_demux_stream_free);
    demux->priv->old_streams = NULL;
  }

  g_free (demux->manifest_uri);
  g_free (demux->manifest_base_uri);
  demux->manifest_uri = NULL;
  demux->manifest_base_uri = NULL;

  gst_adapter_clear (demux->priv->input_adapter);
  g_atomic_int_set (&demux->priv->have_manifest, FALSE);

  gst_segment_init (&demux->segment, GST_FORMAT_TIME);

  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;
  demux->priv->segment_seqnum = gst_util_seqnum_next ();
}

static void
gst_adaptive_demux_handle_message (GstBin * bin, GstMessage * msg)
{
  GstAdaptiveDemux *demux = GST_ADAPTIVE_DEMUX_CAST (bin);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GList *iter;
      GstAdaptiveDemuxStream *stream = NULL;
      GError *err = NULL;
      gchar *debug = NULL;
      gchar *new_error = NULL;
      const GstStructure *details = NULL;

      GST_MANIFEST_LOCK (demux);

      for (iter = demux->streams; iter; iter = g_list_next (iter)) {
        GstAdaptiveDemuxStream *cur = iter->data;
        if (gst_object_has_as_ancestor (GST_MESSAGE_SRC (msg),
                GST_OBJECT_CAST (cur->src))) {
          stream = cur;
          break;
        }
      }
      if (stream == NULL) {
        for (iter = demux->prepared_streams; iter; iter = g_list_next (iter)) {
          GstAdaptiveDemuxStream *cur = iter->data;
          if (gst_object_has_as_ancestor (GST_MESSAGE_SRC (msg),
                  GST_OBJECT_CAST (cur->src))) {
            stream = cur;
            break;
          }
        }
        if (stream == NULL) {
          GST_WARNING_OBJECT (demux,
              "Failed to locate stream for errored element");
          break;
        }
      }

      gst_message_parse_error (msg, &err, &debug);

      GST_WARNING_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
          "Source posted error: %d:%d %s (%s)", err->domain, err->code,
          err->message, debug);

      if (debug)
        new_error = g_strdup_printf ("%s: %s\n", err->message, debug);
      if (new_error) {
        g_free (err->message);
        err->message = new_error;
      }

      gst_message_parse_error_details (msg, &details);
      if (details) {
        gst_structure_get_uint (details, "http-status-code",
            &stream->last_status_code);
      }

      /* error, but ask to retry */
      gst_adaptive_demux_stream_fragment_download_finish (stream,
          GST_FLOW_CUSTOM_ERROR, err);

      g_error_free (err);
      g_free (debug);

      GST_MANIFEST_UNLOCK (demux);

      gst_message_unref (msg);
      msg = NULL;
    }
      break;
    default:
      break;
  }

  if (msg)
    GST_BIN_CLASS (parent_class)->handle_message (bin, msg);
}

void
gst_adaptive_demux_set_stream_struct_size (GstAdaptiveDemux * demux,
    gsize struct_size)
{
  GST_API_LOCK (demux);
  GST_MANIFEST_LOCK (demux);
  demux->stream_struct_size = struct_size;
  GST_MANIFEST_UNLOCK (demux);
  GST_API_UNLOCK (demux);
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_prepare_stream (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstPad *pad = stream->pad;
  gchar *name = gst_pad_get_name (pad);
  GstEvent *event;
  gchar *stream_id;

  gst_pad_set_active (pad, TRUE);
  stream->need_header = TRUE;

  stream_id = gst_pad_create_stream_id (pad, GST_ELEMENT_CAST (demux), name);

  event =
      gst_pad_get_sticky_event (GST_ADAPTIVE_DEMUX_SINK_PAD (demux),
      GST_EVENT_STREAM_START, 0);
  if (event) {
    if (gst_event_parse_group_id (event, &demux->group_id))
      demux->have_group_id = TRUE;
    else
      demux->have_group_id = FALSE;
    gst_event_unref (event);
  } else if (!demux->have_group_id) {
    demux->have_group_id = TRUE;
    demux->group_id = gst_util_group_id_next ();
  }
  event = gst_event_new_stream_start (stream_id);
  if (demux->have_group_id)
    gst_event_set_group_id (event, demux->group_id);

  gst_pad_push_event (pad, event);
  g_free (stream_id);
  g_free (name);

  GST_DEBUG_OBJECT (demux, "Preparing srcpad %s:%s", GST_DEBUG_PAD_NAME (pad));

  stream->discont = TRUE;

  return TRUE;
}

static gboolean
gst_adaptive_demux_expose_stream (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  gboolean ret;
  GstPad *pad = stream->pad;
  GstCaps *caps;

  if (stream->pending_caps) {
    gst_pad_set_caps (pad, stream->pending_caps);
    caps = stream->pending_caps;
    stream->pending_caps = NULL;
  } else {
    caps = gst_pad_get_current_caps (pad);
  }

  GST_DEBUG_OBJECT (demux, "Exposing srcpad %s:%s with caps %" GST_PTR_FORMAT,
      GST_DEBUG_PAD_NAME (pad), caps);
  if (caps)
    gst_caps_unref (caps);

  gst_object_ref (pad);

  /* Don't hold the manifest lock while exposing a pad */
  GST_MANIFEST_UNLOCK (demux);
  ret = gst_element_add_pad (GST_ELEMENT_CAST (demux), pad);
  GST_MANIFEST_LOCK (demux);

  return ret;
}

/* must be called with manifest_lock taken */
static GstClockTime
gst_adaptive_demux_stream_get_presentation_offset (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemuxClass *klass;

  klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->get_presentation_offset == NULL)
    return 0;

  return klass->get_presentation_offset (demux, stream);
}

/* must be called with manifest_lock taken */
static GstClockTime
gst_adaptive_demux_get_period_start_time (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass;

  klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->get_period_start_time == NULL)
    return 0;

  return klass->get_period_start_time (demux);
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_prepare_streams (GstAdaptiveDemux * demux,
    gboolean first_and_live)
{
  GList *iter;
  GstClockTime period_start, min_pts = GST_CLOCK_TIME_NONE;

  g_return_val_if_fail (demux->next_streams != NULL, FALSE);
  if (demux->prepared_streams != NULL) {
    /* Old streams that were never exposed, due to a seek or so */
    GST_FIXME_OBJECT (demux,
        "Preparing new streams without cleaning up old ones!");
    return FALSE;
  }

  demux->prepared_streams = demux->next_streams;
  demux->next_streams = NULL;

  if (!gst_adaptive_demux_is_running (demux)) {
    GST_DEBUG_OBJECT (demux, "Not exposing pads due to shutdown");
    return TRUE;
  }

  for (iter = demux->prepared_streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;

    stream->do_block = TRUE;

    if (!gst_adaptive_demux_prepare_stream (demux,
            GST_ADAPTIVE_DEMUX_STREAM_CAST (stream))) {
      /* TODO act on error */
      GST_FIXME_OBJECT (stream->pad,
          "Do something on failure to expose stream");
    }

    if (first_and_live) {
      /* TODO we only need the first timestamp, maybe create a simple function to
       * get the current PTS of a fragment ? */
      GST_DEBUG_OBJECT (demux, "Calling update_fragment_info");
      gst_adaptive_demux_stream_update_fragment_info (demux, stream);

      if (GST_CLOCK_TIME_IS_VALID (min_pts)) {
        min_pts = MIN (min_pts, stream->fragment.timestamp);
      } else {
        min_pts = stream->fragment.timestamp;
      }
    }
  }

  period_start = gst_adaptive_demux_get_period_start_time (demux);

  /* For live streams, the subclass is supposed to seek to the current
   * fragment and then tell us its timestamp in stream->fragment.timestamp.
   * We now also have to seek our demuxer segment to reflect this.
   *
   * FIXME: This needs some refactoring at some point.
   */
  if (first_and_live) {
    gst_segment_do_seek (&demux->segment, demux->segment.rate, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_SET, min_pts + period_start,
        GST_SEEK_TYPE_NONE, -1, NULL);
  }

  for (iter = demux->prepared_streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    GstClockTime offset;

    offset = gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
    stream->segment = demux->segment;

    /* The demuxer segment is just built from seek events, but for each stream
     * we have to adjust segments according to the current period and the
     * stream specific presentation time offset.
     *
     * For each period, buffer timestamps start again from 0. Additionally the
     * buffer timestamps are shifted by the stream specific presentation time
     * offset, so the first buffer timestamp of a period is 0 + presentation
     * time offset. If the stream contains timestamps itself, this is also
     * supposed to be the presentation time stored inside the stream.
     *
     * The stream time over periods is supposed to be continuous, that is the
     * buffer timestamp 0 + presentation time offset should map to the start
     * time of the current period.
     *
     *
     * The adjustment of the stream segments as such works the following.
     *
     * If the demuxer segment start is bigger than the period start, this
     * means that we have to drop some media at the beginning of the current
     * period, e.g. because a seek into the middle of the period has
     * happened. The amount of media to drop is the difference between the
     * period start and the demuxer segment start, and as each period starts
     * again from 0, this difference is going to be the actual stream's
     * segment start. As all timestamps of the stream are shifted by the
     * presentation time offset, we will also have to move the segment start
     * by that offset.
     *
     * Likewise, the demuxer segment stop value is adjusted in the same
     * fashion.
     *
     * Now the running time and stream time at the stream's segment start has
     * to be the one that is stored inside the demuxer's segment, which means
     * that segment.base and segment.time have to be copied over (done just
     * above)
     *
     *
     * If the demuxer segment start is smaller than the period start time,
     * this means that the whole period is inside the segment. As each period
     * starts timestamps from 0, and additionally timestamps are shifted by
     * the presentation time offset, the stream's first timestamp (and as such
     * the stream's segment start) has to be the presentation time offset.
     * The stream time at the segment start is supposed to be the stream time
     * of the period start according to the demuxer segment, so the stream
     * segment's time would be set to that. The same goes for the stream
     * segment's base, which is supposed to be the running time of the period
     * start according to the demuxer's segment.
     *
     * The same logic applies for negative rates with the segment stop and
     * the period stop time (which gets clamped).
     *
     *
     * For the first case where not the complete period is inside the segment,
     * the segment time and base as calculated by the second case would be
     * equivalent.
     */
    GST_DEBUG_OBJECT (demux, "Using demux segment %" GST_SEGMENT_FORMAT,
        &demux->segment);
    GST_DEBUG_OBJECT (demux,
        "period_start: %" GST_TIME_FORMAT " offset: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (period_start), GST_TIME_ARGS (offset));
    /* note for readers:
     * Since stream->segment is initially a copy of demux->segment,
     * only the values that need updating are modified below. */
    if (first_and_live) {
      /* If first and live, demuxer did seek to the current position already */
      stream->segment.start = demux->segment.start - period_start + offset;
      if (GST_CLOCK_TIME_IS_VALID (demux->segment.stop))
        stream->segment.stop = demux->segment.stop - period_start + offset;
      /* FIXME : Do we need to handle negative rates for this ? */
      stream->segment.position = stream->segment.start;
    } else if (demux->segment.start > period_start) {
      /* seek within a period */
      stream->segment.start = demux->segment.start - period_start + offset;
      if (GST_CLOCK_TIME_IS_VALID (demux->segment.stop))
        stream->segment.stop = demux->segment.stop - period_start + offset;
      if (stream->segment.rate >= 0)
        stream->segment.position = offset;
      else
        stream->segment.position = stream->segment.stop;
    } else {
      stream->segment.start = offset;
      if (GST_CLOCK_TIME_IS_VALID (demux->segment.stop))
        stream->segment.stop = demux->segment.stop - period_start + offset;
      if (stream->segment.rate >= 0)
        stream->segment.position = offset;
      else
        stream->segment.position = stream->segment.stop;
      stream->segment.time =
          gst_segment_to_stream_time (&demux->segment, GST_FORMAT_TIME,
          period_start);
      stream->segment.base =
          gst_segment_to_running_time (&demux->segment, GST_FORMAT_TIME,
          period_start);
    }

    stream->pending_segment = gst_event_new_segment (&stream->segment);
    gst_event_set_seqnum (stream->pending_segment, demux->priv->segment_seqnum);

    GST_DEBUG_OBJECT (demux,
        "Prepared segment %" GST_SEGMENT_FORMAT " for stream %p",
        &stream->segment, stream);
  }
  demux->priv->qos_earliest_time = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static gboolean
gst_adaptive_demux_expose_streams (GstAdaptiveDemux * demux)
{
  GList *iter;
  GList *old_streams;

  g_return_val_if_fail (demux->prepared_streams != NULL, FALSE);

  old_streams = demux->streams;
  demux->streams = demux->prepared_streams;
  demux->prepared_streams = NULL;

  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;

    if (!gst_adaptive_demux_expose_stream (demux,
            GST_ADAPTIVE_DEMUX_STREAM_CAST (stream))) {
      /* TODO act on error */
    }
  }
  demux->priv->preroll_pending = 0;

  GST_MANIFEST_UNLOCK (demux);
  gst_element_no_more_pads (GST_ELEMENT_CAST (demux));
  GST_MANIFEST_LOCK (demux);

  if (old_streams) {
    GstEvent *eos = gst_event_new_eos ();

    /* before we put streams in the demux->priv->old_streams list,
     * we ask the download task to stop. In this way, it will no longer be
     * allowed to change the demux object.
     */
    for (iter = old_streams; iter; iter = g_list_next (iter)) {
      GstAdaptiveDemuxStream *stream = iter->data;
      GstPad *pad = gst_object_ref (GST_PAD (stream->pad));

      GST_MANIFEST_UNLOCK (demux);

      GST_DEBUG_OBJECT (pad, "Pushing EOS");
      gst_pad_push_event (pad, gst_event_ref (eos));
      gst_pad_set_active (pad, FALSE);

      GST_LOG_OBJECT (pad, "Removing stream");
      gst_element_remove_pad (GST_ELEMENT (demux), pad);
      GST_MANIFEST_LOCK (demux);

      gst_object_unref (GST_OBJECT (pad));

      /* ask the download task to stop.
       * We will not join it now, because our thread can be one of these tasks.
       * We will do the joining later, from another stream download task or
       * from gst_adaptive_demux_stop_tasks.
       * We also cannot change the state of the stream->src element, because
       * that will wait on the streaming thread (which could be this thread)
       * to stop first.
       * Because we sent an EOS to the downstream element, the stream->src
       * element should detect this in its streaming task and stop.
       * Even if it doesn't do that, we will change its state later in
       * gst_adaptive_demux_stop_tasks.
       */
      GST_LOG_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
          "Marking stream as cancelled");
      gst_task_stop (stream->download_task);
      g_mutex_lock (&stream->fragment_download_lock);
      stream->cancelled = TRUE;
      stream->replaced = TRUE;
      g_cond_signal (&stream->fragment_download_cond);
      g_mutex_unlock (&stream->fragment_download_lock);
    }
    gst_event_unref (eos);

    /* The list should be freed from another thread as we can't properly
     * cleanup a GstTask from itself */
    demux->priv->old_streams =
        g_list_concat (demux->priv->old_streams, old_streams);
  }

  /* Unblock after removing oldstreams */
  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    stream->do_block = FALSE;
  }

  GST_DEBUG_OBJECT (demux, "All streams are exposed");

  return TRUE;
}

/* must be called with manifest_lock taken */
GstAdaptiveDemuxStream *
gst_adaptive_demux_stream_new (GstAdaptiveDemux * demux, GstPad * pad)
{
  GstAdaptiveDemuxStream *stream;

  stream = g_malloc0 (demux->stream_struct_size);

  /* Downloading task */
  g_rec_mutex_init (&stream->download_lock);
  stream->download_task =
      gst_task_new ((GstTaskFunction) gst_adaptive_demux_stream_download_loop,
      stream, NULL);
  gst_task_set_lock (stream->download_task, &stream->download_lock);

  stream->pad = pad;
  stream->demux = demux;
  stream->fragment_bitrates =
      g_malloc0 (sizeof (guint64) * NUM_LOOKBACK_FRAGMENTS);
  gst_pad_set_element_private (pad, stream);
  stream->qos_earliest_time = GST_CLOCK_TIME_NONE;

  g_mutex_lock (&demux->priv->preroll_lock);
  stream->do_block = TRUE;
  demux->priv->preroll_pending++;
  g_mutex_unlock (&demux->priv->preroll_lock);

  gst_pad_set_query_function (pad,
      GST_DEBUG_FUNCPTR (gst_adaptive_demux_src_query));
  gst_pad_set_event_function (pad,
      GST_DEBUG_FUNCPTR (gst_adaptive_demux_src_event));

  gst_segment_init (&stream->segment, GST_FORMAT_TIME);
  g_cond_init (&stream->fragment_download_cond);
  g_mutex_init (&stream->fragment_download_lock);

  demux->next_streams = g_list_append (demux->next_streams, stream);

  return stream;
}

GstAdaptiveDemuxStream *
gst_adaptive_demux_find_stream_for_pad (GstAdaptiveDemux * demux, GstPad * pad)
{
  GList *iter;

  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    if (stream->pad == pad) {
      return stream;
    }
  }

  return NULL;
}

/* must be called with manifest_lock taken.
 * It will temporarily drop the manifest_lock in order to join the task.
 * It will join only the old_streams (the demux->streams are joined by
 * gst_adaptive_demux_stop_tasks before gst_adaptive_demux_stream_free is
 * called)
 */
static void
gst_adaptive_demux_stream_free (GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemux *demux = stream->demux;
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->stream_free)
    klass->stream_free (stream);

  g_clear_error (&stream->last_error);
  if (stream->download_task) {
    if (GST_TASK_STATE (stream->download_task) != GST_TASK_STOPPED) {
      GST_DEBUG_OBJECT (demux, "Leaving streaming task %s:%s",
          GST_DEBUG_PAD_NAME (stream->pad));

      gst_task_stop (stream->download_task);

      g_mutex_lock (&stream->fragment_download_lock);
      stream->cancelled = TRUE;
      g_cond_signal (&stream->fragment_download_cond);
      g_mutex_unlock (&stream->fragment_download_lock);
    }
    GST_LOG_OBJECT (demux, "Waiting for task to finish");

    /* temporarily drop the manifest lock to join the task */
    GST_MANIFEST_UNLOCK (demux);

    gst_task_join (stream->download_task);

    GST_MANIFEST_LOCK (demux);

    GST_LOG_OBJECT (demux, "Finished");
    gst_object_unref (stream->download_task);
    g_rec_mutex_clear (&stream->download_lock);
    stream->download_task = NULL;
  }

  gst_adaptive_demux_stream_fragment_clear (&stream->fragment);

  if (stream->pending_segment) {
    gst_event_unref (stream->pending_segment);
    stream->pending_segment = NULL;
  }

  if (stream->pending_events) {
    g_list_free_full (stream->pending_events, (GDestroyNotify) gst_event_unref);
    stream->pending_events = NULL;
  }

  if (stream->internal_pad) {
    gst_object_unparent (GST_OBJECT_CAST (stream->internal_pad));
  }

  if (stream->src_srcpad) {
    gst_object_unref (stream->src_srcpad);
    stream->src_srcpad = NULL;
  }

  if (stream->src) {
    GstElement *src = stream->src;

    stream->src = NULL;

    GST_MANIFEST_UNLOCK (demux);
    gst_element_set_locked_state (src, TRUE);
    gst_element_set_state (src, GST_STATE_NULL);
    gst_bin_remove (GST_BIN_CAST (demux), src);
    GST_MANIFEST_LOCK (demux);
  }

  g_cond_clear (&stream->fragment_download_cond);
  g_mutex_clear (&stream->fragment_download_lock);
  g_free (stream->fragment_bitrates);

  if (stream->pad) {
    gst_object_unref (stream->pad);
    stream->pad = NULL;
  }
  if (stream->pending_caps)
    gst_caps_unref (stream->pending_caps);

  g_clear_pointer (&stream->pending_tags, gst_tag_list_unref);

  g_free (stream);
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_get_live_seek_range (GstAdaptiveDemux * demux,
    gint64 * range_start, gint64 * range_stop)
{
  GstAdaptiveDemuxClass *klass;

  klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  g_return_val_if_fail (klass->get_live_seek_range, FALSE);

  return klass->get_live_seek_range (demux, range_start, range_stop);
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_stream_in_live_seek_range (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  gint64 range_start, range_stop;
  if (gst_adaptive_demux_get_live_seek_range (demux, &range_start, &range_stop)) {
    GST_LOG_OBJECT (stream->pad,
        "stream position %" GST_TIME_FORMAT "  live seek range %"
        GST_STIME_FORMAT " - %" GST_STIME_FORMAT,
        GST_TIME_ARGS (stream->segment.position), GST_STIME_ARGS (range_start),
        GST_STIME_ARGS (range_stop));
    return (stream->segment.position >= range_start
        && stream->segment.position <= range_stop);
  }

  return FALSE;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_can_seek (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass;

  klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  if (gst_adaptive_demux_is_live (demux)) {
    return klass->get_live_seek_range != NULL;
  }

  return klass->seek != NULL;
}

static void
gst_adaptive_demux_update_streams_segment (GstAdaptiveDemux * demux,
    GList * streams, gint64 period_start, GstSeekType start_type,
    GstSeekType stop_type)
{
  GList *iter;
  for (iter = streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    GstEvent *seg_evt;
    GstClockTime offset;

    /* See comments in gst_adaptive_demux_get_period_start_time() for
     * an explanation of the segment modifications */
    stream->segment = demux->segment;
    offset = gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
    stream->segment.start += offset - period_start;
    if (GST_CLOCK_TIME_IS_VALID (demux->segment.stop))
      stream->segment.stop += offset - period_start;
    if (demux->segment.rate > 0 && start_type != GST_SEEK_TYPE_NONE)
      stream->segment.position = stream->segment.start;
    else if (demux->segment.rate < 0 && stop_type != GST_SEEK_TYPE_NONE)
      stream->segment.position = stream->segment.stop;
    seg_evt = gst_event_new_segment (&stream->segment);
    gst_event_set_seqnum (seg_evt, demux->priv->segment_seqnum);
    gst_event_replace (&stream->pending_segment, seg_evt);
    GST_DEBUG_OBJECT (stream->pad, "Pending segment now %" GST_PTR_FORMAT,
        stream->pending_segment);
    gst_event_unref (seg_evt);
    /* Make sure the first buffer after a seek has the discont flag */
    stream->discont = TRUE;
  }
  demux->priv->qos_earliest_time = GST_CLOCK_TIME_NONE;
}

#define IS_SNAP_SEEK(f) (f & (GST_SEEK_FLAG_SNAP_BEFORE |	  \
                              GST_SEEK_FLAG_SNAP_AFTER |	  \
                              GST_SEEK_FLAG_SNAP_NEAREST |	  \
			      GST_SEEK_FLAG_TRICKMODE_KEY_UNITS | \
			      GST_SEEK_FLAG_KEY_UNIT))
#define REMOVE_SNAP_FLAGS(f) (f & ~(GST_SEEK_FLAG_SNAP_BEFORE | \
                              GST_SEEK_FLAG_SNAP_AFTER | \
                              GST_SEEK_FLAG_SNAP_NEAREST))

static gboolean
gst_adaptive_demux_handle_seek_event (GstAdaptiveDemux * demux, GstPad * pad,
    GstEvent * event)
{
  GstAdaptiveDemuxClass *demux_class = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  guint32 seqnum;
  gboolean update;
  gboolean ret;
  GstSegment oldsegment;
  GstAdaptiveDemuxStream *stream = NULL;

  GST_INFO_OBJECT (demux, "Received seek event");

  GST_API_LOCK (demux);
  GST_MANIFEST_LOCK (demux);

  gst_event_parse_seek (event, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  if (format != GST_FORMAT_TIME) {
    GST_MANIFEST_UNLOCK (demux);
    GST_API_UNLOCK (demux);
    GST_WARNING_OBJECT (demux,
        "Adaptive demuxers only support TIME-based seeking");
    gst_event_unref (event);
    return FALSE;
  }

  if (flags & GST_SEEK_FLAG_SEGMENT) {
    GST_FIXME_OBJECT (demux, "Handle segment seeks");
    GST_MANIFEST_UNLOCK (demux);
    GST_API_UNLOCK (demux);
    gst_event_unref (event);
    return FALSE;
  }

  seqnum = gst_event_get_seqnum (event);

  if (flags & GST_SEEK_FLAG_INSTANT_RATE_CHANGE) {
    /* For instant rate seeks, reply directly and update
     * our segment so the new rate is reflected in any future
     * fragments */
    GstEvent *ev;

    /* instant rate change only supported if direction does not change. All
     * other requirements are already checked before creating the seek event
     * but let's double-check here to be sure */
    if ((demux->segment.rate > 0 && rate < 0) ||
        (demux->segment.rate < 0 && rate > 0) ||
        start_type != GST_SEEK_TYPE_NONE ||
        stop_type != GST_SEEK_TYPE_NONE || (flags & GST_SEEK_FLAG_FLUSH)) {
      GST_ERROR_OBJECT (demux,
          "Instant rate change seeks only supported in the "
          "same direction, without flushing and position change");
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);
      return FALSE;
    }

    ev = gst_event_new_instant_rate_change (rate / demux->segment.rate,
        (GstSegmentFlags) flags);
    gst_event_set_seqnum (ev, seqnum);

    GST_MANIFEST_UNLOCK (demux);

    ret = gst_adaptive_demux_push_src_event (demux, ev);

    GST_API_UNLOCK (demux);
    gst_event_unref (event);

    return ret;
  }

  if (!gst_adaptive_demux_can_seek (demux)) {
    GST_MANIFEST_UNLOCK (demux);
    GST_API_UNLOCK (demux);
    gst_event_unref (event);
    return FALSE;
  }

  if (gst_adaptive_demux_is_live (demux)) {
    gint64 range_start, range_stop;
    gboolean changed = FALSE;
    gboolean start_valid = TRUE, stop_valid = TRUE;

    if (!gst_adaptive_demux_get_live_seek_range (demux, &range_start,
            &range_stop)) {
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);
      gst_event_unref (event);
      GST_WARNING_OBJECT (demux, "Failure getting the live seek ranges");
      return FALSE;
    }

    GST_DEBUG_OBJECT (demux,
        "Live range is %" GST_STIME_FORMAT " %" GST_STIME_FORMAT,
        GST_STIME_ARGS (range_start), GST_STIME_ARGS (range_stop));

    /* Handle relative positioning for live streams (relative to the range_stop) */
    if (start_type == GST_SEEK_TYPE_END) {
      start = range_stop + start;
      start_type = GST_SEEK_TYPE_SET;
      changed = TRUE;
    }
    if (stop_type == GST_SEEK_TYPE_END) {
      stop = range_stop + stop;
      stop_type = GST_SEEK_TYPE_SET;
      changed = TRUE;
    }

    /* Adjust the requested start/stop position if it falls beyond the live
     * seek range.
     * The only case where we don't adjust is for the starting point of
     * an accurate seek (start if forward and stop if backwards)
     */
    if (start_type == GST_SEEK_TYPE_SET && start < range_start &&
        (rate < 0 || !(flags & GST_SEEK_FLAG_ACCURATE))) {
      GST_DEBUG_OBJECT (demux,
          "seek before live stream start, setting to range start: %"
          GST_TIME_FORMAT, GST_TIME_ARGS (range_start));
      start = range_start;
      changed = TRUE;
    }
    /* truncate stop position also if set */
    if (stop_type == GST_SEEK_TYPE_SET && stop > range_stop &&
        (rate > 0 || !(flags & GST_SEEK_FLAG_ACCURATE))) {
      GST_DEBUG_OBJECT (demux,
          "seek ending after live start, adjusting to: %"
          GST_TIME_FORMAT, GST_TIME_ARGS (range_stop));
      stop = range_stop;
      changed = TRUE;
    }

    if (start_type == GST_SEEK_TYPE_SET && GST_CLOCK_TIME_IS_VALID (start) &&
        (start < range_start || start > range_stop)) {
      GST_WARNING_OBJECT (demux,
          "Seek to invalid position start:%" GST_STIME_FORMAT
          " out of seekable range (%" GST_STIME_FORMAT " - %" GST_STIME_FORMAT
          ")", GST_STIME_ARGS (start), GST_STIME_ARGS (range_start),
          GST_STIME_ARGS (range_stop));
      start_valid = FALSE;
    }
    if (stop_type == GST_SEEK_TYPE_SET && GST_CLOCK_TIME_IS_VALID (stop) &&
        (stop < range_start || stop > range_stop)) {
      GST_WARNING_OBJECT (demux,
          "Seek to invalid position stop:%" GST_STIME_FORMAT
          " out of seekable range (%" GST_STIME_FORMAT " - %" GST_STIME_FORMAT
          ")", GST_STIME_ARGS (stop), GST_STIME_ARGS (range_start),
          GST_STIME_ARGS (range_stop));
      stop_valid = FALSE;
    }

    /* If the seek position is still outside of the seekable range, refuse the seek */
    if (!start_valid || !stop_valid) {
      GST_MANIFEST_UNLOCK (demux);
      GST_API_UNLOCK (demux);
      gst_event_unref (event);
      return FALSE;
    }

    /* Re-create seek event with changed/updated values */
    if (changed) {
      gst_event_unref (event);
      event =
          gst_event_new_seek (rate, format, flags,
          start_type, start, stop_type, stop);
      gst_event_set_seqnum (event, seqnum);
    }
  }

  GST_DEBUG_OBJECT (demux, "seek event, %" GST_PTR_FORMAT, event);

  /* have a backup in case seek fails */
  gst_segment_copy_into (&demux->segment, &oldsegment);

  if (flags & GST_SEEK_FLAG_FLUSH) {
    GstEvent *fevent;

    GST_DEBUG_OBJECT (demux, "sending flush start");
    fevent = gst_event_new_flush_start ();
    gst_event_set_seqnum (fevent, seqnum);
    GST_MANIFEST_UNLOCK (demux);
    gst_adaptive_demux_push_src_event (demux, fevent);
    GST_MANIFEST_LOCK (demux);

    gst_adaptive_demux_stop_tasks (demux, FALSE);
  } else if ((rate > 0 && start_type != GST_SEEK_TYPE_NONE) ||
      (rate < 0 && stop_type != GST_SEEK_TYPE_NONE)) {

    gst_adaptive_demux_stop_tasks (demux, FALSE);
  }

  GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);

  /*
   * Handle snap seeks as follows:
   * 1) do the snap seeking on the stream that received
   *    the event
   * 2) use the final position on this stream to seek
   *    on the other streams to the same position
   *
   * We can't snap at all streams at the same time as
   * they might end in different positions, so just
   * use the one that received the event as the 'leading'
   * one to do the snap seek.
   */
  if (IS_SNAP_SEEK (flags) && demux_class->stream_seek && (stream =
          gst_adaptive_demux_find_stream_for_pad (demux, pad))) {
    GstClockTime ts;
    GstSeekFlags stream_seek_flags = flags;

    /* snap-seek on the stream that received the event and then
     * use the resulting position to seek on all streams */

    if (rate >= 0) {
      if (start_type != GST_SEEK_TYPE_NONE)
        ts = start;
      else {
        ts = stream->segment.position;
        start_type = GST_SEEK_TYPE_SET;
      }
    } else {
      if (stop_type != GST_SEEK_TYPE_NONE)
        ts = stop;
      else {
        stop_type = GST_SEEK_TYPE_SET;
        ts = stream->segment.position;
      }
    }

    if (stream) {
      demux_class->stream_seek (stream, rate >= 0, stream_seek_flags, ts, &ts);
    }

    /* replace event with a new one without snapping to seek on all streams */
    gst_event_unref (event);
    if (rate >= 0) {
      start = ts;
    } else {
      stop = ts;
    }
    event =
        gst_event_new_seek (rate, format, REMOVE_SNAP_FLAGS (flags),
        start_type, start, stop_type, stop);
    GST_DEBUG_OBJECT (demux, "Adapted snap seek to %" GST_PTR_FORMAT, event);
  }
  stream = NULL;

  ret = gst_segment_do_seek (&demux->segment, rate, format, flags, start_type,
      start, stop_type, stop, &update);

  if (ret) {
    /* FIXME - this seems unatural, do_seek() is updating base when we
     * only want the start/stop position to change, maybe do_seek() needs
     * some fixing? */
    if (!(flags & GST_SEEK_FLAG_FLUSH) && ((rate > 0
                && start_type == GST_SEEK_TYPE_NONE) || (rate < 0
                && stop_type == GST_SEEK_TYPE_NONE))) {
      demux->segment.base = oldsegment.base;
    }

    GST_DEBUG_OBJECT (demux, "Calling subclass seek: %" GST_PTR_FORMAT, event);

    ret = demux_class->seek (demux, event);
  }

  if (!ret) {
    /* Is there anything else we can do if it fails? */
    gst_segment_copy_into (&oldsegment, &demux->segment);
  } else {
    demux->priv->segment_seqnum = seqnum;
  }
  GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

  if (flags & GST_SEEK_FLAG_FLUSH) {
    GstEvent *fevent;

    GST_DEBUG_OBJECT (demux, "Sending flush stop on all pad");
    fevent = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (fevent, seqnum);
    gst_adaptive_demux_push_src_event (demux, fevent);
  }

  if (demux->next_streams) {
    /* If the seek generated new streams, get them
     * to preroll */
    gst_adaptive_demux_prepare_streams (demux, FALSE);
    gst_adaptive_demux_start_tasks (demux, TRUE);
  } else {
    GstClockTime period_start =
        gst_adaptive_demux_get_period_start_time (demux);

    GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
    gst_adaptive_demux_update_streams_segment (demux, demux->streams,
        period_start, start_type, stop_type);
    gst_adaptive_demux_update_streams_segment (demux, demux->prepared_streams,
        period_start, start_type, stop_type);
    GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

    /* Restart the demux */
    gst_adaptive_demux_start_tasks (demux, FALSE);
  }

  GST_MANIFEST_UNLOCK (demux);
  GST_API_UNLOCK (demux);
  gst_event_unref (event);

  return ret;
}

static gboolean
gst_adaptive_demux_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstAdaptiveDemux *demux;

  demux = GST_ADAPTIVE_DEMUX_CAST (parent);

  /* FIXME handle events received on pads that are to be removed */

  switch (event->type) {
    case GST_EVENT_SEEK:
    {
      guint32 seqnum = gst_event_get_seqnum (event);
      if (seqnum == demux->priv->segment_seqnum) {
        GST_LOG_OBJECT (pad,
            "Drop duplicated SEEK event seqnum %" G_GUINT32_FORMAT, seqnum);
        gst_event_unref (event);
        return TRUE;
      }
      return gst_adaptive_demux_handle_seek_event (demux, pad, event);
    }
    case GST_EVENT_RECONFIGURE:{
      GstAdaptiveDemuxStream *stream;

      GST_MANIFEST_LOCK (demux);
      stream = gst_adaptive_demux_find_stream_for_pad (demux, pad);

      if (stream) {
        if (!stream->cancelled && gst_adaptive_demux_is_running (demux) &&
            stream->last_ret == GST_FLOW_NOT_LINKED) {
          stream->last_ret = GST_FLOW_OK;
          stream->restart_download = TRUE;
          stream->need_header = TRUE;
          stream->discont = TRUE;
          GST_DEBUG_OBJECT (stream->pad, "Restarting download loop");
          gst_task_start (stream->download_task);
        }
        gst_event_unref (event);
        GST_MANIFEST_UNLOCK (demux);
        return TRUE;
      }
      GST_MANIFEST_UNLOCK (demux);
    }
      break;
    case GST_EVENT_LATENCY:{
      /* Upstream and our internal source are irrelevant
       * for latency, and we should not fail here to
       * configure the latency */
      gst_event_unref (event);
      return TRUE;
    }
    case GST_EVENT_QOS:{
      GstClockTimeDiff diff;
      GstClockTime timestamp;
      GstClockTime earliest_time;

      gst_event_parse_qos (event, NULL, NULL, &diff, &timestamp);
      /* Only take into account lateness if late */
      if (diff > 0)
        earliest_time = timestamp + 2 * diff;
      else
        earliest_time = timestamp;

      GST_OBJECT_LOCK (demux);
      if (!GST_CLOCK_TIME_IS_VALID (demux->priv->qos_earliest_time) ||
          earliest_time > demux->priv->qos_earliest_time) {
        demux->priv->qos_earliest_time = earliest_time;
        GST_DEBUG_OBJECT (demux, "qos_earliest_time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (demux->priv->qos_earliest_time));
      }
      GST_OBJECT_UNLOCK (demux);
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_adaptive_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query)
{
  GstAdaptiveDemux *demux;
  GstAdaptiveDemuxClass *demux_class;
  gboolean ret = FALSE;

  if (query == NULL)
    return FALSE;

  demux = GST_ADAPTIVE_DEMUX_CAST (parent);
  demux_class = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  switch (query->type) {
    case GST_QUERY_DURATION:{
      GstClockTime duration = -1;
      GstFormat fmt;

      gst_query_parse_duration (query, &fmt, NULL);

      if (gst_adaptive_demux_is_live (demux)) {
        /* We are able to answer this query: the duration is unknown */
        gst_query_set_duration (query, fmt, -1);
        ret = TRUE;
        break;
      }

      if (fmt == GST_FORMAT_TIME
          && g_atomic_int_get (&demux->priv->have_manifest)) {
        duration = demux_class->get_duration (demux);

        if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0) {
          gst_query_set_duration (query, GST_FORMAT_TIME, duration);
          ret = TRUE;
        }
      }

      GST_LOG_OBJECT (demux, "GST_QUERY_DURATION returns %s with duration %"
          GST_TIME_FORMAT, ret ? "TRUE" : "FALSE", GST_TIME_ARGS (duration));
      break;
    }
    case GST_QUERY_LATENCY:{
      gst_query_set_latency (query, FALSE, 0, -1);
      ret = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gint64 stop = -1;
      gint64 start = 0;

      if (!g_atomic_int_get (&demux->priv->have_manifest)) {
        GST_INFO_OBJECT (demux,
            "Don't have manifest yet, can't answer seeking query");
        return FALSE;           /* can't answer without manifest */
      }

      GST_MANIFEST_LOCK (demux);

      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      GST_INFO_OBJECT (demux, "Received GST_QUERY_SEEKING with format %d", fmt);
      if (fmt == GST_FORMAT_TIME) {
        GstClockTime duration;
        gboolean can_seek = gst_adaptive_demux_can_seek (demux);

        ret = TRUE;
        if (can_seek) {
          if (gst_adaptive_demux_is_live (demux)) {
            ret = gst_adaptive_demux_get_live_seek_range (demux, &start, &stop);
            if (!ret) {
              GST_MANIFEST_UNLOCK (demux);
              GST_INFO_OBJECT (demux, "can't answer seeking query");
              return FALSE;
            }
          } else {
            duration = demux_class->get_duration (demux);
            if (GST_CLOCK_TIME_IS_VALID (duration) && duration > 0)
              stop = duration;
          }
        }
        gst_query_set_seeking (query, fmt, can_seek, start, stop);
        GST_INFO_OBJECT (demux, "GST_QUERY_SEEKING returning with start : %"
            GST_TIME_FORMAT ", stop : %" GST_TIME_FORMAT,
            GST_TIME_ARGS (start), GST_TIME_ARGS (stop));
      }
      GST_MANIFEST_UNLOCK (demux);
      break;
    }
    case GST_QUERY_URI:

      GST_MANIFEST_LOCK (demux);

      /* TODO HLS can answer this differently it seems */
      if (demux->manifest_uri) {
        /* FIXME: (hls) Do we answer with the variant playlist, with the current
         * playlist or the the uri of the last downlowaded fragment? */
        gst_query_set_uri (query, demux->manifest_uri);
        ret = TRUE;
      }

      GST_MANIFEST_UNLOCK (demux);
      break;
    default:
      /* Don't forward queries upstream because of the special nature of this
       *  "demuxer", which relies on the upstream element only to be fed
       *  the Manifest
       */
      break;
  }

  return ret;
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_start_tasks (GstAdaptiveDemux * demux,
    gboolean start_preroll_streams)
{
  GList *iter;

  if (!gst_adaptive_demux_is_running (demux)) {
    GST_DEBUG_OBJECT (demux, "Not starting tasks due to shutdown");
    return;
  }

  GST_INFO_OBJECT (demux, "Starting streams' tasks");

  iter = start_preroll_streams ? demux->prepared_streams : demux->streams;

  for (; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;

    if (!start_preroll_streams) {
      g_mutex_lock (&stream->fragment_download_lock);
      stream->cancelled = FALSE;
      stream->replaced = FALSE;
      g_mutex_unlock (&stream->fragment_download_lock);
    }

    stream->last_ret = GST_FLOW_OK;
    gst_task_start (stream->download_task);
  }
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_stop_manifest_update_task (GstAdaptiveDemux * demux)
{
  gst_uri_downloader_cancel (demux->downloader);

  gst_task_stop (demux->priv->updates_task);

  g_mutex_lock (&demux->priv->updates_timed_lock);
  GST_DEBUG_OBJECT (demux, "requesting stop of the manifest update task");
  demux->priv->stop_updates_task = TRUE;
  g_cond_signal (&demux->priv->updates_timed_cond);
  g_mutex_unlock (&demux->priv->updates_timed_lock);
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_start_manifest_update_task (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *demux_class = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (gst_adaptive_demux_is_live (demux)) {
    gst_uri_downloader_reset (demux->downloader);
    g_mutex_lock (&demux->priv->updates_timed_lock);
    demux->priv->stop_updates_task = FALSE;
    g_mutex_unlock (&demux->priv->updates_timed_lock);
    /* Task to periodically update the manifest */
    if (demux_class->requires_periodical_playlist_update (demux)) {
      GST_DEBUG_OBJECT (demux, "requesting start of the manifest update task");
      gst_task_start (demux->priv->updates_task);
    }
  }
}

/* must be called with manifest_lock taken
 * This function will temporarily release manifest_lock in order to join the
 * download threads.
 * The api_lock will still protect it against other threads trying to modify
 * the demux element.
 */
static void
gst_adaptive_demux_stop_tasks (GstAdaptiveDemux * demux, gboolean stop_updates)
{
  int i;
  GList *iter;
  GList *list_to_process;

  GST_LOG_OBJECT (demux, "Stopping tasks");

  if (stop_updates)
    gst_adaptive_demux_stop_manifest_update_task (demux);

  list_to_process = demux->streams;
  for (i = 0; i < 2; ++i) {
    for (iter = list_to_process; iter; iter = g_list_next (iter)) {
      GstAdaptiveDemuxStream *stream = iter->data;

      g_mutex_lock (&stream->fragment_download_lock);
      stream->cancelled = TRUE;
      gst_task_stop (stream->download_task);
      g_cond_signal (&stream->fragment_download_cond);
      g_mutex_unlock (&stream->fragment_download_lock);
    }
    list_to_process = demux->prepared_streams;
  }

  GST_MANIFEST_UNLOCK (demux);
  g_mutex_lock (&demux->priv->preroll_lock);
  g_cond_broadcast (&demux->priv->preroll_cond);
  g_mutex_unlock (&demux->priv->preroll_lock);
  GST_MANIFEST_LOCK (demux);

  g_mutex_lock (&demux->priv->manifest_update_lock);
  g_cond_broadcast (&demux->priv->manifest_cond);
  g_mutex_unlock (&demux->priv->manifest_update_lock);

  /* need to release manifest_lock before stopping the src element.
   * The streams were asked to cancel, so they will not make any writes to demux
   * object. Even if we temporarily release manifest_lock, the demux->streams
   * cannot change and iter cannot be invalidated.
   */
  list_to_process = demux->streams;
  for (i = 0; i < 2; ++i) {
    for (iter = list_to_process; iter; iter = g_list_next (iter)) {
      GstAdaptiveDemuxStream *stream = iter->data;
      GstElement *src = stream->src;

      GST_MANIFEST_UNLOCK (demux);

      if (src) {
        gst_element_set_locked_state (src, TRUE);
        gst_element_set_state (src, GST_STATE_READY);
      }

      /* stream->download_task value never changes, so it is safe to read it
       * outside critical section
       */
      gst_task_join (stream->download_task);

      GST_MANIFEST_LOCK (demux);
    }
    list_to_process = demux->prepared_streams;
  }

  GST_MANIFEST_UNLOCK (demux);
  if (stop_updates)
    gst_task_join (demux->priv->updates_task);

  GST_MANIFEST_LOCK (demux);

  list_to_process = demux->streams;
  for (i = 0; i < 2; ++i) {
    for (iter = list_to_process; iter; iter = g_list_next (iter)) {
      GstAdaptiveDemuxStream *stream = iter->data;

      stream->download_error_count = 0;
      stream->need_header = TRUE;
    }
    list_to_process = demux->prepared_streams;
  }
  demux->priv->qos_earliest_time = GST_CLOCK_TIME_NONE;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_push_src_event (GstAdaptiveDemux * demux, GstEvent * event)
{
  GList *iter;
  gboolean ret = TRUE;

  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    gst_event_ref (event);
    ret = ret & gst_pad_push_event (stream->pad, event);
  }
  gst_event_unref (event);
  return ret;
}

/* must be called with manifest_lock taken */
void
gst_adaptive_demux_stream_set_caps (GstAdaptiveDemuxStream * stream,
    GstCaps * caps)
{
  GST_DEBUG_OBJECT (stream->pad, "setting new caps for stream %" GST_PTR_FORMAT,
      caps);
  gst_caps_replace (&stream->pending_caps, caps);
  gst_caps_unref (caps);
}

/* must be called with manifest_lock taken */
void
gst_adaptive_demux_stream_set_tags (GstAdaptiveDemuxStream * stream,
    GstTagList * tags)
{
  GST_DEBUG_OBJECT (stream->pad, "setting new tags for stream %" GST_PTR_FORMAT,
      tags);
  if (stream->pending_tags) {
    gst_tag_list_unref (stream->pending_tags);
  }
  stream->pending_tags = tags;
}

/* must be called with manifest_lock taken */
void
gst_adaptive_demux_stream_queue_event (GstAdaptiveDemuxStream * stream,
    GstEvent * event)
{
  stream->pending_events = g_list_append (stream->pending_events, event);
}

/* must be called with manifest_lock taken */
static guint64
_update_average_bitrate (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, guint64 new_bitrate)
{
  gint index = stream->moving_index % NUM_LOOKBACK_FRAGMENTS;

  stream->moving_bitrate -= stream->fragment_bitrates[index];
  stream->fragment_bitrates[index] = new_bitrate;
  stream->moving_bitrate += new_bitrate;

  stream->moving_index += 1;

  if (stream->moving_index > NUM_LOOKBACK_FRAGMENTS)
    return stream->moving_bitrate / NUM_LOOKBACK_FRAGMENTS;
  return stream->moving_bitrate / stream->moving_index;
}

/* must be called with manifest_lock taken */
static guint64
gst_adaptive_demux_stream_update_current_bitrate (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  guint64 average_bitrate;
  guint64 fragment_bitrate;

  if (demux->connection_speed) {
    GST_LOG_OBJECT (demux, "Connection-speed is set to %u kbps, using it",
        demux->connection_speed / 1000);
    stream->current_download_rate = demux->connection_speed;
    return demux->connection_speed;
  }

  fragment_bitrate = stream->last_bitrate;
  GST_DEBUG_OBJECT (demux, "Download bitrate is : %" G_GUINT64_FORMAT " bps",
      fragment_bitrate);

  average_bitrate = _update_average_bitrate (demux, stream, fragment_bitrate);

  GST_INFO_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
      "last fragment bitrate was %" G_GUINT64_FORMAT, fragment_bitrate);
  GST_INFO_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
      "Last %u fragments average bitrate is %" G_GUINT64_FORMAT,
      NUM_LOOKBACK_FRAGMENTS, average_bitrate);

  /* Conservative approach, make sure we don't upgrade too fast */
  stream->current_download_rate = MIN (average_bitrate, fragment_bitrate);

  stream->current_download_rate *= demux->bitrate_limit;
  GST_DEBUG_OBJECT (demux, "Bitrate after bitrate limit (%0.2f): %"
      G_GUINT64_FORMAT, demux->bitrate_limit, stream->current_download_rate);

#if 0
  /* Debugging code, modulate the bitrate every few fragments */
  {
    static guint ctr = 0;
    if (ctr % 3 == 0) {
      GST_INFO_OBJECT (demux, "Halving reported bitrate for debugging");
      stream->current_download_rate /= 2;
    }
    ctr++;
  }
#endif

  return stream->current_download_rate;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_combine_flows (GstAdaptiveDemux * demux)
{
  gboolean all_notlinked = TRUE;
  gboolean all_eos = TRUE;
  GList *iter;

  for (iter = demux->streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;

    if (stream->last_ret != GST_FLOW_NOT_LINKED) {
      all_notlinked = FALSE;
      if (stream->last_ret != GST_FLOW_EOS)
        all_eos = FALSE;
    }

    if (stream->last_ret <= GST_FLOW_NOT_NEGOTIATED
        || stream->last_ret == GST_FLOW_FLUSHING) {
      return stream->last_ret;
    }
  }
  if (all_notlinked)
    return GST_FLOW_NOT_LINKED;
  else if (all_eos)
    return GST_FLOW_EOS;
  return GST_FLOW_OK;
}

/* Called with preroll_lock */
static void
gst_adaptive_demux_handle_preroll (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  demux->priv->preroll_pending--;
  if (demux->priv->preroll_pending == 0) {
    /* That was the last one, time to release all streams
     * and expose them */
    GST_DEBUG_OBJECT (demux, "All streams prerolled. exposing");
    gst_adaptive_demux_expose_streams (demux);
    g_cond_broadcast (&demux->priv->preroll_cond);
  }
}

/* must be called with manifest_lock taken.
 * Temporarily releases manifest_lock
 */
GstFlowReturn
gst_adaptive_demux_stream_push_buffer (GstAdaptiveDemuxStream * stream,
    GstBuffer * buffer)
{
  GstAdaptiveDemux *demux = stream->demux;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean discont = FALSE;
  /* Pending events */
  GstEvent *pending_caps = NULL, *pending_segment = NULL, *pending_tags = NULL;
  GList *pending_events = NULL;

  /* FIXME :
   * This is duplicating *exactly* the same thing as what is done at the beginning
   * of _src_chain if starting_fragment is TRUE */
  if (stream->first_fragment_buffer) {
    GstClockTime offset =
        gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
    GstClockTime period_start =
        gst_adaptive_demux_get_period_start_time (demux);

    GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
    if (demux->segment.rate < 0)
      /* Set DISCONT flag for every first buffer in reverse playback mode
       * as each fragment for its own has to be reversed */
      discont = TRUE;

    GST_BUFFER_PTS (buffer) = stream->fragment.timestamp;
    if (GST_BUFFER_PTS_IS_VALID (buffer))
      GST_BUFFER_PTS (buffer) += offset;

    if (GST_BUFFER_PTS_IS_VALID (buffer)) {
      stream->segment.position = GST_BUFFER_PTS (buffer);

      /* Convert from position inside the stream's segment to the demuxer's
       * segment, they are not necessarily the same */
      if (stream->segment.position - offset + period_start >
          demux->segment.position)
        demux->segment.position =
            stream->segment.position - offset + period_start;
    }
    GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

    GST_LOG_OBJECT (stream->pad,
        "Going to push buffer with PTS %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_PTS (buffer)));
  } else {
    GST_BUFFER_PTS (buffer) = GST_CLOCK_TIME_NONE;
  }

  if (stream->discont) {
    discont = TRUE;
    stream->discont = FALSE;
  }

  if (discont) {
    GST_DEBUG_OBJECT (stream->pad, "Marking fragment as discontinuous");
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DISCONT);
  } else {
    GST_BUFFER_FLAG_UNSET (buffer, GST_BUFFER_FLAG_DISCONT);
  }

  stream->first_fragment_buffer = FALSE;

  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;
  if (G_UNLIKELY (stream->pending_caps)) {
    pending_caps = gst_event_new_caps (stream->pending_caps);
    gst_caps_unref (stream->pending_caps);
    stream->pending_caps = NULL;
  }

  if (stream->do_block) {

    g_mutex_lock (&demux->priv->preroll_lock);

    /* If we are preroll state, set caps in here */
    if (pending_caps) {
      gst_pad_push_event (stream->pad, pending_caps);
      pending_caps = NULL;
    }

    gst_adaptive_demux_handle_preroll (demux, stream);
    GST_MANIFEST_UNLOCK (demux);

    while (stream->do_block && !stream->cancelled) {
      GST_LOG_OBJECT (demux, "Stream %p sleeping for preroll", stream);
      g_cond_wait (&demux->priv->preroll_cond, &demux->priv->preroll_lock);
    }
    if (stream->cancelled) {
      GST_LOG_OBJECT (demux, "stream %p cancelled", stream);
      gst_buffer_unref (buffer);
      g_mutex_unlock (&demux->priv->preroll_lock);
      return GST_FLOW_FLUSHING;
    }

    g_mutex_unlock (&demux->priv->preroll_lock);
    GST_MANIFEST_LOCK (demux);
  }

  if (G_UNLIKELY (stream->pending_segment)) {
    GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
    pending_segment = stream->pending_segment;
    stream->pending_segment = NULL;
    GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);
  }
  if (G_UNLIKELY (stream->pending_tags || stream->bitrate_changed)) {
    GstTagList *tags = stream->pending_tags;

    stream->pending_tags = NULL;
    stream->bitrate_changed = 0;

    if (stream->fragment.bitrate != 0) {
      if (tags)
        tags = gst_tag_list_make_writable (tags);
      else
        tags = gst_tag_list_new_empty ();

      gst_tag_list_add (tags, GST_TAG_MERGE_KEEP,
          GST_TAG_NOMINAL_BITRATE, stream->fragment.bitrate, NULL);
    }
    if (tags)
      pending_tags = gst_event_new_tag (tags);
  }
  if (G_UNLIKELY (stream->pending_events)) {
    pending_events = stream->pending_events;
    stream->pending_events = NULL;
  }

  GST_MANIFEST_UNLOCK (demux);

  /* Do not push events or buffers holding the manifest lock */
  if (G_UNLIKELY (pending_caps)) {
    GST_DEBUG_OBJECT (stream->pad, "Setting pending caps: %" GST_PTR_FORMAT,
        pending_caps);
    gst_pad_push_event (stream->pad, pending_caps);
  }
  if (G_UNLIKELY (pending_segment)) {
    GST_DEBUG_OBJECT (stream->pad, "Sending pending seg: %" GST_PTR_FORMAT,
        pending_segment);
    gst_pad_push_event (stream->pad, pending_segment);
  }
  if (G_UNLIKELY (pending_tags)) {
    GST_DEBUG_OBJECT (stream->pad, "Sending pending tags: %" GST_PTR_FORMAT,
        pending_tags);
    gst_pad_push_event (stream->pad, pending_tags);
  }
  while (pending_events != NULL) {
    GstEvent *event = pending_events->data;

    if (!gst_pad_push_event (stream->pad, event))
      GST_ERROR_OBJECT (stream->pad, "Failed to send pending event");

    pending_events = g_list_delete_link (pending_events, pending_events);
  }

  /* Wait for preroll if blocking */
  GST_DEBUG_OBJECT (stream->pad,
      "About to push buffer of size %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buffer));

  ret = gst_pad_push (stream->pad, buffer);

  GST_MANIFEST_LOCK (demux);

  g_mutex_lock (&stream->fragment_download_lock);
  if (G_UNLIKELY (stream->cancelled)) {
    GST_LOG_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
        "Stream was cancelled");
    ret = stream->last_ret = GST_FLOW_FLUSHING;
    g_mutex_unlock (&stream->fragment_download_lock);
    return ret;
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  GST_LOG_OBJECT (stream->pad, "Push result: %d %s", ret,
      gst_flow_get_name (ret));

  return ret;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_stream_finish_fragment_default (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  /* No need to advance, this isn't a real fragment */
  if (G_UNLIKELY (stream->downloading_header || stream->downloading_index))
    return GST_FLOW_OK;

  return gst_adaptive_demux_stream_advance_fragment (demux, stream,
      stream->fragment.duration);
}

/* must be called with manifest_lock taken.
 * Can temporarily release manifest_lock
 */
static GstFlowReturn
gst_adaptive_demux_stream_data_received_default (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer)
{
  return gst_adaptive_demux_stream_push_buffer (stream, buffer);
}

static gboolean
gst_adaptive_demux_requires_periodical_playlist_update_default (GstAdaptiveDemux
    * demux)
{
  return TRUE;
}

static GstFlowReturn
_src_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstAdaptiveDemuxStream *stream;
  GstAdaptiveDemux *demux;
  GstAdaptiveDemuxClass *klass;
  GstFlowReturn ret = GST_FLOW_OK;

  demux = GST_ADAPTIVE_DEMUX_CAST (parent);
  stream = gst_pad_get_element_private (pad);
  klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  GST_MANIFEST_LOCK (demux);

  /* do not make any changes if the stream is cancelled */
  g_mutex_lock (&stream->fragment_download_lock);
  if (G_UNLIKELY (stream->cancelled)) {
    g_mutex_unlock (&stream->fragment_download_lock);
    gst_buffer_unref (buffer);
    ret = stream->last_ret = GST_FLOW_FLUSHING;
    GST_MANIFEST_UNLOCK (demux);
    return ret;
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  /* starting_fragment is set to TRUE at the beginning of
   * _stream_download_fragment()
   * /!\ If there is a header/index being downloaded, then this will
   * be TRUE for the first one ... but FALSE for the remaining ones,
   * including the *actual* fragment ! */
  if (stream->starting_fragment) {
    GstClockTime offset =
        gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
    GstClockTime period_start =
        gst_adaptive_demux_get_period_start_time (demux);

    stream->starting_fragment = FALSE;
    if (klass->start_fragment) {
      if (!klass->start_fragment (demux, stream)) {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    GST_BUFFER_PTS (buffer) = stream->fragment.timestamp;
    if (GST_BUFFER_PTS_IS_VALID (buffer))
      GST_BUFFER_PTS (buffer) += offset;

    GST_LOG_OBJECT (stream->pad, "set fragment pts=%" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_PTS (buffer)));

    if (GST_BUFFER_PTS_IS_VALID (buffer)) {
      GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
      stream->segment.position = GST_BUFFER_PTS (buffer);

      /* Convert from position inside the stream's segment to the demuxer's
       * segment, they are not necessarily the same */
      if (stream->segment.position - offset + period_start >
          demux->segment.position)
        demux->segment.position =
            stream->segment.position - offset + period_start;
      GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);
    }

  } else {
    GST_BUFFER_PTS (buffer) = GST_CLOCK_TIME_NONE;
  }

  /* downloading_first_buffer is set to TRUE in download_uri() just before
   * activating the source (i.e. requesting a given URI)
   *
   * The difference with starting_fragment is that this will be called
   * for *all* first buffers (of index, and header, and fragment)
   *
   * ... to then only do something useful (in this block) for actual
   * fragments... */
  if (stream->downloading_first_buffer) {
    gint64 chunk_size = 0;

    stream->downloading_first_buffer = FALSE;

    if (!stream->downloading_header && !stream->downloading_index) {
      /* If this is the first buffer of a fragment (not the headers or index)
       * and we don't have a bitrate from the sub-class, then see if we
       * can work it out from the fragment size and duration */
      if (stream->fragment.bitrate == 0 &&
          stream->fragment.duration != 0 &&
          gst_element_query_duration (stream->uri_handler, GST_FORMAT_BYTES,
              &chunk_size) && chunk_size != -1) {
        guint bitrate = MIN (G_MAXUINT, gst_util_uint64_scale (chunk_size,
                8 * GST_SECOND, stream->fragment.duration));
        GST_LOG_OBJECT (demux,
            "Fragment has size %" G_GINT64_FORMAT " duration %" GST_TIME_FORMAT
            " = bitrate %u", chunk_size,
            GST_TIME_ARGS (stream->fragment.duration), bitrate);
        stream->fragment.bitrate = bitrate;
      }
      if (stream->fragment.bitrate) {
        stream->bitrate_changed = TRUE;
      } else {
        GST_WARNING_OBJECT (demux, "Bitrate for fragment not available");
      }
    }
  }

  stream->download_total_bytes += gst_buffer_get_size (buffer);

  GST_TRACE_OBJECT (stream->pad, "Received buffer of size %" G_GSIZE_FORMAT,
      gst_buffer_get_size (buffer));

  ret = klass->data_received (demux, stream, buffer);

  if (ret == GST_FLOW_FLUSHING) {
    /* do not make any changes if the stream is cancelled */
    g_mutex_lock (&stream->fragment_download_lock);
    if (G_UNLIKELY (stream->cancelled)) {
      g_mutex_unlock (&stream->fragment_download_lock);
      GST_MANIFEST_UNLOCK (demux);
      return ret;
    }
    g_mutex_unlock (&stream->fragment_download_lock);
  }

  if (ret != GST_FLOW_OK) {
    gboolean finished = FALSE;

    if (ret < GST_FLOW_EOS) {
      GST_ELEMENT_FLOW_ERROR (demux, ret);

      /* TODO push this on all pads */
      gst_pad_push_event (stream->pad, gst_event_new_eos ());
    } else {
      GST_DEBUG_OBJECT (stream->pad, "stream stopped, reason %s",
          gst_flow_get_name (ret));
    }

    if (ret == (GstFlowReturn) GST_ADAPTIVE_DEMUX_FLOW_SWITCH) {
      ret = GST_FLOW_EOS;       /* return EOS to make the source stop */
    } else if (ret == GST_ADAPTIVE_DEMUX_FLOW_END_OF_FRAGMENT) {
      /* Behaves like an EOS event from upstream */
      stream->fragment.finished = TRUE;
      ret = klass->finish_fragment (demux, stream);
      if (ret == (GstFlowReturn) GST_ADAPTIVE_DEMUX_FLOW_SWITCH) {
        ret = GST_FLOW_EOS;     /* return EOS to make the source stop */
      } else if (ret != GST_FLOW_OK) {
        goto error;
      }
      finished = TRUE;
    }

    gst_adaptive_demux_stream_fragment_download_finish (stream, ret, NULL);
    if (finished)
      ret = GST_FLOW_EOS;
  }

error:

  GST_MANIFEST_UNLOCK (demux);

  return ret;
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_stream_fragment_download_finish (GstAdaptiveDemuxStream *
    stream, GstFlowReturn ret, GError * err)
{
  GST_DEBUG_OBJECT (stream->pad, "Download finish: %d %s - err: %p", ret,
      gst_flow_get_name (ret), err);

  /* if we have an error, only replace last_ret if it was OK before to avoid
   * overwriting the first error we got */
  if (stream->last_ret == GST_FLOW_OK) {
    stream->last_ret = ret;
    if (err) {
      g_clear_error (&stream->last_error);
      stream->last_error = g_error_copy (err);
    }
  }
  g_mutex_lock (&stream->fragment_download_lock);
  stream->download_finished = TRUE;
  g_cond_signal (&stream->fragment_download_cond);
  g_mutex_unlock (&stream->fragment_download_lock);
}

static GstFlowReturn
gst_adaptive_demux_eos_handling (GstAdaptiveDemuxStream * stream)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (stream->demux);

  if (!klass->need_another_chunk || stream->fragment.chunk_size == -1
      || !klass->need_another_chunk (stream)
      || stream->fragment.chunk_size == 0) {
    stream->fragment.finished = TRUE;

    /* Last chance to figure out a fallback nominal bitrate if neither baseclass
       nor the HTTP Content-Length implementation worked. */
    if (stream->fragment.bitrate == 0 && stream->fragment.duration != 0 &&
        stream->fragment_bytes_downloaded != 0 && !stream->downloading_index &&
        !stream->downloading_header) {
      guint bitrate = MIN (G_MAXUINT,
          gst_util_uint64_scale (stream->fragment_bytes_downloaded,
              8 * GST_SECOND, stream->fragment.duration));
      GST_LOG_OBJECT (stream->pad,
          "Fragment has size %" G_GUINT64_FORMAT " duration %" GST_TIME_FORMAT
          " = bitrate %u", stream->fragment_bytes_downloaded,
          GST_TIME_ARGS (stream->fragment.duration), bitrate);
      stream->fragment.bitrate = bitrate;
      stream->bitrate_changed = TRUE;
    }
    ret = klass->finish_fragment (stream->demux, stream);
  }
  gst_adaptive_demux_stream_fragment_download_finish (stream, ret, NULL);

  return ret;
}

static gboolean
_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstAdaptiveDemuxStream *stream = gst_pad_get_element_private (pad);
  GstAdaptiveDemux *demux = stream->demux;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:{
      GST_DEBUG_OBJECT (pad, "Saw EOS on src pad");
      GST_MANIFEST_LOCK (demux);

      gst_adaptive_demux_eos_handling (stream);

      /* FIXME ?
       * _eos_handling() calls  fragment_download_finish() which does the
       * same thing as below.
       * Could this cause races ? */
      g_mutex_lock (&stream->fragment_download_lock);
      stream->download_finished = TRUE;
      g_cond_signal (&stream->fragment_download_cond);
      g_mutex_unlock (&stream->fragment_download_lock);

      GST_MANIFEST_UNLOCK (demux);
      break;
    }
    default:
      break;
  }

  gst_event_unref (event);

  return TRUE;
}

static gboolean
_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstAdaptiveDemuxStream *stream = gst_pad_get_element_private (pad);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ALLOCATION:
      return FALSE;
      break;
    default:
      break;
  }

  return gst_pad_peer_query (stream->pad, query);
}

static GstPadProbeReturn
_uri_handler_probe (GstPad * pad, GstPadProbeInfo * info,
    GstAdaptiveDemuxStream * stream)
{
  GstPadProbeReturn ret = GST_PAD_PROBE_OK;

  if (GST_PAD_PROBE_INFO_TYPE (info) & GST_PAD_PROBE_TYPE_BUFFER) {
    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER (info);
    if (stream->fragment_bytes_downloaded == 0) {
      stream->last_latency =
          gst_adaptive_demux_get_monotonic_time (stream->demux) -
          (stream->download_start_time * GST_USECOND);
      GST_DEBUG_OBJECT (pad,
          "FIRST BYTE since download_start %" GST_TIME_FORMAT,
          GST_TIME_ARGS (stream->last_latency));
    }
    stream->fragment_bytes_downloaded += gst_buffer_get_size (buf);
    GST_LOG_OBJECT (pad,
        "Received buffer, size %" G_GSIZE_FORMAT " total %" G_GUINT64_FORMAT,
        gst_buffer_get_size (buf), stream->fragment_bytes_downloaded);
  } else if (GST_PAD_PROBE_INFO_TYPE (info) &
      GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *ev = GST_PAD_PROBE_INFO_EVENT (info);
    GST_LOG_OBJECT (pad, "Received event %s %" GST_PTR_FORMAT,
        GST_EVENT_TYPE_NAME (ev), ev);
    switch (GST_EVENT_TYPE (ev)) {
      case GST_EVENT_SEGMENT:
        stream->fragment_bytes_downloaded = 0;
        break;
      case GST_EVENT_EOS:
      {
        stream->last_download_time =
            gst_adaptive_demux_get_monotonic_time (stream->demux) -
            (stream->download_start_time * GST_USECOND);
        stream->last_bitrate =
            gst_util_uint64_scale (stream->fragment_bytes_downloaded,
            8 * GST_SECOND, stream->last_download_time);
        GST_DEBUG_OBJECT (pad,
            "EOS since download_start %" GST_TIME_FORMAT " bitrate %"
            G_GUINT64_FORMAT " bps", GST_TIME_ARGS (stream->last_download_time),
            stream->last_bitrate);
        /* Calculate bitrate since URI request */
      }
        break;
      default:
        break;
    }
  }

  return ret;
}

/* must be called with manifest_lock taken.
 * Can temporarily release manifest_lock
 */
static gboolean
gst_adaptive_demux_stream_wait_manifest_update (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  gboolean ret = TRUE;

  /* Wait until we're cancelled or there's something for
   * us to download in the playlist or the playlist
   * became non-live */
  while (TRUE) {
    GST_DEBUG_OBJECT (demux, "No fragment left but live playlist, wait a bit");

    /* get the manifest_update_lock while still holding the manifest_lock.
     * This will prevent other threads to signal the condition (they will need
     * both manifest_lock and manifest_update_lock in order to signal).
     * It cannot deadlock because all threads always get the manifest_lock first
     * and manifest_update_lock second.
     */
    g_mutex_lock (&demux->priv->manifest_update_lock);

    GST_MANIFEST_UNLOCK (demux);

    g_cond_wait (&demux->priv->manifest_cond,
        &demux->priv->manifest_update_lock);
    g_mutex_unlock (&demux->priv->manifest_update_lock);

    GST_MANIFEST_LOCK (demux);

    /* check for cancelled every time we get the manifest_lock */
    g_mutex_lock (&stream->fragment_download_lock);
    if (G_UNLIKELY (stream->cancelled)) {
      ret = FALSE;
      stream->last_ret = GST_FLOW_FLUSHING;
      g_mutex_unlock (&stream->fragment_download_lock);
      break;
    }
    g_mutex_unlock (&stream->fragment_download_lock);

    /* Got a new fragment or not live anymore? */
    if (gst_adaptive_demux_stream_update_fragment_info (demux, stream) ==
        GST_FLOW_OK) {
      GST_DEBUG_OBJECT (demux, "new fragment available, "
          "not waiting for manifest update");
      ret = TRUE;
      break;
    }

    if (!gst_adaptive_demux_is_live (demux)) {
      GST_DEBUG_OBJECT (demux, "Not live anymore, "
          "not waiting for manifest update");
      ret = FALSE;
      break;
    }
  }
  GST_DEBUG_OBJECT (demux, "Retrying now");
  return ret;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_stream_update_source (GstAdaptiveDemuxStream * stream,
    const gchar * uri, const gchar * referer, gboolean refresh,
    gboolean allow_cache)
{
  GstAdaptiveDemux *demux = stream->demux;

  if (!gst_uri_is_valid (uri)) {
    GST_WARNING_OBJECT (stream->pad, "Invalid URI: %s", uri);
    return FALSE;
  }

  /* Try to re-use existing source element */
  if (stream->src != NULL) {
    gchar *old_protocol, *new_protocol;
    gchar *old_uri;

    old_uri = gst_uri_handler_get_uri (GST_URI_HANDLER (stream->uri_handler));
    old_protocol = gst_uri_get_protocol (old_uri);
    new_protocol = gst_uri_get_protocol (uri);

    if (!g_str_equal (old_protocol, new_protocol)) {
      GstElement *src = stream->src;

      stream->src = NULL;
      gst_object_unref (stream->src_srcpad);
      stream->src_srcpad = NULL;
      GST_MANIFEST_UNLOCK (demux);
      gst_element_set_locked_state (src, TRUE);
      gst_element_set_state (src, GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (demux), src);
      GST_MANIFEST_LOCK (demux);
      GST_DEBUG_OBJECT (demux, "Can't re-use old source element");
    } else {
      GError *err = NULL;

      GST_DEBUG_OBJECT (demux, "Re-using old source element");
      if (!gst_uri_handler_set_uri (GST_URI_HANDLER (stream->uri_handler), uri,
              &err)) {
        GstElement *src = stream->src;

        stream->src = NULL;
        GST_DEBUG_OBJECT (demux, "Failed to re-use old source element: %s",
            err ? err->message : "Unknown error");
        g_clear_error (&err);
        gst_object_unref (stream->src_srcpad);
        stream->src_srcpad = NULL;
        GST_MANIFEST_UNLOCK (demux);
        gst_element_set_locked_state (src, TRUE);
        gst_element_set_state (src, GST_STATE_NULL);
        gst_bin_remove (GST_BIN_CAST (demux), src);
        GST_MANIFEST_LOCK (demux);
      }
    }
    g_free (old_uri);
    g_free (old_protocol);
    g_free (new_protocol);
  }

  if (stream->src == NULL) {
    GstPad *uri_handler_src;
    GstPad *queue_sink;
    GstPad *queue_src;
    GstElement *uri_handler;
    GstElement *queue;
    GstPadLinkReturn pad_link_ret;
    GObjectClass *gobject_class;
    gchar *internal_name, *bin_name;

    /* Our src consists of a bin containing uri_handler -> queue . The
     * purpose of the queue is to allow the uri_handler to download an
     * entire fragment without blocking, so we can accurately measure the
     * download bitrate. */

    queue = gst_element_factory_make ("queue", NULL);
    if (queue == NULL)
      return FALSE;

    g_object_set (queue, "max-size-bytes", (guint) SRC_QUEUE_MAX_BYTES, NULL);
    g_object_set (queue, "max-size-buffers", (guint) 0, NULL);
    g_object_set (queue, "max-size-time", (guint64) 0, NULL);

    uri_handler = gst_element_make_from_uri (GST_URI_SRC, uri, NULL, NULL);
    if (uri_handler == NULL) {
      GST_ELEMENT_ERROR (demux, CORE, MISSING_PLUGIN,
          ("Missing plugin to handle URI: '%s'", uri), (NULL));
      gst_object_unref (queue);
      return FALSE;
    }

    gobject_class = G_OBJECT_GET_CLASS (uri_handler);

    if (g_object_class_find_property (gobject_class, "compress"))
      g_object_set (uri_handler, "compress", FALSE, NULL);
    if (g_object_class_find_property (gobject_class, "keep-alive"))
      g_object_set (uri_handler, "keep-alive", TRUE, NULL);
    if (g_object_class_find_property (gobject_class, "extra-headers")) {
      if (referer || refresh || !allow_cache) {
        GstStructure *extra_headers = gst_structure_new_empty ("headers");

        if (referer)
          gst_structure_set (extra_headers, "Referer", G_TYPE_STRING, referer,
              NULL);

        if (!allow_cache)
          gst_structure_set (extra_headers, "Cache-Control", G_TYPE_STRING,
              "no-cache", NULL);
        else if (refresh)
          gst_structure_set (extra_headers, "Cache-Control", G_TYPE_STRING,
              "max-age=0", NULL);

        g_object_set (uri_handler, "extra-headers", extra_headers, NULL);

        gst_structure_free (extra_headers);
      } else {
        g_object_set (uri_handler, "extra-headers", NULL, NULL);
      }
    }

    /* Source bin creation */
    bin_name = g_strdup_printf ("srcbin-%s", GST_PAD_NAME (stream->pad));
    stream->src = gst_bin_new (bin_name);
    g_free (bin_name);
    if (stream->src == NULL) {
      gst_object_unref (queue);
      gst_object_unref (uri_handler);
      return FALSE;
    }

    gst_bin_add (GST_BIN_CAST (stream->src), queue);
    gst_bin_add (GST_BIN_CAST (stream->src), uri_handler);

    uri_handler_src = gst_element_get_static_pad (uri_handler, "src");
    queue_sink = gst_element_get_static_pad (queue, "sink");

    pad_link_ret =
        gst_pad_link_full (uri_handler_src, queue_sink,
        GST_PAD_LINK_CHECK_NOTHING);
    if (GST_PAD_LINK_FAILED (pad_link_ret)) {
      GST_WARNING_OBJECT (demux,
          "Could not link pads %s:%s to %s:%s for reason %d",
          GST_DEBUG_PAD_NAME (uri_handler_src), GST_DEBUG_PAD_NAME (queue_sink),
          pad_link_ret);
      g_object_unref (queue_sink);
      g_object_unref (uri_handler_src);
      gst_object_unref (stream->src);
      stream->src = NULL;
      return FALSE;
    }

    /* Add a downstream event and data probe */
    gst_pad_add_probe (uri_handler_src, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
        (GstPadProbeCallback) _uri_handler_probe, stream, NULL);

    g_object_unref (queue_sink);
    g_object_unref (uri_handler_src);
    queue_src = gst_element_get_static_pad (queue, "src");
    stream->src_srcpad = gst_ghost_pad_new ("src", queue_src);
    g_object_unref (queue_src);
    gst_element_add_pad (stream->src, stream->src_srcpad);

    gst_element_set_locked_state (stream->src, TRUE);
    gst_bin_add (GST_BIN_CAST (demux), stream->src);
    stream->src_srcpad = gst_element_get_static_pad (stream->src, "src");

    /* set up our internal floating pad to drop all events from
     * the http src we don't care about. On the chain function
     * we just push the buffer forward */
    internal_name = g_strdup_printf ("internal-%s", GST_PAD_NAME (stream->pad));
    stream->internal_pad = gst_pad_new (internal_name, GST_PAD_SINK);
    g_free (internal_name);
    gst_object_set_parent (GST_OBJECT_CAST (stream->internal_pad),
        GST_OBJECT_CAST (demux));
    GST_OBJECT_FLAG_SET (stream->internal_pad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_element_private (stream->internal_pad, stream);
    gst_pad_set_active (stream->internal_pad, TRUE);
    gst_pad_set_chain_function (stream->internal_pad, _src_chain);
    gst_pad_set_event_function (stream->internal_pad, _src_event);
    gst_pad_set_query_function (stream->internal_pad, _src_query);

    if (gst_pad_link_full (stream->src_srcpad, stream->internal_pad,
            GST_PAD_LINK_CHECK_NOTHING) != GST_PAD_LINK_OK) {
      GST_ERROR_OBJECT (stream->pad, "Failed to link internal pad");
      return FALSE;
    }

    stream->uri_handler = uri_handler;
    stream->queue = queue;

    stream->last_status_code = 200;     /* default to OK */
  }
  return TRUE;
}

static GstPadProbeReturn
gst_ad_stream_src_to_ready_cb (GstPad * pad, GstPadProbeInfo * info,
    gpointer user_data)
{
  GstAdaptiveDemuxStream *stream = user_data;

  /* The source's src pad is IDLE so now set the state to READY */
  g_mutex_lock (&stream->fragment_download_lock);
  stream->src_at_ready = TRUE;
  g_cond_signal (&stream->fragment_download_cond);
  g_mutex_unlock (&stream->fragment_download_lock);

  return GST_PAD_PROBE_REMOVE;
}

#ifndef GST_DISABLE_GST_DEBUG
static const char *
uritype (GstAdaptiveDemuxStream * s)
{
  if (s->downloading_header)
    return "header";
  if (s->downloading_index)
    return "index";
  return "fragment";
}
#endif

/* must be called with manifest_lock taken.
 * Can temporarily release manifest_lock
 *
 * Will return when URI is fully downloaded (or aborted/errored)
 */
static GstFlowReturn
gst_adaptive_demux_stream_download_uri (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, const gchar * uri, gint64 start,
    gint64 end, guint * http_status)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GST_DEBUG_OBJECT (stream->pad,
      "Downloading %s uri: %s, range:%" G_GINT64_FORMAT " - %" G_GINT64_FORMAT,
      uritype (stream), uri, start, end);

  if (http_status)
    *http_status = 200;         /* default to ok if no further information */

  if (!gst_adaptive_demux_stream_update_source (stream, uri, NULL, FALSE, TRUE)) {
    ret = stream->last_ret = GST_FLOW_ERROR;
    return ret;
  }

  gst_element_set_locked_state (stream->src, TRUE);

  GST_MANIFEST_UNLOCK (demux);
  if (gst_element_set_state (stream->src,
          GST_STATE_READY) != GST_STATE_CHANGE_FAILURE) {
    /* If ranges are specified, seek to it */
    if (start != 0 || end != -1) {
      /* HTTP ranges are inclusive, GStreamer segments are exclusive for the
       * stop position */
      if (end != -1)
        end += 1;
      /* Send the seek event to the uri_handler, as the other pipeline elements
       * can't handle it when READY. */
      if (!gst_element_send_event (stream->uri_handler, gst_event_new_seek (1.0,
                  GST_FORMAT_BYTES, (GstSeekFlags) GST_SEEK_FLAG_FLUSH,
                  GST_SEEK_TYPE_SET, start, GST_SEEK_TYPE_SET, end))) {

        GST_MANIFEST_LOCK (demux);
        /* looks like the source can't handle seeks in READY */
        g_clear_error (&stream->last_error);
        stream->last_error = g_error_new (GST_CORE_ERROR,
            GST_CORE_ERROR_NOT_IMPLEMENTED,
            "Source element can't handle range requests");
        stream->last_ret = GST_FLOW_ERROR;
      } else {
        GST_MANIFEST_LOCK (demux);
      }
    } else {
      GST_MANIFEST_LOCK (demux);
    }

    if (G_LIKELY (stream->last_ret == GST_FLOW_OK)) {
      stream->download_start_time =
          GST_TIME_AS_USECONDS (gst_adaptive_demux_get_monotonic_time (demux));

      /* src element is in state READY. Before we start it, we reset
       * download_finished
       */
      g_mutex_lock (&stream->fragment_download_lock);
      stream->download_finished = FALSE;
      stream->downloading_first_buffer = TRUE;
      g_mutex_unlock (&stream->fragment_download_lock);

      GST_MANIFEST_UNLOCK (demux);

      if (!gst_element_sync_state_with_parent (stream->src)) {
        GST_WARNING_OBJECT (demux, "Could not sync state for src element");
        GST_MANIFEST_LOCK (demux);
        ret = stream->last_ret = GST_FLOW_ERROR;
        return ret;
      }

      /* wait for the fragment to be completely downloaded */
      GST_DEBUG_OBJECT (stream->pad,
          "Waiting for %s download to finish: %s", uritype (stream), uri);

      g_mutex_lock (&stream->fragment_download_lock);
      stream->src_at_ready = FALSE;
      if (G_UNLIKELY (stream->cancelled)) {
        g_mutex_unlock (&stream->fragment_download_lock);
        GST_MANIFEST_LOCK (demux);
        ret = stream->last_ret = GST_FLOW_FLUSHING;
        return ret;
      }
      /* download_finished is only set:
       * * in ::fragment_download_finish()
       * * if EOS is received on the _src pad
       * */
      while (!stream->cancelled && !stream->download_finished) {
        g_cond_wait (&stream->fragment_download_cond,
            &stream->fragment_download_lock);
      }
      g_mutex_unlock (&stream->fragment_download_lock);

      GST_DEBUG_OBJECT (stream->pad,
          "Finished Waiting for %s download: %s", uritype (stream), uri);

      GST_MANIFEST_LOCK (demux);
      g_mutex_lock (&stream->fragment_download_lock);
      if (G_UNLIKELY (stream->cancelled)) {
        ret = stream->last_ret = GST_FLOW_FLUSHING;
        g_mutex_unlock (&stream->fragment_download_lock);
        return ret;
      }
      g_mutex_unlock (&stream->fragment_download_lock);

      ret = stream->last_ret;

      GST_DEBUG_OBJECT (stream->pad, "%s download finished: %s %d %s",
          uritype (stream), uri, stream->last_ret,
          gst_flow_get_name (stream->last_ret));
      if (stream->last_ret != GST_FLOW_OK && http_status) {
        *http_status = stream->last_status_code;
      }
    }

    /* changing src element state might try to join the streaming thread, so
     * we must not hold the manifest lock.
     */
    GST_MANIFEST_UNLOCK (demux);
  } else {
    GST_MANIFEST_UNLOCK (demux);
    if (stream->last_ret == GST_FLOW_OK)
      stream->last_ret = GST_FLOW_CUSTOM_ERROR;
    ret = GST_FLOW_CUSTOM_ERROR;
  }

  stream->src_at_ready = FALSE;

  gst_element_set_locked_state (stream->src, TRUE);
  gst_pad_add_probe (stream->src_srcpad, GST_PAD_PROBE_TYPE_IDLE,
      gst_ad_stream_src_to_ready_cb, stream, NULL);

  g_mutex_lock (&stream->fragment_download_lock);
  while (!stream->src_at_ready) {
    g_cond_wait (&stream->fragment_download_cond,
        &stream->fragment_download_lock);
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  gst_element_set_state (stream->src, GST_STATE_READY);

  /* Need to drop the fragment_download_lock to get the MANIFEST lock */
  GST_MANIFEST_LOCK (demux);
  g_mutex_lock (&stream->fragment_download_lock);
  if (G_UNLIKELY (stream->cancelled)) {
    ret = stream->last_ret = GST_FLOW_FLUSHING;
    g_mutex_unlock (&stream->fragment_download_lock);
    return ret;
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  /* deactivate and reactivate our ghostpad to make it fresh for a new
   * stream */
  gst_pad_set_active (stream->internal_pad, FALSE);
  gst_pad_set_active (stream->internal_pad, TRUE);

  return ret;
}

/* must be called with manifest_lock taken.
 * Can temporarily release manifest_lock
 */
static GstFlowReturn
gst_adaptive_demux_stream_download_header_fragment (GstAdaptiveDemuxStream *
    stream)
{
  GstAdaptiveDemux *demux = stream->demux;
  GstFlowReturn ret = GST_FLOW_OK;

  if (stream->fragment.header_uri != NULL) {
    GST_DEBUG_OBJECT (demux, "Fetching header %s %" G_GINT64_FORMAT "-%"
        G_GINT64_FORMAT, stream->fragment.header_uri,
        stream->fragment.header_range_start, stream->fragment.header_range_end);

    stream->downloading_header = TRUE;
    ret = gst_adaptive_demux_stream_download_uri (demux, stream,
        stream->fragment.header_uri, stream->fragment.header_range_start,
        stream->fragment.header_range_end, NULL);
    stream->downloading_header = FALSE;
  }

  /* check if we have an index */
  if (ret == GST_FLOW_OK) {     /* TODO check for other valid types */

    if (stream->fragment.index_uri != NULL) {
      GST_DEBUG_OBJECT (demux,
          "Fetching index %s %" G_GINT64_FORMAT "-%" G_GINT64_FORMAT,
          stream->fragment.index_uri,
          stream->fragment.index_range_start, stream->fragment.index_range_end);
      stream->downloading_index = TRUE;
      ret = gst_adaptive_demux_stream_download_uri (demux, stream,
          stream->fragment.index_uri, stream->fragment.index_range_start,
          stream->fragment.index_range_end, NULL);
      stream->downloading_index = FALSE;
    }
  }

  return ret;
}

/* must be called with manifest_lock taken.
 * Can temporarily release manifest_lock
 */
static GstFlowReturn
gst_adaptive_demux_stream_download_fragment (GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemux *demux = stream->demux;
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  gchar *url = NULL;
  GstFlowReturn ret;
  gboolean retried_once = FALSE, live;
  guint http_status;
  guint last_status_code;

  /* FIXME :  */
  /* THERE ARE THREE DIFFERENT VARIABLES FOR THE "BEGINNING" OF A FRAGMENT ! */
  stream->starting_fragment = TRUE;
  stream->last_ret = GST_FLOW_OK;
  stream->first_fragment_buffer = TRUE;

  GST_DEBUG_OBJECT (stream->pad, "Downloading %s%s%s",
      stream->fragment.uri ? "FRAGMENT " : "",
      stream->fragment.header_uri ? "HEADER " : "",
      stream->fragment.index_uri ? "INDEX" : "");

  if (stream->fragment.uri == NULL && stream->fragment.header_uri == NULL &&
      stream->fragment.index_uri == NULL)
    goto no_url_error;

  if (stream->need_header) {
    ret = gst_adaptive_demux_stream_download_header_fragment (stream);
    if (ret != GST_FLOW_OK) {
      return ret;
    }
    stream->need_header = FALSE;
  }

again:
  ret = GST_FLOW_OK;
  url = stream->fragment.uri;
  GST_DEBUG_OBJECT (stream->pad, "Got url '%s' for stream %p", url, stream);
  if (!url)
    return ret;

  stream->last_ret = GST_FLOW_OK;
  http_status = 200;

  /* Download the actual fragment, either in fragments or in one go */
  if (klass->need_another_chunk && klass->need_another_chunk (stream)
      && stream->fragment.chunk_size != 0) {
    /* Handle chunk downloading */
    gint64 range_start, range_end, chunk_start, chunk_end;
    guint64 download_total_bytes;
    gint chunk_size = stream->fragment.chunk_size;

    range_start = chunk_start = stream->fragment.range_start;
    range_end = stream->fragment.range_end;
    /* HTTP ranges are inclusive for the end */
    if (chunk_size != -1)
      chunk_end = range_start + chunk_size - 1;
    else
      chunk_end = range_end;

    if (range_end != -1)
      chunk_end = MIN (chunk_end, range_end);

    while (!stream->fragment.finished && (chunk_start <= range_end
            || range_end == -1)) {
      download_total_bytes = stream->download_total_bytes;

      ret =
          gst_adaptive_demux_stream_download_uri (demux, stream, url,
          chunk_start, chunk_end, &http_status);

      GST_DEBUG_OBJECT (stream->pad,
          "Fragment chunk download result: %d (%d) %s", stream->last_ret,
          http_status, gst_flow_get_name (stream->last_ret));

      /* Don't retry for any chunks except the first. We would have sent
       * data downstream already otherwise and it's difficult to recover
       * from that in a meaningful way */
      if (chunk_start > range_start)
        retried_once = TRUE;

      /* FIXME: Check for 416 Range Not Satisfiable here and fall back to
       * downloading up to -1. We don't know the full duration.
       * Needs https://bugzilla.gnome.org/show_bug.cgi?id=756806 */
      if (ret != GST_FLOW_OK && chunk_end == -1) {
        break;
      } else if (ret != GST_FLOW_OK) {
        chunk_end = -1;
        stream->last_ret = GST_FLOW_OK;
        continue;
      }

      if (chunk_end == -1)
        break;

      /* Short read, we're at the end now */
      if (stream->download_total_bytes - download_total_bytes <
          chunk_end + 1 - chunk_start)
        break;

      if (!klass->need_another_chunk (stream))
        break;

      /* HTTP ranges are inclusive for the end */
      chunk_start += chunk_size;
      chunk_size = stream->fragment.chunk_size;
      if (chunk_size != -1)
        chunk_end = chunk_start + chunk_size - 1;
      else
        chunk_end = range_end;

      if (range_end != -1)
        chunk_end = MIN (chunk_end, range_end);
    }
  } else {
    ret =
        gst_adaptive_demux_stream_download_uri (demux, stream, url,
        stream->fragment.range_start, stream->fragment.range_end, &http_status);
    GST_DEBUG_OBJECT (stream->pad, "Fragment download result: %d (%d) %s",
        stream->last_ret, http_status, gst_flow_get_name (stream->last_ret));
  }
  if (ret == GST_FLOW_OK)
    goto beach;

  g_mutex_lock (&stream->fragment_download_lock);
  if (G_UNLIKELY (stream->cancelled)) {
    g_mutex_unlock (&stream->fragment_download_lock);
    return ret;
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  /* TODO check if we are truly stopping */
  if (ret != GST_FLOW_CUSTOM_ERROR)
    goto beach;

  last_status_code = stream->last_status_code;
  GST_WARNING_OBJECT (stream->pad, "Got custom error, status %u, dc %d",
      last_status_code, stream->download_error_count);

  live = gst_adaptive_demux_is_live (demux);
  if (!retried_once && ((last_status_code / 100 == 4 && live)
          || last_status_code / 100 == 5)) {
    /* 4xx/5xx */
    /* if current position is before available start, switch to next */
    if (!gst_adaptive_demux_stream_has_next_fragment (demux, stream))
      goto flushing;

    if (live) {
      gint64 range_start, range_stop;

      if (!gst_adaptive_demux_get_live_seek_range (demux, &range_start,
              &range_stop))
        goto flushing;

      if (demux->segment.position < range_start) {
        GST_DEBUG_OBJECT (stream->pad, "Retrying once with next segment");
        stream->last_ret = GST_FLOW_OK;
        ret = gst_adaptive_demux_eos_handling (stream);
        GST_DEBUG_OBJECT (stream->pad, "finish_fragment: %s",
            gst_flow_get_name (ret));
        GST_DEBUG_OBJECT (demux, "Calling update_fragment_info");
        ret = gst_adaptive_demux_stream_update_fragment_info (demux, stream);
        GST_DEBUG_OBJECT (stream->pad, "finish_fragment: %s",
            gst_flow_get_name (ret));
        if (ret == GST_FLOW_OK) {
          retried_once = TRUE;
          goto again;
        }
      } else if (demux->segment.position > range_stop) {
        /* wait a bit to be in range, we don't have any locks at that point */
        gint64 wait_time =
            gst_adaptive_demux_stream_get_fragment_waiting_time (demux, stream);
        if (wait_time > 0) {
          gint64 end_time = g_get_monotonic_time () + wait_time / GST_USECOND;

          GST_DEBUG_OBJECT (stream->pad,
              "Download waiting for %" GST_TIME_FORMAT,
              GST_TIME_ARGS (wait_time));

          GST_MANIFEST_UNLOCK (demux);
          g_mutex_lock (&stream->fragment_download_lock);
          if (G_UNLIKELY (stream->cancelled)) {
            g_mutex_unlock (&stream->fragment_download_lock);
            GST_MANIFEST_LOCK (demux);
            stream->last_ret = GST_FLOW_FLUSHING;
            goto flushing;
          }
          do {
            g_cond_wait_until (&stream->fragment_download_cond,
                &stream->fragment_download_lock, end_time);
            if (G_UNLIKELY (stream->cancelled)) {
              g_mutex_unlock (&stream->fragment_download_lock);
              GST_MANIFEST_LOCK (demux);
              stream->last_ret = GST_FLOW_FLUSHING;
              goto flushing;
            }
          } while (!stream->download_finished);
          g_mutex_unlock (&stream->fragment_download_lock);

          GST_MANIFEST_LOCK (demux);
        }
      }
    }

  flushing:
    if (stream->download_error_count >= MAX_DOWNLOAD_ERROR_COUNT) {
      /* looks like there is no way of knowing when a live stream has ended
       * Have to assume we are falling behind and cause a manifest reload */
      GST_DEBUG_OBJECT (stream->pad, "Converting error of live stream to EOS");
      return GST_FLOW_EOS;
    }
  } else if (!gst_adaptive_demux_stream_has_next_fragment (demux, stream)) {
    /* If this is the last fragment, consider failures EOS and not actual
     * errors. Due to rounding errors in the durations, the last fragment
     * might not actually exist */
    GST_DEBUG_OBJECT (stream->pad, "Converting error for last fragment to EOS");
    return GST_FLOW_EOS;
  } else {
    /* retry once (same segment) for 5xx (server errors) */
    if (!retried_once) {
      retried_once = TRUE;
      /* wait a short time in case the server needs a bit to recover, we don't
       * care if we get woken up before end time. We can use sleep here since
       * we're already blocking and just want to wait some time. */
      g_usleep (100000);        /* a tenth of a second */
      goto again;
    }
  }

beach:
  return ret;

no_url_error:
  {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX,
        (_("Failed to get fragment URL.")),
        ("An error happened when getting fragment URL"));
    gst_task_stop (stream->download_task);
    return GST_FLOW_ERROR;
  }
}

/* this function will take the manifest_lock and will keep it until the end.
 * It will release it temporarily only when going to sleep.
 * Every time it takes the manifest_lock, it will check for cancelled condition
 */
static void
gst_adaptive_demux_stream_download_loop (GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemux *demux = stream->demux;
  GstClockTime next_download = gst_adaptive_demux_get_monotonic_time (demux);
  GstFlowReturn ret;
  gboolean live;

  GST_LOG_OBJECT (stream->pad, "download loop start");

  GST_MANIFEST_LOCK (demux);

  g_mutex_lock (&stream->fragment_download_lock);
  if (G_UNLIKELY (stream->cancelled)) {
    stream->last_ret = GST_FLOW_FLUSHING;
    g_mutex_unlock (&stream->fragment_download_lock);
    goto cancelled;
  }
  g_mutex_unlock (&stream->fragment_download_lock);

  /* Check if we're done with our segment */
  GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
  if (demux->segment.rate > 0) {
    if (GST_CLOCK_TIME_IS_VALID (demux->segment.stop)
        && stream->segment.position >= stream->segment.stop) {
      GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);
      ret = GST_FLOW_EOS;
      gst_task_stop (stream->download_task);
      goto end_of_manifest;
    }
  } else {
    if (GST_CLOCK_TIME_IS_VALID (demux->segment.start)
        && stream->segment.position <= stream->segment.start) {
      GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);
      ret = GST_FLOW_EOS;
      gst_task_stop (stream->download_task);
      goto end_of_manifest;
    }
  }
  GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

  /* Cleanup old streams if any */
  if (G_UNLIKELY (demux->priv->old_streams != NULL)) {
    GList *old_streams = demux->priv->old_streams;
    demux->priv->old_streams = NULL;

    GST_DEBUG_OBJECT (stream->pad, "Cleaning up old streams");
    g_list_free_full (old_streams,
        (GDestroyNotify) gst_adaptive_demux_stream_free);
    GST_DEBUG_OBJECT (stream->pad, "Cleaning up old streams (done)");

    /* gst_adaptive_demux_stream_free had temporarily released the manifest_lock.
     * Recheck the cancelled flag.
     */
    g_mutex_lock (&stream->fragment_download_lock);
    if (G_UNLIKELY (stream->cancelled)) {
      stream->last_ret = GST_FLOW_FLUSHING;
      g_mutex_unlock (&stream->fragment_download_lock);
      goto cancelled;
    }
    g_mutex_unlock (&stream->fragment_download_lock);
  }

  /* Restarting download, figure out new position
   * FIXME : Move this to a separate function ? */
  if (G_UNLIKELY (stream->restart_download)) {
    GstEvent *seg_event;
    GstClockTime cur, ts = 0;
    gint64 pos;

    GST_DEBUG_OBJECT (stream->pad,
        "Activating stream due to reconfigure event");

    if (gst_pad_peer_query_position (stream->pad, GST_FORMAT_TIME, &pos)) {
      ts = (GstClockTime) pos;
      GST_DEBUG_OBJECT (demux, "Downstream position: %"
          GST_TIME_FORMAT, GST_TIME_ARGS (ts));
    } else {
      /* query other pads as some faulty element in the pad's branch might
       * reject position queries. This should be better than using the
       * demux segment position that can be much ahead */
      GList *iter;

      for (iter = demux->streams; iter != NULL; iter = g_list_next (iter)) {
        GstAdaptiveDemuxStream *cur_stream =
            (GstAdaptiveDemuxStream *) iter->data;

        if (gst_pad_peer_query_position (cur_stream->pad, GST_FORMAT_TIME,
                &pos)) {
          ts = (GstClockTime) pos;
          GST_DEBUG_OBJECT (stream->pad, "Downstream position: %"
              GST_TIME_FORMAT, GST_TIME_ARGS (ts));
          break;
        }
      }
    }

    GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
    cur =
        gst_segment_to_stream_time (&stream->segment, GST_FORMAT_TIME,
        stream->segment.position);

    /* we might have already pushed this data */
    ts = MAX (ts, cur);

    GST_DEBUG_OBJECT (stream->pad, "Restarting stream at "
        "position %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));

    if (GST_CLOCK_TIME_IS_VALID (ts)) {
      GstClockTime offset, period_start;

      offset =
          gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
      period_start = gst_adaptive_demux_get_period_start_time (demux);

      /* TODO check return */
      gst_adaptive_demux_stream_seek (demux, stream, demux->segment.rate >= 0,
          0, ts, &ts);

      stream->segment.position = ts - period_start + offset;
    }

    /* The stream's segment is still correct except for
     * the position, so let's send a new one with the
     * updated position */
    seg_event = gst_event_new_segment (&stream->segment);
    gst_event_set_seqnum (seg_event, demux->priv->segment_seqnum);
    GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

    GST_DEBUG_OBJECT (stream->pad, "Sending restart segment: %"
        GST_PTR_FORMAT, seg_event);
    gst_pad_push_event (stream->pad, seg_event);

    stream->discont = TRUE;
    stream->restart_download = FALSE;
  }

  live = gst_adaptive_demux_is_live (demux);

  /* Get information about the fragment to download */
  GST_DEBUG_OBJECT (demux, "Calling update_fragment_info");
  ret = gst_adaptive_demux_stream_update_fragment_info (demux, stream);
  GST_DEBUG_OBJECT (stream->pad, "Fragment info update result: %d %s",
      ret, gst_flow_get_name (ret));
  if (ret == GST_FLOW_OK) {

    /* wait for live fragments to be available */
    if (live) {
      gint64 wait_time =
          gst_adaptive_demux_stream_get_fragment_waiting_time (demux, stream);
      if (wait_time > 0) {
        GstClockTime end_time =
            gst_adaptive_demux_get_monotonic_time (demux) + wait_time;

        GST_DEBUG_OBJECT (stream->pad, "Download waiting for %" GST_TIME_FORMAT,
            GST_TIME_ARGS (wait_time));

        GST_MANIFEST_UNLOCK (demux);

        g_mutex_lock (&stream->fragment_download_lock);
        if (G_UNLIKELY (stream->cancelled)) {
          g_mutex_unlock (&stream->fragment_download_lock);
          GST_MANIFEST_LOCK (demux);
          stream->last_ret = GST_FLOW_FLUSHING;
          goto cancelled;
        }
        gst_adaptive_demux_wait_until (demux->realtime_clock,
            &stream->fragment_download_cond, &stream->fragment_download_lock,
            end_time);
        g_mutex_unlock (&stream->fragment_download_lock);

        GST_DEBUG_OBJECT (stream->pad, "Download finished waiting");

        GST_MANIFEST_LOCK (demux);

        g_mutex_lock (&stream->fragment_download_lock);
        if (G_UNLIKELY (stream->cancelled)) {
          stream->last_ret = GST_FLOW_FLUSHING;
          g_mutex_unlock (&stream->fragment_download_lock);
          goto cancelled;
        }
        g_mutex_unlock (&stream->fragment_download_lock);
      }
    }

    stream->last_ret = GST_FLOW_OK;

    next_download = gst_adaptive_demux_get_monotonic_time (demux);
    ret = gst_adaptive_demux_stream_download_fragment (stream);

    if (ret == GST_FLOW_FLUSHING) {
      g_mutex_lock (&stream->fragment_download_lock);
      if (G_UNLIKELY (stream->cancelled)) {
        stream->last_ret = GST_FLOW_FLUSHING;
        g_mutex_unlock (&stream->fragment_download_lock);
        goto cancelled;
      }
      g_mutex_unlock (&stream->fragment_download_lock);
    }

  } else {
    stream->last_ret = ret;
  }

  switch (ret) {
    case GST_FLOW_OK:
      break;                    /* all is good, let's go */
    case GST_FLOW_EOS:
      GST_DEBUG_OBJECT (stream->pad, "EOS, checking to stop download loop");

      /* we push the EOS after releasing the object lock */
      if (gst_adaptive_demux_is_live (demux)
          && (demux->segment.rate == 1.0
              || gst_adaptive_demux_stream_in_live_seek_range (demux,
                  stream))) {
        GstAdaptiveDemuxClass *demux_class =
            GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

        /* this might be a fragment download error, refresh the manifest, just in case */
        if (!demux_class->requires_periodical_playlist_update (demux)) {
          ret = gst_adaptive_demux_update_manifest (demux);
          break;
          /* Wait only if we can ensure current manifest has been expired.
           * The meaning "we have next period" *WITH* EOS is that, current
           * period has been ended but we can continue to the next period */
        } else if (!gst_adaptive_demux_has_next_period (demux) &&
            gst_adaptive_demux_stream_wait_manifest_update (demux, stream)) {
          goto end;
        }
        gst_task_stop (stream->download_task);
        if (stream->replaced) {
          goto end;
        }
      } else {
        gst_task_stop (stream->download_task);
      }

      if (gst_adaptive_demux_combine_flows (demux) == GST_FLOW_EOS) {
        if (gst_adaptive_demux_has_next_period (demux)) {
          GST_DEBUG_OBJECT (stream->pad,
              "Next period available, not sending EOS");
          gst_adaptive_demux_advance_period (demux);
          ret = GST_FLOW_OK;
        }
      }
      break;

    case GST_FLOW_NOT_LINKED:
    {
      GstFlowReturn ret;
      gst_task_stop (stream->download_task);

      ret = gst_adaptive_demux_combine_flows (demux);
      if (ret == GST_FLOW_NOT_LINKED) {
        GST_ELEMENT_FLOW_ERROR (demux, ret);
      }
    }
      break;

    case GST_FLOW_FLUSHING:{
      GList *iter;

      for (iter = demux->streams; iter; iter = g_list_next (iter)) {
        GstAdaptiveDemuxStream *other;

        other = iter->data;
        gst_task_stop (other->download_task);
      }
    }
      break;

    default:
      if (ret <= GST_FLOW_ERROR) {
        gboolean is_live = gst_adaptive_demux_is_live (demux);
        GST_WARNING_OBJECT (demux, "Error while downloading fragment");
        if (++stream->download_error_count > MAX_DOWNLOAD_ERROR_COUNT) {
          goto download_error;
        }

        g_clear_error (&stream->last_error);

        /* First try to update the playlist for non-live playlists
         * in case the URIs have changed in the meantime. But only
         * try it the first time, after that we're going to wait a
         * a bit to not flood the server */
        if (stream->download_error_count == 1 && !is_live) {
          /* TODO hlsdemux had more options to this function (boolean and err) */

          if (gst_adaptive_demux_update_manifest (demux) == GST_FLOW_OK) {
            /* Retry immediately, the playlist actually has changed */
            GST_DEBUG_OBJECT (demux, "Updated the playlist");
            goto end;
          }
        }

        /* Wait half the fragment duration before retrying */
        next_download += stream->fragment.duration / 2;

        GST_MANIFEST_UNLOCK (demux);

        g_mutex_lock (&stream->fragment_download_lock);
        if (G_UNLIKELY (stream->cancelled)) {
          g_mutex_unlock (&stream->fragment_download_lock);
          GST_MANIFEST_LOCK (demux);
          stream->last_ret = GST_FLOW_FLUSHING;
          goto cancelled;
        }
        gst_adaptive_demux_wait_until (demux->realtime_clock,
            &stream->fragment_download_cond, &stream->fragment_download_lock,
            next_download);
        g_mutex_unlock (&stream->fragment_download_lock);

        GST_DEBUG_OBJECT (demux, "Retrying now");

        GST_MANIFEST_LOCK (demux);

        g_mutex_lock (&stream->fragment_download_lock);
        if (G_UNLIKELY (stream->cancelled)) {
          stream->last_ret = GST_FLOW_FLUSHING;
          g_mutex_unlock (&stream->fragment_download_lock);
          goto cancelled;
        }
        g_mutex_unlock (&stream->fragment_download_lock);

        /* Refetch the playlist now after we waited */
        if (!is_live
            && gst_adaptive_demux_update_manifest (demux) == GST_FLOW_OK) {
          GST_DEBUG_OBJECT (demux, "Updated the playlist");
        }
        goto end;
      }
      break;
  }

end_of_manifest:
  if (G_UNLIKELY (ret == GST_FLOW_EOS)) {
    if (GST_OBJECT_PARENT (stream->pad) != NULL) {
      if (demux->next_streams == NULL && demux->prepared_streams == NULL) {
        GST_DEBUG_OBJECT (stream->src, "Pushing EOS on pad");
        gst_adaptive_demux_stream_push_event (stream, gst_event_new_eos ());
      } else {
        GST_DEBUG_OBJECT (stream->src,
            "Stream is EOS, but we're switching fragments. Not sending.");
      }
    } else {
      GST_ERROR_OBJECT (demux, "Can't push EOS on non-exposed pad");
      goto download_error;
    }
  }

end:
  GST_MANIFEST_UNLOCK (demux);
  GST_LOG_OBJECT (stream->pad, "download loop end");
  return;

cancelled:
  {
    GST_DEBUG_OBJECT (stream->pad, "Stream has been cancelled");
    goto end;
  }
download_error:
  {
    GstMessage *msg;

    if (stream->last_error) {
      gchar *debug = g_strdup_printf ("Error on stream %s:%s",
          GST_DEBUG_PAD_NAME (stream->pad));
      msg =
          gst_message_new_error (GST_OBJECT_CAST (demux), stream->last_error,
          debug);
      GST_ERROR_OBJECT (stream->pad, "Download error: %s",
          stream->last_error->message);
      g_free (debug);
    } else {
      GError *err =
          g_error_new (GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_FOUND,
          _("Couldn't download fragments"));
      msg =
          gst_message_new_error (GST_OBJECT_CAST (demux), err,
          "Fragment downloading has failed consecutive times");
      g_error_free (err);
      GST_ERROR_OBJECT (stream->pad,
          "Download error: Couldn't download fragments, too many failures");
    }

    gst_task_stop (stream->download_task);
    if (stream->src) {
      GstElement *src = stream->src;

      stream->src = NULL;
      GST_MANIFEST_UNLOCK (demux);
      gst_element_set_locked_state (src, TRUE);
      gst_element_set_state (src, GST_STATE_NULL);
      gst_bin_remove (GST_BIN_CAST (demux), src);
      GST_MANIFEST_LOCK (demux);
    }

    gst_element_post_message (GST_ELEMENT_CAST (demux), msg);

    goto end;
  }
}

static void
gst_adaptive_demux_updates_loop (GstAdaptiveDemux * demux)
{
  GstClockTime next_update;
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  /* Loop for updating of the playlist. This periodically checks if
   * the playlist is updated and does so, then signals the streaming
   * thread in case it can continue downloading now. */

  /* block until the next scheduled update or the signal to quit this thread */
  GST_DEBUG_OBJECT (demux, "Started updates task");

  GST_MANIFEST_LOCK (demux);

  next_update =
      gst_adaptive_demux_get_monotonic_time (demux) +
      klass->get_manifest_update_interval (demux) * GST_USECOND;

  /* Updating playlist only needed for live playlists */
  while (gst_adaptive_demux_is_live (demux)) {
    GstFlowReturn ret = GST_FLOW_OK;

    /* Wait here until we should do the next update or we're cancelled */
    GST_DEBUG_OBJECT (demux, "Wait for next playlist update");

    GST_MANIFEST_UNLOCK (demux);

    g_mutex_lock (&demux->priv->updates_timed_lock);
    if (demux->priv->stop_updates_task) {
      g_mutex_unlock (&demux->priv->updates_timed_lock);
      goto quit;
    }
    gst_adaptive_demux_wait_until (demux->realtime_clock,
        &demux->priv->updates_timed_cond,
        &demux->priv->updates_timed_lock, next_update);
    g_mutex_unlock (&demux->priv->updates_timed_lock);

    g_mutex_lock (&demux->priv->updates_timed_lock);
    if (demux->priv->stop_updates_task) {
      g_mutex_unlock (&demux->priv->updates_timed_lock);
      goto quit;
    }
    g_mutex_unlock (&demux->priv->updates_timed_lock);

    GST_MANIFEST_LOCK (demux);

    GST_DEBUG_OBJECT (demux, "Updating playlist");

    ret = gst_adaptive_demux_update_manifest (demux);

    if (ret == GST_FLOW_EOS) {
    } else if (ret != GST_FLOW_OK) {
      /* update_failed_count is used only here, no need to protect it */
      demux->priv->update_failed_count++;
      if (demux->priv->update_failed_count <= DEFAULT_FAILED_COUNT) {
        GST_WARNING_OBJECT (demux, "Could not update the playlist, flow: %s",
            gst_flow_get_name (ret));
        next_update = gst_adaptive_demux_get_monotonic_time (demux)
            + klass->get_manifest_update_interval (demux) * GST_USECOND;
      } else {
        GST_ELEMENT_ERROR (demux, STREAM, FAILED,
            (_("Internal data stream error.")), ("Could not update playlist"));
        GST_DEBUG_OBJECT (demux, "Stopped updates task because of error");
        gst_task_stop (demux->priv->updates_task);
        GST_MANIFEST_UNLOCK (demux);
        goto end;
      }
    } else {
      GST_DEBUG_OBJECT (demux, "Updated playlist successfully");
      demux->priv->update_failed_count = 0;
      next_update =
          gst_adaptive_demux_get_monotonic_time (demux) +
          klass->get_manifest_update_interval (demux) * GST_USECOND;

      /* Wake up download tasks */
      g_mutex_lock (&demux->priv->manifest_update_lock);
      g_cond_broadcast (&demux->priv->manifest_cond);
      g_mutex_unlock (&demux->priv->manifest_update_lock);
    }
  }

  GST_MANIFEST_UNLOCK (demux);

quit:
  {
    GST_DEBUG_OBJECT (demux, "Stop updates task request detected.");
  }

end:
  {
    return;
  }
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_stream_push_event (GstAdaptiveDemuxStream * stream,
    GstEvent * event)
{
  gboolean ret;
  GstPad *pad;
  GstAdaptiveDemux *demux = stream->demux;

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    stream->eos = TRUE;
  }

  pad = gst_object_ref (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream));

  /* Can't push events holding the manifest lock */
  GST_MANIFEST_UNLOCK (demux);

  GST_DEBUG_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (stream),
      "Pushing event %" GST_PTR_FORMAT, event);

  ret = gst_pad_push_event (pad, event);

  gst_object_unref (pad);

  GST_MANIFEST_LOCK (demux);

  return ret;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_is_live (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->is_live)
    return klass->is_live (demux);
  return FALSE;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_stream_seek (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, gboolean forward, GstSeekFlags flags,
    GstClockTime ts, GstClockTime * final_ts)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->stream_seek)
    return klass->stream_seek (stream, forward, flags, ts, final_ts);
  return GST_FLOW_ERROR;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_stream_has_next_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  gboolean ret = TRUE;

  if (klass->stream_has_next_fragment)
    ret = klass->stream_has_next_fragment (stream);

  return ret;
}

/* must be called with manifest_lock taken */
/* Called from:
 *  the ::finish_fragment() handlers when an *actual* fragment is done
 *   */
GstFlowReturn
gst_adaptive_demux_stream_advance_fragment (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstClockTime duration)
{
  GstFlowReturn ret;

  if (stream->last_ret == GST_FLOW_OK) {
    stream->last_ret =
        gst_adaptive_demux_stream_advance_fragment_unlocked (demux, stream,
        duration);
  }
  ret = stream->last_ret;

  return ret;
}

/* must be called with manifest_lock taken */
GstFlowReturn
gst_adaptive_demux_stream_advance_fragment_unlocked (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstClockTime duration)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  GstFlowReturn ret;

  g_return_val_if_fail (klass->stream_advance_fragment != NULL, GST_FLOW_ERROR);

  GST_LOG_OBJECT (stream->pad,
      "timestamp %" GST_TIME_FORMAT " duration:%" GST_TIME_FORMAT,
      GST_TIME_ARGS (stream->fragment.timestamp), GST_TIME_ARGS (duration));

  stream->download_error_count = 0;
  g_clear_error (&stream->last_error);

  /* FIXME - url has no indication of byte ranges for subsegments */
  /* FIXME : All those time statistics are biased, since they are calculated
   * *AFTER* the queue2, which might be blocking. They should ideally be
   * calculated *before* queue2 in the uri_handler_probe */
  gst_element_post_message (GST_ELEMENT_CAST (demux),
      gst_message_new_element (GST_OBJECT_CAST (demux),
          gst_structure_new (GST_ADAPTIVE_DEMUX_STATISTICS_MESSAGE_NAME,
              "manifest-uri", G_TYPE_STRING,
              demux->manifest_uri, "uri", G_TYPE_STRING,
              stream->fragment.uri, "fragment-start-time",
              GST_TYPE_CLOCK_TIME, stream->download_start_time,
              "fragment-stop-time", GST_TYPE_CLOCK_TIME,
              gst_util_get_timestamp (), "fragment-size", G_TYPE_UINT64,
              stream->download_total_bytes, "fragment-download-time",
              GST_TYPE_CLOCK_TIME, stream->last_download_time, NULL)));

  /* Don't update to the end of the segment if in reverse playback */
  GST_ADAPTIVE_DEMUX_SEGMENT_LOCK (demux);
  if (GST_CLOCK_TIME_IS_VALID (duration) && demux->segment.rate > 0) {
    GstClockTime offset =
        gst_adaptive_demux_stream_get_presentation_offset (demux, stream);
    GstClockTime period_start =
        gst_adaptive_demux_get_period_start_time (demux);

    stream->segment.position += duration;

    /* Convert from position inside the stream's segment to the demuxer's
     * segment, they are not necessarily the same */
    if (stream->segment.position - offset + period_start >
        demux->segment.position)
      demux->segment.position =
          stream->segment.position - offset + period_start;
  }
  GST_ADAPTIVE_DEMUX_SEGMENT_UNLOCK (demux);

  /* When advancing with a non 1.0 rate on live streams, we need to check
   * the live seeking range again to make sure we can still advance to
   * that position */
  if (demux->segment.rate != 1.0 && gst_adaptive_demux_is_live (demux)) {
    if (!gst_adaptive_demux_stream_in_live_seek_range (demux, stream))
      ret = GST_FLOW_EOS;
    else
      ret = klass->stream_advance_fragment (stream);
  } else if (gst_adaptive_demux_is_live (demux)
      || gst_adaptive_demux_stream_has_next_fragment (demux, stream)) {
    ret = klass->stream_advance_fragment (stream);
  } else {
    ret = GST_FLOW_EOS;
  }

  stream->download_start_time =
      GST_TIME_AS_USECONDS (gst_adaptive_demux_get_monotonic_time (demux));

  if (ret == GST_FLOW_OK) {
    if (gst_adaptive_demux_stream_select_bitrate (demux, stream,
            gst_adaptive_demux_stream_update_current_bitrate (demux, stream))) {
      stream->need_header = TRUE;
      ret = (GstFlowReturn) GST_ADAPTIVE_DEMUX_FLOW_SWITCH;
    }

    /* the subclass might want to switch pads */
    if (G_UNLIKELY (demux->next_streams)) {
      GList *iter;
      gboolean can_expose = TRUE;

      gst_task_stop (stream->download_task);

      ret = GST_FLOW_EOS;

      for (iter = demux->streams; iter; iter = g_list_next (iter)) {
        /* Only expose if all streams are now cancelled or finished downloading */
        GstAdaptiveDemuxStream *other = iter->data;
        if (other != stream) {
          g_mutex_lock (&other->fragment_download_lock);
          can_expose &= (other->cancelled == TRUE
              || other->download_finished == TRUE);
          g_mutex_unlock (&other->fragment_download_lock);
        }
      }

      if (can_expose) {
        GST_DEBUG_OBJECT (demux, "Subclass wants new pads "
            "to do bitrate switching");
        gst_adaptive_demux_prepare_streams (demux, FALSE);
        gst_adaptive_demux_start_tasks (demux, TRUE);
      } else {
        GST_LOG_OBJECT (demux, "Not switching yet - ongoing downloads");
      }
    }
  }

  return ret;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_stream_select_bitrate (GstAdaptiveDemux *
    demux, GstAdaptiveDemuxStream * stream, guint64 bitrate)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->stream_select_bitrate)
    return klass->stream_select_bitrate (stream, bitrate);
  return FALSE;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_stream_update_fragment_info (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  GstFlowReturn ret;

  g_return_val_if_fail (klass->stream_update_fragment_info != NULL,
      GST_FLOW_ERROR);

  /* Make sure the sub-class will update bitrate, or else
   * we will later */
  stream->fragment.bitrate = 0;
  stream->fragment.finished = FALSE;

  GST_LOG_OBJECT (stream->pad, "position %" GST_TIME_FORMAT,
      GST_TIME_ARGS (stream->segment.position));

  ret = klass->stream_update_fragment_info (stream);

  GST_LOG_OBJECT (stream->pad, "ret:%s uri:%s", gst_flow_get_name (ret),
      stream->fragment.uri);
  if (ret == GST_FLOW_OK) {
    GST_LOG_OBJECT (stream->pad,
        "timestamp %" GST_TIME_FORMAT " duration:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (stream->fragment.timestamp),
        GST_TIME_ARGS (stream->fragment.duration));
    GST_LOG_OBJECT (stream->pad,
        "range start:%" G_GINT64_FORMAT " end:%" G_GINT64_FORMAT,
        stream->fragment.range_start, stream->fragment.range_end);
  }

  return ret;
}

/* must be called with manifest_lock taken */
static gint64
gst_adaptive_demux_stream_get_fragment_waiting_time (GstAdaptiveDemux *
    demux, GstAdaptiveDemuxStream * stream)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  if (klass->stream_get_fragment_waiting_time)
    return klass->stream_get_fragment_waiting_time (stream);
  return 0;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_update_manifest_default (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  GstFragment *download;
  GstBuffer *buffer;
  GstFlowReturn ret;
  GError *error = NULL;

  download = gst_uri_downloader_fetch_uri (demux->downloader,
      demux->manifest_uri, NULL, TRUE, TRUE, TRUE, &error);
  if (download) {
    g_free (demux->manifest_uri);
    g_free (demux->manifest_base_uri);
    if (download->redirect_permanent && download->redirect_uri) {
      demux->manifest_uri = g_strdup (download->redirect_uri);
      demux->manifest_base_uri = NULL;
    } else {
      demux->manifest_uri = g_strdup (download->uri);
      demux->manifest_base_uri = g_strdup (download->redirect_uri);
    }

    buffer = gst_fragment_get_buffer (download);
    g_object_unref (download);
    ret = klass->update_manifest_data (demux, buffer);
    gst_buffer_unref (buffer);
    /* FIXME: Should the manifest uri vars be reverted to original
     * values if updating fails? */
  } else {
    GST_WARNING_OBJECT (demux, "Failed to download manifest: %s",
        error->message);
    ret = GST_FLOW_NOT_LINKED;
  }
  g_clear_error (&error);

  return ret;
}

/* must be called with manifest_lock taken */
static GstFlowReturn
gst_adaptive_demux_update_manifest (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  GstFlowReturn ret;

  ret = klass->update_manifest (demux);

  if (ret == GST_FLOW_OK) {
    GstClockTime duration;
    /* Send an updated duration message */
    duration = klass->get_duration (demux);
    if (duration != GST_CLOCK_TIME_NONE) {
      GST_DEBUG_OBJECT (demux,
          "Sending duration message : %" GST_TIME_FORMAT,
          GST_TIME_ARGS (duration));
      gst_element_post_message (GST_ELEMENT (demux),
          gst_message_new_duration_changed (GST_OBJECT (demux)));
    } else {
      GST_DEBUG_OBJECT (demux,
          "Duration unknown, can not send the duration message");
    }

    /* If a manifest changes it's liveness or periodic updateness, we need
     * to start/stop the manifest update task appropriately */
    /* Keep this condition in sync with the one in
     * gst_adaptive_demux_start_manifest_update_task()
     */
    if (gst_adaptive_demux_is_live (demux) &&
        klass->requires_periodical_playlist_update (demux)) {
      gst_adaptive_demux_start_manifest_update_task (demux);
    } else {
      gst_adaptive_demux_stop_manifest_update_task (demux);
    }
  }

  return ret;
}

void
gst_adaptive_demux_stream_fragment_clear (GstAdaptiveDemuxStreamFragment * f)
{
  g_free (f->uri);
  f->uri = NULL;
  f->range_start = 0;
  f->range_end = -1;

  g_free (f->header_uri);
  f->header_uri = NULL;
  f->header_range_start = 0;
  f->header_range_end = -1;

  g_free (f->index_uri);
  f->index_uri = NULL;
  f->index_range_start = 0;
  f->index_range_end = -1;

  f->finished = FALSE;
}

/* must be called with manifest_lock taken */
static gboolean
gst_adaptive_demux_has_next_period (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);
  gboolean ret = FALSE;

  if (klass->has_next_period)
    ret = klass->has_next_period (demux);
  GST_DEBUG_OBJECT (demux, "Has next period: %d", ret);
  return ret;
}

/* must be called with manifest_lock taken */
static void
gst_adaptive_demux_advance_period (GstAdaptiveDemux * demux)
{
  GstAdaptiveDemuxClass *klass = GST_ADAPTIVE_DEMUX_GET_CLASS (demux);

  g_return_if_fail (klass->advance_period != NULL);

  GST_DEBUG_OBJECT (demux, "Advancing to next period");
  klass->advance_period (demux);
  gst_adaptive_demux_prepare_streams (demux, FALSE);
  gst_adaptive_demux_start_tasks (demux, TRUE);
}

/**
 * gst_adaptive_demux_get_monotonic_time:
 * Returns: a monotonically increasing time, using the system realtime clock
 */
GstClockTime
gst_adaptive_demux_get_monotonic_time (GstAdaptiveDemux * demux)
{
  g_return_val_if_fail (demux != NULL, GST_CLOCK_TIME_NONE);
  return gst_clock_get_time (demux->realtime_clock);
}

/**
 * gst_adaptive_demux_get_client_now_utc:
 * @demux: #GstAdaptiveDemux
 * Returns: the client's estimate of UTC
 *
 * Used to find the client's estimate of UTC, using the system realtime clock.
 */
GDateTime *
gst_adaptive_demux_get_client_now_utc (GstAdaptiveDemux * demux)
{
  GstClockTime rtc_now;
  GDateTime *unix_datetime;
  GDateTime *result_datetime;
  gint64 utc_now_in_us;

  rtc_now = gst_clock_get_time (demux->realtime_clock);
  utc_now_in_us = demux->clock_offset + GST_TIME_AS_USECONDS (rtc_now);
  unix_datetime =
      g_date_time_new_from_unix_utc (utc_now_in_us / G_TIME_SPAN_SECOND);
  result_datetime =
      g_date_time_add (unix_datetime, utc_now_in_us % G_TIME_SPAN_SECOND);
  g_date_time_unref (unix_datetime);
  return result_datetime;
}

/**
 * gst_adaptive_demux_is_running
 * @demux: #GstAdaptiveDemux
 * Returns: whether the demuxer is processing data
 *
 * Returns FALSE if shutdown has started (transitioning down from
 * PAUSED), otherwise TRUE.
 */
gboolean
gst_adaptive_demux_is_running (GstAdaptiveDemux * demux)
{
  return g_atomic_int_get (&demux->running);
}

static GstAdaptiveDemuxTimer *
gst_adaptive_demux_timer_new (GCond * cond, GMutex * mutex)
{
  GstAdaptiveDemuxTimer *timer;

  timer = g_slice_new (GstAdaptiveDemuxTimer);
  timer->fired = FALSE;
  timer->cond = cond;
  timer->mutex = mutex;
  g_atomic_int_set (&timer->ref_count, 1);
  return timer;
}

static GstAdaptiveDemuxTimer *
gst_adaptive_demux_timer_ref (GstAdaptiveDemuxTimer * timer)
{
  g_return_val_if_fail (timer != NULL, NULL);
  g_atomic_int_inc (&timer->ref_count);
  return timer;
}

static void
gst_adaptive_demux_timer_unref (GstAdaptiveDemuxTimer * timer)
{
  g_return_if_fail (timer != NULL);
  if (g_atomic_int_dec_and_test (&timer->ref_count)) {
    g_slice_free (GstAdaptiveDemuxTimer, timer);
  }
}

/* gst_adaptive_demux_wait_until:
 * A replacement for g_cond_wait_until that uses the clock rather
 * than system time to control the duration of the sleep. Typically
 * clock is actually a #GstSystemClock, in which case this function
 * behaves exactly like g_cond_wait_until. Inside unit tests,
 * the clock is typically a #GstTestClock, which allows tests to run
 * in non-realtime.
 * This function must be called with mutex held.
 */
static gboolean
gst_adaptive_demux_wait_until (GstClock * clock, GCond * cond, GMutex * mutex,
    GstClockTime end_time)
{
  GstAdaptiveDemuxTimer *timer;
  gboolean fired;
  GstClockReturn res;

  if (G_UNLIKELY (!GST_CLOCK_TIME_IS_VALID (end_time))) {
    /* for an invalid time, gst_clock_id_wait_async will try to call
     * gst_adaptive_demux_clock_callback from the current thread.
     * It still holds the mutex while doing that, so it will deadlock.
     * g_cond_wait_until would return immediately with false, so we'll do the same.
     */
    return FALSE;
  }
  timer = gst_adaptive_demux_timer_new (cond, mutex);
  timer->clock_id = gst_clock_new_single_shot_id (clock, end_time);
  res =
      gst_clock_id_wait_async (timer->clock_id,
      gst_adaptive_demux_clock_callback, gst_adaptive_demux_timer_ref (timer),
      (GDestroyNotify) gst_adaptive_demux_timer_unref);
  /* clock does not support asynchronously wait. Assert and return */
  if (res == GST_CLOCK_UNSUPPORTED) {
    gst_clock_id_unref (timer->clock_id);
    gst_adaptive_demux_timer_unref (timer);
    g_return_val_if_reached (TRUE);
  }
  g_assert (!timer->fired);
  /* the gst_adaptive_demux_clock_callback() will signal the
   * cond when the clock's single shot timer fires, or the cond will be
   * signalled by another thread that wants to cause this wait to finish
   * early (e.g. to terminate the waiting thread).
   * There is no need for a while loop here, because that logic is
   * implemented by the function calling gst_adaptive_demux_wait_until() */
  g_cond_wait (cond, mutex);
  fired = timer->fired;
  if (!fired)
    gst_clock_id_unschedule (timer->clock_id);
  gst_clock_id_unref (timer->clock_id);
  gst_adaptive_demux_timer_unref (timer);
  return !fired;
}

static gboolean
gst_adaptive_demux_clock_callback (GstClock * clock,
    GstClockTime time, GstClockID id, gpointer user_data)
{
  GstAdaptiveDemuxTimer *timer = (GstAdaptiveDemuxTimer *) user_data;
  g_return_val_if_fail (timer != NULL, FALSE);
  g_mutex_lock (timer->mutex);
  timer->fired = TRUE;
  g_cond_signal (timer->cond);
  g_mutex_unlock (timer->mutex);
  return TRUE;
}

/**
 * gst_adaptive_demux_get_qos_earliest_time:
 *
 * Returns: The QOS earliest time
 *
 * Since: 1.20
 */
GstClockTime
gst_adaptive_demux_get_qos_earliest_time (GstAdaptiveDemux * demux)
{
  GstClockTime earliest;

  GST_OBJECT_LOCK (demux);
  earliest = demux->priv->qos_earliest_time;
  GST_OBJECT_UNLOCK (demux);

  return earliest;
}
