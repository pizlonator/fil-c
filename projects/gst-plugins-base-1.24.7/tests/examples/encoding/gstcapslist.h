/* GStreamer
 * Copyright (C) <2010> Edward Hervey <edward.hervey@collabora.co.uk>
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

#include <gst/gst.h>

GstCaps *gst_caps_list_compatible_codecs (const GstCaps *containerformat,
					  GstCaps *codecformats,
					  GList *muxers);

GstCaps *gst_caps_list_compatible_containers (GstCaps *mediaformat,
					      GList *containerformats);


GstCaps *gst_caps_list_container_formats (GstRank minrank);

GstCaps *gst_caps_list_video_encoding_formats (GstRank minrank);

GstCaps *gst_caps_list_audio_encoding_formats (GstRank minrank);

