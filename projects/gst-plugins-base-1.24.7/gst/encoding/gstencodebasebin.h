/* GStreamer encoding bin
 * Copyright (C) 2009 Edward Hervey <edward.hervey@collabora.co.uk>
 *           (C) 2009 Nokia Corporation
 *           (C) 2016 Jan Schmidt <jan@centricular.com>
 *           (C) 2020 Thibault saunier <tsaunier@igalia.com>
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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#define GST_TYPE_ENCODE_BASE_BIN               (gst_encode_base_bin_get_type())
#define GST_ENCODE_BASE_BIN(obj)               (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ENCODE_BASE_BIN,GstEncodeBin))
#define GST_ENCODE_BASE_BIN_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ENCODE_BASE_BIN,GstEncodeBinClass))
#define GST_IS_ENCODE_BASE_BIN(obj)            (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ENCODE_BASE_BIN))
#define GST_IS_ENCODE_BASE_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ENCODE_BASE_BIN))
#define GST_ENCODE_BASE_BIN_GET_CLASS(klass)   (G_TYPE_INSTANCE_GET_CLASS ((klass), GST_TYPE_ENCODE_BASE_BIN, GstEncodeBaseBinClass))

typedef struct _GstEncodeBaseBin GstEncodeBaseBin;
typedef struct _GstEncodeBaseBinClass GstEncodeBaseBinClass;

typedef enum
{
  GST_ENCODEBIN_FLAG_NO_AUDIO_CONVERSION = (1 << 0),
  GST_ENCODEBIN_FLAG_NO_VIDEO_CONVERSION = (1 << 1)
} GstEncodeBaseBinFlags;

struct _GstEncodeBaseBin
{
  GstBin parent;

  /* the profile field is only valid if it could be entirely setup */
  GstEncodingProfile *profile;

  GList *streams;               /* List of StreamGroup, not sorted */

  GstElement *muxer;
  /* Ghostpad with changing target */
  GstPad *srcpad;

  /* TRUE if in PAUSED/PLAYING */
  gboolean active;

  /* available muxers, encoders and parsers */
  GList *muxers;
  GList *formatters;
  GList *encoders;
  GList *parsers;
  GList *timestampers;

  /* Increasing counter for unique pad name */
  guint last_pad_id;

  /* Cached caps for identification */
  GstCaps *raw_video_caps;
  GstCaps *raw_audio_caps;
  /* GstCaps *raw_text_caps; */

  guint queue_buffers_max;
  guint queue_bytes_max;
  guint64 queue_time_max;

  guint64 tolerance;
  gboolean avoid_reencoding;

  GstEncodeBaseBinFlags flags;
};

struct _GstEncodeBaseBinClass
{
  GstBinClass parent;

  /* Action Signals */
  GstPad *(*request_pad) (GstEncodeBaseBin * encodebin, GstCaps * caps);
  GstPad *(*request_profile_pad) (GstEncodeBaseBin * encodebin,
      const gchar * profilename);
};

GType gst_encode_base_bin_get_type(void);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GstEncodeBaseBin, gst_object_unref)