/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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
#include <config.h>
#endif

#include "gstasiosink.h"
#include "gstasioobject.h"
#include "gstasioringbuffer.h"
#include <atlconv.h>
#include <string.h>
#include <set>
#include <vector>

GST_DEBUG_CATEGORY_STATIC (gst_asio_sink_debug);
#define GST_CAT_DEFAULT gst_asio_sink_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_ASIO_STATIC_CAPS));

enum
{
  PROP_0,
  PROP_DEVICE_CLSID,
  PROP_OUTPUT_CHANNELS,
  PROP_BUFFER_SIZE,
  PROP_OCCUPY_ALL_CHANNELS,
};

#define DEFAULT_BUFFER_SIZE 0
#define DEFAULT_OCCUPY_ALL_CHANNELS TRUE

struct _GstAsioSink
{
  GstAudioSink parent;

  /* properties */
  gchar *device_clsid;
  gchar *output_channels;
  guint buffer_size;
  gboolean occupy_all_channels;
};

static void gst_asio_sink_finalize (GObject * object);
static void gst_asio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_asio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_asio_sink_get_caps (GstBaseSink * sink, GstCaps * filter);

static GstAudioRingBuffer *gst_asio_sink_create_ringbuffer (GstAudioBaseSink *
    sink);

#define gst_asio_sink_parent_class parent_class
G_DEFINE_TYPE (GstAsioSink, gst_asio_sink, GST_TYPE_AUDIO_BASE_SINK);

static void
gst_asio_sink_class_init (GstAsioSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstAudioBaseSinkClass *audiobasesink_class =
      GST_AUDIO_BASE_SINK_CLASS (klass);

  gobject_class->finalize = gst_asio_sink_finalize;
  gobject_class->set_property = gst_asio_sink_set_property;
  gobject_class->get_property = gst_asio_sink_get_property;

  g_object_class_install_property (gobject_class, PROP_DEVICE_CLSID,
      g_param_spec_string ("device-clsid", "Device CLSID",
          "ASIO device CLSID as a string", NULL,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_CHANNELS,
      g_param_spec_string ("output-channels", "Output Channels",
          "Comma-separated list of ASIO channels to output", NULL,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffer Size",
          "Preferred buffer size (0 for default)",
          0, G_MAXINT32, DEFAULT_BUFFER_SIZE,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_OCCUPY_ALL_CHANNELS,
      g_param_spec_boolean ("occupy-all-channels",
          "Occupy All Channles",
          "When enabled, ASIO device will allocate resources for all in/output "
          "channles",
          DEFAULT_OCCUPY_ALL_CHANNELS,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS)));

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_set_static_metadata (element_class, "AsioSink",
      "Source/Audio/Hardware",
      "Stream audio from an audio capture device through ASIO",
      "Seungha Yang <seungha@centricular.com>");

  basesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_asio_sink_get_caps);

  audiobasesink_class->create_ringbuffer =
      GST_DEBUG_FUNCPTR (gst_asio_sink_create_ringbuffer);

  GST_DEBUG_CATEGORY_INIT (gst_asio_sink_debug, "asiosink", 0, "asiosink");
}

static void
gst_asio_sink_init (GstAsioSink * self)
{
  self->buffer_size = DEFAULT_BUFFER_SIZE;
  self->occupy_all_channels = DEFAULT_OCCUPY_ALL_CHANNELS;
}

static void
gst_asio_sink_finalize (GObject * object)
{
  GstAsioSink *self = GST_ASIO_SINK (object);

  g_free (self->device_clsid);
  g_free (self->output_channels);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_asio_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAsioSink *self = GST_ASIO_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE_CLSID:
      g_free (self->device_clsid);
      self->device_clsid = g_value_dup_string (value);
      break;
    case PROP_OUTPUT_CHANNELS:
      g_free (self->output_channels);
      self->output_channels = g_value_dup_string (value);
      break;
    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      break;
    case PROP_OCCUPY_ALL_CHANNELS:
      self->occupy_all_channels = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_asio_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAsioSink *self = GST_ASIO_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE_CLSID:
      g_value_set_string (value, self->device_clsid);
      break;
    case PROP_OUTPUT_CHANNELS:
      g_value_set_string (value, self->output_channels);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;
    case PROP_OCCUPY_ALL_CHANNELS:
      g_value_set_boolean (value, self->occupy_all_channels);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstCaps *
gst_asio_sink_get_caps (GstBaseSink * sink, GstCaps * filter)
{
  GstAudioBaseSink *asink = GST_AUDIO_BASE_SINK (sink);
  GstAsioSink *self = GST_ASIO_SINK (sink);
  GstCaps *caps = nullptr;

  if (asink->ringbuffer)
    caps =
        gst_asio_ring_buffer_get_caps (GST_ASIO_RING_BUFFER
        (asink->ringbuffer));

  if (!caps)
    caps = gst_pad_get_pad_template_caps (GST_BASE_SINK_PAD (sink));

  if (filter) {
    GstCaps *filtered =
        gst_caps_intersect_full (filter, caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (caps);
    caps = filtered;
  }

  GST_DEBUG_OBJECT (self, "returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static GstAudioRingBuffer *
gst_asio_sink_create_ringbuffer (GstAudioBaseSink * sink)
{
  GstAsioSink *self = GST_ASIO_SINK (sink);
  GstAsioRingBuffer *ringbuffer = nullptr;
  HRESULT hr;
  CLSID clsid = GUID_NULL;
  GList *device_infos = nullptr;
  GstAsioDeviceInfo *info = nullptr;
  GstAsioObject *asio_object = nullptr;
  glong max_input_ch = 0;
  glong max_output_ch = 0;
  std::set < guint > channel_list;
  std::vector < guint > channel_indices;
  guint i;
  gchar *ringbuffer_name;

  USES_CONVERSION;

  GST_DEBUG_OBJECT (self, "Create ringbuffer");

  if (gst_asio_enum (&device_infos) == 0) {
    GST_WARNING_OBJECT (self, "No available ASIO devices");
    return nullptr;
  }

  if (self->device_clsid) {
    hr = CLSIDFromString (A2COLE (self->device_clsid), &clsid);
    if (FAILED (hr)) {
      GST_WARNING_OBJECT (self, "Failed to convert %s to CLSID",
          self->device_clsid);
      clsid = GUID_NULL;
    }
  }

  /* Pick the first device */
  if (clsid == GUID_NULL) {
    info = (GstAsioDeviceInfo *) device_infos->data;
  } else {
    /* Find matching device */
    GList *iter;
    for (iter = device_infos; iter; iter = g_list_next (iter)) {
      GstAsioDeviceInfo *tmp = (GstAsioDeviceInfo *) iter->data;
      if (tmp->clsid == clsid) {
        info = tmp;
        break;
      }
    }
  }

  if (!info) {
    GST_WARNING_OBJECT (self, "Failed to find matching device");
    goto out;
  }

  asio_object = gst_asio_object_new (info, self->occupy_all_channels);
  if (!asio_object) {
    GST_WARNING_OBJECT (self, "Failed to create ASIO object");
    goto out;
  }

  /* Configure channels to use */
  if (!gst_asio_object_get_max_num_channels (asio_object, &max_input_ch,
          &max_output_ch) || max_output_ch <= 0) {
    GST_WARNING_OBJECT (self, "No available output channels");
    goto out;
  }

  /* Check if user requested specific channel(s) */
  if (self->output_channels) {
    gchar **ch;

    ch = g_strsplit (self->output_channels, ",", 0);

    auto num_channels = g_strv_length (ch);
    if (num_channels > max_output_ch) {
      GST_WARNING_OBJECT (self, "To many channels %d were requested",
          num_channels);
    } else {
      for (i = 0; i < num_channels; i++) {
        guint64 c = g_ascii_strtoull (ch[i], nullptr, 0);
        if (c >= (guint64) max_output_ch) {
          GST_WARNING_OBJECT (self, "Invalid channel index");
          channel_list.clear ();
          break;
        }

        channel_list.insert ((guint) c);
      }
    }

    g_strfreev (ch);
  }

  if (channel_list.size () == 0) {
    for (i = 0; i < max_output_ch; i++)
      channel_indices.push_back (i);
  } else {
    for (auto iter : channel_list)
      channel_indices.push_back (iter);
  }

  ringbuffer_name = g_strdup_printf ("%s-asioringbuffer",
      GST_OBJECT_NAME (sink));

  ringbuffer =
      (GstAsioRingBuffer *) gst_asio_ring_buffer_new (asio_object,
      GST_ASIO_DEVICE_CLASS_RENDER, ringbuffer_name);
  g_free (ringbuffer_name);

  if (!ringbuffer) {
    GST_WARNING_OBJECT (self, "Couldn't create ringbuffer object");
    goto out;
  }

  if (!gst_asio_ring_buffer_configure (ringbuffer, channel_indices.data (),
          channel_indices.size (), self->buffer_size)) {
    GST_WARNING_OBJECT (self, "Failed to configure ringbuffer");
    gst_clear_object (&ringbuffer);
    goto out;
  }

out:
  if (device_infos)
    g_list_free_full (device_infos, (GDestroyNotify) gst_asio_device_info_free);

  gst_clear_object (&asio_object);

  return GST_AUDIO_RING_BUFFER_CAST (ringbuffer);
}
