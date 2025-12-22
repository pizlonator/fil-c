/* GStreamer
 * Copyright (C)  2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowvideosrc.h:
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

#ifndef __GST_DSHOWVIDEOSRC_H__
#define __GST_DSHOWVIDEOSRC_H__

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include "gstdshow.h"
#include "gstdshowfakesink.h"

// 30323449-0000-0010-8000-00AA00389B71            MEDIASUBTYPE_I420
DEFINE_GUID (MEDIASUBTYPE_I420, 0x30323449, 0x0000, 0x0010, 0x80, 0x00, 0x00,
	0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID (MEDIASUBTYPE_UYVY, 0x59565955, 0x0000, 0x0010, 0x80, 0x00, 0x00,
    0xAA, 0x00, 0x38, 0x9B, 0x71);

G_BEGIN_DECLS
#define GST_TYPE_DSHOWVIDEOSRC              (gst_dshowvideosrc_get_type())
#define GST_DSHOWVIDEOSRC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DSHOWVIDEOSRC,GstDshowVideoSrc))
#define GST_DSHOWVIDEOSRC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DSHOWVIDEOSRC,GstDshowVideoSrcClass))
#define GST_IS_DSHOWVIDEOSRC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DSHOWVIDEOSRC))
#define GST_IS_DSHOWVIDEOSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DSHOWVIDEOSRC))
typedef struct _GstDshowVideoSrc GstDshowVideoSrc;
typedef struct _GstDshowVideoSrcClass GstDshowVideoSrcClass;


struct _GstDshowVideoSrc
{
  GstPushSrc src;

  /* device dshow reference (generally classid/name) */
  gchar *device;

  /* device friendly name */
  gchar *device_name;

  /* device index */
  gint device_index;

  /* list of caps created from the list of supported media types of the dshow capture filter */
  GstCaps *caps;

  /* list of dshow media types from the filter's capture pins */
  GList *pins_mediatypes;

  /* dshow video capture filter */
  IBaseFilter *video_cap_filter;

  /* dshow sink filter */
  CDshowFakeSink *dshow_fakesink;

  /* graph manager interfaces */
  IMediaFilter *media_filter;
  IFilterGraph *filter_graph;

  IGraphBuilder	*graph_builder;
  ICaptureGraphBuilder2 *capture_builder;
  IAMVideoCompression *pVC;
  //IAMVfwCaptureDialogs *pDlg;
  //IAMStreamConfig *pASC;      // for audio cap
  IAMStreamConfig *pVSC;      // for video cap

  /* the last buffer from DirectShow */
  GCond buffer_cond;
  GMutex buffer_mutex;
  GstBuffer *buffer;
  gboolean stop_requested;

  gboolean is_rgb;
  gboolean is_running;
  gint width;
  gint height;
};

struct _GstDshowVideoSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_dshowvideosrc_get_type (void);


GstCaps * gst_dshowvideosrc_getcaps_from_capture_filter (IBaseFilter * filter,
    GList ** pins_mediatypes);


G_END_DECLS
#endif /* __GST_DSHOWVIDEOSRC_H__ */
