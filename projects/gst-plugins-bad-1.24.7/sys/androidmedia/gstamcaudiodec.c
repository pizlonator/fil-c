/*
 * Initially based on gst-omx/omx/gstomxvideodec.c
 *
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <string.h>

#ifdef HAVE_ORC
#include <orc/orc.h>
#else
#define orc_memcpy memcpy
#endif

#include "gstamcaudiodec.h"
#include "gstamc-constants.h"

GST_DEBUG_CATEGORY_STATIC (gst_amc_audio_dec_debug_category);
#define GST_CAT_DEFAULT gst_amc_audio_dec_debug_category

#define GST_AUDIO_DECODER_ERROR_FROM_ERROR(el, err) G_STMT_START { \
  gchar *__dbg = g_strdup (err->message);                               \
  GstAudioDecoder *__dec = GST_AUDIO_DECODER (el);                      \
  GST_WARNING_OBJECT (el, "error: %s", __dbg);                          \
  _gst_audio_decoder_error (__dec, 1,                                   \
    err->domain, err->code,                                             \
    NULL, __dbg, __FILE__, GST_FUNCTION, __LINE__);                     \
  g_clear_error (&err); \
} G_STMT_END

/* prototypes */
static void gst_amc_audio_dec_finalize (GObject * object);

static GstStateChangeReturn
gst_amc_audio_dec_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_amc_audio_dec_open (GstAudioDecoder * decoder);
static gboolean gst_amc_audio_dec_close (GstAudioDecoder * decoder);
static gboolean gst_amc_audio_dec_start (GstAudioDecoder * decoder);
static gboolean gst_amc_audio_dec_stop (GstAudioDecoder * decoder);
static gboolean gst_amc_audio_dec_set_format (GstAudioDecoder * decoder,
    GstCaps * caps);
static void gst_amc_audio_dec_flush (GstAudioDecoder * decoder, gboolean hard);
static GstFlowReturn gst_amc_audio_dec_handle_frame (GstAudioDecoder * decoder,
    GstBuffer * buffer);

static GstFlowReturn gst_amc_audio_dec_drain (GstAmcAudioDec * self);

enum
{
  PROP_0
};

/* class initialization */

static void gst_amc_audio_dec_class_init (GstAmcAudioDecClass * klass);
static void gst_amc_audio_dec_init (GstAmcAudioDec * self);
static void gst_amc_audio_dec_base_init (gpointer g_class);

static GstAudioDecoderClass *parent_class = NULL;

GType
gst_amc_audio_dec_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstAmcAudioDecClass),
      gst_amc_audio_dec_base_init,
      NULL,
      (GClassInitFunc) gst_amc_audio_dec_class_init,
      NULL,
      NULL,
      sizeof (GstAmcAudioDec),
      0,
      (GInstanceInitFunc) gst_amc_audio_dec_init,
      NULL
    };

    _type = g_type_register_static (GST_TYPE_AUDIO_DECODER, "GstAmcAudioDec",
        &info, 0);

    GST_DEBUG_CATEGORY_INIT (gst_amc_audio_dec_debug_category, "amcaudiodec", 0,
        "Android MediaCodec audio decoder");

    g_once_init_leave (&type, _type);
  }
  return type;
}

static const gchar *
caps_to_mime (GstCaps * caps)
{
  GstStructure *s;
  const gchar *name;

  s = gst_caps_get_structure (caps, 0);
  if (!s)
    return NULL;

  name = gst_structure_get_name (s);

  if (strcmp (name, "audio/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion))
      return NULL;

    if (mpegversion == 1) {
      gint layer;

      if (!gst_structure_get_int (s, "layer", &layer) || layer == 3)
        return "audio/mpeg";
      else if (layer == 2)
        return "audio/mpeg-L2";
    } else if (mpegversion == 2 || mpegversion == 4) {
      return "audio/mp4a-latm";
    }
  } else if (strcmp (name, "audio/AMR") == 0) {
    return "audio/3gpp";
  } else if (strcmp (name, "audio/AMR-WB") == 0) {
    return "audio/amr-wb";
  } else if (strcmp (name, "audio/x-alaw") == 0) {
    return "audio/g711-alaw";
  } else if (strcmp (name, "audio/x-mulaw") == 0) {
    return "audio/g711-mlaw";
  } else if (strcmp (name, "audio/x-vorbis") == 0) {
    return "audio/vorbis";
  } else if (strcmp (name, "audio/x-opus") == 0) {
    return "audio/opus";
  }

  return NULL;
}

static void
gst_amc_audio_dec_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstAmcAudioDecClass *amcaudiodec_class = GST_AMC_AUDIO_DEC_CLASS (g_class);
  const GstAmcCodecInfo *codec_info;
  GstPadTemplate *templ;
  GstCaps *sink_caps, *src_caps;
  gchar *longname;

  codec_info =
      g_type_get_qdata (G_TYPE_FROM_CLASS (g_class), gst_amc_codec_info_quark);
  /* This happens for the base class and abstract subclasses */
  if (!codec_info)
    return;

  amcaudiodec_class->codec_info = codec_info;

  gst_amc_codec_info_to_caps (codec_info, &sink_caps, &src_caps);
  /* Add pad templates */
  templ =
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (sink_caps);

  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, src_caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (src_caps);

  longname = g_strdup_printf ("Android MediaCodec %s", codec_info->name);
  gst_element_class_set_metadata (element_class,
      codec_info->name,
      "Codec/Decoder/Audio",
      longname, "Sebastian Dröge <sebastian.droege@collabora.co.uk>");
  g_free (longname);
}

static void
gst_amc_audio_dec_class_init (GstAmcAudioDecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioDecoderClass *audiodec_class = GST_AUDIO_DECODER_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_amc_audio_dec_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_amc_audio_dec_change_state);

  audiodec_class->start = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_start);
  audiodec_class->stop = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_stop);
  audiodec_class->open = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_open);
  audiodec_class->close = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_close);
  audiodec_class->flush = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_flush);
  audiodec_class->set_format = GST_DEBUG_FUNCPTR (gst_amc_audio_dec_set_format);
  audiodec_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_amc_audio_dec_handle_frame);
}

static void
gst_amc_audio_dec_init (GstAmcAudioDec * self)
{
  gst_audio_decoder_set_needs_format (GST_AUDIO_DECODER (self), TRUE);
  gst_audio_decoder_set_drainable (GST_AUDIO_DECODER (self), TRUE);

  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);
  self->output_adapter = gst_adapter_new ();
}

static gboolean
gst_amc_audio_dec_open (GstAudioDecoder * decoder)
{
  GstAmcAudioDec *self = GST_AMC_AUDIO_DEC (decoder);
  GstAmcAudioDecClass *klass = GST_AMC_AUDIO_DEC_GET_CLASS (self);
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Opening decoder");

  self->codec = gst_amc_codec_new (klass->codec_info->name, FALSE, &err);
  if (!self->codec) {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    return FALSE;
  }
  self->started = FALSE;
  self->flushing = TRUE;

  GST_DEBUG_OBJECT (self, "Opened decoder");

  return TRUE;
}

static gboolean
gst_amc_audio_dec_close (GstAudioDecoder * decoder)
{
  GstAmcAudioDec *self = GST_AMC_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Closing decoder");

  if (self->codec) {
    GError *err = NULL;

    gst_amc_codec_release (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);

    gst_amc_codec_free (self->codec);
  }
  self->codec = NULL;

  self->started = FALSE;
  self->flushing = TRUE;

  GST_DEBUG_OBJECT (self, "Closed decoder");

  return TRUE;
}

static void
gst_amc_audio_dec_finalize (GObject * object)
{
  GstAmcAudioDec *self = GST_AMC_AUDIO_DEC (object);

  if (self->output_adapter)
    gst_object_unref (self->output_adapter);
  self->output_adapter = NULL;

  g_mutex_clear (&self->drain_lock);
  g_cond_clear (&self->drain_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_amc_audio_dec_change_state (GstElement * element, GstStateChange transition)
{
  GstAmcAudioDec *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GError *err = NULL;

  g_return_val_if_fail (GST_IS_AMC_AUDIO_DEC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_AMC_AUDIO_DEC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->downstream_flow_ret = GST_FLOW_OK;
      self->draining = FALSE;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->flushing = TRUE;
      gst_amc_codec_flush (self->codec, &err);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_amc_audio_dec_set_src_caps (GstAmcAudioDec * self, GstAmcFormat * format)
{
  gint rate, channels;
  guint32 channel_mask = 0;
  GstAudioChannelPosition to[64];
  GError *err = NULL;

  if (!gst_amc_format_get_int (format, "sample-rate", &rate, &err) ||
      !gst_amc_format_get_int (format, "channel-count", &channels, &err)) {
    GST_ERROR_OBJECT (self, "Failed to get output format metadata: %s",
        err->message);
    g_clear_error (&err);
    return FALSE;
  }

  if (rate == 0 || channels == 0) {
    GST_ERROR_OBJECT (self, "Rate or channels not set");
    return FALSE;
  }

  /* Not always present */
  gst_amc_format_get_int (format, "channel-mask", (gint *) & channel_mask,
      NULL);

  gst_amc_audio_channel_mask_to_positions (channel_mask, channels,
      self->positions);
  memcpy (to, self->positions, sizeof (to));
  gst_audio_channel_positions_to_valid_order (to, channels);
  self->needs_reorder =
      (memcmp (self->positions, to,
          sizeof (GstAudioChannelPosition) * channels) != 0);
  if (self->needs_reorder)
    gst_audio_get_channel_reorder_map (channels, self->positions, to,
        self->reorder_map);

  gst_audio_info_init (&self->info);
  gst_audio_info_set_format (&self->info, GST_AUDIO_FORMAT_S16, rate, channels,
      to);

  if (!gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (self),
          &self->info))
    return FALSE;

  self->input_caps_changed = FALSE;

  return TRUE;
}

static void
gst_amc_audio_dec_loop (GstAmcAudioDec * self)
{
  GstFlowReturn flow_ret = GST_FLOW_OK;
  gboolean is_eos;
  GstAmcBuffer *buf;
  GstAmcBufferInfo buffer_info;
  gint idx;
  GError *err = NULL;

  GST_AUDIO_DECODER_STREAM_LOCK (self);

retry:
  /*if (self->input_caps_changed) {
     idx = INFO_OUTPUT_FORMAT_CHANGED;
     } else { */
  GST_DEBUG_OBJECT (self, "Waiting for available output buffer");
  GST_AUDIO_DECODER_STREAM_UNLOCK (self);
  /* Wait at most 100ms here, some codecs don't fail dequeueing if
   * the codec is flushing, causing deadlocks during shutdown */
  idx =
      gst_amc_codec_dequeue_output_buffer (self->codec, &buffer_info, 100000,
      &err);
  GST_AUDIO_DECODER_STREAM_LOCK (self);
  /*} */

  if (idx < 0) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }

    switch (idx) {
      case INFO_OUTPUT_BUFFERS_CHANGED:
        /* Handled internally */
        g_assert_not_reached ();
        break;
      case INFO_OUTPUT_FORMAT_CHANGED:{
        GstAmcFormat *format;
        gchar *format_string;

        GST_DEBUG_OBJECT (self, "Output format has changed");

        format = gst_amc_codec_get_output_format (self->codec, &err);
        if (!format)
          goto format_error;

        format_string = gst_amc_format_to_string (format, &err);
        if (err) {
          gst_amc_format_free (format);
          goto format_error;
        }
        GST_DEBUG_OBJECT (self, "Got new output format: %s", format_string);
        g_free (format_string);

        if (!gst_amc_audio_dec_set_src_caps (self, format)) {
          gst_amc_format_free (format);
          goto format_error;
        }
        gst_amc_format_free (format);

        goto retry;

      }
      case INFO_TRY_AGAIN_LATER:
        GST_DEBUG_OBJECT (self, "Dequeueing output buffer timed out");
        goto retry;

      case G_MININT:
        GST_ERROR_OBJECT (self, "Failure dequeueing output buffer");
        goto dequeue_error;

      default:
        g_assert_not_reached ();
        break;
    }

    goto retry;
  }

  GST_DEBUG_OBJECT (self,
      "Got output buffer at index %d: offset %d size %d time %" G_GINT64_FORMAT
      " flags 0x%08x", idx, buffer_info.offset, buffer_info.size,
      buffer_info.presentation_time_us, buffer_info.flags);

  is_eos = !!(buffer_info.flags & BUFFER_FLAG_END_OF_STREAM);

  buf = gst_amc_codec_get_output_buffer (self->codec, idx, &err);
  if (err) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }
    goto failed_to_get_output_buffer;
  } else if (!buf) {
    goto got_null_output_buffer;
  }

  if (buffer_info.size > 0) {
    GstBuffer *outbuf;
    GstMapInfo minfo;

    /* This sometimes happens at EOS or if the input is not properly framed,
     * let's handle it gracefully by allocating a new buffer for the current
     * caps and filling it
     */

    if (buffer_info.size % self->info.bpf != 0)
      goto invalid_buffer_size;

    outbuf =
        gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (self),
        buffer_info.size);
    if (!outbuf)
      goto failed_allocate;

    gst_buffer_map (outbuf, &minfo, GST_MAP_WRITE);
    if (self->needs_reorder) {
      gint i, n_samples, c, n_channels;
      gint *reorder_map = self->reorder_map;
      gint16 *dest, *source;

      dest = (gint16 *) minfo.data;
      source = (gint16 *) (buf->data + buffer_info.offset);
      n_samples = buffer_info.size / self->info.bpf;
      n_channels = self->info.channels;

      for (i = 0; i < n_samples; i++) {
        for (c = 0; c < n_channels; c++) {
          dest[i * n_channels + reorder_map[c]] = source[i * n_channels + c];
        }
      }
    } else {
      orc_memcpy (minfo.data, buf->data + buffer_info.offset, buffer_info.size);
    }
    gst_buffer_unmap (outbuf, &minfo);

    if (self->spf != -1) {
      gst_adapter_push (self->output_adapter, outbuf);
    } else {
      flow_ret =
          gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (self), outbuf, 1);
    }
  }

  gst_amc_buffer_free (buf);
  buf = NULL;

  if (self->spf != -1) {
    GstBuffer *outbuf;
    guint avail = gst_adapter_available (self->output_adapter);
    guint nframes;

    /* On EOS we take the complete adapter content, no matter
     * if it is a multiple of the codec frame size or not.
     * Otherwise we take a multiple of codec frames and push
     * them downstream
     */
    avail /= self->info.bpf;
    if (!is_eos) {
      nframes = avail / self->spf;
      avail = nframes * self->spf;
    } else {
      nframes = (avail + self->spf - 1) / self->spf;
    }
    avail *= self->info.bpf;

    if (avail > 0) {
      outbuf = gst_adapter_take_buffer (self->output_adapter, avail);
      flow_ret =
          gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (self), outbuf,
          nframes);
    }
  }

  if (!gst_amc_codec_release_output_buffer (self->codec, idx, FALSE, &err)) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }
    goto failed_release;
  }

  if (is_eos || flow_ret == GST_FLOW_EOS) {
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    if (self->draining) {
      GST_DEBUG_OBJECT (self, "Drained");
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
    } else if (flow_ret == GST_FLOW_OK) {
      GST_DEBUG_OBJECT (self, "Component signalled EOS");
      flow_ret = GST_FLOW_EOS;
    }
    g_mutex_unlock (&self->drain_lock);
    GST_AUDIO_DECODER_STREAM_LOCK (self);
  } else {
    GST_DEBUG_OBJECT (self, "Finished frame: %s", gst_flow_get_name (flow_ret));
  }

  self->downstream_flow_ret = flow_ret;

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  GST_AUDIO_DECODER_STREAM_UNLOCK (self);

  return;

dequeue_error:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

format_error:
  {
    if (err)
      GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    else
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
          ("Failed to handle format"));
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
failed_release:
  {
    GST_AUDIO_DECODER_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_FLUSHING;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    return;
  }

flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");
      gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    } else if (flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_FLOW_ERROR (self, flow_ret);
      gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    } else if (flow_ret == GST_FLOW_FLUSHING) {
      GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
      gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    }
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

failed_to_get_output_buffer:
  {
    GST_AUDIO_DECODER_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

got_null_output_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Got no output buffer"));
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

invalid_buffer_size:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
        ("Invalid buffer size %u (bfp %d)", buffer_info.size, self->info.bpf));
    gst_amc_codec_release_output_buffer (self->codec, idx, FALSE, &err);
    if (err && !self->flushing)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    g_clear_error (&err);
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

failed_allocate:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Failed to allocate output buffer"));
    gst_amc_codec_release_output_buffer (self->codec, idx, FALSE, &err);
    if (err && !self->flushing)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    g_clear_error (&err);
    gst_pad_push_event (GST_AUDIO_DECODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_AUDIO_DECODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
}

static gboolean
gst_amc_audio_dec_start (GstAudioDecoder * decoder)
{
  GstAmcAudioDec *self;

  self = GST_AMC_AUDIO_DEC (decoder);
  self->last_upstream_ts = 0;
  self->drained = TRUE;
  self->downstream_flow_ret = GST_FLOW_OK;
  self->started = FALSE;
  self->flushing = TRUE;

  return TRUE;
}

static gboolean
gst_amc_audio_dec_stop (GstAudioDecoder * decoder)
{
  GstAmcAudioDec *self;
  GError *err = NULL;

  self = GST_AMC_AUDIO_DEC (decoder);
  GST_DEBUG_OBJECT (self, "Stopping decoder");
  self->flushing = TRUE;
  if (self->started) {
    gst_amc_codec_flush (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    gst_amc_codec_stop (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    self->started = FALSE;
  }
  gst_pad_stop_task (GST_AUDIO_DECODER_SRC_PAD (decoder));

  memset (self->positions, 0, sizeof (self->positions));

  gst_adapter_flush (self->output_adapter,
      gst_adapter_available (self->output_adapter));

  g_list_foreach (self->codec_datas, (GFunc) g_free, NULL);
  g_list_free (self->codec_datas);
  self->codec_datas = NULL;

  self->downstream_flow_ret = GST_FLOW_FLUSHING;
  self->drained = TRUE;
  g_mutex_lock (&self->drain_lock);
  self->draining = FALSE;
  g_cond_broadcast (&self->drain_cond);
  g_mutex_unlock (&self->drain_lock);

  GST_DEBUG_OBJECT (self, "Stopped decoder");
  return TRUE;
}

static gboolean
gst_amc_audio_dec_set_format (GstAudioDecoder * decoder, GstCaps * caps)
{
  GstAmcAudioDec *self;
  GstStructure *s;
  GstAmcFormat *format;
  const gchar *mime;
  gboolean is_format_change = FALSE;
  gboolean needs_disable = FALSE;
  gchar *format_string;
  gint rate, channels;
  GError *err = NULL;

  self = GST_AMC_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Setting new caps %" GST_PTR_FORMAT, caps);

  /* Check if the caps change is a real format change or if only irrelevant
   * parts of the caps have changed or nothing at all.
   */
  is_format_change |= (!self->input_caps
      || !gst_caps_is_equal (self->input_caps, caps));

  needs_disable = self->started;

  /* If the component is not started and a real format change happens
   * we have to restart the component. If no real format change
   * happened we can just exit here.
   */
  if (needs_disable && !is_format_change) {
    /* Framerate or something minor changed */
    self->input_caps_changed = TRUE;
    GST_DEBUG_OBJECT (self,
        "Already running and caps did not change the format");
    return TRUE;
  }

  if (needs_disable && is_format_change) {
    gst_amc_audio_dec_drain (self);
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    gst_amc_audio_dec_stop (GST_AUDIO_DECODER (self));
    GST_AUDIO_DECODER_STREAM_LOCK (self);
    gst_amc_audio_dec_close (GST_AUDIO_DECODER (self));
    if (!gst_amc_audio_dec_open (GST_AUDIO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to open codec again");
      return FALSE;
    }

    if (!gst_amc_audio_dec_start (GST_AUDIO_DECODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to start codec again");
    }
  }
  /* srcpad task is not running at this point */

  mime = caps_to_mime (caps);
  if (!mime) {
    GST_ERROR_OBJECT (self, "Failed to convert caps to mime");
    return FALSE;
  }

  s = gst_caps_get_structure (caps, 0);
  if (!gst_structure_get_int (s, "rate", &rate) ||
      !gst_structure_get_int (s, "channels", &channels)) {
    GST_ERROR_OBJECT (self, "Failed to get rate/channels");
    return FALSE;
  }

  format = gst_amc_format_new_audio (mime, rate, channels, &err);
  if (!format) {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    return FALSE;
  }

  if (gst_structure_has_name (s, "audio/mpeg")) {
    gint mpegversion;
    const gchar *stream_format;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion))
      mpegversion = -1;
    stream_format = gst_structure_get_string (s, "stream-format");

    if (mpegversion == 4 && g_strcmp0 (stream_format, "adts") == 0) {
      gst_amc_format_set_int (format, "is-adts", 1, &err);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    }
  }

  /* FIXME: These buffers needs to be valid until the codec is stopped again */
  g_list_foreach (self->codec_datas, (GFunc) gst_buffer_unref, NULL);
  g_list_free (self->codec_datas);
  self->codec_datas = NULL;
  if (gst_structure_has_field (s, "codec_data")) {
    const GValue *h = gst_structure_get_value (s, "codec_data");
    GstBuffer *codec_data = gst_value_get_buffer (h);
    GstMapInfo minfo;
    guint8 *data;

    gst_buffer_map (codec_data, &minfo, GST_MAP_READ);
    data = g_memdup2 (minfo.data, minfo.size);
    self->codec_datas = g_list_prepend (self->codec_datas, data);
    gst_amc_format_set_buffer (format, "csd-0", data, minfo.size, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    gst_buffer_unmap (codec_data, &minfo);
  } else if (gst_structure_has_field (s, "streamheader")) {
    const GValue *sh = gst_structure_get_value (s, "streamheader");
    gint nsheaders = gst_value_array_get_size (sh);
    GstBuffer *buf;
    const GValue *h;
    gint i, j;
    gchar *fname;
    GstMapInfo minfo;
    guint8 *data;

    for (i = 0, j = 0; i < nsheaders; i++) {
      h = gst_value_array_get_value (sh, i);
      buf = gst_value_get_buffer (h);

      if (strcmp (mime, "audio/vorbis") == 0) {
        guint8 header_type;

        gst_buffer_extract (buf, 0, &header_type, 1);

        /* Only use the identification and setup packets */
        if (header_type != 0x01 && header_type != 0x05)
          continue;
      }

      fname = g_strdup_printf ("csd-%d", j);
      gst_buffer_map (buf, &minfo, GST_MAP_READ);
      data = g_memdup2 (minfo.data, minfo.size);
      self->codec_datas = g_list_prepend (self->codec_datas, data);
      gst_amc_format_set_buffer (format, fname, data, minfo.size, &err);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      gst_buffer_unmap (buf, &minfo);
      g_free (fname);
      j++;
    }
  }

  format_string = gst_amc_format_to_string (format, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (self, err);
  GST_DEBUG_OBJECT (self, "Configuring codec with format: %s",
      GST_STR_NULL (format_string));
  g_free (format_string);

  if (!gst_amc_codec_configure (self->codec, format, NULL, &err)) {
    GST_ERROR_OBJECT (self, "Failed to configure codec");
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    return FALSE;
  }

  gst_amc_format_free (format);

  if (!gst_amc_codec_start (self->codec, &err)) {
    GST_ERROR_OBJECT (self, "Failed to start codec");
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    return FALSE;
  }

  self->spf = -1;
  /* TODO: Implement for other codecs too */
  if (gst_structure_has_name (s, "audio/mpeg")) {
    gint mpegversion = -1;

    gst_structure_get_int (s, "mpegversion", &mpegversion);
    if (mpegversion == 1) {
      gint layer = -1, mpegaudioversion = -1;

      gst_structure_get_int (s, "layer", &layer);
      gst_structure_get_int (s, "mpegaudioversion", &mpegaudioversion);
      if (layer == 1)
        self->spf = 384;
      else if (layer == 2)
        self->spf = 1152;
      else if (layer == 3 && mpegaudioversion != -1)
        self->spf = (mpegaudioversion == 1 ? 1152 : 576);
    }
  }

  self->started = TRUE;
  self->input_caps_changed = TRUE;

  /* Start the srcpad loop again */
  self->flushing = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_AUDIO_DECODER_SRC_PAD (self),
      (GstTaskFunction) gst_amc_audio_dec_loop, decoder, NULL);

  return TRUE;
}

static void
gst_amc_audio_dec_flush (GstAudioDecoder * decoder, gboolean hard)
{
  GstAmcAudioDec *self;
  GError *err = NULL;

  self = GST_AMC_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Resetting decoder");

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Codec not started yet");
    return;
  }

  self->flushing = TRUE;
  /* Wait until the srcpad loop is finished,
   * unlock GST_AUDIO_DECODER_STREAM_LOCK to prevent deadlocks
   * caused by using this lock from inside the loop function */
  GST_AUDIO_DECODER_STREAM_UNLOCK (self);
  GST_PAD_STREAM_LOCK (GST_AUDIO_DECODER_SRC_PAD (self));
  GST_PAD_STREAM_UNLOCK (GST_AUDIO_DECODER_SRC_PAD (self));
  GST_AUDIO_DECODER_STREAM_LOCK (self);
  gst_amc_codec_flush (self->codec, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (self, err);
  gst_adapter_flush (self->output_adapter,
      gst_adapter_available (self->output_adapter));
  self->flushing = FALSE;

  /* Start the srcpad loop again */
  self->last_upstream_ts = 0;
  self->drained = TRUE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_AUDIO_DECODER_SRC_PAD (self),
      (GstTaskFunction) gst_amc_audio_dec_loop, decoder, NULL);

  GST_DEBUG_OBJECT (self, "Reset decoder");
}

static GstFlowReturn
gst_amc_audio_dec_handle_frame (GstAudioDecoder * decoder, GstBuffer * inbuf)
{
  GstAmcAudioDec *self;
  gint idx;
  GstAmcBuffer *buf;
  GstAmcBufferInfo buffer_info;
  guint offset = 0;
  GstClockTime timestamp, duration, timestamp_offset = 0;
  GstMapInfo minfo;
  GError *err = NULL;

  memset (&minfo, 0, sizeof (minfo));

  self = GST_AMC_AUDIO_DEC (decoder);

  GST_DEBUG_OBJECT (self, "Handling frame");

  /* Make sure to keep a reference to the input here,
   * it can be unreffed from the other thread if
   * finish_frame() is called */
  if (inbuf)
    inbuf = gst_buffer_ref (inbuf);

  if (!self->started) {
    GST_ERROR_OBJECT (self, "Codec not started yet");
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (self->flushing)
    goto flushing;

  if (self->downstream_flow_ret != GST_FLOW_OK)
    goto downstream_error;

  if (!inbuf)
    return gst_amc_audio_dec_drain (self);

  timestamp = GST_BUFFER_PTS (inbuf);
  duration = GST_BUFFER_DURATION (inbuf);

  gst_buffer_map (inbuf, &minfo, GST_MAP_READ);

  while (offset < minfo.size) {
    /* Make sure to release the base class stream lock, otherwise
     * _loop() can't call _finish_frame() and we might block forever
     * because no input buffers are released */
    GST_AUDIO_DECODER_STREAM_UNLOCK (self);
    /* Wait at most 100ms here, some codecs don't fail dequeueing if
     * the codec is flushing, causing deadlocks during shutdown */
    idx = gst_amc_codec_dequeue_input_buffer (self->codec, 100000, &err);
    GST_AUDIO_DECODER_STREAM_LOCK (self);

    if (idx < 0) {
      if (self->flushing || self->downstream_flow_ret == GST_FLOW_FLUSHING) {
        g_clear_error (&err);
        goto flushing;
      }

      switch (idx) {
        case INFO_TRY_AGAIN_LATER:
          GST_DEBUG_OBJECT (self, "Dequeueing input buffer timed out");
          continue;             /* next try */
          break;
        case G_MININT:
          GST_ERROR_OBJECT (self, "Failed to dequeue input buffer");
          goto dequeue_error;
        default:
          g_assert_not_reached ();
          break;
      }

      continue;
    }

    if (self->flushing) {
      memset (&buffer_info, 0, sizeof (buffer_info));
      gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, NULL);
      goto flushing;
    }

    if (self->downstream_flow_ret != GST_FLOW_OK) {
      memset (&buffer_info, 0, sizeof (buffer_info));
      gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, &err);
      if (err && !self->flushing)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      g_clear_error (&err);
      goto downstream_error;
    }

    /* Now handle the frame */

    /* Copy the buffer content in chunks of size as requested
     * by the port */
    buf = gst_amc_codec_get_input_buffer (self->codec, idx, &err);
    if (err)
      goto failed_to_get_input_buffer;
    else if (!buf)
      goto got_null_input_buffer;

    memset (&buffer_info, 0, sizeof (buffer_info));
    buffer_info.offset = 0;
    buffer_info.size = MIN (minfo.size - offset, buf->size);
    gst_amc_buffer_set_position_and_limit (buf, NULL, buffer_info.offset,
        buffer_info.size);

    orc_memcpy (buf->data, minfo.data + offset, buffer_info.size);

    gst_amc_buffer_free (buf);
    buf = NULL;

    /* Interpolate timestamps if we're passing the buffer
     * in multiple chunks */
    if (offset != 0 && duration != GST_CLOCK_TIME_NONE) {
      timestamp_offset = gst_util_uint64_scale (offset, duration, minfo.size);
    }

    if (timestamp != GST_CLOCK_TIME_NONE) {
      buffer_info.presentation_time_us =
          gst_util_uint64_scale (timestamp + timestamp_offset, 1, GST_USECOND);
      self->last_upstream_ts = timestamp + timestamp_offset;
    }
    if (duration != GST_CLOCK_TIME_NONE)
      self->last_upstream_ts += duration;

    if (offset == 0) {
      if (!GST_BUFFER_FLAG_IS_SET (inbuf, GST_BUFFER_FLAG_DELTA_UNIT))
        buffer_info.flags |= BUFFER_FLAG_SYNC_FRAME;
    }

    offset += buffer_info.size;
    GST_DEBUG_OBJECT (self,
        "Queueing buffer %d: size %d time %" G_GINT64_FORMAT " flags 0x%08x",
        idx, buffer_info.size, buffer_info.presentation_time_us,
        buffer_info.flags);
    if (!gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info,
            &err)) {
      if (self->flushing) {
        g_clear_error (&err);
        goto flushing;
      }
      goto queue_error;
    }
    self->drained = FALSE;
  }
  gst_buffer_unmap (inbuf, &minfo);
  gst_buffer_unref (inbuf);

  return self->downstream_flow_ret;

downstream_error:
  {
    GST_ERROR_OBJECT (self, "Downstream returned %s",
        gst_flow_get_name (self->downstream_flow_ret));
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return self->downstream_flow_ret;
  }
failed_to_get_input_buffer:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }
got_null_input_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Got no input buffer"));
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }
dequeue_error:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }
queue_error:
  {
    GST_AUDIO_DECODER_ERROR_FROM_ERROR (self, err);
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- returning FLUSHING");
    if (minfo.data)
      gst_buffer_unmap (inbuf, &minfo);
    if (inbuf)
      gst_buffer_unref (inbuf);
    return GST_FLOW_FLUSHING;
  }
}

static GstFlowReturn
gst_amc_audio_dec_drain (GstAmcAudioDec * self)
{
  GstFlowReturn ret;
  gint idx;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Draining codec");
  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Codec not started yet");
    return GST_FLOW_OK;
  }

  /* Don't send drain buffer twice, this doesn't work */
  if (self->drained) {
    GST_DEBUG_OBJECT (self, "Codec is drained already");
    return GST_FLOW_OK;
  }

  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_AUDIO_DECODER_STREAM_UNLOCK (self);
  /* Send an EOS buffer to the component and let the base
   * class drop the EOS event. We will send it later when
   * the EOS buffer arrives on the output port.
   * Wait at most 0.5s here. */
  idx = gst_amc_codec_dequeue_input_buffer (self->codec, 500000, &err);
  GST_AUDIO_DECODER_STREAM_LOCK (self);

  if (idx >= 0) {
    GstAmcBuffer *buf;
    GstAmcBufferInfo buffer_info;

    buf = gst_amc_codec_get_input_buffer (self->codec, idx, &err);
    if (buf) {
      GST_AUDIO_DECODER_STREAM_UNLOCK (self);
      g_mutex_lock (&self->drain_lock);
      self->draining = TRUE;

      memset (&buffer_info, 0, sizeof (buffer_info));
      buffer_info.size = 0;
      buffer_info.presentation_time_us =
          gst_util_uint64_scale (self->last_upstream_ts, 1, GST_USECOND);
      buffer_info.flags |= BUFFER_FLAG_END_OF_STREAM;

      gst_amc_buffer_set_position_and_limit (buf, NULL, 0, 0);
      gst_amc_buffer_free (buf);
      buf = NULL;

      if (gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info,
              &err)) {
        GST_DEBUG_OBJECT (self, "Waiting until codec is drained");
        g_cond_wait (&self->drain_cond, &self->drain_lock);
        GST_DEBUG_OBJECT (self, "Drained codec");
        ret = GST_FLOW_OK;
      } else {
        GST_ERROR_OBJECT (self, "Failed to queue input buffer");
        if (self->flushing) {
          g_clear_error (&err);
          ret = GST_FLOW_FLUSHING;
        } else {
          GST_ELEMENT_WARNING_FROM_ERROR (self, err);
          ret = GST_FLOW_ERROR;
        }
      }

      self->drained = TRUE;
      self->draining = FALSE;
      g_mutex_unlock (&self->drain_lock);
      GST_AUDIO_DECODER_STREAM_LOCK (self);
    } else {
      GST_ERROR_OBJECT (self, "Failed to get buffer for EOS: %d", idx);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      ret = GST_FLOW_ERROR;
    }
  } else {
    GST_ERROR_OBJECT (self, "Failed to acquire buffer for EOS: %d", idx);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    ret = GST_FLOW_ERROR;
  }

  gst_adapter_flush (self->output_adapter,
      gst_adapter_available (self->output_adapter));

  return ret;
}
