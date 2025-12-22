/* 
 * GStreamer
 * Copyright (C) 2011 Stefan Sauer <ensonic@users.sf.net>
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
 
#ifndef __GST_FREEVERB_H__
#define __GST_FREEVERB_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_FREEVERB            (gst_freeverb_get_type())
#define GST_FREEVERB(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FREEVERB,GstFreeverb))
#define GST_IS_FREEVERB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FREEVERB))
#define GST_FREEVERB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_FREEVERB,GstFreeverbClass))
#define GST_IS_FREEVERB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_FREEVERB))
#define GST_FREEVERB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_FREEVERB,GstFreeverbClass))

typedef struct _GstFreeverb        GstFreeverb;
typedef struct _GstFreeverbClass   GstFreeverbClass;
typedef struct _GstFreeverbPrivate GstFreeverbPrivate;

typedef gboolean (*GstFreeverbProcessFunc)(GstFreeverb*, guint8*, guint8*, guint);

struct _GstFreeverb {
  GstBaseTransform element;
  
  /* < private > */
  gfloat room_size;
  gfloat damping;
  gfloat pan_width;
  gfloat level;

  GstFreeverbProcessFunc process;
  GstAudioInfo info;

  gboolean drained;
  
  GstFreeverbPrivate *priv;
};

struct _GstFreeverbClass {
  GstBaseTransformClass parent_class;
};

GType gst_freeverb_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (freeverb);

G_END_DECLS

#endif /* __GST_FREEVERB_H__ */
