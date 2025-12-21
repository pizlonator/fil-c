/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2005 Wim Taymans <wim@fluendo.com>
 *
 * gstaudiosink.c: simple audio sink base class
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

/**
 * SECTION:gstaudiosink
 * @title: GstAudioSink
 * @short_description: Simple base class for audio sinks
 * @see_also: #GstAudioBaseSink, #GstAudioRingBuffer, #GstAudioSink.
 *
 * This is the most simple base class for audio sinks that only requires
 * subclasses to implement a set of simple functions:
 *
 * * `open()` :Open the device.
 *
 * * `prepare()` :Configure the device with the specified format.
 *
 * * `write()` :Write samples to the device.
 *
 * * `reset()` :Unblock writes and flush the device.
 *
 * * `delay()` :Get the number of samples written but not yet played
 * by the device.
 *
 * * `unprepare()` :Undo operations done by prepare.
 *
 * * `close()` :Close the device.
 *
 * All scheduling of samples and timestamps is done in this base class
 * together with #GstAudioBaseSink using a default implementation of a
 * #GstAudioRingBuffer that uses threads.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/audio/audio.h>
#include <gst/audio/gstdsd.h>
#include "gstaudiosink.h"
#include "gstaudioutilsprivate.h"

GST_DEBUG_CATEGORY_STATIC (gst_audio_sink_debug);
#define GST_CAT_DEFAULT gst_audio_sink_debug

#define GST_TYPE_AUDIO_SINK_RING_BUFFER        \
        (gst_audio_sink_ring_buffer_get_type())
#define GST_AUDIO_SINK_RING_BUFFER(obj)        \
        (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_SINK_RING_BUFFER,GstAudioSinkRingBuffer))
#define GST_AUDIO_SINK_RING_BUFFER_CLASS(klass) \
        (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_SINK_RING_BUFFER,GstAudioSinkRingBufferClass))
#define GST_AUDIO_SINK_RING_BUFFER_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_AUDIO_SINK_RING_BUFFER, GstAudioSinkRingBufferClass))
#define GST_AUDIO_SINK_RING_BUFFER_CAST(obj)        \
        ((GstAudioSinkRingBuffer *)obj)
#define GST_IS_AUDIO_SINK_RING_BUFFER(obj)     \
        (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_SINK_RING_BUFFER))
#define GST_IS_AUDIO_SINK_RING_BUFFER_CLASS(klass)\
        (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_SINK_RING_BUFFER))

typedef struct _GstAudioSinkRingBuffer GstAudioSinkRingBuffer;
typedef struct _GstAudioSinkRingBufferClass GstAudioSinkRingBufferClass;

#define GST_AUDIO_SINK_RING_BUFFER_GET_COND(buf) (&(((GstAudioSinkRingBuffer *)buf)->cond))
#define GST_AUDIO_SINK_RING_BUFFER_WAIT(buf)     (g_cond_wait (GST_AUDIO_SINK_RING_BUFFER_GET_COND (buf), GST_OBJECT_GET_LOCK (buf)))
#define GST_AUDIO_SINK_RING_BUFFER_SIGNAL(buf)   (g_cond_signal (GST_AUDIO_SINK_RING_BUFFER_GET_COND (buf)))
#define GST_AUDIO_SINK_RING_BUFFER_BROADCAST(buf)(g_cond_broadcast (GST_AUDIO_SINK_RING_BUFFER_GET_COND (buf)))

struct _GstAudioSinkRingBuffer
{
  GstAudioRingBuffer object;

  gboolean running;
  gint queuedseg;

  GCond cond;
};

struct _GstAudioSinkRingBufferClass
{
  GstAudioRingBufferClass parent_class;
};

static void gst_audio_sink_ring_buffer_class_init (GstAudioSinkRingBufferClass *
    klass);
static void gst_audio_sink_ring_buffer_init (GstAudioSinkRingBuffer *
    ringbuffer, GstAudioSinkRingBufferClass * klass);
static void gst_audio_sink_ring_buffer_dispose (GObject * object);
static void gst_audio_sink_ring_buffer_finalize (GObject * object);

static GstAudioRingBufferClass *ring_parent_class = NULL;

static gboolean gst_audio_sink_ring_buffer_open_device (GstAudioRingBuffer *
    buf);
static gboolean gst_audio_sink_ring_buffer_close_device (GstAudioRingBuffer *
    buf);
static gboolean gst_audio_sink_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec);
static gboolean gst_audio_sink_ring_buffer_release (GstAudioRingBuffer * buf);
static gboolean gst_audio_sink_ring_buffer_start (GstAudioRingBuffer * buf);
static gboolean gst_audio_sink_ring_buffer_pause (GstAudioRingBuffer * buf);
static gboolean gst_audio_sink_ring_buffer_resume (GstAudioRingBuffer * buf);
static gboolean gst_audio_sink_ring_buffer_stop (GstAudioRingBuffer * buf);
static guint gst_audio_sink_ring_buffer_delay (GstAudioRingBuffer * buf);
static gboolean gst_audio_sink_ring_buffer_activate (GstAudioRingBuffer * buf,
    gboolean active);
static void gst_audio_sink_ring_buffer_clear_all (GstAudioRingBuffer * buf);

/* ringbuffer abstract base class */
static GType
gst_audio_sink_ring_buffer_get_type (void)
{
  static GType ringbuffer_type = 0;

  if (!ringbuffer_type) {
    static const GTypeInfo ringbuffer_info = {
      sizeof (GstAudioSinkRingBufferClass),
      NULL,
      NULL,
      (GClassInitFunc) gst_audio_sink_ring_buffer_class_init,
      NULL,
      NULL,
      sizeof (GstAudioSinkRingBuffer),
      0,
      (GInstanceInitFunc) gst_audio_sink_ring_buffer_init,
      NULL
    };

    ringbuffer_type =
        g_type_register_static (GST_TYPE_AUDIO_RING_BUFFER,
        "GstAudioSinkRingBuffer", &ringbuffer_info, 0);
  }
  return ringbuffer_type;
}

static void
gst_audio_sink_ring_buffer_class_init (GstAudioSinkRingBufferClass * klass)
{
  GObjectClass *gobject_class;
  GstAudioRingBufferClass *gstringbuffer_class;

  gobject_class = (GObjectClass *) klass;
  gstringbuffer_class = (GstAudioRingBufferClass *) klass;

  ring_parent_class = g_type_class_peek_parent (klass);

  gobject_class->dispose = gst_audio_sink_ring_buffer_dispose;
  gobject_class->finalize = gst_audio_sink_ring_buffer_finalize;

  gstringbuffer_class->open_device =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_open_device);
  gstringbuffer_class->close_device =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_close_device);
  gstringbuffer_class->acquire =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_acquire);
  gstringbuffer_class->release =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_release);
  gstringbuffer_class->start =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_start);
  gstringbuffer_class->pause =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_pause);
  gstringbuffer_class->resume =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_resume);
  gstringbuffer_class->stop =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_stop);
  gstringbuffer_class->delay =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_delay);
  gstringbuffer_class->activate =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_activate);
  gstringbuffer_class->clear_all =
      GST_DEBUG_FUNCPTR (gst_audio_sink_ring_buffer_clear_all);
}

typedef gint (*WriteFunc) (GstAudioSink * sink, gpointer data, guint length);

/* this internal thread does nothing else but write samples to the audio device.
 * It will write each segment in the ringbuffer and will update the play
 * pointer.
 * The start/stop methods control the thread.
 */
static void
audioringbuffer_thread_func (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  GstAudioSinkRingBuffer *abuf = GST_AUDIO_SINK_RING_BUFFER_CAST (buf);
  WriteFunc writefunc;
  GstMessage *message;
  GValue val = { 0 };
  gpointer handle;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  GST_DEBUG_OBJECT (sink, "enter thread");

  GST_OBJECT_LOCK (abuf);
  GST_DEBUG_OBJECT (sink, "signal wait");
  GST_AUDIO_SINK_RING_BUFFER_SIGNAL (buf);
  GST_OBJECT_UNLOCK (abuf);

  writefunc = csink->write;
  if (writefunc == NULL)
    goto no_function;

  if (G_UNLIKELY (!__gst_audio_set_thread_priority (&handle)))
    GST_WARNING_OBJECT (sink, "failed to set thread priority");

  message = gst_message_new_stream_status (GST_OBJECT_CAST (buf),
      GST_STREAM_STATUS_TYPE_ENTER, GST_ELEMENT_CAST (sink));
  g_value_init (&val, GST_TYPE_G_THREAD);
  g_value_set_boxed (&val, g_thread_self ());
  gst_message_set_stream_status_object (message, &val);
  g_value_unset (&val);
  GST_DEBUG_OBJECT (sink, "posting ENTER stream status");
  gst_element_post_message (GST_ELEMENT_CAST (sink), message);

  while (TRUE) {
    gint left, len;
    guint8 *readptr;
    gint readseg;

    /* buffer must be started */
    if (gst_audio_ring_buffer_prepare_read (buf, &readseg, &readptr, &len)) {
      gint written;

      left = len;
      do {
        written = writefunc (sink, readptr, left);
        GST_LOG_OBJECT (sink, "transferred %d bytes of %d from segment %d",
            written, left, readseg);
        if (written < 0 || written > left) {
          /* might not be critical, it e.g. happens when aborting playback */
          GST_WARNING_OBJECT (sink,
              "error writing data in %s (reason: %s), skipping segment (left: %d, written: %d)",
              GST_DEBUG_FUNCPTR_NAME (writefunc),
              (errno > 1 ? g_strerror (errno) : "unknown"), left, written);
          break;
        } else if (written == 0 && G_UNLIKELY (g_atomic_int_get (&buf->state) !=
                GST_AUDIO_RING_BUFFER_STATE_STARTED)) {
          break;
        }
        left -= written;
        readptr += written;
      } while (left > 0);

      /* clear written samples */
      gst_audio_ring_buffer_clear (buf, readseg);

      /* we wrote one segment */
      gst_audio_ring_buffer_advance (buf, 1);
    } else {
      GST_OBJECT_LOCK (abuf);
      if (!abuf->running)
        goto stop_running;
      if (G_UNLIKELY (g_atomic_int_get (&buf->state) ==
              GST_AUDIO_RING_BUFFER_STATE_STARTED)) {
        GST_OBJECT_UNLOCK (abuf);
        continue;
      }
      GST_DEBUG_OBJECT (sink, "signal wait");
      GST_AUDIO_SINK_RING_BUFFER_SIGNAL (buf);
      GST_DEBUG_OBJECT (sink, "wait for action");
      GST_AUDIO_SINK_RING_BUFFER_WAIT (buf);
      GST_DEBUG_OBJECT (sink, "got signal");
      if (!abuf->running)
        goto stop_running;
      GST_DEBUG_OBJECT (sink, "continue running");
      GST_OBJECT_UNLOCK (abuf);
    }
  }

  /* Will never be reached */
  g_assert_not_reached ();
  return;

  /* ERROR */
no_function:
  {
    GST_DEBUG_OBJECT (sink, "no write function, exit thread");
    return;
  }
stop_running:
  {
    GST_OBJECT_UNLOCK (abuf);
    GST_DEBUG_OBJECT (sink, "stop running, exit thread");
    message = gst_message_new_stream_status (GST_OBJECT_CAST (buf),
        GST_STREAM_STATUS_TYPE_LEAVE, GST_ELEMENT_CAST (sink));
    g_value_init (&val, GST_TYPE_G_THREAD);
    g_value_set_boxed (&val, g_thread_self ());
    gst_message_set_stream_status_object (message, &val);
    g_value_unset (&val);
    GST_DEBUG_OBJECT (sink, "posting LEAVE stream status");
    gst_element_post_message (GST_ELEMENT_CAST (sink), message);

    if (G_UNLIKELY (!__gst_audio_restore_thread_priority (handle)))
      GST_WARNING_OBJECT (sink, "failed to restore thread priority");
    return;
  }
}

static void
gst_audio_sink_ring_buffer_init (GstAudioSinkRingBuffer * ringbuffer,
    GstAudioSinkRingBufferClass * g_class)
{
  ringbuffer->running = FALSE;
  ringbuffer->queuedseg = 0;

  g_cond_init (&ringbuffer->cond);
}

static void
gst_audio_sink_ring_buffer_dispose (GObject * object)
{
  G_OBJECT_CLASS (ring_parent_class)->dispose (object);
}

static void
gst_audio_sink_ring_buffer_finalize (GObject * object)
{
  GstAudioSinkRingBuffer *ringbuffer = GST_AUDIO_SINK_RING_BUFFER_CAST (object);

  g_cond_clear (&ringbuffer->cond);

  G_OBJECT_CLASS (ring_parent_class)->finalize (object);
}

static gboolean
gst_audio_sink_ring_buffer_open_device (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = TRUE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->open)
    result = csink->open (sink);

  if (!result)
    goto could_not_open;

  return result;

could_not_open:
  {
    GST_DEBUG_OBJECT (sink, "could not open device");
    return FALSE;
  }
}

static gboolean
gst_audio_sink_ring_buffer_close_device (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = TRUE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->close)
    result = csink->close (sink);

  if (!result)
    goto could_not_close;

  return result;

could_not_close:
  {
    GST_DEBUG_OBJECT (sink, "could not close device");
    return FALSE;
  }
}

static gboolean
gst_audio_sink_ring_buffer_acquire (GstAudioRingBuffer * buf,
    GstAudioRingBufferSpec * spec)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = FALSE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->prepare)
    result = csink->prepare (sink, spec);
  if (!result)
    goto could_not_prepare;

  /* set latency to one more segment as we need some headroom */
  spec->seglatency = spec->segtotal + 1;

  buf->size = spec->segtotal * spec->segsize;

  buf->memory = g_malloc (buf->size);

  switch (buf->spec.type) {
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_RAW:
      gst_audio_format_info_fill_silence (buf->spec.info.finfo, buf->memory,
          buf->size);
      break;
    case GST_AUDIO_RING_BUFFER_FORMAT_TYPE_DSD:
      memset (buf->memory, GST_DSD_SILENCE_PATTERN_BYTE, buf->size);
      break;
    default:
      /* FIXME, non-raw formats get 0 as the empty sample */
      memset (buf->memory, 0, buf->size);
      break;
  }


  return TRUE;

  /* ERRORS */
could_not_prepare:
  {
    GST_DEBUG_OBJECT (sink, "could not prepare device");
    return FALSE;
  }
}

static gboolean
gst_audio_sink_ring_buffer_activate (GstAudioRingBuffer * buf, gboolean active)
{
  GstAudioSink *sink;
  GstAudioSinkRingBuffer *abuf;
  GError *error = NULL;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  abuf = GST_AUDIO_SINK_RING_BUFFER_CAST (buf);

  if (active) {
    abuf->running = TRUE;

    GST_DEBUG_OBJECT (sink, "starting thread");

    sink->thread = g_thread_try_new ("audiosink-ringbuffer",
        (GThreadFunc) audioringbuffer_thread_func, buf, &error);

    if (!sink->thread || error != NULL)
      goto thread_failed;

    GST_DEBUG_OBJECT (sink, "waiting for thread");
    /* the object lock is taken */
    GST_AUDIO_SINK_RING_BUFFER_WAIT (buf);
    GST_DEBUG_OBJECT (sink, "thread is started");
  } else {
    abuf->running = FALSE;
    GST_DEBUG_OBJECT (sink, "signal wait");
    GST_AUDIO_SINK_RING_BUFFER_SIGNAL (buf);

    GST_OBJECT_UNLOCK (buf);

    /* join the thread */
    g_thread_join (sink->thread);

    GST_OBJECT_LOCK (buf);
  }
  return TRUE;

  /* ERRORS */
thread_failed:
  {
    if (error)
      GST_ERROR_OBJECT (sink, "could not create thread %s", error->message);
    else
      GST_ERROR_OBJECT (sink, "could not create thread for unknown reason");
    g_clear_error (&error);
    return FALSE;
  }
}

/* function is called with LOCK */
static gboolean
gst_audio_sink_ring_buffer_release (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  gboolean result = FALSE;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* free the buffer */
  g_free (buf->memory);
  buf->memory = NULL;

  if (csink->unprepare)
    result = csink->unprepare (sink);

  if (!result)
    goto could_not_unprepare;

  GST_DEBUG_OBJECT (sink, "unprepared");

  return result;

could_not_unprepare:
  {
    GST_DEBUG_OBJECT (sink, "could not unprepare device");
    return FALSE;
  }
}

static gboolean
gst_audio_sink_ring_buffer_start (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));

  GST_DEBUG_OBJECT (sink, "start, sending signal");
  GST_AUDIO_SINK_RING_BUFFER_SIGNAL (buf);

  return TRUE;
}

static gboolean
gst_audio_sink_ring_buffer_pause (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* unblock any pending writes to the audio device */
  if (csink->pause) {
    GST_DEBUG_OBJECT (sink, "pause...");
    csink->pause (sink);
    GST_DEBUG_OBJECT (sink, "pause done");
  } else if (csink->reset) {
    /* fallback to reset for audio sinks that don't provide pause */
    GST_DEBUG_OBJECT (sink, "reset...");
    csink->reset (sink);
    GST_DEBUG_OBJECT (sink, "reset done");
  }
  return TRUE;
}

static gboolean
gst_audio_sink_ring_buffer_resume (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->resume) {
    GST_DEBUG_OBJECT (sink, "resume...");
    csink->resume (sink);
    GST_DEBUG_OBJECT (sink, "resume done");
  }

  gst_audio_sink_ring_buffer_start (buf);

  return TRUE;
}

static gboolean
gst_audio_sink_ring_buffer_stop (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  /* unblock any pending writes to the audio device */
  if (csink->stop) {
    GST_DEBUG_OBJECT (sink, "stop...");
    csink->stop (sink);
    GST_DEBUG_OBJECT (sink, "stop done");
  } else if (csink->reset) {
    /* fallback to reset for audio sinks that don't provide stop */
    GST_DEBUG_OBJECT (sink, "reset...");
    csink->reset (sink);
    GST_DEBUG_OBJECT (sink, "reset done");
  }
#if 0
  if (abuf->running) {
    GST_DEBUG_OBJECT (sink, "stop, waiting...");
    GST_AUDIO_SINK_RING_BUFFER_WAIT (buf);
    GST_DEBUG_OBJECT (sink, "stopped");
  }
#endif

  return TRUE;
}

static guint
gst_audio_sink_ring_buffer_delay (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;
  guint res = 0;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->delay)
    res = csink->delay (sink);

  return res;
}

static void
gst_audio_sink_ring_buffer_clear_all (GstAudioRingBuffer * buf)
{
  GstAudioSink *sink;
  GstAudioSinkClass *csink;

  sink = GST_AUDIO_SINK (GST_OBJECT_PARENT (buf));
  csink = GST_AUDIO_SINK_GET_CLASS (sink);

  if (csink->extension->clear_all) {
    GST_DEBUG_OBJECT (sink, "clear all");
    csink->extension->clear_all (sink);
  }

  /* chain up to the parent implementation */
  ring_parent_class->clear_all (buf);
}

/* AudioSink signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  ARG_0,
};

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_audio_sink_debug, "audiosink", 0, "audiosink element"); \
    g_type_add_class_private (g_define_type_id, \
        sizeof (GstAudioSinkClassExtension));
#define gst_audio_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstAudioSink, gst_audio_sink,
    GST_TYPE_AUDIO_BASE_SINK, _do_init);

static GstAudioRingBuffer *gst_audio_sink_create_ringbuffer (GstAudioBaseSink *
    sink);

static void
gst_audio_sink_class_init (GstAudioSinkClass * klass)
{
  GstAudioBaseSinkClass *gstaudiobasesink_class;

  gstaudiobasesink_class = (GstAudioBaseSinkClass *) klass;

  gstaudiobasesink_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_audio_sink_create_ringbuffer);

  g_type_class_ref (GST_TYPE_AUDIO_SINK_RING_BUFFER);

  klass->extension = G_TYPE_CLASS_GET_PRIVATE (klass,
      GST_TYPE_AUDIO_SINK, GstAudioSinkClassExtension);
}

static void
gst_audio_sink_init (GstAudioSink * audiosink)
{
}

static GstAudioRingBuffer *
gst_audio_sink_create_ringbuffer (GstAudioBaseSink * sink)
{
  GstAudioRingBuffer *buffer;

  GST_DEBUG_OBJECT (sink, "creating ringbuffer");
  buffer = g_object_new (GST_TYPE_AUDIO_SINK_RING_BUFFER, NULL);
  GST_DEBUG_OBJECT (sink, "created ringbuffer @%p", buffer);

  return buffer;
}
