/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
 * SECTION:element-vorbisenc
 * @title: vorbisenc
 * @see_also: vorbisdec, oggmux
 *
 * This element encodes raw float audio into a Vorbis stream.
 * [Vorbis](http://www.vorbis.com/) is a royalty-free audio codec maintained by
 * the [Xiph.org Foundation](http://www.xiph.org/).
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc wave=sine num-buffers=100 ! audioconvert ! vorbisenc ! oggmux ! filesink location=sine.ogg
 * ]|
 *  Encode a test sine signal to Ogg/Vorbis.  Note that the resulting file
 * will be really small because a sine signal compresses very well.
 * |[
 * gst-launch-1.0 -v autoaudiosrc ! audioconvert ! vorbisenc ! oggmux ! filesink location=alsasrc.ogg
 * ]|
 *  Record from a sound card and encode to Ogg/Vorbis.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vorbis/vorbisenc.h>

#include <gst/gsttagsetter.h>
#include <gst/tag/tag.h>
#include <gst/audio/audio.h>
#include "gstvorbisenc.h"

#include "gstvorbiselements.h"
#include "gstvorbiscommon.h"

GST_DEBUG_CATEGORY_STATIC (vorbisenc_debug);
#define GST_CAT_DEFAULT vorbisenc_debug

static GstStaticPadTemplate vorbis_enc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-vorbis, "
        "rate = (int) [ 1, 200000 ], " "channels = (int) [ 1, 255 ]")
    );

enum
{
  ARG_0,
  ARG_MAX_BITRATE,
  ARG_BITRATE,
  ARG_MIN_BITRATE,
  ARG_QUALITY,
  ARG_MANAGED,
  ARG_LAST_MESSAGE
};

static GstFlowReturn gst_vorbis_enc_output_buffers (GstVorbisEnc * vorbisenc);
static GstCaps *gst_vorbis_enc_generate_sink_caps (void);


#define MAX_BITRATE_DEFAULT     -1
#define BITRATE_DEFAULT         -1
#define MIN_BITRATE_DEFAULT     -1
#define QUALITY_DEFAULT         0.3
#define LOWEST_BITRATE          6000    /* lowest allowed for a 8 kHz stream */
#define HIGHEST_BITRATE         250001  /* highest allowed for a 44 kHz stream */

static gboolean gst_vorbis_enc_start (GstAudioEncoder * enc);
static gboolean gst_vorbis_enc_stop (GstAudioEncoder * enc);
static gboolean gst_vorbis_enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_vorbis_enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);
static gboolean gst_vorbis_enc_sink_event (GstAudioEncoder * enc,
    GstEvent * event);

static gboolean gst_vorbis_enc_setup (GstVorbisEnc * vorbisenc);

static void gst_vorbis_enc_dispose (GObject * object);
static void gst_vorbis_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_vorbis_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_vorbis_enc_flush (GstAudioEncoder * vorbisenc);

#define gst_vorbis_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVorbisEnc, gst_vorbis_enc,
    GST_TYPE_AUDIO_ENCODER, G_IMPLEMENT_INTERFACE (GST_TYPE_TAG_SETTER, NULL));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vorbisenc, "vorbisenc",
    GST_RANK_PRIMARY, GST_TYPE_VORBISENC,
    GST_DEBUG_CATEGORY_INIT (vorbisenc_debug, "vorbisenc", 0,
        "vorbis encoding element");
    vorbis_element_init (plugin));

static void
gst_vorbis_enc_class_init (GstVorbisEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstAudioEncoderClass *base_class;
  GstCaps *sink_caps;
  GstPadTemplate *sink_templ;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  base_class = (GstAudioEncoderClass *) (klass);

  gobject_class->set_property = gst_vorbis_enc_set_property;
  gobject_class->get_property = gst_vorbis_enc_get_property;
  gobject_class->dispose = gst_vorbis_enc_dispose;

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MAX_BITRATE,
      g_param_spec_int ("max-bitrate", "Maximum Bitrate",
          "Specify a maximum bitrate (in bps). Useful for streaming "
          "applications. (-1 == disabled)",
          -1, HIGHEST_BITRATE, MAX_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_BITRATE,
      g_param_spec_int ("bitrate", "Target Bitrate",
          "Attempt to encode at a bitrate averaging this (in bps). "
          "This uses the bitrate management engine, and is not recommended for most users. "
          "Quality is a better alternative. (-1 == disabled)", -1,
          HIGHEST_BITRATE, BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MIN_BITRATE,
      g_param_spec_int ("min-bitrate", "Minimum Bitrate",
          "Specify a minimum bitrate (in bps). Useful for encoding for a "
          "fixed-size channel. (-1 == disabled)", -1, HIGHEST_BITRATE,
          MIN_BITRATE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_QUALITY,
      g_param_spec_float ("quality", "Quality",
          "Specify quality instead of specifying a particular bitrate.", -0.1,
          1.0, QUALITY_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_MANAGED,
      g_param_spec_boolean ("managed", "Managed",
          "Enable bitrate management engine", FALSE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_LAST_MESSAGE,
      g_param_spec_string ("last-message", "last-message",
          "The last status message", NULL,
          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  sink_caps = gst_vorbis_enc_generate_sink_caps ();
  sink_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
  gst_element_class_add_pad_template (gstelement_class, sink_templ);
  gst_caps_unref (sink_caps);

  gst_element_class_add_static_pad_template (gstelement_class,
      &vorbis_enc_src_factory);

  gst_element_class_set_static_metadata (gstelement_class,
      "Vorbis audio encoder", "Codec/Encoder/Audio",
      "Encodes audio in Vorbis format",
      "Monty <monty@xiph.org>, " "Wim Taymans <wim@fluendo.com>");

  base_class->start = GST_DEBUG_FUNCPTR (gst_vorbis_enc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_vorbis_enc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_vorbis_enc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_vorbis_enc_handle_frame);
  base_class->sink_event = GST_DEBUG_FUNCPTR (gst_vorbis_enc_sink_event);
  base_class->flush = GST_DEBUG_FUNCPTR (gst_vorbis_enc_flush);
}

static void
gst_vorbis_enc_init (GstVorbisEnc * vorbisenc)
{
  GstAudioEncoder *enc = GST_AUDIO_ENCODER (vorbisenc);

  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (enc));

  vorbisenc->channels = -1;
  vorbisenc->frequency = -1;

  vorbisenc->managed = FALSE;
  vorbisenc->max_bitrate = MAX_BITRATE_DEFAULT;
  vorbisenc->bitrate = BITRATE_DEFAULT;
  vorbisenc->min_bitrate = MIN_BITRATE_DEFAULT;
  vorbisenc->quality = QUALITY_DEFAULT;
  vorbisenc->quality_set = FALSE;
  vorbisenc->last_message = NULL;

  /* arrange granulepos marking (and required perfect ts) */
  gst_audio_encoder_set_mark_granule (enc, TRUE);
  gst_audio_encoder_set_perfect_timestamp (enc, TRUE);
}

static void
gst_vorbis_enc_dispose (GObject * object)
{
  GstVorbisEnc *vorbisenc = GST_VORBISENC (object);

  if (vorbisenc->sinkcaps) {
    gst_caps_unref (vorbisenc->sinkcaps);
    vorbisenc->sinkcaps = NULL;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static gboolean
gst_vorbis_enc_start (GstAudioEncoder * enc)
{
  GstVorbisEnc *vorbisenc = GST_VORBISENC (enc);

  GST_DEBUG_OBJECT (enc, "start");
  vorbisenc->tags = gst_tag_list_new_empty ();
  vorbisenc->header_sent = FALSE;
  vorbisenc->last_size = 0;

  return TRUE;
}

static gboolean
gst_vorbis_enc_stop (GstAudioEncoder * enc)
{
  GstVorbisEnc *vorbisenc = GST_VORBISENC (enc);

  GST_DEBUG_OBJECT (enc, "stop");
  vorbis_block_clear (&vorbisenc->vb);
  vorbis_dsp_clear (&vorbisenc->vd);
  vorbis_info_clear (&vorbisenc->vi);
  g_free (vorbisenc->last_message);
  vorbisenc->last_message = NULL;
  gst_tag_list_unref (vorbisenc->tags);
  vorbisenc->tags = NULL;

  gst_tag_setter_reset_tags (GST_TAG_SETTER (enc));

  return TRUE;
}

static GstCaps *
gst_vorbis_enc_generate_sink_caps (void)
{
  GstCaps *caps = gst_caps_new_empty ();
  int i, c;

  gst_caps_append_structure (caps, gst_structure_new ("audio/x-raw",
          "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
          "layout", G_TYPE_STRING, "interleaved",
          "rate", GST_TYPE_INT_RANGE, 1, 200000,
          "channels", G_TYPE_INT, 1, NULL));

  for (i = 2; i <= 8; i++) {
    GstStructure *structure;
    guint64 channel_mask = 0;
    const GstAudioChannelPosition *pos = gst_vorbis_channel_positions[i - 1];

    for (c = 0; c < i; c++) {
      channel_mask |= G_GUINT64_CONSTANT (1) << pos[c];
    }

    structure = gst_structure_new ("audio/x-raw",
        "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
        "layout", G_TYPE_STRING, "interleaved",
        "rate", GST_TYPE_INT_RANGE, 1, 200000, "channels", G_TYPE_INT, i,
        "channel-mask", GST_TYPE_BITMASK, channel_mask, NULL);

    gst_caps_append_structure (caps, structure);
  }

  gst_caps_append_structure (caps, gst_structure_new ("audio/x-raw",
          "format", G_TYPE_STRING, GST_AUDIO_NE (F32),
          "layout", G_TYPE_STRING, "interleaved",
          "rate", GST_TYPE_INT_RANGE, 1, 200000,
          "channels", GST_TYPE_INT_RANGE, 9, 255,
          "channel-mask", GST_TYPE_BITMASK, G_GUINT64_CONSTANT (0), NULL));

  return caps;
}

static gint64
gst_vorbis_enc_get_latency (GstVorbisEnc * vorbisenc)
{
  /* FIXME, this probably depends on the bitrate and other setting but for now
   * we return this value, which was obtained by totally unscientific
   * measurements */
  return 58 * GST_MSECOND;
}

static gboolean
gst_vorbis_enc_set_format (GstAudioEncoder * enc, GstAudioInfo * info)
{
  GstVorbisEnc *vorbisenc;

  vorbisenc = GST_VORBISENC (enc);

  vorbisenc->channels = GST_AUDIO_INFO_CHANNELS (info);
  vorbisenc->frequency = GST_AUDIO_INFO_RATE (info);

  /* if re-configured, we were drained and cleared already */
  vorbisenc->header_sent = FALSE;
  if (!gst_vorbis_enc_setup (vorbisenc))
    return FALSE;

  /* feedback to base class */
  gst_audio_encoder_set_latency (enc,
      gst_vorbis_enc_get_latency (vorbisenc),
      gst_vorbis_enc_get_latency (vorbisenc));

  return TRUE;
}

static void
gst_vorbis_enc_metadata_set1 (const GstTagList * list, const gchar * tag,
    gpointer vorbisenc)
{
  GstVorbisEnc *enc = GST_VORBISENC (vorbisenc);
  GList *vc_list, *l;

  vc_list = gst_tag_to_vorbis_comments (list, tag);

  for (l = vc_list; l != NULL; l = l->next) {
    const gchar *vc_string = (const gchar *) l->data;
    gchar *key = NULL, *val = NULL;

    GST_LOG_OBJECT (vorbisenc, "vorbis comment: %s", vc_string);
    if (gst_tag_parse_extended_comment (vc_string, &key, NULL, &val, TRUE)) {
      vorbis_comment_add_tag (&enc->vc, key, val);
      g_free (key);
      g_free (val);
    }
  }

  g_list_foreach (vc_list, (GFunc) g_free, NULL);
  g_list_free (vc_list);
}

static void
gst_vorbis_enc_set_metadata (GstVorbisEnc * enc)
{
  GstTagList *merged_tags;
  const GstTagList *user_tags;

  vorbis_comment_init (&enc->vc);

  user_tags = gst_tag_setter_get_tag_list (GST_TAG_SETTER (enc));

  GST_DEBUG_OBJECT (enc, "upstream tags = %" GST_PTR_FORMAT, enc->tags);
  GST_DEBUG_OBJECT (enc, "user-set tags = %" GST_PTR_FORMAT, user_tags);

  /* gst_tag_list_merge() will handle NULL for either or both lists fine */
  merged_tags = gst_tag_list_merge (user_tags, enc->tags,
      gst_tag_setter_get_tag_merge_mode (GST_TAG_SETTER (enc)));

  if (merged_tags) {
    GST_DEBUG_OBJECT (enc, "merged   tags = %" GST_PTR_FORMAT, merged_tags);
    gst_tag_list_foreach (merged_tags, gst_vorbis_enc_metadata_set1, enc);
    gst_tag_list_unref (merged_tags);
  }
}

static gchar *
get_constraints_string (GstVorbisEnc * vorbisenc)
{
  gint min = vorbisenc->min_bitrate;
  gint max = vorbisenc->max_bitrate;
  gchar *result;

  if (min > 0 && max > 0)
    result = g_strdup_printf ("(min %d bps, max %d bps)", min, max);
  else if (min > 0)
    result = g_strdup_printf ("(min %d bps, no max)", min);
  else if (max > 0)
    result = g_strdup_printf ("(no min, max %d bps)", max);
  else
    result = g_strdup_printf ("(no min or max)");

  return result;
}

static void
update_start_message (GstVorbisEnc * vorbisenc)
{
  gchar *constraints;

  g_free (vorbisenc->last_message);

  if (vorbisenc->bitrate > 0) {
    if (vorbisenc->managed) {
      constraints = get_constraints_string (vorbisenc);
      vorbisenc->last_message =
          g_strdup_printf ("encoding at average bitrate %d bps %s",
          vorbisenc->bitrate, constraints);
      g_free (constraints);
    } else {
      vorbisenc->last_message =
          g_strdup_printf
          ("encoding at approximate bitrate %d bps (VBR encoding enabled)",
          vorbisenc->bitrate);
    }
  } else {
    if (vorbisenc->quality_set) {
      if (vorbisenc->managed) {
        constraints = get_constraints_string (vorbisenc);
        vorbisenc->last_message =
            g_strdup_printf
            ("encoding at quality level %2.2f using constrained VBR %s",
            vorbisenc->quality, constraints);
        g_free (constraints);
      } else {
        vorbisenc->last_message =
            g_strdup_printf ("encoding at quality level %2.2f",
            vorbisenc->quality);
      }
    } else {
      constraints = get_constraints_string (vorbisenc);
      vorbisenc->last_message =
          g_strdup_printf ("encoding using bitrate management %s", constraints);
      g_free (constraints);
    }
  }

  g_object_notify (G_OBJECT (vorbisenc), "last_message");
}

static gboolean
gst_vorbis_enc_setup (GstVorbisEnc * vorbisenc)
{

  GST_LOG_OBJECT (vorbisenc, "setup");

  if (vorbisenc->bitrate < 0 && vorbisenc->min_bitrate < 0
      && vorbisenc->max_bitrate < 0) {
    vorbisenc->quality_set = TRUE;
  }

  update_start_message (vorbisenc);

  /* choose an encoding mode */
  /* (mode 0: 44kHz stereo uncoupled, roughly 128kbps VBR) */
  vorbis_info_init (&vorbisenc->vi);

  if (vorbisenc->quality_set) {
    if (vorbis_encode_setup_vbr (&vorbisenc->vi,
            vorbisenc->channels, vorbisenc->frequency,
            vorbisenc->quality) != 0) {
      GST_ERROR_OBJECT (vorbisenc,
          "vorbisenc: initialisation failed: invalid parameters for quality");
      vorbis_info_clear (&vorbisenc->vi);
      return FALSE;
    }

    /* do we have optional hard quality restrictions? */
    if (vorbisenc->max_bitrate > 0 || vorbisenc->min_bitrate > 0) {
      struct ovectl_ratemanage_arg ai;

      vorbis_encode_ctl (&vorbisenc->vi, OV_ECTL_RATEMANAGE_GET, &ai);

      ai.bitrate_hard_min = vorbisenc->min_bitrate;
      ai.bitrate_hard_max = vorbisenc->max_bitrate;
      ai.management_active = 1;

      vorbis_encode_ctl (&vorbisenc->vi, OV_ECTL_RATEMANAGE_SET, &ai);
    }
  } else {
    long min_bitrate, max_bitrate;

    min_bitrate = vorbisenc->min_bitrate > 0 ? vorbisenc->min_bitrate : -1;
    max_bitrate = vorbisenc->max_bitrate > 0 ? vorbisenc->max_bitrate : -1;

    if (vorbis_encode_setup_managed (&vorbisenc->vi,
            vorbisenc->channels,
            vorbisenc->frequency,
            max_bitrate, vorbisenc->bitrate, min_bitrate) != 0) {
      GST_ERROR_OBJECT (vorbisenc,
          "vorbis_encode_setup_managed "
          "(c %d, rate %d, max br %ld, br %d, min br %ld) failed",
          vorbisenc->channels, vorbisenc->frequency, max_bitrate,
          vorbisenc->bitrate, min_bitrate);
      vorbis_info_clear (&vorbisenc->vi);
      return FALSE;
    }
  }

  if (vorbisenc->managed && vorbisenc->bitrate < 0) {
    vorbis_encode_ctl (&vorbisenc->vi, OV_ECTL_RATEMANAGE_AVG, NULL);
  } else if (!vorbisenc->managed) {
    /* Turn off management entirely (if it was turned on). */
    vorbis_encode_ctl (&vorbisenc->vi, OV_ECTL_RATEMANAGE_SET, NULL);
  }
  vorbis_encode_setup_init (&vorbisenc->vi);

  /* set up the analysis state and auxiliary encoding storage */
  vorbis_analysis_init (&vorbisenc->vd, &vorbisenc->vi);
  vorbis_block_init (&vorbisenc->vd, &vorbisenc->vb);

  /* samples == granulepos start at 0 again */
  vorbisenc->samples_out = 0;

  /* fresh encoder available */
  vorbisenc->setup = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_vorbis_enc_clear (GstVorbisEnc * vorbisenc)
{
  GstFlowReturn ret = GST_FLOW_OK;

  if (vorbisenc->setup) {
    vorbis_analysis_wrote (&vorbisenc->vd, 0);
    ret = gst_vorbis_enc_output_buffers (vorbisenc);

    /* marked EOS to encoder, recreate if needed */
    vorbisenc->setup = FALSE;
  }

  /* clean up and exit.  vorbis_info_clear() must be called last */
  vorbis_block_clear (&vorbisenc->vb);
  vorbis_dsp_clear (&vorbisenc->vd);
  vorbis_info_clear (&vorbisenc->vi);

  return ret;
}

static void
gst_vorbis_enc_flush (GstAudioEncoder * enc)
{
  GstVorbisEnc *vorbisenc = GST_VORBISENC (enc);

  gst_vorbis_enc_clear (vorbisenc);
  vorbisenc->header_sent = FALSE;
}

/* copied and adapted from ext/ogg/gstoggstream.c */
static gint64
packet_duration_vorbis (GstVorbisEnc * enc, ogg_packet * packet)
{
  int mode;
  int size;
  int duration;

  if (packet->bytes == 0 || packet->packet[0] & 1)
    return 0;

  mode = (packet->packet[0] >> 1) & ((1 << enc->vorbis_log2_num_modes) - 1);
  size = enc->vorbis_mode_sizes[mode] ? enc->long_size : enc->short_size;

  if (enc->last_size == 0) {
    duration = 0;
  } else {
    duration = enc->last_size / 4 + size / 4;
  }
  enc->last_size = size;

  GST_DEBUG_OBJECT (enc, "duration %d", (int) duration);

  return duration;
}

/* copied and adapted from ext/ogg/gstoggstream.c */
static void
parse_vorbis_header_packet (GstVorbisEnc * enc, ogg_packet * packet)
{
  /*
   * on the first (b_o_s) packet, determine the long and short sizes,
   * and then calculate l/2, l/4 - s/4, 3 * l/4 - s/4, l/2 - s/2 and s/2
   */

  enc->long_size = 1 << (packet->packet[28] >> 4);
  enc->short_size = 1 << (packet->packet[28] & 0xF);
}

/* copied and adapted from ext/ogg/gstoggstream.c */
static void
parse_vorbis_codebooks_packet (GstVorbisEnc * enc, ogg_packet * op)
{
  /*
   * the code pages, a whole bunch of other fairly useless stuff, AND,
   * RIGHT AT THE END (of a bunch of variable-length compressed rubbish that
   * basically has only one actual set of values that everyone uses BUT YOU
   * CAN'T BE SURE OF THAT, OH NO YOU CAN'T) is the only piece of data that's
   * actually useful to us - the packet modes (because it's inconceivable to
   * think people might want _just that_ and nothing else, you know, for
   * seeking and stuff).
   *
   * Fortunately, because of the mandate that non-used bits must be zero
   * at the end of the packet, we might be able to sneakily work backwards
   * and find out the information we need (namely a mapping of modes to
   * packet sizes)
   */
  unsigned char *current_pos = &op->packet[op->bytes - 1];
  int offset;
  int size;
  int size_check;
  int *mode_size_ptr;
  int i;
  int ii;

  /*
   * This is the format of the mode data at the end of the packet for all
   * Vorbis Version 1 :
   *
   * [ 6:number_of_modes ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:size | 16:window_type(0) | 16:transform_type(0) | 8:mapping ]
   * [ 1:framing(1) ]
   *
   * e.g.:
   *
   *              <-
   * 0 0 0 0 0 1 0 0
   * 0 0 1 0 0 0 0 0
   * 0 0 1 0 0 0 0 0
   * 0 0 1|0 0 0 0 0
   * 0 0 0 0|0|0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 0 0|0 0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 0 0|0 0 0 0
   * 0 0 0|1|0 0 0 0 |
   * 0 0 0 0 0 0 0 0 V
   * 0 0 0|0 0 0 0 0
   * 0 0 0 0 0 0 0 0
   * 0 0 1|0 0 0 0 0
   * 0 0|1|0 0 0 0 0
   *
   *
   * i.e. each entry is an important bit, 32 bits of 0, 8 bits of blah, a
   * bit of 1.
   * Let's find our last 1 bit first.
   *
   */

  size = 0;

  offset = 8;
  while (!((1 << --offset) & *current_pos)) {
    if (offset == 0) {
      offset = 8;
      current_pos -= 1;
    }
  }

  while (1) {

    /*
     * from current_pos-5:(offset+1) to current_pos-1:(offset+1) should
     * be zero
     */
    offset = (offset + 7) % 8;
    if (offset == 7)
      current_pos -= 1;

    if (((current_pos[-5] & ~((1 << (offset + 1)) - 1)) != 0)
        ||
        current_pos[-4] != 0
        ||
        current_pos[-3] != 0
        ||
        current_pos[-2] != 0
        || ((current_pos[-1] & ((1 << (offset + 1)) - 1)) != 0)
        ) {
      break;
    }

    size += 1;

    current_pos -= 5;

  }

  /* Give ourselves a chance to recover if we went back too far by using
   * the size check. */
  for (ii = 0; ii < 2; ii++) {
    if (offset > 4) {
      size_check = (current_pos[0] >> (offset - 5)) & 0x3F;
    } else {
      /* mask part of byte from current_pos */
      size_check = (current_pos[0] & ((1 << (offset + 1)) - 1));
      /* shift to appropriate position */
      size_check <<= (5 - offset);
      /* or in part of byte from current_pos - 1 */
      size_check |= (current_pos[-1] & ~((1 << (offset + 3)) - 1)) >>
          (offset + 3);
    }

    size_check += 1;
    if (size_check == size) {
      break;
    }
    offset = (offset + 1) % 8;
    if (offset == 0)
      current_pos += 1;
    current_pos += 5;
    size -= 1;
  }

  /* Store mode size information in our info struct */
  i = -1;
  while ((1 << (++i)) < size);
  enc->vorbis_log2_num_modes = i;

  mode_size_ptr = enc->vorbis_mode_sizes;

  for (i = 0; i < size; i++) {
    offset = (offset + 1) % 8;
    if (offset == 0)
      current_pos += 1;
    *mode_size_ptr++ = (current_pos[0] >> offset) & 0x1;
    current_pos += 5;
  }

}

static GstBuffer *
gst_vorbis_enc_buffer_from_header_packet (GstVorbisEnc * vorbisenc,
    ogg_packet * packet)
{
  GstBuffer *outbuf;

  if (packet->bytes > 0 && packet->packet[0] == '\001') {
    parse_vorbis_header_packet (vorbisenc, packet);
  } else if (packet->bytes > 0 && packet->packet[0] == '\005') {
    parse_vorbis_codebooks_packet (vorbisenc, packet);
  }

  outbuf =
      gst_audio_encoder_allocate_output_buffer (GST_AUDIO_ENCODER (vorbisenc),
      packet->bytes);
  gst_buffer_fill (outbuf, 0, packet->packet, packet->bytes);
  GST_BUFFER_OFFSET (outbuf) = 0;
  GST_BUFFER_OFFSET_END (outbuf) = 0;
  GST_BUFFER_TIMESTAMP (outbuf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_HEADER);

  GST_DEBUG ("created header packet buffer, %" G_GSIZE_FORMAT " bytes",
      gst_buffer_get_size (outbuf));
  return outbuf;
}

static gboolean
gst_vorbis_enc_sink_event (GstAudioEncoder * enc, GstEvent * event)
{
  GstVorbisEnc *vorbisenc;

  vorbisenc = GST_VORBISENC (enc);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      if (vorbisenc->tags) {
        GstTagList *list;

        gst_event_parse_tag (event, &list);
        gst_tag_list_insert (vorbisenc->tags, list,
            gst_tag_setter_get_tag_merge_mode (GST_TAG_SETTER (vorbisenc)));
      } else {
        g_assert_not_reached ();
      }
      break;
      /* fall through */
    default:
      break;
  }

  /* we only peeked, let base class handle it */
  return GST_AUDIO_ENCODER_CLASS (parent_class)->sink_event (enc, event);
}

/*
 * (really really) FIXME: move into core (dixit tpm)
 */
/*
 * _gst_caps_set_buffer_array:
 * @caps: (transfer full): a #GstCaps
 * @field: field in caps to set
 * @buf: header buffers
 *
 * Adds given buffers to an array of buffers set as the given @field
 * on the given @caps.  List of buffer arguments must be NULL-terminated.
 *
 * Returns: (transfer full): input caps with a streamheader field added, or NULL
 *     if some error occurred
 */
static GstCaps *
_gst_caps_set_buffer_array (GstCaps * caps, const gchar * field,
    GstBuffer * buf, ...)
{
  GstStructure *structure = NULL;
  va_list va;
  GValue array = { 0 };
  GValue value = { 0 };

  g_return_val_if_fail (caps != NULL, NULL);
  g_return_val_if_fail (gst_caps_is_fixed (caps), NULL);
  g_return_val_if_fail (field != NULL, NULL);

  caps = gst_caps_make_writable (caps);
  structure = gst_caps_get_structure (caps, 0);

  g_value_init (&array, GST_TYPE_ARRAY);

  va_start (va, buf);
  /* put buffers in a fixed list */
  while (buf) {
    g_value_init (&value, GST_TYPE_BUFFER);
    gst_value_set_buffer (&value, buf);
    gst_value_array_append_value (&array, &value);
    g_value_unset (&value);

    buf = va_arg (va, GstBuffer *);
  }
  va_end (va);

  gst_structure_take_value (structure, field, &array);

  return caps;
}

static GstFlowReturn
gst_vorbis_enc_handle_frame (GstAudioEncoder * enc, GstBuffer * buffer)
{
  GstVorbisEnc *vorbisenc;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;
  gfloat *ptr;
  gulong size;
  gulong i, j;
  float **vorbis_buffer;
  GstBuffer *buf1, *buf2, *buf3;

  vorbisenc = GST_VORBISENC (enc);

  if (G_UNLIKELY (!vorbisenc->setup)) {
    if (buffer) {
      GST_DEBUG_OBJECT (vorbisenc, "forcing setup");
      /* should not fail, as setup before same way */
      if (!gst_vorbis_enc_setup (vorbisenc))
        return GST_FLOW_ERROR;
    } else {
      /* end draining */
      GST_LOG_OBJECT (vorbisenc, "already drained");
      return GST_FLOW_OK;
    }
  }

  if (!vorbisenc->header_sent) {
    /* Vorbis streams begin with three headers; the initial header (with
       most of the codec setup parameters) which is mandated by the Ogg
       bitstream spec.  The second header holds any comment fields.  The
       third header holds the bitstream codebook.  We merely need to
       make the headers, then pass them to libvorbis one at a time;
       libvorbis handles the additional Ogg bitstream constraints */
    ogg_packet header;
    ogg_packet header_comm;
    ogg_packet header_code;
    GstCaps *caps;
    GList *headers;

    GST_DEBUG_OBJECT (vorbisenc, "creating and sending header packets");
    gst_vorbis_enc_set_metadata (vorbisenc);
    vorbis_analysis_headerout (&vorbisenc->vd, &vorbisenc->vc, &header,
        &header_comm, &header_code);
    vorbis_comment_clear (&vorbisenc->vc);

    /* create header buffers */
    buf1 = gst_vorbis_enc_buffer_from_header_packet (vorbisenc, &header);
    buf2 = gst_vorbis_enc_buffer_from_header_packet (vorbisenc, &header_comm);
    buf3 = gst_vorbis_enc_buffer_from_header_packet (vorbisenc, &header_code);

    /* mark and put on caps */
    caps = gst_caps_new_simple ("audio/x-vorbis",
        "rate", G_TYPE_INT, vorbisenc->frequency,
        "channels", G_TYPE_INT, vorbisenc->channels, NULL);
    caps = _gst_caps_set_buffer_array (caps, "streamheader",
        buf1, buf2, buf3, NULL);

    /* negotiate with these caps */
    GST_DEBUG_OBJECT (vorbisenc, "here are the caps: %" GST_PTR_FORMAT, caps);
    gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (vorbisenc), caps);
    gst_caps_unref (caps);

    /* store buffers for later pre_push sending */
    headers = NULL;
    GST_DEBUG_OBJECT (vorbisenc, "storing header buffers");
    headers = g_list_prepend (headers, buf3);
    headers = g_list_prepend (headers, buf2);
    headers = g_list_prepend (headers, buf1);
    gst_audio_encoder_set_headers (enc, headers);

    vorbisenc->header_sent = TRUE;
  }

  if (!buffer)
    return gst_vorbis_enc_clear (vorbisenc);

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  /* data to encode */
  size = map.size / (vorbisenc->channels * sizeof (float));
  ptr = (gfloat *) map.data;

  /* expose the buffer to submit data */
  vorbis_buffer = vorbis_analysis_buffer (&vorbisenc->vd, size);

  /* deinterleave samples, write the buffer data */
  if (vorbisenc->channels < 2 || vorbisenc->channels > 8) {
    for (i = 0; i < size; i++) {
      for (j = 0; j < vorbisenc->channels; j++) {
        vorbis_buffer[j][i] = *ptr++;
      }
    }
  } else {
    gint i, j;

    /* Reorder */
    for (i = 0; i < size; i++) {
      for (j = 0; j < vorbisenc->channels; j++) {
        vorbis_buffer[gst_vorbis_reorder_map[vorbisenc->channels - 1][j]][i] =
            ptr[j];
      }
      ptr += vorbisenc->channels;
    }
  }

  /* tell the library how much we actually submitted */
  vorbis_analysis_wrote (&vorbisenc->vd, size);
  gst_buffer_unmap (buffer, &map);

  GST_LOG_OBJECT (vorbisenc, "wrote %lu samples to vorbis", size);

  ret = gst_vorbis_enc_output_buffers (vorbisenc);

  return ret;
}

static GstFlowReturn
gst_vorbis_enc_output_buffers (GstVorbisEnc * vorbisenc)
{
  GstFlowReturn ret;
  gint64 duration;

  /* vorbis does some data preanalysis, then divides up blocks for
     more involved (potentially parallel) processing.  Get a single
     block for encoding now */
  while (vorbis_analysis_blockout (&vorbisenc->vd, &vorbisenc->vb) == 1) {
    ogg_packet op;

    GST_LOG_OBJECT (vorbisenc, "analysed to a block");

    /* analysis */
    vorbis_analysis (&vorbisenc->vb, NULL);
    vorbis_bitrate_addblock (&vorbisenc->vb);

    while (vorbis_bitrate_flushpacket (&vorbisenc->vd, &op)) {
      GstBuffer *buf;

      GST_LOG_OBJECT (vorbisenc, "pushing out a data packet");
      buf =
          gst_audio_encoder_allocate_output_buffer (GST_AUDIO_ENCODER
          (vorbisenc), op.bytes);
      gst_buffer_fill (buf, 0, op.packet, op.bytes);

      /* we have to call this every packet, not just on e_o_s, since
         each packet's duration depends on the previous one's */
      duration = packet_duration_vorbis (vorbisenc, &op);
      if (op.e_o_s) {
        gint64 samples = op.granulepos - vorbisenc->samples_out;
        if (samples < duration) {
          gint64 trim_end = duration - samples;
          GST_DEBUG_OBJECT (vorbisenc,
              "Adding trim-end %" G_GUINT64_FORMAT, trim_end);
          gst_buffer_add_audio_clipping_meta (buf, GST_FORMAT_DEFAULT, 0,
              trim_end);
        }
      }
      /* tracking granulepos should tell us samples accounted for */
      ret =
          gst_audio_encoder_finish_frame (GST_AUDIO_ENCODER
          (vorbisenc), buf, op.granulepos - vorbisenc->samples_out);
      vorbisenc->samples_out = op.granulepos;

      if (ret != GST_FLOW_OK)
        return ret;
    }
  }

  return GST_FLOW_OK;
}

static void
gst_vorbis_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstVorbisEnc *vorbisenc;

  g_return_if_fail (GST_IS_VORBISENC (object));

  vorbisenc = GST_VORBISENC (object);

  switch (prop_id) {
    case ARG_MAX_BITRATE:
      g_value_set_int (value, vorbisenc->max_bitrate);
      break;
    case ARG_BITRATE:
      g_value_set_int (value, vorbisenc->bitrate);
      break;
    case ARG_MIN_BITRATE:
      g_value_set_int (value, vorbisenc->min_bitrate);
      break;
    case ARG_QUALITY:
      g_value_set_float (value, vorbisenc->quality);
      break;
    case ARG_MANAGED:
      g_value_set_boolean (value, vorbisenc->managed);
      break;
    case ARG_LAST_MESSAGE:
      g_value_set_string (value, vorbisenc->last_message);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_vorbis_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVorbisEnc *vorbisenc;

  g_return_if_fail (GST_IS_VORBISENC (object));

  vorbisenc = GST_VORBISENC (object);

  switch (prop_id) {
    case ARG_MAX_BITRATE:
    {
      gboolean old_value = vorbisenc->managed;

      vorbisenc->max_bitrate = g_value_get_int (value);
      if (vorbisenc->max_bitrate >= 0
          && vorbisenc->max_bitrate < LOWEST_BITRATE) {
        g_warning ("Lowest allowed bitrate is %d", LOWEST_BITRATE);
        vorbisenc->max_bitrate = LOWEST_BITRATE;
      }
      if (vorbisenc->min_bitrate > 0 && vorbisenc->max_bitrate > 0)
        vorbisenc->managed = TRUE;
      else
        vorbisenc->managed = FALSE;

      if (old_value != vorbisenc->managed)
        g_object_notify (object, "managed");
      break;
    }
    case ARG_BITRATE:
      vorbisenc->bitrate = g_value_get_int (value);
      if (vorbisenc->bitrate >= 0 && vorbisenc->bitrate < LOWEST_BITRATE) {
        g_warning ("Lowest allowed bitrate is %d", LOWEST_BITRATE);
        vorbisenc->bitrate = LOWEST_BITRATE;
      }
      break;
    case ARG_MIN_BITRATE:
    {
      gboolean old_value = vorbisenc->managed;

      vorbisenc->min_bitrate = g_value_get_int (value);
      if (vorbisenc->min_bitrate >= 0
          && vorbisenc->min_bitrate < LOWEST_BITRATE) {
        g_warning ("Lowest allowed bitrate is %d", LOWEST_BITRATE);
        vorbisenc->min_bitrate = LOWEST_BITRATE;
      }
      if (vorbisenc->min_bitrate > 0 && vorbisenc->max_bitrate > 0)
        vorbisenc->managed = TRUE;
      else
        vorbisenc->managed = FALSE;

      if (old_value != vorbisenc->managed)
        g_object_notify (object, "managed");
      break;
    }
    case ARG_QUALITY:
      vorbisenc->quality = g_value_get_float (value);
      if (vorbisenc->quality >= 0.0)
        vorbisenc->quality_set = TRUE;
      else
        vorbisenc->quality_set = FALSE;
      break;
    case ARG_MANAGED:
      vorbisenc->managed = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}
