/*
 * GStreamer
 * Copyright (C) 2011 Robert Jobbagy <jobbagy.robert@gmail.com>
 * Copyright (C) 2011 - 2018 Nicola Murino <nicola.murino@gmail.com>
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

#ifndef __GST_MOTIONCELLS_H__
#define __GST_MOTIONCELLS_H__

#include <gst/opencv/gstopencvvideofilter.h>
#include "motioncells_wrapper.h"

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define GST_TYPE_MOTIONCELLS \
  (gst_motion_cells_get_type())
#define gst_motion_cells(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MOTIONCELLS,GstMotioncells))
#define gst_motion_cells_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MOTIONCELLS,GstMotioncellsClass))
#define GST_IS_MOTIONCELLS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MOTIONCELLS))
#define GST_IS_MOTIONCELLS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MOTIONCELLS))
typedef struct _GstMotioncells GstMotioncells;
typedef struct _GstMotioncellsClass GstMotioncellsClass;

struct _GstMotioncells
{
  GstOpencvVideoFilter element;
  GstState state;
  gboolean display, calculate_motion, firstgridx, firstgridy, changed_gridx,
      changed_gridy, changed_startime;
  gboolean previous_motion, changed_datafile, postallmotion, usealpha,
      firstdatafile, firstframe;
  gboolean sent_init_error_msg, sent_save_error_msg;
  gchar *prev_datafile, *cur_datafile, *basename_datafile, *datafile_extension;
  gint prevgridx, gridx, prevgridy, gridy, id;
  gdouble sensitivity, threshold;
  motionmaskcoordrect *motionmaskcoords;
  cellscolor *motioncellscolor;
  motioncellidx *motioncellsidx, *motionmaskcellsidx;
  int motionmaskcoord_count, motioncells_count, motionmaskcells_count;
  int thickness;
  guint gap, datafileidx, postnomotion, minimum_motion_frames;
  guint64 motion_begin_timestamp, last_motion_timestamp, motion_timestamp,
      last_nomotion_notified, prev_buff_timestamp, cur_buff_timestamp;
  gint64 diff_timestamp, starttime;
  guint64 consecutive_motion;
  gint width, height;
  double framerate;
  //Video width and height are known in "gst_motion_cells_handle_sink_event",
  // but not when setting the "motionmaskcoords".
  gchar has_delayed_mask;
};

struct _GstMotioncellsClass
{
  GstOpencvVideoFilterClass parent_class;
};

GType gst_motion_cells_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (motioncells);

G_END_DECLS
#endif /* __GST_MOTION_CELLS_H__ */
