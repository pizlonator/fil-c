/* GStreamer
 * Copyright (C)  2005 Wim Taymans <wim@fluendo.com>
 *
 * gstalsasink.h:
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


#ifndef __GST_ALSASINK_H__
#define __GST_ALSASINK_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <alsa/asoundlib.h>

G_BEGIN_DECLS

#define GST_TYPE_ALSA_SINK            (gst_alsasink_get_type())
#define GST_ALSA_SINK(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ALSA_SINK,GstAlsaSink))
#define GST_ALSA_SINK_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ALSA_SINK,GstAlsaSinkClass))
#define GST_IS_ALSA_SINK(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ALSA_SINK))
#define GST_IS_ALSA_SINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ALSA_SINK))
#define GST_ALSA_SINK_CAST(obj)       ((GstAlsaSink *) (obj))

typedef struct _GstAlsaSink GstAlsaSink;
typedef struct _GstAlsaSinkClass GstAlsaSinkClass;

#define GST_ALSA_SINK_GET_LOCK(obj)	(&GST_ALSA_SINK_CAST (obj)->alsa_lock)
#define GST_ALSA_SINK_LOCK(obj)	    (g_mutex_lock (GST_ALSA_SINK_GET_LOCK (obj)))
#define GST_ALSA_SINK_UNLOCK(obj)   (g_mutex_unlock (GST_ALSA_SINK_GET_LOCK (obj)))

#define GST_DELAY_SINK_GET_LOCK(obj)	(&GST_ALSA_SINK_CAST (obj)->delay_lock)
#define GST_DELAY_SINK_LOCK(obj)	        (g_mutex_lock (GST_DELAY_SINK_GET_LOCK (obj)))
#define GST_DELAY_SINK_UNLOCK(obj)	(g_mutex_unlock (GST_DELAY_SINK_GET_LOCK (obj)))

/**
 * GstAlsaSink:
 *
 * Opaque data structure
 */
struct _GstAlsaSink {
  GstAudioSink    sink;

  gchar                 *device;

  snd_pcm_t             *handle;

  snd_pcm_access_t access;
  snd_pcm_format_t format;
  guint rate;
  guint channels;
  gint bpf;
  gboolean iec958;
  gboolean need_swap;

  guint buffer_time;
  guint period_time;
  snd_pcm_uframes_t buffer_size;
  snd_pcm_uframes_t period_size;

  GstCaps *cached_caps;

  gboolean is_paused;
  gboolean after_paused;
  gboolean hw_support_pause;
  snd_pcm_sframes_t pos_in_buffer;

  GMutex alsa_lock;
  GMutex delay_lock;
};

struct _GstAlsaSinkClass {
  GstAudioSinkClass parent_class;
};

GType gst_alsasink_get_type(void);

G_END_DECLS

#endif /* __GST_ALSASINK_H__ */
