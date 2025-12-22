/* GStreamer
 * Copyright (C)  2007 Sebastien Moutte <sebastien@moutte.net>
 *
 * gstdshowaudiosrc.h:
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


#ifndef __GST_DSHOWAUDIOSRC_H__
#define __GST_DSHOWAUDIOSRC_H__

#include <gst/gst.h>
#include <gst/audio/gstaudiosrc.h>

#include "gstdshow.h"
#include "gstdshowfakesink.h"

G_BEGIN_DECLS
#define GST_TYPE_DSHOWAUDIOSRC              (gst_dshowaudiosrc_get_type())
#define GST_DSHOWAUDIOSRC(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DSHOWAUDIOSRC,GstDshowAudioSrc))
#define GST_DSHOWAUDIOSRC_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DSHOWAUDIOSRC,GstDshowAudioSrcClass))
#define GST_IS_DSHOWAUDIOSRC(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DSHOWAUDIOSRC))
#define GST_IS_DSHOWAUDIOSRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DSHOWAUDIOSRC))
typedef struct _GstDshowAudioSrc GstDshowAudioSrc;
typedef struct _GstDshowAudioSrcClass GstDshowAudioSrcClass;

struct _GstDshowAudioSrc
{
  GstAudioSrc src;

  /* device dshow reference (generally classid/name) */
  gchar *device;

  /* device friendly name */
  gchar *device_name;

  /* device index */
  gint device_index;

  /* list of caps created from the list of supported media types of the dshow capture filter */
  GstCaps *caps;

  /* list of dshow media types filter's pins mediatypes */
  GList *pins_mediatypes;

  /* dshow audio capture filter */
  IBaseFilter *audio_cap_filter;

  /* dshow fakesink filter */
  CDshowFakeSink *dshow_fakesink;

  /* graph manager interfaces */
  IMediaFilter *media_filter;
  IFilterGraph *filter_graph;

  /* bytes array */
  GByteArray *gbarray;
  GMutex gbarray_lock;

  gboolean is_running;
};

struct _GstDshowAudioSrcClass
{
  GstAudioSrcClass parent_class;
};

GType gst_dshowaudiosrc_get_type (void);

G_END_DECLS
#endif /* __GST_DSHOWAUDIOSRC_H__ */
