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
#ifndef _GST_APP_UTILS_H_
#define _GST_APP_UTILS_H_

#include <gst/gst.h>

typedef struct _GstQueueStatusInfo
{
  guint64 queued_bytes, queued_buffers;
  /* Used to calculate the current time level */
  GstClockTime last_in_running_time, last_out_running_time;
  /* Updated based on the above whenever they change */
  GstClockTime queued_time;
  guint num_events;
} GstQueueStatusInfo;

void gst_queue_status_info_reset (GstQueueStatusInfo * info);

gboolean gst_queue_status_info_is_full (const GstQueueStatusInfo * info,
    guint64 max_buffers, guint64 max_bytes, GstClockTime max_time);

void gst_queue_status_info_push (GstQueueStatusInfo * info,
    GstMiniObject * item, GstSegment * last_segment, GstObject * log_context);

void gst_queue_status_info_push_event (GstQueueStatusInfo * info);

void gst_queue_status_info_pop (GstQueueStatusInfo * info, GstMiniObject * item,
    GstSegment * current_segment, GstSegment * last_segment,
    GstObject * log_context);

#endif
