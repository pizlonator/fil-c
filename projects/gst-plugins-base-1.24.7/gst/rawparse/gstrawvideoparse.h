/* GStreamer
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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

#ifndef __GST_RAW_VIDEO_PARSE_H__
#define __GST_RAW_VIDEO_PARSE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstrawbaseparse.h"

G_BEGIN_DECLS

#define GST_TYPE_RAW_VIDEO_PARSE (gst_raw_video_parse_get_type())
#define GST_RAW_VIDEO_PARSE_CAST(obj) ((GstRawVideoParse *)(obj))
G_DECLARE_FINAL_TYPE (GstRawVideoParse, gst_raw_video_parse,
    GST, RAW_VIDEO_PARSE, GstRawBaseParse)

typedef struct _GstRawVideoParseConfig GstRawVideoParseConfig;

/* Contains information about the video frame format. */
struct _GstRawVideoParseConfig
{
  /* If TRUE, then this configuration is ready to use */
  gboolean ready;

  /* FIXME: These values should not be necessary, since there's
   * GstVideoInfo. However, setting these values in the video
   * info independently is currently difficult. For example,
   * setting the video format requires the gst_video_info_set_format()
   * function, but this function also overwrites plane strides
   * and offsets. */
  gint width, height;
  GstVideoFormat format;
  gint pixel_aspect_ratio_n, pixel_aspect_ratio_d;
  gint framerate_n, framerate_d;
  gboolean interlaced;
  gsize plane_offsets[GST_VIDEO_MAX_PLANES];
  gint plane_strides[GST_VIDEO_MAX_PLANES];
  GstVideoColorimetry colorimetry;

  /* If TRUE, then TFF flags are added to outgoing buffers and
   * their video metadata */
  gboolean top_field_first;

  /* Distance between the start of each frame, in bytes. If this value
   * is larger than the actual size of a frame, then the extra bytes
   * are skipped. For example, with frames that have 115200 bytes, a
   * frame_size value of 120000 means that 4800 trailing bytes are
   * skipped after the 115200 frame bytes. This is useful to skip
   * metadata in between frames. */
  guint frame_size;

  GstVideoInfo info;

  gboolean custom_plane_strides;
};

struct _GstRawVideoParse
{
  GstRawBaseParse parent;

  /*< private > */

  /* Configuration controlled by the object properties. Its ready value
   * is set to TRUE from the start, so it can be used right away.
   */
  GstRawVideoParseConfig properties_config;
  /* Configuration controlled by the sink caps. Its ready value is
   * initially set to FALSE until valid sink caps come in. It is set to
   * FALSE again when the stream-start event is observed.
   */
  GstRawVideoParseConfig sink_caps_config;
  /* Currently active configuration. Points either to properties_config
   * or to sink_caps_config. This is never NULL. */
  GstRawVideoParseConfig *current_config;
};

G_END_DECLS

#endif
