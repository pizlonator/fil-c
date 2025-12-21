/* GStreamer
 * Copyright (C) 2004 Benjamin Otte <in7y118@public.uni-hamburg.de>
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
 * SECTION:element-vorbisdec
 * @title: vorbisdec
 * @see_also: vorbisenc, oggdemux
 *
 * This element decodes a Vorbis stream to raw float audio.
 * [Vorbis](http://www.vorbis.com/) is a royalty-free audio codec maintained
 * by the [Xiph.org Foundation](http://www.xiph.org/). As it outputs raw float
 * audio you will often need to put an audioconvert element after it.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v filesrc location=sine.ogg ! oggdemux ! vorbisdec ! audioconvert ! audioresample ! autoaudiosink
 * ]|
 *  Decode an Ogg/Vorbis. To create an Ogg/Vorbis file refer to the documentation of vorbisenc.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstvorbisdec.h"
#include <string.h>
#include <gst/audio/audio.h>
#include <gst/tag/tag.h>

#include "gstvorbiselements.h"
#include "gstvorbiscommon.h"

#ifndef TREMOR
GST_DEBUG_CATEGORY_STATIC (vorbisdec_debug);
#define GST_CAT_DEFAULT vorbisdec_debug
#else
GST_DEBUG_CATEGORY_STATIC (ivorbisdec_debug);
#define GST_CAT_DEFAULT ivorbisdec_debug
#endif

static GstStaticPadTemplate vorbis_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_VORBIS_DEC_SRC_CAPS);

static GstStaticPadTemplate vorbis_dec_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-vorbis")
    );

#define gst_vorbis_dec_parent_class parent_class
G_DEFINE_TYPE (GstVorbisDec, gst_vorbis_dec, GST_TYPE_AUDIO_DECODER);
#ifndef TREMOR
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vorbisdec, "vorbisdec",
    GST_RANK_PRIMARY, GST_TYPE_VORBIS_DEC,
    GST_DEBUG_CATEGORY_INIT (vorbisdec_debug, "vorbisdec", 0,
        "vorbis decoding element");
    vorbis_element_init (plugin));
#else
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (ivorbisdec, "ivorbisdec",
    GST_RANK_SECONDARY, GST_TYPE_VORBIS_DEC,
    GST_DEBUG_CATEGORY_INIT (ivorbisdec_debug, "ivorbisdec", 0,
        "vorbis decoding element (integer decoder)");
    vorbis_element_init (plugin));
#endif

static void vorbis_dec_finalize (GObject * object);

static gboolean vorbis_dec_start (GstAudioDecoder * dec);
static gboolean vorbis_dec_stop (GstAudioDecoder * dec);
static GstFlowReturn vorbis_dec_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);
static void vorbis_dec_flush (GstAudioDecoder * dec, gboolean hard);
static gboolean vorbis_dec_set_format (GstAudioDecoder * dec, GstCaps * caps);
static void vorbis_dec_reset (GstAudioDecoder * dec);

static void
gst_vorbis_dec_class_init (GstVorbisDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *base_class = GST_AUDIO_DECODER_CLASS (klass);

  gobject_class->finalize = vorbis_dec_finalize;

  gst_element_class_add_static_pad_template (element_class,
      &vorbis_dec_src_factory);
  gst_element_class_add_static_pad_template (element_class,
      &vorbis_dec_sink_factory);

  gst_element_class_set_static_metadata (element_class,
      "Vorbis audio decoder", "Codec/Decoder/Audio",
      GST_VORBIS_DEC_DESCRIPTION,
      "Benjamin Otte <otte@gnome.org>, Chris Lord <chris@openedhand.com>");

  base_class->start = GST_DEBUG_FUNCPTR (vorbis_dec_start);
  base_class->stop = GST_DEBUG_FUNCPTR (vorbis_dec_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (vorbis_dec_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (vorbis_dec_handle_frame);
  base_class->flush = GST_DEBUG_FUNCPTR (vorbis_dec_flush);
}

static void
gst_vorbis_dec_init (GstVorbisDec * dec)
{
  gst_audio_decoder_set_use_default_pad_acceptcaps (GST_AUDIO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_DECODER_SINK_PAD (dec));
}

static void
vorbis_dec_finalize (GObject * object)
{
  /* Release any possibly allocated libvorbis data.
   * _clear functions can safely be called multiple times
   */
  GstVorbisDec *vd = GST_VORBIS_DEC (object);

#ifndef USE_TREMOLO
  vorbis_block_clear (&vd->vb);
#endif
  vorbis_dsp_clear (&vd->vd);
  vorbis_comment_clear (&vd->vc);
  vorbis_info_clear (&vd->vi);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
vorbis_dec_start (GstAudioDecoder * dec)
{
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  GST_DEBUG_OBJECT (dec, "start");
  vorbis_info_init (&vd->vi);
  vorbis_comment_init (&vd->vc);
  vd->initialized = FALSE;

  return TRUE;
}

static gboolean
vorbis_dec_stop (GstAudioDecoder * dec)
{
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  GST_DEBUG_OBJECT (dec, "stop");
  vd->initialized = FALSE;
#ifndef USE_TREMOLO
  vorbis_block_clear (&vd->vb);
#endif
  vorbis_dsp_clear (&vd->vd);
  vorbis_comment_clear (&vd->vc);
  vorbis_info_clear (&vd->vi);
  if (vd->pending_headers) {
    g_list_free_full (vd->pending_headers, (GDestroyNotify) gst_buffer_unref);
    vd->pending_headers = NULL;
  }

  return TRUE;
}

static GstFlowReturn
vorbis_handle_identification_packet (GstVorbisDec * vd)
{
  GstAudioInfo info;

  switch (vd->vi.channels) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    {
      const GstAudioChannelPosition *pos;

      pos = gst_vorbis_default_channel_positions[vd->vi.channels - 1];
      gst_audio_info_set_format (&info, GST_VORBIS_AUDIO_FORMAT, vd->vi.rate,
          vd->vi.channels, pos);
      break;
    }
    default:{
      GstAudioChannelPosition position[64];
      gint i, max_pos = MAX (vd->vi.channels, 64);

      GST_ELEMENT_WARNING (vd, STREAM, DECODE,
          (NULL), ("Using NONE channel layout for more than 8 channels"));
      for (i = 0; i < max_pos; i++)
        position[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
      gst_audio_info_set_format (&info, GST_VORBIS_AUDIO_FORMAT, vd->vi.rate,
          vd->vi.channels, position);
      break;
    }
  }

  gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (vd), &info);

  vd->info = info;
  /* select a copy_samples function, this way we can have specialized versions
   * for mono/stereo and avoid the depth switch in tremor case */
  vd->copy_samples = gst_vorbis_get_copy_sample_func (info.channels);

  return GST_FLOW_OK;
}

/* FIXME 0.11: remove tag handling and let container take care of that? */
static GstFlowReturn
vorbis_handle_comment_packet (GstVorbisDec * vd, ogg_packet * packet)
{
  guint bitrate = 0;
  gchar *encoder = NULL;
  GstTagList *list;
  guint8 *data;
  gsize size;

  GST_DEBUG_OBJECT (vd, "parsing comment packet");

  data = gst_ogg_packet_data (packet);
  size = gst_ogg_packet_size (packet);

  list =
      gst_tag_list_from_vorbiscomment (data, size, (guint8 *) "\003vorbis", 7,
      &encoder);

  if (!list) {
    GST_ERROR_OBJECT (vd, "couldn't decode comments");
    list = gst_tag_list_new_empty ();
  }

  if (encoder) {
    if (encoder[0])
      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
          GST_TAG_ENCODER, encoder, NULL);
    g_free (encoder);
  }
  gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
      GST_TAG_ENCODER_VERSION, vd->vi.version,
      GST_TAG_AUDIO_CODEC, "Vorbis", NULL);
  if (vd->vi.bitrate_nominal > 0 && vd->vi.bitrate_nominal <= 0x7FFFFFFF) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_NOMINAL_BITRATE, (guint) vd->vi.bitrate_nominal, NULL);
    bitrate = vd->vi.bitrate_nominal;
  }
  if (vd->vi.bitrate_upper > 0 && vd->vi.bitrate_upper <= 0x7FFFFFFF) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_MAXIMUM_BITRATE, (guint) vd->vi.bitrate_upper, NULL);
    if (!bitrate)
      bitrate = vd->vi.bitrate_upper;
  }
  if (vd->vi.bitrate_lower > 0 && vd->vi.bitrate_lower <= 0x7FFFFFFF) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_MINIMUM_BITRATE, (guint) vd->vi.bitrate_lower, NULL);
    if (!bitrate)
      bitrate = vd->vi.bitrate_lower;
  }
  if (bitrate) {
    gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
        GST_TAG_BITRATE, (guint) bitrate, NULL);
  }

  gst_audio_decoder_merge_tags (GST_AUDIO_DECODER_CAST (vd), list,
      GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (list);

  return GST_FLOW_OK;
}

static GstFlowReturn
vorbis_handle_type_packet (GstVorbisDec * vd)
{
  gint res;

  g_assert (!vd->initialized);

#ifdef USE_TREMOLO
  if (G_UNLIKELY ((res = vorbis_dsp_init (&vd->vd, &vd->vi))))
    goto synthesis_init_error;
#else
  if (G_UNLIKELY ((res = vorbis_synthesis_init (&vd->vd, &vd->vi))))
    goto synthesis_init_error;

  if (G_UNLIKELY ((res = vorbis_block_init (&vd->vd, &vd->vb))))
    goto block_init_error;
#endif

  vd->initialized = TRUE;

  return GST_FLOW_OK;

  /* ERRORS */
synthesis_init_error:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("couldn't initialize synthesis (%d)", res));
    return GST_FLOW_ERROR;
  }
block_init_error:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("couldn't initialize block (%d)", res));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
vorbis_handle_header_packet (GstVorbisDec * vd, ogg_packet * packet)
{
  GstFlowReturn res;
  gint ret;

  GST_DEBUG_OBJECT (vd, "parsing header packet");

  /* Packetno = 0 if the first byte is exactly 0x01 */
  packet->b_o_s = ((gst_ogg_packet_data (packet))[0] == 0x1) ? 1 : 0;

#ifdef USE_TREMOLO
  if ((ret = vorbis_dsp_headerin (&vd->vi, &vd->vc, packet)))
#else
  if ((ret = vorbis_synthesis_headerin (&vd->vi, &vd->vc, packet)))
#endif
    goto header_read_error;

  switch ((gst_ogg_packet_data (packet))[0]) {
    case 0x01:
      res = vorbis_handle_identification_packet (vd);
      break;
    case 0x03:
      res = vorbis_handle_comment_packet (vd, packet);
      break;
    case 0x05:
      res = vorbis_handle_type_packet (vd);
      break;
    default:
      /* ignore */
      GST_WARNING_OBJECT (vd, "unknown vorbis header packet found");
      res = GST_FLOW_OK;
      break;
  }

  return res;

  /* ERRORS */
header_read_error:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("couldn't read header packet (%d)", ret));
    return GST_FLOW_ERROR;
  }
}

/* Does not take ownership of buffer */
static GstFlowReturn
vorbis_dec_handle_header_buffer (GstVorbisDec * vd, GstBuffer * buffer)
{
  ogg_packet *packet;
  ogg_packet_wrapper packet_wrapper;
  GstFlowReturn ret;
  GstMapInfo map;

  gst_ogg_packet_wrapper_map (&packet_wrapper, buffer, &map);
  packet = gst_ogg_packet_from_wrapper (&packet_wrapper);

  ret = vorbis_handle_header_packet (vd, packet);

  gst_ogg_packet_wrapper_unmap (&packet_wrapper, buffer, &map);

  return ret;
}

#define MIN_NUM_HEADERS 3
static GstFlowReturn
vorbis_dec_handle_header_caps (GstVorbisDec * vd)
{
  GstFlowReturn result = GST_FLOW_OK;
  GstCaps *caps;
  GstStructure *s = NULL;
  const GValue *array = NULL;

  caps = gst_pad_get_current_caps (GST_AUDIO_DECODER_SINK_PAD (vd));
  if (caps)
    s = gst_caps_get_structure (caps, 0);
  if (s)
    array = gst_structure_get_value (s, "streamheader");

  if (caps)
    gst_caps_unref (caps);

  if (array && (gst_value_array_get_size (array) >= MIN_NUM_HEADERS)) {
    const GValue *value = NULL;
    GstBuffer *buf = NULL;
    gint i = 0;

    if (vd->pending_headers) {
      GST_DEBUG_OBJECT (vd,
          "got new headers from caps, discarding old pending headers");

      g_list_free_full (vd->pending_headers, (GDestroyNotify) gst_buffer_unref);
      vd->pending_headers = NULL;
    }

    while (result == GST_FLOW_OK && i < gst_value_array_get_size (array)) {
      value = gst_value_array_get_value (array, i);
      buf = gst_value_get_buffer (value);
      if (!buf)
        goto null_buffer;
      result = vorbis_dec_handle_header_buffer (vd, buf);
      i++;
    }
  } else
    goto array_error;

done:
  return (result != GST_FLOW_OK ? GST_FLOW_NOT_NEGOTIATED : GST_FLOW_OK);

  /* ERRORS */
array_error:
  {
    GST_WARNING_OBJECT (vd, "streamheader array not found");
    result = GST_FLOW_ERROR;
    goto done;
  }
null_buffer:
  {
    GST_WARNING_OBJECT (vd, "streamheader with null buffer received");
    result = GST_FLOW_ERROR;
    goto done;
  }
}


static GstFlowReturn
vorbis_handle_data_packet (GstVorbisDec * vd, ogg_packet * packet,
    GstClockTime timestamp, GstClockTime duration)
{
#ifdef USE_TREMOLO
  vorbis_sample_t *pcm;
#else
  vorbis_sample_t **pcm;
#endif
  guint sample_count;
  GstBuffer *out = NULL;
  GstFlowReturn result;
  GstMapInfo map;
  gsize size;

  if (G_UNLIKELY (!vd->initialized)) {
    result = vorbis_dec_handle_header_caps (vd);
    if (result != GST_FLOW_OK)
      goto not_initialized;
  }

  /* normal data packet */
  /* FIXME, we can skip decoding if the packet is outside of the
   * segment, this is however not very trivial as we need a previous
   * packet to decode the current one so we must be careful not to
   * throw away too much. For now we decode everything and clip right
   * before pushing data. */

#ifdef USE_TREMOLO
  if (G_UNLIKELY (vorbis_dsp_synthesis (&vd->vd, packet, 1)))
    goto could_not_read;
#else
  if (G_UNLIKELY (vorbis_synthesis (&vd->vb, packet)))
    goto could_not_read;

  if (G_UNLIKELY (vorbis_synthesis_blockin (&vd->vd, &vd->vb) < 0))
    goto not_accepted;
#endif

  /* assume all goes well here */
  result = GST_FLOW_OK;

  /* count samples ready for reading */
#ifdef USE_TREMOLO
  if ((sample_count = vorbis_dsp_pcmout (&vd->vd, NULL, 0)) == 0)
#else
  if ((sample_count = vorbis_synthesis_pcmout (&vd->vd, NULL)) == 0)
    goto done;
#endif

  size = sample_count * vd->info.bpf;
  GST_LOG_OBJECT (vd, "%d samples ready for reading, size %" G_GSIZE_FORMAT,
      sample_count, size);

  /* alloc buffer for it */
  out = gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (vd), size);

  gst_buffer_map (out, &map, GST_MAP_WRITE);
  /* get samples ready for reading now, should be sample_count */
#ifdef USE_TREMOLO
  if (G_UNLIKELY (vorbis_dsp_pcmout (&vd->vd, map.data, sample_count) !=
          sample_count))
#else
  if (G_UNLIKELY (vorbis_synthesis_pcmout (&vd->vd, &pcm) != sample_count))
#endif
    goto wrong_samples;

#ifdef USE_TREMOLO
  if (vd->info.channels < 9)
    gst_audio_reorder_channels (map.data, map.size, GST_VORBIS_AUDIO_FORMAT,
        vd->info.channels, gst_vorbis_channel_positions[vd->info.channels - 1],
        gst_vorbis_default_channel_positions[vd->info.channels - 1]);
#else
  /* copy samples in buffer */
  vd->copy_samples ((vorbis_sample_t *) map.data, pcm,
      sample_count, vd->info.channels);
#endif

  GST_LOG_OBJECT (vd, "have output size of %" G_GSIZE_FORMAT, size);
  gst_buffer_unmap (out, &map);

done:
  /* whether or not data produced, consume one frame and advance time */
  result = gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (vd), out, 1);

#ifdef USE_TREMOLO
  vorbis_dsp_read (&vd->vd, sample_count);
#else
  vorbis_synthesis_read (&vd->vd, sample_count);
#endif

  return result;

  /* ERRORS */
not_initialized:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("no header sent yet"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
could_not_read:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("couldn't read data packet"));
    return GST_FLOW_ERROR;
  }
not_accepted:
  {
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("vorbis decoder did not accept data packet"));
    return GST_FLOW_ERROR;
  }
wrong_samples:
  {
    gst_buffer_unref (out);
    GST_ELEMENT_ERROR (GST_ELEMENT (vd), STREAM, DECODE,
        (NULL), ("vorbis decoder reported wrong number of samples"));
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
check_pending_headers (GstVorbisDec * vd)
{
  GstBuffer *buffer1, *buffer3, *buffer5;
  GstMapInfo map;
  gboolean isvalid;
  GList *tmp = vd->pending_headers;
  GstFlowReturn result = GST_FLOW_OK;

  if (g_list_length (vd->pending_headers) < MIN_NUM_HEADERS)
    goto not_enough;

  buffer1 = (GstBuffer *) tmp->data;
  tmp = tmp->next;
  buffer3 = (GstBuffer *) tmp->data;
  tmp = tmp->next;
  buffer5 = (GstBuffer *) tmp->data;

  /* Start checking the headers */
  gst_buffer_map (buffer1, &map, GST_MAP_READ);
  isvalid = map.size >= 1 && map.data[0] == 0x01;
  gst_buffer_unmap (buffer1, &map);
  if (!isvalid) {
    GST_WARNING_OBJECT (vd, "Pending first header was invalid");
    goto cleanup;
  }

  gst_buffer_map (buffer3, &map, GST_MAP_READ);
  isvalid = map.size >= 1 && map.data[0] == 0x03;
  gst_buffer_unmap (buffer3, &map);
  if (!isvalid) {
    GST_WARNING_OBJECT (vd, "Pending second header was invalid");
    goto cleanup;
  }

  gst_buffer_map (buffer5, &map, GST_MAP_READ);
  isvalid = map.size >= 1 && map.data[0] == 0x05;
  gst_buffer_unmap (buffer5, &map);
  if (!isvalid) {
    GST_WARNING_OBJECT (vd, "Pending third header was invalid");
    goto cleanup;
  }

  /* Discard any other pending headers */
  if (tmp->next) {
    GST_DEBUG_OBJECT (vd, "Discarding extra headers");
    g_list_free_full (tmp->next, (GDestroyNotify) gst_buffer_unref);
    tmp->next = NULL;
  }
  g_list_free (vd->pending_headers);
  vd->pending_headers = NULL;

  GST_DEBUG_OBJECT (vd, "Resetting and processing new headers");

  /* All good, let's reset ourselves and process the headers */
  vorbis_dec_reset ((GstAudioDecoder *) vd);
  result = vorbis_dec_handle_header_buffer (vd, buffer1);
  gst_buffer_unref (buffer1);
  if (result != GST_FLOW_OK) {
    gst_buffer_unref (buffer3);
    gst_buffer_unref (buffer5);
    return result;
  }
  result = vorbis_dec_handle_header_buffer (vd, buffer3);
  gst_buffer_unref (buffer3);
  if (result != GST_FLOW_OK) {
    gst_buffer_unref (buffer5);
    return result;
  }
  result = vorbis_dec_handle_header_buffer (vd, buffer5);
  gst_buffer_unref (buffer5);

  return result;

  /* ERRORS */
cleanup:
  {
    g_list_free_full (vd->pending_headers, (GDestroyNotify) gst_buffer_unref);
    vd->pending_headers = NULL;
    return result;
  }
not_enough:
  {
    GST_LOG_OBJECT (vd,
        "Not enough pending headers to properly reset, ignoring them");
    goto cleanup;
  }
}

static GstFlowReturn
vorbis_dec_handle_frame (GstAudioDecoder * dec, GstBuffer * buffer)
{
  ogg_packet *packet;
  ogg_packet_wrapper packet_wrapper;
  GstFlowReturn result = GST_FLOW_OK;
  GstMapInfo map;
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  /* no draining etc */
  if (G_UNLIKELY (!buffer))
    return GST_FLOW_OK;

  GST_LOG_OBJECT (vd, "got buffer %p", buffer);
  /* make ogg_packet out of the buffer */
  gst_ogg_packet_wrapper_map (&packet_wrapper, buffer, &map);
  packet = gst_ogg_packet_from_wrapper (&packet_wrapper);
  /* set some more stuff */
  packet->granulepos = -1;
  packet->packetno = 0;         /* we don't care */
  /* EOS does not matter, it is used in vorbis to implement clipping the last
   * block of samples based on the granulepos. We clip based on segments. */
  packet->e_o_s = 0;

  GST_LOG_OBJECT (vd, "decode buffer of size %ld", packet->bytes);

  /* error out on empty header packets, but just skip empty data packets */
  if (G_UNLIKELY (packet->bytes == 0)) {
    if (vd->initialized)
      goto empty_buffer;
    else
      goto empty_header;
  }

  /* switch depending on packet type */
  if ((gst_ogg_packet_data (packet))[0] & 1) {
    gboolean have_all_headers;

    GST_LOG_OBJECT (vd, "storing header for later analyzis");

    /* An identification packet starts a new set of headers */
    if (vd->pending_headers && (gst_ogg_packet_data (packet))[0] == 0x01) {
      GST_DEBUG_OBJECT (vd,
          "got new identification header packet, discarding old pending headers");

      g_list_free_full (vd->pending_headers, (GDestroyNotify) gst_buffer_unref);
      vd->pending_headers = NULL;
    }

    /* if we have more than 3 headers with the new one and the new one is the
     * type header, we can initialize the decoder now */
    have_all_headers = g_list_length (vd->pending_headers) >= 2
        && (gst_ogg_packet_data (packet))[0] == 0x05;

    if (!vd->pending_headers && (gst_ogg_packet_data (packet))[0] != 0x01) {
      if (vd->initialized) {
        GST_DEBUG_OBJECT (vd,
            "Got another non-identification header after initialization, ignoring");
      } else {
        GST_WARNING_OBJECT (vd,
            "First header was not a identification header, dropping");
      }
      result = gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (vd), NULL, 1);
    } else {
      vd->pending_headers =
          g_list_append (vd->pending_headers, gst_buffer_ref (buffer));
      result = gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (vd), NULL, 1);
    }

    if (result == GST_FLOW_OK && have_all_headers) {
      result = check_pending_headers (vd);
    }
  } else {
    GstClockTime timestamp, duration;

    if (vd->pending_headers)
      result = check_pending_headers (vd);
    if (G_UNLIKELY (result != GST_FLOW_OK))
      goto done;

    timestamp = GST_BUFFER_TIMESTAMP (buffer);
    duration = GST_BUFFER_DURATION (buffer);

    result = vorbis_handle_data_packet (vd, packet, timestamp, duration);
  }

done:
  GST_LOG_OBJECT (vd, "unmap buffer %p", buffer);
  gst_ogg_packet_wrapper_unmap (&packet_wrapper, buffer, &map);

  return result;

empty_buffer:
  {
    /* don't error out here, just ignore the buffer, it's invalid for vorbis
     * but not fatal. */
    GST_WARNING_OBJECT (vd, "empty buffer received, ignoring");
    result = GST_FLOW_OK;
    goto done;
  }

/* ERRORS */
empty_header:
  {
    GST_ELEMENT_ERROR (vd, STREAM, DECODE, (NULL), ("empty header received"));
    result = GST_FLOW_ERROR;
    goto done;
  }
}

static void
vorbis_dec_flush (GstAudioDecoder * dec, gboolean hard)
{
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  vorbis_synthesis_restart (&vd->vd);
}

static void
vorbis_dec_reset (GstAudioDecoder * dec)
{
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  vd->initialized = FALSE;
#ifndef USE_TREMOLO
  vorbis_block_clear (&vd->vb);
#endif
  vorbis_dsp_clear (&vd->vd);

  vorbis_comment_clear (&vd->vc);
  vorbis_info_clear (&vd->vi);
  vorbis_info_init (&vd->vi);
  vorbis_comment_init (&vd->vc);
}

static gboolean
vorbis_dec_set_format (GstAudioDecoder * dec, GstCaps * caps)
{
  GstVorbisDec *vd = GST_VORBIS_DEC (dec);

  GST_DEBUG_OBJECT (vd, "New caps %" GST_PTR_FORMAT " - resetting", caps);

  /* A set_format call implies new data with new header packets */
  if (!vd->initialized)
    return TRUE;

  /* We need to free and re-init libvorbis,
   * or it chokes */
  vorbis_dec_reset (dec);

  return TRUE;
}
