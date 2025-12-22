/* RIST Retransmission sender element for GStreamer
 *
 * gsristprtxsend.c:
 *
 * Copyright (C) 2013-2019 Collabora Ltd.
 *   @author Julien Isorce <julien.isorce@collabora.co.uk>
 *           Nicoas Dufresne <nicolas.dufresne@collabora.com>
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
 * SECTION:element-ristrtxsend
 * @title: ristrtxsend
 * @see_also: ristrtxreceive
 *
 * This elements replies to custom events 'GstRTPRetransmissionRequest' and
 * when available sends in RIST form the lost packet. This element is intented
 * to be used by ristsink element.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/base/gstdataqueue.h>

#include "gstrist.h"

GST_DEBUG_CATEGORY_STATIC (gst_rist_rtx_send_debug);
#define GST_CAT_DEFAULT gst_rist_rtx_send_debug

#define DEFAULT_MAX_SIZE_TIME    0
#define DEFAULT_MAX_SIZE_PACKETS 100

enum
{
  PROP_0,
  PROP_MAX_SIZE_TIME,
  PROP_MAX_SIZE_PACKETS,
  PROP_NUM_RTX_REQUESTS,
  PROP_NUM_RTX_PACKETS,
};

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, " "clock-rate = (int) [1, MAX]")
    );

struct _GstRistRtxSend
{
  GstElement element;

  /* pad */
  GstPad *sinkpad;
  GstPad *srcpad;

  /* rtp packets that will be pushed out */
  GstDataQueue *queue;

  /* ssrc -> SSRCRtxData */
  GHashTable *ssrc_data;
  /* rtx ssrc -> master ssrc */
  GHashTable *rtx_ssrcs;

  /* buffering control properties */
  guint max_size_time;
  guint max_size_packets;

  /* statistics */
  guint num_rtx_requests;
  guint num_rtx_packets;
};

static gboolean gst_rist_rtx_send_queue_check_full (GstDataQueue * queue,
    guint visible, guint bytes, guint64 time, gpointer checkdata);

static gboolean gst_rist_rtx_send_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_rist_rtx_send_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static GstFlowReturn gst_rist_rtx_send_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);
static GstFlowReturn gst_rist_rtx_send_chain_list (GstPad * pad,
    GstObject * parent, GstBufferList * list);

static void gst_rist_rtx_send_src_loop (GstRistRtxSend * rtx);
static gboolean gst_rist_rtx_send_activate_mode (GstPad * pad,
    GstObject * parent, GstPadMode mode, gboolean active);

static GstStateChangeReturn gst_rist_rtx_send_change_state (GstElement *
    element, GstStateChange transition);

static void gst_rist_rtx_send_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rist_rtx_send_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rist_rtx_send_finalize (GObject * object);

G_DEFINE_TYPE_WITH_CODE (GstRistRtxSend, gst_rist_rtx_send, GST_TYPE_ELEMENT,
    GST_DEBUG_CATEGORY_INIT (gst_rist_rtx_send_debug, "ristrtxsend", 0,
        "RIST retransmission sender"));
GST_ELEMENT_REGISTER_DEFINE (ristrtxsend, "ristrtxsend", GST_RANK_NONE,
    GST_TYPE_RIST_RTX_SEND);

typedef struct
{
  guint32 extseqnum;
  guint32 timestamp;
  GstBuffer *buffer;
} BufferQueueItem;

static void
buffer_queue_item_free (BufferQueueItem * item)
{
  gst_buffer_unref (item->buffer);
  g_free (item);
}

typedef struct
{
  guint32 rtx_ssrc;
  guint16 seqnum_base, next_seqnum;
  gint clock_rate;

  /* history of rtp packets */
  GSequence *queue;
  guint32 max_extseqnum;

  /* current rtcp app seqnum extension */
  gboolean has_seqnum_ext;
  guint16 seqnum_ext;
} SSRCRtxData;

static SSRCRtxData *
ssrc_rtx_data_new (guint32 rtx_ssrc)
{
  SSRCRtxData *data = g_new0 (SSRCRtxData, 1);

  data->rtx_ssrc = rtx_ssrc;
  data->next_seqnum = data->seqnum_base = g_random_int_range (0, G_MAXUINT16);
  data->queue = g_sequence_new ((GDestroyNotify) buffer_queue_item_free);
  data->max_extseqnum = -1;

  return data;
}

static void
ssrc_rtx_data_free (SSRCRtxData * data)
{
  g_sequence_free (data->queue);
  g_free (data);
}

static void
gst_rist_rtx_send_class_init (GstRistRtxSendClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->get_property = gst_rist_rtx_send_get_property;
  gobject_class->set_property = gst_rist_rtx_send_set_property;
  gobject_class->finalize = gst_rist_rtx_send_finalize;

  g_object_class_install_property (gobject_class, PROP_MAX_SIZE_TIME,
      g_param_spec_uint ("max-size-time", "Max Size Time",
          "Amount of ms to queue (0 = unlimited)", 0, G_MAXUINT,
          DEFAULT_MAX_SIZE_TIME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_SIZE_PACKETS,
      g_param_spec_uint ("max-size-packets", "Max Size Packets",
          "Amount of packets to queue (0 = unlimited)", 0, G_MAXINT16,
          DEFAULT_MAX_SIZE_PACKETS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_REQUESTS,
      g_param_spec_uint ("num-rtx-requests", "Num RTX Requests",
          "Number of retransmission events received", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_NUM_RTX_PACKETS,
      g_param_spec_uint ("num-rtx-packets", "Num RTX Packets",
          " Number of retransmission packets sent", 0, G_MAXUINT,
          0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);
  gst_element_class_add_static_pad_template (gstelement_class, &sink_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "RIST Retransmission Sender", "Codec",
      "Retransmit RTP packets when needed, according to VSF TR-06-1",
      "Nicolas Dufresne <nicolas.dufresne@collabora.com>");

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_change_state);
}

static void
gst_rist_rtx_send_reset (GstRistRtxSend * rtx)
{
  GST_OBJECT_LOCK (rtx);
  gst_data_queue_flush (rtx->queue);
  g_hash_table_remove_all (rtx->ssrc_data);
  g_hash_table_remove_all (rtx->rtx_ssrcs);
  rtx->num_rtx_requests = 0;
  rtx->num_rtx_packets = 0;
  GST_OBJECT_UNLOCK (rtx);
}

static void
gst_rist_rtx_send_finalize (GObject * object)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (object);

  g_hash_table_unref (rtx->ssrc_data);
  g_hash_table_unref (rtx->rtx_ssrcs);
  g_object_unref (rtx->queue);

  G_OBJECT_CLASS (gst_rist_rtx_send_parent_class)->finalize (object);
}

static void
gst_rist_rtx_send_init (GstRistRtxSend * rtx)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (rtx);

  rtx->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  GST_PAD_SET_PROXY_CAPS (rtx->srcpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->srcpad);
  gst_pad_set_event_function (rtx->srcpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_src_event));
  gst_pad_set_activatemode_function (rtx->srcpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_activate_mode));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->srcpad);

  rtx->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");
  GST_PAD_SET_PROXY_CAPS (rtx->sinkpad);
  GST_PAD_SET_PROXY_ALLOCATION (rtx->sinkpad);
  gst_pad_set_event_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_sink_event));
  gst_pad_set_chain_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_chain));
  gst_pad_set_chain_list_function (rtx->sinkpad,
      GST_DEBUG_FUNCPTR (gst_rist_rtx_send_chain_list));
  gst_element_add_pad (GST_ELEMENT (rtx), rtx->sinkpad);

  rtx->queue = gst_data_queue_new (gst_rist_rtx_send_queue_check_full, NULL,
      NULL, rtx);
  rtx->ssrc_data = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) ssrc_rtx_data_free);
  rtx->rtx_ssrcs = g_hash_table_new (g_direct_hash, g_direct_equal);

  rtx->max_size_time = DEFAULT_MAX_SIZE_TIME;
  rtx->max_size_packets = DEFAULT_MAX_SIZE_PACKETS;
}

static void
gst_rist_rtx_send_set_flushing (GstRistRtxSend * rtx, gboolean flush)
{
  GST_OBJECT_LOCK (rtx);
  gst_data_queue_set_flushing (rtx->queue, flush);
  gst_data_queue_flush (rtx->queue);
  GST_OBJECT_UNLOCK (rtx);
}

static gboolean
gst_rist_rtx_send_queue_check_full (GstDataQueue * queue,
    guint visible, guint bytes, guint64 time, gpointer checkdata)
{
  return FALSE;
}

static void
gst_rtp_rtx_data_queue_item_free (gpointer item)
{
  GstDataQueueItem *data = item;
  if (data->object)
    gst_mini_object_unref (data->object);
  g_free (data);
}

static gboolean
gst_rist_rtx_send_push_out (GstRistRtxSend * rtx, gpointer object)
{
  GstDataQueueItem *data;
  gboolean success;

  data = g_new0 (GstDataQueueItem, 1);
  data->object = GST_MINI_OBJECT (object);
  data->size = 1;
  data->duration = 1;
  data->visible = TRUE;
  data->destroy = gst_rtp_rtx_data_queue_item_free;

  success = gst_data_queue_push (rtx->queue, data);

  if (!success)
    data->destroy (data);

  return success;
}

static SSRCRtxData *
gst_rist_rtx_send_get_ssrc_data (GstRistRtxSend * rtx, guint32 ssrc)
{
  SSRCRtxData *data;
  guint32 rtx_ssrc = 0;

  data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));
  if (!data) {
    /* See 5.3.2 Retransmitted Packets, original packet have SSRC LSB set to
     * 0, while RTX packet have LSB set to 1 */
    rtx_ssrc = ssrc + 1;
    data = ssrc_rtx_data_new (rtx_ssrc);
    g_hash_table_insert (rtx->ssrc_data, GUINT_TO_POINTER (ssrc), data);
    g_hash_table_insert (rtx->rtx_ssrcs, GUINT_TO_POINTER (rtx_ssrc),
        GUINT_TO_POINTER (ssrc));
  }

  return data;
}

/*
 * see RIST TR-06-1 5.3.2 Retransmitted Packets
 *
 * RIST simply resend the packet verbatim, with SSRC+1, the defaults SSRC always
 * have the LSB set to 0, so we can differentiate the retransmission and the
 * normal packet.
 */
static GstBuffer *
gst_rtp_rist_buffer_new (GstRistRtxSend * rtx, GstBuffer * buffer, guint32 ssrc)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;

  buffer = gst_buffer_copy_deep (buffer);
  gst_rtp_buffer_map (buffer, GST_MAP_WRITE, &rtp);
  gst_rtp_buffer_set_ssrc (&rtp, ssrc + 1);
  gst_rtp_buffer_unmap (&rtp);

  return buffer;
}

static gint
buffer_queue_items_cmp (BufferQueueItem * a, BufferQueueItem * b,
    gpointer user_data)
{
  /* gst_rtp_buffer_compare_seqnum returns the opposite of what we want,
   * it returns negative when seqnum1 > seqnum2 and we want negative
   * when b > a, i.e. a is smaller, so it comes first in the sequence */
  return a->extseqnum - b->extseqnum;
}

static gboolean
gst_rist_rtx_send_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:
    {
      const GstStructure *s = gst_event_get_structure (event);

      /* This event usually comes from the downstream gstrtpsession */
      if (gst_structure_has_name (s, "GstRTPRetransmissionRequest")) {
        guint seqnum = 0;
        guint ssrc = 0;
        GstBuffer *rtx_buf = NULL;

        /* retrieve seqnum of the packet that need to be retransmitted */
        if (!gst_structure_get_uint (s, "seqnum", &seqnum))
          seqnum = -1;

        /* retrieve ssrc of the packet that need to be retransmitted */
        if (!gst_structure_get_uint (s, "ssrc", &ssrc))
          ssrc = -1;

        GST_DEBUG_OBJECT (rtx, "got rtx request for seqnum: %u, ssrc: %X",
            seqnum, ssrc);

        GST_OBJECT_LOCK (rtx);
        /* check if request is for us */
        if (g_hash_table_contains (rtx->ssrc_data, GUINT_TO_POINTER (ssrc))) {
          SSRCRtxData *data;
          GSequenceIter *iter;
          BufferQueueItem search_item;
          guint32 extseqnum;

          /* update statistics */
          ++rtx->num_rtx_requests;

          data = gst_rist_rtx_send_get_ssrc_data (rtx, ssrc);


          if (data->has_seqnum_ext) {
            extseqnum = data->seqnum_ext << 16 | seqnum;
          } else {
            guint32 max_extseqnum = data->max_extseqnum;
            extseqnum = gst_rist_rtp_ext_seq (&max_extseqnum, seqnum);
          }

          search_item.extseqnum = extseqnum;
          iter = g_sequence_lookup (data->queue, &search_item,
              (GCompareDataFunc) buffer_queue_items_cmp, NULL);
          if (iter) {
            BufferQueueItem *item = g_sequence_get (iter);
            GST_LOG_OBJECT (rtx, "found %u (%u:%u)", item->extseqnum,
                item->extseqnum >> 16, item->extseqnum & 0xFFFF);
            rtx_buf = gst_rtp_rist_buffer_new (rtx, item->buffer, ssrc);
          }
#ifndef GST_DISABLE_DEBUG
          else {
            BufferQueueItem *item = NULL;

            iter = g_sequence_get_begin_iter (data->queue);
            if (!g_sequence_iter_is_end (iter))
              item = g_sequence_get (iter);

            if (item && extseqnum < item->extseqnum) {
              GST_DEBUG_OBJECT (rtx, "requested seqnum %u has already been "
                  "removed from the rtx queue; the first available is %u",
                  seqnum, item->extseqnum);
            } else {
              GST_WARNING_OBJECT (rtx, "requested seqnum %u has not been "
                  "transmitted yet in the original stream; either the remote end "
                  "is not configured correctly, or the source is too slow",
                  seqnum);
            }
#endif
          }
        }
        GST_OBJECT_UNLOCK (rtx);

        if (rtx_buf)
          gst_rist_rtx_send_push_out (rtx, rtx_buf);

        gst_event_unref (event);
        return TRUE;
      }
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static gboolean
gst_rist_rtx_send_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (parent);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      gst_pad_push_event (rtx->srcpad, event);
      gst_rist_rtx_send_set_flushing (rtx, TRUE);
      gst_pad_pause_task (rtx->srcpad);
      return TRUE;
    case GST_EVENT_FLUSH_STOP:
      gst_pad_push_event (rtx->srcpad, event);
      gst_rist_rtx_send_set_flushing (rtx, FALSE);
      gst_pad_start_task (rtx->srcpad,
          (GstTaskFunction) gst_rist_rtx_send_src_loop, rtx, NULL);
      return TRUE;
    case GST_EVENT_EOS:
      GST_INFO_OBJECT (rtx, "Got EOS - enqueueing it");
      gst_rist_rtx_send_push_out (rtx, event);
      return TRUE;
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;
      GstStructure *s;
      guint ssrc;
      gint payload;
      SSRCRtxData *data;

      gst_event_parse_caps (event, &caps);

      s = gst_caps_get_structure (caps, 0);
      if (!gst_structure_get_uint (s, "ssrc", &ssrc))
        ssrc = -1;
      if (!gst_structure_get_int (s, "payload", &payload))
        payload = -1;

      if (payload == -1)
        GST_WARNING_OBJECT (rtx, "No payload in caps");

      GST_OBJECT_LOCK (rtx);
      data = gst_rist_rtx_send_get_ssrc_data (rtx, ssrc);

      GST_DEBUG_OBJECT (rtx,
          "got caps for payload: %d->%d, ssrc: %u : %" GST_PTR_FORMAT,
          payload, ssrc, data->rtx_ssrc, caps);

      gst_structure_get_int (s, "clock-rate", &data->clock_rate);

      /* The session might need to know the RTX ssrc */
      caps = gst_caps_copy (caps);
      gst_caps_set_simple (caps, "rtx-ssrc", G_TYPE_UINT, data->rtx_ssrc,
          "rtx-seqnum-offset", G_TYPE_UINT, data->seqnum_base, NULL);

      GST_DEBUG_OBJECT (rtx, "got clock-rate from caps: %d for ssrc: %u",
          data->clock_rate, ssrc);
      GST_OBJECT_UNLOCK (rtx);

      gst_event_unref (event);
      event = gst_event_new_caps (caps);
      gst_caps_unref (caps);
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

/* like rtp_jitter_buffer_get_ts_diff() */
static guint32
gst_rist_rtx_send_get_ts_diff (SSRCRtxData * data)
{
  guint64 high_ts, low_ts;
  BufferQueueItem *high_buf, *low_buf;
  guint32 result;

  high_buf =
      g_sequence_get (g_sequence_iter_prev (g_sequence_get_end_iter
          (data->queue)));
  low_buf = g_sequence_get (g_sequence_get_begin_iter (data->queue));

  if (!high_buf || !low_buf || high_buf == low_buf)
    return 0;

  high_ts = high_buf->timestamp;
  low_ts = low_buf->timestamp;

  /* it needs to work if ts wraps */
  if (high_ts >= low_ts) {
    result = (guint32) (high_ts - low_ts);
  } else {
    result = (guint32) (high_ts + G_MAXUINT32 + 1 - low_ts);
  }

  /* return value in ms instead of clock ticks */
  return (guint32) gst_util_uint64_scale_int (result, 1000, data->clock_rate);
}

/* Must be called with lock */
static void
process_buffer (GstRistRtxSend * rtx, GstBuffer * buffer)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  BufferQueueItem *item;
  SSRCRtxData *data;
  guint16 seqnum;
  guint32 ssrc, rtptime;
  guint16 bits;
  gpointer extdata;
  guint extlen;
  gboolean has_seqnum_ext = FALSE;
  guint32 extseqnum;

  /* read the information we want from the buffer */
  gst_rtp_buffer_map (buffer, GST_MAP_READ, &rtp);
  seqnum = gst_rtp_buffer_get_seq (&rtp);
  ssrc = gst_rtp_buffer_get_ssrc (&rtp);
  rtptime = gst_rtp_buffer_get_timestamp (&rtp);
  if (gst_rtp_buffer_get_extension_data (&rtp, &bits, &extdata, &extlen)) {
    /* Has header extension */
    has_seqnum_ext = (bits >> 14) & 1;  /* E */
    if (extlen != 1)
      has_seqnum_ext = FALSE;
    if (has_seqnum_ext)
      extseqnum = GST_READ_UINT16_BE (extdata) << 16 | seqnum;
  }
  gst_rtp_buffer_unmap (&rtp);

  GST_TRACE_OBJECT (rtx, "Processing buffer seqnum: %u, ssrc: %X", seqnum,
      ssrc);

  data = gst_rist_rtx_send_get_ssrc_data (rtx, ssrc);

  if (has_seqnum_ext)
    data->max_extseqnum = MAX (data->max_extseqnum, extseqnum);
  else
    extseqnum = gst_rist_rtp_ext_seq (&data->max_extseqnum, seqnum);

  /* add current rtp buffer to queue history */
  item = g_new0 (BufferQueueItem, 1);
  item->extseqnum = extseqnum;
  item->timestamp = rtptime;
  item->buffer = gst_buffer_ref (buffer);
  g_sequence_append (data->queue, item);

  /* remove oldest packets from history if they are too many */
  if (rtx->max_size_packets) {
    while (g_sequence_get_length (data->queue) > rtx->max_size_packets)
      g_sequence_remove (g_sequence_get_begin_iter (data->queue));
  }
  if (rtx->max_size_time) {
    while (gst_rist_rtx_send_get_ts_diff (data) > rtx->max_size_time)
      g_sequence_remove (g_sequence_get_begin_iter (data->queue));
  }
}

static GstFlowReturn
gst_rist_rtx_send_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (parent);
  GstFlowReturn ret;

  GST_OBJECT_LOCK (rtx);
  process_buffer (rtx, buffer);
  GST_OBJECT_UNLOCK (rtx);
  ret = gst_pad_push (rtx->srcpad, buffer);

  return ret;
}

static gboolean
process_buffer_from_list (GstBuffer ** buffer, guint idx, gpointer user_data)
{
  process_buffer (user_data, *buffer);
  return TRUE;
}

static GstFlowReturn
gst_rist_rtx_send_chain_list (GstPad * pad, GstObject * parent,
    GstBufferList * list)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (parent);
  GstFlowReturn ret;

  GST_OBJECT_LOCK (rtx);
  gst_buffer_list_foreach (list, process_buffer_from_list, rtx);
  GST_OBJECT_UNLOCK (rtx);

  ret = gst_pad_push_list (rtx->srcpad, list);

  return ret;
}

static void
gst_rist_rtx_send_src_loop (GstRistRtxSend * rtx)
{
  GstDataQueueItem *data;

  if (gst_data_queue_pop (rtx->queue, &data)) {
    GST_LOG_OBJECT (rtx, "pushing rtx buffer %p", data->object);

    if (G_LIKELY (GST_IS_BUFFER (data->object))) {
      GST_OBJECT_LOCK (rtx);
      /* Update statistics just before pushing. */
      rtx->num_rtx_packets++;
      GST_OBJECT_UNLOCK (rtx);

      gst_pad_push (rtx->srcpad, GST_BUFFER (data->object));
    } else if (GST_IS_EVENT (data->object)) {
      gst_pad_push_event (rtx->srcpad, GST_EVENT (data->object));

      /* after EOS, we should not send any more buffers,
       * even if there are more requests coming in */
      if (GST_EVENT_TYPE (data->object) == GST_EVENT_EOS) {
        gst_rist_rtx_send_set_flushing (rtx, TRUE);
      }
    } else {
      g_assert_not_reached ();
    }

    data->object = NULL;        /* we no longer own that object */
    data->destroy (data);
  } else {
    GST_LOG_OBJECT (rtx, "flushing");
    gst_pad_pause_task (rtx->srcpad);
  }
}

static gboolean
gst_rist_rtx_send_activate_mode (GstPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (parent);
  gboolean ret = FALSE;

  switch (mode) {
    case GST_PAD_MODE_PUSH:
      if (active) {
        gst_rist_rtx_send_set_flushing (rtx, FALSE);
        ret = gst_pad_start_task (rtx->srcpad,
            (GstTaskFunction) gst_rist_rtx_send_src_loop, rtx, NULL);
      } else {
        gst_rist_rtx_send_set_flushing (rtx, TRUE);
        ret = gst_pad_stop_task (rtx->srcpad);
      }
      GST_INFO_OBJECT (rtx, "activate_mode: active %d, ret %d", active, ret);
      break;
    default:
      break;
  }
  return ret;
}

static void
gst_rist_rtx_send_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (object);

  switch (prop_id) {
    case PROP_MAX_SIZE_TIME:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->max_size_time);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_SIZE_PACKETS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->max_size_packets);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_NUM_RTX_REQUESTS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_requests);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_NUM_RTX_PACKETS:
      GST_OBJECT_LOCK (rtx);
      g_value_set_uint (value, rtx->num_rtx_packets);
      GST_OBJECT_UNLOCK (rtx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rist_rtx_send_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (object);

  switch (prop_id) {
    case PROP_MAX_SIZE_TIME:
      GST_OBJECT_LOCK (rtx);
      rtx->max_size_time = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (rtx);
      break;
    case PROP_MAX_SIZE_PACKETS:
      GST_OBJECT_LOCK (rtx);
      rtx->max_size_packets = g_value_get_uint (value);
      GST_OBJECT_UNLOCK (rtx);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_rist_rtx_send_change_state (GstElement * element, GstStateChange transition)
{
  GstStateChangeReturn ret;
  GstRistRtxSend *rtx = GST_RIST_RTX_SEND (element);

  ret =
      GST_ELEMENT_CLASS (gst_rist_rtx_send_parent_class)->change_state (element,
      transition);

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_rist_rtx_send_reset (rtx);
      break;
    default:
      break;
  }

  return ret;
}

void
gst_rist_rtx_send_set_extseqnum (GstRistRtxSend * rtx, guint32 ssrc,
    guint16 seqnum_ext)
{
  SSRCRtxData *data;

  GST_OBJECT_LOCK (rtx);
  data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));

  if (data) {
    data->has_seqnum_ext = TRUE;
    data->seqnum_ext = seqnum_ext;
  }
  GST_OBJECT_UNLOCK (rtx);
}

void
gst_rist_rtx_send_clear_extseqnum (GstRistRtxSend * rtx, guint32 ssrc)
{
  SSRCRtxData *data;

  GST_OBJECT_LOCK (rtx);
  data = g_hash_table_lookup (rtx->ssrc_data, GUINT_TO_POINTER (ssrc));

  if (data)
    data->has_seqnum_ext = FALSE;
  GST_OBJECT_UNLOCK (rtx);
}
