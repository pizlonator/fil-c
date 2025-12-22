/* GStreamer
 * Copyright (C) <2017> Carlos Rafael Giani <dv at pseudoterminal dot org>
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
 * SECTION:element-openmptdec
 * @see_also: #GstOpenMptDec
 *
 * openmpdec decodes module music formats, such as S3M, MOD, XM, IT.
 * It uses the [OpenMPT library](https://lib.openmpt.org) for this purpose.
 * It can be autoplugged and therefore works with decodebin.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 filesrc location=media/example.it ! openmptdec ! audioconvert ! audioresample ! autoaudiosink
 * ]|
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "gstopenmptdec.h"

#ifndef OPENMPT_API_VERSION_AT_LEAST
#define OPENMPT_API_VERSION_AT_LEAST(x, y, z) (FALSE)
#endif

GST_DEBUG_CATEGORY_STATIC (openmptdec_debug);
#define GST_CAT_DEFAULT openmptdec_debug


enum
{
  PROP_0,
  PROP_MASTER_GAIN,
  PROP_STEREO_SEPARATION,
  PROP_FILTER_LENGTH,
  PROP_VOLUME_RAMPING,
  PROP_OUTPUT_BUFFER_SIZE
};


#define DEFAULT_MASTER_GAIN 0
#define DEFAULT_STEREO_SEPARATION 100
#define DEFAULT_FILTER_LENGTH 0
#define DEFAULT_VOLUME_RAMPING -1
#define DEFAULT_OUTPUT_BUFFER_SIZE 1024

#define DEFAULT_SAMPLE_FORMAT GST_AUDIO_FORMAT_F32
#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_NUM_CHANNELS 2



static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-mod, "
        "type = (string) { 669, asylum-amf, dsmi-amf, extreme-ams, velvet-ams, "
        "dbm, digi, dmf, dsm, far, gdm, imf, it, j2b, mdl, med, mod, mt2, mtm, "
        "okt, psm, ptm, s3m, stm, ult, xm }")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) { " GST_AUDIO_NE (S16) ", " GST_AUDIO_NE (F32) " }, "
        "layout = (string) interleaved, "
        "rate = (int) [ 1, 192000 ], " "channels = (int) { 1, 2, 4 } ")
    );



G_DEFINE_TYPE (GstOpenMptDec, gst_openmpt_dec,
    GST_TYPE_NONSTREAM_AUDIO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (openmptdec, "openmptdec", GST_RANK_PRIMARY + 2,
    gst_openmpt_dec_get_type ());


static void gst_openmpt_dec_finalize (GObject * object);

static void gst_openmpt_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_openmpt_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_openmpt_dec_seek (GstNonstreamAudioDecoder * dec,
    GstClockTime * new_position);
static GstClockTime gst_openmpt_dec_tell (GstNonstreamAudioDecoder * dec);

static void gst_openmpt_dec_log_func (char const *message, void *user);
static void gst_openmpt_dec_add_metadata_to_tag_list (GstOpenMptDec *
    openmpt_dec, GstTagList * tags, char const *key, gchar const *tag);
static gboolean gst_openmpt_dec_load_from_buffer (GstNonstreamAudioDecoder *
    dec, GstBuffer * source_data, guint initial_subsong,
    GstNonstreamAudioSubsongMode initial_subsong_mode,
    GstClockTime * initial_position,
    GstNonstreamAudioOutputMode * initial_output_mode,
    gint * initial_num_loops);

static GstTagList *gst_openmpt_dec_get_main_tags (GstNonstreamAudioDecoder *
    dec);

static gboolean gst_openmpt_dec_set_current_subsong (GstNonstreamAudioDecoder *
    dec, guint subsong, GstClockTime * initial_position);
static guint gst_openmpt_dec_get_current_subsong (GstNonstreamAudioDecoder *
    dec);

static guint gst_openmpt_dec_get_num_subsongs (GstNonstreamAudioDecoder * dec);
static GstClockTime
gst_openmpt_dec_get_subsong_duration (GstNonstreamAudioDecoder * dec,
    guint subsong);
static GstTagList *gst_openmpt_dec_get_subsong_tags (GstNonstreamAudioDecoder *
    dec, guint subsong);
static gboolean gst_openmpt_dec_set_subsong_mode (GstNonstreamAudioDecoder *
    dec, GstNonstreamAudioSubsongMode mode, GstClockTime * initial_position);

static gboolean gst_openmpt_dec_set_num_loops (GstNonstreamAudioDecoder * dec,
    gint num_loops);
static gint gst_openmpt_dec_get_num_loops (GstNonstreamAudioDecoder * dec);

static guint
gst_openmpt_dec_get_supported_output_modes (GstNonstreamAudioDecoder * dec);
static gboolean gst_openmpt_dec_decode (GstNonstreamAudioDecoder * dec,
    GstBuffer ** buffer, guint * num_samples);

static gboolean gst_openmpt_dec_select_subsong (GstOpenMptDec *
    openmpt_dec, GstNonstreamAudioSubsongMode subsong_mode,
    gint openmpt_subsong);


void
gst_openmpt_dec_class_init (GstOpenMptDecClass * klass)
{
  GObjectClass *object_class;
  GstElementClass *element_class;
  GstNonstreamAudioDecoderClass *dec_class;

  GST_DEBUG_CATEGORY_INIT (openmptdec_debug, "openmptdec", 0,
      "OpenMPT-based module music decoder");

  object_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  dec_class = GST_NONSTREAM_AUDIO_DECODER_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_openmpt_dec_finalize);
  object_class->set_property = GST_DEBUG_FUNCPTR (gst_openmpt_dec_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_property);

  dec_class->seek = GST_DEBUG_FUNCPTR (gst_openmpt_dec_seek);
  dec_class->tell = GST_DEBUG_FUNCPTR (gst_openmpt_dec_tell);
  dec_class->load_from_buffer =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_load_from_buffer);
  dec_class->get_main_tags = GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_main_tags);
  dec_class->set_num_loops = GST_DEBUG_FUNCPTR (gst_openmpt_dec_set_num_loops);
  dec_class->get_num_loops = GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_num_loops);
  dec_class->get_supported_output_modes =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_supported_output_modes);
  dec_class->decode = GST_DEBUG_FUNCPTR (gst_openmpt_dec_decode);
  dec_class->set_current_subsong =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_set_current_subsong);
  dec_class->get_current_subsong =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_current_subsong);
  dec_class->get_num_subsongs =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_num_subsongs);
  dec_class->get_subsong_duration =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_subsong_duration);
  dec_class->get_subsong_tags =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_get_subsong_tags);
  dec_class->set_subsong_mode =
      GST_DEBUG_FUNCPTR (gst_openmpt_dec_set_subsong_mode);

  gst_element_class_set_static_metadata (element_class,
      "OpenMPT-based module music decoder",
      "Codec/Decoder/Audio",
      "Decoders module files (MOD/S3M/XM/IT/MTM/...) using OpenMPT",
      "Carlos Rafael Giani <dv@pseudoterminal.org>");

  g_object_class_install_property (object_class,
      PROP_MASTER_GAIN,
      g_param_spec_int ("master-gain",
          "Master gain",
          "Gain to apply to the playback, in millibel",
          -G_MAXINT, G_MAXINT,
          DEFAULT_MASTER_GAIN, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_STEREO_SEPARATION,
      g_param_spec_int ("stereo-separation",
          "Stereo separation",
          "Degree of separation for stereo channels, in percent",
          0, 400,
          DEFAULT_STEREO_SEPARATION, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_FILTER_LENGTH,
      g_param_spec_int ("filter-length",
          "Filter length",
          "Length of interpolation filter to use for the samples (0 = internal default)",
          0, 8,
          DEFAULT_FILTER_LENGTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  g_object_class_install_property (object_class,
      PROP_VOLUME_RAMPING,
      g_param_spec_int ("volume-ramping",
          "Volume ramping",
          "Volume ramping strength; higher value -> slower ramping (-1 = internal default)",
          -1, 10,
          DEFAULT_VOLUME_RAMPING, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
  /* 4*4 => quad output with F32 samples; this ensures that no overflow can happen */
  g_object_class_install_property (object_class,
      PROP_OUTPUT_BUFFER_SIZE,
      g_param_spec_uint ("output-buffer-size",
          "Output buffer size",
          "Size of each output buffer, in samples (actual size can be smaller "
          "than this during flush or EOS)",
          1, G_MAXUINT / (4 * 4),
          DEFAULT_OUTPUT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
      );
}


void
gst_openmpt_dec_init (GstOpenMptDec * openmpt_dec)
{
  openmpt_dec->mod = NULL;

  openmpt_dec->cur_subsong = 0;
  openmpt_dec->num_subsongs = 0;
  openmpt_dec->subsong_durations = NULL;

  openmpt_dec->num_loops = 0;

  openmpt_dec->master_gain = DEFAULT_MASTER_GAIN;
  openmpt_dec->stereo_separation = DEFAULT_STEREO_SEPARATION;
  openmpt_dec->filter_length = DEFAULT_FILTER_LENGTH;
  openmpt_dec->volume_ramping = DEFAULT_VOLUME_RAMPING;

  openmpt_dec->output_buffer_size = DEFAULT_OUTPUT_BUFFER_SIZE;

  openmpt_dec->main_tags = NULL;

  openmpt_dec->sample_format = DEFAULT_SAMPLE_FORMAT;
  openmpt_dec->sample_rate = DEFAULT_SAMPLE_RATE;
  openmpt_dec->num_channels = DEFAULT_NUM_CHANNELS;
}


static void
gst_openmpt_dec_finalize (GObject * object)
{
  GstOpenMptDec *openmpt_dec;

  g_return_if_fail (GST_IS_OPENMPT_DEC (object));
  openmpt_dec = GST_OPENMPT_DEC (object);

  if (openmpt_dec->main_tags != NULL)
    gst_tag_list_unref (openmpt_dec->main_tags);

  if (openmpt_dec->mod != NULL)
    openmpt_module_destroy (openmpt_dec->mod);

  g_free (openmpt_dec->subsong_durations);

  G_OBJECT_CLASS (gst_openmpt_dec_parent_class)->finalize (object);
}


static void
gst_openmpt_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNonstreamAudioDecoder *dec;
  GstOpenMptDec *openmpt_dec;

  dec = GST_NONSTREAM_AUDIO_DECODER (object);
  openmpt_dec = GST_OPENMPT_DEC (object);

  switch (prop_id) {
    case PROP_MASTER_GAIN:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      openmpt_dec->master_gain = g_value_get_int (value);
      if (openmpt_dec->mod != NULL)
        openmpt_module_set_render_param (openmpt_dec->mod,
            OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL,
            openmpt_dec->master_gain);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_STEREO_SEPARATION:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      openmpt_dec->stereo_separation = g_value_get_int (value);
      if (openmpt_dec->mod != NULL)
        openmpt_module_set_render_param (openmpt_dec->mod,
            OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT,
            openmpt_dec->stereo_separation);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_FILTER_LENGTH:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      openmpt_dec->filter_length = g_value_get_int (value);
      if (openmpt_dec->mod != NULL)
        openmpt_module_set_render_param (openmpt_dec->mod,
            OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
            openmpt_dec->filter_length);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_VOLUME_RAMPING:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      openmpt_dec->volume_ramping = g_value_get_int (value);
      if (openmpt_dec->mod != NULL)
        openmpt_module_set_render_param (openmpt_dec->mod,
            OPENMPT_MODULE_RENDER_VOLUMERAMPING_STRENGTH,
            openmpt_dec->volume_ramping);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    case PROP_OUTPUT_BUFFER_SIZE:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      openmpt_dec->output_buffer_size = g_value_get_uint (value);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_openmpt_dec_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (object);

  switch (prop_id) {
    case PROP_MASTER_GAIN:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_int (value, openmpt_dec->master_gain);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;
    }

    case PROP_STEREO_SEPARATION:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_int (value, openmpt_dec->stereo_separation);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;
    }

    case PROP_FILTER_LENGTH:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_int (value, openmpt_dec->filter_length);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;
    }

    case PROP_VOLUME_RAMPING:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_int (value, openmpt_dec->volume_ramping);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;
    }

    case PROP_OUTPUT_BUFFER_SIZE:
    {
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (object);
      g_value_set_uint (value, openmpt_dec->output_buffer_size);
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (object);
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static gboolean
gst_openmpt_dec_seek (GstNonstreamAudioDecoder * dec,
    GstClockTime * new_position)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  g_return_val_if_fail (openmpt_dec->mod != NULL, FALSE);

  openmpt_module_set_position_seconds (openmpt_dec->mod,
      (double) (*new_position) / GST_SECOND);
  *new_position = gst_openmpt_dec_tell (dec);

  return TRUE;
}


static GstClockTime
gst_openmpt_dec_tell (GstNonstreamAudioDecoder * dec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  g_return_val_if_fail (openmpt_dec->mod != NULL, GST_CLOCK_TIME_NONE);

  return (GstClockTime) (openmpt_module_get_position_seconds (openmpt_dec->mod)
      * GST_SECOND);
}


static void
gst_openmpt_dec_log_func (char const *message, void *user)
{
  GST_LOG_OBJECT (GST_OBJECT (user), "%s", message);
}


static void
gst_openmpt_dec_add_metadata_to_tag_list (GstOpenMptDec * openmpt_dec,
    GstTagList * tags, char const *key, gchar const *tag)
{
  char const *metadata = openmpt_module_get_metadata (openmpt_dec->mod, key);

  if (metadata && *metadata) {
    GST_DEBUG_OBJECT (openmpt_dec,
        "adding metadata \"%s\" with key \"%s\" to tag list as tag \"%s\"",
        metadata, key, tag);

    if (g_strcmp0 (tag, GST_TAG_DATE_TIME) == 0) {
      /* Special handling for date-time tags - interpret the
       * metadata string as an iso8601 string and convert it
       * to a GstDateTime value, since this is the data type
       * that GST_TAG_DATE_TIME expects. */

      GstDateTime *date_time = gst_date_time_new_from_iso8601_string (metadata);
      if (date_time) {
        GST_DEBUG_OBJECT (openmpt_dec,
            "successfully created date-time object out of iso8601 string");
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, tag, date_time, NULL);
        gst_date_time_unref (date_time);
      } else
        GST_WARNING_OBJECT (openmpt_dec,
            "could not create date-time object out of iso8601 string - not adding metadata to tags");
    } else {
      /* Default handling - just insert the metadata string as-is */
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, tag, metadata, NULL);
    }
  } else
    GST_DEBUG_OBJECT (openmpt_dec,
        "attempted to add metadata with key \"%s\" to tag list as tag \"%s\", but none exists",
        key, tag);

  if (metadata)
    openmpt_free_string (metadata);
}


static gboolean
gst_openmpt_dec_load_from_buffer (GstNonstreamAudioDecoder * dec,
    GstBuffer * source_data, guint initial_subsong,
    GstNonstreamAudioSubsongMode initial_subsong_mode,
    GstClockTime * initial_position,
    GstNonstreamAudioOutputMode * initial_output_mode, gint * initial_num_loops)
{
  GstMapInfo map;
  GstOpenMptDec *openmpt_dec;

  openmpt_dec = GST_OPENMPT_DEC (dec);

  /* First, determine the sample rate, channel count, and sample format to use */
  openmpt_dec->sample_format = DEFAULT_SAMPLE_FORMAT;
  openmpt_dec->sample_rate = DEFAULT_SAMPLE_RATE;
  openmpt_dec->num_channels = DEFAULT_NUM_CHANNELS;
  gst_nonstream_audio_decoder_get_downstream_info (dec,
      &(openmpt_dec->sample_format), &(openmpt_dec->sample_rate),
      &(openmpt_dec->num_channels));

  /* Set output format */
  if (!gst_nonstream_audio_decoder_set_output_format_simple (dec,
          openmpt_dec->sample_rate,
          openmpt_dec->sample_format, openmpt_dec->num_channels))
    return FALSE;

  /* Pass the module data to OpenMPT for loading */
  gst_buffer_map (source_data, &map, GST_MAP_READ);
#if OPENMPT_API_VERSION_AT_LEAST(0,3,0)
  openmpt_dec->mod =
      openmpt_module_create_from_memory2 (map.data, map.size,
      gst_openmpt_dec_log_func, dec, NULL, NULL, NULL, NULL, NULL);
#else
  openmpt_dec->mod =
      openmpt_module_create_from_memory (map.data, map.size,
      gst_openmpt_dec_log_func, dec, NULL);
#endif
  gst_buffer_unmap (source_data, &map);

  if (openmpt_dec->mod == NULL) {
    GST_ERROR_OBJECT (dec, "loading module failed");
    return FALSE;
  }

  /* Copy subsong states */
  openmpt_dec->cur_subsong = initial_subsong;
  openmpt_dec->cur_subsong_mode = initial_subsong_mode;

  /* Query the number of subsongs available for logging and for checking
   * the initial subsong index */
  openmpt_dec->num_subsongs =
      openmpt_module_get_num_subsongs (openmpt_dec->mod);
  if (G_UNLIKELY (initial_subsong >= openmpt_dec->num_subsongs)) {
    GST_WARNING_OBJECT (openmpt_dec,
        "initial subsong %u out of bounds (there are %u subsongs) - setting it to 0",
        initial_subsong, openmpt_dec->num_subsongs);
    initial_subsong = 0;
  }
  GST_INFO_OBJECT (openmpt_dec, "%d subsong(s) available",
      openmpt_dec->num_subsongs);

  /* Query the OpenMPT default subsong (can be -1)
   * The default subsong is the one that is initially selected, so we
   * need to query it here, *before* any openmpt_module_select_subsong()
   * calls are done */
  {

#if OPENMPT_API_VERSION_AT_LEAST(0,5,0)
    gchar const *subsong_cstr =
        openmpt_module_ctl_get_text (openmpt_dec->mod, "subsong");
#else
    gchar const *subsong_cstr =
        openmpt_module_ctl_get (openmpt_dec->mod, "subsong");
#endif
    gchar *endptr;

    if (subsong_cstr != NULL) {
      openmpt_dec->default_openmpt_subsong =
          g_ascii_strtoll (subsong_cstr, &endptr, 10);
      if (subsong_cstr == endptr) {
        GST_WARNING_OBJECT (openmpt_dec,
            "could not convert ctl string \"%s\" to subsong index - using default OpenMPT index -1 instead",
            subsong_cstr);
        openmpt_dec->default_openmpt_subsong = -1;
      } else
        GST_DEBUG_OBJECT (openmpt_dec, "default OpenMPT subsong index is %d",
            openmpt_dec->default_openmpt_subsong);

      openmpt_free_string (subsong_cstr);
    } else {
      GST_INFO_OBJECT (openmpt_dec,
          "could not get subsong ctl string - using default OpenMPT index -1 instead");
      openmpt_dec->default_openmpt_subsong = -1;
    }
  }

  /* Seek to initial position */
  if (*initial_position != 0) {
    openmpt_module_set_position_seconds (openmpt_dec->mod,
        (double) (*initial_position) / GST_SECOND);
    *initial_position =
        (GstClockTime) (openmpt_module_get_position_seconds (openmpt_dec->mod) *
        GST_SECOND);
  }

  /* LOOPING output mode is not supported */
  *initial_output_mode = GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY;

  /* Query the durations of each subsong (if any exist) */
  if (openmpt_dec->num_subsongs > 0) {
    guint i;

    openmpt_dec->subsong_durations =
        g_try_malloc (openmpt_dec->num_subsongs * sizeof (double));
    if (openmpt_dec->subsong_durations == NULL) {
      GST_NONSTREAM_AUDIO_DECODER_UNLOCK_MUTEX (dec);
      GST_ELEMENT_ERROR (openmpt_dec, RESOURCE, NO_SPACE_LEFT,
          ("could not allocate memory for subsong duration array"), (NULL));
      GST_NONSTREAM_AUDIO_DECODER_LOCK_MUTEX (dec);
      return FALSE;
    }

    for (i = 0; i < openmpt_dec->num_subsongs; ++i) {
      openmpt_module_select_subsong (openmpt_dec->mod, i);
      openmpt_dec->subsong_durations[i] =
          openmpt_module_get_duration_seconds (openmpt_dec->mod);
    }
  }

  /* Select the initial subsong */
  gst_openmpt_dec_select_subsong (openmpt_dec, initial_subsong_mode,
      initial_subsong);

  /* Set the number of loops, and query the actual number
   * that was chosen by OpenMPT */
  {
    int32_t actual_repeat_count;
    openmpt_module_set_repeat_count (openmpt_dec->mod, *initial_num_loops);
    actual_repeat_count = openmpt_module_get_repeat_count (openmpt_dec->mod);

    if (actual_repeat_count != *initial_num_loops) {
      GST_DEBUG_OBJECT (openmpt_dec,
          "requested num-loops value %d differs from actual value %d",
          *initial_num_loops, actual_repeat_count);
      *initial_num_loops = actual_repeat_count;
    }
  }

  /* Set render parameters (adjustable via properties) */
  openmpt_module_set_render_param (openmpt_dec->mod,
      OPENMPT_MODULE_RENDER_MASTERGAIN_MILLIBEL, openmpt_dec->master_gain);
  openmpt_module_set_render_param (openmpt_dec->mod,
      OPENMPT_MODULE_RENDER_STEREOSEPARATION_PERCENT,
      openmpt_dec->stereo_separation);
  openmpt_module_set_render_param (openmpt_dec->mod,
      OPENMPT_MODULE_RENDER_INTERPOLATIONFILTER_LENGTH,
      openmpt_dec->filter_length);
  openmpt_module_set_render_param (openmpt_dec->mod,
      OPENMPT_MODULE_RENDER_VOLUMERAMPING_STRENGTH,
      openmpt_dec->volume_ramping);

  /* Log the available metadata keys, and produce a
   * tag list if any keys are available */
  {
    char const *metadata_keys =
        openmpt_module_get_metadata_keys (openmpt_dec->mod);
    if (metadata_keys != NULL) {
      GstTagList *tags = gst_tag_list_new_empty ();

      GST_DEBUG_OBJECT (dec, "metadata keys: [%s]", metadata_keys);
      openmpt_free_string (metadata_keys);

      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "title",
          GST_TAG_TITLE);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "artist",
          GST_TAG_ARTIST);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "message",
          GST_TAG_COMMENT);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "tracker",
          GST_TAG_APPLICATION_NAME);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "type_long",
          GST_TAG_CODEC);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags, "date",
          GST_TAG_DATE_TIME);
      gst_openmpt_dec_add_metadata_to_tag_list (openmpt_dec, tags,
          "container_long", GST_TAG_CONTAINER_FORMAT);

      openmpt_dec->main_tags = tags;
    } else {
      GST_DEBUG_OBJECT (dec,
          "no metadata keys found - not producing a tag list");
    }
  }

  /* Log any warnings that were produced by OpenMPT while loading */
  {
    char const *warnings =
        openmpt_module_get_metadata (openmpt_dec->mod, "warnings");
    if (warnings) {
      if (*warnings)
        GST_WARNING_OBJECT (openmpt_dec, "reported warnings during loading: %s",
            warnings);
      openmpt_free_string (warnings);
    }
  }

  return TRUE;
}


static GstTagList *
gst_openmpt_dec_get_main_tags (GstNonstreamAudioDecoder * dec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  return gst_tag_list_ref (openmpt_dec->main_tags);
}


static gboolean
gst_openmpt_dec_set_current_subsong (GstNonstreamAudioDecoder * dec,
    guint subsong, GstClockTime * initial_position)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  g_return_val_if_fail (openmpt_dec->mod != NULL, FALSE);

  if (gst_openmpt_dec_select_subsong (openmpt_dec,
          openmpt_dec->cur_subsong_mode, subsong)) {
    GST_DEBUG_OBJECT (openmpt_dec,
        "selected subsong %u and switching subsong mode to SINGLE", subsong);
    openmpt_dec->cur_subsong_mode = GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE;
    openmpt_dec->cur_subsong = subsong;
    *initial_position = 0;
    return TRUE;
  } else {
    GST_ERROR_OBJECT (openmpt_dec, "could not select subsong %u", subsong);
    return FALSE;
  }
}


static guint
gst_openmpt_dec_get_current_subsong (GstNonstreamAudioDecoder * dec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  return openmpt_dec->cur_subsong;
}


static guint
gst_openmpt_dec_get_num_subsongs (GstNonstreamAudioDecoder * dec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  return openmpt_dec->num_subsongs;
}


static GstClockTime
gst_openmpt_dec_get_subsong_duration (GstNonstreamAudioDecoder * dec,
    guint subsong)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  return (GstClockTime) (openmpt_dec->subsong_durations[subsong] * GST_SECOND);
}


static GstTagList *
gst_openmpt_dec_get_subsong_tags (GstNonstreamAudioDecoder * dec, guint subsong)
{
  GstOpenMptDec *openmpt_dec;
  char const *name;

  openmpt_dec = GST_OPENMPT_DEC (dec);

  name = openmpt_module_get_subsong_name (openmpt_dec->mod, subsong);
  if (name != NULL) {
    GstTagList *tags = NULL;

    if (*name) {
      tags = gst_tag_list_new_empty ();
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, "title", name, NULL);
    }

    openmpt_free_string (name);

    return tags;
  } else
    return NULL;
}


static gboolean
gst_openmpt_dec_set_subsong_mode (GstNonstreamAudioDecoder * dec,
    GstNonstreamAudioSubsongMode mode, GstClockTime * initial_position)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  g_return_val_if_fail (openmpt_dec->mod != NULL, FALSE);

  if (gst_openmpt_dec_select_subsong (openmpt_dec, mode,
          openmpt_dec->cur_subsong)) {
    GST_DEBUG_OBJECT (openmpt_dec, "set subsong mode");
    openmpt_dec->cur_subsong_mode = mode;
    *initial_position = 0;
    return TRUE;
  } else {
    GST_ERROR_OBJECT (openmpt_dec, "could not set subsong mode");
    return FALSE;
  }
}


static gboolean
gst_openmpt_dec_set_num_loops (GstNonstreamAudioDecoder * dec, gint num_loops)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  openmpt_dec->num_loops = num_loops;

  if (openmpt_dec->mod != NULL) {
    if (openmpt_module_set_repeat_count (openmpt_dec->mod, num_loops)) {
      GST_DEBUG_OBJECT (openmpt_dec, "successfully set repeat count %d",
          num_loops);
      return TRUE;
    } else {
      GST_ERROR_OBJECT (openmpt_dec, "could not set repeat count %d",
          num_loops);
      return FALSE;
    }
  } else
    return TRUE;
}


static gint
gst_openmpt_dec_get_num_loops (GstNonstreamAudioDecoder * dec)
{
  GstOpenMptDec *openmpt_dec = GST_OPENMPT_DEC (dec);
  return openmpt_dec->num_loops;
}


static guint
gst_openmpt_dec_get_supported_output_modes (G_GNUC_UNUSED
    GstNonstreamAudioDecoder * dec)
{
  return 1u << GST_NONSTREAM_AUDIO_OUTPUT_MODE_STEADY;
}


static gboolean
gst_openmpt_dec_decode (GstNonstreamAudioDecoder * dec, GstBuffer ** buffer,
    guint * num_samples)
{
  GstOpenMptDec *openmpt_dec;
  GstBuffer *outbuf;
  GstMapInfo map;
  size_t num_read_samples;
  gsize outbuf_size;
  GstAudioFormatInfo const *fmt_info;

  openmpt_dec = GST_OPENMPT_DEC (dec);

  fmt_info = gst_audio_format_get_info (openmpt_dec->sample_format);

  /* Allocate output buffer */
  outbuf_size =
      openmpt_dec->output_buffer_size * (fmt_info->width / 8) *
      openmpt_dec->num_channels;
  outbuf =
      gst_nonstream_audio_decoder_allocate_output_buffer (dec, outbuf_size);
  if (G_UNLIKELY (outbuf == NULL))
    return FALSE;

  /* Write samples into the output buffer */

  gst_buffer_map (outbuf, &map, GST_MAP_WRITE);

  switch (openmpt_dec->sample_format) {
    case GST_AUDIO_FORMAT_S16:
    {
      int16_t *out_samples = (int16_t *) (map.data);
      switch (openmpt_dec->num_channels) {
        case 1:
          num_read_samples =
              openmpt_module_read_mono (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        case 2:
          num_read_samples =
              openmpt_module_read_interleaved_stereo (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        case 4:
          num_read_samples =
              openmpt_module_read_interleaved_quad (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        default:
          g_assert_not_reached ();
      }
      break;
    }
    case GST_AUDIO_FORMAT_F32:
    {
      float *out_samples = (float *) (map.data);
      switch (openmpt_dec->num_channels) {
        case 1:
          num_read_samples =
              openmpt_module_read_float_mono (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        case 2:
          num_read_samples =
              openmpt_module_read_interleaved_float_stereo (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        case 4:
          num_read_samples =
              openmpt_module_read_interleaved_float_quad (openmpt_dec->mod,
              openmpt_dec->sample_rate, openmpt_dec->output_buffer_size,
              out_samples);
          break;
        default:
          g_assert_not_reached ();
      }
      break;
    }
    default:
    {
      GST_ERROR_OBJECT (dec, "using unsupported sample format %s",
          fmt_info->name);
      g_assert_not_reached ();
    }
  }

  gst_buffer_unmap (outbuf, &map);

  if (num_read_samples == 0)
    return FALSE;

  *buffer = outbuf;
  *num_samples = num_read_samples;

  return TRUE;
}


static gboolean
gst_openmpt_dec_select_subsong (GstOpenMptDec * openmpt_dec,
    GstNonstreamAudioSubsongMode subsong_mode, gint openmpt_subsong)
{
  switch (subsong_mode) {
    case GST_NONSTREAM_AUDIO_SUBSONG_MODE_SINGLE:
      GST_DEBUG_OBJECT (openmpt_dec, "setting subsong mode to SINGLE");
      return openmpt_module_select_subsong (openmpt_dec->mod, openmpt_subsong);

    case GST_NONSTREAM_AUDIO_SUBSONG_MODE_ALL:
      GST_DEBUG_OBJECT (openmpt_dec, "setting subsong mode to ALL");
      return openmpt_module_select_subsong (openmpt_dec->mod, -1);

    case GST_NONSTREAM_AUDIO_SUBSONG_MODE_DECODER_DEFAULT:
      /* NOTE: The OpenMPT documentation recommends to not bother
       * calling openmpt_module_select_subsong() if the decoder
       * default shall be used. However, the user might have switched
       * the subsong mode from SINGLE or ALL to DECODER_DEFAULT,
       * in which case we *do* have to set the default subsong index.
       * So, just set the default index here. */
      GST_DEBUG_OBJECT (openmpt_dec, "setting subsong mode to DECODER_DEFAULT");
      return openmpt_module_select_subsong (openmpt_dec->mod,
          openmpt_dec->default_openmpt_subsong);

    default:
      g_assert_not_reached ();
      return TRUE;
  }
}
