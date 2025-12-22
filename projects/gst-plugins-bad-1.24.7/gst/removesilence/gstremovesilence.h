/* GStreamer
 * Copyright (C) 2011 Tiago Katcipis <tiagokatcipis@gmail.com>
 * Copyright (C) 2011 Paulo Pizarro  <paulo.pizarro@gmail.com>
 * Copyright (C) 2012-2016 Nicola Murino  <nicola.murino@gmail.com>
 *
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

#ifndef __GST_REMOVE_SILENCE_H__
#define __GST_REMOVE_SILENCE_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include "vad_private.h"

G_BEGIN_DECLS

#define GST_TYPE_REMOVE_SILENCE \
  (gst_remove_silence_get_type())
#define GST_REMOVE_SILENCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_REMOVE_SILENCE,GstRemoveSilence))
#define GST_REMOVE_SILENCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_REMOVE_SILENCE,GstRemoveSilenceClass))
#define GST_IS_REMOVESILENCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_REMOVE_SILENCE))
#define GST_IS_REMOVESILENCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_REMOVE_SILENCE))

typedef struct _GstRemoveSilence {
  GstBaseTransform parent;
  VADFilter* vad;
  gboolean remove;
  gboolean squash;
  gboolean silent;
  guint16 minimum_silence_buffers;
  guint64 minimum_silence_time;
  /* filter params protected by STREAM_LOCK */
  guint64 ts_offset;
  gboolean silence_detected;
  guint64 consecutive_silence_buffers;
  guint64 consecutive_silence_time;
} GstRemoveSilence;

typedef struct _GstRemoveSilenceClass {
  GstBaseTransformClass parent_class;
} GstRemoveSilenceClass;

GType gst_remove_silence_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (removesilence);

G_END_DECLS

#endif /* __GST_REMOVE_SILENCE_H__ */
