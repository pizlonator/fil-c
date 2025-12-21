/*
 * Copyright (C) 2001 CodeFactory AB
 * Copyright (C) 2001 Thomas Nyberg <thomas@codefactory.se>
 * Copyright (C) 2001-2002 Andy Wingo <apwingo@eos.ncsu.edu>
 * Copyright (C) 2003 Benjamin Otte <in7y118@public.uni-hamburg.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifndef __GST_ALSA_H__
#define __GST_ALSA_H__


#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API

#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <alsa/error.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>

#define GST_CHECK_ALSA_VERSION(major,minor,micro) \
    (SND_LIB_MAJOR > (major) || \
     (SND_LIB_MAJOR == (major) && SND_LIB_MINOR > (minor)) || \
     (SND_LIB_MAJOR == (major) && SND_LIB_MINOR == (minor) && \
      SND_LIB_SUBMINOR >= (micro)))

#define PASSTHROUGH_CAPS \
    "audio/x-ac3, framed = (boolean) true;" \
    "audio/x-eac3, framed = (boolean) true; " \
    "audio/x-dts, framed = (boolean) true, " \
      "block-size = (int) { 512, 1024, 2048 }; " \
    "audio/mpeg, mpegversion = (int) 1, " \
      "mpegaudioversion = (int) [ 1, 3 ], parsed = (boolean) true;"


GST_DEBUG_CATEGORY_EXTERN (alsa_debug);
#define GST_CAT_DEFAULT alsa_debug

snd_pcm_t * gst_alsa_open_iec958_pcm (GstObject * obj, gchar *device);

GstCaps * gst_alsa_probe_supported_formats (GstObject      * obj,
                                            gchar          * device,
                                            snd_pcm_t      * handle,
                                            const GstCaps  * template_caps);

gchar   * gst_alsa_find_device_name (GstObject        * obj,
                                     const gchar      * device,
                                     snd_pcm_t        * handle,
                                     snd_pcm_stream_t   stream);

gchar *   gst_alsa_find_card_name   (GstObject        * obj,
                                     const gchar      * devcard,
                                     snd_pcm_stream_t   stream);

void      gst_alsa_add_channel_reorder_map (GstObject * obj,
                                            GstCaps   * caps);

extern const GstAudioChannelPosition alsa_position[][8];
#ifdef SND_CHMAP_API_VERSION
gboolean alsa_chmap_to_channel_positions (const snd_pcm_chmap_t *chmap,
                                          GstAudioChannelPosition *pos);

void alsa_detect_channels_mapping (GstObject * obj,
                                   snd_pcm_t * handle,
                                   GstAudioRingBufferSpec * spec,
                                   guint channels,
                                   GstAudioRingBuffer * buf);
#endif

#endif /* __GST_ALSA_H__ */
