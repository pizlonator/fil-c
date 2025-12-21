/* GStreamer
 *
 * unit test for the audiosink base class
 *
 * Copyright (C) 2020 Seungha Yang <seungha.yang@navercorp.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/audio/gstaudiosink.h>

#define GST_TYPE_AUDIO_FOO_SINK           (gst_audio_foo_sink_get_type())
#define GST_AUDIO_FOO_SINK(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_FOO_SINK,GstAudioFooSink))
#define GST_AUDIO_FOO_SINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_FOO_SINK,GstAudioFooSinkClass))
typedef struct _GstAudioFooSink GstAudioFooSink;
typedef struct _GstAudioFooSinkClass GstAudioFooSinkClass;

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL)));

struct _GstAudioFooSink
{
  GstAudioSink parent;

  guint num_clear_all_call;
};

struct _GstAudioFooSinkClass
{
  GstAudioSinkClass parent_class;
};

GType gst_audio_foo_sink_get_type (void);
G_DEFINE_TYPE (GstAudioFooSink, gst_audio_foo_sink, GST_TYPE_AUDIO_SINK);

static void
gst_audio_foo_sink_clear_all (GstAudioSink * sink)
{
  GstAudioFooSink *self = GST_AUDIO_FOO_SINK (sink);

  self->num_clear_all_call++;
}

static void
gst_audio_foo_sink_init (GstAudioFooSink * src)
{
}

static void
gst_audio_foo_sink_class_init (GstAudioFooSinkClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioSinkClass *audiosink_class = GST_AUDIO_SINK_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_set_metadata (element_class,
      "AudioFooSink", "Sink/Audio",
      "Audio Sink Unit Test element", "Foo Bar <foo@bar.com>");

  audiosink_class->extension->clear_all = gst_audio_foo_sink_clear_all;
}

GST_START_TEST (test_class_extension)
{
  GstAudioFooSink *foosink = NULL;
  GstAudioBaseSink *bsink;
  GstAudioRingBuffer *ringbuffer;

  foosink = g_object_new (GST_TYPE_AUDIO_FOO_SINK, NULL);
  fail_unless (foosink != NULL);

  /* change state to READY to prepare audio ringbuffer */
  fail_unless (gst_element_set_state (GST_ELEMENT (foosink),
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS);

  bsink = GST_AUDIO_BASE_SINK (foosink);
  ringbuffer = bsink->ringbuffer;
  fail_unless (ringbuffer != NULL);

  /* This will invoke GstAudioSink::clear_all method */
  gst_audio_ring_buffer_clear_all (ringbuffer);
  fail_unless_equals_int (foosink->num_clear_all_call, 1);

  gst_element_set_state (GST_ELEMENT (foosink), GST_STATE_NULL);
  gst_clear_object (&foosink);
}

GST_END_TEST;


static Suite *
audiosink_suite (void)
{
  Suite *s = suite_create ("audiosink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_class_extension);

  return s;
}

GST_CHECK_MAIN (audiosink)
