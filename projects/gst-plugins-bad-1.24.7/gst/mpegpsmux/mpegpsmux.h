/* MPEG-PS muxer plugin for GStreamer
 * Copyright 2008 Lin YANG <oxcsnicho@gmail.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#ifndef __MPEGPSMUX_H__
#define __MPEGPSMUX_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS

#include "psmux.h"

#define GST_TYPE_MPEG_PSMUX  (mpegpsmux_get_type())
#define GST_MPEG_PSMUX(obj)  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MPEG_PSMUX, MpegPsMux))

typedef struct MpegPsMux MpegPsMux;
typedef struct MpegPsMuxClass MpegPsMuxClass;
typedef struct MpegPsPadData MpegPsPadData;

typedef GstBuffer * (*MpegPsPadDataPrepareFunction) (GstBuffer * buf,
    MpegPsPadData * data, MpegPsMux * mux);

struct MpegPsMux {
  GstElement parent;

  GstPad *srcpad;

  guint video_stream_id;   /* stream id of primary video stream */

  GstCollectPads *collect; /* pads collector */

  PsMux *psmux;

  gboolean first;
  GstFlowReturn last_flow_ret;
  
  GstClockTime last_ts;

  GstBufferList *gop_list;
  gboolean       aggregate_gops;
};

struct MpegPsMuxClass  {
  GstElementClass parent_class;
};

struct MpegPsPadData {
  GstCollectData collect; /* Parent */

  guint8 stream_id;
  guint8 stream_id_ext; 
  PsMuxStream *stream;

  /* Currently pulled buffer */
  struct {
    GstBuffer *buf;
    GstClockTime ts;  /* adjusted TS = MIN (DTS, PTS) for the pulled buffer */
    GstClockTime pts; /* adjusted PTS (running time) */
    GstClockTime dts; /* adjusted DTS (running time) */
  } queued;

  /* Most recent valid TS (DTS or PTS) for this stream */
  GstClockTime last_ts;

  GstBuffer * codec_data; /* Optional codec data available in the caps */

  MpegPsPadDataPrepareFunction prepare_func; /* Handler to prepare input data */

  gboolean eos;
};

GType mpegpsmux_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (mpegpsmux);

#define CLOCK_BASE 9LL
#define CLOCK_FREQ (CLOCK_BASE * 10000)

#define GSTTIME_TO_MPEGTIME(time) \
    (GST_CLOCK_TIME_IS_VALID(time) ? \
        gst_util_uint64_scale ((time), CLOCK_BASE, GST_MSECOND/10) : \
            -1)

#define NORMAL_TS_PACKET_LENGTH 188
#define M2TS_PACKET_LENGTH      192
#define STANDARD_TIME_CLOCK     27000000
/*33 bits as 1 ie 0x1ffffffff*/
#define TWO_POW_33_MINUS1     ((0xffffffff * 2) - 1) 
G_END_DECLS

#endif
