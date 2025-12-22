/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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


#ifndef __GST_MODPLUG_H__
#define __GST_MODPLUG_H__

#include <gst/gst.h>

G_BEGIN_DECLS
        
#define GST_TYPE_MODPLUG \
  (gst_modplug_get_type())
  
#define GST_MODPLUG(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MODPLUG,GstModPlug))
#define GST_MODPLUG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MODPLUG,GstModPlugClass))
#define GST_IS_MODPLUG(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MODPLUG))
#define GST_IS_MODPLUG_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MODPLUG))
  
struct _GstModPlug {
  GstElement  element;

  GstPad     *sinkpad;
  GstPad     *srcpad;

  /* properties */
  const gchar *songname;
  gboolean     reverb;
  gint         reverb_depth;
  gint         reverb_delay;
  gboolean     megabass;
  gint         megabass_amount;
  gint         megabass_range;
  gboolean     surround;
  gint         surround_depth;
  gint         surround_delay;
  gboolean     noise_reduction;
  gint         bits;
  gboolean     oversamp;
  gint         channel;
  gint         frequency;

  /* state */
  GstBuffer   *buffer;

  gint32       read_bytes;
  gint32       read_samples;

  gint64       seek_at;      /* pending seek, or -1                 */
  gint64       song_size;    /* size of the raw song data in bytes  */
  gint64       song_length;  /* duration of the song in nanoseconds */
  gint64       offset;       /* current position in samples         */
  gint64       timestamp;

  CSoundFile  *mSoundFile;
};

struct _GstModPlugClass {
  GstElementClass parent_class;
};

typedef struct _GstModPlug GstModPlug;
typedef struct _GstModPlugClass GstModPlugClass;

GType gst_modplug_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (modplug);

G_END_DECLS

#endif /* __GST_MODPLUG_H__ */
