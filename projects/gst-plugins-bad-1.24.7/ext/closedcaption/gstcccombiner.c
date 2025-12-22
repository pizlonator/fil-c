/*
 * GStreamer
 * Copyright (C) 2018 Sebastian Dröge <sebastian@centricular.com>
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
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/video/video.h>
#include <string.h>

#include "ccutils.h"
#include "gstcccombiner.h"

GST_DEBUG_CATEGORY_STATIC (gst_cc_combiner_debug);
#define GST_CAT_DEFAULT gst_cc_combiner_debug

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate captiontemplate =
    GST_STATIC_PAD_TEMPLATE ("caption",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS
    ("closedcaption/x-cea-608,format={ (string) raw, (string) s334-1a}; "
        "closedcaption/x-cea-708,format={ (string) cc_data, (string) cdp }"));

#define parent_class gst_cc_combiner_parent_class
G_DEFINE_TYPE (GstCCCombiner, gst_cc_combiner, GST_TYPE_AGGREGATOR);
GST_ELEMENT_REGISTER_DEFINE (cccombiner, "cccombiner",
    GST_RANK_NONE, GST_TYPE_CCCOMBINER);

enum
{
  PROP_0,
  PROP_SCHEDULE,
  PROP_OUTPUT_PADDING,
  PROP_MAX_SCHEDULED,
};

#define DEFAULT_MAX_SCHEDULED 30
#define DEFAULT_SCHEDULE TRUE
#define DEFAULT_OUTPUT_PADDING TRUE

typedef struct
{
  GstVideoCaptionType caption_type;
  GstBuffer *buffer;
} CaptionData;

typedef struct
{
  GstBuffer *buffer;
  GstClockTime running_time;
  GstClockTime stream_time;
} CaptionQueueItem;

static void
caption_data_clear (CaptionData * data)
{
  gst_buffer_unref (data->buffer);
}

static void
gst_cc_combiner_finalize (GObject * object)
{
  GstCCCombiner *self = GST_CCCOMBINER (object);

  g_array_unref (self->current_frame_captions);
  self->current_frame_captions = NULL;

  gst_clear_object (&self->cc_buffer);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

#define GST_FLOW_NEED_DATA GST_FLOW_CUSTOM_SUCCESS

static guint
extract_cdp (GstCCCombiner * self, const guint8 * cdp, guint cdp_len,
    guint8 * cc_data)
{
  const struct cdp_fps_entry *out_fps_entry;
  GstVideoTimeCode tc = GST_VIDEO_TIME_CODE_INIT;

  return convert_cea708_cdp_to_cc_data (GST_OBJECT (self), cdp, cdp_len,
      cc_data, &tc, &out_fps_entry);
}

#define MAX_CEA608_LEN 32
#define CDP_MODE (GST_CC_CDP_MODE_CC_DATA | GST_CC_CDP_MODE_TIME_CODE)

static GstBuffer *
make_cdp_buffer (GstCCCombiner * self, const guint8 * cc_data,
    guint cc_data_len, const struct cdp_fps_entry *fps_entry,
    const GstVideoTimeCode * tc)
{
  guint len;
  GstBuffer *ret = gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
  GstMapInfo map;

  gst_buffer_map (ret, &map, GST_MAP_WRITE);

  len = convert_cea708_cc_data_to_cdp (GST_OBJECT (self), CDP_MODE,
      self->cdp_hdr_sequence_cntr, cc_data, cc_data_len, map.data, map.size,
      tc, fps_entry);
  self->cdp_hdr_sequence_cntr++;

  gst_buffer_unmap (ret, &map);

  gst_buffer_set_size (ret, len);

  return ret;
}

static GstBuffer *
make_buffer (GstCCCombiner * self, const guint8 * cc_data, guint cc_data_len)
{
  GstBuffer *ret = gst_buffer_new_allocate (NULL, cc_data_len, NULL);
  gst_buffer_fill (ret, 0, cc_data, cc_data_len);
  return ret;
}

static void
write_cc_data_to (GstCCCombiner * self, GstBuffer * buffer)
{
  GstMapInfo map;
  guint len;

  gst_buffer_map (buffer, &map, GST_MAP_WRITE);
  len = map.size;
  cc_buffer_take_cc_data (self->cc_buffer, self->cdp_fps_entry, TRUE, map.data,
      &len);
  gst_buffer_unmap (buffer, &map);
  gst_buffer_set_size (buffer, len);
}

static void
prepend_s334_to_cea608 (guint field, guint8 * data, guint * len,
    guint alloc_len)
{
  int i;

  g_assert (*len / 2 * 3 <= alloc_len);

  for (i = *len / 2; i >= 0; i--) {
    data[i * 3 + 2] = data[i * 2 + 1];
    data[i * 3 + 1] = data[i * 2 + 0];
    data[i * 3 + 0] = field == 0 ? 0x80 : 0x00;
  }

  *len = *len * 3 / 2;
}

static void
take_s334_both_fields (GstCCCombiner * self, GstBuffer * buffer)
{
  GstMapInfo out = GST_MAP_INFO_INIT;
  guint s334_len, cc_data_len, i;

  gst_buffer_map (buffer, &out, GST_MAP_READWRITE);

  cc_data_len = out.size;
  cc_buffer_take_cc_data (self->cc_buffer, self->cdp_fps_entry, FALSE,
      out.data, &cc_data_len);
  s334_len = drop_ccp_from_cc_data (out.data, cc_data_len);
  if (s334_len < 0) {
    s334_len = 0;
    goto out;
  }

  for (i = 0; i < s334_len / 3; i++) {
    guint byte = out.data[i * 3];
    /* We have to assume a line offset of 0 */
    out.data[i * 3] = (byte == 0xfc || byte == 0xf8) ? 0x80 : 0x00;
  }

out:
  gst_buffer_unmap (buffer, &out);
  gst_buffer_set_size (buffer, s334_len);
}

static void
schedule_cdp (GstCCCombiner * self, const GstVideoTimeCode * tc,
    const guint8 * data, guint len, GstClockTime pts, GstClockTime duration)
{
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint cc_data_len;

  cc_data_len = extract_cdp (self, data, len, cc_data);
  if (cc_buffer_push_cc_data (self->cc_buffer, cc_data, cc_data_len))
    self->current_scheduled++;
}

static void
schedule_cea608_s334_1a (GstCCCombiner * self, guint8 * data, guint len,
    GstClockTime pts, GstClockTime duration)
{
  guint8 field0_data[3], field1_data[3];
  guint field0_len = 0, field1_len = 0;
  guint i;

  if (len % 3 != 0) {
    GST_WARNING ("Invalid cc_data buffer size %u. Truncating to a multiple "
        "of 3", len);
    len = len - (len % 3);
  }

  for (i = 0; i < len / 3; i++) {
    if (data[i * 3] & 0x80) {
      if (data[i * 3 + 1] == 0x80 && data[i * 3 + 2] == 0x80)
        continue;

      field0_data[field0_len++] = data[i * 3 + 1];
      field0_data[field0_len++] = data[i * 3 + 2];
    } else {
      if (data[i * 3 + 1] == 0x80 && data[i * 3 + 2] == 0x80)
        continue;

      field1_data[field1_len++] = data[i * 3 + 1];
      field1_data[field1_len++] = data[i * 3 + 2];
    }
  }

  if (cc_buffer_push_separated (self->cc_buffer, field0_data, field0_len,
          field1_data, field1_len, NULL, 0))
    self->current_scheduled++;
}

static void
schedule_cea708_raw (GstCCCombiner * self, guint8 * data, guint len,
    GstClockTime pts, GstClockTime duration)
{
  if (cc_buffer_push_cc_data (self->cc_buffer, data, len))
    self->current_scheduled++;
}

static void
schedule_cea608_raw (GstCCCombiner * self, guint8 * data, guint len)
{
  if (cc_buffer_push_separated (self->cc_buffer, data, len, NULL, 0, NULL, 0))
    self->current_scheduled++;
}

static void
schedule_caption (GstCCCombiner * self, GstBuffer * caption_buf,
    const GstVideoTimeCode * tc)
{
  GstMapInfo map;
  GstClockTime pts, duration;

  pts = GST_BUFFER_PTS (caption_buf);
  duration = GST_BUFFER_DURATION (caption_buf);

  if (self->current_scheduled + 1 >= self->max_scheduled) {
    GstClockTime stream_time, running_time;
    GstAggregatorPad *caption_pad;

    caption_pad =
        GST_AGGREGATOR_PAD_CAST (gst_element_get_static_pad (GST_ELEMENT_CAST
            (self), "caption"));

    GST_WARNING_OBJECT (self,
        "scheduled queue runs too long, discarding stored");

    running_time =
        gst_segment_to_running_time (&caption_pad->segment, GST_FORMAT_TIME,
        pts);
    stream_time =
        gst_segment_to_stream_time (&caption_pad->segment, GST_FORMAT_TIME,
        pts);

    gst_element_post_message (GST_ELEMENT_CAST (self),
        gst_message_new_qos (GST_OBJECT_CAST (self), FALSE,
            running_time, stream_time, pts, duration));

    cc_buffer_discard (self->cc_buffer);
    self->current_scheduled = 0;

    gst_clear_object (&caption_pad);
  }

  gst_buffer_map (caption_buf, &map, GST_MAP_READ);

  switch (self->caption_type) {
    case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
      schedule_cdp (self, tc, map.data, map.size, pts, duration);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
      schedule_cea708_raw (self, map.data, map.size, pts, duration);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
      schedule_cea608_s334_1a (self, map.data, map.size, pts, duration);
      break;
    case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
      schedule_cea608_raw (self, map.data, map.size);
      break;
    default:
      break;
  }

  gst_buffer_unmap (caption_buf, &map);
}

static void
dequeue_caption (GstCCCombiner * self, GstVideoTimeCode * tc, gboolean drain)
{
  guint8 cea608_1[MAX_CEA608_LEN], cea608_2[MAX_CEA608_LEN];
  guint8 cc_data[MAX_CDP_PACKET_LEN];
  guint cea608_1_len = MAX_CEA608_LEN, cea608_2_len = MAX_CEA608_LEN;
  guint cc_data_len = MAX_CDP_PACKET_LEN;
  CaptionData caption_data;

  g_assert (self->current_frame_captions->len == 0);

  if (drain && cc_buffer_is_empty (self->cc_buffer))
    return;

  caption_data.caption_type = self->caption_type;
  switch (self->caption_type) {
    case GST_VIDEO_CAPTION_TYPE_CEA708_CDP:
    {
      /* Only relevant in alternate and mixed mode, no need to look at the caps */
      if (GST_BUFFER_FLAG_IS_SET (self->current_video_buffer,
              GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
        if (!GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (self->current_video_buffer)) {
          cc_buffer_take_cc_data (self->cc_buffer, self->cdp_fps_entry, TRUE,
              cc_data, &cc_data_len);
          caption_data.buffer =
              make_cdp_buffer (self, cc_data, cc_data_len, self->cdp_fps_entry,
              tc);
          g_array_append_val (self->current_frame_captions, caption_data);
        }
      } else {
        cc_buffer_take_cc_data (self->cc_buffer, self->cdp_fps_entry, TRUE,
            cc_data, &cc_data_len);
        caption_data.buffer =
            make_cdp_buffer (self, cc_data, cc_data_len, self->cdp_fps_entry,
            tc);
        g_array_append_val (self->current_frame_captions, caption_data);
      }
      break;
    }
    case GST_VIDEO_CAPTION_TYPE_CEA708_RAW:
    {
      /* Only relevant in alternate and mixed mode, no need to look at the caps */
      if (GST_BUFFER_FLAG_IS_SET (self->current_video_buffer,
              GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
        if (!GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (self->current_video_buffer)) {
          caption_data.buffer =
              gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
          write_cc_data_to (self, caption_data.buffer);
          g_array_append_val (self->current_frame_captions, caption_data);
        }
      } else {
        caption_data.buffer =
            gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
        write_cc_data_to (self, caption_data.buffer);
        g_array_append_val (self->current_frame_captions, caption_data);
      }
      break;
    }
    case GST_VIDEO_CAPTION_TYPE_CEA608_S334_1A:
    {
      if (self->progressive) {
        cc_buffer_take_separated (self->cc_buffer, self->cdp_fps_entry,
            cea608_1, &cea608_1_len, cea608_2, &cea608_2_len, cc_data,
            &cc_data_len);
        prepend_s334_to_cea608 (0, cea608_1, &cea608_1_len, sizeof (cea608_1));
        caption_data.buffer = make_buffer (self, cea608_1, cea608_1_len);
        g_array_append_val (self->current_frame_captions, caption_data);
      } else if (GST_BUFFER_FLAG_IS_SET (self->current_video_buffer,
              GST_VIDEO_BUFFER_FLAG_INTERLACED) &&
          GST_BUFFER_FLAG_IS_SET (self->current_video_buffer,
              GST_VIDEO_BUFFER_FLAG_ONEFIELD)) {
        cc_buffer_take_separated (self->cc_buffer, self->cdp_fps_entry,
            cea608_1, &cea608_1_len, cea608_2, &cea608_2_len, cc_data,
            &cc_data_len);
        if (GST_VIDEO_BUFFER_IS_TOP_FIELD (self->current_video_buffer)) {
          prepend_s334_to_cea608 (0, cea608_1, &cea608_1_len,
              sizeof (cea608_1));
          caption_data.buffer = make_buffer (self, cea608_1, cea608_1_len);
        } else {
          prepend_s334_to_cea608 (1, cea608_2, &cea608_2_len,
              sizeof (cea608_2));
          caption_data.buffer = make_buffer (self, cea608_2, cea608_2_len);
        }
        g_array_append_val (self->current_frame_captions, caption_data);
      } else {
        caption_data.buffer =
            gst_buffer_new_allocate (NULL, MAX_CDP_PACKET_LEN, NULL);
        take_s334_both_fields (self, caption_data.buffer);
        g_array_append_val (self->current_frame_captions, caption_data);
      }
      break;
    }
    case GST_VIDEO_CAPTION_TYPE_CEA608_RAW:
    {
      cc_buffer_take_separated (self->cc_buffer, self->cdp_fps_entry,
          cea608_1, &cea608_1_len, cea608_2, &cea608_2_len, cc_data,
          &cc_data_len);
      if (self->progressive) {
        caption_data.buffer = make_buffer (self, cea608_1, cea608_1_len);
        g_array_append_val (self->current_frame_captions, caption_data);
      } else if (GST_BUFFER_FLAG_IS_SET (self->current_video_buffer,
              GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
        if (!GST_VIDEO_BUFFER_IS_BOTTOM_FIELD (self->current_video_buffer)) {
          caption_data.buffer = make_buffer (self, cea608_1, cea608_1_len);
          g_array_append_val (self->current_frame_captions, caption_data);
        }
      } else {
        caption_data.buffer = make_buffer (self, cea608_1, cea608_1_len);
        g_array_append_val (self->current_frame_captions, caption_data);
      }
      break;
    }
    default:
      break;
  }
}

static GstFlowReturn
gst_cc_combiner_collect_captions (GstCCCombiner * self, gboolean timeout)
{
  GstAggregatorPad *src_pad =
      GST_AGGREGATOR_PAD (GST_AGGREGATOR_SRC_PAD (self));
  GstAggregatorPad *caption_pad;
  GstBuffer *video_buf;
  GstVideoTimeCodeMeta *tc_meta;
  GstVideoTimeCode *tc = NULL;
  gboolean caption_pad_is_eos = FALSE;

  g_assert (self->current_video_buffer != NULL);

  caption_pad =
      GST_AGGREGATOR_PAD_CAST (gst_element_get_static_pad (GST_ELEMENT_CAST
          (self), "caption"));
  /* No caption pad, forward buffer directly */
  if (!caption_pad) {
    GST_LOG_OBJECT (self, "No caption pad, passing through video");
    video_buf = self->current_video_buffer;
    gst_aggregator_selected_samples (GST_AGGREGATOR_CAST (self),
        GST_BUFFER_PTS (video_buf), GST_BUFFER_DTS (video_buf),
        GST_BUFFER_DURATION (video_buf), NULL);
    self->current_video_buffer = NULL;
    goto done;
  }

  tc_meta = gst_buffer_get_video_time_code_meta (self->current_video_buffer);

  if (tc_meta) {
    tc = &tc_meta->tc;
  }

  GST_LOG_OBJECT (self, "Trying to collect captions for queued video buffer");
  do {
    GstBuffer *caption_buf;
    GstClockTime caption_time;
    CaptionData caption_data;

    caption_buf = gst_aggregator_pad_peek_buffer (caption_pad);
    if (!caption_buf) {
      if (gst_aggregator_pad_is_eos (caption_pad)) {
        GST_DEBUG_OBJECT (self, "Caption pad is EOS, we're done");

        caption_pad_is_eos = TRUE;
        break;
      } else if (!timeout) {
        GST_DEBUG_OBJECT (self, "Need more caption data");
        gst_object_unref (caption_pad);
        return GST_FLOW_NEED_DATA;
      } else {
        GST_DEBUG_OBJECT (self, "No caption data on timeout");
        break;
      }
    }

    caption_time = GST_BUFFER_PTS (caption_buf);
    if (!GST_CLOCK_TIME_IS_VALID (caption_time)) {
      GST_ERROR_OBJECT (self, "Caption buffer without PTS");

      gst_buffer_unref (caption_buf);
      gst_object_unref (caption_pad);

      return GST_FLOW_ERROR;
    }

    caption_time =
        gst_segment_to_running_time (&caption_pad->segment, GST_FORMAT_TIME,
        caption_time);

    if (!GST_CLOCK_TIME_IS_VALID (caption_time)) {
      GST_DEBUG_OBJECT (self, "Caption buffer outside segment, dropping");

      gst_aggregator_pad_drop_buffer (caption_pad);
      gst_buffer_unref (caption_buf);

      continue;
    }

    if (gst_buffer_get_size (caption_buf) == 0 &&
        GST_BUFFER_FLAG_IS_SET (caption_buf, GST_BUFFER_FLAG_GAP)) {
      /* This is a gap, we can go ahead. We only consume it once its end point
       * is behind the current video running time. Important to note that
       * we can't deal with gaps with no duration (-1)
       */
      if (!GST_CLOCK_TIME_IS_VALID (GST_BUFFER_DURATION (caption_buf))) {
        GST_ERROR_OBJECT (self, "GAP buffer without a duration");

        gst_buffer_unref (caption_buf);
        gst_object_unref (caption_pad);

        return GST_FLOW_ERROR;
      }

      gst_buffer_unref (caption_buf);

      if (caption_time + GST_BUFFER_DURATION (caption_buf) <
          self->current_video_running_time_end) {
        gst_aggregator_pad_drop_buffer (caption_pad);
        continue;
      } else {
        break;
      }
    }

    /* Collected all caption buffers for this video buffer */
    if (caption_time >= self->current_video_running_time_end) {
      gst_buffer_unref (caption_buf);
      break;
    } else if (!self->schedule) {
      if (GST_CLOCK_TIME_IS_VALID (self->previous_video_running_time_end)) {
        if (caption_time < self->previous_video_running_time_end) {
          GST_WARNING_OBJECT (self,
              "Caption buffer before end of last video frame, dropping");

          gst_aggregator_pad_drop_buffer (caption_pad);
          gst_buffer_unref (caption_buf);
          continue;
        }
      } else if (caption_time < self->current_video_running_time) {
        GST_WARNING_OBJECT (self,
            "Caption buffer before current video frame, dropping");

        gst_aggregator_pad_drop_buffer (caption_pad);
        gst_buffer_unref (caption_buf);
        continue;
      }
    }

    /* This caption buffer has to be collected */
    GST_LOG_OBJECT (self,
        "Collecting caption buffer %p %" GST_TIME_FORMAT " for video buffer %p",
        caption_buf, GST_TIME_ARGS (caption_time), self->current_video_buffer);

    caption_data.caption_type = self->caption_type;

    gst_aggregator_pad_drop_buffer (caption_pad);

    if (!self->schedule) {
      caption_data.buffer = caption_buf;
      g_array_append_val (self->current_frame_captions, caption_data);
    } else {
      schedule_caption (self, caption_buf, tc);
      gst_buffer_unref (caption_buf);
    }
  } while (TRUE);

  if (self->schedule) {
    dequeue_caption (self, tc, caption_pad_is_eos);
  }

  gst_aggregator_selected_samples (GST_AGGREGATOR_CAST (self),
      GST_BUFFER_PTS (self->current_video_buffer),
      GST_BUFFER_DTS (self->current_video_buffer),
      GST_BUFFER_DURATION (self->current_video_buffer), NULL);

  GST_LOG_OBJECT (self, "Attaching %u captions to buffer %p",
      self->current_frame_captions->len, self->current_video_buffer);

  if (self->current_frame_captions->len > 0) {
    guint i;

    if (self->schedule)
      self->current_scheduled = MAX (1, self->current_scheduled) - 1;

    video_buf = gst_buffer_make_writable (self->current_video_buffer);
    self->current_video_buffer = NULL;

    for (i = 0; i < self->current_frame_captions->len; i++) {
      CaptionData *caption_data =
          &g_array_index (self->current_frame_captions, CaptionData, i);
      GstMapInfo map;

      gst_buffer_map (caption_data->buffer, &map, GST_MAP_READ);
      gst_buffer_add_video_caption_meta (video_buf, caption_data->caption_type,
          map.data, map.size);
      gst_buffer_unmap (caption_data->buffer, &map);
    }

    g_array_set_size (self->current_frame_captions, 0);
  } else {
    GST_LOG_OBJECT (self, "No captions for buffer %p",
        self->current_video_buffer);
    video_buf = self->current_video_buffer;
    self->current_video_buffer = NULL;
  }

  gst_object_unref (caption_pad);

done:
  src_pad->segment.position =
      GST_BUFFER_PTS (video_buf) + GST_BUFFER_DURATION (video_buf);

  return gst_aggregator_finish_buffer (GST_AGGREGATOR_CAST (self), video_buf);
}

static GstFlowReturn
gst_cc_combiner_aggregate (GstAggregator * aggregator, gboolean timeout)
{
  GstCCCombiner *self = GST_CCCOMBINER (aggregator);
  GstFlowReturn flow_ret = GST_FLOW_OK;

  /* If we have no current video buffer, queue one. If we have one but
   * its end running time is not known yet, try to determine it from the
   * next video buffer */
  if (!self->current_video_buffer
      || !GST_CLOCK_TIME_IS_VALID (self->current_video_running_time_end)) {
    GstAggregatorPad *video_pad;
    GstClockTime video_start;
    GstBuffer *video_buf;

    video_pad =
        GST_AGGREGATOR_PAD_CAST (gst_element_get_static_pad (GST_ELEMENT_CAST
            (aggregator), "sink"));
    video_buf = gst_aggregator_pad_peek_buffer (video_pad);
    if (!video_buf) {
      if (gst_aggregator_pad_is_eos (video_pad)) {
        GST_DEBUG_OBJECT (aggregator, "Video pad is EOS, we're done");

        /* Assume that this buffer ends where it started +50ms (25fps) and handle it */
        if (self->current_video_buffer) {
          self->current_video_running_time_end =
              self->current_video_running_time + 50 * GST_MSECOND;
          flow_ret = gst_cc_combiner_collect_captions (self, timeout);
        }

        /* If we collected all captions for the remaining video frame we're
         * done, otherwise get called another time and go directly into the
         * outer branch for finishing the current video frame */
        if (flow_ret == GST_FLOW_NEED_DATA)
          flow_ret = GST_FLOW_OK;
        else
          flow_ret = GST_FLOW_EOS;
      } else {
        flow_ret = GST_FLOW_OK;
      }

      gst_object_unref (video_pad);
      return flow_ret;
    }

    video_start = GST_BUFFER_PTS (video_buf);
    if (!GST_CLOCK_TIME_IS_VALID (video_start)) {
      gst_buffer_unref (video_buf);
      gst_object_unref (video_pad);

      GST_ERROR_OBJECT (aggregator, "Video buffer without PTS");

      return GST_FLOW_ERROR;
    }

    video_start =
        gst_segment_to_running_time (&video_pad->segment, GST_FORMAT_TIME,
        video_start);
    if (!GST_CLOCK_TIME_IS_VALID (video_start)) {
      GST_DEBUG_OBJECT (aggregator, "Buffer outside segment, dropping");
      gst_aggregator_pad_drop_buffer (video_pad);
      gst_buffer_unref (video_buf);
      gst_object_unref (video_pad);
      return GST_FLOW_OK;
    }

    if (self->current_video_buffer) {
      /* If we already have a video buffer just update the current end running
       * time accordingly. That's what was missing and why we got here */
      self->current_video_running_time_end = video_start;
      gst_buffer_unref (video_buf);
      GST_LOG_OBJECT (self,
          "Determined end timestamp for video buffer: %p %" GST_TIME_FORMAT
          " - %" GST_TIME_FORMAT, self->current_video_buffer,
          GST_TIME_ARGS (self->current_video_running_time),
          GST_TIME_ARGS (self->current_video_running_time_end));
    } else {
      /* Otherwise we had no buffer queued currently. Let's do that now
       * so that we can collect captions for it */
      gst_buffer_replace (&self->current_video_buffer, video_buf);
      self->current_video_running_time = video_start;
      gst_aggregator_pad_drop_buffer (video_pad);
      gst_buffer_unref (video_buf);

      if (GST_BUFFER_DURATION_IS_VALID (video_buf)) {
        GstClockTime end_time =
            GST_BUFFER_PTS (video_buf) + GST_BUFFER_DURATION (video_buf);
        if (video_pad->segment.stop != -1 && end_time > video_pad->segment.stop)
          end_time = video_pad->segment.stop;
        self->current_video_running_time_end =
            gst_segment_to_running_time (&video_pad->segment, GST_FORMAT_TIME,
            end_time);
      } else if (self->video_fps_n != 0 && self->video_fps_d != 0) {
        GstClockTime end_time =
            GST_BUFFER_PTS (video_buf) + gst_util_uint64_scale_int (GST_SECOND,
            self->video_fps_d, self->video_fps_n);
        if (video_pad->segment.stop != -1 && end_time > video_pad->segment.stop)
          end_time = video_pad->segment.stop;
        self->current_video_running_time_end =
            gst_segment_to_running_time (&video_pad->segment, GST_FORMAT_TIME,
            end_time);
      } else {
        self->current_video_running_time_end = GST_CLOCK_TIME_NONE;
      }

      GST_LOG_OBJECT (self,
          "Queued new video buffer: %p %" GST_TIME_FORMAT " - %"
          GST_TIME_FORMAT, self->current_video_buffer,
          GST_TIME_ARGS (self->current_video_running_time),
          GST_TIME_ARGS (self->current_video_running_time_end));
    }

    gst_object_unref (video_pad);
  }

  /* At this point we have a video buffer queued and can start collecting
   * caption buffers for it */
  g_assert (self->current_video_buffer != NULL);
  g_assert (GST_CLOCK_TIME_IS_VALID (self->current_video_running_time));
  g_assert (GST_CLOCK_TIME_IS_VALID (self->current_video_running_time_end));

  flow_ret = gst_cc_combiner_collect_captions (self, timeout);

  /* Only if we collected all captions we replace the current video buffer
   * with NULL and continue with the next one on the next call */
  if (flow_ret == GST_FLOW_NEED_DATA) {
    flow_ret = GST_FLOW_OK;
  } else {
    gst_buffer_replace (&self->current_video_buffer, NULL);
    self->previous_video_running_time_end =
        self->current_video_running_time_end;
    self->current_video_running_time = self->current_video_running_time_end =
        GST_CLOCK_TIME_NONE;
  }

  return flow_ret;
}

static gboolean
gst_cc_combiner_sink_event (GstAggregator * aggregator,
    GstAggregatorPad * agg_pad, GstEvent * event)
{
  GstCCCombiner *self = GST_CCCOMBINER (aggregator);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:{
      GstCaps *caps;
      GstStructure *s;

      gst_event_parse_caps (event, &caps);
      s = gst_caps_get_structure (caps, 0);

      if (strcmp (GST_OBJECT_NAME (agg_pad), "caption") == 0) {
        GstVideoCaptionType caption_type =
            gst_video_caption_type_from_caps (caps);

        if (self->caption_type != GST_VIDEO_CAPTION_TYPE_UNKNOWN &&
            caption_type != self->caption_type) {
          GST_ERROR_OBJECT (self, "Changing caption type is not allowed");

          GST_ELEMENT_ERROR (self, CORE, NEGOTIATION, (NULL),
              ("Changing caption type is not allowed"));

          return FALSE;
        }
        self->caption_type = caption_type;
      } else {
        gint fps_n, fps_d;
        const gchar *interlace_mode;

        fps_n = fps_d = 0;

        gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d);

        interlace_mode = gst_structure_get_string (s, "interlace-mode");

        self->progressive = !interlace_mode
            || !g_strcmp0 (interlace_mode, "progressive");

        if (fps_n != self->video_fps_n || fps_d != self->video_fps_d) {
          GstClockTime latency;

          latency = gst_util_uint64_scale (GST_SECOND, fps_d, fps_n);
          gst_aggregator_set_latency (aggregator, latency, latency);
        }

        self->video_fps_n = fps_n;
        self->video_fps_d = fps_d;

        self->cdp_fps_entry = cdp_fps_entry_from_fps (fps_n, fps_d);
        if (!self->cdp_fps_entry || self->cdp_fps_entry->fps_n == 0) {
          GST_WARNING_OBJECT (self, "Missing valid caption framerate in "
              "video caps");

          GST_ELEMENT_WARNING (self, CORE, NEGOTIATION, (NULL),
              ("Missing valid caption framerate in video caps"));

          self->cdp_fps_entry = cdp_fps_entry_from_fps (60, 1);
        }

        gst_aggregator_set_src_caps (aggregator, caps);
      }

      break;
    }
    case GST_EVENT_SEGMENT:{
      if (strcmp (GST_OBJECT_NAME (agg_pad), "sink") == 0) {
        const GstSegment *segment;

        gst_event_parse_segment (event, &segment);
        gst_aggregator_update_segment (aggregator, segment);
      }
      break;
    }
    default:
      break;
  }

  return GST_AGGREGATOR_CLASS (parent_class)->sink_event (aggregator, agg_pad,
      event);
}

static gboolean
gst_cc_combiner_stop (GstAggregator * aggregator)
{
  GstCCCombiner *self = GST_CCCOMBINER (aggregator);

  self->video_fps_n = self->video_fps_d = 0;
  self->current_video_running_time = self->current_video_running_time_end =
      self->previous_video_running_time_end = GST_CLOCK_TIME_NONE;
  gst_buffer_replace (&self->current_video_buffer, NULL);

  g_array_set_size (self->current_frame_captions, 0);
  self->caption_type = GST_VIDEO_CAPTION_TYPE_UNKNOWN;

  cc_buffer_discard (self->cc_buffer);
  self->current_scheduled = 0;
  self->cdp_fps_entry = &null_fps_entry;

  return TRUE;
}

static GstFlowReturn
gst_cc_combiner_flush (GstAggregator * aggregator)
{
  GstCCCombiner *self = GST_CCCOMBINER (aggregator);
  GstAggregatorPad *src_pad =
      GST_AGGREGATOR_PAD (GST_AGGREGATOR_SRC_PAD (aggregator));

  self->current_video_running_time = self->current_video_running_time_end =
      self->previous_video_running_time_end = GST_CLOCK_TIME_NONE;
  gst_buffer_replace (&self->current_video_buffer, NULL);

  g_array_set_size (self->current_frame_captions, 0);

  src_pad->segment.position = GST_CLOCK_TIME_NONE;

  self->cdp_hdr_sequence_cntr = 0;

  cc_buffer_discard (self->cc_buffer);
  self->current_scheduled = 0;

  return GST_FLOW_OK;
}

static GstAggregatorPad *
gst_cc_combiner_create_new_pad (GstAggregator * aggregator,
    GstPadTemplate * templ, const gchar * req_name, const GstCaps * caps)
{
  GstCCCombiner *self = GST_CCCOMBINER (aggregator);
  GstAggregatorPad *agg_pad;

  if (templ->direction != GST_PAD_SINK)
    return NULL;

  if (templ->presence != GST_PAD_REQUEST)
    return NULL;

  if (strcmp (templ->name_template, "caption") != 0)
    return NULL;

  GST_OBJECT_LOCK (self);
  agg_pad = g_object_new (GST_TYPE_AGGREGATOR_PAD,
      "name", "caption", "direction", GST_PAD_SINK, "template", templ, NULL);
  self->caption_type = GST_VIDEO_CAPTION_TYPE_UNKNOWN;
  GST_OBJECT_UNLOCK (self);

  return agg_pad;
}

static gboolean
gst_cc_combiner_src_query (GstAggregator * aggregator, GstQuery * query)
{
  GstPad *video_sinkpad =
      gst_element_get_static_pad (GST_ELEMENT_CAST (aggregator), "sink");
  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    case GST_QUERY_DURATION:
    case GST_QUERY_URI:
    case GST_QUERY_CAPS:
    case GST_QUERY_ALLOCATION:
      ret = gst_pad_peer_query (video_sinkpad, query);
      break;
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *caps;
      GstCaps *templ = gst_static_pad_template_get_caps (&srctemplate);

      gst_query_parse_accept_caps (query, &caps);
      gst_query_set_accept_caps_result (query, gst_caps_is_subset (caps,
              templ));
      gst_caps_unref (templ);
      ret = TRUE;
      break;
    }
    default:
      ret = GST_AGGREGATOR_CLASS (parent_class)->src_query (aggregator, query);
      break;
  }

  gst_object_unref (video_sinkpad);

  return ret;
}

static gboolean
gst_cc_combiner_sink_query (GstAggregator * aggregator,
    GstAggregatorPad * aggpad, GstQuery * query)
{
  GstPad *video_sinkpad =
      gst_element_get_static_pad (GST_ELEMENT_CAST (aggregator), "sink");
  GstPad *srcpad = GST_AGGREGATOR_SRC_PAD (aggregator);

  gboolean ret;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    case GST_QUERY_DURATION:
    case GST_QUERY_URI:
    case GST_QUERY_ALLOCATION:
      if (GST_PAD_CAST (aggpad) == video_sinkpad) {
        ret = gst_pad_peer_query (srcpad, query);
      } else {
        ret =
            GST_AGGREGATOR_CLASS (parent_class)->sink_query (aggregator,
            aggpad, query);
      }
      break;
    case GST_QUERY_CAPS:
      if (GST_PAD_CAST (aggpad) == video_sinkpad) {
        ret = gst_pad_peer_query (srcpad, query);
      } else {
        GstCaps *filter;
        GstCaps *templ = gst_static_pad_template_get_caps (&captiontemplate);

        gst_query_parse_caps (query, &filter);

        if (filter) {
          GstCaps *caps =
              gst_caps_intersect_full (filter, templ, GST_CAPS_INTERSECT_FIRST);
          gst_query_set_caps_result (query, caps);
          gst_caps_unref (caps);
        } else {
          gst_query_set_caps_result (query, templ);
        }
        gst_caps_unref (templ);
        ret = TRUE;
      }
      break;
    case GST_QUERY_ACCEPT_CAPS:
      if (GST_PAD_CAST (aggpad) == video_sinkpad) {
        ret = gst_pad_peer_query (srcpad, query);
      } else {
        GstCaps *caps;
        GstCaps *templ = gst_static_pad_template_get_caps (&captiontemplate);

        gst_query_parse_accept_caps (query, &caps);
        gst_query_set_accept_caps_result (query, gst_caps_is_subset (caps,
                templ));
        gst_caps_unref (templ);
        ret = TRUE;
      }
      break;
    default:
      ret = GST_AGGREGATOR_CLASS (parent_class)->sink_query (aggregator,
          aggpad, query);
      break;
  }

  gst_object_unref (video_sinkpad);

  return ret;
}

static GstSample *
gst_cc_combiner_peek_next_sample (GstAggregator * agg,
    GstAggregatorPad * aggpad)
{
  GstAggregatorPad *caption_pad, *video_pad;
  GstCCCombiner *self = GST_CCCOMBINER (agg);
  GstSample *res = NULL;

  caption_pad =
      GST_AGGREGATOR_PAD_CAST (gst_element_get_static_pad (GST_ELEMENT_CAST
          (self), "caption"));
  video_pad =
      GST_AGGREGATOR_PAD_CAST (gst_element_get_static_pad (GST_ELEMENT_CAST
          (self), "sink"));

  if (aggpad == caption_pad) {
    if (self->current_frame_captions->len > 0) {
      GstCaps *caps = gst_pad_get_current_caps (GST_PAD (aggpad));
      GstBufferList *buflist = gst_buffer_list_new ();
      guint i;

      for (i = 0; i < self->current_frame_captions->len; i++) {
        CaptionData *caption_data =
            &g_array_index (self->current_frame_captions, CaptionData, i);
        gst_buffer_list_add (buflist, gst_buffer_ref (caption_data->buffer));
      }

      res = gst_sample_new (NULL, caps, &aggpad->segment, NULL);
      gst_caps_unref (caps);

      gst_sample_set_buffer_list (res, buflist);
      gst_buffer_list_unref (buflist);
    }
  } else if (aggpad == video_pad) {
    if (self->current_video_buffer) {
      GstCaps *caps = gst_pad_get_current_caps (GST_PAD (aggpad));
      res = gst_sample_new (self->current_video_buffer,
          caps, &aggpad->segment, NULL);
      gst_caps_unref (caps);
    }
  }

  if (caption_pad)
    gst_object_unref (caption_pad);

  if (video_pad)
    gst_object_unref (video_pad);

  return res;
}

static GstStateChangeReturn
gst_cc_combiner_change_state (GstElement * element, GstStateChange transition)
{
  GstCCCombiner *self = GST_CCCOMBINER (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->schedule = self->prop_schedule;
      self->max_scheduled = self->prop_max_scheduled;
      self->output_padding = self->prop_output_padding;
      cc_buffer_set_max_buffer_time (self->cc_buffer, GST_CLOCK_TIME_NONE);
      cc_buffer_set_output_padding (self->cc_buffer, self->prop_output_padding,
          self->prop_output_padding);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_cc_combiner_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCCCombiner *self = GST_CCCOMBINER (object);

  switch (prop_id) {
    case PROP_SCHEDULE:
      self->prop_schedule = g_value_get_boolean (value);
      break;
    case PROP_MAX_SCHEDULED:
      self->prop_max_scheduled = g_value_get_uint (value);
      break;
    case PROP_OUTPUT_PADDING:
      self->prop_output_padding = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cc_combiner_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstCCCombiner *self = GST_CCCOMBINER (object);

  switch (prop_id) {
    case PROP_SCHEDULE:
      g_value_set_boolean (value, self->prop_schedule);
      break;
    case PROP_MAX_SCHEDULED:
      g_value_set_uint (value, self->prop_max_scheduled);
      break;
    case PROP_OUTPUT_PADDING:
      g_value_set_boolean (value, self->prop_output_padding);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cc_combiner_class_init (GstCCCombinerClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAggregatorClass *aggregator_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  aggregator_class = (GstAggregatorClass *) klass;

  gobject_class->finalize = gst_cc_combiner_finalize;
  gobject_class->set_property = gst_cc_combiner_set_property;
  gobject_class->get_property = gst_cc_combiner_get_property;

  gst_element_class_set_static_metadata (gstelement_class,
      "Closed Caption Combiner",
      "Filter",
      "Combines GstVideoCaptionMeta with video input stream",
      "Sebastian Dröge <sebastian@centricular.com>");

  /**
   * GstCCCombiner:schedule:
   *
   * Controls whether caption buffers should be smoothly scheduled
   * in order to have exactly one per output video buffer.
   *
   * This can involve rewriting input captions, for example when the
   * input is CDP sequence counters are rewritten, time codes are dropped
   * and potentially re-injected if the input video frame had a time code
   * meta.
   *
   * Caption buffers may also get split up in order to assign captions to
   * the correct field when the input is interlaced.
   *
   * This can also imply that the input will drift from synchronization,
   * when there isn't enough padding in the input stream to catch up. In
   * that case the element will start dropping old caption buffers once
   * the number of buffers in its internal queue reaches
   * #GstCCCombiner:max-scheduled.
   *
   * When this is set to %FALSE, the behaviour of this element is essentially
   * that of a funnel.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_SCHEDULE, g_param_spec_boolean ("schedule",
          "Schedule",
          "Schedule caption buffers so that exactly one is output per video frame",
          DEFAULT_SCHEDULE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstCCCombiner:max-scheduled:
   *
   * Controls the number of scheduled buffers after which the element
   * will start dropping old buffers from its internal queues. See
   * #GstCCCombiner:schedule.
   *
   * Since: 1.20
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_MAX_SCHEDULED, g_param_spec_uint ("max-scheduled",
          "Max Scheduled",
          "Maximum number of buffers to queue for scheduling", 0, G_MAXUINT,
          DEFAULT_MAX_SCHEDULED,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));

  /**
   * GstCCCombiner:output-padding:
   *
   * When #GstCCCombiner:schedule is %TRUE, this property controls
   * whether the output closed caption meta stream will be padded.
   *
   * Since: 1.22
   */
  g_object_class_install_property (G_OBJECT_CLASS (klass),
      PROP_OUTPUT_PADDING, g_param_spec_boolean ("output-padding",
          "Output padding",
          "Whether to output padding packets when schedule=true",
          DEFAULT_OUTPUT_PADDING,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_READY));


  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &sinktemplate, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &srctemplate, GST_TYPE_AGGREGATOR_PAD);
  gst_element_class_add_static_pad_template_with_gtype (gstelement_class,
      &captiontemplate, GST_TYPE_AGGREGATOR_PAD);

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_cc_combiner_change_state);

  aggregator_class->aggregate = gst_cc_combiner_aggregate;
  aggregator_class->stop = gst_cc_combiner_stop;
  aggregator_class->flush = gst_cc_combiner_flush;
  aggregator_class->create_new_pad = gst_cc_combiner_create_new_pad;
  aggregator_class->sink_event = gst_cc_combiner_sink_event;
  aggregator_class->negotiate = NULL;
  aggregator_class->get_next_time = gst_aggregator_simple_get_next_time;
  aggregator_class->src_query = gst_cc_combiner_src_query;
  aggregator_class->sink_query = gst_cc_combiner_sink_query;
  aggregator_class->peek_next_sample = gst_cc_combiner_peek_next_sample;

  GST_DEBUG_CATEGORY_INIT (gst_cc_combiner_debug, "cccombiner",
      0, "Closed Caption combiner");
}

static void
gst_cc_combiner_init (GstCCCombiner * self)
{
  GstPadTemplate *templ;
  GstAggregatorPad *agg_pad;

  templ = gst_static_pad_template_get (&sinktemplate);
  agg_pad = g_object_new (GST_TYPE_AGGREGATOR_PAD,
      "name", "sink", "direction", GST_PAD_SINK, "template", templ, NULL);
  gst_object_unref (templ);
  gst_element_add_pad (GST_ELEMENT_CAST (self), GST_PAD_CAST (agg_pad));

  self->current_frame_captions =
      g_array_new (FALSE, FALSE, sizeof (CaptionData));
  g_array_set_clear_func (self->current_frame_captions,
      (GDestroyNotify) caption_data_clear);

  self->current_video_running_time = self->current_video_running_time_end =
      self->previous_video_running_time_end = GST_CLOCK_TIME_NONE;

  self->caption_type = GST_VIDEO_CAPTION_TYPE_UNKNOWN;

  self->prop_schedule = DEFAULT_SCHEDULE;
  self->prop_max_scheduled = DEFAULT_MAX_SCHEDULED;
  self->prop_output_padding = DEFAULT_OUTPUT_PADDING;
  self->cdp_hdr_sequence_cntr = 0;
  self->cdp_fps_entry = &null_fps_entry;

  self->cc_buffer = cc_buffer_new ();
  cc_buffer_set_max_buffer_time (self->cc_buffer, GST_CLOCK_TIME_NONE);
}
