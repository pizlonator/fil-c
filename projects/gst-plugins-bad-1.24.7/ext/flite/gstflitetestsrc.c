/* GStreamer
 * Copyright (C) 2010 David Schleef <ds@schleef.org>
 * Copyright (C) 2005 Stefan Kost <ensonic@users.sf.net>
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

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstadapter.h>
#include <gst/audio/audio.h>

#include <flite/flite.h>

#define GST_TYPE_FLITE_TEST_SRC \
  (gst_flite_test_src_get_type())
#define GST_FLITE_TEST_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FLITE_TEST_SRC,GstFliteTestSrc))
#define GST_FLITE_TEST_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FLITE_TEST_SRC,GstFliteTestSrcClass))
#define GST_IS_FLITE_TEST_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FLITE_TEST_SRC))
#define GST_IS_FLITE_TEST_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FLITE_TEST_SRC))

typedef struct _GstFliteTestSrc GstFliteTestSrc;
typedef struct _GstFliteTestSrcClass GstFliteTestSrcClass;

struct _GstFliteTestSrc
{
  GstBaseSrc parent;

  GstAdapter *adapter;

  GstAudioInfo info;

  int samples_per_buffer;

  int channel;

  cst_voice *voice;
};

struct _GstFliteTestSrcClass
{
  GstBaseSrcClass parent_class;
};

GType gst_flite_test_src_get_type (void);



GST_DEBUG_CATEGORY_STATIC (flite_test_src_debug);
#define GST_CAT_DEFAULT flite_test_src_debug

#define DEFAULT_SAMPLES_PER_BUFFER 1024

enum
{
  PROP_0,
  PROP_SAMPLES_PER_BUFFER,
  PROP_LAST
};


static GstStaticPadTemplate gst_flite_test_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) 48000, " "channels = (int) [1, 8]")
    );

GST_ELEMENT_REGISTER_DECLARE (flitetestsrc);
#define gst_flite_test_src_parent_class parent_class
G_DEFINE_TYPE (GstFliteTestSrc, gst_flite_test_src, GST_TYPE_BASE_SRC);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (flitetestsrc, "flitetestsrc",
    GST_RANK_NONE, gst_flite_test_src_get_type (), flite_init ());

static void gst_flite_test_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_flite_test_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static gboolean gst_flite_test_src_start (GstBaseSrc * basesrc);
static gboolean gst_flite_test_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_flite_test_src_create (GstBaseSrc * basesrc,
    guint64 offset, guint length, GstBuffer ** buffer);
static gboolean
gst_flite_test_src_set_caps (GstBaseSrc * basesrc, GstCaps * caps);
static GstCaps *gst_flite_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps);

static void
gst_flite_test_src_class_init (GstFliteTestSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;

  gobject_class->set_property = gst_flite_test_src_set_property;
  gobject_class->get_property = gst_flite_test_src_get_property;

  g_object_class_install_property (gobject_class, PROP_SAMPLES_PER_BUFFER,
      g_param_spec_int ("samplesperbuffer", "Samples per buffer",
          "Number of samples in each outgoing buffer",
          1, G_MAXINT, DEFAULT_SAMPLES_PER_BUFFER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_flite_test_src_src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "Flite speech test source", "Source/Audio",
      "Creates audio test signals identifying channels",
      "David Schleef <ds@schleef.org>");

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_flite_test_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_flite_test_src_stop);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_flite_test_src_create);
  gstbasesrc_class->set_caps = GST_DEBUG_FUNCPTR (gst_flite_test_src_set_caps);
  gstbasesrc_class->fixate = GST_DEBUG_FUNCPTR (gst_flite_test_src_fixate);

  GST_DEBUG_CATEGORY_INIT (flite_test_src_debug, "flitetestsrc", 0,
      "Flite Audio Test Source");
}

static void
gst_flite_test_src_init (GstFliteTestSrc * src)
{
  src->samples_per_buffer = DEFAULT_SAMPLES_PER_BUFFER;

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);

  gst_base_src_set_blocksize (GST_BASE_SRC (src), -1);
}

static gint
n_bits_set (guint64 x)
{
  gint i;
  gint c = 0;
  guint64 y = 1;

  for (i = 0; i < 64; i++) {
    if (x & y)
      c++;
    y <<= 1;
  }

  return c;
}

static GstCaps *
gst_flite_test_src_fixate (GstBaseSrc * bsrc, GstCaps * caps)
{
  GstStructure *structure;
  gint channels;

  caps = gst_caps_truncate (caps);
  caps = gst_caps_make_writable (caps);

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "channels", 2);
  gst_structure_get_int (structure, "channels", &channels);

  if (channels == 1) {
    gst_structure_remove_field (structure, "channel-mask");
  } else {
    guint64 channel_mask = 0;
    gint x = 63;

    if (!gst_structure_get (structure, "channel-mask", GST_TYPE_BITMASK,
            &channel_mask, NULL)) {
      switch (channels) {
        case 8:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_CENTER) |
              GST_AUDIO_CHANNEL_POSITION_MASK (LFE1) |
              GST_AUDIO_CHANNEL_POSITION_MASK (SIDE_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (SIDE_RIGHT);
          break;
        case 7:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_CENTER) |
              GST_AUDIO_CHANNEL_POSITION_MASK (LFE1) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_CENTER);
          break;
        case 6:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_CENTER) |
              GST_AUDIO_CHANNEL_POSITION_MASK (LFE1);
          break;
        case 5:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_CENTER);
          break;
        case 4:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (REAR_RIGHT);
          break;
        case 3:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (LFE1);
          break;
        case 2:
          channel_mask =
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_LEFT) |
              GST_AUDIO_CHANNEL_POSITION_MASK (FRONT_RIGHT);
          break;
        default:
          channel_mask = 0;
          break;
      }
    }

    while (n_bits_set (channel_mask) > channels) {
      channel_mask &= ~(G_GUINT64_CONSTANT (1) << x);
      x--;
    }

    gst_structure_set (structure, "channel-mask", GST_TYPE_BITMASK,
        channel_mask, NULL);
  }

  return GST_BASE_SRC_CLASS (parent_class)->fixate (bsrc, caps);
}

static gboolean
gst_flite_test_src_set_caps (GstBaseSrc * basesrc, GstCaps * caps)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (basesrc);

  gst_audio_info_init (&src->info);
  if (!gst_audio_info_from_caps (&src->info, caps)) {
    GST_ERROR_OBJECT (src, "Invalid caps");
    return FALSE;
  }

  return TRUE;
}

#if 0
static gboolean
gst_flite_test_src_query (GstBaseSrc * basesrc, GstQuery * query)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (basesrc);
  gboolean res = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (src_fmt == dest_fmt) {
        dest_val = src_val;
        goto done;
      }

      switch (src_fmt) {
        case GST_FORMAT_DEFAULT:
          switch (dest_fmt) {
            case GST_FORMAT_TIME:
              /* samples to time */
              dest_val =
                  gst_util_uint64_scale_int (src_val, GST_SECOND,
                  src->samplerate);
              break;
            default:
              goto error;
          }
          break;
        case GST_FORMAT_TIME:
          switch (dest_fmt) {
            case GST_FORMAT_DEFAULT:
              /* time to samples */
              dest_val =
                  gst_util_uint64_scale_int (src_val, src->samplerate,
                  GST_SECOND);
              break;
            default:
              goto error;
          }
          break;
        default:
          goto error;
      }
    done:
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (basesrc, query);
      break;
  }

  return res;
  /* ERROR */
error:
  {
    GST_DEBUG_OBJECT (src, "query failed");
    return FALSE;
  }
}
#endif


#if 0
static void
gst_flite_test_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}
#endif

/* there is no header for libflite_cmu_us_kal */
cst_voice *register_cmu_us_kal ();

static gboolean
gst_flite_test_src_start (GstBaseSrc * basesrc)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (basesrc);

  src->adapter = gst_adapter_new ();

  src->voice = register_cmu_us_kal ();

  return TRUE;
}

static gboolean
gst_flite_test_src_stop (GstBaseSrc * basesrc)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (basesrc);

  g_object_unref (src->adapter);

  return TRUE;
}

static char *
get_channel_name (GstFliteTestSrc * src, int channel)
{
  static const char *numbers[10] = {
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine"
  };
  static const char *names[64] = {
    "front left", "front right", "front center", "lfe 1", "rear left",
    "rear right", "front left of center", "front right of center",
    "rear center", "lfe 2", "side left", "side right", "top front left",
    "top front right", "top front center", "top center", "top rear left",
    "top rear right", "top side left", "top side right", "top rear center",
    "bottom front center", "bottom front left", "bottom front right",
    "wide left", "wide right", "surround left", "surround right"
  };
  const char *name;

  if (src->info.position[channel] == GST_AUDIO_CHANNEL_POSITION_INVALID) {
    name = "invalid";
  } else if (src->info.position[channel] == GST_AUDIO_CHANNEL_POSITION_NONE) {
    name = "none";
  } else if (src->info.position[channel] == GST_AUDIO_CHANNEL_POSITION_MONO) {
    name = "mono";
  } else {
    name = names[src->info.position[channel]];
  }

  return g_strdup_printf ("%s, %s", numbers[channel], name);
}

static GstFlowReturn
gst_flite_test_src_create (GstBaseSrc * basesrc, guint64 offset,
    guint length, GstBuffer ** buffer)
{
  GstFliteTestSrc *src;
  int n_bytes;

  src = GST_FLITE_TEST_SRC (basesrc);

  n_bytes = src->info.channels * sizeof (gint16) * src->samples_per_buffer;

  while (gst_adapter_available (src->adapter) < n_bytes) {
    GstBuffer *buf;
    char *text;
    int i;
    GstMapInfo map;
    gint16 *data;
    cst_wave *wave;
    gsize size;

    text = get_channel_name (src, src->channel);

    wave = flite_text_to_wave (text, src->voice);
    g_free (text);
    cst_wave_resample (wave, src->info.rate);

    GST_DEBUG ("type %s, sample_rate %d, num_samples %d, num_channels %d",
        wave->type, wave->sample_rate, wave->num_samples, wave->num_channels);

    size = src->info.channels * sizeof (gint16) * wave->num_samples;
    buf = gst_buffer_new_and_alloc (size);

    gst_buffer_map (buf, &map, GST_MAP_WRITE);
    data = (gint16 *) map.data;
    memset (data, 0, size);
    for (i = 0; i < wave->num_samples; i++) {
      data[i * src->info.channels + src->channel] = wave->samples[i];
    }
    gst_buffer_unmap (buf, &map);

    src->channel++;
    if (src->channel == src->info.channels) {
      src->channel = 0;
    }

    gst_adapter_push (src->adapter, buf);
  }

  *buffer = gst_adapter_take_buffer (src->adapter, n_bytes);

  return GST_FLOW_OK;
}

static void
gst_flite_test_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      src->samples_per_buffer = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_flite_test_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstFliteTestSrc *src = GST_FLITE_TEST_SRC (object);

  switch (prop_id) {
    case PROP_SAMPLES_PER_BUFFER:
      g_value_set_int (value, src->samples_per_buffer);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
