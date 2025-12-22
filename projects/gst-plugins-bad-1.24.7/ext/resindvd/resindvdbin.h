/* GStreamer
 * Copyright (C) 2008 Jan Schmidt <thaytan@noraisin.net>
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

#ifndef __RESINDVDBIN_H__
#define __RESINDVDBIN_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define RESIN_TYPE_DVDBIN \
  (rsn_dvdbin_get_type())
#define RESINDVDBIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),RESIN_TYPE_DVDBIN,RsnDvdBin))
#define RESINDVDBIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),RESIN_TYPE_DVDBIN,RsnDvdBinClass))
#define IS_RESINDVDBIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),RESIN_TYPE_DVDBIN))
#define IS_RESINDVDBIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),RESIN_TYPE_DVDBIN))

typedef struct _RsnDvdBin      RsnDvdBin;
typedef struct _RsnDvdBinClass RsnDvdBinClass;

#define DVD_ELEM_SOURCE         0
#define DVD_ELEM_DEMUX          1
#define DVD_ELEM_MQUEUE         2
#define DVD_ELEM_SPUQ           3
#define DVD_ELEM_VIDPARSE       4
#define DVD_ELEM_VIDDEC         5
#define DVD_ELEM_PARSET         6
#define DVD_ELEM_AUDPARSE       7 
#define DVD_ELEM_AUDDEC         8
#define DVD_ELEM_VIDQ           9
#define DVD_ELEM_SPU_SELECT     10
#define DVD_ELEM_AUD_SELECT     11
#define DVD_ELEM_LAST           12

struct _RsnDvdBin
{
  GstBin element;

  /* Protects pieces list and properties */
  GMutex dvd_lock;
  GMutex preroll_lock;

  gchar *device;
  gchar *last_uri;
  GstElement *pieces[DVD_ELEM_LAST];

  GstPad *video_pad;
  GstPad *audio_pad;
  GstPad *subpicture_pad;

  gboolean video_added;
  gboolean audio_added;
  gboolean audio_broken;
  gboolean subpicture_added;

  gboolean did_no_more_pads;

  GList *mq_req_pads;
};

struct _RsnDvdBinClass 
{
  GstBinClass parent_class;
};

GType rsn_dvdbin_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (rsndvdbin);

G_END_DECLS

#endif /* __RESINDVDBIN_H__ */
