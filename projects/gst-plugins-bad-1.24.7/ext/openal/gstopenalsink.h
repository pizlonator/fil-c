/*
 * GStreamer
 *
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2009-2010 Chris Robinson <chris.kcat@gmail.com>
 * Copyright (C) 2013 Juan Manuel Borges Caño <juanmabcmail@gmail.com>
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

#ifndef __GST_OPENALSINK_H__
#define __GST_OPENALSINK_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>

#ifdef _WIN32
#include <al.h>
#include <alc.h>
#include <alext.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#endif

G_BEGIN_DECLS

#define GST_TYPE_OPENAL_SINK \
    (gst_openal_sink_get_type())
#define GST_OPENAL_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENAL_SINK,GstOpenALSink))
#define GST_OPENAL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENAL_SINK,GstOpenALSinkClass))
#define GST_IS_OPENAL_SINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENAL_SINK))
#define GST_IS_OPENAL_SINK_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENAL_SINK))
#define GST_OPENAL_SINK_CAST(obj) \
    ((GstOpenALSink*)obj)

#if 1
#define GST_ALC_ERROR(Device)  ("ALC error: %s", alcGetString((Device), alcGetError((Device))))
#else
#define GST_ALC_ERROR(Device)  ("ALC error: 0x%x", alcGetError((Device)))
#endif

typedef struct _GstOpenALSink GstOpenALSink;
typedef struct _GstOpenALSinkClass GstOpenALSinkClass;

#define GST_OPENAL_SINK_GET_LOCK(obj) (&GST_OPENAL_SINK_CAST(obj)->openal_lock)
#define GST_OPENAL_SINK_LOCK(obj)     (g_mutex_lock(GST_OPENAL_SINK_GET_LOCK(obj)))
#define GST_OPENAL_SINK_UNLOCK(obj)   (g_mutex_unlock(GST_OPENAL_SINK_GET_LOCK(obj)))

struct _GstOpenALSink
{
  GstAudioSink sink;

  gchar *device_name;

  ALCdevice *default_device;
  /* When set, device is not owned */
  ALCdevice *user_device;

  ALCcontext *default_context;
  /* When set, device or context is not owned */
  ALCcontext *user_context;

  ALuint default_source;
  /* When set, source is not owned */
  ALuint user_source;

  ALuint buffer_idx;
  ALuint buffer_count;
  ALuint *buffers;
  ALuint buffer_length;

  ALenum format;
  ALuint rate;
  ALuint channels;
  ALuint bytes_per_sample;

  ALboolean write_reset;

  GstCaps *probed_caps;

  GMutex openal_lock;
};

struct _GstOpenALSinkClass
{
  GstAudioSinkClass parent_class;
};

GType gst_openal_sink_get_type (void);

G_END_DECLS

#endif /* __GST_OPENALSINK_H__ */
