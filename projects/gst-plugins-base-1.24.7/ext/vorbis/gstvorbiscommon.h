/* GStreamer
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

#ifndef __GST_VORBIS_COMMON_H__
#define __GST_VORBIS_COMMON_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

extern const GstAudioChannelPosition gst_vorbis_channel_positions[][8];
extern const GstAudioChannelPosition gst_vorbis_default_channel_positions[][8];

#ifndef USE_TREMOLO
extern const gint gst_vorbis_reorder_map[][8];
#endif

#endif /* __GST_VORBIS_COMMON_H__ */
