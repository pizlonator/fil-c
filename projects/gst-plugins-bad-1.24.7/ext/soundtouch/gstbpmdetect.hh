/* GStreamer
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org> 
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


#ifndef __GST_BPM_DETECT_H__
#define __GST_BPM_DETECT_H__


#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/audio/gstaudiofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_BPM_DETECT            (gst_bpm_detect_get_type())
#define GST_BPM_DETECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BPM_DETECT,GstBPMDetect))
#define GST_IS_BPM_DETECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BPM_DETECT))
#define GST_BPM_DETECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_BPM_DETECT,GstBPMDetectClass))
#define GST_IS_BPM_DETECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_BPM_DETECT))

typedef struct _GstBPMDetect GstBPMDetect;
typedef struct _GstBPMDetectClass GstBPMDetectClass;
typedef struct _GstBPMDetectPrivate GstBPMDetectPrivate;

struct _GstBPMDetect {
  GstAudioFilter element;

  gfloat bpm;

  GstBPMDetectPrivate *priv;
};

struct _GstBPMDetectClass {
  GstAudioFilterClass parent_class;
};

GType gst_bpm_detect_get_type (void);
GST_ELEMENT_REGISTER_DECLARE (bpmdetect);


G_END_DECLS

#endif /* __GST_BPM_DETECT_H__ */
