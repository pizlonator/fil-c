/* GStreamer
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

#include "gstapputils.h"

void
gst_queue_status_info_reset (GstQueueStatusInfo * info)
{
  g_return_if_fail (info != NULL);

  info->queued_bytes = 0;
  info->queued_buffers = 0;
  info->queued_time = 0;
  info->num_events = 0;
  info->last_in_running_time = GST_CLOCK_TIME_NONE;
  info->last_out_running_time = GST_CLOCK_TIME_NONE;
}

gboolean
gst_queue_status_info_is_full (const GstQueueStatusInfo * info,
    guint64 max_buffers, guint64 max_bytes, GstClockTime max_time)
{
  g_return_val_if_fail (info != NULL, FALSE);

  return (max_buffers > 0 && info->queued_buffers >= max_buffers)
      || (max_bytes > 0 && info->queued_bytes >= max_bytes)
      || (max_time > 0 && info->queued_time >= max_time);
}

void
gst_queue_status_info_push_event (GstQueueStatusInfo * info)
{
  g_return_if_fail (info != NULL);

  info->num_events++;
}

/* Update the currently queued bytes/buffers/time information for the item
 * that was just added to the queue.
 */
void
gst_queue_status_info_push (GstQueueStatusInfo * info, GstMiniObject * item,
    GstSegment * last_segment, GstObject * log_context)
{
  GstClockTime start_buffer_ts = GST_CLOCK_TIME_NONE;
  GstClockTime end_buffer_ts = GST_CLOCK_TIME_NONE;
  guint buf_size = 0;
  guint n_buffers = 0;

  g_return_if_fail (info != NULL);

  if (GST_IS_EVENT (item)) {
    info->num_events++;
    return;
  }

  if (GST_IS_BUFFER (item)) {
    GstBuffer *buf = GST_BUFFER_CAST (item);

    buf_size = gst_buffer_get_size (buf);
    n_buffers = 1;

    start_buffer_ts = end_buffer_ts = GST_BUFFER_DTS_OR_PTS (buf);
    if (end_buffer_ts != GST_CLOCK_TIME_NONE
        && GST_BUFFER_DURATION_IS_VALID (buf))
      end_buffer_ts += GST_BUFFER_DURATION (buf);
  } else if (GST_IS_BUFFER_LIST (item)) {
    GstBufferList *buffer_list = GST_BUFFER_LIST_CAST (item);
    guint i;

    n_buffers = gst_buffer_list_length (buffer_list);

    for (i = 0; i < n_buffers; i++) {
      GstBuffer *tmp = gst_buffer_list_get (buffer_list, i);
      GstClockTime ts = GST_BUFFER_DTS_OR_PTS (tmp);

      buf_size += gst_buffer_get_size (tmp);

      if (ts != GST_CLOCK_TIME_NONE) {
        if (start_buffer_ts == GST_CLOCK_TIME_NONE)
          start_buffer_ts = ts;
        end_buffer_ts = ts;
        if (GST_BUFFER_DURATION_IS_VALID (tmp))
          end_buffer_ts += GST_BUFFER_DURATION (tmp);
      }
    }
  }

  info->queued_bytes += buf_size;
  info->queued_buffers += n_buffers;

  /* Update time level if working on a TIME segment */
  if (last_segment->format == GST_FORMAT_TIME
      && end_buffer_ts != GST_CLOCK_TIME_NONE) {
    /* Clip to the last segment boundaries */
    if (last_segment->stop != -1 && end_buffer_ts > last_segment->stop)
      end_buffer_ts = last_segment->stop;
    else if (last_segment->start > end_buffer_ts)
      end_buffer_ts = last_segment->start;

    info->last_in_running_time =
        gst_segment_to_running_time (last_segment, GST_FORMAT_TIME,
        end_buffer_ts);

    /* If this is the only buffer then we can directly update the queued time
     * here. This is especially useful if this was the first buffer because
     * otherwise we would have to wait until it is actually unqueued to know
     * the queued duration */
    if (info->queued_buffers == 1) {
      if (last_segment->stop != -1 && start_buffer_ts > last_segment->stop)
        start_buffer_ts = last_segment->stop;
      else if (last_segment->start > start_buffer_ts)
        start_buffer_ts = last_segment->start;

      info->last_out_running_time =
          gst_segment_to_running_time (last_segment, GST_FORMAT_TIME,
          start_buffer_ts);
    }

    GST_TRACE_OBJECT (log_context,
        "Last in running time %" GST_TIME_FORMAT ", last out running time %"
        GST_TIME_FORMAT, GST_TIME_ARGS (info->last_in_running_time),
        GST_TIME_ARGS (info->last_out_running_time));

    if (info->last_out_running_time != GST_CLOCK_TIME_NONE
        && info->last_in_running_time != GST_CLOCK_TIME_NONE) {
      if (info->last_out_running_time > info->last_in_running_time) {
        info->queued_time = 0;
      } else {
        info->queued_time =
            info->last_in_running_time - info->last_out_running_time;
      }
    }
  }

  GST_DEBUG_OBJECT (log_context,
      "Currently queued: %" G_GUINT64_FORMAT " bytes, %" G_GUINT64_FORMAT
      " buffers, %" GST_TIME_FORMAT, info->queued_bytes, info->queued_buffers,
      GST_TIME_ARGS (info->queued_time));
}

void
gst_queue_status_info_pop (GstQueueStatusInfo * info, GstMiniObject * item,
    GstSegment * current_segment, GstSegment * last_segment,
    GstObject * log_context)
{
  guint buf_size = 0;
  guint n_buffers = 0;
  GstClockTime end_buffer_ts = GST_CLOCK_TIME_NONE;

  g_return_if_fail (info != NULL);

  if (GST_IS_EVENT (item)) {
    info->num_events--;
    return;
  }

  if (GST_IS_BUFFER (item)) {
    GstBuffer *buf = GST_BUFFER_CAST (item);
    buf_size = gst_buffer_get_size (buf);
    n_buffers = 1;

    end_buffer_ts = GST_BUFFER_DTS_OR_PTS (buf);
    if (end_buffer_ts != GST_CLOCK_TIME_NONE
        && GST_BUFFER_DURATION_IS_VALID (buf))
      end_buffer_ts += GST_BUFFER_DURATION (buf);

    GST_LOG_OBJECT (log_context, "have buffer %p of size %u", buf, buf_size);
  } else if (GST_IS_BUFFER_LIST (item)) {
    GstBufferList *buffer_list = GST_BUFFER_LIST_CAST (item);
    guint i;

    n_buffers = gst_buffer_list_length (buffer_list);

    for (i = 0; i < n_buffers; i++) {
      GstBuffer *tmp = gst_buffer_list_get (buffer_list, i);
      GstClockTime ts = GST_BUFFER_DTS_OR_PTS (tmp);

      buf_size += gst_buffer_get_size (tmp);
      /* Update to the last buffer's timestamp that is known */
      if (ts != GST_CLOCK_TIME_NONE) {
        end_buffer_ts = ts;
        if (GST_BUFFER_DURATION_IS_VALID (tmp))
          end_buffer_ts += GST_BUFFER_DURATION (tmp);
      }
    }
  }

  info->queued_bytes -= buf_size;
  info->queued_buffers -= n_buffers;

  /* Update time level if working on a TIME segment */
  if ((current_segment->format == GST_FORMAT_TIME
          || (current_segment->format == GST_FORMAT_UNDEFINED
              && last_segment->format == GST_FORMAT_TIME))
      && end_buffer_ts != GST_CLOCK_TIME_NONE) {
    const GstSegment *segment =
        current_segment->format ==
        GST_FORMAT_TIME ? current_segment : last_segment;

    /* Clip to the current segment boundaries */
    if (segment->stop != -1 && end_buffer_ts > segment->stop)
      end_buffer_ts = segment->stop;
    else if (segment->start > end_buffer_ts)
      end_buffer_ts = segment->start;

    info->last_out_running_time =
        gst_segment_to_running_time (segment, GST_FORMAT_TIME, end_buffer_ts);

    GST_TRACE_OBJECT (log_context,
        "Last in running time %" GST_TIME_FORMAT ", last out running time %"
        GST_TIME_FORMAT, GST_TIME_ARGS (info->last_in_running_time),
        GST_TIME_ARGS (info->last_out_running_time));

    /* If timestamps on both sides are known, calculate the current
     * fill level in time and consider the queue empty if the output
     * running time is lower than the input one (i.e. some kind of reset
     * has happened).
     */
    if (info->last_out_running_time != GST_CLOCK_TIME_NONE
        && info->last_in_running_time != GST_CLOCK_TIME_NONE) {
      if (info->last_out_running_time > info->last_in_running_time) {
        info->queued_time = 0;
      } else {
        info->queued_time =
            info->last_in_running_time - info->last_out_running_time;
      }
    }
  }

  GST_DEBUG_OBJECT (log_context,
      "Currently queued: %" G_GUINT64_FORMAT " bytes, %" G_GUINT64_FORMAT
      " buffers, %" GST_TIME_FORMAT, info->queued_bytes,
      info->queued_buffers, GST_TIME_ARGS (info->queued_time));
}
