/* GStreamer
 * Copyright (C) 2004 Wim Taymans <wim@fluendo.com>
 *
 * gstoggdemux.c: ogg stream demuxer
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
 * SECTION:element-oggdemux
 * @title: oggdemux
 * @see_also: <link linkend="gst-plugins-base-plugins-oggmux">oggmux</link>
 *
 * This element demuxes ogg files into their encoded audio and video components.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=test.ogg ! oggdemux ! vorbisdec ! audioconvert ! audioresample ! autoaudiosink
 * ]|
 *  Decodes a vorbis audio stream stored inside an ogg container and plays it.
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gst/tag/tag.h>
#include <gst/audio/audio.h>

#include "gstoggelements.h"
#include "gstoggdemux.h"

#define CHUNKSIZE (8500)        /* this is out of vorbisfile */

/* we hope we get a granpos within this many bytes off the end */
#define DURATION_CHUNK_OFFSET (128*1024)

/* An Ogg page can not be larger than 255 segments of 255 bytes, plus
   26 bytes of header */
#define MAX_OGG_PAGE_SIZE (255 * 255 + 26)

#define GST_FLOW_LIMIT GST_FLOW_CUSTOM_ERROR
#define GST_FLOW_SKIP_PUSH GST_FLOW_CUSTOM_SUCCESS_1

#define SEEK_GIVE_UP_THRESHOLD (3*GST_SECOND)

#define GST_CHAIN_LOCK(ogg)     g_mutex_lock(&(ogg)->chain_lock)
#define GST_CHAIN_UNLOCK(ogg)   g_mutex_unlock(&(ogg)->chain_lock)

#define GST_PUSH_LOCK(ogg)                  \
  do {                                      \
    GST_TRACE_OBJECT(ogg, "Push lock");     \
    g_mutex_lock(&(ogg)->push_lock);        \
  } while(0)

#define GST_PUSH_UNLOCK(ogg)                \
  do {                                      \
    GST_TRACE_OBJECT(ogg, "Push unlock");   \
    g_mutex_unlock(&(ogg)->push_lock);      \
  } while(0)

GST_DEBUG_CATEGORY (gst_ogg_demux_debug);
GST_DEBUG_CATEGORY (gst_ogg_demux_setup_debug);
#define GST_CAT_DEFAULT gst_ogg_demux_debug


static ogg_packet *
_ogg_packet_copy (const ogg_packet * packet)
{
  ogg_packet *ret = g_new (ogg_packet, 1);

  *ret = *packet;
  ret->packet = g_memdup2 (packet->packet, packet->bytes);

  return ret;
}

static void
_ogg_packet_free (ogg_packet * packet)
{
  g_free (packet->packet);
  g_free (packet);
}

static ogg_page *
gst_ogg_page_copy (ogg_page * page)
{
  ogg_page *p = g_new (ogg_page, 1);

  /* make a copy of the page */
  p->header = g_memdup2 (page->header, page->header_len);
  p->header_len = page->header_len;
  p->body = g_memdup2 (page->body, page->body_len);
  p->body_len = page->body_len;

  return p;
}

static void
gst_ogg_page_free (ogg_page * page)
{
  g_free (page->header);
  g_free (page->body);
  g_free (page);
}

static gboolean gst_ogg_demux_collect_chain_info (GstOggDemux * ogg,
    GstOggChain * chain);
static gboolean gst_ogg_demux_activate_chain (GstOggDemux * ogg,
    GstOggChain * chain, GstEvent * event);
static void gst_ogg_pad_mark_discont (GstOggPad * pad);
static void gst_ogg_chain_mark_discont (GstOggChain * chain);

static gboolean gst_ogg_demux_perform_seek (GstOggDemux * ogg,
    GstEvent * event);
static gboolean gst_ogg_demux_receive_event (GstElement * element,
    GstEvent * event);

static void gst_ogg_pad_dispose (GObject * object);
static void gst_ogg_pad_finalize (GObject * object);

static gboolean gst_ogg_pad_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);
static gboolean gst_ogg_pad_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstOggPad *gst_ogg_chain_get_stream (GstOggChain * chain,
    guint32 serialno);

static GstFlowReturn gst_ogg_demux_combine_flows (GstOggDemux * ogg,
    GstOggPad * pad, GstFlowReturn ret);
static void gst_ogg_demux_sync_streams (GstOggDemux * ogg);

static GstCaps *gst_ogg_demux_set_header_on_caps (GstOggDemux * ogg,
    GstCaps * caps, GList * headers);
static gboolean gst_ogg_demux_send_event (GstOggDemux * ogg, GstEvent * event);
static gboolean gst_ogg_demux_perform_seek_push (GstOggDemux * ogg,
    GstEvent * event);
static gboolean gst_ogg_demux_check_duration_push (GstOggDemux * ogg,
    GstSeekFlags flags, GstEvent * event);

GType gst_ogg_pad_get_type (void);
G_DEFINE_TYPE (GstOggPad, gst_ogg_pad, GST_TYPE_PAD);

static void
gst_ogg_pad_class_init (GstOggPadClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = gst_ogg_pad_dispose;
  gobject_class->finalize = gst_ogg_pad_finalize;
}

static void
gst_ogg_pad_init (GstOggPad * pad)
{
  gst_pad_set_event_function (GST_PAD (pad),
      GST_DEBUG_FUNCPTR (gst_ogg_pad_event));
  gst_pad_set_query_function (GST_PAD (pad),
      GST_DEBUG_FUNCPTR (gst_ogg_pad_src_query));
  gst_pad_use_fixed_caps (GST_PAD (pad));

  pad->current_granule = -1;
  pad->prev_granule = -1;
  pad->keyframe_granule = -1;

  pad->start_time = GST_CLOCK_TIME_NONE;

  pad->position = GST_CLOCK_TIME_NONE;

  pad->have_type = FALSE;
  pad->continued = NULL;
  pad->map.headers = NULL;
  pad->map.queued = NULL;

  pad->map.granulerate_n = 0;
  pad->map.granulerate_d = 0;
  pad->map.granuleshift = -1;
}

static void
gst_ogg_pad_dispose (GObject * object)
{
  GstOggPad *pad = GST_OGG_PAD (object);

  pad->chain = NULL;
  pad->ogg = NULL;

  g_list_foreach (pad->map.headers, (GFunc) _ogg_packet_free, NULL);
  g_list_free (pad->map.headers);
  pad->map.headers = NULL;
  g_list_foreach (pad->map.queued, (GFunc) _ogg_packet_free, NULL);
  g_list_free (pad->map.queued);
  pad->map.queued = NULL;

  g_free (pad->map.index);
  pad->map.index = NULL;

  /* clear continued pages */
  g_list_foreach (pad->continued, (GFunc) gst_ogg_page_free, NULL);
  g_list_free (pad->continued);
  pad->continued = NULL;

  if (pad->map.caps) {
    gst_caps_unref (pad->map.caps);
    pad->map.caps = NULL;
  }

  if (pad->map.taglist) {
    gst_tag_list_unref (pad->map.taglist);
    pad->map.taglist = NULL;
  }

  ogg_stream_reset (&pad->map.stream);

  G_OBJECT_CLASS (gst_ogg_pad_parent_class)->dispose (object);
}

static void
gst_ogg_pad_finalize (GObject * object)
{
  GstOggPad *pad = GST_OGG_PAD (object);

  ogg_stream_clear (&pad->map.stream);

  G_OBJECT_CLASS (gst_ogg_pad_parent_class)->finalize (object);
}

static gboolean
gst_ogg_pad_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean res = TRUE;
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      GstOggPad *ogg_pad = GST_OGG_PAD (pad);

      gst_query_parse_position (query, &format, NULL);
      /* can only get position in time */
      if (format != GST_FORMAT_TIME)
        goto wrong_format;

      gst_query_set_position (query, format, ogg_pad->position);
      break;
    }
    case GST_QUERY_DURATION:
    {
      GstFormat format;
      gint64 total_time = -1;

      gst_query_parse_duration (query, &format, NULL);
      /* can only get duration in time */
      if (format != GST_FORMAT_TIME)
        goto wrong_format;

      if (ogg->total_time != -1) {
        /* we can return the total length */
        total_time = ogg->total_time;
      } else {
        gint bitrate = ogg->bitrate;

        /* try with length and bitrate */
        if (bitrate > 0) {
          GstQuery *uquery;

          /* ask upstream for total length in bytes */
          uquery = gst_query_new_duration (GST_FORMAT_BYTES);
          if (gst_pad_peer_query (ogg->sinkpad, uquery)) {
            gint64 length;

            gst_query_parse_duration (uquery, NULL, &length);

            /* estimate using the bitrate */
            total_time =
                gst_util_uint64_scale (length, 8 * GST_SECOND, bitrate);

            GST_LOG_OBJECT (ogg,
                "length: %" G_GINT64_FORMAT ", bitrate %d, total_time %"
                GST_TIME_FORMAT, length, bitrate, GST_TIME_ARGS (total_time));
          }
          gst_query_unref (uquery);
        }
      }

      gst_query_set_duration (query, GST_FORMAT_TIME, total_time);
      break;
    }
    case GST_QUERY_SEEKING:
    {
      GstFormat format;

      gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
      if (format == GST_FORMAT_TIME) {
        gboolean seekable = FALSE;
        gint64 stop = -1;

        GST_CHAIN_LOCK (ogg);
        if (ogg->pullmode) {
          seekable = TRUE;
          stop = ogg->total_time;
        } else if (ogg->push_disable_seeking) {
          seekable = FALSE;
        } else if (ogg->current_chain == NULL) {
          GstQuery *squery;

          /* assume we can seek if upstream is seekable in BYTES format */
          GST_LOG_OBJECT (ogg, "no current chain, check upstream seekability");
          squery = gst_query_new_seeking (GST_FORMAT_BYTES);
          if (gst_pad_peer_query (ogg->sinkpad, squery))
            gst_query_parse_seeking (squery, NULL, &seekable, NULL, NULL);
          else
            seekable = FALSE;
          gst_query_unref (squery);
        } else if (ogg->current_chain->streams->len) {
          gint i;

          seekable = FALSE;
          for (i = 0; i < ogg->current_chain->streams->len; i++) {
            GstOggPad *pad =
                g_array_index (ogg->current_chain->streams, GstOggPad *, i);

            seekable = TRUE;
            if (pad->map.index != NULL && pad->map.n_index != 0) {
              GstOggIndex *idx;
              GstClockTime idx_time;

              idx = &pad->map.index[pad->map.n_index - 1];
              idx_time =
                  gst_util_uint64_scale (idx->timestamp, GST_SECOND,
                  pad->map.kp_denom);
              if (stop == -1)
                stop = idx_time;
              else
                stop = MAX (idx_time, stop);
            } else {
              stop = ogg->push_time_length;
              if (stop == -1)
                stop = ogg->total_time;
            }
          }
        }

        gst_query_set_seeking (query, GST_FORMAT_TIME, seekable, 0, stop);
        GST_CHAIN_UNLOCK (ogg);
      } else {
        res = FALSE;
      }
      break;
    }
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = ogg->segment.format;

      start =
          gst_segment_to_stream_time (&ogg->segment, format,
          ogg->segment.start);
      if ((stop = ogg->segment.stop) == -1)
        stop = ogg->segment.duration;
      else
        stop = gst_segment_to_stream_time (&ogg->segment, format, stop);

      gst_query_set_segment (query, ogg->segment.rate, format, start, stop);
      res = TRUE;
      break;
    }
    default:
      res = gst_pad_query_default (pad, parent, query);
      break;
  }
done:

  return res;

  /* ERRORS */
wrong_format:
  {
    GST_DEBUG_OBJECT (ogg, "only query position/duration on TIME is supported");
    res = FALSE;
    goto done;
  }
}

static gboolean
gst_ogg_demux_receive_event (GstElement * element, GstEvent * event)
{
  gboolean res;
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (element);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* now do the seek */
      res = gst_ogg_demux_perform_seek (ogg, event);
      gst_event_unref (event);
      break;
    default:
      GST_DEBUG_OBJECT (ogg, "We only handle seek events here");
      goto error;
  }

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (ogg, "error handling event");
    gst_event_unref (event);
    return FALSE;
  }
}

static gboolean
gst_ogg_pad_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      /* now do the seek */
      res = gst_ogg_demux_perform_seek (ogg, event);
      gst_event_unref (event);
      break;
    case GST_EVENT_RECONFIGURE:
      GST_OGG_PAD (pad)->last_ret = GST_FLOW_OK;
      res = gst_pad_event_default (pad, parent, event);
      break;
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

static void
gst_ogg_pad_reset (GstOggPad * pad)
{
  ogg_stream_reset (&pad->map.stream);

  GST_DEBUG_OBJECT (pad, "doing reset");

  /* clear continued pages */
  g_list_foreach (pad->continued, (GFunc) gst_ogg_page_free, NULL);
  g_list_free (pad->continued);
  pad->continued = NULL;

  pad->last_ret = GST_FLOW_OK;
  pad->position = GST_CLOCK_TIME_NONE;
  pad->current_granule = -1;
  pad->prev_granule = -1;
  pad->keyframe_granule = -1;
  pad->is_eos = FALSE;
}

/* queue data, basically takes the packet, puts it in a buffer and store the
 * buffer in the queued list.  */
static GstFlowReturn
gst_ogg_demux_queue_data (GstOggPad * pad, ogg_packet * packet)
{
#ifndef GST_DISABLE_GST_DEBUG
  GstOggDemux *ogg = pad->ogg;
#endif

  GST_DEBUG_OBJECT (ogg, "%p queueing data serial %08x",
      pad, pad->map.serialno);

  pad->map.queued = g_list_append (pad->map.queued, _ogg_packet_copy (packet));

  /* we are ok now */
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_ogg_demux_chain_peer (GstOggPad * pad, ogg_packet * packet,
    gboolean push_headers)
{
  GstBuffer *buf = NULL;
  GstFlowReturn ret, cret;
  GstOggDemux *ogg = pad->ogg;
  gint64 current_time;
  GstOggChain *chain;
  gint64 duration;
  gint offset;
  gint trim;
  GstClockTime out_timestamp, out_duration;
  guint64 out_offset, out_offset_end;
  gboolean delta_unit = FALSE;
  gboolean is_header;
  guint64 clip_start = 0, clip_end = 0;

  ret = cret = GST_FLOW_OK;
  GST_DEBUG_OBJECT (pad, "Chaining %d %d %" GST_TIME_FORMAT " %d %p",
      ogg->pullmode, ogg->push_state, GST_TIME_ARGS (ogg->push_time_length),
      ogg->push_disable_seeking, ogg->building_chain);

  if (G_UNLIKELY (pad->is_eos)) {
    GST_DEBUG_OBJECT (pad, "Skipping packet on pad that is eos");
    ret = GST_FLOW_EOS;
    goto combine;
  }

  GST_PUSH_LOCK (ogg);
  if (!ogg->pullmode && ogg->push_state == PUSH_PLAYING
      && ogg->push_time_length == GST_CLOCK_TIME_NONE
      && !ogg->push_disable_seeking) {
    if (!ogg->building_chain) {
      /* we got all headers, now try to get duration */
      if (!gst_ogg_demux_check_duration_push (ogg, GST_SEEK_FLAG_FLUSH, NULL)) {
        GST_PUSH_UNLOCK (ogg);
        return GST_FLOW_OK;
      }
    }
    GST_PUSH_UNLOCK (ogg);
    return GST_FLOW_OK;
  }
  GST_PUSH_UNLOCK (ogg);

  GST_DEBUG_OBJECT (ogg,
      "%p streaming to peer serial %08x", pad, pad->map.serialno);

  gst_ogg_stream_update_stats (&pad->map, packet);

  if (pad->map.is_ogm) {
    const guint8 *data;
    long bytes;

    data = packet->packet;
    bytes = packet->bytes;

    if (bytes < 1)
      goto empty_packet;

    if ((data[0] & 1) || (data[0] & 3 && pad->map.is_ogm_text)) {
      /* We don't push header packets for OGM */
      goto done;
    }

    offset = 1 + (((data[0] & 0xc0) >> 6) | ((data[0] & 0x02) << 1));
    delta_unit = (((data[0] & 0x08) >> 3) == 0);

    trim = 0;

    /* Strip trailing \0 for subtitles */
    if (pad->map.is_ogm_text) {
      while (bytes && data[bytes - 1] == 0) {
        trim++;
        bytes--;
      }
    }
  } else if (pad->map.is_vp8) {
    if ((packet->bytes >= 7 && memcmp (packet->packet, "OVP80\2 ", 7) == 0) ||
        packet->b_o_s ||
        (packet->bytes >= 5 && memcmp (packet->packet, "OVP80", 5) == 0)) {
      /* Request the first packet being pushed downstream to have the header
         flag set, unblocking the keyframe_waiter_probe in decodebin3. */
      pad->need_header_flag = TRUE;
      /* We don't push header packets for VP8 */
      goto done;
    }
    offset = 0;
    trim = 0;
    delta_unit = !gst_ogg_stream_packet_is_key_frame (&pad->map, packet);
  } else {
    offset = 0;
    trim = 0;
    delta_unit = !gst_ogg_stream_packet_is_key_frame (&pad->map, packet);
  }

  /* get timing info for the packet */
  is_header = gst_ogg_stream_packet_is_header (&pad->map, packet);
  if (is_header) {
    duration = 0;
    GST_DEBUG_OBJECT (ogg, "packet is header");
  } else {
    duration = gst_ogg_stream_get_packet_duration (&pad->map, packet);
    GST_DEBUG_OBJECT (ogg, "packet duration %" G_GUINT64_FORMAT, duration);
  }


  /* If we get a hole at start, it might be we're catching a stream
   * partway through. In that case, if the stream has an index, the
   * index might be mooted. However, as it's totally valid to index
   * a stream with a hole at start (ie, capturing a live stream and
   * then index it), we now check whether the index references some
   * offset beyond the byte length (if known). If this is the case,
   * we can be reasonably sure we're getting a stream partway, with
   * its index being now useless since we don't know how many bytes
   * were skipped, preventing us from patching the index offsets to
   * match the hole size. */
  if (!is_header && ogg->check_index_overflow) {
    GstQuery *query;
    GstFormat format;
    int i;
    gint64 length;
    gboolean beyond;

    if (ogg->current_chain) {
      query = gst_query_new_duration (GST_FORMAT_BYTES);
      if (gst_pad_peer_query (ogg->sinkpad, query)) {
        gst_query_parse_duration (query, &format, &length);
        if (format == GST_FORMAT_BYTES && length >= 0) {
          for (i = 0; i < ogg->current_chain->streams->len; i++) {
            GstOggPad *ipad =
                g_array_index (ogg->current_chain->streams, GstOggPad *, i);
            if (!ipad->map.index)
              continue;
            beyond = ipad->map.n_index
                && ipad->map.index[ipad->map.n_index - 1].offset >= length;
            if (beyond) {
              GST_WARNING_OBJECT (pad, "Index offsets beyond byte length");
              if (ipad->discont) {
                /* hole - the index is most likely screwed up */
                GST_WARNING_OBJECT (ogg, "Discarding entire index");
                g_free (ipad->map.index);
                ipad->map.index = NULL;
                ipad->map.n_index = 0;
              } else {
                /* no hole - we can just clip the index if needed */
                GST_WARNING_OBJECT (ogg, "Clipping index");
                while (ipad->map.n_index > 0
                    && ipad->map.index[ipad->map.n_index - 1].offset >= length)
                  ipad->map.n_index--;
                if (ipad->map.n_index == 0) {
                  GST_WARNING_OBJECT (ogg, "The entire index was clipped");
                  g_free (ipad->map.index);
                  ipad->map.index = NULL;
                }
              }
              /* We can't trust the total time if the index goes beyond */
              ipad->map.total_time = -1;
            } else {
              /* use total time to update the total ogg time */
              if (ogg->total_time == -1) {
                ogg->total_time = ipad->map.total_time;
              } else if (ipad->map.total_time > 0) {
                ogg->total_time = MAX (ogg->total_time, ipad->map.total_time);
              }
            }
          }
        }
      }
      gst_query_unref (query);
    }
    ogg->check_index_overflow = FALSE;
  }

  if (packet->b_o_s) {
    out_timestamp = GST_CLOCK_TIME_NONE;
    out_duration = GST_CLOCK_TIME_NONE;
    out_offset = 0;
    out_offset_end = -1;
  } else {
    if (packet->granulepos > -1) {
      gint64 granule = gst_ogg_stream_granulepos_to_granule (&pad->map,
          packet->granulepos);
      if (granule < 0) {
        GST_ERROR_OBJECT (ogg,
            "granulepos %" G_GINT64_FORMAT " yielded granule %" G_GINT64_FORMAT,
            (gint64) packet->granulepos, (gint64) granule);
        return GST_FLOW_ERROR;
      }
      pad->current_granule = granule;
      pad->keyframe_granule =
          gst_ogg_stream_granulepos_to_key_granule (&pad->map,
          packet->granulepos);
      GST_DEBUG_OBJECT (ogg, "new granule %" G_GUINT64_FORMAT,
          pad->current_granule);
    } else if (pad->current_granule != -1) {
      pad->current_granule += duration;
      if (!delta_unit) {
        pad->keyframe_granule = pad->current_granule;
      }
      GST_DEBUG_OBJECT (ogg, "interpolating granule %" G_GUINT64_FORMAT,
          pad->current_granule);
    }

    if (ogg->segment.rate < 0.0 && pad->current_granule == -1) {
      /* negative rates, allow output of packets with no timestamp, let downstream reconstruct */
      out_timestamp = -1;
      out_duration = -1;
      out_offset = -1;
      out_offset_end = -1;
      pad->prev_granule = -1;
    } else {
      /* we only push buffers after we have a valid granule. This is done so that
       * we nicely skip packets without a timestamp after a seek. This is ok
       * because we base our seek on the packet after the page with the smaller
       * timestamp. */
      if (pad->current_granule == -1) {
        pad->prev_granule = -1;
        goto no_timestamp;
      }

      if (pad->map.is_ogm) {
        out_timestamp = gst_ogg_stream_granule_to_time (&pad->map,
            pad->current_granule);
        out_duration = gst_util_uint64_scale (duration,
            GST_SECOND * pad->map.granulerate_d, pad->map.granulerate_n);
      } else if (pad->map.is_sparse) {
        out_timestamp = gst_ogg_stream_granule_to_time (&pad->map,
            pad->current_granule);
        if (duration == GST_CLOCK_TIME_NONE) {
          out_duration = GST_CLOCK_TIME_NONE;
        } else {
          out_duration = gst_util_uint64_scale (duration,
              GST_SECOND * pad->map.granulerate_d, pad->map.granulerate_n);
        }
      } else {
        /* The last packet may be clipped. This will be represented
           by the last granule being smaller than what it would otherwise
           have been, had no content been clipped. In that case, we
           cannot calculate the PTS of the audio from the packet length
           and granule. */
        if (packet->e_o_s) {
          if (pad->prev_granule >= 0)
            out_timestamp = gst_ogg_stream_granule_to_time (&pad->map,
                pad->prev_granule);
          else
            out_timestamp = GST_CLOCK_TIME_NONE;

          if (pad->map.audio_clipping
              && pad->current_granule < pad->prev_granule + duration) {
            clip_end = pad->prev_granule + duration - pad->current_granule;
          }
          if (pad->map.audio_clipping
              && pad->current_granule - duration < -pad->map.granule_offset) {
            if (pad->current_granule >= -pad->map.granule_offset) {
              guint64 already_removed =
                  pad->current_granule >
                  duration ? pad->current_granule - duration : 0;
              clip_start =
                  already_removed >
                  -pad->map.granule_offset ? 0 : -pad->map.granule_offset -
                  already_removed;
            } else
              clip_start = pad->current_granule;
          }
        } else {
          out_timestamp = gst_ogg_stream_granule_to_time (&pad->map,
              pad->current_granule - duration);

          if (pad->map.audio_clipping
              && pad->current_granule - duration < -pad->map.granule_offset) {
            if (pad->current_granule >= -pad->map.granule_offset) {
              guint64 already_removed =
                  pad->current_granule >
                  duration ? pad->current_granule - duration : 0;
              clip_start =
                  already_removed >
                  -pad->map.granule_offset ? 0 : -pad->map.granule_offset -
                  already_removed;
            } else
              clip_start = pad->current_granule;
          }
        }
        out_duration =
            gst_ogg_stream_granule_to_time (&pad->map,
            pad->current_granule) - out_timestamp;
      }
      out_offset_end =
          gst_ogg_stream_granule_to_granulepos (&pad->map,
          pad->current_granule, pad->keyframe_granule);
      out_offset =
          gst_ogg_stream_granule_to_time (&pad->map, pad->current_granule);
    }
    pad->prev_granule = pad->current_granule;
  }

  if (G_UNLIKELY (offset + trim > packet->bytes))
    goto invalid_packet;
  else if (pad->map.is_ogm_text) {
    /* check for invalid buffer sizes */
    if (G_UNLIKELY (offset + trim >= packet->bytes))
      goto empty_packet;
  }

  if (!pad->added)
    goto not_added;

  buf = gst_buffer_new_and_alloc (packet->bytes - offset - trim);

  if (pad->map.audio_clipping && (clip_start || clip_end)) {
    GST_DEBUG_OBJECT (pad,
        "Clipping %" G_GUINT64_FORMAT " %" G_GUINT64_FORMAT " (%"
        G_GUINT64_FORMAT " / %" G_GUINT64_FORMAT ")", clip_start, clip_end,
        clip_start + clip_end, duration);
    gst_buffer_add_audio_clipping_meta (buf, GST_FORMAT_DEFAULT, clip_start,
        clip_end);
  }

  /* set delta flag for OGM content */
  if (delta_unit)
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

  /* set header flag for buffers that are also in the streamheaders or when explicitely requested (VP8). */
  if (is_header || pad->need_header_flag) {
    pad->need_header_flag = FALSE;
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_HEADER);
  }

  if (packet->packet != NULL) {
    /* copy packet in buffer */
    gst_buffer_fill (buf, 0, packet->packet + offset,
        packet->bytes - offset - trim);
  }

  GST_BUFFER_TIMESTAMP (buf) = out_timestamp;
  GST_BUFFER_DURATION (buf) = out_duration;
  GST_BUFFER_OFFSET (buf) = out_offset;
  GST_BUFFER_OFFSET_END (buf) = out_offset_end;

  /* Mark discont on the buffer */
  if (pad->discont) {
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
    if (ogg->segment.rate < 0.0 || GST_BUFFER_TIMESTAMP_IS_VALID (buf))
      pad->discont = FALSE;
  }

  /* don't push the header packets when we are asked to skip them */
  if (!packet->b_o_s || push_headers) {
    if (pad->last_ret == GST_FLOW_OK) {
      GST_LOG_OBJECT (ogg, "Pushing buf %" GST_PTR_FORMAT, buf);
      ret = gst_pad_push (GST_PAD_CAST (pad), buf);
    } else {
      GST_DEBUG_OBJECT (ogg, "not pushing buffer on error pad");
      ret = pad->last_ret;
      gst_buffer_unref (buf);
    }
    buf = NULL;
  }

  /* we're done with skeleton stuff */
  if (pad->map.is_skeleton)
    goto combine;

  /* check if valid granulepos, then we can calculate the current
   * position. We know the granule for each packet but we only want to update
   * the position when we have a valid granulepos on the packet because else
   * our time jumps around for the different streams. */
  if (packet->granulepos < 0)
    goto combine;

  /* convert to time */
  current_time = gst_ogg_stream_get_end_time_for_granulepos (&pad->map,
      packet->granulepos);

  /* convert to stream time */
  if ((chain = pad->chain)) {
    gint64 chain_start = 0;

    if (chain->segment_start != GST_CLOCK_TIME_NONE)
      chain_start = chain->segment_start;

    current_time = current_time - chain_start + chain->begin_time;
  }

  /* and store as the current position */
  ogg->segment.position = current_time;

  GST_DEBUG_OBJECT (ogg, "ogg current time %" GST_TIME_FORMAT
      " (%" G_GINT64_FORMAT ")", GST_TIME_ARGS (current_time), current_time);

  pad->position = ogg->segment.position;

  /* check stream eos */
  if (!pad->is_eos && !delta_unit &&
      ((ogg->segment.rate > 0.0 &&
              ogg->segment.stop != GST_CLOCK_TIME_NONE &&
              current_time >= ogg->segment.stop) ||
          (ogg->segment.rate < 0.0 && current_time <= ogg->segment.start))) {
    GST_DEBUG_OBJECT (ogg, "marking pad %p EOS", pad);
    pad->is_eos = TRUE;

    if (ret == GST_FLOW_OK) {
      ret = GST_FLOW_EOS;
    }
  }

combine:
  /* combine flows */
  cret = gst_ogg_demux_combine_flows (ogg, pad, ret);

done:
  if (buf)
    gst_buffer_unref (buf);
  /* return combined flow result */
  return cret;

  /* special cases */
empty_packet:
  {
    GST_DEBUG_OBJECT (ogg, "Skipping empty packet");
    goto done;
  }

invalid_packet:
  {
    GST_DEBUG_OBJECT (ogg, "Skipping invalid packet");
    goto done;
  }

no_timestamp:
  {
    GST_DEBUG_OBJECT (ogg, "skipping packet: no valid granule found yet");
    goto done;
  }
not_added:
  {
    GST_DEBUG_OBJECT (ogg, "pad not added yet");
    goto done;
  }
}

static guint64
gst_ogg_demux_collect_start_time (GstOggDemux * ogg, GstOggChain * chain)
{
  gint i;
  guint64 start_time = G_MAXUINT64;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    if (pad->map.is_skeleton)
      continue;

    /*  can do this if the pad start time is not defined */
    GST_DEBUG_OBJECT (ogg, "Pad %08x (%s) start time is %" GST_TIME_FORMAT,
        pad->map.serialno, gst_ogg_stream_get_media_type (&pad->map),
        GST_TIME_ARGS (pad->start_time));
    if (pad->start_time == GST_CLOCK_TIME_NONE) {
      if (!pad->map.is_sparse) {
        start_time = G_MAXUINT64;
        break;
      }
    } else {
      start_time = MIN (start_time, pad->start_time);
    }
  }
  return start_time;
}

static GstClockTime
gst_ogg_demux_collect_sync_time (GstOggDemux * ogg, GstOggChain * chain)
{
  gint i;
  GstClockTime sync_time = GST_CLOCK_TIME_NONE;

  if (!chain) {
    GST_WARNING_OBJECT (ogg, "No chain!");
    return GST_CLOCK_TIME_NONE;
  }

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    if (pad->map.is_sparse)
      continue;

    if (pad->push_sync_time == GST_CLOCK_TIME_NONE) {
      sync_time = GST_CLOCK_TIME_NONE;
      break;
    } else {
      if (sync_time == GST_CLOCK_TIME_NONE)
        sync_time = pad->push_sync_time;
      else
        sync_time = MAX (sync_time, pad->push_sync_time);
    }
  }
  return sync_time;
}

/* submit a packet to the oggpad, this function will run the type detection
 * code for the pad if this is the first packet for this stream
 */
static GstFlowReturn
gst_ogg_pad_submit_packet (GstOggPad * pad, ogg_packet * packet)
{
  gint64 granule;
  GstFlowReturn ret = GST_FLOW_OK;

  GstOggDemux *ogg = pad->ogg;

  GST_DEBUG_OBJECT (ogg, "%p submit packet serial %08x",
      pad, pad->map.serialno);

  if (!pad->have_type) {
    pad->have_type = gst_ogg_stream_setup_map (&pad->map, packet);
    if (!pad->have_type && !pad->map.caps) {
      pad->map.caps = gst_caps_new_empty_simple ("application/x-unknown");
    }
    if (pad->map.is_skeleton) {
      GST_DEBUG_OBJECT (ogg, "we have a fishead");
      /* copy values over to global ogg level */
      ogg->basetime = pad->map.basetime;
      ogg->prestime = pad->map.prestime;

      /* use total time to update the total ogg time */
      if (ogg->total_time == -1) {
        ogg->total_time = pad->map.total_time;
      } else if (pad->map.total_time > 0) {
        ogg->total_time = MAX (ogg->total_time, pad->map.total_time);
      }
    }
    if (!pad->map.caps) {
      GST_WARNING_OBJECT (ogg, "stream parser didn't create src pad caps");
    }
  }

  if (pad->map.is_skeleton) {
    guint32 serialno;
    GstOggPad *skel_pad;
    GstOggSkeleton type;

    /* try to parse the serialno first */
    if (gst_ogg_map_parse_fisbone (&pad->map, packet->packet, packet->bytes,
            &serialno, &type)) {

      GST_DEBUG_OBJECT (pad->ogg,
          "got skeleton packet for stream 0x%08x", serialno);

      skel_pad = gst_ogg_chain_get_stream (pad->chain, serialno);
      if (skel_pad) {
        switch (type) {
          case GST_OGG_SKELETON_FISBONE:
            /* parse the remainder of the fisbone in the pad with the serialno,
             * note that we ignore the start_time as this is usually wrong for
             * live streams */
            gst_ogg_map_add_fisbone (&skel_pad->map, &pad->map, packet->packet,
                packet->bytes, NULL);
            break;
          case GST_OGG_SKELETON_INDEX:
            gst_ogg_map_add_index (&skel_pad->map, &pad->map, packet->packet,
                packet->bytes);
            ogg->check_index_overflow = TRUE;
            break;
          default:
            break;
        }

      } else {
        GST_WARNING_OBJECT (pad->ogg,
            "found skeleton fisbone for an unknown stream 0x%08x", serialno);
      }
    }
  }

  GST_DEBUG_OBJECT (ogg, "%p packet has granulepos %" G_GINT64_FORMAT, pad,
      (gint64) packet->granulepos);
  granule =
      gst_ogg_stream_granulepos_to_granule (&pad->map, packet->granulepos);
  if (granule > 0) {
    GST_DEBUG_OBJECT (ogg, "%p has granule %" G_GINT64_FORMAT, pad, granule);
    pad->current_granule = granule;
  } else if (granule == 0) {
    /* headers */
  } else if (granule != -1) {
    GST_ERROR_OBJECT (ogg,
        "granulepos %" G_GINT64_FORMAT " yielded granule %" G_GINT64_FORMAT,
        (gint64) packet->granulepos, (gint64) granule);
    return GST_FLOW_ERROR;
  }

  /* restart header packet count when seeing a b_o_s page;
   * particularly useful following a seek or even following chain finding */
  if (packet->b_o_s) {
    GST_DEBUG_OBJECT (ogg, "b_o_s packet, resetting header packet count");
    pad->map.n_header_packets_seen = 0;
    if (!pad->map.have_headers) {
      GST_DEBUG_OBJECT (ogg, "clearing header packets");
      g_list_foreach (pad->map.headers, (GFunc) _ogg_packet_free, NULL);
      g_list_free (pad->map.headers);
      pad->map.headers = NULL;
    }
  }

  /* Overload the value of b_o_s in ogg_packet with a flag whether or
   * not this is a header packet.  Maybe some day this could be cleaned
   * up.  */
  packet->b_o_s = gst_ogg_stream_packet_is_header (&pad->map, packet);
  if (!packet->b_o_s) {
    GST_DEBUG ("found non-header packet");
    pad->map.have_headers = TRUE;
    if (pad->start_time == GST_CLOCK_TIME_NONE) {
      gint64 duration = gst_ogg_stream_get_packet_duration (&pad->map, packet);
      GST_DEBUG ("duration %" G_GINT64_FORMAT, duration);
      if (duration != -1) {
        pad->map.accumulated_granule += duration;
        GST_DEBUG ("accumulated granule %" G_GINT64_FORMAT,
            pad->map.accumulated_granule);
      }

      if (packet->granulepos != -1) {
        ogg_int64_t start_granule;
        gint64 granule;

        granule = gst_ogg_stream_granulepos_to_granule (&pad->map,
            packet->granulepos);
        if (granule < 0) {
          GST_ERROR_OBJECT (ogg,
              "granulepos %" G_GINT64_FORMAT " yielded granule %"
              G_GINT64_FORMAT, (gint64) packet->granulepos, (gint64) granule);
          return GST_FLOW_ERROR;
        }

        if (granule >= pad->map.accumulated_granule)
          start_granule = granule - pad->map.accumulated_granule;
        else
          start_granule = 0;

        pad->start_time = gst_ogg_stream_granule_to_time (&pad->map,
            start_granule);
        GST_DEBUG_OBJECT (ogg,
            "start time %" GST_TIME_FORMAT " (%" GST_TIME_FORMAT ") for %s "
            "from granpos %" G_GINT64_FORMAT " (granule %" G_GINT64_FORMAT ", "
            "accumulated granule %" G_GINT64_FORMAT,
            GST_TIME_ARGS (pad->start_time), GST_TIME_ARGS (pad->start_time),
            gst_ogg_stream_get_media_type (&pad->map),
            (gint64) packet->granulepos, granule, pad->map.accumulated_granule);
      } else {
        packet->granulepos = gst_ogg_stream_granule_to_granulepos (&pad->map,
            pad->map.accumulated_granule + pad->current_granule,
            pad->keyframe_granule);
      }
    }
  } else {
    /* look for tags in header packet (before inc header count) */
    gst_ogg_stream_extract_tags (&pad->map, packet);
    pad->map.n_header_packets_seen++;
    if (!pad->map.have_headers) {
      pad->map.headers =
          g_list_append (pad->map.headers, _ogg_packet_copy (packet));
      GST_DEBUG ("keeping header packet %d", pad->map.n_header_packets_seen);
    }
  }

  /* we know the start_time of the pad data, see if we
   * can activate the complete chain if this is a dynamic
   * chain. We need all the headers too for this. */
  if (pad->start_time != GST_CLOCK_TIME_NONE && pad->map.have_headers) {
    GstOggChain *chain = pad->chain;

    /* check if complete chain has start time */
    if (chain == ogg->building_chain) {
      GstEvent *event = NULL;

      if (ogg->resync) {
        guint64 start_time;

        GST_DEBUG_OBJECT (ogg, "need to resync");

        /* when we need to resync after a seek, we wait until we have received
         * timestamps on all streams */
        start_time = gst_ogg_demux_collect_start_time (ogg, chain);

        if (start_time != G_MAXUINT64) {
          gint64 segment_time;
          GstSegment segment;

          GST_DEBUG_OBJECT (ogg, "start_time:  %" GST_TIME_FORMAT,
              GST_TIME_ARGS (start_time));

          if (chain->segment_start < start_time)
            segment_time =
                (start_time - chain->segment_start) + chain->begin_time;
          else
            segment_time = chain->begin_time;

          /* create the newsegment event we are going to send out */
          gst_segment_init (&segment, GST_FORMAT_TIME);

          GST_PUSH_LOCK (ogg);
          if (!ogg->pullmode && ogg->push_state == PUSH_LINEAR2) {
            /* if we are fast forwarding to the actual seek target,
               ensure previous frames are clipped */
            GST_DEBUG_OBJECT (ogg,
                "Resynced, starting segment at %" GST_TIME_FORMAT
                ", start_time %" GST_TIME_FORMAT,
                GST_TIME_ARGS (ogg->push_seek_time_original_target),
                GST_TIME_ARGS (start_time));
            segment.rate = ogg->push_seek_rate;
            segment.start = ogg->push_seek_time_original_target;
            segment.position = ogg->push_seek_time_original_target;
            segment.stop = ogg->push_seek_time_original_stop;
            segment.time = ogg->push_seek_time_original_target;
            segment.base = ogg->segment.base;
            event = gst_event_new_segment (&segment);
            ogg->push_state = PUSH_PLAYING;
          } else {
            segment.rate = ogg->segment.rate;
            segment.applied_rate = ogg->segment.applied_rate;
            segment.start = start_time;
            segment.position = start_time;
            segment.stop = chain->segment_stop;
            segment.time = segment_time;
            segment.base = ogg->segment.base;
            event = gst_event_new_segment (&segment);
          }
          GST_PUSH_UNLOCK (ogg);

          ogg->resync = FALSE;
        }
      } else {
        /* see if we have enough info to activate the chain, we have enough info
         * when all streams have a valid start time. */
        if (gst_ogg_demux_collect_chain_info (ogg, chain)) {
          GstSegment segment;

          GST_DEBUG_OBJECT (ogg, "segment_start: %" GST_TIME_FORMAT,
              GST_TIME_ARGS (chain->segment_start));
          GST_DEBUG_OBJECT (ogg, "segment_stop:  %" GST_TIME_FORMAT,
              GST_TIME_ARGS (chain->segment_stop));
          GST_DEBUG_OBJECT (ogg, "segment_time:  %" GST_TIME_FORMAT,
              GST_TIME_ARGS (chain->begin_time));

          /* create the newsegment event we are going to send out */
          gst_segment_init (&segment, GST_FORMAT_TIME);
          segment.rate = ogg->segment.rate;
          segment.applied_rate = ogg->segment.applied_rate;
          segment.start = chain->segment_start;
          segment.position = chain->segment_start;
          segment.stop = chain->segment_stop;
          segment.time = chain->begin_time;
          segment.base = ogg->segment.base + segment.time;
          event = gst_event_new_segment (&segment);
        }
      }

      if (event) {
        gst_event_set_seqnum (event, ogg->seqnum);

        gst_ogg_demux_activate_chain (ogg, chain, event);

        ogg->building_chain = NULL;
      }
    }
  }

  /* if we are building a chain, store buffer for when we activate
   * it. This path is taken if we operate in streaming mode. */
  if (ogg->building_chain) {
    /* bos packets where stored in the header list so we can discard
     * them here*/
    if (!packet->b_o_s)
      ret = gst_ogg_demux_queue_data (pad, packet);
  }
  /* else we are completely streaming to the peer */
  else {
    ret = gst_ogg_demux_chain_peer (pad, packet, !ogg->pullmode);
  }
  return ret;
}

/* flush at most @npackets from the stream layer. All packets if 
 * @npackets is 0;
 */
static GstFlowReturn
gst_ogg_pad_stream_out (GstOggPad * pad, gint npackets)
{
  GstFlowReturn result = GST_FLOW_OK;
  gboolean done = FALSE;
  GstOggDemux *ogg;

  ogg = pad->ogg;

  while (!done) {
    int ret;
    ogg_packet packet;

    ret = ogg_stream_packetout (&pad->map.stream, &packet);
    switch (ret) {
      case 0:
        GST_LOG_OBJECT (ogg, "packetout done");
        done = TRUE;
        break;
      case -1:
        GST_LOG_OBJECT (ogg, "packetout discont");
        if (!pad->map.is_sparse) {
          gst_ogg_chain_mark_discont (pad->chain);
        } else {
          gst_ogg_pad_mark_discont (pad);
        }
        break;
      case 1:
        GST_LOG_OBJECT (ogg, "packetout gave packet of size %ld", packet.bytes);

        if (packet.granulepos < -1) {
          GST_WARNING_OBJECT (ogg,
              "Invalid granulepos (%" G_GINT64_FORMAT "), resetting stream",
              (gint64) packet.granulepos);
          gst_ogg_pad_reset (pad);
          break;
        }

        if (packet.bytes > ogg->max_packet_size)
          ogg->max_packet_size = packet.bytes;
        result = gst_ogg_pad_submit_packet (pad, &packet);
        /* not linked is not a problem, it's possible that we are still
         * collecting headers and that we don't have exposed the pads yet */
        if (result == GST_FLOW_NOT_LINKED)
          break;
        else if (result <= GST_FLOW_EOS)
          goto could_not_submit;
        break;
      default:
        GST_WARNING_OBJECT (ogg,
            "invalid return value %d for ogg_stream_packetout, resetting stream",
            ret);
        gst_ogg_pad_reset (pad);
        break;
    }
    if (npackets > 0) {
      npackets--;
      done = (npackets == 0);
    }
  }
  return result;

  /* ERRORS */
could_not_submit:
  {
    GST_WARNING_OBJECT (ogg,
        "could not submit packet for stream %08x, "
        "error: %d", pad->map.serialno, result);
    gst_ogg_pad_reset (pad);
    return result;
  }
}

static void
gst_ogg_demux_setup_first_granule (GstOggDemux * ogg, GstOggPad * pad,
    ogg_page * page)
{
  /* When we submit a page, we check if we have started tracking granules.
   * If not, we calculate the granule corresponding to the first packet
   * on the page. */
  gboolean valid_granule = TRUE;

  if (pad->current_granule == -1) {
    ogg_int64_t granpos = ogg_page_granulepos (page);
    if (granpos > 0) {
      gint64 granule =
          (gint64) gst_ogg_stream_granulepos_to_granule (&pad->map, granpos);
      gint64 duration;
      int packets = ogg_page_packets (page), n;
      GST_DEBUG_OBJECT (pad,
          "This page completes %d packets, granule %" G_GINT64_FORMAT, packets,
          granule);

      if (packets > 0) {
        ogg_stream_state os;
        ogg_packet op;
        int last_size = pad->map.last_size;

        memcpy (&os, &pad->map.stream, sizeof (os));
        for (n = 0; valid_granule && n < packets; ++n) {
          int ret = ogg_stream_packetout (&os, &op);
          if (ret < 0) {
            /* This usually means a continued packet after a seek and we can't calc the first granule,
             * but sometimes not - so if it's ret == -1 and first packet, try again */
            if (ret == -1 && n == 0) {
              n--;
              continue;
            }
            GST_DEBUG_OBJECT (pad, "Failed to read packet off first page");
            valid_granule = FALSE;
            break;
          }
          if (ret == 0) {
            GST_WARNING_OBJECT (pad,
                "Short read getting %d packets off first page", packets);
            valid_granule = FALSE;
            break;
          }
          duration = gst_ogg_stream_get_packet_duration (&pad->map, &op);
          GST_DEBUG_OBJECT (pad, "Packet %d has duration %" G_GINT64_FORMAT,
              n, duration);
          granule -= duration;
        }
        pad->map.last_size = last_size;
        if (valid_granule) {
          if (granule >= 0) {
            pad->current_granule = granule;
            GST_INFO_OBJECT (pad,
                "Starting with first granule %" G_GINT64_FORMAT, granule);
          } else {
            pad->current_granule = 0;
            GST_INFO_OBJECT (pad, "Extrapolated first granule is negative, "
                "used to clip samples at start");
          }
        }
      } else {
        GST_WARNING_OBJECT (pad,
            "Ogg page finishing no packets, but a valid granule");
      }
    }
  }
}

static void
gst_ogg_demux_setup_bisection_bounds (GstOggDemux * ogg)
{
  if (ogg->push_last_seek_time >= ogg->push_seek_time_target) {
    GST_DEBUG_OBJECT (ogg, "We overshot by %" GST_TIME_FORMAT,
        GST_TIME_ARGS (ogg->push_last_seek_time - ogg->push_seek_time_target));
    ogg->push_offset1 = ogg->push_last_seek_offset;
    ogg->push_time1 = ogg->push_last_seek_time;
    ogg->seek_undershot = FALSE;
  } else {
    GST_DEBUG_OBJECT (ogg, "We undershot by %" GST_TIME_FORMAT,
        GST_TIME_ARGS (ogg->push_seek_time_target - ogg->push_last_seek_time));
    ogg->push_offset0 = ogg->push_last_seek_offset;
    ogg->push_time0 = ogg->push_last_seek_time;
    ogg->seek_undershot = TRUE;
  }
}

static gint64
gst_ogg_demux_estimate_bisection_target (GstOggDemux * ogg, float seek_quality)
{
  gint64 best;
  gint64 segment_bitrate;
  gint64 skew;

  /* we might not know the length of the stream in time,
     so push_time1 might not be set */
  GST_DEBUG_OBJECT (ogg,
      "push time 1: %" GST_TIME_FORMAT ", dbytes %" G_GINT64_FORMAT,
      GST_TIME_ARGS (ogg->push_time1), ogg->push_offset1 - ogg->push_offset0);
  if (ogg->push_time1 == GST_CLOCK_TIME_NONE) {
    GST_DEBUG_OBJECT (ogg,
        "New segment to consider: bytes %" G_GINT64_FORMAT " %" G_GINT64_FORMAT
        ", time %" GST_TIME_FORMAT " (open ended)", ogg->push_offset0,
        ogg->push_offset1, GST_TIME_ARGS (ogg->push_time0));
    if (ogg->push_last_seek_time == ogg->push_start_time) {
      /* if we're at start and don't know the end time, we can't estimate
         bitrate, so get the nominal declared bitrate as a failsafe, or some
         random constant which will be discarded after we made a (probably
         dire) first guess */
      segment_bitrate = (ogg->bitrate > 0 ? ogg->bitrate : 1000);
    } else {
      segment_bitrate =
          gst_util_uint64_scale (ogg->push_last_seek_offset - 0,
          8 * GST_SECOND, ogg->push_last_seek_time - ogg->push_start_time);
    }
    best =
        ogg->push_offset0 +
        gst_util_uint64_scale (ogg->push_seek_time_target - ogg->push_time0,
        segment_bitrate, 8 * GST_SECOND);
    ogg->seek_secant = TRUE;
  } else {
    GST_DEBUG_OBJECT (ogg,
        "New segment to consider: bytes %" G_GINT64_FORMAT " %" G_GINT64_FORMAT
        ", time %" GST_TIME_FORMAT " %" GST_TIME_FORMAT, ogg->push_offset0,
        ogg->push_offset1, GST_TIME_ARGS (ogg->push_time0),
        GST_TIME_ARGS (ogg->push_time1));
    if (ogg->push_time0 == ogg->push_time1) {
      best = ogg->push_offset0;
    } else {
      segment_bitrate =
          gst_util_uint64_scale (ogg->push_offset1 - ogg->push_offset0,
          8 * GST_SECOND, ogg->push_time1 - ogg->push_time0);
      GST_DEBUG_OBJECT (ogg,
          "Local bitrate on the %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT
          " segment: %" G_GINT64_FORMAT, GST_TIME_ARGS (ogg->push_time0),
          GST_TIME_ARGS (ogg->push_time1), segment_bitrate);

      best =
          ogg->push_offset0 +
          gst_util_uint64_scale (ogg->push_seek_time_target - ogg->push_time0,
          segment_bitrate, 8 * GST_SECOND);
      if (seek_quality < 0.5f && ogg->seek_secant) {
        gint64 new_best, best2 = (ogg->push_offset0 + ogg->push_offset1) / 2;
        /* if dire result, give as much as 25% weight to a dumb bisection guess */
        float secant_weight = 1.0f - ((0.5 - seek_quality) / 0.5f) * 0.25;
        new_best = (best * secant_weight + best2 * (1.0f - secant_weight));
        GST_DEBUG_OBJECT (ogg,
            "Secant says %" G_GINT64_FORMAT ", straight is %" G_GINT64_FORMAT
            ", new best %" G_GINT64_FORMAT " with secant_weight %f", best,
            best2, new_best, secant_weight);
        best = new_best;
        ogg->seek_secant = FALSE;
      } else {
        ogg->seek_secant = TRUE;
      }
    }
  }

  GST_DEBUG_OBJECT (ogg, "Raw best guess: %" G_GINT64_FORMAT, best);

  /* offset the guess down as we need to capture the start of the
     page we are targeting - but only do so if we did not undershoot
     last time, as we're likely to still do this time */
  if (!ogg->seek_undershot) {
    /* very small packets are packed on pages, so offset by at least
       a value which is likely to get us at least one page where the
       packet starts */
    skew =
        ogg->max_packet_size >
        ogg->max_page_size ? ogg->max_packet_size : ogg->max_page_size;
    GST_DEBUG_OBJECT (ogg, "Offsetting by %" G_GINT64_FORMAT, skew);
    best -= skew;
  }

  /* do not seek too close to the bounds, as we stop seeking
     when we get to within max_packet_size before the target */
  if (best > ogg->push_offset1 - ogg->max_packet_size) {
    best = ogg->push_offset1 - ogg->max_packet_size;
    GST_DEBUG_OBJECT (ogg,
        "Too close to high bound, pushing back to %" G_GINT64_FORMAT, best);
  } else if (best < ogg->push_offset0 + ogg->max_packet_size) {
    best = ogg->push_offset0 + ogg->max_packet_size;
    GST_DEBUG_OBJECT (ogg,
        "Too close to low bound, pushing forth to %" G_GINT64_FORMAT, best);
  }

  /* keep within bounds */
  if (best > ogg->push_offset1)
    best = ogg->push_offset1;
  if (best < ogg->push_offset0)
    best = ogg->push_offset0;

  GST_DEBUG_OBJECT (ogg, "Choosing target %" G_GINT64_FORMAT, best);
  return best;
}

static void
gst_ogg_demux_record_keyframe_time (GstOggDemux * ogg, GstOggPad * pad,
    ogg_int64_t granpos)
{
  gint64 kf_granule;
  GstClockTime kf_time;

  kf_granule = gst_ogg_stream_granulepos_to_key_granule (&pad->map, granpos);
  kf_time = gst_ogg_stream_granule_to_time (&pad->map, kf_granule);

  pad->push_kf_time = kf_time;
}

/* returns the earliest keyframe time for all non sparse pads in the chain,
 * if known, and GST_CLOCK_TIME_NONE if not */
static GstClockTime
gst_ogg_demux_get_earliest_keyframe_time (GstOggDemux * ogg)
{
  GstClockTime t = GST_CLOCK_TIME_NONE;
  GstOggChain *chain = ogg->building_chain;
  int i;

  if (!chain) {
    GST_WARNING_OBJECT (ogg, "No chain!");
    return GST_CLOCK_TIME_NONE;
  }
  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    if (pad->map.is_sparse)
      continue;
    if (pad->push_kf_time == GST_CLOCK_TIME_NONE)
      return GST_CLOCK_TIME_NONE;
    if (t == GST_CLOCK_TIME_NONE || pad->push_kf_time < t)
      t = pad->push_kf_time;
  }

  return t;
}

/* MUST be called with the push lock locked, and will unlock it
   regardless of return value. */
static GstFlowReturn
gst_ogg_demux_seek_back_after_push_duration_check_unlock (GstOggDemux * ogg)
{
  GstEvent *event;

  /* Get the delayed event, if any */
  event = ogg->push_mode_seek_delayed_event;
  ogg->push_mode_seek_delayed_event = NULL;

  /* if we haven't learnt about the total time yet, disable seeking */
  if (ogg->total_time == -1)
    ogg->push_disable_seeking = TRUE;

  ogg->push_state = PUSH_PLAYING;

  /* If there is one, perform it. Otherwise, seek back at start to start
   * normal playback  */
  if (!event) {
    GST_INFO_OBJECT (ogg, "Seeking back to 0 after duration check");
    event = gst_event_new_seek (1.0, GST_FORMAT_BYTES,
        GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH,
        GST_SEEK_TYPE_SET, 1, GST_SEEK_TYPE_SET, GST_CLOCK_TIME_NONE);
    /* drop everything until this seek event completed.  We can't wait until the
     * seek thread sets this because there would be race between receiving e.g.
     * an EOS or any data and the seek thread actually picking up the seek. */
    ogg->seek_event_drop_till = gst_event_get_seqnum (event);
  }
  gst_event_replace (&ogg->seek_event, event);
  gst_event_unref (event);
  GST_PUSH_UNLOCK (ogg);
  g_mutex_lock (&ogg->seek_event_mutex);
  g_cond_broadcast (&ogg->seek_event_cond);
  g_mutex_unlock (&ogg->seek_event_mutex);

  return GST_FLOW_OK;
}

static float
gst_ogg_demux_estimate_seek_quality (GstOggDemux * ogg)
{
  GstClockTimeDiff diff;        /* how far from the goal we ended up */
  GstClockTimeDiff dist;        /* how far we moved this iteration */
  float seek_quality;

  if (ogg->push_prev_seek_time == GST_CLOCK_TIME_NONE) {
    /* for the first seek, we pretend we got a good seek,
       as we don't have a previous seek yet */
    return 1.0f;
  }

  /* We take a guess at how good the last seek was at guessing
     the byte target by comparing the amplitude of the last
     seek to the error */
  diff = GST_CLOCK_DIFF (ogg->push_seek_time_target, ogg->push_last_seek_time);
  if (diff < 0)
    diff = -diff;
  dist = GST_CLOCK_DIFF (ogg->push_last_seek_time, ogg->push_prev_seek_time);
  if (dist < 0)
    dist = -dist;

  seek_quality = (dist == 0) ? 0.0f : 1.0f / (1.0f + diff / (float) dist);

  GST_DEBUG_OBJECT (ogg,
      "We moved %" GST_STIME_FORMAT ", we're off by %" GST_STIME_FORMAT
      ", seek quality %f", GST_STIME_ARGS (dist), GST_STIME_ARGS (diff),
      seek_quality);
  return seek_quality;
}

static void
gst_ogg_demux_update_bisection_stats (GstOggDemux * ogg)
{
  int n;

  GST_INFO_OBJECT (ogg, "Bisection needed %d + %d steps",
      ogg->push_bisection_steps[0], ogg->push_bisection_steps[1]);

  for (n = 0; n < 2; ++n) {
    ogg->stats_bisection_steps[n] += ogg->push_bisection_steps[n];
    if (ogg->stats_bisection_max_steps[n] < ogg->push_bisection_steps[n])
      ogg->stats_bisection_max_steps[n] = ogg->push_bisection_steps[n];
  }
  ogg->stats_nbisections++;

  GST_INFO_OBJECT (ogg,
      "So far, %.2f + %.2f bisections needed per seek (max %d + %d)",
      ogg->stats_bisection_steps[0] / (float) ogg->stats_nbisections,
      ogg->stats_bisection_steps[1] / (float) ogg->stats_nbisections,
      ogg->stats_bisection_max_steps[0], ogg->stats_bisection_max_steps[1]);
}

static gboolean
gst_ogg_pad_handle_push_mode_state (GstOggPad * pad, ogg_page * page)
{
  GstOggDemux *ogg = pad->ogg;
  ogg_int64_t granpos = ogg_page_granulepos (page);

  GST_PUSH_LOCK (ogg);
  if (granpos >= 0 && pad->have_type) {
    if (ogg->push_start_time == GST_CLOCK_TIME_NONE) {
      ogg->push_start_time =
          gst_ogg_stream_get_start_time_for_granulepos (&pad->map, granpos);
      GST_DEBUG_OBJECT (ogg, "Stream start time: %" GST_TIME_FORMAT,
          GST_TIME_ARGS (ogg->push_start_time));
    }
    ogg->push_time_offset =
        gst_ogg_stream_get_end_time_for_granulepos (&pad->map, granpos);
    if (ogg->push_time_offset > 0) {
      GST_DEBUG_OBJECT (ogg, "Bitrate since start: %" G_GUINT64_FORMAT,
          gst_util_uint64_scale (ogg->push_byte_offset, 8 * GST_SECOND,
              ogg->push_time_offset));
    }

    if (ogg->push_state == PUSH_DURATION) {
      GstClockTime t =
          gst_ogg_stream_get_end_time_for_granulepos (&pad->map, granpos);

      if (ogg->total_time == GST_CLOCK_TIME_NONE || t > ogg->total_time) {
        GST_DEBUG_OBJECT (ogg, "New total time: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (t));
        ogg->total_time = t;
        ogg->push_time_length = t;
      }

      /* If we're still receiving data from before the seek segment, drop it */
      if (ogg->seek_event_drop_till != 0) {
        GST_PUSH_UNLOCK (ogg);
        return GST_FLOW_SKIP_PUSH;
      }

      /* If we were determining the duration of the stream, we're now done,
         and can get back to sending the original event we delayed.
         We stop a bit before the end of the stream, as if we get a EOS
         event and there is a queue2 upstream (such as when using playbin),
         it will pause the task *after* we come back from the EOS handler,
         so we cannot prevent the pausing by issuing a seek. */
      if (ogg->push_byte_offset >= ogg->push_byte_length) {
        GstMessage *message;
        GstFlowReturn res;

        /* tell the pipeline we've just found out the duration */
        ogg->push_time_length = ogg->total_time;
        GST_INFO_OBJECT (ogg, "New duration found: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (ogg->total_time));
        message = gst_message_new_duration_changed (GST_OBJECT (ogg));
        gst_element_post_message (GST_ELEMENT (ogg), message);

        GST_DEBUG_OBJECT (ogg,
            "We're close enough to the end, and we're scared "
            "to get too close, seeking back to start");

        res = gst_ogg_demux_seek_back_after_push_duration_check_unlock (ogg);
        if (res != GST_FLOW_OK)
          return res;
        return GST_FLOW_SKIP_PUSH;
      } else {
        GST_PUSH_UNLOCK (ogg);
      }
      return GST_FLOW_SKIP_PUSH;
    }
  }

  /* if we're seeking, look at time, and decide what to do */
  if (ogg->push_state != PUSH_PLAYING && ogg->push_state != PUSH_LINEAR2) {
    GstClockTime t;
    gint64 best = -1;
    GstEvent *sevent;
    gboolean close_enough;
    float seek_quality;

    /* ignore -1 granpos when seeking, we want to sync on a real granpos */
    if (granpos < 0) {
      GST_PUSH_UNLOCK (ogg);
      if (ogg_stream_pagein (&pad->map.stream, page) != 0)
        goto choked;
      if (pad->current_granule == -1)
        gst_ogg_demux_setup_first_granule (ogg, pad, page);
      return GST_FLOW_SKIP_PUSH;
    }

    t = gst_ogg_stream_get_end_time_for_granulepos (&pad->map, granpos);

    if (ogg->push_state == PUSH_BISECT1 || ogg->push_state == PUSH_BISECT2) {
      GstClockTime sync_time;

      if (pad->push_sync_time == GST_CLOCK_TIME_NONE)
        pad->push_sync_time = t;
      GST_DEBUG_OBJECT (ogg, "Got PTS %" GST_TIME_FORMAT " for %s",
          GST_TIME_ARGS (t), gst_ogg_stream_get_media_type (&pad->map));
      sync_time = gst_ogg_demux_collect_sync_time (ogg, ogg->building_chain);
      if (sync_time == GST_CLOCK_TIME_NONE) {
        GST_PUSH_UNLOCK (ogg);
        GST_DEBUG_OBJECT (ogg,
            "Not enough timing info collected for sync, waiting for more");
        if (ogg_stream_pagein (&pad->map.stream, page) != 0)
          goto choked;
        if (pad->current_granule == -1)
          gst_ogg_demux_setup_first_granule (ogg, pad, page);
        return GST_FLOW_SKIP_PUSH;
      }
      ogg->push_last_seek_time = sync_time;

      GST_DEBUG_OBJECT (ogg,
          "Bisection just seeked at %" G_GINT64_FORMAT ", time %"
          GST_TIME_FORMAT ", target was %" GST_TIME_FORMAT,
          ogg->push_last_seek_offset,
          GST_TIME_ARGS (ogg->push_last_seek_time),
          GST_TIME_ARGS (ogg->push_seek_time_target));

      if (ogg->push_time1 != GST_CLOCK_TIME_NONE) {
        seek_quality = gst_ogg_demux_estimate_seek_quality (ogg);
        GST_DEBUG_OBJECT (ogg,
            "Interval was %" G_GINT64_FORMAT " - %" G_GINT64_FORMAT " (%"
            G_GINT64_FORMAT "), time %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT
            " (%" GST_TIME_FORMAT "), seek quality %f", ogg->push_offset0,
            ogg->push_offset1, ogg->push_offset1 - ogg->push_offset0,
            GST_TIME_ARGS (ogg->push_time0), GST_TIME_ARGS (ogg->push_time1),
            GST_TIME_ARGS (ogg->push_time1 - ogg->push_time0), seek_quality);
      } else {
        /* in a open ended seek, we can't do bisection, so we pretend
           we like our result so far */
        seek_quality = 1.0f;
        GST_DEBUG_OBJECT (ogg,
            "Interval was %" G_GINT64_FORMAT " - %" G_GINT64_FORMAT " (%"
            G_GINT64_FORMAT "), time %" GST_TIME_FORMAT " - unknown",
            ogg->push_offset0, ogg->push_offset1,
            ogg->push_offset1 - ogg->push_offset0,
            GST_TIME_ARGS (ogg->push_time0));
      }
      ogg->push_prev_seek_time = ogg->push_last_seek_time;

      gst_ogg_demux_setup_bisection_bounds (ogg);

      best = gst_ogg_demux_estimate_bisection_target (ogg, seek_quality);

      if (ogg->push_seek_time_target == 0) {
        GST_DEBUG_OBJECT (ogg, "Seeking to 0, deemed close enough");
        close_enough = (ogg->push_last_seek_time == 0);
      } else {
        /* TODO: make this dependent on framerate ? */
        GstClockTime time_threshold = GST_SECOND / 2;
        guint64 byte_threshold =
            (ogg->max_packet_size >
            64 * 1024 ? ogg->max_packet_size : 64 * 1024);

        /* We want to be within half a second before the target,
           or before the target and half less or equal to the max
           packet size left to search in */
        if (time_threshold > ogg->push_seek_time_target)
          time_threshold = ogg->push_seek_time_target;
        close_enough = ogg->push_last_seek_time < ogg->push_seek_time_target
            && (ogg->push_last_seek_time >=
            ogg->push_seek_time_target - time_threshold
            || ogg->push_offset1 <= ogg->push_offset0 + byte_threshold);
        GST_DEBUG_OBJECT (ogg,
            "testing if we're close enough: %" GST_TIME_FORMAT " <= %"
            GST_TIME_FORMAT " < %" GST_TIME_FORMAT ", or %" G_GUINT64_FORMAT
            " <= %" G_GUINT64_FORMAT " ? %s",
            GST_TIME_ARGS (ogg->push_seek_time_target - time_threshold),
            GST_TIME_ARGS (ogg->push_last_seek_time),
            GST_TIME_ARGS (ogg->push_seek_time_target),
            ogg->push_offset1 - ogg->push_offset0, byte_threshold,
            close_enough ? "Yes" : "No");
      }

      if (close_enough || best == ogg->push_last_seek_offset) {
        if (ogg->push_state == PUSH_BISECT1) {
          /* we now know the time segment we'll have to search for
             the second bisection */
          ogg->push_time0 = ogg->push_start_time;
          ogg->push_offset0 = 0;

          GST_DEBUG_OBJECT (ogg,
              "Seek to %" GST_TIME_FORMAT
              " (%lx) done, now gathering pages for all non-sparse streams",
              GST_TIME_ARGS (ogg->push_seek_time_target), (long) granpos);
          ogg->push_state = PUSH_LINEAR1;
        } else {
          /* If we're asked for an accurate seek, we'll go forward till
             we get to the original seek target time, else we'll just drop
             here at the keyframe */
          if (ogg->push_seek_flags & GST_SEEK_FLAG_ACCURATE) {
            GST_INFO_OBJECT (ogg,
                "Seek to keyframe at %" GST_TIME_FORMAT " done (we're at %"
                GST_TIME_FORMAT "), skipping to original target (%"
                GST_TIME_FORMAT ")",
                GST_TIME_ARGS (ogg->push_seek_time_target),
                GST_TIME_ARGS (sync_time),
                GST_TIME_ARGS (ogg->push_seek_time_original_target));
            ogg->push_state = PUSH_LINEAR2;
          } else {
            GST_INFO_OBJECT (ogg, "Seek to keyframe done, playing");

            /* we're synced to the seek target, so flush stream and stuff
               any queued pages into the stream so we start decoding there */
            ogg->push_state = PUSH_PLAYING;
          }
          gst_ogg_demux_update_bisection_stats (ogg);
        }
      }
    } else if (ogg->push_state == PUSH_LINEAR1) {
      if (pad->push_kf_time == GST_CLOCK_TIME_NONE) {
        GstClockTime earliest_keyframe_time;

        gst_ogg_demux_record_keyframe_time (ogg, pad, granpos);
        GST_DEBUG_OBJECT (ogg,
            "Previous keyframe for %s stream at %" GST_TIME_FORMAT,
            gst_ogg_stream_get_media_type (&pad->map),
            GST_TIME_ARGS (pad->push_kf_time));
        earliest_keyframe_time = gst_ogg_demux_get_earliest_keyframe_time (ogg);
        if (earliest_keyframe_time != GST_CLOCK_TIME_NONE) {
          if (earliest_keyframe_time > ogg->push_last_seek_time) {
            GST_INFO_OBJECT (ogg,
                "All non sparse streams now have a previous keyframe time, "
                "and we already decoded it, switching to playing");
            ogg->push_state = PUSH_PLAYING;
            gst_ogg_demux_update_bisection_stats (ogg);
          } else {
            GST_INFO_OBJECT (ogg,
                "All non sparse streams now have a previous keyframe time, "
                "bisecting again to %" GST_TIME_FORMAT,
                GST_TIME_ARGS (earliest_keyframe_time));

            ogg->push_seek_time_target = earliest_keyframe_time;
            ogg->push_offset0 = 0;
            ogg->push_time0 = ogg->push_start_time;
            ogg->push_offset1 = ogg->push_last_seek_offset;
            ogg->push_time1 = ogg->push_last_seek_time;
            ogg->push_prev_seek_time = GST_CLOCK_TIME_NONE;
            ogg->seek_secant = FALSE;
            ogg->seek_undershot = FALSE;

            ogg->push_state = PUSH_BISECT2;
            best = gst_ogg_demux_estimate_bisection_target (ogg, 1.0f);
          }
        }
      }
    }

    if (ogg->push_state == PUSH_BISECT1 || ogg->push_state == PUSH_BISECT2) {
      gint i;

      ogg_sync_reset (&ogg->sync);
      for (i = 0; i < ogg->building_chain->streams->len; i++) {
        GstOggPad *pad =
            g_array_index (ogg->building_chain->streams, GstOggPad *, i);

        pad->push_sync_time = GST_CLOCK_TIME_NONE;
        ogg_stream_reset (&pad->map.stream);
      }

      GST_DEBUG_OBJECT (ogg,
          "seeking to %" G_GINT64_FORMAT " - %" G_GINT64_FORMAT, best,
          (gint64) - 1);
      /* do seek */
      g_assert (best != -1);
      ogg->push_bisection_steps[ogg->push_state == PUSH_BISECT2 ? 1 : 0]++;
      sevent =
          gst_event_new_seek (ogg->push_seek_rate, GST_FORMAT_BYTES,
          ogg->push_seek_flags, GST_SEEK_TYPE_SET, best,
          GST_SEEK_TYPE_NONE, -1);
      gst_event_set_seqnum (sevent, ogg->seqnum);

      gst_event_replace (&ogg->seek_event, sevent);
      gst_event_unref (sevent);
      GST_PUSH_UNLOCK (ogg);
      g_mutex_lock (&ogg->seek_event_mutex);
      g_cond_broadcast (&ogg->seek_event_cond);
      g_mutex_unlock (&ogg->seek_event_mutex);
      return GST_FLOW_SKIP_PUSH;
    }

    if (ogg->push_state != PUSH_PLAYING) {
      GST_PUSH_UNLOCK (ogg);
      return GST_FLOW_SKIP_PUSH;
    }
  }
  GST_PUSH_UNLOCK (ogg);

  return GST_FLOW_OK;

choked:
  {
    GST_WARNING_OBJECT (ogg,
        "ogg stream choked on page (serial %08x), "
        "resetting stream", pad->map.serialno);
    gst_ogg_pad_reset (pad);
    /* we continue to recover */
    return GST_FLOW_SKIP_PUSH;
  }
}

static void
gst_ogg_demux_query_duration_push (GstOggDemux * ogg)
{
  if (!ogg->pullmode && ogg->push_byte_length == -1) {
    GstQuery *query;
    gboolean seekable = FALSE;

    query = gst_query_new_seeking (GST_FORMAT_BYTES);
    if (gst_pad_peer_query (ogg->sinkpad, query))
      gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);
    gst_query_unref (query);

    if (seekable) {
      gint64 length = -1;
      if (!gst_element_query_duration (GST_ELEMENT (ogg), GST_FORMAT_BYTES,
              &length)
          || length <= 0) {
        GST_DEBUG_OBJECT (ogg,
            "Unable to determine stream size, assuming live, seeking disabled");
        ogg->push_disable_seeking = TRUE;
      } else {
        ogg->push_disable_seeking = FALSE;
      }
    } else {
      GST_DEBUG_OBJECT (ogg, "Stream is not seekable, seeking disabled");
      ogg->push_disable_seeking = TRUE;
    }
  }
}

/* submit a page to an oggpad, this function will then submit all
 * the packets in the page.
 */
static GstFlowReturn
gst_ogg_pad_submit_page (GstOggPad * pad, ogg_page * page)
{
  GstFlowReturn result = GST_FLOW_OK;
  GstOggDemux *ogg;
  gboolean continued = FALSE;

  ogg = pad->ogg;

  /* for negative rates we read pages backwards and must therefore be careful
   * with continued pages */
  if (ogg->segment.rate < 0.0) {
    gint npackets;

    continued = ogg_page_continued (page);

    /* number of completed packets in the page */
    npackets = ogg_page_packets (page);
    if (!continued) {
      /* page is not continued so it contains at least one packet start. It's
       * possible that no packet ends on this page (npackets == 0). In that
       * case, the next (continued) page(s) we kept contain the remainder of the
       * packets. We mark npackets=1 to make us start decoding the pages in the
       * remainder of the algorithm. */
      if (npackets == 0)
        npackets = 1;
    }
    GST_LOG_OBJECT (ogg, "continued: %d, %d packets", continued, npackets);

    if (npackets == 0) {
      GST_LOG_OBJECT (ogg, "no decodable packets, we need a previous page");
      goto done;
    }
  }

  gst_ogg_demux_query_duration_push (ogg);

  /* keep track of time in push mode */
  if (!ogg->pullmode) {
    result = gst_ogg_pad_handle_push_mode_state (pad, page);
    if (result == GST_FLOW_SKIP_PUSH)
      return GST_FLOW_OK;
    if (result != GST_FLOW_OK)
      return result;
  }

  if (page->header_len + page->body_len > ogg->max_page_size)
    ogg->max_page_size = page->header_len + page->body_len;

  if (ogg_stream_pagein (&pad->map.stream, page) != 0)
    goto choked;
  if (pad->current_granule == -1)
    gst_ogg_demux_setup_first_granule (ogg, pad, page);

  /* flush all packets in the stream layer, this might not give a packet if
   * the page had no packets finishing on the page (npackets == 0). */
  result = gst_ogg_pad_stream_out (pad, 0);

  if (pad->continued) {
    ogg_packet packet;

    /* now send the continued pages to the stream layer */
    while (pad->continued) {
      ogg_page *p = (ogg_page *) pad->continued->data;

      GST_LOG_OBJECT (ogg, "submitting continued page %p", p);
      if (ogg_stream_pagein (&pad->map.stream, p) != 0)
        goto choked;

      pad->continued = g_list_delete_link (pad->continued, pad->continued);

      /* free the page */
      gst_ogg_page_free (p);
    }

    GST_LOG_OBJECT (ogg, "flushing last continued packet");
    /* flush 1 continued packet in the stream layer */
    result = gst_ogg_pad_stream_out (pad, 1);

    /* flush all remaining packets, we pushed them in the previous round.
     * We don't use _reset() because we still want to get the discont when
     * we submit a next page. */
    while (ogg_stream_packetout (&pad->map.stream, &packet) != 0);
  }

done:
  /* keep continued pages (only in reverse mode) */
  if (continued) {
    ogg_page *p = gst_ogg_page_copy (page);

    GST_LOG_OBJECT (ogg, "keeping continued page %p", p);
    pad->continued = g_list_prepend (pad->continued, p);
  }

  return result;

choked:
  {
    GST_WARNING_OBJECT (ogg,
        "ogg stream choked on page (serial %08x), "
        "resetting stream", pad->map.serialno);
    gst_ogg_pad_reset (pad);
    /* we continue to recover */
    return GST_FLOW_OK;
  }
}


static GstOggChain *
gst_ogg_chain_new (GstOggDemux * ogg)
{
  GstOggChain *chain = g_new0 (GstOggChain, 1);

  GST_DEBUG_OBJECT (ogg, "creating new chain %p", chain);
  chain->ogg = ogg;
  chain->offset = -1;
  chain->bytes = -1;
  chain->have_bos = FALSE;
  chain->streams = g_array_new (FALSE, TRUE, sizeof (GstOggPad *));
  chain->begin_time = GST_CLOCK_TIME_NONE;
  chain->segment_start = GST_CLOCK_TIME_NONE;
  chain->segment_stop = GST_CLOCK_TIME_NONE;
  chain->total_time = GST_CLOCK_TIME_NONE;

  return chain;
}

static void
gst_ogg_chain_free (GstOggChain * chain)
{
  gint i;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    gst_object_unref (pad);
  }
  g_array_free (chain->streams, TRUE);
  g_free (chain);
}

static void
gst_ogg_pad_mark_discont (GstOggPad * pad)
{
  GST_LOG_OBJECT (pad, "Marking discont on pad");
  pad->discont = TRUE;
  pad->map.last_size = 0;
}

static void
gst_ogg_chain_mark_discont (GstOggChain * chain)
{
  gint i;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    gst_ogg_pad_mark_discont (pad);
  }
}

static void
gst_ogg_chain_reset (GstOggChain * chain)
{
  gint i;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    gst_ogg_pad_reset (pad);
  }
}

static GstOggPad *
gst_ogg_chain_new_stream (GstOggChain * chain, guint32 serialno)
{
  GstOggPad *ret;
  gchar *name;

  GST_DEBUG_OBJECT (chain->ogg,
      "creating new stream %08x in chain %p", serialno, chain);

  name = g_strdup_printf ("src_%08x", serialno);
  ret = g_object_new (GST_TYPE_OGG_PAD, "name", name, NULL);
  g_free (name);
  /* we own this one */
  gst_object_ref_sink (ret);

  GST_PAD_DIRECTION (ret) = GST_PAD_SRC;
  gst_ogg_pad_mark_discont (ret);

  ret->chain = chain;
  ret->ogg = chain->ogg;

  ret->map.serialno = serialno;
  if (ogg_stream_init (&ret->map.stream, serialno) != 0)
    goto init_failed;

  GST_DEBUG_OBJECT (chain->ogg,
      "created new ogg src %p for stream with serial %08x", ret, serialno);

  g_array_append_val (chain->streams, ret);
  gst_pad_set_active (GST_PAD_CAST (ret), TRUE);

  return ret;

  /* ERRORS */
init_failed:
  {
    GST_ERROR ("Could not initialize ogg_stream struct for serial %08x",
        serialno);
    gst_object_unref (ret);
    return NULL;
  }
}

static GstOggPad *
gst_ogg_chain_get_stream (GstOggChain * chain, guint32 serialno)
{
  gint i;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    if (pad->map.serialno == serialno)
      return pad;
  }
  return NULL;
}

static gboolean
gst_ogg_chain_has_stream (GstOggChain * chain, guint32 serialno)
{
  return gst_ogg_chain_get_stream (chain, serialno) != NULL;
}

/* signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0
      /* FILL ME */
};

static GstStaticPadTemplate ogg_demux_src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src_%08x",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate ogg_demux_sink_template_factory =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/ogg; audio/ogg; video/ogg; application/kate")
    );

static void gst_ogg_demux_finalize (GObject * object);

static GstFlowReturn gst_ogg_demux_read_chain (GstOggDemux * ogg,
    GstOggChain ** chain);
static GstFlowReturn gst_ogg_demux_read_end_chain (GstOggDemux * ogg,
    GstOggChain * chain);

static gboolean gst_ogg_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static void gst_ogg_demux_loop (GstOggPad * pad);
static GstFlowReturn gst_ogg_demux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static gboolean gst_ogg_demux_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static gboolean gst_ogg_demux_sink_activate_mode (GstPad * sinkpad,
    GstObject * parent, GstPadMode mode, gboolean active);
static GstStateChangeReturn gst_ogg_demux_change_state (GstElement * element,
    GstStateChange transition);

static void gst_ogg_print (GstOggDemux * demux);
static gboolean gst_ogg_demux_plugin_init (GstPlugin * plugin);

#define gst_ogg_demux_parent_class parent_class
G_DEFINE_TYPE (GstOggDemux, gst_ogg_demux, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (oggdemux, "oggdemux", GST_RANK_PRIMARY,
    GST_TYPE_OGG_DEMUX, gst_ogg_demux_plugin_init (plugin));

static void
gst_ogg_demux_class_init (GstOggDemuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gst_element_class_set_static_metadata (gstelement_class,
      "Ogg demuxer", "Codec/Demuxer",
      "demux ogg streams (info about ogg: http://xiph.org)",
      "Wim Taymans <wim@fluendo.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_demux_sink_template_factory);
  gst_element_class_add_static_pad_template (gstelement_class,
      &ogg_demux_src_template_factory);

  gstelement_class->change_state = gst_ogg_demux_change_state;
  gstelement_class->send_event = gst_ogg_demux_receive_event;

  gobject_class->finalize = gst_ogg_demux_finalize;
}

static void
gst_ogg_demux_init (GstOggDemux * ogg)
{
  /* create the sink pad */
  ogg->sinkpad =
      gst_pad_new_from_static_template (&ogg_demux_sink_template_factory,
      "sink");

  gst_pad_set_event_function (ogg->sinkpad, gst_ogg_demux_sink_event);
  gst_pad_set_chain_function (ogg->sinkpad, gst_ogg_demux_chain);
  gst_pad_set_activate_function (ogg->sinkpad, gst_ogg_demux_sink_activate);
  gst_pad_set_activatemode_function (ogg->sinkpad,
      gst_ogg_demux_sink_activate_mode);
  gst_element_add_pad (GST_ELEMENT (ogg), ogg->sinkpad);

  g_mutex_init (&ogg->chain_lock);
  g_mutex_init (&ogg->push_lock);
  g_mutex_init (&ogg->seek_event_mutex);
  g_cond_init (&ogg->seek_event_cond);
  g_cond_init (&ogg->thread_started_cond);

  ogg->chains = g_array_new (FALSE, TRUE, sizeof (GstOggChain *));

  ogg->stats_nbisections = 0;
  ogg->stats_bisection_steps[0] = 0;
  ogg->stats_bisection_steps[1] = 0;
  ogg->stats_bisection_max_steps[0] = 0;
  ogg->stats_bisection_max_steps[1] = 0;

  ogg->newsegment = NULL;
  ogg->seqnum = GST_SEQNUM_INVALID;

  ogg->chunk_size = CHUNKSIZE;
  ogg->flowcombiner = gst_flow_combiner_new ();
}

static void
gst_ogg_demux_finalize (GObject * object)
{
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (object);

  g_array_free (ogg->chains, TRUE);
  g_mutex_clear (&ogg->chain_lock);
  g_mutex_clear (&ogg->push_lock);
  g_cond_clear (&ogg->seek_event_cond);
  g_cond_clear (&ogg->thread_started_cond);
  g_mutex_clear (&ogg->seek_event_mutex);

  ogg_sync_clear (&ogg->sync);

  if (ogg->newsegment)
    gst_event_unref (ogg->newsegment);

  gst_flow_combiner_free (ogg->flowcombiner);

  if (ogg->building_chain)
    gst_ogg_chain_free (ogg->building_chain);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_ogg_demux_reset_streams (GstOggDemux * ogg)
{
  GstOggChain *chain;
  guint i;

  chain = ogg->current_chain;
  if (chain == NULL)
    return;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *stream = g_array_index (chain->streams, GstOggPad *, i);

    stream->start_time = -1;
    stream->map.accumulated_granule = 0;
    stream->current_granule = -1;
    stream->keyframe_granule = -1;
  }
  ogg->building_chain = chain;
  GST_DEBUG_OBJECT (ogg, "Resetting current chain");
  ogg->current_chain = NULL;
  ogg->resync = TRUE;
  gst_ogg_chain_mark_discont (chain);

  ogg->chunk_size = CHUNKSIZE;
}

static gboolean
gst_ogg_demux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean res;
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      if (ogg->seqnum != GST_SEQNUM_INVALID) {
        event = gst_event_make_writable (event);
        gst_event_set_seqnum (event, ogg->seqnum);
      }
      res = gst_ogg_demux_send_event (ogg, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (ogg, "got a flush stop event");
      ogg_sync_reset (&ogg->sync);
      if (ogg->seqnum != GST_SEQNUM_INVALID) {
        event = gst_event_make_writable (event);
        gst_event_set_seqnum (event, ogg->seqnum);
      }
      res = gst_ogg_demux_send_event (ogg, event);
      if (ogg->pullmode || ogg->push_state != PUSH_DURATION) {
        /* it's starting to feel reaaaally dirty :(
           if we're on a spliced seek to get duration, don't reset streams,
           we'll need them for the delayed seek */
        gst_ogg_demux_reset_streams (ogg);
      }
      break;
    case GST_EVENT_SEGMENT:
      GST_DEBUG_OBJECT (ogg, "got a new segment event");
      {
        GstSegment segment;
        gboolean update;

        gst_event_copy_segment (event, &segment);

        if (segment.format == GST_FORMAT_BYTES) {
          GST_PUSH_LOCK (ogg);
          ogg->push_byte_offset = segment.start;
          ogg->push_last_seek_offset = segment.start;

          if (gst_event_get_seqnum (event) == ogg->seqnum) {
            GstSeekType stop_type = GST_SEEK_TYPE_NONE;
            if (ogg->push_seek_time_original_stop != -1)
              stop_type = GST_SEEK_TYPE_SET;
            gst_segment_do_seek (&ogg->segment, ogg->push_seek_rate,
                GST_FORMAT_TIME, ogg->push_seek_flags, GST_SEEK_TYPE_SET,
                ogg->push_seek_time_original_target, stop_type,
                ogg->push_seek_time_original_stop, &update);
          } else if (ogg->seqnum == GST_SEQNUM_INVALID) {
            ogg->seqnum = GST_EVENT_SEQNUM (event);
          }

          if (!ogg->pullmode && !(ogg->push_seek_flags & GST_SEEK_FLAG_FLUSH)) {
            int i;
            GstOggChain *chain = ogg->current_chain;

            ogg->push_seek_flags = 0;
            if (!chain) {
              /* This will happen when we bisect, as we clear the chain when
                 we do the first seek. On subsequent ones, we just reset the
                 ogg sync object as we already reset the chain */
              GST_DEBUG_OBJECT (ogg, "No chain, just resetting ogg sync");
              ogg_sync_reset (&ogg->sync);
            } else {
              /* reset pad push mode seeking state */
              for (i = 0; i < chain->streams->len; i++) {
                GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);
                pad->push_kf_time = GST_CLOCK_TIME_NONE;
                pad->push_sync_time = GST_CLOCK_TIME_NONE;
              }
              ogg_sync_reset (&ogg->sync);
              gst_ogg_demux_reset_streams (ogg);
            }
          }

          if (!ogg->pullmode) {
            if (ogg->seek_event_drop_till == gst_event_get_seqnum (event)) {
              GST_DEBUG_OBJECT (ogg,
                  "Got event seqnum %u, stopping dropping (ogg->seqnum:%u)",
                  ogg->seek_event_drop_till, ogg->seqnum);
              ogg->seek_event_drop_till = 0;
            }
          }
          GST_PUSH_UNLOCK (ogg);
        } else {
          GST_WARNING_OBJECT (ogg, "unexpected segment format: %s",
              gst_format_get_name (segment.format));
        }
      }

      gst_event_unref (event);
      res = TRUE;
      break;
    case GST_EVENT_EOS:
    {
      gboolean drop = FALSE;
      GST_DEBUG_OBJECT (ogg, "got an EOS event");
      GST_PUSH_LOCK (ogg);
      if (ogg->push_state == PUSH_DURATION) {
        GST_DEBUG_OBJECT (ogg, "Got EOS while determining length");
        res = gst_ogg_demux_seek_back_after_push_duration_check_unlock (ogg);
        if (res != GST_FLOW_OK) {
          GST_DEBUG_OBJECT (ogg, "Error seeking back after duration check: %d",
              res);
        }
        gst_event_unref (event);
        res = TRUE;
        break;
      } else {
        if (ogg->seek_event_drop_till > 0) {
          GST_DEBUG_OBJECT (ogg, "Dropping EOS (seqnum:%u) because we have "
              "a pending seek (seqnum:%u)", gst_event_get_seqnum (event),
              ogg->seek_event_drop_till);
          drop = TRUE;
        }
        GST_PUSH_UNLOCK (ogg);
        res = TRUE;
      }
      if (!drop)
        res = gst_ogg_demux_send_event (ogg, event);
      else
        gst_event_unref (event);
      if (ogg->current_chain == NULL) {
        GST_WARNING_OBJECT (ogg,
            "EOS while trying to retrieve chain, seeking disabled");
        ogg->push_disable_seeking = TRUE;
        res = TRUE;
      }
      break;
    }
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}

/* submit the given buffer to the ogg sync */
static GstFlowReturn
gst_ogg_demux_submit_buffer (GstOggDemux * ogg, GstBuffer * buffer)
{
  gsize size;
  gchar *oggbuffer;
  GstFlowReturn ret = GST_FLOW_OK;

  size = gst_buffer_get_size (buffer);
  GST_DEBUG_OBJECT (ogg, "submitting %" G_GSIZE_FORMAT " bytes", size);
  if (G_UNLIKELY (size == 0))
    goto done;

  oggbuffer = ogg_sync_buffer (&ogg->sync, size);
  if (G_UNLIKELY (oggbuffer == NULL))
    goto no_buffer;

  gst_buffer_extract (buffer, 0, oggbuffer, size);

  if (G_UNLIKELY (ogg_sync_wrote (&ogg->sync, size) < 0))
    goto write_failed;

  if (!ogg->pullmode) {
    GST_PUSH_LOCK (ogg);
    ogg->push_byte_offset += size;
    GST_PUSH_UNLOCK (ogg);
  }

done:
  gst_buffer_unref (buffer);

  return ret;

  /* ERRORS */
no_buffer:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE,
        (NULL), ("failed to get ogg sync buffer"));
    ret = GST_FLOW_ERROR;
    goto done;
  }
write_failed:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE, (NULL),
        ("failed to write %" G_GSIZE_FORMAT " bytes to the sync buffer", size));
    ret = GST_FLOW_ERROR;
    goto done;
  }
}

/* in random access mode this code updates the current read position
 * and resets the ogg sync buffer so that the next read will happen
 * from this new location.
 */
static void
gst_ogg_demux_seek (GstOggDemux * ogg, gint64 offset)
{
  GST_LOG_OBJECT (ogg, "seeking to %" G_GINT64_FORMAT, offset);

  ogg->offset = offset;
  ogg->read_offset = offset;
  ogg_sync_reset (&ogg->sync);
}

/* read more data from the current offset and submit to
 * the ogg sync layer.
 */
static GstFlowReturn
gst_ogg_demux_get_data (GstOggDemux * ogg, gint64 end_offset)
{
  GstFlowReturn ret;
  GstBuffer *buffer;
  gchar *oggbuffer;
  gsize size;

  GST_LOG_OBJECT (ogg,
      "get data %" G_GINT64_FORMAT " %" G_GINT64_FORMAT " %" G_GINT64_FORMAT,
      ogg->read_offset, ogg->length, end_offset);

  if (end_offset > 0 && ogg->read_offset >= end_offset)
    goto boundary_reached;

  if (ogg->read_offset == ogg->length)
    goto eos;

  oggbuffer = ogg_sync_buffer (&ogg->sync, ogg->chunk_size);
  if (G_UNLIKELY (oggbuffer == NULL))
    goto no_buffer;

  buffer =
      gst_buffer_new_wrapped_full (0, oggbuffer, ogg->chunk_size, 0,
      ogg->chunk_size, NULL, NULL);

  ret =
      gst_pad_pull_range (ogg->sinkpad, ogg->read_offset, ogg->chunk_size,
      &buffer);
  if (ret != GST_FLOW_OK)
    goto error;

  size = gst_buffer_get_size (buffer);

  if (G_UNLIKELY (ogg_sync_wrote (&ogg->sync, size) < 0))
    goto write_failed;

  ogg->read_offset += size;
  gst_buffer_unref (buffer);

  return ret;

  /* ERROR */
boundary_reached:
  {
    GST_LOG_OBJECT (ogg, "reached boundary");
    return GST_FLOW_LIMIT;
  }
eos:
  {
    GST_LOG_OBJECT (ogg, "reached EOS");
    return GST_FLOW_EOS;
  }
no_buffer:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE,
        (NULL), ("failed to get ogg sync buffer"));
    return GST_FLOW_ERROR;
  }
error:
  {
    GST_WARNING_OBJECT (ogg, "got %d (%s) from pull range", ret,
        gst_flow_get_name (ret));
    gst_buffer_unref (buffer);
    return ret;
  }
write_failed:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE, (NULL),
        ("failed to write %" G_GSIZE_FORMAT " bytes to the sync buffer", size));
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }
}

/* Read the next page from the current offset.
 * boundary: number of bytes ahead we allow looking for;
 * -1 if no boundary
 *
 * @offset will contain the offset the next page starts at when this function
 * returns GST_FLOW_OK.
 *
 * GST_FLOW_EOS is returned on EOS.
 *
 * GST_FLOW_LIMIT is returned when we did not find a page before the
 * boundary. If @boundary is -1, this is never returned.
 *
 * Any other error returned while retrieving data from the peer is returned as
 * is.
 */
static GstFlowReturn
gst_ogg_demux_get_next_page (GstOggDemux * ogg, ogg_page * og,
    gint64 boundary, gint64 * offset)
{
  gint64 end_offset = -1;
  GstFlowReturn ret;

  GST_LOG_OBJECT (ogg,
      "get next page, current offset %" G_GINT64_FORMAT ", bytes boundary %"
      G_GINT64_FORMAT, ogg->offset, boundary);

  if (boundary >= 0)
    end_offset = ogg->offset + boundary;

  while (TRUE) {
    glong more;

    if (end_offset > 0 && ogg->offset >= end_offset)
      goto boundary_reached;

    more = ogg_sync_pageseek (&ogg->sync, og);

    GST_LOG_OBJECT (ogg, "pageseek gave %ld", more);

    if (more < 0) {
      /* skipped n bytes */
      ogg->offset -= more;
      GST_LOG_OBJECT (ogg, "skipped %ld bytes, offset %" G_GINT64_FORMAT,
          more, ogg->offset);
    } else if (more == 0) {
      /* we need more data */
      if (boundary == 0)
        goto boundary_reached;

      GST_LOG_OBJECT (ogg, "need more data");
      ret = gst_ogg_demux_get_data (ogg, end_offset);
      if (ret != GST_FLOW_OK)
        break;
    } else {
      gint64 res_offset = ogg->offset;

      /* got a page.  Return the offset at the page beginning,
         advance the internal offset past the page end */
      if (offset)
        *offset = res_offset;
      ret = GST_FLOW_OK;

      ogg->offset += more;

      GST_LOG_OBJECT (ogg,
          "got page at %" G_GINT64_FORMAT ", serial %08x, end at %"
          G_GINT64_FORMAT ", granule %" G_GINT64_FORMAT, res_offset,
          ogg_page_serialno (og), ogg->offset,
          (gint64) ogg_page_granulepos (og));
      break;
    }
  }
  GST_LOG_OBJECT (ogg, "returning %d", ret);

  return ret;

  /* ERRORS */
boundary_reached:
  {
    GST_LOG_OBJECT (ogg,
        "offset %" G_GINT64_FORMAT " >= end_offset %" G_GINT64_FORMAT,
        ogg->offset, end_offset);
    return GST_FLOW_LIMIT;
  }
}

/* from the current offset, find the previous page, seeking backwards
 * until we find the page. 
 */
static GstFlowReturn
gst_ogg_demux_get_prev_page (GstOggDemux * ogg, ogg_page * og, gint64 * offset)
{
  GstFlowReturn ret;
  gint64 begin = ogg->offset;
  gint64 end = begin;
  gint64 cur_offset = -1;

  GST_LOG_OBJECT (ogg, "getting page before %" G_GINT64_FORMAT, begin);

  while (cur_offset == -1) {
    begin -= ogg->chunk_size;
    if (begin < 0)
      begin = 0;

    /* seek ogg->chunk_size back */
    GST_LOG_OBJECT (ogg, "seeking back to %" G_GINT64_FORMAT, begin);
    gst_ogg_demux_seek (ogg, begin);

    /* now continue reading until we run out of data, if we find a page
     * start, we save it. It might not be the final page as there could be
     * another page after this one. */
    while (ogg->offset < end) {
      gint64 new_offset, boundary;

      /* An Ogg page cannot be more than a bit less than 64 KB, so we can
         bound the boundary to that size when searching backwards if we
         haven't found a page yet. So the most we have to look at is twice
         the max page size, which is the worst case if we start scanning
         just after a large page, after which also lies a large page. */
      boundary = end - ogg->offset;
      if (boundary > 2 * MAX_OGG_PAGE_SIZE)
        boundary = 2 * MAX_OGG_PAGE_SIZE;

      ret = gst_ogg_demux_get_next_page (ogg, og, boundary, &new_offset);
      /* we hit the upper limit, offset contains the last page start */
      if (ret == GST_FLOW_LIMIT) {
        GST_LOG_OBJECT (ogg, "hit limit");
        break;
      }
      /* something went wrong */
      if (ret == GST_FLOW_EOS) {
        new_offset = 0;
        GST_LOG_OBJECT (ogg, "got unexpected");
        /* We hit EOS. */
        goto beach;
      } else if (ret != GST_FLOW_OK) {
        GST_LOG_OBJECT (ogg, "got error %d", ret);
        return ret;
      }

      GST_LOG_OBJECT (ogg, "found page at %" G_GINT64_FORMAT, new_offset);

      /* offset is next page start */
      cur_offset = new_offset;
    }
  }

  GST_LOG_OBJECT (ogg, "found previous page at %" G_GINT64_FORMAT, cur_offset);

  /* we have the offset.  Actually snork and hold the page now */
  gst_ogg_demux_seek (ogg, cur_offset);
  ret = gst_ogg_demux_get_next_page (ogg, og, -1, NULL);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (ogg, "can't get last page at %" G_GINT64_FORMAT,
        cur_offset);
    /* this shouldn't be possible */
    return ret;
  }

  if (offset)
    *offset = cur_offset;

beach:
  return ret;
}

static gboolean
gst_ogg_demux_deactivate_current_chain (GstOggDemux * ogg)
{
  gint i;
  GstOggChain *chain = ogg->current_chain;

  if (chain == NULL)
    return TRUE;

  GST_DEBUG_OBJECT (ogg, "deactivating chain %p", chain);

  /* send EOS on all the pads */
  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);
    GstEvent *event;

    if (!pad->added)
      continue;

    event = gst_event_new_eos ();
    gst_event_set_seqnum (event, ogg->seqnum);
    gst_pad_push_event (GST_PAD_CAST (pad), event);

    GST_DEBUG_OBJECT (ogg, "removing pad %" GST_PTR_FORMAT, pad);

    /* deactivate first */
    gst_pad_set_active (GST_PAD_CAST (pad), FALSE);

    gst_flow_combiner_remove_pad (ogg->flowcombiner, GST_PAD_CAST (pad));

    gst_element_remove_pad (GST_ELEMENT (ogg), GST_PAD_CAST (pad));

    pad->added = FALSE;
  }

  /* if we cannot seek back to the chain, we can destroy the chain 
   * completely */
  if (!ogg->pullmode) {
    if (ogg->building_chain == chain)
      ogg->building_chain = NULL;
    ogg->current_chain = NULL;
    gst_ogg_chain_free (chain);
  }

  return TRUE;
}

static GstCaps *
gst_ogg_demux_set_header_on_caps (GstOggDemux * ogg, GstCaps * caps,
    GList * headers)
{
  GstStructure *structure;
  GValue array = { 0 };

  GST_LOG_OBJECT (ogg, "caps: %" GST_PTR_FORMAT, caps);

  if (G_UNLIKELY (!caps))
    return NULL;
  if (G_UNLIKELY (!headers))
    return caps;

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  g_value_init (&array, GST_TYPE_ARRAY);

  while (headers) {
    GValue value = { 0 };
    GstBuffer *buffer;
    ogg_packet *op = headers->data;
    g_assert (op);
    buffer = gst_buffer_new_and_alloc (op->bytes);
    if (op->bytes)
      gst_buffer_fill (buffer, 0, op->packet, op->bytes);
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_HEADER);
    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_take_buffer (&value, buffer);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);
    headers = headers->next;
  }

  gst_structure_take_value (structure, "streamheader", &array);
  GST_LOG_OBJECT (ogg, "here are the newly set caps: %" GST_PTR_FORMAT, caps);

  return caps;
}

static void
gst_ogg_demux_push_queued_buffers (GstOggDemux * ogg, GstOggPad * pad)
{
  GList *walk;

  /* push queued packets */
  for (walk = pad->map.queued; walk; walk = g_list_next (walk)) {
    ogg_packet *p = walk->data;

    gst_ogg_demux_chain_peer (pad, p, TRUE);
    _ogg_packet_free (p);
  }
  /* and free the queued buffers */
  g_list_free (pad->map.queued);
  pad->map.queued = NULL;
}

static gboolean
gst_ogg_demux_activate_chain (GstOggDemux * ogg, GstOggChain * chain,
    GstEvent * event)
{
  gint i;
  gint bitrate, idx_bitrate;

  g_return_val_if_fail (chain != NULL, FALSE);

  if (chain == ogg->current_chain) {
    if (event)
      gst_event_unref (event);

    for (i = 0; i < chain->streams->len; i++) {
      GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);
      gst_ogg_demux_push_queued_buffers (ogg, pad);
    }
    return TRUE;
  }


  GST_DEBUG_OBJECT (ogg, "activating chain %p", chain);

  bitrate = idx_bitrate = 0;

  /* first add the pads */
  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad;
    GstEvent *ss_event;
    gchar *stream_id;

    pad = g_array_index (chain->streams, GstOggPad *, i);

    if (pad->map.idx_bitrate)
      idx_bitrate = MAX (idx_bitrate, pad->map.idx_bitrate);

    bitrate += pad->map.bitrate;

    /* mark discont */
    gst_ogg_pad_mark_discont (pad);
    pad->last_ret = GST_FLOW_OK;

    if (pad->map.is_skeleton || pad->map.is_cmml || pad->added
        || !pad->map.caps)
      continue;

    GST_DEBUG_OBJECT (ogg, "adding pad %" GST_PTR_FORMAT, pad);

    /* activate first */
    gst_pad_set_active (GST_PAD_CAST (pad), TRUE);

    stream_id =
        gst_pad_create_stream_id_printf (GST_PAD (pad), GST_ELEMENT_CAST (ogg),
        "%08x", pad->map.serialno);
    ss_event =
        gst_pad_get_sticky_event (ogg->sinkpad, GST_EVENT_STREAM_START, 0);
    if (ss_event) {
      if (gst_event_parse_group_id (ss_event, &ogg->group_id))
        ogg->have_group_id = TRUE;
      else
        ogg->have_group_id = FALSE;
      gst_event_unref (ss_event);
    } else if (!ogg->have_group_id) {
      ogg->have_group_id = TRUE;
      ogg->group_id = gst_util_group_id_next ();
    }
    ss_event = gst_event_new_stream_start (stream_id);
    if (ogg->have_group_id)
      gst_event_set_group_id (ss_event, ogg->group_id);

    gst_pad_push_event (GST_PAD (pad), ss_event);
    g_free (stream_id);

    /* Set headers on caps */
    pad->map.caps =
        gst_ogg_demux_set_header_on_caps (ogg, pad->map.caps, pad->map.headers);
    gst_pad_set_caps (GST_PAD_CAST (pad), pad->map.caps);

    gst_element_add_pad (GST_ELEMENT (ogg), GST_PAD_CAST (pad));
    pad->added = TRUE;
    gst_flow_combiner_add_pad (ogg->flowcombiner, GST_PAD_CAST (pad));
  }
  /* prefer the index bitrate over the ones encoded in the streams */
  ogg->bitrate = (idx_bitrate ? idx_bitrate : bitrate);

  /* after adding the new pads, remove the old pads */
  gst_ogg_demux_deactivate_current_chain (ogg);

  GST_DEBUG_OBJECT (ogg, "Setting current chain to %p", chain);
  ogg->current_chain = chain;

  /* we are finished now */
  gst_element_no_more_pads (GST_ELEMENT (ogg));

  GST_DEBUG_OBJECT (ogg, "starting chain");

  /* then send out any headers and queued packets */
  for (i = 0; i < chain->streams->len; i++) {
    GList *walk;
    GstOggPad *pad;
    GstTagList *tags;

    pad = g_array_index (chain->streams, GstOggPad *, i);

    /* Skip pads that were not added, e.g. Skeleton streams */
    if (!pad->added)
      continue;

    /* FIXME, must be sent from the streaming thread */
    if (event)
      gst_pad_push_event (GST_PAD_CAST (pad), gst_event_ref (event));

    /* FIXME also streaming thread */
    if (pad->map.taglist) {
      GST_DEBUG_OBJECT (ogg, "pushing tags");
      gst_pad_push_event (GST_PAD_CAST (pad),
          gst_event_new_tag (pad->map.taglist));
      pad->map.taglist = NULL;
    }

    tags = gst_tag_list_new (GST_TAG_CONTAINER_FORMAT, "Ogg", NULL);
    gst_tag_list_set_scope (tags, GST_TAG_SCOPE_GLOBAL);
    gst_pad_push_event (GST_PAD (pad), gst_event_new_tag (tags));

    GST_DEBUG_OBJECT (ogg, "pushing headers");
    /* push headers */
    for (walk = pad->map.headers; walk; walk = g_list_next (walk)) {
      ogg_packet *p = walk->data;

      gst_ogg_demux_chain_peer (pad, p, TRUE);
    }

    GST_DEBUG_OBJECT (ogg, "pushing queued buffers");
    gst_ogg_demux_push_queued_buffers (ogg, pad);
  }

  if (event)
    gst_event_unref (event);

  return TRUE;
}

static gboolean
do_binary_search (GstOggDemux * ogg, GstOggChain * chain, gint64 begin,
    gint64 end, gint64 begintime, gint64 endtime, gint64 target,
    gint64 * offset, gboolean only_serial_no, gint serialno)
{
  gint64 best;
  GstFlowReturn ret;
  gint64 result = 0;

  best = begin;

  GST_DEBUG_OBJECT (ogg,
      "chain offset %" G_GINT64_FORMAT ", end offset %" G_GINT64_FORMAT,
      begin, end);
  GST_DEBUG_OBJECT (ogg,
      "chain begin time %" GST_TIME_FORMAT ", end time %" GST_TIME_FORMAT,
      GST_TIME_ARGS (begintime), GST_TIME_ARGS (endtime));
  GST_DEBUG_OBJECT (ogg, "target %" GST_TIME_FORMAT, GST_TIME_ARGS (target));

  /* perform the seek */
  while (begin < end) {
    gint64 bisect;

    if ((end - begin < ogg->chunk_size) || (endtime == begintime)) {
      bisect = begin;
    } else {
      /* take a (pretty decent) guess, avoiding overflow */
      gint64 rate = (end - begin) * GST_MSECOND / (endtime - begintime);

      bisect =
          (target - begintime) / GST_MSECOND * rate + begin - ogg->chunk_size;

      if (bisect <= begin)
        bisect = begin;
      GST_DEBUG_OBJECT (ogg, "Initial guess: %" G_GINT64_FORMAT, bisect);
    }
    gst_ogg_demux_seek (ogg, bisect);

    while (begin < end) {
      ogg_page og;

      GST_DEBUG_OBJECT (ogg,
          "after seek, bisect %" G_GINT64_FORMAT ", begin %" G_GINT64_FORMAT
          ", end %" G_GINT64_FORMAT, bisect, begin, end);

      ret = gst_ogg_demux_get_next_page (ogg, &og, end - ogg->offset, &result);
      GST_LOG_OBJECT (ogg, "looking for next page returned %" G_GINT64_FORMAT,
          result);

      if (ret == GST_FLOW_LIMIT) {
        /* we hit the upper limit, go back a bit */
        if (bisect <= begin + 1) {
          end = begin;          /* found it */
        } else {
          if (bisect == 0)
            goto seek_error;

          bisect -= ogg->chunk_size;
          if (bisect <= begin)
            bisect = begin + 1;

          gst_ogg_demux_seek (ogg, bisect);
        }
      } else if (ret == GST_FLOW_OK) {
        /* found offset of next ogg page */
        gint64 granulepos;
        GstClockTime granuletime;
        GstOggPad *pad;

        /* get the granulepos */
        GST_LOG_OBJECT (ogg, "found next ogg page at %" G_GINT64_FORMAT,
            result);
        granulepos = ogg_page_granulepos (&og);
        if (granulepos == -1) {
          GST_LOG_OBJECT (ogg, "granulepos of next page is -1");
          continue;
        }

        /* Avoid seeking to an incorrect granuletime by only considering 
           the stream for which we found the earliest time */
        if (only_serial_no && ogg_page_serialno (&og) != serialno)
          continue;

        /* get the stream */
        pad = gst_ogg_chain_get_stream (chain, ogg_page_serialno (&og));
        if (pad == NULL || pad->map.is_skeleton)
          continue;

        /* convert granulepos to time */
        granuletime = gst_ogg_stream_get_end_time_for_granulepos (&pad->map,
            granulepos);
        if (granuletime < pad->start_time)
          continue;

        GST_LOG_OBJECT (ogg, "granulepos %" G_GINT64_FORMAT " maps to PTS %"
            GST_TIME_FORMAT, granulepos, GST_TIME_ARGS (granuletime));

        granuletime -= pad->start_time;
        granuletime += chain->begin_time;

        GST_DEBUG_OBJECT (ogg,
            "found page with granule %" G_GINT64_FORMAT " and time %"
            GST_TIME_FORMAT, granulepos, GST_TIME_ARGS (granuletime));

        if (granuletime < target) {
          best = result;        /* raw offset of packet with granulepos */
          begin = ogg->offset;  /* raw offset of next page */
          begintime = granuletime;

          bisect = begin;       /* *not* begin + 1 */
        } else {
          if (bisect <= begin + 1) {
            end = begin;        /* found it */
          } else {
            if (end == ogg->offset) {   /* we're pretty close - we'd be stuck in */
              end = result;
              bisect -= ogg->chunk_size;        /* an endless loop otherwise. */
              if (bisect <= begin)
                bisect = begin + 1;
              gst_ogg_demux_seek (ogg, bisect);
            } else {
              end = result;
              endtime = granuletime;
              break;
            }
          }
        }
      } else
        goto seek_error;
    }
  }
  GST_DEBUG_OBJECT (ogg, "seeking to %" G_GINT64_FORMAT, best);
  gst_ogg_demux_seek (ogg, best);
  *offset = best;

  return TRUE;

  /* ERRORS */
seek_error:
  {
    GST_DEBUG_OBJECT (ogg, "got a seek error");
    return FALSE;
  }
}

static gboolean
do_index_search (GstOggDemux * ogg, GstOggChain * chain, gint64 begin,
    gint64 end, gint64 begintime, gint64 endtime, gint64 target,
    gint64 * p_offset, gint64 * p_timestamp)
{
  guint i;
  guint64 timestamp, offset;
  guint64 r_timestamp, r_offset;
  gboolean result = FALSE;

  target -= begintime;

  r_offset = -1;
  r_timestamp = -1;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

    timestamp = target;
    if (gst_ogg_map_search_index (&pad->map, TRUE, &timestamp, &offset)) {
      GST_INFO ("found %" G_GUINT64_FORMAT " at offset %" G_GUINT64_FORMAT,
          timestamp, offset);

      if (r_offset == -1 || offset < r_offset) {
        r_offset = offset;
        r_timestamp = timestamp;
      }
      result |= TRUE;
    }
  }

  if (p_timestamp)
    *p_timestamp = r_timestamp;
  if (p_offset)
    *p_offset = r_offset;

  return result;
}

/*
 * do seek to time @position, return FALSE or chain and TRUE
 */
static gboolean
gst_ogg_demux_do_seek (GstOggDemux * ogg, GstSegment * segment,
    gboolean accurate, gboolean keyframe, GstOggChain ** rchain)
{
  guint64 position;
  GstOggChain *chain = NULL;
  gint64 begin, end;
  gint64 begintime, endtime;
  gint64 target, keytarget;
  gint64 best;
  gint64 total;
  gint64 result = 0;
  GstFlowReturn ret;
  gint i, pending;
  gint serialno = 0;
  gboolean found_keyframe = FALSE;
  GstClockTime ts, first_ts = GST_CLOCK_TIME_NONE;

  position = segment->position;

  /* first find the chain to search in */
  total = ogg->total_time;
  if (ogg->chains->len == 0)
    goto no_chains;

  for (i = ogg->chains->len - 1; i >= 0; i--) {
    chain = g_array_index (ogg->chains, GstOggChain *, i);
    total -= chain->total_time;
    if (position >= total)
      break;
  }

  /* first step, locate page containing the required data */
  begin = chain->offset;
  end = chain->end_offset;
  begintime = chain->begin_time;
  endtime = begintime + chain->total_time;
  target = position - total + begintime;

  if (!do_binary_search (ogg, chain, begin, end, begintime, endtime, target,
          &best, FALSE, 0))
    goto seek_error;

  /* second step: find pages for all relevant streams. We use the
   * keyframe_granule to keep track of which ones we saw. If we have
   * seen a page for each stream we can calculate the positions of
   * each keyframe.
   * Relevant streams are defined as those streams which are not
   * Skeleton (which only has header pages). Discontinuous streams
   * such as Kate and CMML are currently excluded, as they could
   * cause performance issues if there are few pages in the area.
   * TODO: We might want to include them on a flag, if we want to
   * not miss a subtitle (Kate has repeat packets for this purpose,
   * but a stream does not have to use them). */
  pending = chain->streams->len;
  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);
    if (!pad) {
      GST_WARNING_OBJECT (ogg, "No pad at index %d", i);
      pending--;
      continue;
    }
    if (pad->map.is_skeleton) {
      GST_DEBUG_OBJECT (ogg, "Not finding pages for Skeleton stream %08x",
          pad->map.serialno);
      pending--;
      continue;
    }
    if (pad->map.is_sparse) {
      GST_DEBUG_OBJECT (ogg, "Not finding pages for sparse stream %08x (%s)",
          pad->map.serialno, gst_ogg_stream_get_media_type (&pad->map));
      pending--;
      continue;
    }
  }
  GST_DEBUG_OBJECT (ogg, "find keyframes for %d/%d streams", pending,
      chain->streams->len);

  /* figure out where the keyframes are */
  keytarget = target;

  while (TRUE) {
    ogg_page og;
    gint64 granulepos;
    GstOggPad *pad;
    GstClockTime keyframe_time, granule_time;

    ret = gst_ogg_demux_get_next_page (ogg, &og, end - ogg->offset, &result);
    GST_LOG_OBJECT (ogg, "looking for next page returned %" G_GINT64_FORMAT,
        result);
    if (ret == GST_FLOW_LIMIT) {
      GST_LOG_OBJECT (ogg, "reached limit");
      break;
    } else if (ret != GST_FLOW_OK)
      goto seek_error;

    /* get the stream */
    pad = gst_ogg_chain_get_stream (chain, ogg_page_serialno (&og));
    if (pad == NULL)
      continue;

    if (pad->map.is_skeleton || pad->map.is_sparse)
      goto next;

    granulepos = ogg_page_granulepos (&og);
    if (granulepos == -1 || granulepos == 0) {
      GST_LOG_OBJECT (ogg, "granulepos of next page is -1");
      continue;
    }

    /* We have a valid granpos, and we bail out when the time since the
       first seen time to the time corresponding to this granpos is larger
       then a threshold, to guard against some streams having large holes
       (eg, a stream ending early, which would cause seeking after that
       to fill up a queue for streams still active). */
    ts = gst_ogg_stream_get_end_time_for_granulepos (&pad->map, granulepos);
    if (GST_CLOCK_TIME_IS_VALID (ts)) {
      if (first_ts == GST_CLOCK_TIME_NONE) {
        GST_WARNING_OBJECT (pad, "Locking on pts %" GST_TIME_FORMAT,
            GST_TIME_ARGS (ts));
        first_ts = ts;
      }
      if (ts - first_ts > SEEK_GIVE_UP_THRESHOLD) {
        GST_WARNING_OBJECT (pad,
            "No data found for %" GST_TIME_FORMAT ", giving up",
            GST_TIME_ARGS (SEEK_GIVE_UP_THRESHOLD));
        found_keyframe = FALSE;
        keytarget = target;
        break;
      }
    }

    /* in reverse we want to go past the page with the lower timestamp */
    if (segment->rate < 0.0) {
      /* get time for this pad */
      granule_time = gst_ogg_stream_get_end_time_for_granulepos (&pad->map,
          granulepos);

      /* Convert to stream time */
      granule_time -= pad->start_time;
      granule_time += chain->begin_time;

      GST_LOG_OBJECT (ogg,
          "looking at page with time %" GST_TIME_FORMAT ", target %"
          GST_TIME_FORMAT, GST_TIME_ARGS (granule_time),
          GST_TIME_ARGS (target));
      if (granule_time < target)
        continue;
    }

    /* we've seen this pad before */
    if (pad->keyframe_granule != -1)
      continue;

    /* convert granule of this pad to the granule of the keyframe */
    pad->keyframe_granule =
        gst_ogg_stream_granulepos_to_key_granule (&pad->map, granulepos);
    GST_LOG_OBJECT (ogg, "marking stream granule %" G_GINT64_FORMAT,
        pad->keyframe_granule);

    /* get time of the keyframe */
    keyframe_time =
        gst_ogg_stream_granule_to_time (&pad->map, pad->keyframe_granule);
    GST_LOG_OBJECT (ogg,
        "stream %08x keyframe granule PTS %" GST_TIME_FORMAT
        " target %" GST_TIME_FORMAT,
        pad->map.serialno, GST_TIME_ARGS (keyframe_time),
        GST_TIME_ARGS (keytarget));

    /* collect smallest value */
    if (keyframe_time != -1) {
      keyframe_time -= pad->start_time;
      keyframe_time += begintime;
      if (keyframe_time < keytarget) {
        serialno = pad->map.serialno;
        keytarget = keyframe_time;
        found_keyframe = TRUE;
        GST_LOG_OBJECT (ogg, "storing keytarget %" GST_TIME_FORMAT,
            GST_TIME_ARGS (keytarget));
      }
    }

  next:
    pending--;
    if (pending == 0)
      break;
  }

  /* for negative rates we will get to the keyframe backwards */
  if (segment->rate < 0.0)
    goto done;

  /* No keyframe found, no need to bisect again, keytarget == target here */
  if (!found_keyframe)
    best = 0;

  if (keytarget != target) {
    GST_LOG_OBJECT (ogg, "final seek to target %" GST_TIME_FORMAT,
        GST_TIME_ARGS (keytarget));

    /* last step, seek to the location of the keyframe */
    if (!do_binary_search (ogg, chain, begin, end, begintime, endtime,
            keytarget, &best, TRUE, serialno))
      goto seek_error;
  } else {
    /* seek back to previous position */
    GST_LOG_OBJECT (ogg, "keyframe on target");
    gst_ogg_demux_seek (ogg, best);
  }

done:
  if (keyframe) {
    if (segment->rate > 0.0)
      segment->time = keytarget;
    segment->position = keytarget - begintime;
  }

  *rchain = chain;

  return TRUE;

no_chains:
  {
    GST_DEBUG_OBJECT (ogg, "no chains");
    return FALSE;
  }
seek_error:
  {
    GST_DEBUG_OBJECT (ogg, "got a seek error");
    return FALSE;
  }
}

/* does not take ownership of the event */
static gboolean
gst_ogg_demux_perform_seek_pull (GstOggDemux * ogg, GstEvent * event)
{
  GstOggChain *chain = NULL;
  gboolean res;
  gboolean accurate, keyframe;
  GstFormat format;
  gdouble rate;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gboolean update;
  guint32 seqnum;

  if (event) {
    GST_DEBUG_OBJECT (ogg, "seek with event");

    gst_event_parse_seek (event, &rate, &format, &flags,
        &start_type, &start, &stop_type, &stop);

    /* we can only seek on time */
    if (format != GST_FORMAT_TIME) {
      GST_DEBUG_OBJECT (ogg, "can only seek on TIME");
      goto error;
    }
    seqnum = gst_event_get_seqnum (event);
  } else {
    GST_DEBUG_OBJECT (ogg, "seek without event");

    flags = 0;
    rate = 1.0;
    seqnum = gst_util_seqnum_next ();
  }

  GST_DEBUG_OBJECT (ogg, "seek, rate %g", rate);

  accurate = flags & GST_SEEK_FLAG_ACCURATE;
  keyframe = flags & GST_SEEK_FLAG_KEY_UNIT;

  gst_pad_pause_task (ogg->sinkpad);

  /* now grab the stream lock so that streaming cannot continue, for
   * non flushing seeks when the element is in PAUSED this could block
   * forever. */
  GST_PAD_STREAM_LOCK (ogg->sinkpad);

  if (event) {
    gst_segment_do_seek (&ogg->segment, rate, format, flags,
        start_type, start, stop_type, stop, &update);
  }

  GST_DEBUG_OBJECT (ogg, "segment positions set to %" GST_TIME_FORMAT "-%"
      GST_TIME_FORMAT, GST_TIME_ARGS (ogg->segment.start),
      GST_TIME_ARGS (ogg->segment.stop));

  {
    gint i;

    /* reset all ogg streams now, need to do this from within the lock to
     * make sure the streaming thread is not messing with the stream */
    for (i = 0; i < ogg->chains->len; i++) {
      GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);

      gst_ogg_chain_reset (chain);
    }
  }

  /* for reverse we will already seek accurately */
  res = gst_ogg_demux_do_seek (ogg, &ogg->segment, accurate, keyframe, &chain);

  /* seek failed, make sure we continue the current chain */
  if (!res) {
    GST_DEBUG_OBJECT (ogg, "seek failed");
    chain = ogg->current_chain;
  } else {
    GST_DEBUG_OBJECT (ogg, "seek success");
  }

  if (!chain)
    goto no_chain;

  /* now we have a new position, prepare for streaming again */
  {
    GstEvent *event;
    gint64 stop;
    gint64 start;
    gint64 position, begin_time;
    GstSegment segment;

    /* we need this to see how far inside the chain we need to start */
    if (chain->begin_time != GST_CLOCK_TIME_NONE)
      begin_time = chain->begin_time;
    else
      begin_time = 0;

    /* segment.start gives the start over all chains, we calculate the amount
     * of time into this chain we need to start */
    start = ogg->segment.start - begin_time;
    if (chain->segment_start != GST_CLOCK_TIME_NONE)
      start += chain->segment_start;

    if ((stop = ogg->segment.stop) == -1)
      stop = ogg->segment.duration;

    /* segment.stop gives the stop time over all chains, calculate the amount of
     * time we need to stop in this chain */
    if (stop != -1) {
      if (stop > begin_time)
        stop -= begin_time;
      else
        stop = 0;
      stop += chain->segment_start;
      /* we must stop when this chain ends and switch to the next chain to play
       * the remainder of the segment. */
      stop = MIN (stop, chain->segment_stop);
    }

    position = ogg->segment.position;
    if (chain->segment_start != GST_CLOCK_TIME_NONE)
      position += chain->segment_start;

    gst_segment_copy_into (&ogg->segment, &segment);

    /* create the segment event we are going to send out */
    if (ogg->segment.rate >= 0.0) {
      segment.start = position;
      segment.stop = stop;
    } else {
      segment.start = start;
      segment.stop = position;
    }
    event = gst_event_new_segment (&segment);
    gst_event_set_seqnum (event, seqnum);

    if (chain != ogg->current_chain) {
      /* switch to different chain, send segment on new chain */
      gst_ogg_demux_activate_chain (ogg, chain, event);
    } else {
      /* mark discont and send segment on current chain */
      gst_ogg_chain_mark_discont (chain);
      /* This event should be sent from the streaming thread (sink pad task) */
      if (ogg->newsegment)
        gst_event_unref (ogg->newsegment);
      ogg->newsegment = event;
    }

    /* notify start of new segment */
    if (ogg->segment.flags & GST_SEEK_FLAG_SEGMENT) {
      GstMessage *message;

      message = gst_message_new_segment_start (GST_OBJECT (ogg),
          GST_FORMAT_TIME, ogg->segment.position);
      gst_message_set_seqnum (message, seqnum);

      gst_element_post_message (GST_ELEMENT (ogg), message);
    }

    ogg->seqnum = seqnum;
    /* restart our task since it might have been stopped when we did the 
     * flush. */
    gst_pad_start_task (ogg->sinkpad, (GstTaskFunction) gst_ogg_demux_loop,
        ogg->sinkpad, NULL);
  }

  /* streaming can continue now */
  GST_PAD_STREAM_UNLOCK (ogg->sinkpad);

done:
  if (event)
    gst_event_unref (event);
  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (ogg, "seek failed");
    res = FALSE;
    goto done;
  }
no_chain:
  {
    GST_DEBUG_OBJECT (ogg, "no chain to seek in");
    GST_PAD_STREAM_UNLOCK (ogg->sinkpad);
    res = FALSE;
    goto done;
  }
}

static gboolean
gst_ogg_demux_get_duration_push (GstOggDemux * ogg, int flags)
{
  /* In push mode, we get to the end of the stream to get the duration */
  gint64 position;
  GstEvent *sevent;

  /* A full Ogg page can be almost 64 KB. There's no guarantee that there'll be a
     granpos there, but it's fairly likely */
  position = ogg->push_byte_length - DURATION_CHUNK_OFFSET;
  if (position < 0)
    position = 0;

  GST_DEBUG_OBJECT (ogg,
      "Getting duration, seeking near the end, to %" G_GINT64_FORMAT, position);
  ogg->push_state = PUSH_DURATION;
  /* do not read the last byte */
  sevent = gst_event_new_seek (1.0, GST_FORMAT_BYTES, flags, GST_SEEK_TYPE_SET,
      position, GST_SEEK_TYPE_SET, ogg->push_byte_length - 1);
  gst_event_replace (&ogg->seek_event, sevent);
  ogg->seek_event_drop_till = gst_event_get_seqnum (sevent);
  gst_event_unref (sevent);
  g_mutex_lock (&ogg->seek_event_mutex);
  g_cond_broadcast (&ogg->seek_event_cond);
  g_mutex_unlock (&ogg->seek_event_mutex);
  return TRUE;
}

static gboolean
gst_ogg_demux_check_duration_push (GstOggDemux * ogg, GstSeekFlags flags,
    GstEvent * event)
{
  if (ogg->push_byte_length < 0) {
    GstPad *peer;

    GST_DEBUG_OBJECT (ogg, "Trying to find byte/time length");
    if ((peer = gst_pad_get_peer (ogg->sinkpad)) != NULL) {
      gint64 length;
      int res;

      res = gst_pad_query_duration (peer, GST_FORMAT_BYTES, &length);
      if (res && length > 0) {
        ogg->push_byte_length = length;
        GST_DEBUG_OBJECT (ogg,
            "File byte length %" G_GINT64_FORMAT, ogg->push_byte_length);
      } else {
        GST_DEBUG_OBJECT (ogg, "File byte length unknown, assuming live");
        ogg->push_disable_seeking = TRUE;
        gst_object_unref (peer);
        return TRUE;
      }
      res = gst_pad_query_duration (peer, GST_FORMAT_TIME, &length);
      gst_object_unref (peer);
      if (res && length >= 0) {
        ogg->push_time_length = length;
        GST_DEBUG_OBJECT (ogg, "File time length %" GST_TIME_FORMAT,
            GST_TIME_ARGS (ogg->push_time_length));
      } else if (!ogg->push_disable_seeking) {
        gboolean res;

        res = gst_ogg_demux_get_duration_push (ogg, flags);
        if (res) {
          GST_DEBUG_OBJECT (ogg,
              "File time length unknown, trying to determine");
          ogg->push_mode_seek_delayed_event = NULL;
          if (event) {
            GST_DEBUG_OBJECT (ogg,
                "Let me intercept this innocent looking seek request");
            ogg->push_mode_seek_delayed_event = gst_event_copy (event);
          }
          return FALSE;
        }
      }
    }
  }
  return TRUE;
}

static gboolean
gst_ogg_demux_perform_seek_push (GstOggDemux * ogg, GstEvent * event)
{
  gint bitrate;
  gboolean res = TRUE;
  GstFormat format;
  gdouble rate;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  GstEvent *sevent;
  GstOggChain *chain;
  gint64 best, best_time;
  gint i;

  GST_DEBUG_OBJECT (ogg, "Push mode seek request received");

  gst_event_parse_seek (event, &rate, &format, &flags,
      &start_type, &start, &stop_type, &stop);

  if (format != GST_FORMAT_TIME) {
    GST_DEBUG_OBJECT (ogg, "can only seek on TIME");
    goto error;
  }

  if (start_type != GST_SEEK_TYPE_SET) {
    GST_DEBUG_OBJECT (ogg, "can only seek to a SET target");
    goto error;
  }

  /* If stop is unset, make sure it is -1, as this value will be tested
     later to check whether stop is set or not */
  if (stop_type == GST_SEEK_TYPE_NONE)
    stop = -1;

  GST_DEBUG_OBJECT (ogg, "Push mode seek request: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (start));

  chain = ogg->current_chain;
  if (!chain) {
    GST_WARNING_OBJECT (ogg, "No chain to seek on");
    goto error;
  }

  /* start accessing push_* members */
  GST_PUSH_LOCK (ogg);

  /* not if we disabled seeking (chained streams) */
  if (ogg->push_disable_seeking) {
    GST_DEBUG_OBJECT (ogg, "Seeking disabled");
    goto error_locked;
  }

  /* not when we're trying to work out duration */
  if (ogg->push_state == PUSH_DURATION) {
    GST_DEBUG_OBJECT (ogg, "Busy working out duration, try again later");
    goto error_locked;
  }

  /* actually, not if we're doing any seeking already */
  if (ogg->push_state != PUSH_PLAYING) {
    GST_DEBUG_OBJECT (ogg, "Already doing some seeking, try again later");
    goto error_locked;
  }

  /* on the first seek, get length if we can */
  if (!gst_ogg_demux_check_duration_push (ogg, flags, event)) {
    GST_PUSH_UNLOCK (ogg);
    return FALSE;
  }

  if (do_index_search (ogg, chain, 0, -1, 0, -1, start, &best, &best_time)) {
    /* the index gave some result */
    GST_DEBUG_OBJECT (ogg,
        "found offset %" G_GINT64_FORMAT " with time %" G_GUINT64_FORMAT,
        best, best_time);
  } else {
    if (ogg->push_time_length > 0) {
      /* if we know the time length, we know the full segment bitrate */
      GST_DEBUG_OBJECT (ogg, "Using real file bitrate");
      bitrate =
          gst_util_uint64_scale (ogg->push_byte_length, 8 * GST_SECOND,
          ogg->push_time_length);
    } else if (ogg->push_time_offset > 0) {
      /* get a first approximation using known bitrate to the current position */
      GST_DEBUG_OBJECT (ogg, "Using file bitrate so far");
      bitrate =
          gst_util_uint64_scale (ogg->push_byte_offset, 8 * GST_SECOND,
          ogg->push_time_offset);
    } else if (ogg->bitrate > 0) {
      /* nominal bitrate is better than nothing, even if it lies often */
      GST_DEBUG_OBJECT (ogg, "Using nominal bitrate");
      bitrate = ogg->bitrate;
    } else {
      /* meh */
      GST_DEBUG_OBJECT (ogg,
          "At stream start, and no nominal bitrate, using some random magic "
          "number to seed");
      /* the bisection, once started, should give us a better approximation */
      bitrate = 1000;
    }
    best = gst_util_uint64_scale (start, bitrate, 8 * GST_SECOND);
  }

  /* offset by typical page length, and ensure our best guess is within
     reasonable bounds */
  best -= ogg->chunk_size;
  if (best < 0)
    best = 0;
  if (ogg->push_byte_length > 0 && best >= ogg->push_byte_length)
    best = ogg->push_byte_length - 1;

  /* set up bisection search */
  ogg->push_offset0 = 0;
  ogg->push_offset1 = ogg->push_byte_length - 1;
  ogg->push_time0 = ogg->push_start_time;
  ogg->push_time1 = ogg->push_time_length;
  ogg->seqnum = gst_event_get_seqnum (event);
  ogg->push_seek_time_target = start;
  ogg->push_prev_seek_time = GST_CLOCK_TIME_NONE;
  ogg->push_seek_time_original_target = start;
  ogg->push_seek_time_original_stop = stop;
  ogg->push_state = PUSH_BISECT1;
  ogg->seek_secant = FALSE;
  ogg->seek_undershot = FALSE;

  if (flags & GST_SEEK_FLAG_FLUSH) {
    /* reset pad push mode seeking state */
    for (i = 0; i < chain->streams->len; i++) {
      GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);
      pad->push_kf_time = GST_CLOCK_TIME_NONE;
      pad->push_sync_time = GST_CLOCK_TIME_NONE;
    }
  }

  GST_DEBUG_OBJECT (ogg,
      "Setting up bisection search for %" G_GINT64_FORMAT " - %" G_GINT64_FORMAT
      " (time %" GST_TIME_FORMAT " - %" GST_TIME_FORMAT ")", ogg->push_offset0,
      ogg->push_offset1, GST_TIME_ARGS (ogg->push_time0),
      GST_TIME_ARGS (ogg->push_time1));
  GST_DEBUG_OBJECT (ogg,
      "Target time is %" GST_TIME_FORMAT ", best first guess is %"
      G_GINT64_FORMAT, GST_TIME_ARGS (ogg->push_seek_time_target), best);

  ogg->push_seek_rate = rate;
  ogg->push_seek_flags = flags;
  ogg->push_mode_seek_delayed_event = NULL;
  ogg->push_bisection_steps[0] = 1;
  ogg->push_bisection_steps[1] = 0;
  sevent = gst_event_new_seek (rate, GST_FORMAT_BYTES, flags,
      start_type, best, GST_SEEK_TYPE_NONE, -1);
  gst_event_set_seqnum (sevent, gst_event_get_seqnum (event));

  gst_event_replace (&ogg->seek_event, sevent);
  gst_event_unref (sevent);
  GST_PUSH_UNLOCK (ogg);
  g_mutex_lock (&ogg->seek_event_mutex);
  g_cond_broadcast (&ogg->seek_event_cond);
  g_mutex_unlock (&ogg->seek_event_mutex);

  return res;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (ogg, "seek failed");
    return FALSE;
  }

error_locked:
  GST_PUSH_UNLOCK (ogg);
  goto error;
}

static gboolean
gst_ogg_demux_setup_seek_pull (GstOggDemux * ogg, GstEvent * event)
{
  gboolean flush;
  GstSeekFlags flags;
  GstEvent *tevent;
  guint32 seqnum = gst_event_get_seqnum (event);

  GST_DEBUG_OBJECT (ogg, "Scheduling seek: %" GST_PTR_FORMAT, event);
  gst_event_parse_seek (event, NULL, NULL, &flags, NULL, NULL, NULL, NULL);

  flush = flags & GST_SEEK_FLAG_FLUSH;

  /* first step is to unlock the streaming thread if it is
   * blocked in a chain call, we do this by starting the flush. because
   * we cannot yet hold any streaming lock, we have to protect the chains
   * with their own lock. */
  if (flush) {
    gint i;

    tevent = gst_event_new_flush_start ();
    gst_event_set_seqnum (tevent, seqnum);

    gst_event_ref (tevent);
    gst_pad_push_event (ogg->sinkpad, tevent);

    GST_CHAIN_LOCK (ogg);
    for (i = 0; i < ogg->chains->len; i++) {
      GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);
      gint j;

      for (j = 0; j < chain->streams->len; j++) {
        GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, j);

        gst_event_ref (tevent);
        gst_pad_push_event (GST_PAD (pad), tevent);
      }
    }
    GST_CHAIN_UNLOCK (ogg);

    gst_event_unref (tevent);
  }

  gst_pad_pause_task (ogg->sinkpad);

  /* now grab the stream lock so that streaming cannot continue, for
   * non flushing seeks when the element is in PAUSED this could block
   * forever. */
  GST_PAD_STREAM_LOCK (ogg->sinkpad);

  /* we need to stop flushing on the sinkpad as we're going to use it
   * next. We can do this as we have the STREAM lock now. */
  if (flush) {
    tevent = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (tevent, seqnum);
    gst_pad_push_event (ogg->sinkpad, gst_event_ref (tevent));
    gst_ogg_demux_send_event (ogg, tevent);
  }

  gst_event_replace (&ogg->seek_event, event);
  gst_pad_start_task (ogg->sinkpad, (GstTaskFunction) gst_ogg_demux_loop,
      ogg->sinkpad, NULL);
  GST_PAD_STREAM_UNLOCK (ogg->sinkpad);

  return TRUE;
}

static gboolean
gst_ogg_demux_perform_seek (GstOggDemux * ogg, GstEvent * event)
{
  gboolean res;

  if (ogg->pullmode) {
    res = gst_ogg_demux_setup_seek_pull (ogg, event);
  } else {
    res = gst_ogg_demux_perform_seek_push (ogg, event);
  }
  return res;
}


/* finds each bitstream link one at a time using a bisection search
 * (has to begin by knowing the offset of the lb's initial page).
 * Recurses for each link so it can alloc the link storage after
 * finding them all, then unroll and fill the cache at the same time
 */
static GstFlowReturn
gst_ogg_demux_bisect_forward_serialno (GstOggDemux * ogg,
    gint64 begin, gint64 searched, gint64 end, GstOggChain * chain, glong m)
{
  gint64 endsearched = end;
  gint64 next = end;
  ogg_page og;
  GstFlowReturn ret;
  gint64 offset;
  GstOggChain *nextchain;

  GST_LOG_OBJECT (ogg,
      "bisect begin: %" G_GINT64_FORMAT ", searched: %" G_GINT64_FORMAT
      ", end %" G_GINT64_FORMAT ", chain: %p", begin, searched, end, chain);

  /* the below guards against garbage separating the last and
   * first pages of two links. */
  while (searched < endsearched) {
    gint64 bisect;

    if (endsearched - searched < ogg->chunk_size) {
      bisect = searched;
    } else {
      bisect = (searched + endsearched) / 2;
    }

    gst_ogg_demux_seek (ogg, bisect);
    ret = gst_ogg_demux_get_next_page (ogg, &og, -1, &offset);

    if (ret == GST_FLOW_EOS) {
      endsearched = bisect;
    } else if (ret == GST_FLOW_OK) {
      guint32 serial = ogg_page_serialno (&og);

      if (!gst_ogg_chain_has_stream (chain, serial)) {
        endsearched = bisect;
        next = offset;
      } else {
        searched = offset + og.header_len + og.body_len;
      }
    } else
      return ret;
  }

  GST_LOG_OBJECT (ogg, "current chain ends at %" G_GINT64_FORMAT, searched);

  chain->end_offset = searched;
  ret = gst_ogg_demux_read_end_chain (ogg, chain);
  if (ret != GST_FLOW_OK)
    return ret;

  GST_LOG_OBJECT (ogg, "found begin at %" G_GINT64_FORMAT, next);

  gst_ogg_demux_seek (ogg, next);
  ret = gst_ogg_demux_read_chain (ogg, &nextchain);
  if (ret == GST_FLOW_EOS) {
    nextchain = NULL;
    ret = GST_FLOW_OK;
    GST_LOG_OBJECT (ogg, "no next chain");
  } else if (ret != GST_FLOW_OK)
    goto done;

  if (searched < end && nextchain != NULL) {
    ret = gst_ogg_demux_bisect_forward_serialno (ogg, next, ogg->offset,
        end, nextchain, m + 1);
    if (ret != GST_FLOW_OK)
      goto done;
  }
  GST_LOG_OBJECT (ogg, "adding chain %p", chain);

  g_array_insert_val (ogg->chains, 0, chain);

done:
  return ret;
}

/* read a chain from the ogg file. This code will
 * read all BOS pages and will create and return a GstOggChain 
 * structure with the results. 
 * 
 * This function will also read N pages from each stream in the
 * chain and submit them to the internal ogg stream parser/mapper
 * until we know the timestamp of the first page in the chain.
 */
static GstFlowReturn
gst_ogg_demux_read_chain (GstOggDemux * ogg, GstOggChain ** res_chain)
{
  GstFlowReturn ret;
  GstOggChain *chain = NULL;
  gint64 offset = ogg->offset;
  ogg_page og;
  gboolean done;
  gint i;

  GST_LOG_OBJECT (ogg, "reading chain at %" G_GINT64_FORMAT, offset);

  /* first read the BOS pages, detect the stream types, create the internal
   * stream mappers, send data to them. */
  while (TRUE) {
    GstOggPad *pad;
    guint32 serial;

    ret = gst_ogg_demux_get_next_page (ogg, &og, -1, NULL);
    if (ret != GST_FLOW_OK) {
      if (ret == GST_FLOW_EOS) {
        GST_DEBUG_OBJECT (ogg, "Reached EOS, done reading end chain");
      } else {
        GST_WARNING_OBJECT (ogg, "problem reading BOS page: ret=%d", ret);
      }
      break;
    }
    if (!ogg_page_bos (&og)) {
      GST_INFO_OBJECT (ogg, "page is not BOS page, all streams identified");
      /* if we did not find a chain yet, assume this is a bogus stream and
       * ignore it */
      if (!chain) {
        GST_WARNING_OBJECT (ogg, "No chain found, no Ogg data in stream ?");
        ret = GST_FLOW_EOS;
      }
      break;
    }

    if (chain == NULL) {
      chain = gst_ogg_chain_new (ogg);
      chain->offset = offset;
    }

    serial = ogg_page_serialno (&og);
    if (gst_ogg_chain_get_stream (chain, serial) != NULL) {
      GST_WARNING_OBJECT (ogg,
          "found serial %08x BOS page twice, ignoring", serial);
      continue;
    }

    pad = gst_ogg_chain_new_stream (chain, serial);
    gst_ogg_pad_submit_page (pad, &og);
  }

  if (ret != GST_FLOW_OK || chain == NULL) {
    if (ret == GST_FLOW_OK) {
      GST_WARNING_OBJECT (ogg, "no chain was found");
      ret = GST_FLOW_ERROR;
    } else if (ret != GST_FLOW_EOS) {
      GST_WARNING_OBJECT (ogg, "failed to read chain");
    } else {
      GST_DEBUG_OBJECT (ogg, "done reading chains");
    }
    if (chain) {
      gst_ogg_chain_free (chain);
    }
    if (res_chain)
      *res_chain = NULL;
    return ret;
  }

  chain->have_bos = TRUE;
  GST_INFO_OBJECT (ogg, "read bos pages, ");

  /* now read pages until each ogg stream mapper has figured out the
   * timestamp of the first packet in the chain */

  /* save the offset to the first non bos page in the chain: if searching for
   * pad->first_time we read past the end of the chain, we'll seek back to this
   * position
   */
  offset = ogg->offset;

  done = FALSE;
  while (!done) {
    guint32 serial;
    gboolean known_serial = FALSE;
    GstFlowReturn ret;

    serial = ogg_page_serialno (&og);
    done = TRUE;
    for (i = 0; i < chain->streams->len; i++) {
      GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

      GST_LOG_OBJECT (ogg,
          "serial %08x time %" GST_TIME_FORMAT,
          pad->map.serialno, GST_TIME_ARGS (pad->start_time));

      if (pad->map.serialno == serial) {
        known_serial = TRUE;

        /* submit the page now, this will fill in the start_time when the
         * internal stream mapper finds it */
        gst_ogg_pad_submit_page (pad, &og);

        if (!pad->map.is_skeleton && pad->start_time == -1
            && ogg_page_eos (&og)) {
          /* got EOS on a pad before we could find its start_time.
           * We have no chance of finding a start_time for every pad so
           * stop searching for the other start_time(s).
           */
          done = TRUE;
          break;
        }
      }
      /* the timestamp will be filled in when we submit the pages */
      if (!pad->map.is_sparse)
        done &= (pad->start_time != GST_CLOCK_TIME_NONE);

      GST_LOG_OBJECT (ogg, "done %08x now %d", pad->map.serialno, done);
    }

    /* we read a page not belonging to the current chain: seek back to the
     * beginning of the chain
     */
    if (!known_serial) {
      GST_LOG_OBJECT (ogg, "unknown serial %08x", serial);
      gst_ogg_demux_seek (ogg, offset);
      break;
    }

    if (!done) {
      ret = gst_ogg_demux_get_next_page (ogg, &og, -1, NULL);
      if (ret != GST_FLOW_OK)
        break;
    }
  }
  GST_LOG_OBJECT (ogg, "done reading chain");

  if (res_chain)
    *res_chain = chain;

  return GST_FLOW_OK;
}

/* read the last pages from the ogg stream to get the final
 * page end_offsets.
 */
static GstFlowReturn
gst_ogg_demux_read_end_chain (GstOggDemux * ogg, GstOggChain * chain)
{
  gint64 begin = chain->end_offset;
  gint64 end = begin;
  gint64 last_granule = -1;
  GstOggPad *last_pad = NULL;
  GstFlowReturn ret;
  gboolean done = FALSE;
  ogg_page og;
  gint i;

  while (!done) {
    begin -= ogg->chunk_size;
    if (begin < 0)
      begin = 0;

    gst_ogg_demux_seek (ogg, begin);

    /* now continue reading until we run out of data, if we find a page
     * start, we save it. It might not be the final page as there could be
     * another page after this one. */
    while (ogg->offset < end) {
      ret = gst_ogg_demux_get_next_page (ogg, &og, end - ogg->offset, NULL);

      if (ret == GST_FLOW_LIMIT)
        break;
      if (ret != GST_FLOW_OK)
        return ret;

      for (i = 0; i < chain->streams->len; i++) {
        GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

        if (pad->map.is_skeleton)
          continue;

        if (pad->map.serialno == ogg_page_serialno (&og)) {
          gint64 granulepos = ogg_page_granulepos (&og);

          if (granulepos != -1) {
            last_granule = granulepos;
            last_pad = pad;
            done = TRUE;
          }
          break;
        }
      }
    }
  }

  if (last_pad) {
    chain->segment_stop =
        gst_ogg_stream_get_end_time_for_granulepos (&last_pad->map,
        last_granule);
  } else {
    chain->segment_stop = GST_CLOCK_TIME_NONE;
  }

  GST_INFO ("segment stop %" G_GUINT64_FORMAT ", for last granule %"
      G_GUINT64_FORMAT, chain->segment_stop, last_granule);

  return GST_FLOW_OK;
}

/* find a pad with a given serial number
 */
static GstOggPad *
gst_ogg_demux_find_pad (GstOggDemux * ogg, guint32 serialno)
{
  GstOggPad *pad;
  gint i;

  /* first look in building chain if any */
  if (ogg->building_chain) {
    pad = gst_ogg_chain_get_stream (ogg->building_chain, serialno);
    if (pad)
      return pad;
  }

  /* then look in current chain if any */
  if (ogg->current_chain) {
    pad = gst_ogg_chain_get_stream (ogg->current_chain, serialno);
    if (pad)
      return pad;
  }

  for (i = 0; i < ogg->chains->len; i++) {
    GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);

    pad = gst_ogg_chain_get_stream (chain, serialno);
    if (pad)
      return pad;
  }
  return NULL;
}

/* find a chain with a given serial number
 */
static GstOggChain *
gst_ogg_demux_find_chain (GstOggDemux * ogg, guint32 serialno)
{
  GstOggPad *pad;

  pad = gst_ogg_demux_find_pad (ogg, serialno);
  if (pad) {
    return pad->chain;
  }
  return NULL;
}

/* returns TRUE if all streams have valid start time */
static gboolean
gst_ogg_demux_collect_chain_info (GstOggDemux * ogg, GstOggChain * chain)
{
  gboolean res = TRUE;

  chain->total_time = GST_CLOCK_TIME_NONE;
  GST_DEBUG_OBJECT (ogg, "trying to collect chain info");

  /* see if we have a start time on all streams */
  chain->segment_start = gst_ogg_demux_collect_start_time (ogg, chain);

  if (chain->segment_start == G_MAXUINT64) {
    /* not yet, stream some more data */
    res = FALSE;
  } else if (chain->segment_stop != GST_CLOCK_TIME_NONE) {
    /* we can calculate a total time */
    chain->total_time = chain->segment_stop - chain->segment_start;
  }

  GST_DEBUG ("total time %" G_GUINT64_FORMAT, chain->total_time);

  GST_DEBUG_OBJECT (ogg, "return %d", res);

  return res;
}

static void
gst_ogg_demux_collect_info (GstOggDemux * ogg)
{
  gint i;

  /* collect all info */
  ogg->total_time = 0;

  for (i = 0; i < ogg->chains->len; i++) {
    GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);

    chain->begin_time = ogg->total_time;

    gst_ogg_demux_collect_chain_info (ogg, chain);

    ogg->total_time += chain->total_time;
  }
  ogg->segment.duration = ogg->total_time;
}

/* find all the chains in the ogg file, this reads the first and
 * last page of the ogg stream, if they match then the ogg file has
 * just one chain, else we do a binary search for all chains.
 */
static GstFlowReturn
gst_ogg_demux_find_chains (GstOggDemux * ogg)
{
  ogg_page og;
  GstPad *peer;
  gboolean res;
  guint32 serialno;
  GstOggChain *chain;
  GstFlowReturn ret;

  /* get peer to figure out length */
  if ((peer = gst_pad_get_peer (ogg->sinkpad)) == NULL)
    goto no_peer;

  /* find length to read last page, we store this for later use. */
  res = gst_pad_query_duration (peer, GST_FORMAT_BYTES, &ogg->length);
  gst_object_unref (peer);
  if (!res || ogg->length <= 0)
    goto no_length;

  GST_DEBUG_OBJECT (ogg, "file length %" G_GINT64_FORMAT, ogg->length);

  /* read chain from offset 0, this is the first chain of the
   * ogg file. */
  gst_ogg_demux_seek (ogg, 0);
  ret = gst_ogg_demux_read_chain (ogg, &chain);
  if (ret != GST_FLOW_OK) {
    if (ret == GST_FLOW_FLUSHING)
      goto flushing;
    else
      goto no_first_chain;
  }

  /* read page from end offset, we use this page to check if its serial
   * number is contained in the first chain. If this is the case then
   * this ogg is not a chained ogg and we can skip the scanning. */
  gst_ogg_demux_seek (ogg, ogg->length);
  ret = gst_ogg_demux_get_prev_page (ogg, &og, NULL);
  if (ret != GST_FLOW_OK)
    goto no_last_page;

  serialno = ogg_page_serialno (&og);

  if (!gst_ogg_chain_has_stream (chain, serialno)) {
    /* the last page is not in the first stream, this means we should
     * find all the chains in this chained ogg. */
    ret =
        gst_ogg_demux_bisect_forward_serialno (ogg, 0, 0, ogg->length, chain,
        0);
  } else {
    /* we still call this function here but with an empty range so that
     * we can reuse the setup code in this routine. */
    ret =
        gst_ogg_demux_bisect_forward_serialno (ogg, 0, ogg->length,
        ogg->length, chain, 0);
  }
  if (ret != GST_FLOW_OK)
    goto done;

  /* all fine, collect and print */
  gst_ogg_demux_collect_info (ogg);

  /* dump our chains and streams */
  gst_ogg_print (ogg);

done:
  return ret;

  /*** error cases ***/
no_peer:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DEMUX, (NULL), ("we don't have a peer"));
    return GST_FLOW_NOT_LINKED;
  }
no_length:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DEMUX, (NULL), ("can't get file length"));
    return GST_FLOW_NOT_SUPPORTED;
  }
no_first_chain:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DEMUX, (NULL), ("can't get first chain"));
    return GST_FLOW_ERROR;
  }
no_last_page:
  {
    GST_DEBUG_OBJECT (ogg, "can't get last page");
    if (chain)
      gst_ogg_chain_free (chain);
    return ret;
  }
flushing:
  {
    GST_DEBUG_OBJECT (ogg, "Flushing, can't read chain");
    return GST_FLOW_FLUSHING;
  }
}

static void
gst_ogg_demux_update_chunk_size (GstOggDemux * ogg, ogg_page * page)
{
  long size = page->header_len + page->body_len;
  long chunk_size = size * 2;
  if (chunk_size > ogg->chunk_size) {
    GST_LOG_OBJECT (ogg, "Updating chunk size to %ld", chunk_size);
    ogg->chunk_size = chunk_size;
  }
}

static GstFlowReturn
gst_ogg_demux_handle_page (GstOggDemux * ogg, ogg_page * page, gboolean discont)
{
  GstOggPad *pad;
  gint64 granule;
  guint32 serialno;
  GstFlowReturn result = GST_FLOW_OK;

  serialno = ogg_page_serialno (page);
  granule = ogg_page_granulepos (page);

  gst_ogg_demux_update_chunk_size (ogg, page);

  GST_LOG_OBJECT (ogg,
      "processing ogg page (serial %08x, "
      "pageno %ld, granulepos %" G_GINT64_FORMAT ", bos %d)", serialno,
      ogg_page_pageno (page), granule, ogg_page_bos (page));

  if (ogg_page_bos (page)) {
    GstOggChain *chain;

    /* first page */
    /* see if we know about the chain already */
    chain = gst_ogg_demux_find_chain (ogg, serialno);
    if (chain) {
      GstEvent *event;
      gint64 start = 0;
      GstSegment segment;

      if (chain->segment_start != GST_CLOCK_TIME_NONE)
        start = chain->segment_start;

      /* create the newsegment event we are going to send out */
      gst_segment_copy_into (&ogg->segment, &segment);
      segment.start = start;
      segment.stop = chain->segment_stop;
      segment.time = chain->begin_time;
      segment.base += chain->begin_time;
      event = gst_event_new_segment (&segment);
      gst_event_set_seqnum (event, ogg->seqnum);

      GST_DEBUG_OBJECT (ogg,
          "segment: start %" GST_TIME_FORMAT ", stop %" GST_TIME_FORMAT
          ", time %" GST_TIME_FORMAT, GST_TIME_ARGS (start),
          GST_TIME_ARGS (chain->segment_stop),
          GST_TIME_ARGS (chain->begin_time));

      /* activate it as it means we have a non-header, this will also deactivate
       * the currently running chain. */
      gst_ogg_demux_activate_chain (ogg, chain, event);
      pad = gst_ogg_demux_find_pad (ogg, serialno);
    } else {
      GstClockTime chain_time;
      gint64 current_time;

      /* this can only happen in push mode */
      if (ogg->pullmode)
        goto unknown_chain;

      current_time = ogg->segment.position;

      /* time of new chain is current time */
      chain_time = current_time;

      if (ogg->building_chain == NULL) {
        GstOggChain *newchain;

        newchain = gst_ogg_chain_new (ogg);
        newchain->offset = 0;
        /* set new chain begin time aligned with end time of old chain */
        newchain->begin_time = chain_time;
        GST_DEBUG_OBJECT (ogg, "new chain, begin time %" GST_TIME_FORMAT,
            GST_TIME_ARGS (chain_time));

        /* and this is the one we are building now */
        ogg->building_chain = newchain;
      }
      pad = gst_ogg_chain_new_stream (ogg->building_chain, serialno);
    }
  } else {
    pad = gst_ogg_demux_find_pad (ogg, serialno);
  }
  if (pad) {
    /* Reset granule interpolation if chaining in reverse (discont = TRUE) */
    if (discont)
      pad->current_granule = -1;

    result = gst_ogg_pad_submit_page (pad, page);
  } else {
    GST_PUSH_LOCK (ogg);
    if (!ogg->pullmode && !ogg->push_disable_seeking) {
      /* no pad while probing for duration, we must have a chained stream,
         and we don't support them, so back off */
      GST_INFO_OBJECT (ogg, "We seem to have a chained stream, we won't seek");
      if (ogg->push_state == PUSH_DURATION) {
        GstFlowReturn res;

        res = gst_ogg_demux_seek_back_after_push_duration_check_unlock (ogg);
        /* Call to function above unlocks, relock */
        GST_PUSH_LOCK (ogg);
        if (res != GST_FLOW_OK)
          return res;
      }

      /* only once we seeked back */
      ogg->push_disable_seeking = TRUE;
    } else {
      GST_PUSH_UNLOCK (ogg);
      /* no pad. This means an ogg page without bos has been seen for this
       * serialno. we just ignore it but post a warning... */
      GST_ELEMENT_WARNING (ogg, STREAM, DECODE,
          (NULL), ("unknown ogg pad for serial %08x detected", serialno));
      return GST_FLOW_OK;
    }
    GST_PUSH_UNLOCK (ogg);
  }
  return result;

  /* ERRORS */
unknown_chain:
  {
    GST_ELEMENT_ERROR (ogg, STREAM, DECODE,
        (NULL), ("unknown ogg chain for serial %08x detected", serialno));
    return GST_FLOW_ERROR;
  }
}

/* streaming mode, receive a buffer, parse it, create pads for
 * the serialno, submit pages and packets to the oggpads
 */
static GstFlowReturn
gst_ogg_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstOggDemux *ogg;
  gint ret = 0;
  GstFlowReturn result = GST_FLOW_OK;
  gboolean drop;

  ogg = GST_OGG_DEMUX (parent);

  GST_PUSH_LOCK (ogg);
  drop = (ogg->seek_event_drop_till > 0);
  GST_PUSH_UNLOCK (ogg);
  if (drop) {
    GST_DEBUG_OBJECT (ogg, "Dropping buffer because we have a pending seek");
    gst_buffer_unref (buffer);
    return GST_FLOW_OK;
  }

  GST_DEBUG_OBJECT (ogg, "enter");
  result = gst_ogg_demux_submit_buffer (ogg, buffer);
  if (result < 0) {
    GST_DEBUG_OBJECT (ogg, "gst_ogg_demux_submit_buffer returned %d", result);
  }

  while (result == GST_FLOW_OK) {
    ogg_page page;

    ret = ogg_sync_pageout (&ogg->sync, &page);
    if (ret == 0)
      /* need more data */
      break;
    if (ret == -1) {
      /* discontinuity in the pages */
      GST_DEBUG_OBJECT (ogg, "discont in page found, continuing");
    } else {
      result = gst_ogg_demux_handle_page (ogg, &page, FALSE);
      if (result < 0) {
        GST_DEBUG_OBJECT (ogg, "gst_ogg_demux_handle_page returned %d", result);
      }
    }
  }
  if (ret == 0 || result == GST_FLOW_OK) {
    gst_ogg_demux_sync_streams (ogg);
  }
  GST_DEBUG_OBJECT (ogg, "leave with %d", result);
  return result;
}

static gboolean
gst_ogg_demux_send_event (GstOggDemux * ogg, GstEvent * event)
{
  GstOggChain *chain = ogg->current_chain;
  gboolean event_sent = FALSE;
  gboolean res = TRUE;

  if (!chain)
    chain = ogg->building_chain;

  if (chain) {
    gint i;

    for (i = 0; i < chain->streams->len; i++) {
      GstOggPad *pad = g_array_index (chain->streams, GstOggPad *, i);

      gst_event_ref (event);
      GST_DEBUG_OBJECT (pad, "Pushing event %" GST_PTR_FORMAT, event);
      res &= gst_pad_push_event (GST_PAD (pad), event);
      if (pad->added)
        event_sent = TRUE;
    }
  }

  gst_event_unref (event);

  if (!event_sent && GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GST_ELEMENT_ERROR (ogg, STREAM, DEMUX, (NULL),
        ("EOS before finding a chain"));
  }

  return res;
}

static GstFlowReturn
gst_ogg_demux_combine_flows (GstOggDemux * ogg, GstOggPad * pad,
    GstFlowReturn ret)
{
  /* store the value */
  pad->last_ret = ret;
  pad->is_eos = (ret == GST_FLOW_EOS);

  return gst_flow_combiner_update_pad_flow (ogg->flowcombiner,
      GST_PAD_CAST (pad), ret);
}

static GstFlowReturn
gst_ogg_demux_loop_forward (GstOggDemux * ogg)
{
  GstFlowReturn ret;
  GstBuffer *buffer = NULL;

  if (ogg->offset == ogg->length) {
    GST_LOG_OBJECT (ogg, "no more data to pull %" G_GINT64_FORMAT
        " == %" G_GINT64_FORMAT, ogg->offset, ogg->length);
    ret = GST_FLOW_EOS;
    goto done;
  }

  GST_LOG_OBJECT (ogg, "pull data %" G_GINT64_FORMAT, ogg->offset);
  ret =
      gst_pad_pull_range (ogg->sinkpad, ogg->offset, ogg->chunk_size, &buffer);
  if (ret != GST_FLOW_OK) {
    GST_LOG_OBJECT (ogg, "Failed pull_range");
    goto done;
  }

  ogg->offset += gst_buffer_get_size (buffer);

  if (G_UNLIKELY (ogg->newsegment)) {
    gst_ogg_demux_send_event (ogg, ogg->newsegment);
    ogg->newsegment = NULL;
  }

  ret = gst_ogg_demux_chain (ogg->sinkpad, GST_OBJECT_CAST (ogg), buffer);
  if (ret != GST_FLOW_OK && ret != GST_FLOW_EOS) {
    GST_LOG_OBJECT (ogg, "Failed demux_chain");
  }

done:
  return ret;
}

/* reverse mode.
 *
 * We read the pages backwards and send the packets forwards. The first packet
 * in the page will be pushed with the DISCONT flag set.
 *
 * Special care has to be taken for continued pages, which we can only decode
 * when we have the previous page(s).
 */
static GstFlowReturn
gst_ogg_demux_loop_reverse (GstOggDemux * ogg)
{
  GstFlowReturn ret;
  ogg_page page;
  gint64 offset;

  if (ogg->offset == 0) {
    GST_LOG_OBJECT (ogg, "no more data to pull %" G_GINT64_FORMAT
        " == 0", ogg->offset);
    ret = GST_FLOW_EOS;
    goto done;
  }

  GST_LOG_OBJECT (ogg, "read page from %" G_GINT64_FORMAT, ogg->offset);
  ret = gst_ogg_demux_get_prev_page (ogg, &page, &offset);
  if (ret != GST_FLOW_OK)
    goto done;

  ogg->offset = offset;

  if (G_UNLIKELY (ogg->newsegment)) {
    gst_ogg_demux_send_event (ogg, ogg->newsegment);
    ogg->newsegment = NULL;
  }

  GST_LOG_OBJECT (ogg, "Handling page at offset %" G_GINT64_FORMAT,
      ogg->offset);
  ret = gst_ogg_demux_handle_page (ogg, &page, TRUE);

done:
  return ret;
}

static void
gst_ogg_demux_sync_streams (GstOggDemux * ogg)
{
  GstClockTime cur;
  GstOggChain *chain;
  guint i;

  chain = ogg->current_chain;
  cur = ogg->segment.position;
  if (chain == NULL || cur == -1)
    return;

  for (i = 0; i < chain->streams->len; i++) {
    GstOggPad *stream = g_array_index (chain->streams, GstOggPad *, i);

    /* Theoretically, we should be doing this for all streams, so we're doing
     * it, but it might break things break things for wrongly-muxed streams
     * (like we used to produce once) */
    if ( /*stream->map.is_sparse && */ stream->position != GST_CLOCK_TIME_NONE) {

      /* Does this stream lag? Random threshold of 2 seconds */
      if (GST_CLOCK_DIFF (stream->position, cur) > (2 * GST_SECOND)) {
        GST_DEBUG_OBJECT (stream, "synchronizing stream with others by "
            "advancing time from %" GST_TIME_FORMAT " to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (stream->position), GST_TIME_ARGS (cur));

        stream->position = cur;

        gst_pad_push_event (GST_PAD_CAST (stream),
            gst_event_new_gap (stream->position, cur - stream->position));
      }
    }
  }
}

/* random access code
 *
 * - first find all the chains and streams by scanning the file.
 * - then get and chain buffers, just like the streaming case.
 * - when seeking, we can use the chain info to perform the seek.
 */
static void
gst_ogg_demux_loop (GstOggPad * pad)
{
  GstOggDemux *ogg;
  gboolean res;
  GstFlowReturn ret;
  GstEvent *seek;

  ogg = GST_OGG_DEMUX (GST_OBJECT_PARENT (pad));
  seek = ogg->seek_event;
  ogg->seek_event = NULL;

  if (ogg->need_chains) {

    /* this is the only place where we write chains and thus need to lock. */
    GST_CHAIN_LOCK (ogg);
    ret = gst_ogg_demux_find_chains (ogg);
    GST_CHAIN_UNLOCK (ogg);
    if (ret != GST_FLOW_OK)
      goto chain_read_failed;

    ogg->need_chains = FALSE;

    GST_OBJECT_LOCK (ogg);
    ogg->running = TRUE;
    GST_OBJECT_UNLOCK (ogg);

    /* and seek to configured positions without FLUSH */
    res = gst_ogg_demux_perform_seek_pull (ogg, seek);

    if (!res)
      goto seek_failed;
  } else if (seek) {
    res = gst_ogg_demux_perform_seek_pull (ogg, seek);
    if (!res)
      goto seek_failed;
  }

  if (ogg->segment.rate >= 0.0)
    ret = gst_ogg_demux_loop_forward (ogg);
  else
    ret = gst_ogg_demux_loop_reverse (ogg);

  if (ret != GST_FLOW_OK)
    goto pause;

  gst_ogg_demux_sync_streams (ogg);
  return;

  /* ERRORS */
chain_read_failed:
  {
    /* error was posted */
    goto pause;
  }
seek_failed:
  {
    gboolean flushing;

    GST_OBJECT_LOCK (pad);
    flushing = GST_PAD_IS_FLUSHING (pad);
    GST_OBJECT_UNLOCK (pad);
    if (flushing) {
      ret = GST_FLOW_FLUSHING;
    } else {
      GST_ELEMENT_FLOW_ERROR (ogg, ret);
      ret = GST_FLOW_ERROR;
    }
    goto pause;
  }
pause:
  {
    const gchar *reason = gst_flow_get_name (ret);
    GstEvent *event = NULL;

    GST_LOG_OBJECT (ogg, "pausing task, reason %s", reason);
    gst_pad_pause_task (ogg->sinkpad);

    if (ret == GST_FLOW_EOS) {
      /* perform EOS logic */
      if (ogg->segment.flags & GST_SEEK_FLAG_SEGMENT) {
        gint64 stop;
        GstMessage *message;

        /* for segment playback we need to post when (in stream time)
         * we stopped, this is either stop (when set) or the duration. */
        if ((stop = ogg->segment.stop) == -1)
          stop = ogg->segment.duration;

        GST_LOG_OBJECT (ogg, "Sending segment done, at end of segment");
        message =
            gst_message_new_segment_done (GST_OBJECT (ogg), GST_FORMAT_TIME,
            stop);
        gst_message_set_seqnum (message, ogg->seqnum);

        gst_element_post_message (GST_ELEMENT (ogg), message);

        event = gst_event_new_segment_done (GST_FORMAT_TIME, stop);
        gst_event_set_seqnum (event, ogg->seqnum);
        gst_ogg_demux_send_event (ogg, event);
        event = NULL;
      } else {
        /* normal playback, send EOS to all linked pads */
        GST_LOG_OBJECT (ogg, "Sending EOS, at end of stream");
        event = gst_event_new_eos ();
      }
    } else if (ret == GST_FLOW_NOT_LINKED || ret < GST_FLOW_EOS) {
      GST_ELEMENT_FLOW_ERROR (ogg, ret);
      event = gst_event_new_eos ();
    }

    /* For wrong-state we still want to pause the task and stop
     * but no error message or other things are necessary.
     * wrong-state is no real error and will be caused by flushing,
     * e.g. because of a flushing seek.
     */
    if (event) {
      /* guard against corrupt/truncated files, where one can hit EOS
         before prerolling is done and a chain created. If we have no
         chain to send the event to, error out. */
      if (ogg->current_chain || ogg->building_chain) {
        gst_event_set_seqnum (event, ogg->seqnum);
        gst_ogg_demux_send_event (ogg, event);
      } else {
        gst_event_unref (event);
        GST_ELEMENT_ERROR (ogg, STREAM, DEMUX, (NULL),
            ("EOS before finding a chain"));
      }
    }
    return;
  }
}

/* The sink pad task function for push mode.
 * It just sends any seek events queued by the streaming thread.
 */
static gpointer
gst_ogg_demux_loop_push (GstOggDemux * ogg)
{
  GstEvent *event = NULL;

  g_mutex_lock (&ogg->seek_event_mutex);
  /* Inform other threads that we started */
  ogg->seek_thread_started = TRUE;
  g_cond_broadcast (&ogg->thread_started_cond);


  while (!ogg->seek_event_thread_stop) {

    while (!ogg->seek_event_thread_stop) {
      GST_PUSH_LOCK (ogg);
      event = ogg->seek_event;
      ogg->seek_event = NULL;
      if (event)
        ogg->seek_event_drop_till = gst_event_get_seqnum (event);
      GST_PUSH_UNLOCK (ogg);

      if (event)
        break;

      g_cond_wait (&ogg->seek_event_cond, &ogg->seek_event_mutex);
    }

    if (ogg->seek_event_thread_stop) {
      break;
    }
    g_assert (event);

    g_mutex_unlock (&ogg->seek_event_mutex);

    GST_DEBUG_OBJECT (ogg->sinkpad, "Pushing event %" GST_PTR_FORMAT, event);
    if (!gst_pad_push_event (ogg->sinkpad, event)) {
      GST_WARNING_OBJECT (ogg, "Failed to push event");
      GST_PUSH_LOCK (ogg);
      if (!ogg->pullmode) {
        ogg->push_state = PUSH_PLAYING;
        ogg->push_disable_seeking = TRUE;
      }
      GST_PUSH_UNLOCK (ogg);
    } else {
      GST_DEBUG_OBJECT (ogg->sinkpad, "Pushed event ok");
    }

    g_mutex_lock (&ogg->seek_event_mutex);
  }

  g_mutex_unlock (&ogg->seek_event_mutex);

  gst_object_unref (ogg);
  return NULL;
}

static void
gst_ogg_demux_clear_chains (GstOggDemux * ogg)
{
  gint i;

  gst_ogg_demux_deactivate_current_chain (ogg);

  GST_CHAIN_LOCK (ogg);
  for (i = 0; i < ogg->chains->len; i++) {
    GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);

    if (chain == ogg->current_chain)
      ogg->current_chain = NULL;
    if (chain == ogg->building_chain)
      ogg->building_chain = NULL;
    gst_ogg_chain_free (chain);
  }
  ogg->chains = g_array_set_size (ogg->chains, 0);
  if (ogg->current_chain != NULL) {
    GST_FIXME_OBJECT (ogg, "current chain was tracked in existing chains !");
    gst_ogg_chain_free (ogg->current_chain);
    ogg->current_chain = NULL;
  }
  if (ogg->building_chain != NULL) {
    GST_FIXME_OBJECT (ogg, "building chain was tracked in existing chains !");
    gst_ogg_chain_free (ogg->building_chain);
    ogg->building_chain = NULL;
  }
  GST_CHAIN_UNLOCK (ogg);
}

/* this function is called when the pad is activated and should start
 * processing data.
 *
 * We check if we can do random access to decide if we work push or
 * pull based.
 */
static gboolean
gst_ogg_demux_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  GstQuery *query;
  gboolean pull_mode;

  query = gst_query_new_scheduling ();

  if (!gst_pad_peer_query (sinkpad, query)) {
    gst_query_unref (query);
    goto activate_push;
  }

  pull_mode = gst_query_has_scheduling_mode_with_flags (query,
      GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE);
  gst_query_unref (query);

  if (!pull_mode)
    goto activate_push;

  GST_DEBUG_OBJECT (sinkpad, "activating pull");
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PULL, TRUE);

activate_push:
  {
    GST_DEBUG_OBJECT (sinkpad, "activating push");
    return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
  }
}

static gboolean
gst_ogg_demux_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  gboolean res;
  GstOggDemux *ogg;

  ogg = GST_OGG_DEMUX (parent);

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      ogg->pullmode = FALSE;
      ogg->resync = FALSE;
      if (active) {
        ogg->seek_event_thread_stop = FALSE;
        ogg->seek_thread_started = FALSE;
        ogg->seek_event_thread = g_thread_new ("seek_event_thread",
            (GThreadFunc) gst_ogg_demux_loop_push, gst_object_ref (ogg));
        /* And wait for the thread to start.
         * FIXME : This is hackish. And one wonders why we need a separate thread to
         * seek to a certain offset */
        g_mutex_lock (&ogg->seek_event_mutex);
        while (!ogg->seek_thread_started) {
          g_cond_wait (&ogg->thread_started_cond, &ogg->seek_event_mutex);
        }
        g_mutex_unlock (&ogg->seek_event_mutex);
      } else {
        g_mutex_lock (&ogg->seek_event_mutex);
        ogg->seek_event_thread_stop = TRUE;
        g_cond_broadcast (&ogg->seek_event_cond);
        g_mutex_unlock (&ogg->seek_event_mutex);
        g_thread_join (ogg->seek_event_thread);
        ogg->seek_event_thread = NULL;
      }
      res = TRUE;
      break;
    case GST_PAD_MODE_PULL:
      if (active) {
        ogg->need_chains = TRUE;
        ogg->pullmode = TRUE;

        res = gst_pad_start_task (sinkpad, (GstTaskFunction) gst_ogg_demux_loop,
            sinkpad, NULL);
      } else {
        res = gst_pad_stop_task (sinkpad);
      }
      break;
    default:
      res = FALSE;
      break;
  }
  return res;
}

static GstStateChangeReturn
gst_ogg_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstOggDemux *ogg;
  GstStateChangeReturn result = GST_STATE_CHANGE_FAILURE;

  ogg = GST_OGG_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      ogg->basetime = 0;
      ogg_sync_init (&ogg->sync);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      ogg_sync_reset (&ogg->sync);
      ogg->running = FALSE;
      ogg->bitrate = 0;
      ogg->total_time = -1;
      GST_PUSH_LOCK (ogg);
      ogg->push_byte_offset = 0;
      ogg->push_byte_length = -1;
      ogg->push_time_length = GST_CLOCK_TIME_NONE;
      ogg->push_time_offset = GST_CLOCK_TIME_NONE;
      ogg->push_state = PUSH_PLAYING;
      ogg->have_group_id = FALSE;
      ogg->group_id = G_MAXUINT;
      ogg->seqnum = GST_SEQNUM_INVALID;

      ogg->push_disable_seeking = FALSE;
      gst_ogg_demux_query_duration_push (ogg);
      GST_PUSH_UNLOCK (ogg);
      gst_segment_init (&ogg->segment, GST_FORMAT_TIME);
      break;
    default:
      break;
  }

  result = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_ogg_demux_clear_chains (ogg);
      GST_OBJECT_LOCK (ogg);
      ogg->running = FALSE;
      gst_event_replace (&ogg->seek_event, NULL);
      GST_OBJECT_UNLOCK (ogg);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      ogg_sync_clear (&ogg->sync);
      break;
    default:
      break;
  }
  return result;
}

static gboolean
gst_ogg_demux_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_ogg_demux_debug, "oggdemux", 0, "ogg demuxer");
  GST_DEBUG_CATEGORY_INIT (gst_ogg_demux_setup_debug, "oggdemux_setup", 0,
      "ogg demuxer setup stage when parsing pipeline");

#ifdef ENABLE_NLS
  GST_DEBUG ("binding text domain %s to locale dir %s", GETTEXT_PACKAGE,
      LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  return TRUE;
}

/* prints all info about the element */
#undef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT gst_ogg_demux_setup_debug

#ifdef GST_DISABLE_GST_DEBUG

static void
gst_ogg_print (GstOggDemux * ogg)
{
  /* NOP */
}

#else /* !GST_DISABLE_GST_DEBUG */

static void
gst_ogg_print (GstOggDemux * ogg)
{
  guint j, i;

  GST_INFO_OBJECT (ogg, "%u chains", ogg->chains->len);
  GST_INFO_OBJECT (ogg, " total time: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (ogg->total_time));

  for (i = 0; i < ogg->chains->len; i++) {
    GstOggChain *chain = g_array_index (ogg->chains, GstOggChain *, i);

    GST_INFO_OBJECT (ogg, " chain %d (%u streams):", i, chain->streams->len);
    GST_INFO_OBJECT (ogg,
        "  offset: %" G_GINT64_FORMAT " - %" G_GINT64_FORMAT, chain->offset,
        chain->end_offset);
    GST_INFO_OBJECT (ogg, "  begin time: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (chain->begin_time));
    GST_INFO_OBJECT (ogg, "  total time: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (chain->total_time));
    GST_INFO_OBJECT (ogg, "  segment start: %" GST_TIME_FORMAT,
        GST_TIME_ARGS (chain->segment_start));
    GST_INFO_OBJECT (ogg, "  segment stop:  %" GST_TIME_FORMAT,
        GST_TIME_ARGS (chain->segment_stop));

    for (j = 0; j < chain->streams->len; j++) {
      GstOggPad *stream = g_array_index (chain->streams, GstOggPad *, j);

      GST_INFO_OBJECT (ogg, "  stream %08x: %s", stream->map.serialno,
          gst_ogg_stream_get_media_type (&stream->map));
      GST_INFO_OBJECT (ogg, "   start time:       %" GST_TIME_FORMAT,
          GST_TIME_ARGS (stream->start_time));
    }
  }
}
#endif /* GST_DISABLE_GST_DEBUG */
