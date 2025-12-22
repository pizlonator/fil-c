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

#define GST_TYPE_AJA_SINK_COMBINER (gst_aja_sink_combiner_get_type())
#define GST_AJA_SINK_COMBINER(obj)                               \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AJA_SINK_COMBINER, \
                              GstAjaSinkCombiner))
#define GST_AJA_SINK_COMBINER_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AJA_SINK_COMBINER, \
                           GstAjaSinkCombinerClass))
#define IS_GST_AJA_SINK_COMBINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AJA_SINK_COMBINER))
#define IS_GST_AJA_SINK_COMBINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AJA_SINK_COMBINER))

typedef struct _GstAjaSinkCombiner GstAjaSinkCombiner;
typedef struct _GstAjaSinkCombinerClass GstAjaSinkCombinerClass;

struct _GstAjaSinkCombiner {
  GstAggregator parent;

  GstPad *audio_sinkpad, *video_sinkpad;
  GstCaps *audio_caps, *video_caps;
  gboolean caps_changed;
};

struct _GstAjaSinkCombinerClass {
  GstAggregatorClass parent_class;
};

G_GNUC_INTERNAL
GType gst_aja_sink_combiner_get_type(void);

G_END_DECLS
