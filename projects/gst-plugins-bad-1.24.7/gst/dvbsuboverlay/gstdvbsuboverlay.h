/* GStreamer DVB subtitles overlay
 * Copyright (c) 2010 Mart Raudsepp <mart.raudsepp@collabora.co.uk>
 * Copyright (c) 2010 ONELAN Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __GST_DVBSUB_OVERLAY_H__
#define __GST_DVBSUB_OVERLAY_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/video-overlay-composition.h>

#include "dvb-sub.h"

G_BEGIN_DECLS

#define GST_TYPE_DVBSUB_OVERLAY (gst_dvbsub_overlay_get_type())
#define GST_DVBSUB_OVERLAY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DVBSUB_OVERLAY,GstDVBSubOverlay))
#define GST_DVBSUB_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DVBSUB_OVERLAY,GstDVBSubOverlayClass))
#define GST_IS_DVBSUB_OVERLAY(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DVBSUB_OVERLAY))
#define GST_IS_DVBSUB_OVERLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DVBSUB_OVERLAY))

typedef struct _GstDVBSubOverlay GstDVBSubOverlay;
typedef struct _GstDVBSubOverlayClass GstDVBSubOverlayClass;

struct _GstDVBSubOverlay
{
  GstElement element;

  GstPad *video_sinkpad, *text_sinkpad, *srcpad;

  /* properties */
  gboolean enable;
  gint max_page_timeout;
  gboolean force_end;

  /* <private> */
  GstSegment video_segment;
  GstSegment subtitle_segment;

  GstVideoInfo info;

  DVBSubtitles *current_subtitle; /* The currently active set of subtitle regions, if any */
  GstVideoOverlayComposition *current_comp;
  GQueue *pending_subtitles; /* A queue of raw subtitle region sets with
			      * metadata that are waiting their running time */

  GMutex dvbsub_mutex; /* protects the queue and the DvbSub instance */
  DvbSub *dvb_sub;

  /* subtitle data submitted to dvb_sub but no sub received yet */
  gboolean pending_sub;
  /* last text pts */
  GstClockTime last_text_pts;

  gboolean attach_compo_to_buffer;
};

struct _GstDVBSubOverlayClass
{
  GstElementClass parent_class;
};

GType gst_dvbsub_overlay_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (dvbsuboverlay);

G_END_DECLS

#endif
