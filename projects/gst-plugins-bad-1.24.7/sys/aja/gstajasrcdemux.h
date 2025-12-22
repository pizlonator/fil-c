/* GStreamer
 * Copyright (C) 2021 Sebastian Dröge <sebastian@centricular.com>
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

#include <gst/base/base.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstajacommon.h"

G_BEGIN_DECLS

#define GST_TYPE_AJA_SRC_DEMUX (gst_aja_src_demux_get_type())
#define GST_AJA_SRC_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_SRC_DEMUX, GstAjaSrcDemux))
#define GST_AJA_SRC_DEMUX_CAST(obj) ((GstAjaSrcDemux *)obj)
#define GST_AJA_SRC_DEMUX_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AJA_SRC_DEMUX, \
                           GstAjaSrcDemuxClass))
#define GST_IS_AJA_SRC_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AJA_SRC_DEMUX))
#define GST_IS_AJA_SRC_DEMUX_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AJA_SRC_DEMUX))

typedef struct _GstAjaSrcDemux GstAjaSrcDemux;
typedef struct _GstAjaSrcDemuxClass GstAjaSrcDemuxClass;

struct _GstAjaSrcDemux {
  GstElement parent;

  GstPad *sink;
  GstPad *video_src, *audio_src;
};

struct _GstAjaSrcDemuxClass {
  GstElementClass parent_class;
};

G_GNUC_INTERNAL
GType gst_aja_src_demux_get_type(void);

G_END_DECLS
