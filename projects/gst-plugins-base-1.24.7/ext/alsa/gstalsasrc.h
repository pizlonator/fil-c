/* GStreamer
 * Copyright (C)  2005 Wim Taymans <wim@fluendo.com>
 *
 * gstalsasrc.h: 
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


#ifndef __GST_ALSASRC_H__
#define __GST_ALSASRC_H__

#include <gst/audio/audio.h>
#include "gstalsa.h"

G_BEGIN_DECLS

#define GST_TYPE_ALSA_SRC            (gst_alsasrc_get_type())
#define GST_ALSA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ALSA_SRC,GstAlsaSrc))
#define GST_ALSA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ALSA_SRC,GstAlsaSrcClass))
#define GST_IS_ALSA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ALSA_SRC))
#define GST_IS_ALSA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ALSA_SRC))
#define GST_ALSA_SRC_CAST(obj)       ((GstAlsaSrc *)(obj))

#define GST_ALSA_SRC_GET_LOCK(obj)  (&GST_ALSA_SRC_CAST (obj)->alsa_lock)
#define GST_ALSA_SRC_LOCK(obj)      (g_mutex_lock (GST_ALSA_SRC_GET_LOCK (obj)))
#define GST_ALSA_SRC_UNLOCK(obj)    (g_mutex_unlock (GST_ALSA_SRC_GET_LOCK (obj)))

typedef struct _GstAlsaSrc GstAlsaSrc;
typedef struct _GstAlsaSrcClass GstAlsaSrcClass;

/**
 * GstAlsaSrc:
 *
 * Opaque data structure
 */
struct _GstAlsaSrc {
  GstAudioSrc           src;

  gchar                 *device;

  snd_pcm_t             *handle;
  snd_pcm_hw_params_t   *hwparams;
  snd_pcm_sw_params_t   *swparams;

  GstCaps               *cached_caps;

  snd_pcm_access_t      access;
  snd_pcm_format_t      format;
  guint                 rate;
  guint                 channels;
  gint                  bpf;
  gboolean              driver_timestamps;
  gboolean              use_driver_timestamps;

  guint                 buffer_time;
  guint                 period_time;
  snd_pcm_uframes_t     buffer_size;
  snd_pcm_uframes_t     period_size;

  GMutex                alsa_lock;
};

struct _GstAlsaSrcClass {
  GstAudioSrcClass parent_class;
};

GType gst_alsasrc_get_type(void);

G_END_DECLS

#endif /* __GST_ALSASRC_H__ */
