/* GStreamer
 * Copyright (C) 2016 Centricular Ltd.
 * Author: Arun Raghavan
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

#ifndef __TINYALSASINK_H__
#define __TINYALSASINK_H__

#include <tinyalsa/asoundlib.h>

#include <gst/audio/gstaudiosink.h>

G_BEGIN_DECLS

#define GST_TYPE_TINYALSA_SINK \
  (gst_tinyalsa_sink_get_type())
#define GST_TINYALSA_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TINYALSA_SINK,GstTinyalsaSink))
#define GST_TINYALSA_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TINYALSA_SINK,GstTinyalsaSinkClass))

typedef struct _GstTinyalsaSink GstTinyalsaSink;
typedef struct _GstTinyalsaSinkClass GstTinyalsaSinkClass;

struct _GstTinyalsaSink {
  GstAudioSink parent;

  int card;
  int device;

  struct pcm *pcm;

  GstCaps *cached_caps; /* for queries made while the device is open */
};

struct _GstTinyalsaSinkClass {
  GstAudioSinkClass parent_class;
};

GType gst_tinyalsa_sink_get_type (void);

#endif /* __TINYALSASINK_H__ */
