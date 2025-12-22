/*
 * GStreamer
 * Copyright (C) 2018 Edward Hervey <edward@centricular.com>
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

#ifndef __GST_CCEXTRACTOR_H__
#define __GST_CCEXTRACTOR_H__

#include <gst/gst.h>
#include <gst/base/gstflowcombiner.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_CCEXTRACTOR \
  (gst_cc_extractor_get_type())
#define GST_CCEXTRACTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CCEXTRACTOR,GstCCExtractor))
#define GST_CCEXTRACTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CCEXTRACTOR,GstCCExtractorClass))
#define GST_IS_CCEXTRACTOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CCEXTRACTOR))
#define GST_IS_CCEXTRACTOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CCEXTRACTOR))

typedef struct _GstCCExtractor GstCCExtractor;
typedef struct _GstCCExtractorClass GstCCExtractorClass;

struct _GstCCExtractor
{
  GstElement element;

  GstPad *sinkpad, *srcpad, *captionpad;
  GstVideoCaptionType caption_type;

  GstVideoInfo video_info;

  GstFlowCombiner *combiner;

  gboolean remove_caption_meta;
};

struct _GstCCExtractorClass
{
  GstElementClass parent_class;
};

GType gst_cc_extractor_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (ccextractor);

G_END_DECLS
#endif /* __GST_CCEXTRACTOR_H__ */
