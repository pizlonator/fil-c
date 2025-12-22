/*
 * GStreamer
 * Copyright (C) 2010 Robert Swain <robert.swain@collabora.co.uk>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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

#ifndef __GST_FIELDANALYSIS_H__
#define __GST_FIELDANALYSIS_H__

#include <gst/gst.h>

G_BEGIN_DECLS
#define GST_TYPE_FIELDANALYSIS \
  (gst_field_analysis_get_type())
#define GST_FIELDANALYSIS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FIELDANALYSIS,GstFieldAnalysis))
#define GST_FIELDANALYSIS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FIELDANALYSIS,GstFieldAnalysisClass))
#define GST_IS_FIELDANALYSIS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FIELDANALYSIS))
#define GST_IS_FIELDANALYSIS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FIELDANALYSIS))

typedef struct _GstFieldAnalysis GstFieldAnalysis;
typedef struct _GstFieldAnalysisClass GstFieldAnalysisClass;
typedef struct _FieldAnalysisFields FieldAnalysisFields;
typedef struct _FieldAnalysisHistory FieldAnalysisHistory;
typedef struct _FieldAnalysis FieldAnalysis;

typedef enum
{
  FIELD_ANALYSIS_PROGRESSIVE,
  FIELD_ANALYSIS_INTERLACED,
  FIELD_ANALYSIS_TELECINE_PROGRESSIVE,
  FIELD_ANALYSIS_TELECINE_MIXED
} FieldAnalysisConclusion;

enum FieldParity
{
  TOP_FIELD,
  BOTTOM_FIELD,
  BOTH_FIELDS
};

struct _FieldAnalysis
{
  /* frame, top, bottom, top with prev bottom, bottom with prev top */
  gfloat f, t, b, t_b, b_t;
  FieldAnalysisConclusion conclusion;
  /* -1 - unknown; 0 - holding none; 1 - top field; 2 - bottom field; 3 - both */
  gint holding;
  gboolean drop;
};

struct _FieldAnalysisFields
{
  GstVideoFrame frame;
  gboolean parity;
};

struct _FieldAnalysisHistory
{
  GstVideoFrame frame;
  FieldAnalysis results;
};

typedef enum
{
  METHOD_32DETECT,
  METHOD_IS_COMBED,
  METHOD_5_TAP
} FieldAnalysisCombMethod;

struct _GstFieldAnalysis
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  guint nframes;
  FieldAnalysisHistory frames[2];
  GstVideoInfo vinfo;
  gfloat (*same_field) (GstFieldAnalysis *, FieldAnalysisFields (*)[2]);
  gfloat (*same_frame) (GstFieldAnalysis *, FieldAnalysisFields (*)[2]);
  guint64 (*block_score_for_row) (GstFieldAnalysis *, FieldAnalysisFields (*)[2], guint8 *, guint8 *);
  gboolean is_telecine;
  gboolean first_buffer; /* indicates the first buffer for which a buffer will be output
                          * after a discont or flushing seek */
  guint8 *comb_mask;
  guint *block_scores;
  gboolean flushing;     /* indicates whether we are flushing or not */

  /* properties */
  guint32 noise_floor; /* threshold for the result of a metric to be valid */
  gfloat field_thresh; /* threshold used for the same parity field metric */
  gfloat frame_thresh; /* threshold used for the opposite parity field metric */
  gint64 spatial_thresh; /* threshold used spatial comb detection */
  guint64 block_width, block_height; /* width/height of window used for comb clusted detection */
  guint64 block_thresh;
  guint64 ignored_lines;
};

struct _GstFieldAnalysisClass
{
  GstElementClass parent_class;
};

GType gst_field_analysis_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (fieldanalysis);

G_END_DECLS
#endif /* __GST_FIELDANALYSIS_H__ */
