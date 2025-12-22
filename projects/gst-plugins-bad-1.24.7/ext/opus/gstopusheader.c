/* GStreamer Opus Encoder
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2008> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2011> Vincent Penquerc'h <vincent.penquerch@collabora.co.uk>
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
#include "config.h"
#endif
#include <gst/tag/tag.h>
#include <gst/base/gstbytewriter.h>
#include "gstopusheader.h"

gboolean
gst_opus_header_is_header (GstBuffer * buf, const char *magic, guint magic_size)
{
  return (gst_buffer_get_size (buf) >= magic_size
      && !gst_buffer_memcmp (buf, 0, magic, magic_size));
}

gboolean
gst_opus_header_is_id_header (GstBuffer * buf)
{
  gsize size = gst_buffer_get_size (buf);
  guint8 *data = NULL;
  guint8 version, channels, channel_mapping_family, n_streams, n_stereo_streams;
  gboolean ret = FALSE;
  GstMapInfo map;

  if (size < 19)
    goto beach;
  if (!gst_opus_header_is_header (buf, "OpusHead", 8))
    goto beach;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  data = map.data;
  size = map.size;

  version = data[8];
  if (version >= 0x0f)          /* major version >=0 is what we grok */
    goto beach;

  channels = data[9];

  if (channels == 0)
    goto beach;

  channel_mapping_family = data[18];

  if (channel_mapping_family == 0) {
    if (channels > 2)
      goto beach;
  } else {
    channels = data[9];
    if (size < 21 + channels)
      goto beach;
    n_streams = data[19];
    n_stereo_streams = data[20];
    if (n_streams == 0)
      goto beach;
    if (n_stereo_streams > n_streams)
      goto beach;
    if (n_streams + n_stereo_streams > 255)
      goto beach;
  }
  ret = TRUE;

beach:
  if (data)
    gst_buffer_unmap (buf, &map);
  return ret;
}

gboolean
gst_opus_header_is_comment_header (GstBuffer * buf)
{
  return gst_opus_header_is_header (buf, "OpusTags", 8);
}
