/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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

#ifndef __GST_WASAPI_SINK_H__
#define __GST_WASAPI_SINK_H__

#include "gstwasapiutil.h"

G_BEGIN_DECLS
#define GST_TYPE_WASAPI_SINK \
  (gst_wasapi_sink_get_type ())
#define GST_WASAPI_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_WASAPI_SINK, GstWasapiSink))
#define GST_WASAPI_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_WASAPI_SINK, GstWasapiSinkClass))
#define GST_IS_WASAPI_SINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_WASAPI_SINK))
#define GST_IS_WASAPI_SINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_WASAPI_SINK))
typedef struct _GstWasapiSink GstWasapiSink;
typedef struct _GstWasapiSinkClass GstWasapiSinkClass;

struct _GstWasapiSink
{
  GstAudioSink parent;

  GstMMDeviceEnumerator *enumerator;

  IMMDevice *device;
  IAudioClient *client;
  IAudioRenderClient *render_client;
  HANDLE event_handle;
  HANDLE cancellable;
  /* Client was reset, so it needs to be started again */
  gboolean client_needs_restart;

  /* Actual size of the allocated buffer */
  guint buffer_frame_count;
  /* The mix format that wasapi prefers in shared mode */
  WAVEFORMATEX *mix_format;
  /* The probed caps that we can accept */
  GstCaps *cached_caps;
  /* The channel positions in the data to be written to the device we
   * will pass this to GstAudioRingbuffer so it can to it translate
   * from the native GStreamer channel layout. */
  GstAudioChannelPosition *positions;

  /* properties */
  gint role;
  gint sharemode;
  gboolean mute;
  gboolean low_latency;
  gboolean try_audioclient3;
  wchar_t *device_strid;
};

struct _GstWasapiSinkClass
{
  GstAudioSinkClass parent_class;
};

GType gst_wasapi_sink_get_type (void);

G_END_DECLS
#endif /* __GST_WASAPI_SINK_H__ */
