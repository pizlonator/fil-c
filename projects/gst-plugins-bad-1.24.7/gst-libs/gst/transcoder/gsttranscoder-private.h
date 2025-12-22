/* GStreamer
 *
 * Copyright (C) 2020 Stephan Hesse <stephan@emliri.com>
 * Copyright (C) 2020 Thibault Saunier <tsaunier@igalia.com>
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

#pragma once

#define GST_TRANSCODER_MESSAGE_DATA "gst-transcoder-message-data"
#define GST_TRANSCODER_MESSAGE_DATA_TYPE "transcoder-message-type"
#define GST_TRANSCODER_MESSAGE_DATA_POSITION "position"
#define GST_TRANSCODER_MESSAGE_DATA_DURATION "duration"
#define GST_TRANSCODER_MESSAGE_DATA_STATE "state"
#define GST_TRANSCODER_MESSAGE_DATA_ERROR "error"
#define GST_TRANSCODER_MESSAGE_DATA_WARNING "warning"
#define GST_TRANSCODER_MESSAGE_DATA_ISSUE_DETAILS "issue-details"

struct _GstTranscoderSignalAdapter
{
  GObject parent;
  GstBus *bus;
  GSource *source;

  GWeakRef transcoder;
};


GstTranscoderSignalAdapter * gst_transcoder_signal_adapter_new_sync_emit (GstTranscoder * transcoder);
GstTranscoderSignalAdapter * gst_transcoder_signal_adapter_new           (GstTranscoder * transcoder, GMainContext * context);
