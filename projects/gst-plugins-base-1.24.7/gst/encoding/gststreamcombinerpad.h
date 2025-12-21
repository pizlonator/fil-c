/* Streamcombiner special-purpose pad
 * Copyright (C) 2013 MathieuDuponchelle <mduponchelle1@gmail.com>
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

#ifndef __GST_STREAM_COMBINER_PAD_H__
#define __GST_STREAM_COMBINER_PAD_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include <gst/base/gstcollectpads.h>

G_BEGIN_DECLS

#define GST_TYPE_STREAM_COMBINER_PAD (gst_stream_combiner_pad_get_type())
G_DECLARE_FINAL_TYPE (GstStreamCombinerPad, gst_stream_combiner_pad,
    GST, STREAM_COMBINER_PAD, GstPad)

/**
 * GstStream_CombinerPad:
 *
 * The opaque #GstStreamCombinerPad structure.
 */
struct _GstStreamCombinerPad
{
  GstPad parent;

  gboolean is_eos;
};

G_END_DECLS
#endif /* __GST_STREAM_COMBINER_PAD_H__ */
