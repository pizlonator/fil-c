/*
 * DASH demux plugin for GStreamer
 *
 * gstdashdemux.c
 *
 * Copyright (C) 2012 Orange
 *
 * Authors:
 *   David Corvoysier <david.corvoysier@orange.com>
 *   Hamid Zakari <hamid.zakari@gmail.com>
 *
 * Copyright (C) 2013 Smart TV Alliance
 *  Author: Thiago Sousa Santos <thiago.sousa.santos@collabora.com>, Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:element-dashdemux
 * @title: dashdemux
 *
 * DASH demuxer element.
 * ## Example launch line
 * |[
 * gst-launch-1.0 playbin uri="http://www-itec.uni-klu.ac.at/ftp/datasets/mmsys12/RedBullPlayStreets/redbull_4s/RedBullPlayStreets_4s_isoffmain_DIS_23009_1_v_2_1c2_2011_08_30.mpd"
 * ]|
 */

/* Implementation notes:
 *
 * The following section describes how dashdemux works internally.
 *
 * Introduction:
 *
 * dashdemux is a "fake" demux, as unlike traditional demux elements, it
 * doesn't split data streams contained in an envelope to expose them to
 * downstream decoding elements.
 *
 * Instead, it parses an XML file called a manifest to identify a set of
 * individual stream fragments it needs to fetch and expose to the actual
 * demux elements that will handle them (this behavior is sometimes
 * referred as the "demux after a demux" scenario).
 *
 * For a given section of content, several representations corresponding
 * to different bitrates may be available: dashdemux will select the most
 * appropriate representation based on local conditions (typically the
 * available bandwidth and the amount of buffering available, capped by
 * a maximum allowed bitrate).
 *
 * The representation selection algorithm can be configured using
 * specific properties: max bitrate, min/max buffering, bandwidth ratio.
 *
 *
 * General Design:
 *
 * dashdemux has a single sink pad that accepts the data corresponding
 * to the manifest, typically fetched from an HTTP or file source.
 *
 * dashdemux exposes the streams it recreates based on the fragments it
 * fetches through dedicated src pads corresponding to the caps of the
 * fragments container (ISOBMFF/MP4 or MPEG2TS).
 *
 * During playback, new representations will typically be exposed as a
 * new set of pads (see 'Switching between representations' below).
 *
 * Fragments downloading is performed using a dedicated task that fills
 * an internal queue. Another task is in charge of popping fragments
 * from the queue and pushing them downstream.
 *
 * Switching between representations:
 *
 * Decodebin supports scenarios allowing to seamlessly switch from one
 * stream to another inside the same "decoding chain".
 *
 * To achieve that, it combines the elements it autoplugged in chains
 *  and groups, allowing only one decoding group to be active at a given
 * time for a given chain.
 *
 * A chain can signal decodebin that it is complete by sending a
 * no-more-pads event, but even after that new pads can be added to
 * create new subgroups, providing that a new no-more-pads event is sent.
 *
 * We take advantage of that to dynamically create a new decoding group
 * in order to select a different representation during playback.
 *
 * Typically, assuming that each fragment contains both audio and video,
 * the following tree would be created:
 *
 * chain "DASH Demux"
 * |_ group "Representation set 1"
 * |   |_ chain "Qt Demux 0"
 * |       |_ group "Stream 0"
 * |           |_ chain "H264"
 * |           |_ chain "AAC"
 * |_ group "Representation set 2"
 *     |_ chain "Qt Demux 1"
 *         |_ group "Stream 1"
 *             |_ chain "H264"
 *             |_ chain "AAC"
 *
 * Or, if audio and video are contained in separate fragments:
 *
 * chain "DASH Demux"
 * |_ group "Representation set 1"
 * |   |_ chain "Qt Demux 0"
 * |   |   |_ group "Stream 0"
 * |   |       |_ chain "H264"
 * |   |_ chain "Qt Demux 1"
 * |       |_ group "Stream 1"
 * |           |_ chain "AAC"
 * |_ group "Representation set 2"
 *     |_ chain "Qt Demux 3"
 *     |   |_ group "Stream 2"
 *     |       |_ chain "H264"
 *     |_ chain "Qt Demux 4"
 *         |_ group "Stream 3"
 *             |_ chain "AAC"
 *
 * In both cases, when switching from Set 1 to Set 2 an EOS is sent on
 * each end pad corresponding to Rep 0, triggering the "drain" state to
 * propagate upstream.
 * Once both EOS have been processed, the "Set 1" group is completely
 * drained, and decodebin2 will switch to the "Set 2" group.
 *
 * Note: nothing can be pushed to the new decoding group before the
 * old one has been drained, which means that in order to be able to
 * adapt quickly to bandwidth changes, we will not be able to rely
 * on downstream buffering, and will instead manage an internal queue.
 *
 *
 * Keyframe trick-mode implementation:
 *
 * When requested (with GST_SEEK_FLAG_TRICKMODE_KEY_UNIT) and if the format
 * is supported (ISOBMFF profiles), dashdemux can download only keyframes
 * in order to provide fast forward/reverse playback without exceeding the
 * available bandwidth/cpu/memory usage.
 *
 * This is done in two parts:
 * 1) Parsing ISOBMFF atoms to detect the location of keyframes and only
 *    download/push those.
 * 2) Deciding what the ideal next keyframe to download is in order to
 *    provide as many keyframes as possible without rebuffering.
 *
 * * Keyframe-only downloads:
 *
 * For each beginning of fragment, the fragment header will be parsed in
 * gst_dash_demux_parse_isobmff() and then the information (offset, pts...)
 * of each keyframe will be stored in moof_sync_samples.
 *
 * gst_dash_demux_stream_update_fragment_info() will specify the range
 * start and end of the current keyframe, which will cause GstAdaptiveDemux
 * to do a new upstream range request.
 *
 * When advancing, if there are still some keyframes in the current
 * fragment, gst_dash_demux_stream_advance_fragment() will call
 * gst_dash_demux_stream_advance_sync_sample() which decides what the next
 * keyframe to get will be (it can be in reverse order for example, or
 * might not be the *next* keyframe but one further as explained below).
 *
 * If no more keyframes are available in the current fragment, dash will
 * advance to the next fragment (just like in the normal case) or to a
 * fragment much further away (as explained below).
 *
 *
 * * Deciding the optimal "next" keyframe/fragment to download:
 *
 * The main reason for doing keyframe-only downloads is for trick-modes
 * (i.e. being able to do fast reverse/forward playback with limited
 * bandwidth/cpu/memory).
 *
 * Downloading all keyframes might not be the optimal solution, especially
 * at high playback rates, since the time taken to download the keyframe
 * might exceed the available running time between two displayed frames
 * (i.e. all frames would end up arriving late). This would cause severe
 * rebuffering.
 *
 * Note: The values specified below can be in either the segment running
 * time or in absolute values. Where position values need to be converted
 * to segment running time the "running_time(val)" notation is used, and
 * where running time need ot be converted to segment poisition the
 * "position(val)" notation is used.
 *
 * The goal instead is to be able to download/display as many frames as
 * possible for a given playback rate. For that the implementation will
 * take into account:
 *  * The requested playback rate and segment
 *  * The average time to request and download a keyframe (in running time)
 *  * The current position of dashdemux in the stream
 *  * The current downstream (i.e. sink) position (in running time)
 *
 * To reach this goal we consider that there is some amount of buffering
 * (in time) between dashdemux and the display sink. While we do not know
 * the exact amount of buffering available, a safe and reasonable assertion
 * is that there is at least a second (in running time).
 *
 * The average time to request and fully download a keyframe (with or
 * without fragment header) is obtained by averaging the
 * GstAdaptiveDemuxStream->last_download_time and is stored in
 * GstDashDemuxStream->average_download_time. Those values include the
 * network latency and full download time, which are more interesting and
 * correct than just bitrates (with small download sizes, the impact of the
 * network latency is much higher).
 *
 * The current position is calculated based on the fragment timestamp and
 * the current keyframe index within that fragment. It is stored in
 * GstDashDemuxStream->actual_position.
 *
 * The downstream position of the pipeline is obtained via QoS events and
 * is stored in GstAdaptiveDemux (note: it's a running time value).
 *
 * The estimated buffering level between dashdemux and downstream is
 * therefore:
 *   buffering_level = running_time(actual_position) - qos_earliest_time
 *
 * In order to avoid rebuffering, we want to ensure that the next keyframe
 * (including potential fragment header) we request will be download, demuxed
 * and decoded in time so that it is not late. That next keyframe time is
 * called the "target_time" and is calculated whenever we have finished
 * pushing a keyframe downstream.
 *
 * One simple observation at this point is that we *need* to make sure that
 * the target time is chosen such that:
 *   running_time(target_time) > qos_earliest_time + average_download_time
 *
 * i.e. we chose a target time which will be greater than the time at which
 * downstream will be once we request and download the keyframe (otherwise
 * we're guaranteed to be late).
 *
 * This would provide the highest number of displayed frames per
 * second, but it is just a *minimal* value and is not enough as-is,
 * since it doesn't take into account the following items which could
 * cause frames to arrive late (and therefore rebuffering):
 * * Network jitter (i.e. by how much the download time can fluctuate)
 * * Network stalling
 * * Different keyframe sizes (and therefore download time)
 * * Decoding speed
 *
 * Instead, we adjust the target time calculation based on the
 * buffering_level.
 *
 * The smaller the buffering level is (i.e. the closer we are between
 * current and downstream), the more aggressively we skip forward (and
 * guarantee the keyframe will be downloaded, decoded and displayed in
 * time). And the higher the buffering level, the least aggresivelly
 * we need to skip forward (and therefore display more frames per
 * second).
 *
 * Right now the threshold for aggressive switching is set to 3
 * average_download_time. Below that buffering level we set the target time
 * to at least 3 average_download_time distance beyond the
 * qos_earliest_time.
 *
 * If we are above that buffering level we set the target time to:
 *      position(running_time(position) + average_download_time)
 *
 * The logic is therefore:
 * WHILE(!EOS)
 *   Calculate target_time
 *   Advance to keyframe/fragment for that target_time
 *   Adaptivedemux downloads that keyframe/fragment
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <gio/gio.h>
#include <gst/base/gsttypefindhelper.h>
#include <gst/tag/tag.h>
#include <gst/net/gstnet.h>
#include <glib/gi18n-lib.h>
#include "gstdashdemux.h"
#include "gstdash_debug.h"

static GstStaticPadTemplate gst_dash_demux_videosrc_template =
GST_STATIC_PAD_TEMPLATE ("video_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_dash_demux_audiosrc_template =
GST_STATIC_PAD_TEMPLATE ("audio_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate gst_dash_demux_subtitlesrc_template =
GST_STATIC_PAD_TEMPLATE ("subtitle_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/dash+xml"));

GST_DEBUG_CATEGORY (gst_dash_demux_debug);
#define GST_CAT_DEFAULT gst_dash_demux_debug

enum
{
  PROP_0,

  PROP_MAX_BUFFERING_TIME,
  PROP_BANDWIDTH_USAGE,
  PROP_MAX_BITRATE,
  PROP_MAX_VIDEO_WIDTH,
  PROP_MAX_VIDEO_HEIGHT,
  PROP_MAX_VIDEO_FRAMERATE,
  PROP_PRESENTATION_DELAY,
  PROP_LAST
};

/* Default values for properties */
#define DEFAULT_MAX_BUFFERING_TIME       30     /* in seconds */
#define DEFAULT_BANDWIDTH_USAGE         0.8f    /* 0 to 1     */
#define DEFAULT_MAX_BITRATE               0     /* in bit/s  */
#define DEFAULT_MAX_VIDEO_WIDTH           0
#define DEFAULT_MAX_VIDEO_HEIGHT          0
#define DEFAULT_MAX_VIDEO_FRAMERATE_N     0
#define DEFAULT_MAX_VIDEO_FRAMERATE_D     1
#define DEFAULT_PRESENTATION_DELAY     "10s"    /* 10s */

/* Clock drift compensation for live streams */
#define SLOW_CLOCK_UPDATE_INTERVAL  (1000000 * 30 * 60) /* 30 minutes */
#define FAST_CLOCK_UPDATE_INTERVAL  (1000000 * 30)      /* 30 seconds */
#define SUPPORTED_CLOCK_FORMATS (GST_MPD_UTCTIMING_TYPE_NTP | GST_MPD_UTCTIMING_TYPE_HTTP_HEAD | GST_MPD_UTCTIMING_TYPE_HTTP_XSDATE | GST_MPD_UTCTIMING_TYPE_HTTP_ISO | GST_MPD_UTCTIMING_TYPE_HTTP_NTP)
#define NTP_TO_UNIX_EPOCH G_GUINT64_CONSTANT(2208988800)        /* difference (in seconds) between NTP epoch and Unix epoch */

struct _GstDashDemuxClockDrift
{
  GMutex clock_lock;            /* used to protect access to struct */
  guint selected_url;
  gint64 next_update;
  /* @clock_compensation: amount (in usecs) to add to client's idea of
     now to map it to the server's idea of now */
  GTimeSpan clock_compensation;
  GstClock *ntp_clock;
};

typedef struct
{
  guint64 start_offset, end_offset;
  /* TODO: Timestamp and duration */
} GstDashStreamSyncSample;

/* GObject */
static void gst_dash_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dash_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dash_demux_dispose (GObject * obj);

/* GstAdaptiveDemux */
static GstClockTime gst_dash_demux_get_duration (GstAdaptiveDemux * ademux);
static gboolean gst_dash_demux_is_live (GstAdaptiveDemux * ademux);
static void gst_dash_demux_reset (GstAdaptiveDemux * ademux);
static gboolean gst_dash_demux_process_manifest (GstAdaptiveDemux * ademux,
    GstBuffer * buf);
static gboolean gst_dash_demux_seek (GstAdaptiveDemux * demux, GstEvent * seek);
static GstFlowReturn
gst_dash_demux_stream_update_fragment_info (GstAdaptiveDemuxStream * stream);
static GstFlowReturn gst_dash_demux_stream_seek (GstAdaptiveDemuxStream *
    stream, gboolean forward, GstSeekFlags flags, GstClockTime ts,
    GstClockTime * final_ts);
static gboolean gst_dash_demux_stream_has_next_fragment (GstAdaptiveDemuxStream
    * stream);
static GstFlowReturn
gst_dash_demux_stream_advance_fragment (GstAdaptiveDemuxStream * stream);
static gboolean
gst_dash_demux_stream_advance_subfragment (GstAdaptiveDemuxStream * stream);
static gboolean gst_dash_demux_stream_select_bitrate (GstAdaptiveDemuxStream *
    stream, guint64 bitrate);
static gint64 gst_dash_demux_get_manifest_update_interval (GstAdaptiveDemux *
    demux);
static GstFlowReturn gst_dash_demux_update_manifest_data (GstAdaptiveDemux *
    demux, GstBuffer * buf);
static gint64
gst_dash_demux_stream_get_fragment_waiting_time (GstAdaptiveDemuxStream *
    stream);
static void gst_dash_demux_advance_period (GstAdaptiveDemux * demux);
static gboolean gst_dash_demux_has_next_period (GstAdaptiveDemux * demux);
static GstFlowReturn gst_dash_demux_data_received (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer);
static gboolean
gst_dash_demux_stream_fragment_start (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static GstFlowReturn
gst_dash_demux_stream_fragment_finished (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream);
static gboolean gst_dash_demux_need_another_chunk (GstAdaptiveDemuxStream *
    stream);

/* GstDashDemux */
static gboolean gst_dash_demux_setup_all_streams (GstDashDemux * demux);
static void gst_dash_demux_stream_free (GstAdaptiveDemuxStream * stream);

static GstCaps *gst_dash_demux_get_input_caps (GstDashDemux * demux,
    GstActiveStream * stream);
static GstPad *gst_dash_demux_create_pad (GstDashDemux * demux,
    GstActiveStream * stream);
static GstDashDemuxClockDrift *gst_dash_demux_clock_drift_new (GstDashDemux *
    demux);
static void gst_dash_demux_clock_drift_free (GstDashDemuxClockDrift *);
static gboolean gst_dash_demux_poll_clock_drift (GstDashDemux * demux);
static GTimeSpan gst_dash_demux_get_clock_compensation (GstDashDemux * demux);
static GDateTime *gst_dash_demux_get_server_now_utc (GstDashDemux * demux);

#define SIDX(s) (&(s)->sidx_parser.sidx)

static inline GstSidxBoxEntry *
SIDX_ENTRY (GstDashDemuxStream * s, gint i)
{
  g_assert (i < SIDX (s)->entries_count);
  return &(SIDX (s)->entries[(i)]);
}

#define SIDX_CURRENT_ENTRY(s) SIDX_ENTRY(s, SIDX(s)->entry_index)

static void gst_dash_demux_send_content_protection_event (gpointer cp_data,
    gpointer stream);

#define gst_dash_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstDashDemux, gst_dash_demux, GST_TYPE_ADAPTIVE_DEMUX,
    GST_DEBUG_CATEGORY_INIT (gst_dash_demux_debug, "dashdemux", 0,
        "dashdemux element")
    );
GST_ELEMENT_REGISTER_DEFINE (dashdemux, "dashdemux", GST_RANK_PRIMARY,
    GST_TYPE_DASH_DEMUX);

static void
gst_dash_demux_dispose (GObject * obj)
{
  GstDashDemux *demux = GST_DASH_DEMUX (obj);

  gst_dash_demux_reset (GST_ADAPTIVE_DEMUX_CAST (demux));

  if (demux->client) {
    gst_mpd_client_free (demux->client);
    demux->client = NULL;
  }

  g_mutex_clear (&demux->client_lock);

  gst_dash_demux_clock_drift_free (demux->clock_drift);
  demux->clock_drift = NULL;
  g_free (demux->default_presentation_delay);
  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static gboolean
gst_dash_demux_get_live_seek_range (GstAdaptiveDemux * demux, gint64 * start,
    gint64 * stop)
{
  GstDashDemux *self = GST_DASH_DEMUX (demux);
  GDateTime *now;
  GDateTime *mstart;
  GTimeSpan stream_now;
  GstClockTime seg_duration;

  if (self->client->mpd_root_node->availabilityStartTime == NULL)
    return FALSE;

  seg_duration = gst_mpd_client_get_maximum_segment_duration (self->client);
  now = gst_dash_demux_get_server_now_utc (self);
  mstart =
      gst_date_time_to_g_date_time (self->client->mpd_root_node->
      availabilityStartTime);
  stream_now = g_date_time_difference (now, mstart);
  g_date_time_unref (now);
  g_date_time_unref (mstart);

  if (stream_now <= 0)
    return FALSE;

  *stop = stream_now * GST_USECOND;
  if (self->client->mpd_root_node->timeShiftBufferDepth ==
      GST_MPD_DURATION_NONE) {
    *start = 0;
  } else {
    *start =
        *stop -
        (self->client->mpd_root_node->timeShiftBufferDepth * GST_MSECOND);
    if (*start < 0)
      *start = 0;
  }

  /* As defined in 5.3.9.5.3 of the DASH specification, a segment does
     not become available until the sum of:
     * the value of the MPD@availabilityStartTime,
     * the PeriodStart time of the containing Period
     * the MPD start time of the Media Segment, and
     * the MPD duration of the Media Segment.
     Therefore we need to subtract the media segment duration from the stop
     time.
   */
  *stop -= seg_duration;
  return TRUE;
}

static GstClockTime
gst_dash_demux_get_presentation_offset (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);

  return gst_mpd_client_get_stream_presentation_offset (dashdemux->client,
      dashstream->index);
}

static GstClockTime
gst_dash_demux_get_period_start_time (GstAdaptiveDemux * demux)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);

  return gst_mpd_client_get_period_start_time (dashdemux->client);
}

static void
gst_dash_demux_class_init (GstDashDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAdaptiveDemuxClass *gstadaptivedemux_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstadaptivedemux_class = (GstAdaptiveDemuxClass *) klass;

  gobject_class->set_property = gst_dash_demux_set_property;
  gobject_class->get_property = gst_dash_demux_get_property;
  gobject_class->dispose = gst_dash_demux_dispose;

#ifndef GST_REMOVE_DEPRECATED
  g_object_class_install_property (gobject_class, PROP_MAX_BUFFERING_TIME,
      g_param_spec_uint ("max-buffering-time", "Maximum buffering time",
          "Maximum number of seconds of buffer accumulated during playback"
          "(deprecated)",
          2, G_MAXUINT, DEFAULT_MAX_BUFFERING_TIME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED));

  g_object_class_install_property (gobject_class, PROP_BANDWIDTH_USAGE,
      g_param_spec_float ("bandwidth-usage",
          "Bandwidth usage [0..1]",
          "Percentage of the available bandwidth to use when "
          "selecting representations (deprecated)",
          0, 1, DEFAULT_BANDWIDTH_USAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED));
#endif

  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max bitrate",
          "Max of bitrate supported by target video decoder (0 = no maximum)",
          0, G_MAXUINT, DEFAULT_MAX_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_VIDEO_WIDTH,
      g_param_spec_uint ("max-video-width", "Max video width",
          "Max video width to select (0 = no maximum)",
          0, G_MAXUINT, DEFAULT_MAX_VIDEO_WIDTH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_VIDEO_HEIGHT,
      g_param_spec_uint ("max-video-height", "Max video height",
          "Max video height to select (0 = no maximum)",
          0, G_MAXUINT, DEFAULT_MAX_VIDEO_HEIGHT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_VIDEO_FRAMERATE,
      gst_param_spec_fraction ("max-video-framerate", "Max video framerate",
          "Max video framerate to select (0/1 = no maximum)",
          0, 1, G_MAXINT, 1, DEFAULT_MAX_VIDEO_FRAMERATE_N,
          DEFAULT_MAX_VIDEO_FRAMERATE_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PRESENTATION_DELAY,
      g_param_spec_string ("presentation-delay", "Presentation delay",
          "Default presentation delay (in seconds, milliseconds or fragments) (e.g. 12s, 2500ms, 3f)",
          DEFAULT_PRESENTATION_DELAY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dash_demux_audiosrc_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dash_demux_videosrc_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_dash_demux_subtitlesrc_template);

  gst_element_class_add_static_pad_template (gstelement_class, &sinktemplate);

  gst_element_class_set_static_metadata (gstelement_class,
      "DASH Demuxer",
      "Codec/Demuxer/Adaptive",
      "Dynamic Adaptive Streaming over HTTP demuxer",
      "David Corvoysier <david.corvoysier@orange.com>, "
      "Hamid Zakari <hamid.zakari@gmail.com>, "
      "Gianluca Gennari <gennarone@gmail.com>");


  gstadaptivedemux_class->get_duration = gst_dash_demux_get_duration;
  gstadaptivedemux_class->is_live = gst_dash_demux_is_live;
  gstadaptivedemux_class->reset = gst_dash_demux_reset;
  gstadaptivedemux_class->seek = gst_dash_demux_seek;

  gstadaptivedemux_class->process_manifest = gst_dash_demux_process_manifest;
  gstadaptivedemux_class->update_manifest_data =
      gst_dash_demux_update_manifest_data;
  gstadaptivedemux_class->get_manifest_update_interval =
      gst_dash_demux_get_manifest_update_interval;

  gstadaptivedemux_class->has_next_period = gst_dash_demux_has_next_period;
  gstadaptivedemux_class->advance_period = gst_dash_demux_advance_period;
  gstadaptivedemux_class->stream_has_next_fragment =
      gst_dash_demux_stream_has_next_fragment;
  gstadaptivedemux_class->stream_advance_fragment =
      gst_dash_demux_stream_advance_fragment;
  gstadaptivedemux_class->stream_get_fragment_waiting_time =
      gst_dash_demux_stream_get_fragment_waiting_time;
  gstadaptivedemux_class->stream_seek = gst_dash_demux_stream_seek;
  gstadaptivedemux_class->stream_select_bitrate =
      gst_dash_demux_stream_select_bitrate;
  gstadaptivedemux_class->stream_update_fragment_info =
      gst_dash_demux_stream_update_fragment_info;
  gstadaptivedemux_class->stream_free = gst_dash_demux_stream_free;
  gstadaptivedemux_class->get_live_seek_range =
      gst_dash_demux_get_live_seek_range;
  gstadaptivedemux_class->get_presentation_offset =
      gst_dash_demux_get_presentation_offset;
  gstadaptivedemux_class->get_period_start_time =
      gst_dash_demux_get_period_start_time;

  gstadaptivedemux_class->start_fragment = gst_dash_demux_stream_fragment_start;
  gstadaptivedemux_class->finish_fragment =
      gst_dash_demux_stream_fragment_finished;
  gstadaptivedemux_class->data_received = gst_dash_demux_data_received;
  gstadaptivedemux_class->need_another_chunk =
      gst_dash_demux_need_another_chunk;
}

static void
gst_dash_demux_init (GstDashDemux * demux)
{
  /* Properties */
  demux->max_buffering_time = DEFAULT_MAX_BUFFERING_TIME * GST_SECOND;
  demux->max_bitrate = DEFAULT_MAX_BITRATE;
  demux->max_video_width = DEFAULT_MAX_VIDEO_WIDTH;
  demux->max_video_height = DEFAULT_MAX_VIDEO_HEIGHT;
  demux->max_video_framerate_n = DEFAULT_MAX_VIDEO_FRAMERATE_N;
  demux->max_video_framerate_d = DEFAULT_MAX_VIDEO_FRAMERATE_D;
  demux->default_presentation_delay = g_strdup (DEFAULT_PRESENTATION_DELAY);

  g_mutex_init (&demux->client_lock);

  gst_adaptive_demux_set_stream_struct_size (GST_ADAPTIVE_DEMUX_CAST (demux),
      sizeof (GstDashDemuxStream));
}

static void
gst_dash_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAdaptiveDemux *adaptivedemux = GST_ADAPTIVE_DEMUX_CAST (object);
  GstDashDemux *demux = GST_DASH_DEMUX (object);

  switch (prop_id) {
    case PROP_MAX_BUFFERING_TIME:
      demux->max_buffering_time = g_value_get_uint (value) * GST_SECOND;
      break;
    case PROP_BANDWIDTH_USAGE:
      adaptivedemux->bitrate_limit = g_value_get_float (value);
      break;
    case PROP_MAX_BITRATE:
      demux->max_bitrate = g_value_get_uint (value);
      break;
    case PROP_MAX_VIDEO_WIDTH:
      demux->max_video_width = g_value_get_uint (value);
      break;
    case PROP_MAX_VIDEO_HEIGHT:
      demux->max_video_height = g_value_get_uint (value);
      break;
    case PROP_MAX_VIDEO_FRAMERATE:
      demux->max_video_framerate_n = gst_value_get_fraction_numerator (value);
      demux->max_video_framerate_d = gst_value_get_fraction_denominator (value);
      break;
    case PROP_PRESENTATION_DELAY:
      g_free (demux->default_presentation_delay);
      demux->default_presentation_delay = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dash_demux_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAdaptiveDemux *adaptivedemux = GST_ADAPTIVE_DEMUX_CAST (object);
  GstDashDemux *demux = GST_DASH_DEMUX (object);

  switch (prop_id) {
    case PROP_MAX_BUFFERING_TIME:
      g_value_set_uint (value, demux->max_buffering_time / GST_SECOND);
      break;
    case PROP_BANDWIDTH_USAGE:
      g_value_set_float (value, adaptivedemux->bitrate_limit);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, demux->max_bitrate);
      break;
    case PROP_MAX_VIDEO_WIDTH:
      g_value_set_uint (value, demux->max_video_width);
      break;
    case PROP_MAX_VIDEO_HEIGHT:
      g_value_set_uint (value, demux->max_video_height);
      break;
    case PROP_MAX_VIDEO_FRAMERATE:
      gst_value_set_fraction (value, demux->max_video_framerate_n,
          demux->max_video_framerate_d);
      break;
    case PROP_PRESENTATION_DELAY:
      if (demux->default_presentation_delay == NULL)
        g_value_set_static_string (value, "");
      else
        g_value_set_string (value, demux->default_presentation_delay);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dash_demux_setup_mpdparser_streams (GstDashDemux * demux,
    GstMPDClient * client)
{
  gboolean has_streams = FALSE;
  GList *adapt_sets, *iter;

  adapt_sets = gst_mpd_client_get_adaptation_sets (client);
  for (iter = adapt_sets; iter; iter = g_list_next (iter)) {
    GstMPDAdaptationSetNode *adapt_set_node = iter->data;

    gst_mpd_client_setup_streaming (client, adapt_set_node);
    has_streams = TRUE;
  }

  if (!has_streams) {
    GST_ELEMENT_ERROR (demux, STREAM, DEMUX, ("Manifest has no playable "
            "streams"), ("No streams could be activated from the manifest"));
  }
  return has_streams;
}

static gboolean
gst_dash_demux_setup_all_streams (GstDashDemux * demux)
{
  guint i;

  GST_DEBUG_OBJECT (demux, "Setting up streams for period %d",
      gst_mpd_client_get_period_index (demux->client));

  /* clean old active stream list, if any */
  gst_mpd_client_active_streams_free (demux->client);

  if (!gst_dash_demux_setup_mpdparser_streams (demux, demux->client)) {
    return FALSE;
  }

  GST_DEBUG_OBJECT (demux, "Creating stream objects");
  for (i = 0; i < gst_mpd_client_get_nb_active_stream (demux->client); i++) {
    GstDashDemuxStream *stream;
    GstActiveStream *active_stream;
    GstCaps *caps;
    GstStructure *s;
    GstPad *srcpad;
    gchar *lang = NULL;
    GstTagList *tags = NULL;
    gchar *track_id = NULL;

    active_stream =
        gst_mpd_client_get_active_stream_by_index (demux->client, i);
    if (active_stream == NULL)
      continue;

    if (demux->trickmode_no_audio
        && active_stream->mimeType == GST_STREAM_AUDIO) {
      GST_DEBUG_OBJECT (demux,
          "Skipping audio stream %d because of TRICKMODE_NO_AUDIO flag", i);
      continue;
    }

    srcpad = gst_dash_demux_create_pad (demux, active_stream);
    if (srcpad == NULL)
      continue;

    if (active_stream->cur_adapt_set) {
      /* FIXME: For CEA 608 and CEA 708 tracks we should check the Accessibility
       * descriptor:
       * https://dev.w3.org/html5/html-sourcing-inband-tracks/#mpegdash
       * * An ISOBMFF CEA 608 caption service: the string "cc" concatenated with
       * the value of the 'channel-number' field in the Accessibility descriptor
       * in the ContentComponent or AdaptationSet.
       * * An ISOBMFF CEA 708 caption service: the string "sn" concatenated with
       * the value of the 'service-number' field in the Accessibility descriptor
       * in the ContentComponent or AdaptationSet.
       * * Otherwise:
       */

      /* Content of the id attribute in the ContentComponent or AdaptationSet
       * element. */
      if (active_stream->cur_adapt_set->id) {
        track_id = g_strdup_printf ("%d", active_stream->cur_adapt_set->id);
      } else {
        GList *it;

        for (it = active_stream->cur_adapt_set->ContentComponents; it;
            it = it->next) {
          GstMPDContentComponentNode *cc_node = it->data;
          if (cc_node->id) {
            track_id = g_strdup_printf ("%u", cc_node->id);
            break;
          }
        }
        /* Empty string if the id attribute is not present on either
         * element. */
        if (!track_id)
          track_id = g_strdup ("");
      }
    }

    if (track_id) {
      tags =
          gst_tag_list_new (GST_TAG_CONTAINER_SPECIFIC_TRACK_ID, track_id,
          NULL);
      g_free (track_id);
    }

    caps = gst_dash_demux_get_input_caps (demux, active_stream);
    GST_LOG_OBJECT (demux, "Creating stream %d %" GST_PTR_FORMAT, i, caps);

    if (active_stream->cur_adapt_set) {
      GstMPDAdaptationSetNode *adp_set = active_stream->cur_adapt_set;
      lang = adp_set->lang;

      /* Fallback to the language in ContentComponent node */
      if (lang == NULL) {
        GList *it;

        for (it = adp_set->ContentComponents; it; it = it->next) {
          GstMPDContentComponentNode *cc_node = it->data;
          if (cc_node->lang) {
            lang = cc_node->lang;
            break;
          }
        }
      }
    }

    if (lang) {
      if (!tags)
        tags = gst_tag_list_new_empty ();
      if (gst_tag_check_language_code (lang))
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_LANGUAGE_CODE,
            lang, NULL);
      else
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_LANGUAGE_NAME,
            lang, NULL);
    }

    stream = (GstDashDemuxStream *)
        gst_adaptive_demux_stream_new (GST_ADAPTIVE_DEMUX_CAST (demux), srcpad);
    stream->active_stream = active_stream;

    if (active_stream->cur_representation) {
      stream->last_representation_id =
          g_strdup (stream->active_stream->cur_representation->id);
    } else {
      stream->last_representation_id = NULL;
    }

    s = gst_caps_get_structure (caps, 0);
    stream->allow_sidx =
        gst_mpd_client_has_isoff_ondemand_profile (demux->client);
    stream->is_isobmff = gst_structure_has_name (s, "video/quicktime")
        || gst_structure_has_name (s, "audio/x-m4a");
    stream->first_sync_sample_always_after_moof = TRUE;
    stream->adapter = gst_adapter_new ();
    gst_adaptive_demux_stream_set_caps (GST_ADAPTIVE_DEMUX_STREAM_CAST (stream),
        caps);
    if (tags)
      gst_adaptive_demux_stream_set_tags (GST_ADAPTIVE_DEMUX_STREAM_CAST
          (stream), tags);
    stream->index = i;
    stream->pending_seek_ts = GST_CLOCK_TIME_NONE;
    stream->sidx_position = GST_CLOCK_TIME_NONE;
    stream->actual_position = GST_CLOCK_TIME_NONE;
    stream->target_time = GST_CLOCK_TIME_NONE;
    /* Set a default average keyframe download time of a quarter of a second */
    stream->average_download_time = 250 * GST_MSECOND;

    if (active_stream->cur_adapt_set &&
        GST_MPD_REPRESENTATION_BASE_NODE (active_stream->
            cur_adapt_set)->ContentProtection) {
      GST_DEBUG_OBJECT (demux, "Adding ContentProtection events to source pad");
      g_list_foreach (GST_MPD_REPRESENTATION_BASE_NODE
          (active_stream->cur_adapt_set)->ContentProtection,
          gst_dash_demux_send_content_protection_event, stream);
    }

    gst_isoff_sidx_parser_init (&stream->sidx_parser);
  }

  return TRUE;
}

static void
gst_dash_demux_send_content_protection_event (gpointer data, gpointer userdata)
{
  GstMPDDescriptorTypeNode *cp = (GstMPDDescriptorTypeNode *) data;
  GstDashDemuxStream *stream = (GstDashDemuxStream *) userdata;
  GstEvent *event;
  GstBuffer *pssi;
  glong pssi_len;
  gchar *schemeIdUri;
  GstPad *pad = GST_ADAPTIVE_DEMUX_STREAM_PAD (stream);

  if (cp->schemeIdUri == NULL)
    return;

  GST_TRACE_OBJECT (pad, "check schemeIdUri %s", cp->schemeIdUri);
  /* RFC 2141 states: The leading "urn:" sequence is case-insensitive */
  schemeIdUri = g_ascii_strdown (cp->schemeIdUri, -1);
  if (g_str_has_prefix (schemeIdUri, "urn:uuid:")) {
    pssi_len = strlen (cp->value);
    pssi = gst_buffer_new_memdup (cp->value, pssi_len);
    /* RFC 4122 states that the hex part of a UUID is in lower case,
     * but some streams seem to ignore this and use upper case for the
     * protection system ID */
    event = gst_event_new_protection (cp->schemeIdUri + 9, pssi, "dash/mpd");
    GST_LOG_OBJECT (pad,
        "Queueing protection event %" GST_PTR_FORMAT " on source pad", event);
    gst_adaptive_demux_stream_queue_event ((GstAdaptiveDemuxStream *) stream,
        event);
    gst_buffer_unref (pssi);
  }
  g_free (schemeIdUri);
}

static GstClockTime
gst_dash_demux_get_duration (GstAdaptiveDemux * ademux)
{
  GstDashDemux *demux = GST_DASH_DEMUX_CAST (ademux);

  g_return_val_if_fail (demux->client != NULL, GST_CLOCK_TIME_NONE);

  return gst_mpd_client_get_media_presentation_duration (demux->client);
}

static gboolean
gst_dash_demux_is_live (GstAdaptiveDemux * ademux)
{
  GstDashDemux *demux = GST_DASH_DEMUX_CAST (ademux);

  g_return_val_if_fail (demux->client != NULL, FALSE);

  return gst_mpd_client_is_live (demux->client);
}

static gboolean
gst_dash_demux_setup_streams (GstAdaptiveDemux * demux)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  gboolean ret = TRUE;
  GstDateTime *now = NULL;
  guint period_idx;

  /* setup video, audio and subtitle streams, starting from first Period if
   * non-live */
  period_idx = 0;
  if (gst_mpd_client_is_live (dashdemux->client)) {
    GDateTime *g_now;
    if (dashdemux->client->mpd_root_node->availabilityStartTime == NULL) {
      ret = FALSE;
      GST_ERROR_OBJECT (demux, "MPD does not have availabilityStartTime");
      goto done;
    }
    if (dashdemux->clock_drift == NULL) {
      gchar **urls;
      urls =
          gst_mpd_client_get_utc_timing_sources (dashdemux->client,
          SUPPORTED_CLOCK_FORMATS, NULL);
      if (urls) {
        GST_DEBUG_OBJECT (dashdemux, "Found a supported UTCTiming element");
        dashdemux->clock_drift = gst_dash_demux_clock_drift_new (dashdemux);
        gst_dash_demux_poll_clock_drift (dashdemux);
      }
    }
    /* get period index for period encompassing the current time */
    g_now = gst_dash_demux_get_server_now_utc (dashdemux);
    now = gst_date_time_new_from_g_date_time (g_now);
    if (dashdemux->client->mpd_root_node->suggestedPresentationDelay != -1) {
      GstDateTime *target = gst_mpd_client_add_time_difference (now,
          dashdemux->client->mpd_root_node->suggestedPresentationDelay * -1000);
      gst_date_time_unref (now);
      now = target;
    } else if (dashdemux->default_presentation_delay) {
      gint64 dfp =
          gst_mpd_client_parse_default_presentation_delay (dashdemux->client,
          dashdemux->default_presentation_delay);
      GstDateTime *target = gst_mpd_client_add_time_difference (now,
          dfp * -1000);
      gst_date_time_unref (now);
      now = target;
    }
    period_idx =
        gst_mpd_client_get_period_index_at_time (dashdemux->client, now);
    if (period_idx == G_MAXUINT) {
#ifndef GST_DISABLE_GST_DEBUG
      gchar *date_str = gst_date_time_to_iso8601_string (now);
      GST_DEBUG_OBJECT (demux, "Unable to find live period active at %s",
          date_str);
      g_free (date_str);
#endif
      ret = FALSE;
      goto done;
    }
  }

  if (!gst_mpd_client_set_period_index (dashdemux->client, period_idx) ||
      !gst_dash_demux_setup_all_streams (dashdemux)) {
    ret = FALSE;
    goto done;
  }

  /* If stream is live, try to find the segment that
   * is closest to current time */
  if (gst_mpd_client_is_live (dashdemux->client)) {
    GDateTime *gnow;

    GST_DEBUG_OBJECT (demux, "Seeking to current time of day for live stream ");

    gnow = gst_date_time_to_g_date_time (now);
    gst_mpd_client_seek_to_time (dashdemux->client, gnow);
    g_date_time_unref (gnow);
  } else {
    GST_DEBUG_OBJECT (demux, "Seeking to first segment for on-demand stream ");

    /* start playing from the first segment */
    gst_mpd_client_seek_to_first_segment (dashdemux->client);
  }

done:
  if (now != NULL)
    gst_date_time_unref (now);
  return ret;
}

static gboolean
gst_dash_demux_process_manifest (GstAdaptiveDemux * demux, GstBuffer * buf)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  gboolean ret = FALSE;
  gchar *manifest;
  GstMapInfo mapinfo;

  if (dashdemux->client)
    gst_mpd_client_free (dashdemux->client);
  dashdemux->client = gst_mpd_client_new ();
  gst_mpd_client_set_uri_downloader (dashdemux->client, demux->downloader);

  dashdemux->client->mpd_uri = g_strdup (demux->manifest_uri);
  dashdemux->client->mpd_base_uri = g_strdup (demux->manifest_base_uri);

  GST_DEBUG_OBJECT (demux, "Fetched MPD file at URI: %s (base: %s)",
      dashdemux->client->mpd_uri,
      GST_STR_NULL (dashdemux->client->mpd_base_uri));

  if (gst_buffer_map (buf, &mapinfo, GST_MAP_READ)) {
    manifest = (gchar *) mapinfo.data;
    if (gst_mpd_client_parse (dashdemux->client, manifest, mapinfo.size)) {
      if (gst_mpd_client_setup_media_presentation (dashdemux->client, 0, 0,
              NULL)) {
        ret = TRUE;
      } else {
        GST_ELEMENT_ERROR (demux, STREAM, DECODE,
            ("Incompatible manifest file."), (NULL));
      }
    }
    gst_buffer_unmap (buf, &mapinfo);
  } else {
    GST_WARNING_OBJECT (demux, "Failed to map manifest buffer");
  }

  if (ret)
    ret = gst_dash_demux_setup_streams (demux);

  return ret;
}

static GstPad *
gst_dash_demux_create_pad (GstDashDemux * demux, GstActiveStream * stream)
{
  GstPad *pad;
  GstPadTemplate *tmpl;
  gchar *name;

  switch (stream->mimeType) {
    case GST_STREAM_AUDIO:
      name = g_strdup_printf ("audio_%02u", demux->n_audio_streams++);
      tmpl = gst_static_pad_template_get (&gst_dash_demux_audiosrc_template);
      break;
    case GST_STREAM_VIDEO:
      name = g_strdup_printf ("video_%02u", demux->n_video_streams++);
      tmpl = gst_static_pad_template_get (&gst_dash_demux_videosrc_template);
      break;
    case GST_STREAM_APPLICATION:
      if (gst_mpd_client_active_stream_contains_subtitles (stream)) {
        name = g_strdup_printf ("subtitle_%02u", demux->n_subtitle_streams++);
        tmpl =
            gst_static_pad_template_get (&gst_dash_demux_subtitlesrc_template);
      } else {
        return NULL;
      }
      break;
    default:
      g_assert_not_reached ();
      return NULL;
  }

  /* Create and activate new pads */
  pad = gst_pad_new_from_template (tmpl, name);
  g_free (name);
  gst_object_unref (tmpl);

  gst_pad_set_active (pad, TRUE);
  GST_INFO_OBJECT (demux, "Creating srcpad %s:%s", GST_DEBUG_PAD_NAME (pad));
  return pad;
}

static void
gst_dash_demux_reset (GstAdaptiveDemux * ademux)
{
  GstDashDemux *demux = GST_DASH_DEMUX_CAST (ademux);

  GST_DEBUG_OBJECT (demux, "Resetting demux");

  demux->end_of_period = FALSE;
  demux->end_of_manifest = FALSE;

  if (demux->client) {
    gst_mpd_client_free (demux->client);
    demux->client = NULL;
  }
  gst_dash_demux_clock_drift_free (demux->clock_drift);
  demux->clock_drift = NULL;
  demux->client = gst_mpd_client_new ();
  gst_mpd_client_set_uri_downloader (demux->client, ademux->downloader);

  demux->n_audio_streams = 0;
  demux->n_video_streams = 0;
  demux->n_subtitle_streams = 0;

  demux->trickmode_no_audio = FALSE;
  demux->allow_trickmode_key_units = TRUE;
}

static GstCaps *
gst_dash_demux_get_video_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  guint width = 0, height = 0;
  gint fps_num = 0, fps_den = 1;
  gboolean have_fps = FALSE;
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;

  /* if bitstreamSwitching is true we don't need to switch pads on resolution change */
  if (!gst_mpd_client_get_bitstream_switching_flag (stream)) {
    width = gst_mpd_client_get_video_stream_width (stream);
    height = gst_mpd_client_get_video_stream_height (stream);
    have_fps =
        gst_mpd_client_get_video_stream_framerate (stream, &fps_num, &fps_den);
  }
  caps = gst_mpd_client_get_stream_caps (stream);
  if (caps == NULL)
    return NULL;

  if (width > 0 && height > 0) {
    gst_caps_set_simple (caps, "width", G_TYPE_INT, width, "height",
        G_TYPE_INT, height, NULL);
  }

  if (have_fps) {
    gst_caps_set_simple (caps, "framerate", GST_TYPE_FRACTION, fps_num,
        fps_den, NULL);
  }

  return caps;
}

static GstCaps *
gst_dash_demux_get_audio_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  guint rate = 0, channels = 0;
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;

  /* if bitstreamSwitching is true we don't need to switch pads on rate/channels change */
  if (!gst_mpd_client_get_bitstream_switching_flag (stream)) {
    channels = gst_mpd_client_get_audio_stream_num_channels (stream);
    rate = gst_mpd_client_get_audio_stream_rate (stream);
  }
  caps = gst_mpd_client_get_stream_caps (stream);
  if (caps == NULL)
    return NULL;

  if (rate > 0) {
    gst_caps_set_simple (caps, "rate", G_TYPE_INT, rate, NULL);
  }
  if (channels > 0) {
    gst_caps_set_simple (caps, "channels", G_TYPE_INT, channels, NULL);
  }

  return caps;
}

static GstCaps *
gst_dash_demux_get_application_input_caps (GstDashDemux * demux,
    GstActiveStream * stream)
{
  GstCaps *caps = NULL;

  if (stream == NULL)
    return NULL;

  caps = gst_mpd_client_get_stream_caps (stream);
  if (caps == NULL)
    return NULL;

  return caps;
}

static GstCaps *
gst_dash_demux_get_input_caps (GstDashDemux * demux, GstActiveStream * stream)
{
  switch (stream->mimeType) {
    case GST_STREAM_VIDEO:
      return gst_dash_demux_get_video_input_caps (demux, stream);
    case GST_STREAM_AUDIO:
      return gst_dash_demux_get_audio_input_caps (demux, stream);
    case GST_STREAM_APPLICATION:
      return gst_dash_demux_get_application_input_caps (demux, stream);
    default:
      return GST_CAPS_NONE;
  }
}

static void
gst_dash_demux_stream_update_headers_info (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  gchar *path = NULL;

  gst_mpd_client_get_next_header (dashdemux->client,
      &path, dashstream->index,
      &stream->fragment.header_range_start, &stream->fragment.header_range_end);

  if (path != NULL) {
    stream->fragment.header_uri =
        gst_uri_join_strings (gst_mpd_client_get_baseURL (dashdemux->client,
            dashstream->index), path);
    g_free (path);
    path = NULL;
  }

  gst_mpd_client_get_next_header_index (dashdemux->client,
      &path, dashstream->index,
      &stream->fragment.index_range_start, &stream->fragment.index_range_end);

  if (path != NULL) {
    stream->fragment.index_uri =
        gst_uri_join_strings (gst_mpd_client_get_baseURL (dashdemux->client,
            dashstream->index), path);
    g_free (path);
  }
}

static GstFlowReturn
gst_dash_demux_stream_update_fragment_info (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  GstClockTime ts;
  GstMediaFragmentInfo fragment;
  gboolean isombff;

  gst_adaptive_demux_stream_fragment_clear (&stream->fragment);

  isombff = gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client);

  /* Reset chunk size if any */
  stream->fragment.chunk_size = 0;
  dashstream->current_fragment_keyframe_distance = GST_CLOCK_TIME_NONE;

  if (GST_ADAPTIVE_DEMUX_STREAM_NEED_HEADER (stream) && isombff) {
    gst_dash_demux_stream_update_headers_info (stream);
    /* sidx entries may not be available in here */
    if (stream->fragment.index_uri
        && dashstream->sidx_position != GST_CLOCK_TIME_NONE) {
      /* request only the index to be downloaded as we need to reposition the
       * stream to a subsegment */
      return GST_FLOW_OK;
    }
  }

  if (dashstream->moof_sync_samples
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux)) {
    GstDashStreamSyncSample *sync_sample =
        &g_array_index (dashstream->moof_sync_samples, GstDashStreamSyncSample,
        dashstream->current_sync_sample);

    gst_mpd_client_get_next_fragment (dashdemux->client, dashstream->index,
        &fragment);

    if (isombff && dashstream->sidx_position != GST_CLOCK_TIME_NONE
        && SIDX (dashstream)->entries) {
      GstSidxBoxEntry *entry = SIDX_CURRENT_ENTRY (dashstream);
      dashstream->current_fragment_timestamp = fragment.timestamp = entry->pts;
      dashstream->current_fragment_duration = fragment.duration =
          entry->duration;
    } else {
      dashstream->current_fragment_timestamp = fragment.timestamp;
      dashstream->current_fragment_duration = fragment.duration;
    }

    dashstream->current_fragment_keyframe_distance =
        fragment.duration / dashstream->moof_sync_samples->len;
    dashstream->actual_position =
        fragment.timestamp +
        dashstream->current_sync_sample *
        dashstream->current_fragment_keyframe_distance;
    if (stream->segment.rate < 0.0)
      dashstream->actual_position +=
          dashstream->current_fragment_keyframe_distance;
    dashstream->actual_position =
        MIN (dashstream->actual_position,
        fragment.timestamp + fragment.duration);

    stream->fragment.uri = fragment.uri;
    stream->fragment.timestamp = GST_CLOCK_TIME_NONE;
    stream->fragment.duration = GST_CLOCK_TIME_NONE;
    stream->fragment.range_start = sync_sample->start_offset;
    stream->fragment.range_end = sync_sample->end_offset;

    GST_DEBUG_OBJECT (stream->pad, "Actual position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (dashstream->actual_position));

    return GST_FLOW_OK;
  }

  if (gst_mpd_client_get_next_fragment_timestamp (dashdemux->client,
          dashstream->index, &ts)) {
    /* For live streams, check whether the underlying representation changed
     * (due to a manifest update with no matching representation) */
    if (gst_mpd_client_is_live (dashdemux->client)
        && !GST_ADAPTIVE_DEMUX_STREAM_NEED_HEADER (stream)) {
      if (dashstream->active_stream
          && dashstream->active_stream->cur_representation) {
        /* id specifies an identifier for this Representation. The
         * identifier shall be unique within a Period unless the
         * Representation is functionally identically to another
         * Representation in the same Period. */
        if (g_strcmp0 (dashstream->active_stream->cur_representation->id,
                dashstream->last_representation_id)) {
          GstCaps *caps;
          stream->need_header = TRUE;

          GST_INFO_OBJECT (dashdemux,
              "Representation changed from %s to %s - updating to bitrate %d",
              GST_STR_NULL (dashstream->last_representation_id),
              GST_STR_NULL (dashstream->active_stream->cur_representation->id),
              dashstream->active_stream->cur_representation->bandwidth);

          caps =
              gst_dash_demux_get_input_caps (dashdemux,
              dashstream->active_stream);
          gst_adaptive_demux_stream_set_caps (stream, caps);

          /* Update the stored last representation id */
          g_free (dashstream->last_representation_id);
          dashstream->last_representation_id =
              g_strdup (dashstream->active_stream->cur_representation->id);
        }
      } else {
        g_free (dashstream->last_representation_id);
        dashstream->last_representation_id = NULL;
      }
    }

    if (GST_ADAPTIVE_DEMUX_STREAM_NEED_HEADER (stream)) {
      gst_adaptive_demux_stream_fragment_clear (&stream->fragment);
      gst_dash_demux_stream_update_headers_info (stream);
    }

    gst_mpd_client_get_next_fragment (dashdemux->client, dashstream->index,
        &fragment);

    stream->fragment.uri = fragment.uri;
    /* If mpd does not specify indexRange (i.e., null index_uri),
     * sidx entries may not be available until download it */
    if (isombff && dashstream->sidx_position != GST_CLOCK_TIME_NONE
        && SIDX (dashstream)->entries) {
      GstSidxBoxEntry *entry = SIDX_CURRENT_ENTRY (dashstream);
      stream->fragment.range_start =
          dashstream->sidx_base_offset + entry->offset;
      dashstream->actual_position = stream->fragment.timestamp = entry->pts;
      dashstream->current_fragment_timestamp = stream->fragment.timestamp =
          entry->pts;
      dashstream->current_fragment_duration = stream->fragment.duration =
          entry->duration;
      if (stream->demux->segment.rate < 0.0) {
        stream->fragment.range_end =
            stream->fragment.range_start + entry->size - 1;
        dashstream->actual_position += entry->duration;
      } else {
        stream->fragment.range_end = fragment.range_end;
      }
    } else {
      dashstream->actual_position = stream->fragment.timestamp =
          fragment.timestamp;
      dashstream->current_fragment_timestamp = fragment.timestamp;
      dashstream->current_fragment_duration = stream->fragment.duration =
          fragment.duration;
      if (stream->demux->segment.rate < 0.0)
        dashstream->actual_position += fragment.duration;
      stream->fragment.range_start =
          MAX (fragment.range_start, dashstream->sidx_base_offset);
      stream->fragment.range_end = fragment.range_end;
    }

    GST_DEBUG_OBJECT (stream->pad, "Actual position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (dashstream->actual_position));

    return GST_FLOW_OK;
  }

  return GST_FLOW_EOS;
}

static gint
gst_dash_demux_index_entry_search (GstSidxBoxEntry * entry, GstClockTime * ts,
    gpointer user_data)
{
  GstClockTime entry_ts = entry->pts + entry->duration;
  if (entry_ts <= *ts)
    return -1;
  else if (entry->pts > *ts)
    return 1;
  else
    return 0;
}

static GstFlowReturn
gst_dash_demux_stream_sidx_seek (GstDashDemuxStream * dashstream,
    gboolean forward, GstSeekFlags flags, GstClockTime ts,
    GstClockTime * final_ts)
{
  GstSidxBox *sidx = SIDX (dashstream);
  GstSidxBoxEntry *entry;
  gint idx = sidx->entries_count;
  GstFlowReturn ret = GST_FLOW_OK;

  if (sidx->entries_count == 0)
    return GST_FLOW_EOS;

  entry =
      gst_util_array_binary_search (sidx->entries, sidx->entries_count,
      sizeof (GstSidxBoxEntry),
      (GCompareDataFunc) gst_dash_demux_index_entry_search,
      GST_SEARCH_MODE_EXACT, &ts, NULL);

  /* No exact match found, nothing in our index
   * This is usually a bug or broken stream, as the seeking code already
   * makes sure that we're in the correct period and segment, and only need
   * to find the correct place inside the segment. Allow for some rounding
   * errors and inaccuracies here though */
  if (!entry) {
    GstSidxBoxEntry *last_entry = &sidx->entries[sidx->entries_count - 1];

    GST_WARNING_OBJECT (dashstream->parent.pad, "Couldn't find SIDX entry");

    if (ts < sidx->entries[0].pts
        && ts + 250 * GST_MSECOND >= sidx->entries[0].pts)
      entry = &sidx->entries[0];
    else if (ts >= last_entry->pts + last_entry->duration &&
        ts < last_entry->pts + last_entry->duration + 250 * GST_MSECOND)
      entry = last_entry;
  }
  if (!entry)
    return GST_FLOW_EOS;

  idx = entry - sidx->entries;

  /* FIXME in reverse mode, if we are exactly at a fragment start it makes more
   * sense to start from the end of the previous fragment */
  if (!forward && idx > 0 && entry->pts == ts) {
    idx--;
    entry = &sidx->entries[idx];
  }

  /* Now entry->pts <= ts < entry->pts + entry->duration, need to adjust for
   * snapping */
  if ((flags & GST_SEEK_FLAG_SNAP_NEAREST) == GST_SEEK_FLAG_SNAP_NEAREST) {
    if (idx + 1 < sidx->entries_count
        && sidx->entries[idx + 1].pts - ts < ts - sidx->entries[idx].pts)
      idx += 1;
  } else if ((forward && (flags & GST_SEEK_FLAG_SNAP_AFTER)) || (!forward
          && (flags & GST_SEEK_FLAG_SNAP_BEFORE))) {
    if (idx + 1 < sidx->entries_count && entry->pts < ts)
      idx += 1;
  }

  g_assert (sidx->entry_index < sidx->entries_count);

  sidx->entry_index = idx;
  dashstream->sidx_position = sidx->entries[idx].pts;

  if (final_ts)
    *final_ts = dashstream->sidx_position;

  return ret;
}

static GstFlowReturn
gst_dash_demux_stream_seek (GstAdaptiveDemuxStream * stream, gboolean forward,
    GstSeekFlags flags, GstClockTime ts, GstClockTime * final_ts)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  gint last_index, last_repeat;
  gboolean is_isobmff;

  last_index = dashstream->active_stream->segment_index;
  last_repeat = dashstream->active_stream->segment_repeat_index;

  if (dashstream->adapter)
    gst_adapter_clear (dashstream->adapter);
  dashstream->current_offset = -1;
  dashstream->current_index_header_or_data = 0;

  dashstream->isobmff_parser.current_fourcc = 0;
  dashstream->isobmff_parser.current_start_offset = 0;
  dashstream->isobmff_parser.current_size = 0;

  if (dashstream->moof)
    gst_isoff_moof_box_free (dashstream->moof);
  dashstream->moof = NULL;
  if (dashstream->moof_sync_samples)
    g_array_free (dashstream->moof_sync_samples, TRUE);
  dashstream->moof_sync_samples = NULL;
  dashstream->current_sync_sample = -1;
  dashstream->target_time = GST_CLOCK_TIME_NONE;

  is_isobmff = gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client);

  if (!gst_mpd_client_stream_seek (dashdemux->client, dashstream->active_stream,
          forward,
          is_isobmff ? (flags & (~(GST_SEEK_FLAG_SNAP_BEFORE |
                      GST_SEEK_FLAG_SNAP_AFTER))) : flags, ts, final_ts)) {
    return GST_FLOW_EOS;
  }

  if (is_isobmff) {
    GstClockTime period_start, offset;

    period_start = gst_mpd_client_get_period_start_time (dashdemux->client);
    offset =
        gst_mpd_client_get_stream_presentation_offset (dashdemux->client,
        dashstream->index);

    if (G_UNLIKELY (ts < period_start))
      ts = offset;
    else
      ts += offset - period_start;

    if (last_index != dashstream->active_stream->segment_index ||
        last_repeat != dashstream->active_stream->segment_repeat_index) {
      GST_LOG_OBJECT (stream->pad,
          "Segment index was changed, reset sidx parser");
      gst_isoff_sidx_parser_clear (&dashstream->sidx_parser);
      dashstream->sidx_base_offset = 0;
      dashstream->allow_sidx = TRUE;
    }

    if (dashstream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
      if (gst_dash_demux_stream_sidx_seek (dashstream, forward, flags, ts,
              final_ts) != GST_FLOW_OK) {
        GST_ERROR_OBJECT (stream->pad, "Couldn't find position in sidx");
        dashstream->sidx_position = GST_CLOCK_TIME_NONE;
        gst_isoff_sidx_parser_clear (&dashstream->sidx_parser);
      }
      dashstream->pending_seek_ts = GST_CLOCK_TIME_NONE;
    } else {
      /* no index yet, seek when we have it */
      /* FIXME - the final_ts won't be correct here */
      dashstream->pending_seek_ts = ts;
    }
  }

  stream->discont = TRUE;

  return GST_FLOW_OK;
}

static gboolean
gst_dash_demux_stream_has_next_sync_sample (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  if (dashstream->moof_sync_samples &&
      GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux)) {
    if (stream->demux->segment.rate > 0.0) {
      if (dashstream->current_sync_sample + 1 <
          dashstream->moof_sync_samples->len)
        return TRUE;
    } else {
      if (dashstream->current_sync_sample >= 1)
        return TRUE;
    }
  }
  return FALSE;
}

static gboolean
gst_dash_demux_stream_has_next_subfragment (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstSidxBox *sidx = SIDX (dashstream);

  if (dashstream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
    if (stream->demux->segment.rate > 0.0) {
      if (sidx->entry_index + 1 < sidx->entries_count)
        return TRUE;
    } else {
      if (sidx->entry_index >= 1)
        return TRUE;
    }
  }
  return FALSE;
}

static gboolean
gst_dash_demux_stream_advance_sync_sample (GstAdaptiveDemuxStream * stream,
    GstClockTime target_time)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  gboolean fragment_finished = FALSE;
  guint idx = -1;

  if (GST_CLOCK_TIME_IS_VALID (target_time)) {
    GST_LOG_OBJECT (stream->pad,
        "target_time:%" GST_TIME_FORMAT " fragment ts %" GST_TIME_FORMAT
        " average keyframe dist: %" GST_TIME_FORMAT
        " current keyframe dist: %" GST_TIME_FORMAT
        " fragment duration:%" GST_TIME_FORMAT,
        GST_TIME_ARGS (target_time),
        GST_TIME_ARGS (dashstream->current_fragment_timestamp),
        GST_TIME_ARGS (dashstream->keyframe_average_distance),
        GST_TIME_ARGS (dashstream->current_fragment_keyframe_distance),
        GST_TIME_ARGS (stream->fragment.duration));

    if (stream->demux->segment.rate > 0.0) {
      idx =
          (target_time -
          dashstream->current_fragment_timestamp) /
          dashstream->current_fragment_keyframe_distance;

      /* Prevent getting stuck in a loop due to rounding errors */
      if (idx == dashstream->current_sync_sample)
        idx++;
    } else {
      GstClockTime end_time =
          dashstream->current_fragment_timestamp +
          dashstream->current_fragment_duration;

      if (end_time < target_time) {
        idx = dashstream->moof_sync_samples->len;
      } else {
        idx =
            (end_time -
            target_time) / dashstream->current_fragment_keyframe_distance;
        if (idx == dashstream->moof_sync_samples->len) {
          dashstream->current_sync_sample = -1;
          fragment_finished = TRUE;
          goto beach;
        }
        idx = dashstream->moof_sync_samples->len - 1 - idx;
      }

      /* Prevent getting stuck in a loop due to rounding errors */
      if (idx == dashstream->current_sync_sample) {
        if (idx == 0) {
          dashstream->current_sync_sample = -1;
          fragment_finished = TRUE;
          goto beach;
        }

        idx--;
      }
    }
  }

  GST_DEBUG_OBJECT (stream->pad,
      "Advancing sync sample #%d target #%d",
      dashstream->current_sync_sample, idx);

  if (idx != -1 && idx >= dashstream->moof_sync_samples->len) {
    dashstream->current_sync_sample = -1;
    fragment_finished = TRUE;
    goto beach;
  }

  if (stream->demux->segment.rate > 0.0) {
    /* Try to get the sync sample for the target time */
    if (idx != -1) {
      dashstream->current_sync_sample = idx;
    } else {
      dashstream->current_sync_sample++;
      if (dashstream->current_sync_sample >= dashstream->moof_sync_samples->len) {
        fragment_finished = TRUE;
      }
    }
  } else {
    if (idx != -1) {
      dashstream->current_sync_sample = idx;
    } else if (dashstream->current_sync_sample == -1) {
      dashstream->current_sync_sample = dashstream->moof_sync_samples->len - 1;
    } else if (dashstream->current_sync_sample == 0) {
      dashstream->current_sync_sample = -1;
      fragment_finished = TRUE;
    } else {
      dashstream->current_sync_sample--;
    }
  }

beach:
  GST_DEBUG_OBJECT (stream->pad,
      "Advancing sync sample #%d fragment_finished:%d",
      dashstream->current_sync_sample, fragment_finished);

  if (!fragment_finished)
    stream->discont = TRUE;

  return !fragment_finished;
}

static gboolean
gst_dash_demux_stream_advance_subfragment (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  GstSidxBox *sidx = SIDX (dashstream);
  gboolean fragment_finished = TRUE;

  if (dashstream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
    if (stream->demux->segment.rate > 0.0) {
      gint idx = ++sidx->entry_index;
      if (idx < sidx->entries_count) {
        fragment_finished = FALSE;
      }

      if (idx == sidx->entries_count)
        dashstream->sidx_position =
            sidx->entries[idx - 1].pts + sidx->entries[idx - 1].duration;
      else
        dashstream->sidx_position = sidx->entries[idx].pts;
    } else {
      gint idx = --sidx->entry_index;

      if (idx >= 0) {
        fragment_finished = FALSE;
        dashstream->sidx_position = sidx->entries[idx].pts;
      } else {
        dashstream->sidx_position = GST_CLOCK_TIME_NONE;
      }
    }
  }

  GST_DEBUG_OBJECT (stream->pad, "New sidx index: %d / %d. "
      "Finished fragment: %d", sidx->entry_index, sidx->entries_count,
      fragment_finished);

  return !fragment_finished;
}

static gboolean
gst_dash_demux_stream_has_next_fragment (GstAdaptiveDemuxStream * stream)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  if (dashstream->moof_sync_samples &&
      GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux)) {
    if (gst_dash_demux_stream_has_next_sync_sample (stream))
      return TRUE;
  }

  if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client)) {
    if (gst_dash_demux_stream_has_next_subfragment (stream))
      return TRUE;
  }

  return gst_mpd_client_has_next_segment (dashdemux->client,
      dashstream->active_stream, stream->demux->segment.rate > 0.0);
}

/* The goal here is to figure out, once we have pushed a keyframe downstream,
 * what the next ideal keyframe to download is.
 *
 * This is done based on:
 * * the current internal position (i.e. actual_position)
 * * the reported downstream position (QoS feedback)
 * * the average keyframe download time (average_download_time)
 */
static GstClockTime
gst_dash_demux_stream_get_target_time (GstDashDemux * dashdemux,
    GstAdaptiveDemuxStream * stream, GstClockTime cur_position,
    GstClockTime min_skip)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstClockTime cur_running, min_running, min_position;
  GstClockTimeDiff diff;
  GstClockTime ret = cur_position;
  GstClockTime deadline;
  GstClockTime upstream_earliest_time;
  GstClockTime earliest_time = GST_CLOCK_TIME_NONE;

  g_assert (min_skip > 0);

  /* minimum stream position we have to skip to */
  if (stream->segment.rate > 0)
    min_position = cur_position + min_skip;
  else if (cur_position < min_skip)
    min_position = 0;
  else
    min_position = cur_position - min_skip;

  /* Use current clock time or the QoS earliest time, whichever is further in
   * the future. The QoS time is only updated on every QoS event and
   * especially not if e.g. a videodecoder or converter drops a frame further
   * downstream.
   *
   * We only use the times if we ever received a QoS event since the last
   * flush, as otherwise base_time and clock might not be correct because of a
   * still pre-rolling sink
   */
  upstream_earliest_time =
      gst_adaptive_demux_get_qos_earliest_time ((GstAdaptiveDemux *) dashdemux);
  if (upstream_earliest_time != GST_CLOCK_TIME_NONE) {
    GstClock *clock;

    clock = gst_element_get_clock (GST_ELEMENT_CAST (dashdemux));

    if (clock) {
      GstClockTime base_time;
      GstClockTime now_time;

      base_time = gst_element_get_base_time (GST_ELEMENT_CAST (dashdemux));
      now_time = gst_clock_get_time (clock);
      if (now_time > base_time)
        now_time -= base_time;
      else
        now_time = 0;

      gst_object_unref (clock);

      earliest_time = MAX (now_time, upstream_earliest_time);
    } else {
      earliest_time = upstream_earliest_time;
    }
  }

  /* our current position in running time */
  cur_running =
      gst_segment_to_running_time (&stream->segment, GST_FORMAT_TIME,
      cur_position);

  /* the minimum position we have to skip to in running time */
  min_running =
      gst_segment_to_running_time (&stream->segment, GST_FORMAT_TIME,
      min_position);

  GST_DEBUG_OBJECT (stream->pad,
      "position: current %" GST_TIME_FORMAT " min next %" GST_TIME_FORMAT,
      GST_TIME_ARGS (cur_position), GST_TIME_ARGS (min_position));
  GST_DEBUG_OBJECT (stream->pad,
      "running time: current %" GST_TIME_FORMAT " min next %" GST_TIME_FORMAT
      " earliest %" GST_TIME_FORMAT, GST_TIME_ARGS (cur_running),
      GST_TIME_ARGS (min_running), GST_TIME_ARGS (earliest_time));

  /* Take configured maximum video bandwidth and framerate into account */
  {
    GstClockTime min_run_dist, min_frame_dist, diff = 0;
    guint max_fps_n, max_fps_d;

    min_run_dist = min_skip / ABS (stream->segment.rate);

    if (dashdemux->max_video_framerate_n != 0) {
      max_fps_n = dashdemux->max_video_framerate_n;
      max_fps_d = dashdemux->max_video_framerate_d;
    } else {
      /* more than 10 fps is not very useful if we're skipping anyway */
      max_fps_n = 10;
      max_fps_d = 1;
    }

    min_frame_dist = gst_util_uint64_scale_ceil (GST_SECOND,
        max_fps_d, max_fps_n);

    GST_DEBUG_OBJECT (stream->pad,
        "Have max framerate %d/%d - Min dist %" GST_TIME_FORMAT
        ", min requested dist %" GST_TIME_FORMAT,
        max_fps_n, max_fps_d,
        GST_TIME_ARGS (min_run_dist), GST_TIME_ARGS (min_frame_dist));
    if (min_frame_dist > min_run_dist)
      diff = MAX (diff, min_frame_dist - min_run_dist);

    if (dashdemux->max_bitrate != 0) {
      guint64 max_bitrate = gst_util_uint64_scale_ceil (GST_SECOND,
          8 * dashstream->keyframe_average_size,
          dashstream->keyframe_average_distance) * ABS (stream->segment.rate);

      if (max_bitrate > dashdemux->max_bitrate) {
        min_frame_dist = gst_util_uint64_scale_ceil (GST_SECOND,
            8 * dashstream->keyframe_average_size,
            dashdemux->max_bitrate) * ABS (stream->segment.rate);

        GST_DEBUG_OBJECT (stream->pad,
            "Have max bitrate %u - Min dist %" GST_TIME_FORMAT
            ", min requested dist %" GST_TIME_FORMAT, dashdemux->max_bitrate,
            GST_TIME_ARGS (min_run_dist), GST_TIME_ARGS (min_frame_dist));
        if (min_frame_dist > min_run_dist)
          diff = MAX (diff, min_frame_dist - min_run_dist);
      }
    }

    if (diff > 0) {
      GST_DEBUG_OBJECT (stream->pad,
          "Skipping further ahead by %" GST_TIME_FORMAT, GST_TIME_ARGS (diff));
      min_running += diff;
    }
  }

  if (earliest_time == GST_CLOCK_TIME_NONE) {
    GstClockTime run_key_dist;

    run_key_dist =
        dashstream->keyframe_average_distance / ABS (stream->segment.rate);

    /* If we don't have downstream information (such as at startup or
     * without live sinks), just get the next time by taking the minimum
     * amount we have to skip ahead
     * Except if it takes us longer to download */
    if (run_key_dist > dashstream->average_download_time)
      ret =
          gst_segment_position_from_running_time (&stream->segment,
          GST_FORMAT_TIME, min_running);
    else
      ret = gst_segment_position_from_running_time (&stream->segment,
          GST_FORMAT_TIME,
          min_running - run_key_dist + dashstream->average_download_time);

    GST_DEBUG_OBJECT (stream->pad,
        "Advancing to %" GST_TIME_FORMAT " (was %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (ret), GST_TIME_ARGS (min_position));

    goto out;
  }

  /* Figure out the difference, in running time, between where we are and
   * where downstream is */
  diff = min_running - earliest_time;
  GST_LOG_OBJECT (stream->pad,
      "min_running %" GST_TIME_FORMAT " diff %" GST_STIME_FORMAT
      " average_download %" GST_TIME_FORMAT, GST_TIME_ARGS (min_running),
      GST_STIME_ARGS (diff), GST_TIME_ARGS (dashstream->average_download_time));

  /* Have at least 500ms or 3 keyframes safety between current position and downstream */
  deadline = MAX (500 * GST_MSECOND, 3 * dashstream->average_download_time);

  /* The furthest away we are from the current position, the least we need to advance */
  if (diff < 0 || diff < deadline) {
    /* Force skipping (but not more than 1s ahead) */
    ret =
        gst_segment_position_from_running_time (&stream->segment,
        GST_FORMAT_TIME, earliest_time + MIN (deadline, GST_SECOND));
    GST_DEBUG_OBJECT (stream->pad,
        "MUST SKIP to at least %" GST_TIME_FORMAT " (was %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (ret), GST_TIME_ARGS (min_position));
  } else if (diff < 4 * dashstream->average_download_time) {
    /* Go forward a bit less aggressively (and at most 1s forward) */
    ret = gst_segment_position_from_running_time (&stream->segment,
        GST_FORMAT_TIME, min_running + MIN (GST_SECOND,
            2 * dashstream->average_download_time));
    GST_DEBUG_OBJECT (stream->pad,
        "MUST SKIP to at least %" GST_TIME_FORMAT " (was %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (ret), GST_TIME_ARGS (min_position));
  } else {
    /* Get the next position satisfying the download time */
    ret = gst_segment_position_from_running_time (&stream->segment,
        GST_FORMAT_TIME, min_running);
    GST_DEBUG_OBJECT (stream->pad,
        "Advance to %" GST_TIME_FORMAT " (was %" GST_TIME_FORMAT ")",
        GST_TIME_ARGS (ret), GST_TIME_ARGS (min_position));
  }

out:

  {
    GstClockTime cur_skip =
        (cur_position < ret) ? ret - cur_position : cur_position - ret;

    if (dashstream->average_skip_size == 0) {
      dashstream->average_skip_size = cur_skip;
    } else {
      dashstream->average_skip_size =
          (cur_skip + 3 * dashstream->average_skip_size) / 4;
    }

    if (dashstream->average_skip_size >
        cur_skip + dashstream->keyframe_average_distance
        && dashstream->average_skip_size > min_skip) {
      if (stream->segment.rate > 0)
        ret = cur_position + dashstream->average_skip_size;
      else if (cur_position > dashstream->average_skip_size)
        ret = cur_position - dashstream->average_skip_size;
      else
        ret = 0;
    }
  }

  return ret;
}

static GstFlowReturn
gst_dash_demux_stream_advance_fragment (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  GstClockTime target_time = GST_CLOCK_TIME_NONE;
  GstClockTime previous_position;
  GstFlowReturn ret;

  GST_DEBUG_OBJECT (stream->pad, "Advance fragment");

  /* Update download statistics */
  if (dashstream->moof_sync_samples &&
      GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux) &&
      GST_CLOCK_TIME_IS_VALID (stream->last_download_time)) {
    if (GST_CLOCK_TIME_IS_VALID (dashstream->average_download_time)) {
      dashstream->average_download_time =
          (3 * dashstream->average_download_time +
          stream->last_download_time) / 4;
    } else {
      dashstream->average_download_time = stream->last_download_time;
    }

    GST_DEBUG_OBJECT (stream->pad,
        "Download time last: %" GST_TIME_FORMAT " average: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (stream->last_download_time),
        GST_TIME_ARGS (dashstream->average_download_time));
  }

  previous_position = dashstream->actual_position;

  /* Update internal position */
  if (GST_CLOCK_TIME_IS_VALID (dashstream->actual_position)) {
    GstClockTime dur;
    if (dashstream->moof_sync_samples
        && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux)) {
      GST_LOG_OBJECT (stream->pad, "current sync sample #%d",
          dashstream->current_sync_sample);
      if (dashstream->current_sync_sample == -1) {
        dur = 0;
      } else if (dashstream->current_sync_sample <
          dashstream->moof_sync_samples->len) {
        dur = dashstream->current_fragment_keyframe_distance;
      } else {
        if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client) &&
            dashstream->sidx_position != GST_CLOCK_TIME_NONE
            && SIDX (dashstream)->entries) {
          GstSidxBoxEntry *entry = SIDX_CURRENT_ENTRY (dashstream);
          dur = entry->duration;
        } else {
          dur =
              dashstream->current_fragment_timestamp +
              dashstream->current_fragment_duration -
              dashstream->actual_position;
        }
      }
    } else if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client) &&
        dashstream->sidx_position != GST_CLOCK_TIME_NONE
        && SIDX (dashstream)->entries) {
      GstSidxBoxEntry *entry = SIDX_CURRENT_ENTRY (dashstream);
      dur = entry->duration;
    } else {
      dur = stream->fragment.duration;
    }

    if (dashstream->moof_sync_samples
        && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux)) {
      /* We just downloaded the header, we actually use the previous
       * target_time now as it was not used up yet */
      if (dashstream->current_sync_sample == -1)
        target_time = dashstream->target_time;
      else
        target_time =
            gst_dash_demux_stream_get_target_time (dashdemux, stream,
            dashstream->actual_position, dur);
      dashstream->actual_position = target_time;
    } else {
      /* Adjust based on direction */
      if (stream->demux->segment.rate > 0.0)
        dashstream->actual_position += dur;
      else if (dashstream->actual_position >= dur)
        dashstream->actual_position -= dur;
      else
        dashstream->actual_position = 0;
    }

    GST_DEBUG_OBJECT (stream->pad, "Actual position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (dashstream->actual_position));
  }
  dashstream->target_time = target_time;

  GST_DEBUG_OBJECT (stream->pad, "target_time: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (target_time));

  /* If downloading only keyframes, switch to the next one or fall through */
  if (dashstream->moof_sync_samples &&
      GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux)) {
    if (gst_dash_demux_stream_advance_sync_sample (stream, target_time))
      return GST_FLOW_OK;
  }

  dashstream->isobmff_parser.current_fourcc = 0;
  dashstream->isobmff_parser.current_start_offset = 0;
  dashstream->isobmff_parser.current_size = 0;

  if (dashstream->moof)
    gst_isoff_moof_box_free (dashstream->moof);
  dashstream->moof = NULL;
  if (dashstream->moof_sync_samples)
    g_array_free (dashstream->moof_sync_samples, TRUE);
  dashstream->moof_sync_samples = NULL;
  dashstream->current_sync_sample = -1;

  /* Check if we just need to 'advance' to the next fragment, or if we
   * need to skip by more. */
  if (GST_CLOCK_TIME_IS_VALID (target_time)
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux) &&
      dashstream->active_stream->mimeType == GST_STREAM_VIDEO) {
    GstClockTime actual_ts;
    GstSeekFlags flags = 0;

    /* Key-unit trick mode, seek to fragment containing target time
     *
     * We first try seeking without snapping. As above code to skip keyframes
     * in the current fragment was not successful, we should go at least one
     * fragment ahead. Due to rounding errors we could end up at the same
     * fragment again here, in which case we retry seeking with the SNAP_AFTER
     * flag.
     *
     * We don't always set that flag as we would then end up one further
     * fragment in the future in all good cases.
     */
    while (TRUE) {
      ret =
          gst_dash_demux_stream_seek (stream, (stream->segment.rate > 0), flags,
          target_time, &actual_ts);

      if (ret != GST_FLOW_OK) {
        GST_WARNING_OBJECT (stream->pad, "Failed to seek to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (target_time));
        /* Give up */
        if (flags != 0)
          break;

        /* Retry with skipping ahead */
        flags |= GST_SEEK_FLAG_SNAP_AFTER;
        continue;
      }

      GST_DEBUG_OBJECT (stream->pad,
          "Skipped to %" GST_TIME_FORMAT " (wanted %" GST_TIME_FORMAT ", was %"
          GST_TIME_FORMAT ")", GST_TIME_ARGS (actual_ts),
          GST_TIME_ARGS (target_time), GST_TIME_ARGS (previous_position));

      if ((stream->segment.rate > 0 && actual_ts <= previous_position) ||
          (stream->segment.rate < 0 && actual_ts >= previous_position)) {
        /* Give up */
        if (flags != 0)
          break;

        /* Retry with forcing skipping ahead */
        flags |= GST_SEEK_FLAG_SNAP_AFTER;

        continue;
      }

      /* All good */
      break;
    }
  } else {
    /* Normal mode, advance to the next fragment */
    if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client)) {
      if (gst_dash_demux_stream_advance_subfragment (stream))
        return GST_FLOW_OK;
    }

    if (dashstream->adapter)
      gst_adapter_clear (dashstream->adapter);

    gst_isoff_sidx_parser_clear (&dashstream->sidx_parser);
    dashstream->sidx_base_offset = 0;
    dashstream->sidx_position = GST_CLOCK_TIME_NONE;
    dashstream->allow_sidx = TRUE;

    ret = gst_mpd_client_advance_segment (dashdemux->client,
        dashstream->active_stream, stream->demux->segment.rate > 0.0);
  }
  return ret;
}

static gboolean
gst_dash_demux_stream_select_bitrate (GstAdaptiveDemuxStream * stream,
    guint64 bitrate)
{
  GstActiveStream *active_stream = NULL;
  GList *rep_list = NULL;
  gint new_index;
  GstAdaptiveDemux *base_demux = stream->demux;
  GstDashDemux *demux = GST_DASH_DEMUX_CAST (stream->demux);
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  gboolean ret = FALSE;

  active_stream = dashstream->active_stream;
  if (active_stream == NULL) {
    goto end;
  }

  /* In key-frame trick mode don't change bitrates */
  if (GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (demux)) {
    GST_DEBUG_OBJECT (demux, "In key-frame trick mode, not changing bitrates");
    goto end;
  }

  /* retrieve representation list */
  if (active_stream->cur_adapt_set)
    rep_list = active_stream->cur_adapt_set->Representations;
  if (!rep_list) {
    goto end;
  }

  GST_DEBUG_OBJECT (stream->pad,
      "Trying to change to bitrate: %" G_GUINT64_FORMAT, bitrate);

  if (active_stream->mimeType == GST_STREAM_VIDEO && demux->max_bitrate) {
    bitrate = MIN (demux->max_bitrate, bitrate);
  }

  /* get representation index with current max_bandwidth */
  if (GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (base_demux) ||
      ABS (base_demux->segment.rate) <= 1.0) {
    new_index =
        gst_mpd_client_get_rep_idx_with_max_bandwidth (rep_list, bitrate,
        demux->max_video_width, demux->max_video_height,
        demux->max_video_framerate_n, demux->max_video_framerate_d);
  } else {
    new_index =
        gst_mpd_client_get_rep_idx_with_max_bandwidth (rep_list,
        bitrate / ABS (base_demux->segment.rate), demux->max_video_width,
        demux->max_video_height, demux->max_video_framerate_n,
        demux->max_video_framerate_d);
  }

  /* if no representation has the required bandwidth, take the lowest one */
  if (new_index == -1)
    new_index = gst_mpd_client_get_rep_idx_with_min_bandwidth (rep_list);

  if (new_index != active_stream->representation_idx) {
    GstMPDRepresentationNode *rep = g_list_nth_data (rep_list, new_index);
    GST_INFO_OBJECT (demux, "Changing representation idx: %d %d %u",
        dashstream->index, new_index, rep->bandwidth);
    if (gst_mpd_client_setup_representation (demux->client, active_stream, rep)) {
      GstCaps *caps;

      GST_INFO_OBJECT (demux, "Switching bitrate to %d",
          active_stream->cur_representation->bandwidth);
      caps = gst_dash_demux_get_input_caps (demux, active_stream);
      gst_adaptive_demux_stream_set_caps (stream, caps);
      ret = TRUE;

      /* Update the stored last representation id */
      g_free (dashstream->last_representation_id);
      dashstream->last_representation_id =
          g_strdup (active_stream->cur_representation->id);
    } else {
      GST_WARNING_OBJECT (demux, "Can not switch representation, aborting...");
    }
  }

  if (ret) {
    if (gst_mpd_client_has_isoff_ondemand_profile (demux->client)
        && SIDX (dashstream)->entries) {
      /* store our current position to change to the same one in a different
       * representation if needed */
      if (SIDX (dashstream)->entry_index < SIDX (dashstream)->entries_count)
        dashstream->sidx_position = SIDX_CURRENT_ENTRY (dashstream)->pts;
      else if (SIDX (dashstream)->entry_index >=
          SIDX (dashstream)->entries_count)
        dashstream->sidx_position =
            SIDX_ENTRY (dashstream,
            SIDX (dashstream)->entries_count - 1)->pts + SIDX_ENTRY (dashstream,
            SIDX (dashstream)->entries_count - 1)->duration;
      else
        dashstream->sidx_position = GST_CLOCK_TIME_NONE;
    } else {
      dashstream->sidx_position = GST_CLOCK_TIME_NONE;
    }

    gst_isoff_sidx_parser_clear (&dashstream->sidx_parser);
    dashstream->sidx_base_offset = 0;
    dashstream->allow_sidx = TRUE;

    /* Reset ISOBMFF box parsing state */
    dashstream->isobmff_parser.current_fourcc = 0;
    dashstream->isobmff_parser.current_start_offset = 0;
    dashstream->isobmff_parser.current_size = 0;

    dashstream->current_offset = -1;
    dashstream->current_index_header_or_data = 0;

    if (dashstream->adapter)
      gst_adapter_clear (dashstream->adapter);

    if (dashstream->moof)
      gst_isoff_moof_box_free (dashstream->moof);
    dashstream->moof = NULL;
    if (dashstream->moof_sync_samples)
      g_array_free (dashstream->moof_sync_samples, TRUE);
    dashstream->moof_sync_samples = NULL;
    dashstream->current_sync_sample = -1;
    dashstream->target_time = GST_CLOCK_TIME_NONE;
  }

end:
  return ret;
}

#define SEEK_UPDATES_PLAY_POSITION(r, start_type, stop_type) \
  ((r >= 0 && start_type != GST_SEEK_TYPE_NONE) || \
   (r < 0 && stop_type != GST_SEEK_TYPE_NONE))

static gboolean
gst_dash_demux_seek (GstAdaptiveDemux * demux, GstEvent * seek)
{
  gdouble rate;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  GList *list;
  GstClockTime current_pos, target_pos;
  guint current_period;
  GstStreamPeriod *period;
  GList *iter, *streams = NULL;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  gboolean trickmode_no_audio;

  gst_event_parse_seek (seek, &rate, &format, &flags, &start_type, &start,
      &stop_type, &stop);

  if (!SEEK_UPDATES_PLAY_POSITION (rate, start_type, stop_type)) {
    /* nothing to do if we don't have to update the current position */
    return TRUE;
  }

  if (demux->segment.rate > 0.0) {
    target_pos = (GstClockTime) start;
  } else {
    target_pos = (GstClockTime) stop;
  }

  /* select the requested Period in the Media Presentation */
  if (!gst_mpd_client_setup_media_presentation (dashdemux->client, target_pos,
          -1, NULL))
    return FALSE;

  current_period = 0;
  for (list = g_list_first (dashdemux->client->periods); list;
      list = g_list_next (list)) {
    period = list->data;
    current_pos = period->start;
    current_period = period->number;
    GST_DEBUG_OBJECT (demux, "Looking at period %u) start:%"
        GST_TIME_FORMAT " - duration:%"
        GST_TIME_FORMAT ") for position %" GST_TIME_FORMAT,
        current_period, GST_TIME_ARGS (current_pos),
        GST_TIME_ARGS (period->duration), GST_TIME_ARGS (target_pos));
    if (current_pos <= target_pos
        && target_pos <= current_pos + period->duration) {
      break;
    }
  }
  if (list == NULL) {
    GST_WARNING_OBJECT (demux, "Could not find seeked Period");
    return FALSE;
  }

  trickmode_no_audio = !!(flags & GST_SEEK_FLAG_TRICKMODE_NO_AUDIO);

  streams = demux->streams;
  if (current_period != gst_mpd_client_get_period_index (dashdemux->client)) {
    GST_DEBUG_OBJECT (demux, "Seeking to Period %d", current_period);

    /* clean old active stream list, if any */
    gst_mpd_client_active_streams_free (dashdemux->client);
    dashdemux->trickmode_no_audio = trickmode_no_audio;

    /* setup video, audio and subtitle streams, starting from the new Period */
    if (!gst_mpd_client_set_period_index (dashdemux->client, current_period)
        || !gst_dash_demux_setup_all_streams (dashdemux))
      return FALSE;
    streams = demux->next_streams;
  } else if (dashdemux->trickmode_no_audio != trickmode_no_audio) {
    /* clean old active stream list, if any */
    gst_mpd_client_active_streams_free (dashdemux->client);
    dashdemux->trickmode_no_audio = trickmode_no_audio;

    /* setup video, audio and subtitle streams, starting from the new Period */
    if (!gst_dash_demux_setup_all_streams (dashdemux))
      return FALSE;
    streams = demux->next_streams;
  }

  /* Update the current sequence on all streams */
  for (iter = streams; iter; iter = g_list_next (iter)) {
    GstAdaptiveDemuxStream *stream = iter->data;
    GstDashDemuxStream *dashstream = iter->data;

    dashstream->average_skip_size = 0;
    if (gst_dash_demux_stream_seek (stream, rate >= 0, 0, target_pos,
            NULL) != GST_FLOW_OK)
      return FALSE;
  }

  return TRUE;
}

static gint64
gst_dash_demux_get_manifest_update_interval (GstAdaptiveDemux * demux)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  return MIN (dashdemux->client->mpd_root_node->minimumUpdatePeriod * 1000,
      SLOW_CLOCK_UPDATE_INTERVAL);
}

static GstFlowReturn
gst_dash_demux_update_manifest_data (GstAdaptiveDemux * demux,
    GstBuffer * buffer)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  GstMPDClient *new_client = NULL;
  GstMapInfo mapinfo;

  GST_DEBUG_OBJECT (demux, "Updating manifest file from URL");

  /* parse the manifest file */
  new_client = gst_mpd_client_new ();
  gst_mpd_client_set_uri_downloader (new_client, demux->downloader);
  new_client->mpd_uri = g_strdup (demux->manifest_uri);
  new_client->mpd_base_uri = g_strdup (demux->manifest_base_uri);
  gst_buffer_map (buffer, &mapinfo, GST_MAP_READ);

  if (gst_mpd_client_parse (new_client, (gchar *) mapinfo.data, mapinfo.size)) {
    const gchar *period_id;
    guint period_idx;
    GList *iter;
    GList *streams_iter;
    GList *streams;

    /* prepare the new manifest and try to transfer the stream position
     * status from the old manifest client  */

    GST_DEBUG_OBJECT (demux, "Updating manifest");

    period_id = gst_mpd_client_get_period_id (dashdemux->client);
    period_idx = gst_mpd_client_get_period_index (dashdemux->client);

    /* setup video, audio and subtitle streams, starting from current Period */
    if (!gst_mpd_client_setup_media_presentation (new_client, -1,
            (period_id ? -1 : period_idx), period_id)) {
      /* TODO */
    }

    if (period_id) {
      if (!gst_mpd_client_set_period_id (new_client, period_id)) {
        GST_DEBUG_OBJECT (demux, "Error setting up the updated manifest file");
        gst_mpd_client_free (new_client);
        gst_buffer_unmap (buffer, &mapinfo);
        return GST_FLOW_EOS;
      }
    } else {
      if (!gst_mpd_client_set_period_index (new_client, period_idx)) {
        GST_DEBUG_OBJECT (demux, "Error setting up the updated manifest file");
        gst_mpd_client_free (new_client);
        gst_buffer_unmap (buffer, &mapinfo);
        return GST_FLOW_EOS;
      }
    }

    if (!gst_dash_demux_setup_mpdparser_streams (dashdemux, new_client)) {
      GST_ERROR_OBJECT (demux, "Failed to setup streams on manifest " "update");
      gst_mpd_client_free (new_client);
      gst_buffer_unmap (buffer, &mapinfo);
      return GST_FLOW_ERROR;
    }

    /* If no pads have been exposed yet, need to use those */
    streams = NULL;
    if (demux->streams == NULL) {
      if (demux->prepared_streams) {
        streams = demux->prepared_streams;
      }
    } else {
      streams = demux->streams;
    }

    /* update the streams to preserve the current representation if there is one,
     * and to play from the next segment */
    for (iter = streams, streams_iter = new_client->active_streams;
        iter && streams_iter;
        iter = g_list_next (iter), streams_iter = g_list_next (streams_iter)) {
      GstDashDemuxStream *demux_stream = iter->data;
      GstActiveStream *new_stream = streams_iter->data;
      GstClockTime ts;

      if (!new_stream) {
        GST_DEBUG_OBJECT (demux,
            "Stream of index %d is missing from manifest update",
            demux_stream->index);
        gst_mpd_client_free (new_client);
        gst_buffer_unmap (buffer, &mapinfo);
        return GST_FLOW_EOS;
      }

      if (new_stream->cur_adapt_set
          && demux_stream->last_representation_id != NULL) {

        GList *rep_list = new_stream->cur_adapt_set->Representations;
        GstMPDRepresentationNode *rep_node =
            gst_mpd_client_get_representation_with_id (rep_list,
            demux_stream->last_representation_id);
        if (rep_node != NULL) {
          if (gst_mpd_client_setup_representation (new_client, new_stream,
                  rep_node)) {
            GST_DEBUG_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (demux_stream),
                "Found and set up matching representation %s in new manifest",
                demux_stream->last_representation_id);
          } else {
            GST_ERROR_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (demux_stream),
                "Failed to set up representation %s in new manifest",
                demux_stream->last_representation_id);
            gst_mpd_client_free (new_client);
            gst_buffer_unmap (buffer, &mapinfo);
            return GST_FLOW_EOS;
          }
        } else {
          /* If we failed to find the current representation,
           * then update_fragment_info() will reconfigure to the
           * new settings after the current download finishes */
          GST_WARNING_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (demux_stream),
              "Failed to find representation %s in new manifest",
              demux_stream->last_representation_id);
        }
      }

      if (gst_mpd_client_get_next_fragment_timestamp (dashdemux->client,
              demux_stream->index, &ts)
          || gst_mpd_client_get_last_fragment_timestamp_end (dashdemux->client,
              demux_stream->index, &ts)) {

        /* Due to rounding when doing the timescale conversions it might happen
         * that the ts falls back to a previous segment, leading the same data
         * to be downloaded twice. We try to work around this by always adding
         * 10 microseconds to get back to the correct segment. The errors are
         * usually on the order of nanoseconds so it should be enough.
         */

        /* _get_next_fragment_timestamp() returned relative timestamp to
         * corresponding period start, but _client_stream_seek expects absolute
         * MPD time. */
        ts += gst_mpd_client_get_period_start_time (dashdemux->client);

        GST_DEBUG_OBJECT (GST_ADAPTIVE_DEMUX_STREAM_PAD (demux_stream),
            "Current position: %" GST_TIME_FORMAT ", updating to %"
            GST_TIME_FORMAT, GST_TIME_ARGS (ts),
            GST_TIME_ARGS (ts + (10 * GST_USECOND)));
        ts += 10 * GST_USECOND;
        gst_mpd_client_stream_seek (new_client, new_stream,
            demux->segment.rate >= 0, 0, ts, NULL);
      }

      demux_stream->active_stream = new_stream;
    }

    gst_mpd_client_free (dashdemux->client);
    dashdemux->client = new_client;

    GST_DEBUG_OBJECT (demux, "Manifest file successfully updated");
    if (dashdemux->clock_drift) {
      gst_dash_demux_poll_clock_drift (dashdemux);
    }
  } else {
    /* In most cases, this will happen if we set a wrong url in the
     * source element and we have received the 404 HTML response instead of
     * the manifest */
    GST_WARNING_OBJECT (demux, "Error parsing the manifest.");
    gst_mpd_client_free (new_client);
    gst_buffer_unmap (buffer, &mapinfo);
    return GST_FLOW_ERROR;
  }

  gst_buffer_unmap (buffer, &mapinfo);

  return GST_FLOW_OK;
}

static gint64
gst_dash_demux_stream_get_fragment_waiting_time (GstAdaptiveDemuxStream *
    stream)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (stream->demux);
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;
  GstDateTime *segmentAvailability;
  GstActiveStream *active_stream = dashstream->active_stream;

  segmentAvailability =
      gst_mpd_client_get_next_segment_availability_start_time
      (dashdemux->client, active_stream);

  if (segmentAvailability) {
    gint64 diff;
    GstDateTime *cur_time;

    cur_time =
        gst_date_time_new_from_g_date_time
        (gst_adaptive_demux_get_client_now_utc (GST_ADAPTIVE_DEMUX_CAST
            (dashdemux)));
    diff =
        gst_mpd_client_calculate_time_difference (cur_time,
        segmentAvailability);
    gst_date_time_unref (segmentAvailability);
    gst_date_time_unref (cur_time);
    /* subtract the server's clock drift, so that if the server's
       time is behind our idea of UTC, we need to sleep for longer
       before requesting a fragment */
    return diff -
        gst_dash_demux_get_clock_compensation (dashdemux) * GST_USECOND;
  }
  return 0;
}

static gboolean
gst_dash_demux_has_next_period (GstAdaptiveDemux * demux)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);

  if (demux->segment.rate >= 0)
    return gst_mpd_client_has_next_period (dashdemux->client);
  else
    return gst_mpd_client_has_previous_period (dashdemux->client);
}

static void
gst_dash_demux_advance_period (GstAdaptiveDemux * demux)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);

  if (demux->segment.rate >= 0) {
    if (!gst_mpd_client_set_period_index (dashdemux->client,
            gst_mpd_client_get_period_index (dashdemux->client) + 1)) {
      /* TODO error */
      return;
    }
  } else {
    if (!gst_mpd_client_set_period_index (dashdemux->client,
            gst_mpd_client_get_period_index (dashdemux->client) - 1)) {
      /* TODO error */
      return;
    }
  }

  gst_dash_demux_setup_all_streams (dashdemux);
  gst_mpd_client_seek_to_first_segment (dashdemux->client);
}

static GstBuffer *
_gst_buffer_split (GstBuffer * buffer, gint offset, gsize size)
{
  GstBuffer *newbuf = gst_buffer_copy_region (buffer,
      GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_META
      | GST_BUFFER_COPY_MEMORY, offset, size == -1 ? size : size - offset);

  gst_buffer_resize (buffer, 0, offset);

  return newbuf;
}

static gboolean
gst_dash_demux_stream_fragment_start (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  GST_LOG_OBJECT (stream->pad, "Actual position %" GST_TIME_FORMAT,
      GST_TIME_ARGS (dashstream->actual_position));

  dashstream->current_index_header_or_data = 0;
  dashstream->current_offset = -1;

  /* We need to mark every first buffer of a key unit as discont,
   * and also every first buffer of a moov and moof. This ensures
   * that qtdemux takes note of our buffer offsets for each of those
   * buffers instead of keeping track of them itself from the first
   * buffer. We need offsets to be consistent between moof and mdat
   */
  if (dashstream->is_isobmff && dashdemux->allow_trickmode_key_units
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (demux)
      && dashstream->active_stream->mimeType == GST_STREAM_VIDEO)
    stream->discont = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_dash_demux_stream_fragment_finished (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  /* We need to mark every first buffer of a key unit as discont,
   * and also every first buffer of a moov and moof. This ensures
   * that qtdemux takes note of our buffer offsets for each of those
   * buffers instead of keeping track of them itself from the first
   * buffer. We need offsets to be consistent between moof and mdat
   */
  if (dashstream->is_isobmff && dashdemux->allow_trickmode_key_units
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (demux)
      && dashstream->active_stream->mimeType == GST_STREAM_VIDEO)
    stream->discont = TRUE;

  /* Only handle fragment advancing specifically for SIDX if we're not
   * in key unit mode */
  if (!(dashstream->moof_sync_samples
          && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (dashdemux))
      && gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client)
      && dashstream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
    /* fragment is advanced on data_received when byte limits are reached */
    if (dashstream->pending_seek_ts != GST_CLOCK_TIME_NONE) {
      if (SIDX (dashstream)->entry_index < SIDX (dashstream)->entries_count)
        return GST_FLOW_OK;
    } else if (gst_dash_demux_stream_has_next_subfragment (stream)) {
      return GST_FLOW_OK;
    }
  }

  if (G_UNLIKELY (stream->downloading_header || stream->downloading_index))
    return GST_FLOW_OK;

  return gst_adaptive_demux_stream_advance_fragment (demux, stream,
      stream->fragment.duration);
}

static gboolean
gst_dash_demux_need_another_chunk (GstAdaptiveDemuxStream * stream)
{
  GstDashDemux *dashdemux = (GstDashDemux *) stream->demux;
  GstDashDemuxStream *dashstream = (GstDashDemuxStream *) stream;

  /* We're chunked downloading for ISOBMFF in KEY_UNITS mode for the actual
   * fragment until we parsed the moof and arrived at the mdat. 8192 is a
   * random guess for the moof size
   */
  if (dashstream->is_isobmff
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux)
      && dashstream->active_stream->mimeType == GST_STREAM_VIDEO
      && !stream->downloading_header && !stream->downloading_index
      && dashdemux->allow_trickmode_key_units) {
    if (dashstream->isobmff_parser.current_fourcc != GST_ISOFF_FOURCC_MDAT) {
      /* Need to download the moof first to know anything */

      stream->fragment.chunk_size = 8192;
      /* Do we have the first fourcc already or are we in the middle */
      if (dashstream->isobmff_parser.current_fourcc == 0) {
        stream->fragment.chunk_size += dashstream->moof_average_size;
        if (dashstream->first_sync_sample_always_after_moof) {
          gboolean first = FALSE;
          /* Check if we'll really need that first sample */
          if (GST_CLOCK_TIME_IS_VALID (dashstream->target_time)) {
            first =
                ((dashstream->target_time -
                    dashstream->current_fragment_timestamp) /
                dashstream->keyframe_average_distance) == 0 ? TRUE : FALSE;
          } else if (stream->segment.rate > 0) {
            first = TRUE;
          }

          if (first)
            stream->fragment.chunk_size += dashstream->keyframe_average_size;
        }
      }

      if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client) &&
          dashstream->sidx_parser.sidx.entries) {
        guint64 sidx_start_offset =
            dashstream->sidx_base_offset +
            SIDX_CURRENT_ENTRY (dashstream)->offset;
        guint64 sidx_end_offset =
            sidx_start_offset + SIDX_CURRENT_ENTRY (dashstream)->size;
        guint64 downloaded_end_offset;

        if (dashstream->current_offset == GST_CLOCK_TIME_NONE) {
          downloaded_end_offset = sidx_start_offset;
        } else {
          downloaded_end_offset =
              dashstream->current_offset +
              gst_adapter_available (dashstream->adapter);
        }

        downloaded_end_offset = MAX (downloaded_end_offset, sidx_start_offset);

        if (stream->fragment.chunk_size +
            downloaded_end_offset > sidx_end_offset) {
          stream->fragment.chunk_size = sidx_end_offset - downloaded_end_offset;
        }
      }
    } else if (dashstream->moof && dashstream->moof_sync_samples) {
      /* Have the moof, either we're done now or we want to download the
       * directly following sync sample */
      if (dashstream->first_sync_sample_after_moof
          && dashstream->current_sync_sample == 0) {
        GstDashStreamSyncSample *sync_sample =
            &g_array_index (dashstream->moof_sync_samples,
            GstDashStreamSyncSample, 0);
        guint64 end_offset = sync_sample->end_offset + 1;
        guint64 downloaded_end_offset;

        downloaded_end_offset =
            dashstream->current_offset +
            gst_adapter_available (dashstream->adapter);

        if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client) &&
            dashstream->sidx_parser.sidx.entries) {
          guint64 sidx_end_offset =
              dashstream->sidx_base_offset +
              SIDX_CURRENT_ENTRY (dashstream)->offset +
              SIDX_CURRENT_ENTRY (dashstream)->size;

          if (end_offset > sidx_end_offset) {
            end_offset = sidx_end_offset;
          }
        }

        if (downloaded_end_offset < end_offset) {
          stream->fragment.chunk_size = end_offset - downloaded_end_offset;
        } else {
          stream->fragment.chunk_size = 0;
        }
      } else {
        stream->fragment.chunk_size = 0;
      }
    } else {
      /* Have moof but can't do key-units mode, just download until the end */
      stream->fragment.chunk_size = -1;
    }
  } else {
    /* We might've decided that we can't allow key-unit only
     * trickmodes while doing chunked downloading. In that case
     * just download from here to the end now */
    if (dashstream->moof
        && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux)) {
      stream->fragment.chunk_size = -1;
    } else {
      stream->fragment.chunk_size = 0;
    }
  }

  return stream->fragment.chunk_size != 0;
}

static GstFlowReturn
gst_dash_demux_parse_isobmff (GstAdaptiveDemux * demux,
    GstDashDemuxStream * dash_stream, gboolean * sidx_seek_needed)
{
  GstAdaptiveDemuxStream *stream = (GstAdaptiveDemuxStream *) dash_stream;
  GstDashDemux *dashdemux = GST_DASH_DEMUX_CAST (demux);
  gsize available;
  GstBuffer *buffer;
  GstMapInfo map;
  GstByteReader reader;
  guint32 fourcc;
  guint header_size;
  guint64 size, buffer_offset;

  *sidx_seek_needed = FALSE;

  /* This must not be called when we're in the mdat. We only look at the mdat
   * header and then stop parsing the boxes as we're only interested in the
   * metadata! Handling mdat is the job of the surrounding code, as well as
   * stopping or starting the next fragment when mdat is over (=> sidx)
   */
  g_assert (dash_stream->isobmff_parser.current_fourcc !=
      GST_ISOFF_FOURCC_MDAT);

  available = gst_adapter_available (dash_stream->adapter);
  buffer = gst_adapter_take_buffer (dash_stream->adapter, available);
  buffer_offset = dash_stream->current_offset;

  /* Always at the start of a box here */
  g_assert (dash_stream->isobmff_parser.current_size == 0);

  /* At the start of a box => Parse it */
  gst_buffer_map (buffer, &map, GST_MAP_READ);
  gst_byte_reader_init (&reader, map.data, map.size);

  /* While there are more boxes left to parse ... */
  dash_stream->isobmff_parser.current_start_offset = buffer_offset;
  do {
    dash_stream->isobmff_parser.current_fourcc = 0;
    dash_stream->isobmff_parser.current_size = 0;

    if (!gst_isoff_parse_box_header (&reader, &fourcc, NULL, &header_size,
            &size)) {
      break;
    }

    dash_stream->isobmff_parser.current_fourcc = fourcc;
    if (size == 0) {
      /* We assume this is mdat, anything else with "size until end"
       * does not seem to make sense */
      g_assert (dash_stream->isobmff_parser.current_fourcc ==
          GST_ISOFF_FOURCC_MDAT);
      dash_stream->isobmff_parser.current_size = -1;
      break;
    }

    dash_stream->isobmff_parser.current_size = size;

    /* Do we have the complete box or are at MDAT */
    if (gst_byte_reader_get_remaining (&reader) < size - header_size ||
        dash_stream->isobmff_parser.current_fourcc == GST_ISOFF_FOURCC_MDAT) {
      /* Reset byte reader to the beginning of the box */
      gst_byte_reader_set_pos (&reader,
          gst_byte_reader_get_pos (&reader) - header_size);
      break;
    }

    GST_LOG_OBJECT (stream->pad,
        "box %" GST_FOURCC_FORMAT " at offset %" G_GUINT64_FORMAT " size %"
        G_GUINT64_FORMAT, GST_FOURCC_ARGS (fourcc),
        dash_stream->isobmff_parser.current_start_offset, size);

    if (dash_stream->isobmff_parser.current_fourcc == GST_ISOFF_FOURCC_MOOF) {
      GstByteReader sub_reader;

      /* Only allow SIDX before the very first moof */
      dash_stream->allow_sidx = FALSE;

      g_assert (dash_stream->moof == NULL);
      g_assert (dash_stream->moof_sync_samples == NULL);
      gst_byte_reader_get_sub_reader (&reader, &sub_reader, size - header_size);
      dash_stream->moof = gst_isoff_moof_box_parse (&sub_reader);
      dash_stream->moof_offset =
          dash_stream->isobmff_parser.current_start_offset;
      dash_stream->moof_size = size;
      dash_stream->current_sync_sample = -1;

      if (dash_stream->moof_average_size) {
        if (dash_stream->moof_average_size < size)
          dash_stream->moof_average_size =
              (size * 3 + dash_stream->moof_average_size) / 4;
        else
          dash_stream->moof_average_size =
              (size + dash_stream->moof_average_size + 3) / 4;
      } else {
        dash_stream->moof_average_size = size;
      }
    } else if (dash_stream->isobmff_parser.current_fourcc ==
        GST_ISOFF_FOURCC_SIDX &&
        gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client) &&
        dash_stream->allow_sidx) {
      GstByteReader sub_reader;
      GstIsoffParserResult res;
      guint dummy;

      dash_stream->sidx_base_offset =
          dash_stream->isobmff_parser.current_start_offset + size;
      dash_stream->allow_sidx = FALSE;

      gst_byte_reader_get_sub_reader (&reader, &sub_reader, size - header_size);

      res =
          gst_isoff_sidx_parser_parse (&dash_stream->sidx_parser, &sub_reader,
          &dummy);

      if (res == GST_ISOFF_PARSER_DONE) {
        guint64 first_offset = dash_stream->sidx_parser.sidx.first_offset;
        GstSidxBox *sidx = SIDX (dash_stream);
        guint i;

        if (first_offset) {
          GST_LOG_OBJECT (stream->pad,
              "non-zero sidx first offset %" G_GUINT64_FORMAT, first_offset);
          dash_stream->sidx_base_offset += first_offset;
        }

        for (i = 0; i < sidx->entries_count; i++) {
          GstSidxBoxEntry *entry = &sidx->entries[i];

          if (entry->ref_type != 0) {
            GST_FIXME_OBJECT (stream->pad, "SIDX ref_type 1 not supported yet");
            dash_stream->sidx_position = GST_CLOCK_TIME_NONE;
            gst_isoff_sidx_parser_clear (&dash_stream->sidx_parser);
            break;
          }
        }

        /* We might've cleared the index above */
        if (sidx->entries_count > 0) {
          if (GST_CLOCK_TIME_IS_VALID (dash_stream->pending_seek_ts)) {
            /* FIXME, preserve seek flags */
            if (gst_dash_demux_stream_sidx_seek (dash_stream,
                    demux->segment.rate >= 0, 0, dash_stream->pending_seek_ts,
                    NULL) != GST_FLOW_OK) {
              GST_ERROR_OBJECT (stream->pad, "Couldn't find position in sidx");
              dash_stream->sidx_position = GST_CLOCK_TIME_NONE;
              gst_isoff_sidx_parser_clear (&dash_stream->sidx_parser);
            }
            dash_stream->pending_seek_ts = GST_CLOCK_TIME_NONE;
          } else {

            if (dash_stream->sidx_position == GST_CLOCK_TIME_NONE) {
              SIDX (dash_stream)->entry_index = 0;
            } else {
              if (gst_dash_demux_stream_sidx_seek (dash_stream,
                      demux->segment.rate >= 0, GST_SEEK_FLAG_SNAP_BEFORE,
                      dash_stream->sidx_position, NULL) != GST_FLOW_OK) {
                GST_ERROR_OBJECT (stream->pad,
                    "Couldn't find position in sidx");
                dash_stream->sidx_position = GST_CLOCK_TIME_NONE;
                gst_isoff_sidx_parser_clear (&dash_stream->sidx_parser);
              }
            }
            dash_stream->sidx_position =
                SIDX (dash_stream)->entries[SIDX (dash_stream)->entry_index].
                pts;
          }
        }

        if (dash_stream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED &&
            SIDX (dash_stream)->entry_index != 0) {
          /* Need to jump to the requested SIDX entry. Push everything up to
           * the SIDX box below and let the caller handle everything else */
          *sidx_seek_needed = TRUE;
          break;
        }
      }
    } else {
      gst_byte_reader_skip (&reader, size - header_size);
    }

    dash_stream->isobmff_parser.current_fourcc = 0;
    dash_stream->isobmff_parser.current_start_offset += size;
    dash_stream->isobmff_parser.current_size = 0;
  } while (gst_byte_reader_get_remaining (&reader) > 0);

  gst_buffer_unmap (buffer, &map);

  /* mdat? Push all we have and wait for it to be over */
  if (dash_stream->isobmff_parser.current_fourcc == GST_ISOFF_FOURCC_MDAT) {
    GstBuffer *pending;

    GST_LOG_OBJECT (stream->pad,
        "box %" GST_FOURCC_FORMAT " at offset %" G_GUINT64_FORMAT " size %"
        G_GUINT64_FORMAT, GST_FOURCC_ARGS (fourcc),
        dash_stream->isobmff_parser.current_start_offset,
        dash_stream->isobmff_parser.current_size);

    /* At mdat. Move the start of the mdat to the adapter and have everything
     * else be pushed. We parsed all header boxes at this point and are not
     * supposed to be called again until the next moof */
    pending = _gst_buffer_split (buffer, gst_byte_reader_get_pos (&reader), -1);
    gst_adapter_push (dash_stream->adapter, pending);
    dash_stream->current_offset += gst_byte_reader_get_pos (&reader);
    dash_stream->isobmff_parser.current_size = 0;

    GST_BUFFER_OFFSET (buffer) = buffer_offset;
    GST_BUFFER_OFFSET_END (buffer) =
        buffer_offset + gst_buffer_get_size (buffer);
    return gst_adaptive_demux_stream_push_buffer (stream, buffer);
  } else if (gst_byte_reader_get_pos (&reader) != 0) {
    GstBuffer *pending;

    /* Multiple complete boxes and no mdat? Push them and keep the remainder,
     * which is the start of the next box if any remainder */

    pending = _gst_buffer_split (buffer, gst_byte_reader_get_pos (&reader), -1);
    gst_adapter_push (dash_stream->adapter, pending);
    dash_stream->current_offset += gst_byte_reader_get_pos (&reader);
    dash_stream->isobmff_parser.current_size = 0;

    GST_BUFFER_OFFSET (buffer) = buffer_offset;
    GST_BUFFER_OFFSET_END (buffer) =
        buffer_offset + gst_buffer_get_size (buffer);
    return gst_adaptive_demux_stream_push_buffer (stream, buffer);
  }

  /* Not even a single complete, non-mdat box, wait */
  dash_stream->isobmff_parser.current_size = 0;
  gst_adapter_push (dash_stream->adapter, buffer);

  return GST_FLOW_OK;
}

static gboolean
gst_dash_demux_find_sync_samples (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstDashDemux *dashdemux = (GstDashDemux *) stream->demux;
  GstDashDemuxStream *dash_stream = (GstDashDemuxStream *) stream;
  guint i;
  guint32 track_id = 0;
  guint64 prev_traf_end;
  gboolean trex_sample_flags = FALSE;

  if (!dash_stream->moof) {
    dashdemux->allow_trickmode_key_units = FALSE;
    return FALSE;
  }

  dash_stream->current_sync_sample = -1;
  dash_stream->moof_sync_samples =
      g_array_new (FALSE, FALSE, sizeof (GstDashStreamSyncSample));

  prev_traf_end = dash_stream->moof_offset;

  /* generate table of keyframes and offsets */
  for (i = 0; i < dash_stream->moof->traf->len; i++) {
    GstTrafBox *traf = &g_array_index (dash_stream->moof->traf, GstTrafBox, i);
    guint64 traf_offset = 0, prev_trun_end;
    guint j;

    if (i == 0) {
      track_id = traf->tfhd.track_id;
    } else if (track_id != traf->tfhd.track_id) {
      GST_ERROR_OBJECT (stream->pad,
          "moof with trafs of different track ids (%u != %u)", track_id,
          traf->tfhd.track_id);
      g_array_free (dash_stream->moof_sync_samples, TRUE);
      dash_stream->moof_sync_samples = NULL;
      dashdemux->allow_trickmode_key_units = FALSE;
      return FALSE;
    }

    if (traf->tfhd.flags & GST_TFHD_FLAGS_BASE_DATA_OFFSET_PRESENT) {
      traf_offset = traf->tfhd.base_data_offset;
    } else if (traf->tfhd.flags & GST_TFHD_FLAGS_DEFAULT_BASE_IS_MOOF) {
      traf_offset = dash_stream->moof_offset;
    } else {
      traf_offset = prev_traf_end;
    }

    prev_trun_end = traf_offset;

    for (j = 0; j < traf->trun->len; j++) {
      GstTrunBox *trun = &g_array_index (traf->trun, GstTrunBox, j);
      guint64 trun_offset, prev_sample_end;
      guint k;

      if (trun->flags & GST_TRUN_FLAGS_DATA_OFFSET_PRESENT) {
        trun_offset = traf_offset + trun->data_offset;
      } else {
        trun_offset = prev_trun_end;
      }

      prev_sample_end = trun_offset;
      for (k = 0; k < trun->samples->len; k++) {
        GstTrunSample *sample =
            &g_array_index (trun->samples, GstTrunSample, k);
        guint64 sample_offset;
        guint32 sample_flags;
#if 0
        guint32 sample_duration;
#endif

        sample_offset = prev_sample_end;

        if (trun->flags & GST_TRUN_FLAGS_SAMPLE_FLAGS_PRESENT) {
          sample_flags = sample->sample_flags;
        } else if ((trun->flags & GST_TRUN_FLAGS_FIRST_SAMPLE_FLAGS_PRESENT)
            && k == 0) {
          sample_flags = trun->first_sample_flags;
        } else if (traf->tfhd.
            flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_FLAGS_PRESENT) {
          sample_flags = traf->tfhd.default_sample_flags;
        } else {
          trex_sample_flags = TRUE;
          continue;
        }

#if 0
        if (trun->flags & GST_TRUN_FLAGS_SAMPLE_DURATION_PRESENT) {
          sample_duration = sample->sample_duration;
        } else if (traf->tfhd.
            flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_DURATION_PRESENT) {
          sample_duration = traf->tfhd.default_sample_duration;
        } else {
          GST_FIXME_OBJECT (stream->pad,
              "Sample duration given by trex - can't download only keyframes");
          g_array_free (dash_stream->moof_sync_samples, TRUE);
          dash_stream->moof_sync_samples = NULL;
          return FALSE;
        }
#endif

        if (trun->flags & GST_TRUN_FLAGS_SAMPLE_SIZE_PRESENT) {
          prev_sample_end += sample->sample_size;
        } else if (traf->tfhd.
            flags & GST_TFHD_FLAGS_DEFAULT_SAMPLE_SIZE_PRESENT) {
          prev_sample_end += traf->tfhd.default_sample_size;
        } else {
          GST_FIXME_OBJECT (stream->pad,
              "Sample size given by trex - can't download only keyframes");
          g_array_free (dash_stream->moof_sync_samples, TRUE);
          dash_stream->moof_sync_samples = NULL;
          dashdemux->allow_trickmode_key_units = FALSE;
          return FALSE;
        }

        /* Non-non-sync sample aka sync sample */
        if (!GST_ISOFF_SAMPLE_FLAGS_SAMPLE_IS_NON_SYNC_SAMPLE (sample_flags) ||
            GST_ISOFF_SAMPLE_FLAGS_SAMPLE_DEPENDS_ON (sample_flags) == 2) {
          GstDashStreamSyncSample sync_sample =
              { sample_offset, prev_sample_end - 1 };
          /* TODO: need timestamps so we can decide to download or not */
          g_array_append_val (dash_stream->moof_sync_samples, sync_sample);
        }
      }

      prev_trun_end = prev_sample_end;
    }

    prev_traf_end = prev_trun_end;
  }

  if (trex_sample_flags) {
    if (dash_stream->moof_sync_samples->len > 0) {
      GST_LOG_OBJECT (stream->pad,
          "Some sample flags given by trex but still found sync samples");
    } else {
      GST_FIXME_OBJECT (stream->pad,
          "Sample flags given by trex - can't download only keyframes");
      g_array_free (dash_stream->moof_sync_samples, TRUE);
      dash_stream->moof_sync_samples = NULL;
      dashdemux->allow_trickmode_key_units = FALSE;
      return FALSE;
    }
  }

  if (dash_stream->moof_sync_samples->len == 0) {
    GST_LOG_OBJECT (stream->pad, "No sync samples found in fragment");
    g_array_free (dash_stream->moof_sync_samples, TRUE);
    dash_stream->moof_sync_samples = NULL;
    dashdemux->allow_trickmode_key_units = FALSE;
    return FALSE;
  }

  {
    GstDashStreamSyncSample *sync_sample;
    guint i;
    guint size;
    GstClockTime current_keyframe_distance;

    for (i = 0; i < dash_stream->moof_sync_samples->len; i++) {
      sync_sample =
          &g_array_index (dash_stream->moof_sync_samples,
          GstDashStreamSyncSample, i);
      size = sync_sample->end_offset + 1 - sync_sample->start_offset;

      if (dash_stream->keyframe_average_size) {
        /* Over-estimate the keyframe size */
        if (dash_stream->keyframe_average_size < size)
          dash_stream->keyframe_average_size =
              (size * 3 + dash_stream->keyframe_average_size) / 4;
        else
          dash_stream->keyframe_average_size =
              (size + dash_stream->keyframe_average_size * 3) / 4;
      } else {
        dash_stream->keyframe_average_size = size;
      }

      if (i == 0) {
        if (dash_stream->moof_offset + dash_stream->moof_size + 8 <
            sync_sample->start_offset) {
          dash_stream->first_sync_sample_after_moof = FALSE;
          dash_stream->first_sync_sample_always_after_moof = FALSE;
        } else {
          dash_stream->first_sync_sample_after_moof =
              (dash_stream->moof_sync_samples->len == 1
              || demux->segment.rate > 0.0);
        }
      }
    }

    g_assert (stream->fragment.duration != 0);
    g_assert (stream->fragment.duration != GST_CLOCK_TIME_NONE);

    if (gst_mpd_client_has_isoff_ondemand_profile (dashdemux->client)
        && dash_stream->sidx_position != GST_CLOCK_TIME_NONE
        && SIDX (dash_stream)->entries) {
      GstSidxBoxEntry *entry = SIDX_CURRENT_ENTRY (dash_stream);
      current_keyframe_distance =
          entry->duration / dash_stream->moof_sync_samples->len;
    } else {
      current_keyframe_distance =
          stream->fragment.duration / dash_stream->moof_sync_samples->len;
    }
    dash_stream->current_fragment_keyframe_distance = current_keyframe_distance;

    if (dash_stream->keyframe_average_distance) {
      /* Under-estimate the keyframe distance */
      if (dash_stream->keyframe_average_distance > current_keyframe_distance)
        dash_stream->keyframe_average_distance =
            (dash_stream->keyframe_average_distance * 3 +
            current_keyframe_distance) / 4;
      else
        dash_stream->keyframe_average_distance =
            (dash_stream->keyframe_average_distance +
            current_keyframe_distance * 3) / 4;
    } else {
      dash_stream->keyframe_average_distance = current_keyframe_distance;
    }

    GST_DEBUG_OBJECT (stream->pad,
        "average keyframe sample size: %" G_GUINT64_FORMAT,
        dash_stream->keyframe_average_size);
    GST_DEBUG_OBJECT (stream->pad,
        "average keyframe distance: %" GST_TIME_FORMAT " (%" GST_TIME_FORMAT
        ")", GST_TIME_ARGS (dash_stream->keyframe_average_distance),
        GST_TIME_ARGS (current_keyframe_distance));
    GST_DEBUG_OBJECT (stream->pad, "first sync sample after moof: %d",
        dash_stream->first_sync_sample_after_moof);
  }

  return TRUE;
}


static GstFlowReturn
gst_dash_demux_handle_isobmff (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dash_stream = (GstDashDemuxStream *) stream;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buffer;
  gboolean sidx_advance = FALSE;

  /* We parse all ISOBMFF boxes of a (sub)fragment until the mdat. This covers
   * at least moov, moof and sidx boxes. Once mdat is received we just output
   * everything until the next (sub)fragment */
  if (dash_stream->isobmff_parser.current_fourcc != GST_ISOFF_FOURCC_MDAT) {
    gboolean sidx_seek_needed = FALSE;

    ret = gst_dash_demux_parse_isobmff (demux, dash_stream, &sidx_seek_needed);
    if (ret != GST_FLOW_OK)
      return ret;

    /* Go to selected segment if needed here */
    if (sidx_seek_needed && !stream->downloading_index)
      return GST_ADAPTIVE_DEMUX_FLOW_END_OF_FRAGMENT;

    /* No mdat yet, let's get called again with the next boxes */
    if (dash_stream->isobmff_parser.current_fourcc != GST_ISOFF_FOURCC_MDAT)
      return ret;

    /* Here we end up only if we're right at the mdat start */

    /* Jump to the next sync sample. As we're doing chunked downloading
     * here, just drop data until our chunk is over so we can reuse the
     * HTTP connection instead of having to create a new one or
     * reuse the data if the sync sample follows the moof */
    if (dash_stream->active_stream->mimeType == GST_STREAM_VIDEO
        && gst_dash_demux_find_sync_samples (demux, stream) &&
        GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux)) {
      guint idx = -1;

      if (GST_CLOCK_TIME_IS_VALID (dash_stream->target_time)) {
        idx =
            (dash_stream->target_time -
            dash_stream->current_fragment_timestamp) /
            dash_stream->current_fragment_keyframe_distance;
      } else if (stream->segment.rate > 0) {
        idx = 0;
      }

      GST_DEBUG_OBJECT (stream->pad, "target %" GST_TIME_FORMAT " idx %d",
          GST_TIME_ARGS (dash_stream->target_time), idx);
      /* Figure out target time */

      if (dash_stream->first_sync_sample_after_moof && idx == 0) {
        /* If we're here, don't throw away data but collect sync
         * sample while we're at it below. We're doing chunked
         * downloading so might need to adjust the next chunk size for
         * the remainder */
        dash_stream->current_sync_sample = 0;
        GST_DEBUG_OBJECT (stream->pad, "Using first keyframe after header");
      }
    }

    if (gst_adapter_available (dash_stream->adapter) == 0)
      return ret;

    /* We have some data from the mdat available in the adapter, handle it
     * below in the push code */
  } else {
    /* Somewhere in the middle of the mdat */
  }

  /* At mdat */
  if (dash_stream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
    guint64 sidx_end_offset =
        dash_stream->sidx_base_offset +
        SIDX_CURRENT_ENTRY (dash_stream)->offset +
        SIDX_CURRENT_ENTRY (dash_stream)->size;
    gboolean has_next = gst_dash_demux_stream_has_next_subfragment (stream);
    gsize available;

    /* Need to handle everything in the adapter according to the parsed SIDX
     * and advance subsegments accordingly */

    available = gst_adapter_available (dash_stream->adapter);
    if (dash_stream->current_offset + available < sidx_end_offset) {
      buffer = gst_adapter_take_buffer (dash_stream->adapter, available);
    } else {
      if (!has_next && sidx_end_offset <= dash_stream->current_offset) {
        /* Drain all bytes, since there might be trailing bytes at the end of subfragment */
        buffer = gst_adapter_take_buffer (dash_stream->adapter, available);
      } else {
        if (sidx_end_offset <= dash_stream->current_offset) {
          /* This means a corrupted stream or a bug: ignoring bugs, it
           * should only happen if the SIDX index is corrupt */
          GST_ERROR_OBJECT (stream->pad, "Invalid SIDX state");
          gst_adapter_clear (dash_stream->adapter);
          return GST_FLOW_ERROR;
        } else {
          buffer =
              gst_adapter_take_buffer (dash_stream->adapter,
              sidx_end_offset - dash_stream->current_offset);
          sidx_advance = TRUE;
        }
      }
    }
  } else {
    /* Take it all and handle it further below */
    buffer =
        gst_adapter_take_buffer (dash_stream->adapter,
        gst_adapter_available (dash_stream->adapter));

    /* Attention: All code paths below need to update dash_stream->current_offset */
  }

  /* We're actually running in key-units trick mode */
  if (dash_stream->active_stream->mimeType == GST_STREAM_VIDEO
      && dash_stream->moof_sync_samples
      && GST_ADAPTIVE_DEMUX_IN_TRICKMODE_KEY_UNITS (stream->demux)) {
    if (dash_stream->current_sync_sample == -1) {
      /* We're doing chunked downloading and wait for finishing the current
       * chunk so we can jump to the first keyframe */
      dash_stream->current_offset += gst_buffer_get_size (buffer);
      gst_buffer_unref (buffer);
      return GST_FLOW_OK;
    } else {
      GstDashStreamSyncSample *sync_sample =
          &g_array_index (dash_stream->moof_sync_samples,
          GstDashStreamSyncSample, dash_stream->current_sync_sample);
      guint64 end_offset =
          dash_stream->current_offset + gst_buffer_get_size (buffer);

      /* Make sure to not download too much, this should only happen for
       * the very first keyframe if it follows the moof */
      if (dash_stream->current_offset >= sync_sample->end_offset + 1) {
        dash_stream->current_offset += gst_buffer_get_size (buffer);
        gst_buffer_unref (buffer);
        return GST_FLOW_OK;
      } else if (end_offset > sync_sample->end_offset + 1) {
        guint64 remaining =
            sync_sample->end_offset + 1 - dash_stream->current_offset;
        GstBuffer *sub = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, 0,
            remaining);
        gst_buffer_unref (buffer);
        buffer = sub;
      }
    }
  }

  GST_BUFFER_OFFSET (buffer) = dash_stream->current_offset;
  dash_stream->current_offset += gst_buffer_get_size (buffer);
  GST_BUFFER_OFFSET_END (buffer) = dash_stream->current_offset;

  ret = gst_adaptive_demux_stream_push_buffer (stream, buffer);
  if (ret != GST_FLOW_OK)
    return ret;

  if (sidx_advance) {
    ret =
        gst_adaptive_demux_stream_advance_fragment (demux, stream,
        SIDX_CURRENT_ENTRY (dash_stream)->duration);
    if (ret != GST_FLOW_OK)
      return ret;

    /* If we still have data available, recurse and use it up if possible */
    if (gst_adapter_available (dash_stream->adapter) > 0)
      return gst_dash_demux_handle_isobmff (demux, stream);
  }

  return ret;
}

static GstFlowReturn
gst_dash_demux_data_received (GstAdaptiveDemux * demux,
    GstAdaptiveDemuxStream * stream, GstBuffer * buffer)
{
  GstDashDemuxStream *dash_stream = (GstDashDemuxStream *) stream;
  GstFlowReturn ret = GST_FLOW_OK;
  guint index_header_or_data;

  if (stream->downloading_index)
    index_header_or_data = 1;
  else if (stream->downloading_header)
    index_header_or_data = 2;
  else
    index_header_or_data = 3;

  if (dash_stream->current_index_header_or_data != index_header_or_data) {
    /* Clear pending data */
    if (gst_adapter_available (dash_stream->adapter) != 0)
      GST_ERROR_OBJECT (stream->pad,
          "Had pending SIDX data after switch between index/header/data");
    gst_adapter_clear (dash_stream->adapter);
    dash_stream->current_index_header_or_data = index_header_or_data;
    dash_stream->current_offset = -1;
  }

  if (dash_stream->current_offset == -1)
    dash_stream->current_offset =
        GST_BUFFER_OFFSET_IS_VALID (buffer) ? GST_BUFFER_OFFSET (buffer) : 0;

  gst_adapter_push (dash_stream->adapter, buffer);
  buffer = NULL;

  if (dash_stream->is_isobmff || stream->downloading_index) {
    /* SIDX index is also ISOBMMF */
    ret = gst_dash_demux_handle_isobmff (demux, stream);
  } else if (dash_stream->sidx_parser.status == GST_ISOFF_SIDX_PARSER_FINISHED) {
    gsize available;

    /* Not ISOBMFF but had a SIDX index. Does this even exist or work? */
    while (ret == GST_FLOW_OK
        && ((available = gst_adapter_available (dash_stream->adapter)) > 0)) {
      gboolean advance = FALSE;
      guint64 sidx_end_offset =
          dash_stream->sidx_base_offset +
          SIDX_CURRENT_ENTRY (dash_stream)->offset +
          SIDX_CURRENT_ENTRY (dash_stream)->size;
      gboolean has_next = gst_dash_demux_stream_has_next_subfragment (stream);

      if (dash_stream->current_offset + available < sidx_end_offset) {
        buffer = gst_adapter_take_buffer (dash_stream->adapter, available);
      } else {
        if (!has_next && sidx_end_offset <= dash_stream->current_offset) {
          /* Drain all bytes, since there might be trailing bytes at the end of subfragment */
          buffer = gst_adapter_take_buffer (dash_stream->adapter, available);
        } else {
          if (sidx_end_offset <= dash_stream->current_offset) {
            /* This means a corrupted stream or a bug: ignoring bugs, it
             * should only happen if the SIDX index is corrupt */
            GST_ERROR_OBJECT (stream->pad, "Invalid SIDX state");
            gst_adapter_clear (dash_stream->adapter);
            ret = GST_FLOW_ERROR;
            break;
          } else {
            buffer =
                gst_adapter_take_buffer (dash_stream->adapter,
                sidx_end_offset - dash_stream->current_offset);
            advance = TRUE;
          }
        }
      }

      GST_BUFFER_OFFSET (buffer) = dash_stream->current_offset;
      GST_BUFFER_OFFSET_END (buffer) =
          GST_BUFFER_OFFSET (buffer) + gst_buffer_get_size (buffer);
      dash_stream->current_offset = GST_BUFFER_OFFSET_END (buffer);

      ret = gst_adaptive_demux_stream_push_buffer (stream, buffer);

      if (advance) {
        if (has_next) {
          GstFlowReturn new_ret;
          new_ret =
              gst_adaptive_demux_stream_advance_fragment (demux, stream,
              SIDX_CURRENT_ENTRY (dash_stream)->duration);

          /* only overwrite if it was OK before */
          if (ret == GST_FLOW_OK)
            ret = new_ret;
        } else {
          break;
        }
      }
    }
  } else {
    /* this should be the main header, just push it all */
    buffer = gst_adapter_take_buffer (dash_stream->adapter,
        gst_adapter_available (dash_stream->adapter));

    GST_BUFFER_OFFSET (buffer) = dash_stream->current_offset;
    GST_BUFFER_OFFSET_END (buffer) =
        GST_BUFFER_OFFSET (buffer) + gst_buffer_get_size (buffer);
    dash_stream->current_offset = GST_BUFFER_OFFSET_END (buffer);

    ret = gst_adaptive_demux_stream_push_buffer (stream, buffer);
  }

  return ret;
}

static void
gst_dash_demux_stream_free (GstAdaptiveDemuxStream * stream)
{
  GstDashDemuxStream *dash_stream = (GstDashDemuxStream *) stream;

  gst_isoff_sidx_parser_clear (&dash_stream->sidx_parser);
  if (dash_stream->adapter)
    g_object_unref (dash_stream->adapter);
  if (dash_stream->moof)
    gst_isoff_moof_box_free (dash_stream->moof);
  if (dash_stream->moof_sync_samples)
    g_array_free (dash_stream->moof_sync_samples, TRUE);
  g_free (dash_stream->last_representation_id);
}

static GstDashDemuxClockDrift *
gst_dash_demux_clock_drift_new (GstDashDemux * demux)
{
  GstDashDemuxClockDrift *clock_drift;

  clock_drift = g_slice_new0 (GstDashDemuxClockDrift);
  g_mutex_init (&clock_drift->clock_lock);
  clock_drift->next_update =
      GST_TIME_AS_USECONDS (gst_adaptive_demux_get_monotonic_time
      (GST_ADAPTIVE_DEMUX_CAST (demux)));
  return clock_drift;
}

static void
gst_dash_demux_clock_drift_free (GstDashDemuxClockDrift * clock_drift)
{
  if (clock_drift) {
    g_mutex_lock (&clock_drift->clock_lock);
    if (clock_drift->ntp_clock)
      g_object_unref (clock_drift->ntp_clock);
    g_mutex_unlock (&clock_drift->clock_lock);
    g_mutex_clear (&clock_drift->clock_lock);
    g_slice_free (GstDashDemuxClockDrift, clock_drift);
  }
}

/*
 * The value attribute of the UTCTiming element contains a white-space
 * separated list of servers that are recommended to be used in
 * combination with the NTP protocol as defined in IETF RFC 5905 for
 * getting the appropriate time.
 *
 * The DASH standard does not specify which version of NTP. This
 * function only works with NTPv4 servers.
*/
static GstDateTime *
gst_dash_demux_poll_ntp_server (GstDashDemuxClockDrift * clock_drift,
    gchar ** urls)
{
  GstClockTime ntp_clock_time;
  GDateTime *dt, *dt2;

  if (!clock_drift->ntp_clock) {
    GResolver *resolver;
    GList *inet_addrs;
    GError *err = NULL;
    gchar *ip_addr;

    resolver = g_resolver_get_default ();
    /* We don't round-robin NTP servers. If the manifest specifies multiple
       NTP time servers, select one at random */
    clock_drift->selected_url = g_random_int_range (0, g_strv_length (urls));
    GST_DEBUG ("Connecting to NTP time server %s",
        urls[clock_drift->selected_url]);
    inet_addrs = g_resolver_lookup_by_name (resolver,
        urls[clock_drift->selected_url], NULL, &err);
    g_object_unref (resolver);
    if (!inet_addrs || g_list_length (inet_addrs) == 0) {
      GST_ERROR ("Failed to resolve hostname of NTP server: %s",
          err ? (err->message) : "unknown error");
      if (inet_addrs)
        g_resolver_free_addresses (inet_addrs);
      if (err)
        g_error_free (err);
      return NULL;
    }
    ip_addr =
        g_inet_address_to_string ((GInetAddress
            *) (g_list_first (inet_addrs)->data));
    clock_drift->ntp_clock = gst_ntp_clock_new ("dashntp", ip_addr, 123, 0);
    g_free (ip_addr);
    g_resolver_free_addresses (inet_addrs);
    if (!clock_drift->ntp_clock) {
      GST_ERROR ("Failed to create NTP clock");
      return NULL;
    }
    if (!gst_clock_wait_for_sync (clock_drift->ntp_clock, 5 * GST_SECOND)) {
      g_object_unref (clock_drift->ntp_clock);
      clock_drift->ntp_clock = NULL;
      GST_ERROR ("Failed to lock to NTP clock");
      return NULL;
    }
  }
  ntp_clock_time = gst_clock_get_time (clock_drift->ntp_clock);
  if (ntp_clock_time == GST_CLOCK_TIME_NONE) {
    GST_ERROR ("Failed to get time from NTP clock");
    return NULL;
  }
  ntp_clock_time -= NTP_TO_UNIX_EPOCH * GST_SECOND;
  dt = g_date_time_new_from_unix_utc (ntp_clock_time / GST_SECOND);
  if (!dt) {
    GST_ERROR ("Failed to create GstDateTime");
    return NULL;
  }
  ntp_clock_time =
      gst_util_uint64_scale (ntp_clock_time % GST_SECOND, 1000000, GST_SECOND);
  dt2 = g_date_time_add (dt, ntp_clock_time);
  g_date_time_unref (dt);
  return gst_date_time_new_from_g_date_time (dt2);
}

struct Rfc5322TimeZone
{
  const gchar *name;
  gfloat tzoffset;
};

/*
 Parse an RFC5322 (section 3.3) date-time from the Date: field in the
 HTTP response.
 See https://tools.ietf.org/html/rfc5322#section-3.3
*/
static GstDateTime *
gst_dash_demux_parse_http_head (GstDashDemuxClockDrift * clock_drift,
    GstFragment * download)
{
  static const gchar *months[] = { NULL, "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec", NULL
  };
  static const struct Rfc5322TimeZone timezones[] = {
    {"Z", 0},
    {"UT", 0},
    {"GMT", 0},
    {"BST", 1},
    {"EST", -5},
    {"EDT", -4},
    {"CST", -6},
    {"CDT", -5},
    {"MST", -7},
    {"MDT", -6},
    {"PST", -8},
    {"PDT", -7},
    {NULL, 0}
  };
  GstDateTime *value = NULL;
  const GstStructure *response_headers;
  const gchar *http_date;
  const GValue *val;
  gint ret;
  const gchar *pos;
  gint year = -1, month = -1, day = -1, hour = -1, minute = -1, second = -1;
  gchar zone[6];
  gchar monthstr[4];
  gfloat tzoffset = 0;
  gboolean parsed_tz = FALSE;

  g_return_val_if_fail (download != NULL, NULL);
  g_return_val_if_fail (download->headers != NULL, NULL);

  val = gst_structure_get_value (download->headers, "response-headers");
  if (!val) {
    return NULL;
  }
  response_headers = gst_value_get_structure (val);
  http_date = gst_structure_get_string (response_headers, "Date");
  if (!http_date) {
    return NULL;
  }

  /* skip optional text version of day of the week */
  pos = strchr (http_date, ',');
  if (pos)
    pos++;
  else
    pos = http_date;
  ret =
      sscanf (pos, "%02d %3s %04d %02d:%02d:%02d %5s", &day, monthstr, &year,
      &hour, &minute, &second, zone);
  if (ret == 7) {
    gchar *z = zone;
    gint i;

    for (i = 1; months[i]; ++i) {
      if (g_ascii_strncasecmp (months[i], monthstr, strlen (months[i])) == 0) {
        month = i;
        break;
      }
    }
    for (i = 0; timezones[i].name && !parsed_tz; ++i) {
      if (g_ascii_strncasecmp (timezones[i].name, z,
              strlen (timezones[i].name)) == 0) {
        tzoffset = timezones[i].tzoffset;
        parsed_tz = TRUE;
      }
    }
    if (!parsed_tz) {
      gint hh, mm;
      gboolean neg = FALSE;
      /* check if it is in the form +-HHMM */
      if (*z == '+' || *z == '-') {
        if (*z == '+')
          ++z;
        else if (*z == '-') {
          ++z;
          neg = TRUE;
        }
        ret = sscanf (z, "%02d%02d", &hh, &mm);
        if (ret == 2) {
          tzoffset = hh;
          tzoffset += mm / 60.0;
          if (neg)
            tzoffset = -tzoffset;
          parsed_tz = TRUE;
        }
      }
    }
    /* Accept year in both 2 digit or 4 digit format */
    if (year < 100)
      year += 2000;
  }
  if (month > 0 && parsed_tz) {
    value = gst_date_time_new (tzoffset,
        year, month, day, hour, minute, second);
  }
  return value;
}

/*
   The timing information is contained in the message body of the HTTP
   response and contains a time value formatted according to NTP timestamp
   format in IETF RFC 5905.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                            Seconds                            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                            Fraction                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

                             NTP Timestamp Format
*/
static GstDateTime *
gst_dash_demux_parse_http_ntp (GstDashDemuxClockDrift * clock_drift,
    GstBuffer * buffer)
{
  gint64 seconds;
  guint64 fraction;
  GDateTime *dt, *dt2;
  GstMapInfo mapinfo;

  /* See https://tools.ietf.org/html/rfc5905#page-12 for details of
     the NTP Timestamp Format */
  gst_buffer_map (buffer, &mapinfo, GST_MAP_READ);
  if (mapinfo.size != 8) {
    gst_buffer_unmap (buffer, &mapinfo);
    return NULL;
  }
  seconds = GST_READ_UINT32_BE (mapinfo.data);
  fraction = GST_READ_UINT32_BE (mapinfo.data + 4);
  gst_buffer_unmap (buffer, &mapinfo);
  fraction = gst_util_uint64_scale (fraction, 1000000,
      G_GUINT64_CONSTANT (1) << 32);
  /* subtract constant to convert from 1900 based time to 1970 based time */
  seconds -= NTP_TO_UNIX_EPOCH;
  dt = g_date_time_new_from_unix_utc (seconds);
  dt2 = g_date_time_add (dt, fraction);
  g_date_time_unref (dt);
  return gst_date_time_new_from_g_date_time (dt2);
}

/*
  The timing information is contained in the message body of the
  HTTP response and contains a time value formatted according to
  xs:dateTime as defined in W3C XML Schema Part 2: Datatypes specification.
*/
static GstDateTime *
gst_dash_demux_parse_http_xsdate (GstDashDemuxClockDrift * clock_drift,
    GstBuffer * buffer)
{
  GstDateTime *value = NULL;
  GstMapInfo mapinfo;

  /* the string from the server might not be zero terminated */
  if (gst_buffer_map (buffer, &mapinfo, GST_MAP_READ)) {
    gchar *str;
    str = g_strndup ((const gchar *) mapinfo.data, mapinfo.size);
    gst_buffer_unmap (buffer, &mapinfo);
    value = gst_date_time_new_from_iso8601_string (str);
    g_free (str);
  }
  return value;
}

static gboolean
gst_dash_demux_poll_clock_drift (GstDashDemux * demux)
{
  GstDashDemuxClockDrift *clock_drift;
  GDateTime *start = NULL, *end;
  GstBuffer *buffer = NULL;
  GstDateTime *value = NULL;
  gboolean ret = FALSE;
  gint64 now;
  GstMPDUTCTimingType method;
  gchar **urls;

  g_return_val_if_fail (demux != NULL, FALSE);
  g_return_val_if_fail (demux->clock_drift != NULL, FALSE);
  clock_drift = demux->clock_drift;
  now =
      GST_TIME_AS_USECONDS (gst_adaptive_demux_get_monotonic_time
      (GST_ADAPTIVE_DEMUX_CAST (demux)));
  if (now < clock_drift->next_update) {
    /*TODO: If a fragment fails to download in adaptivedemux, it waits
       for a manifest reload before another attempt to fetch a fragment.
       Section 10.8.6 of the DVB-DASH standard states that the DASH client
       shall refresh the manifest and resynchronise to one of the time sources.

       Currently the fact that the manifest refresh follows a download failure
       does not make it into dashdemux. */
    return TRUE;
  }
  urls = gst_mpd_client_get_utc_timing_sources (demux->client,
      SUPPORTED_CLOCK_FORMATS, &method);
  if (!urls) {
    return FALSE;
  }
  /* Update selected_url just in case the number of URLs in the UTCTiming
     element has shrunk since the last poll */
  clock_drift->selected_url = clock_drift->selected_url % g_strv_length (urls);
  g_mutex_lock (&clock_drift->clock_lock);

  if (method == GST_MPD_UTCTIMING_TYPE_NTP) {
    value = gst_dash_demux_poll_ntp_server (clock_drift, urls);
    if (!value) {
      GST_ERROR_OBJECT (demux, "Failed to fetch time from NTP server %s",
          urls[clock_drift->selected_url]);
      g_mutex_unlock (&clock_drift->clock_lock);
      goto quit;
    }
  }
  start =
      gst_adaptive_demux_get_client_now_utc (GST_ADAPTIVE_DEMUX_CAST (demux));
  if (!value) {
    GstFragment *download;
    gint64 range_start = 0, range_end = -1;
    GST_DEBUG_OBJECT (demux, "Fetching current time from %s",
        urls[clock_drift->selected_url]);
    if (method == GST_MPD_UTCTIMING_TYPE_HTTP_HEAD) {
      range_start = -1;
    }
    download =
        gst_uri_downloader_fetch_uri_with_range (GST_ADAPTIVE_DEMUX_CAST
        (demux)->downloader, urls[clock_drift->selected_url], NULL, TRUE, TRUE,
        TRUE, range_start, range_end, NULL);
    if (download) {
      if (method == GST_MPD_UTCTIMING_TYPE_HTTP_HEAD && download->headers) {
        value = gst_dash_demux_parse_http_head (clock_drift, download);
      } else {
        buffer = gst_fragment_get_buffer (download);
      }
      g_object_unref (download);
    }
  }
  g_mutex_unlock (&clock_drift->clock_lock);
  if (!value && !buffer) {
    GST_ERROR_OBJECT (demux, "Failed to fetch time from %s",
        urls[clock_drift->selected_url]);
    goto quit;
  }
  end = gst_adaptive_demux_get_client_now_utc (GST_ADAPTIVE_DEMUX_CAST (demux));
  if (!value && method == GST_MPD_UTCTIMING_TYPE_HTTP_NTP) {
    value = gst_dash_demux_parse_http_ntp (clock_drift, buffer);
  } else if (!value) {
    /* GST_MPD_UTCTIMING_TYPE_HTTP_XSDATE or GST_MPD_UTCTIMING_TYPE_HTTP_ISO */
    value = gst_dash_demux_parse_http_xsdate (clock_drift, buffer);
  }
  if (buffer)
    gst_buffer_unref (buffer);
  if (value) {
    GTimeSpan download_duration = g_date_time_difference (end, start);
    GDateTime *client_now, *server_now;
    /* We don't know when the server sampled its clock, but we know
       it must have been before "end" and probably after "start".
       A reasonable estimate is to use (start+end)/2
     */
    client_now = g_date_time_add (start, download_duration / 2);
    server_now = gst_date_time_to_g_date_time (value);
    /* If gst_date_time_new_from_iso8601_string is given an unsupported
       ISO 8601 format, it can return a GstDateTime that is not valid,
       which causes gst_date_time_to_g_date_time to return NULL */
    if (server_now) {
      g_mutex_lock (&clock_drift->clock_lock);
      clock_drift->clock_compensation =
          g_date_time_difference (server_now, client_now);
      g_mutex_unlock (&clock_drift->clock_lock);
      GST_DEBUG_OBJECT (demux,
          "Difference between client and server clocks is %lfs",
          ((double) clock_drift->clock_compensation) / 1000000.0);
      g_date_time_unref (server_now);
      ret = TRUE;
    } else {
      GST_ERROR_OBJECT (demux, "Failed to parse DateTime from server");
    }
    g_date_time_unref (client_now);
    gst_date_time_unref (value);
  } else {
    GST_ERROR_OBJECT (demux, "Failed to parse DateTime from server");
  }
  g_date_time_unref (end);
quit:
  if (start)
    g_date_time_unref (start);
  /* if multiple URLs were specified, use a simple round-robin to
     poll each server */
  g_mutex_lock (&clock_drift->clock_lock);
  if (method == GST_MPD_UTCTIMING_TYPE_NTP) {
    clock_drift->next_update = now + FAST_CLOCK_UPDATE_INTERVAL;
  } else {
    clock_drift->selected_url =
        (1 + clock_drift->selected_url) % g_strv_length (urls);
    if (ret) {
      clock_drift->next_update = now + SLOW_CLOCK_UPDATE_INTERVAL;
    } else {
      clock_drift->next_update = now + FAST_CLOCK_UPDATE_INTERVAL;
    }
  }
  g_mutex_unlock (&clock_drift->clock_lock);
  return ret;
}

static GTimeSpan
gst_dash_demux_get_clock_compensation (GstDashDemux * demux)
{
  GTimeSpan rv = 0;
  if (demux->clock_drift) {
    g_mutex_lock (&demux->clock_drift->clock_lock);
    rv = demux->clock_drift->clock_compensation;
    g_mutex_unlock (&demux->clock_drift->clock_lock);
  }
  GST_LOG_OBJECT (demux, "Clock drift %" GST_STIME_FORMAT, GST_STIME_ARGS (rv));
  return rv;
}

static GDateTime *
gst_dash_demux_get_server_now_utc (GstDashDemux * demux)
{
  GDateTime *client_now;
  GDateTime *server_now;

  client_now =
      gst_adaptive_demux_get_client_now_utc (GST_ADAPTIVE_DEMUX_CAST (demux));
  server_now =
      g_date_time_add (client_now,
      gst_dash_demux_get_clock_compensation (demux));
  g_date_time_unref (client_now);
  return server_now;
}
