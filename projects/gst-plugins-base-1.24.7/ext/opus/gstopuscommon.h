/* GStreamer Opus Encoder
 * Copyright (C) 2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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


#ifndef __GST_OPUS_COMMON_H__
#define __GST_OPUS_COMMON_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

G_BEGIN_DECLS

extern const GstAudioChannelPosition gst_opus_channel_positions[][8];
extern const char *gst_opus_channel_names[];
extern void gst_opus_common_log_channel_mapping_table (GstElement *element,
    GstDebugCategory * category, const char *msg,
    int n_channels, const guint8 *table);

G_END_DECLS

#endif /* __GST_OPUS_COMMON_H__ */
