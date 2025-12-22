/*
 * GStreamer HEVC/H.265 video codec.
 *
 * Copyright (c) 2014 struktur AG, Joachim Bauch <bauch@struktur.de>
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
 * SECTION:element-libde265dec
 * @title: libde265dec
 *
 * Decodes HEVC/H.265 video.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location=bitstream.hevc ! 'video/x-hevc,stream-format=byte-stream,framerate=25/1' ! libde265dec ! autovideosink
 * ]| The above pipeline decodes the HEVC/H.265 bitstream and renders it to the screen.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "libde265-dec.h"

/* use two decoder threads if no information about
 * available CPU cores can be retrieved */
#define DEFAULT_THREAD_COUNT        2

#define parent_class gst_libde265_dec_parent_class
G_DEFINE_TYPE (GstLibde265Dec, gst_libde265_dec, GST_TYPE_VIDEO_DECODER);
GST_ELEMENT_REGISTER_DEFINE (libde265dec, "libde265dec",
    GST_RANK_SECONDARY, GST_TYPE_LIBDE265_DEC);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("video/x-h265, stream-format=(string) { hvc1, hev1, byte-stream }, "
        "alignment=(string) { au, nal }, "
        /* TODO: Can support more profiles by adding the according
           output formats in src caps. */
        "profile=(string) main")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("I420"))
    );

enum
{
  PROP_0,
  PROP_MAX_THREADS,
  PROP_LAST
};

#define DEFAULT_FORMAT      GST_TYPE_LIBDE265_FORMAT_PACKETIZED
#define DEFAULT_MAX_THREADS 0

static void gst_libde265_dec_finalize (GObject * object);

static void gst_libde265_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_libde265_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_libde265_dec_start (GstVideoDecoder * decoder);
static gboolean gst_libde265_dec_stop (GstVideoDecoder * decoder);
static gboolean gst_libde265_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_libde265_dec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_libde265_dec_finish (GstVideoDecoder * decoder);
static GstFlowReturn _gst_libde265_return_image (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, const struct de265_image *img);
static GstFlowReturn gst_libde265_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn _gst_libde265_image_available (GstVideoDecoder * decoder,
    int width, int height);

static void
gst_libde265_dec_class_init (GstLibde265DecClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstVideoDecoderClass *decoder_class = GST_VIDEO_DECODER_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->finalize = gst_libde265_dec_finalize;
  gobject_class->set_property = gst_libde265_dec_set_property;
  gobject_class->get_property = gst_libde265_dec_get_property;

  g_object_class_install_property (gobject_class, PROP_MAX_THREADS,
      g_param_spec_int ("max-threads", "Maximum decode threads",
          "Maximum number of worker threads to spawn. (0 = auto)",
          0, G_MAXINT, DEFAULT_MAX_THREADS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  decoder_class->start = GST_DEBUG_FUNCPTR (gst_libde265_dec_start);
  decoder_class->stop = GST_DEBUG_FUNCPTR (gst_libde265_dec_stop);
  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_libde265_dec_set_format);
  decoder_class->flush = GST_DEBUG_FUNCPTR (gst_libde265_dec_flush);
  decoder_class->finish = GST_DEBUG_FUNCPTR (gst_libde265_dec_finish);
  decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_libde265_dec_handle_frame);

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class,
      "HEVC/H.265 decoder",
      "Codec/Decoder/Video",
      "Decodes HEVC/H.265 video streams using libde265",
      "struktur AG <opensource@struktur.de>");
}

static inline void
_gst_libde265_dec_reset_decoder (GstLibde265Dec * dec)
{
  dec->ctx = NULL;
  dec->buffer_full = 0;
  dec->codec_data = NULL;
  dec->codec_data_size = 0;
  dec->input_state = NULL;
  dec->output_state = NULL;
}

static void
gst_libde265_dec_init (GstLibde265Dec * dec)
{
  dec->format = DEFAULT_FORMAT;
  dec->max_threads = DEFAULT_MAX_THREADS;
  dec->length_size = 4;
  _gst_libde265_dec_reset_decoder (dec);
  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (dec), TRUE);
  gst_video_decoder_set_use_default_pad_acceptcaps (GST_VIDEO_DECODER_CAST
      (dec), TRUE);
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_DECODER_SINK_PAD (dec));
}

static inline void
_gst_libde265_dec_free_decoder (GstLibde265Dec * dec)
{
  if (dec->ctx != NULL) {
    de265_free_decoder (dec->ctx);
  }
  free (dec->codec_data);
  if (dec->input_state != NULL) {
    gst_video_codec_state_unref (dec->input_state);
  }
  if (dec->output_state != NULL) {
    gst_video_codec_state_unref (dec->output_state);
  }
  _gst_libde265_dec_reset_decoder (dec);
}

static void
gst_libde265_dec_finalize (GObject * object)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (object);

  _gst_libde265_dec_free_decoder (dec);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_libde265_dec_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (object);

  switch (prop_id) {
    case PROP_MAX_THREADS:
      dec->max_threads = g_value_get_int (value);
      if (dec->max_threads) {
        GST_DEBUG_OBJECT (dec, "Max. threads set to %d", dec->max_threads);
      } else {
        GST_DEBUG_OBJECT (dec, "Max. threads set to auto");
      }
      break;
    default:
      break;
  }
}

static void
gst_libde265_dec_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (object);

  switch (prop_id) {
    case PROP_MAX_THREADS:
      g_value_set_int (value, dec->max_threads);
      break;
    default:
      break;
  }
}

struct GstLibde265FrameRef
{
  GstVideoDecoder *decoder;
  GstVideoCodecFrame *frame;
  GstVideoFrame vframe;
  GstBuffer *buffer;
  gboolean mapped;
};

static void
gst_libde265_dec_release_frame_ref (struct GstLibde265FrameRef *ref)
{
  if (ref->mapped) {
    gst_video_frame_unmap (&ref->vframe);
  }
  gst_video_codec_frame_unref (ref->frame);
  gst_buffer_replace (&ref->buffer, NULL);
  g_free (ref);
}

static int
gst_libde265_dec_get_buffer (de265_decoder_context * ctx,
    struct de265_image_spec *spec, struct de265_image *img, void *userdata)
{
  GstVideoDecoder *base = (GstVideoDecoder *) userdata;
  GstLibde265Dec *dec = GST_LIBDE265_DEC (base);
  GstVideoCodecFrame *frame = NULL;
  int i;
  int width = spec->width;
  int height = spec->height;
  GstFlowReturn ret;
  struct GstLibde265FrameRef *ref;
  GstVideoInfo *info;
  int frame_number;

  frame_number = (uintptr_t) de265_get_image_user_data (img) - 1;
  if (G_UNLIKELY (frame_number == -1)) {
    /* should not happen... */
    GST_WARNING_OBJECT (base, "Frame has no number assigned!");
    goto fallback;
  }

  frame = gst_video_decoder_get_frame (base, frame_number);
  if (G_UNLIKELY (frame == NULL)) {
    /* should not happen... */
    GST_WARNING_OBJECT (base, "Couldn't get codec frame!");
    goto fallback;
  }

  if (width % spec->alignment) {
    width += spec->alignment - (width % spec->alignment);
  }
  if (width != spec->visible_width || height != spec->visible_height) {
    /* clipping not supported for now */
    goto fallback;
  }

  ret = _gst_libde265_image_available (base, width, height);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_ERROR_OBJECT (dec, "Failed to notify about available image");
    goto fallback;
  }

  ret =
      gst_video_decoder_allocate_output_frame (GST_VIDEO_DECODER (dec), frame);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_ERROR_OBJECT (dec, "Failed to allocate output buffer");
    goto fallback;
  }

  ref = (struct GstLibde265FrameRef *) g_malloc0 (sizeof (*ref));
  g_assert (ref != NULL);
  ref->decoder = base;
  ref->frame = frame;

  gst_buffer_replace (&ref->buffer, frame->output_buffer);
  gst_buffer_replace (&frame->output_buffer, NULL);

  info = &dec->output_state->info;
  if (!gst_video_frame_map (&ref->vframe, info, ref->buffer, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (dec, "Failed to map frame output buffer");
    goto error;
  }

  ref->mapped = TRUE;
  if (GST_VIDEO_FRAME_PLANE_STRIDE (&ref->vframe,
          0) < width * GST_VIDEO_FRAME_COMP_PSTRIDE (&ref->vframe, 0)) {
    GST_DEBUG_OBJECT (dec, "plane 0: pitch too small (%d/%d*%d)",
        GST_VIDEO_FRAME_PLANE_STRIDE (&ref->vframe, 0), width,
        GST_VIDEO_FRAME_COMP_PSTRIDE (&ref->vframe, 0));
    goto error;
  }

  if (GST_VIDEO_FRAME_COMP_HEIGHT (&ref->vframe, 0) < height) {
    GST_DEBUG_OBJECT (dec, "plane 0: lines too few (%d/%d)",
        GST_VIDEO_FRAME_COMP_HEIGHT (&ref->vframe, 0), height);
    goto error;
  }

  for (i = 0; i < 3; i++) {
    uint8_t *data;
    int stride = GST_VIDEO_FRAME_PLANE_STRIDE (&ref->vframe, i);
    if (stride % spec->alignment) {
      GST_DEBUG_OBJECT (dec, "plane %d: pitch not aligned (%d%%%d)",
          i, stride, spec->alignment);
      goto error;
    }

    data = GST_VIDEO_FRAME_PLANE_DATA (&ref->vframe, i);
    if ((uintptr_t) (data) % spec->alignment) {
      GST_DEBUG_OBJECT (dec, "plane %d not aligned", i);
      goto error;
    }

    de265_set_image_plane (img, i, data, stride, ref);
  }
  return 1;

error:
  gst_libde265_dec_release_frame_ref (ref);
  frame = NULL;

fallback:
  if (frame != NULL) {
    gst_video_codec_frame_unref (frame);
  }
  return de265_get_default_image_allocation_functions ()->get_buffer (ctx,
      spec, img, userdata);
}

static void
gst_libde265_dec_release_buffer (de265_decoder_context * ctx,
    struct de265_image *img, void *userdata)
{
  GstVideoDecoder *base = (GstVideoDecoder *) userdata;
  struct GstLibde265FrameRef *ref =
      (struct GstLibde265FrameRef *) de265_get_image_plane_user_data (img, 0);
  if (ref == NULL) {
    de265_get_default_image_allocation_functions ()->release_buffer (ctx, img,
        userdata);
    return;
  }
  gst_libde265_dec_release_frame_ref (ref);
  (void) base;                  /* unused */
}

static gboolean
gst_libde265_dec_start (GstVideoDecoder * decoder)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);
  int threads = dec->max_threads;
  struct de265_image_allocation allocation;

  _gst_libde265_dec_free_decoder (dec);
  dec->ctx = de265_new_decoder ();
  if (dec->ctx == NULL) {
    return FALSE;
  }
  if (threads == 0) {
    threads = g_get_num_processors ();

    /* NOTE: We start more threads than cores for now, as some threads
     * might get blocked while waiting for dependent data. Having more
     * threads increases decoding speed by about 10% */
    threads *= 2;
  }
  if (threads > 1) {
    if (threads > 32) {
      /* TODO: this limit should come from the libde265 headers */
      threads = 32;
    }
    de265_start_worker_threads (dec->ctx, threads);
  }
  GST_INFO_OBJECT (dec, "Using libde265 %s with %d worker threads",
      de265_get_version (), threads);

  allocation.get_buffer = gst_libde265_dec_get_buffer;
  allocation.release_buffer = gst_libde265_dec_release_buffer;
  de265_set_image_allocation_functions (dec->ctx, &allocation, decoder);
  /* NOTE: we explicitly disable hash checks for now */
  de265_set_parameter_bool (dec->ctx, DE265_DECODER_PARAM_BOOL_SEI_CHECK_HASH,
      0);
  return TRUE;
}

static gboolean
gst_libde265_dec_stop (GstVideoDecoder * decoder)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);

  _gst_libde265_dec_free_decoder (dec);

  return TRUE;
}

static gboolean
gst_libde265_dec_flush (GstVideoDecoder * decoder)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);

  de265_reset (dec->ctx);
  dec->buffer_full = 0;
  if (dec->codec_data != NULL
      && dec->format == GST_TYPE_LIBDE265_FORMAT_BYTESTREAM) {
    int more;
    de265_error err =
        de265_push_data (dec->ctx, dec->codec_data, dec->codec_data_size, 0,
        NULL);
    if (!de265_isOK (err)) {
      GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
          ("Failed to push codec data: %s (code=%d)",
              de265_get_error_text (err), err), (NULL));
      return FALSE;
    }
    de265_push_end_of_NAL (dec->ctx);
    do {
      err = de265_decode (dec->ctx, &more);
      switch (err) {
        case DE265_OK:
          break;

        case DE265_ERROR_IMAGE_BUFFER_FULL:
        case DE265_ERROR_WAITING_FOR_INPUT_DATA:
          /* not really an error */
          more = 0;
          break;

        default:
          if (!de265_isOK (err)) {
            GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                ("Failed to decode codec data: %s (code=%d)",
                    de265_get_error_text (err), err), (NULL));
            return FALSE;
          }
      }
    } while (more);
  }

  return TRUE;
}

static GstFlowReturn
gst_libde265_dec_finish (GstVideoDecoder * decoder)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);
  de265_error err;
  const struct de265_image *img;
  int more;
  GstFlowReturn result;

  err = de265_flush_data (dec->ctx);
  if (!de265_isOK (err)) {
    GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
        ("Failed to flush decoder: %s (code=%d)",
            de265_get_error_text (err), err), (NULL));
    return GST_FLOW_ERROR;
  }

  do {
    err = de265_decode (dec->ctx, &more);
    switch (err) {
      case DE265_OK:
      case DE265_ERROR_IMAGE_BUFFER_FULL:
        img = de265_get_next_picture (dec->ctx);
        if (img != NULL) {
          result = _gst_libde265_return_image (decoder, NULL, img);
          if (result != GST_FLOW_OK) {
            return result;
          }
        }
        break;

      case DE265_ERROR_WAITING_FOR_INPUT_DATA:
        /* not really an error */
        more = 0;
        break;

      default:
        if (!de265_isOK (err)) {
          GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
              ("Failed to decode codec data: %s (code=%d)",
                  de265_get_error_text (err), err), (NULL));
          return FALSE;
        }
    }
  } while (more);

  return GST_FLOW_OK;
}

static GstFlowReturn
_gst_libde265_image_available (GstVideoDecoder * decoder, int width, int height)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);

  if (G_UNLIKELY (dec->output_state == NULL
          || width != dec->output_state->info.width
          || height != dec->output_state->info.height)) {
    GstVideoCodecState *state =
        gst_video_decoder_set_output_state (decoder, GST_VIDEO_FORMAT_I420,
        width, height, dec->input_state);
    if (state == NULL) {
      GST_ERROR_OBJECT (dec, "Failed to set output state");
      return GST_FLOW_ERROR;
    }
    if (!gst_video_decoder_negotiate (decoder)) {
      GST_ERROR_OBJECT (dec, "Failed to negotiate format");
      gst_video_codec_state_unref (state);
      return GST_FLOW_ERROR;
    }
    if (dec->output_state != NULL) {
      gst_video_codec_state_unref (dec->output_state);
    }
    dec->output_state = state;
    GST_DEBUG_OBJECT (dec, "Frame dimensions are %d x %d", width, height);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_libde265_dec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);

  if (dec->input_state != NULL) {
    gst_video_codec_state_unref (dec->input_state);
  }
  dec->input_state = state;
  if (state != NULL) {
    gst_video_codec_state_ref (state);
  }
  if (state != NULL && state->caps != NULL) {
    GstStructure *str;
    const GValue *value;
    str = gst_caps_get_structure (state->caps, 0);
    if ((value = gst_structure_get_value (str, "codec_data"))) {
      GstMapInfo info;
      guint8 *data;
      gsize size;
      GstBuffer *buf;
      de265_error err;
      int more;

      buf = gst_value_get_buffer (value);
      if (!gst_buffer_map (buf, &info, GST_MAP_READ)) {
        GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
            ("Failed to map codec data"), (NULL));
        return FALSE;
      }
      data = info.data;
      size = info.size;
      free (dec->codec_data);
      dec->codec_data = malloc (size);
      g_assert (dec->codec_data != NULL);
      dec->codec_data_size = size;
      memcpy (dec->codec_data, data, size);
      if (size > 3 && (data[0] || data[1] || data[2] > 1)) {
        /* encoded in "hvcC" format (assume version 0) */
        dec->format = GST_TYPE_LIBDE265_FORMAT_PACKETIZED;
        if (size > 22) {
          int i;
          int num_param_sets;
          int pos;
          if (data[0] != 0) {
            GST_ELEMENT_WARNING (decoder, STREAM,
                DECODE, ("Unsupported extra data version %d, decoding may fail",
                    data[0]), (NULL));
          }
          dec->length_size = (data[21] & 3) + 1;
          num_param_sets = data[22];
          pos = 23;
          for (i = 0; i < num_param_sets; i++) {
            int j;
            int nal_count;
            if (pos + 3 > size) {
              GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                  ("Buffer underrun in extra header (%d >= %" G_GSIZE_FORMAT
                      ")", pos + 3, size), (NULL));
              return FALSE;
            }
            /* ignore flags + NAL type (1 byte) */
            nal_count = data[pos + 1] << 8 | data[pos + 2];
            pos += 3;
            for (j = 0; j < nal_count; j++) {
              int nal_size;
              if (pos + 2 > size) {
                GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                    ("Buffer underrun in extra nal header (%d >= %"
                        G_GSIZE_FORMAT ")", pos + 2, size), (NULL));
                return FALSE;
              }
              nal_size = data[pos] << 8 | data[pos + 1];
              if (pos + 2 + nal_size > size) {
                GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                    ("Buffer underrun in extra nal (%d >= %" G_GSIZE_FORMAT ")",
                        pos + 2 + nal_size, size), (NULL));
                return FALSE;
              }
              err =
                  de265_push_NAL (dec->ctx, data + pos + 2, nal_size, 0, NULL);
              if (!de265_isOK (err)) {
                GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                    ("Failed to push data: %s (%d)", de265_get_error_text (err),
                        err), (NULL));
                return FALSE;
              }
              pos += 2 + nal_size;
            }
          }
        }
        GST_DEBUG ("Assuming packetized data (%d bytes length)",
            dec->length_size);
      } else {
        dec->format = GST_TYPE_LIBDE265_FORMAT_BYTESTREAM;
        GST_DEBUG_OBJECT (dec, "Assuming non-packetized data");
        err = de265_push_data (dec->ctx, data, size, 0, NULL);
        if (!de265_isOK (err)) {
          gst_buffer_unmap (buf, &info);
          GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
              ("Failed to push codec data: %s (code=%d)",
                  de265_get_error_text (err), err), (NULL));
          return FALSE;
        }
      }
      gst_buffer_unmap (buf, &info);
      de265_push_end_of_NAL (dec->ctx);
      do {
        err = de265_decode (dec->ctx, &more);
        switch (err) {
          case DE265_OK:
            break;

          case DE265_ERROR_IMAGE_BUFFER_FULL:
          case DE265_ERROR_WAITING_FOR_INPUT_DATA:
            /* not really an error */
            more = 0;
            break;

          default:
            if (!de265_isOK (err)) {
              GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
                  ("Failed to decode codec data: %s (code=%d)",
                      de265_get_error_text (err), err), (NULL));
              return FALSE;
            }
        }
      } while (more);
    } else if ((value = gst_structure_get_value (str, "stream-format"))) {
      const gchar *str = g_value_get_string (value);
      if (strcmp (str, "byte-stream") == 0) {
        dec->format = GST_TYPE_LIBDE265_FORMAT_BYTESTREAM;
        GST_DEBUG_OBJECT (dec, "Assuming raw byte-stream");
      }
    }
  }

  return TRUE;
}

static GstFlowReturn
_gst_libde265_return_image (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame, const struct de265_image *img)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);
  struct GstLibde265FrameRef *ref;
  GstFlowReturn result;
  GstVideoFrame outframe;
  GstVideoCodecFrame *out_frame;
  int frame_number;
  int plane;

  ref = (struct GstLibde265FrameRef *) de265_get_image_plane_user_data (img, 0);
  if (ref != NULL) {
    /* decoder is using direct rendering */
    out_frame = gst_video_codec_frame_ref (ref->frame);
    if (frame != NULL) {
      gst_video_codec_frame_unref (frame);
    }
    gst_buffer_replace (&out_frame->output_buffer, ref->buffer);
    gst_buffer_replace (&ref->buffer, NULL);
    return gst_video_decoder_finish_frame (decoder, out_frame);
  }

  result =
      _gst_libde265_image_available (decoder, de265_get_image_width (img, 0),
      de265_get_image_height (img, 0));
  if (result != GST_FLOW_OK) {
    GST_ERROR_OBJECT (dec, "Failed to notify about available image");
    return result;
  }

  frame_number = (uintptr_t) de265_get_image_user_data (img) - 1;
  if (frame_number != -1) {
    out_frame = gst_video_decoder_get_frame (decoder, frame_number);
  } else {
    out_frame = NULL;
  }
  if (frame != NULL) {
    gst_video_codec_frame_unref (frame);
  }

  if (out_frame == NULL) {
    GST_ERROR_OBJECT (dec, "No frame available to return");
    return GST_FLOW_ERROR;
  }

  result = gst_video_decoder_allocate_output_frame (decoder, out_frame);
  if (result != GST_FLOW_OK) {
    GST_ERROR_OBJECT (dec, "Failed to allocate output frame");
    return result;
  }

  g_assert (dec->output_state != NULL);
  if (!gst_video_frame_map (&outframe, &dec->output_state->info,
          out_frame->output_buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (dec, "Failed to map output buffer");
    return GST_FLOW_ERROR;
  }

  for (plane = 0; plane < 3; plane++) {
    int width = de265_get_image_width (img, plane);
    int height = de265_get_image_height (img, plane);
    int srcstride = width;
    int dststride = GST_VIDEO_FRAME_COMP_STRIDE (&outframe, plane);
    const uint8_t *src = de265_get_image_plane (img, plane, &srcstride);
    uint8_t *dest = GST_VIDEO_FRAME_COMP_DATA (&outframe, plane);
    if (srcstride == width && dststride == width) {
      memcpy (dest, src, height * width);
    } else {
      while (height--) {
        memcpy (dest, src, width);
        src += srcstride;
        dest += dststride;
      }
    }
  }
  gst_video_frame_unmap (&outframe);
  return gst_video_decoder_finish_frame (decoder, out_frame);
}

static GstFlowReturn
gst_libde265_dec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstLibde265Dec *dec = GST_LIBDE265_DEC (decoder);
  uint8_t *frame_data;
  uint8_t *end_data;
  const struct de265_image *img;
  de265_error ret = DE265_OK;
  int more = 0;
  GstClockTime pts;
  gsize size;
  GstMapInfo info;

  pts = frame->pts;
  if (pts == GST_CLOCK_TIME_NONE) {
    pts = frame->dts;
  }

  if (!gst_buffer_map (frame->input_buffer, &info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (dec, "Failed to map input buffer");
    return GST_FLOW_ERROR;
  }

  frame_data = info.data;
  size = info.size;
  end_data = frame_data + size;

  if (size > 0) {
    if (dec->format == GST_TYPE_LIBDE265_FORMAT_PACKETIZED) {
      /* stream contains length fields and NALs */
      uint8_t *start_data = frame_data;
      while (start_data + dec->length_size <= end_data) {
        int nal_size = 0;
        int i;
        for (i = 0; i < dec->length_size; i++) {
          nal_size = (nal_size << 8) | start_data[i];
        }
        if (start_data + dec->length_size + nal_size > end_data) {
          GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
              ("Overflow in input data, check stream format"), (NULL));
          goto error_input;
        }
        ret =
            de265_push_NAL (dec->ctx, start_data + dec->length_size, nal_size,
            (de265_PTS) pts,
            (void *) (uintptr_t) (frame->system_frame_number + 1));
        if (ret != DE265_OK) {
          GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
              ("Error while pushing data: %s (code=%d)",
                  de265_get_error_text (ret), ret), (NULL));
          goto error_input;
        }
        start_data += dec->length_size + nal_size;
      }
    } else {
      ret =
          de265_push_data (dec->ctx, frame_data, size, (de265_PTS) pts,
          (void *) (uintptr_t) (frame->system_frame_number + 1));
      if (ret != DE265_OK) {
        GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
            ("Error while pushing data: %s (code=%d)",
                de265_get_error_text (ret), ret), (NULL));
        goto error_input;
      }
    }
  } else {
    ret = de265_flush_data (dec->ctx);
    if (ret != DE265_OK) {
      GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
          ("Error while flushing data: %s (code=%d)",
              de265_get_error_text (ret), ret), (NULL));
      goto error_input;
    }
  }
  gst_buffer_unmap (frame->input_buffer, &info);

  /* decode as much as possible */
  do {
    ret = de265_decode (dec->ctx, &more);
  } while (more && ret == DE265_OK);

  switch (ret) {
    case DE265_OK:
    case DE265_ERROR_WAITING_FOR_INPUT_DATA:
      break;

    case DE265_ERROR_IMAGE_BUFFER_FULL:
      dec->buffer_full = 1;
      if ((img = de265_peek_next_picture (dec->ctx)) == NULL) {
        return GST_FLOW_OK;
      }
      break;

    default:
      GST_ELEMENT_ERROR (decoder, STREAM, DECODE,
          ("Error while decoding: %s (code=%d)", de265_get_error_text (ret),
              ret), (NULL));
      return GST_FLOW_ERROR;
  }

  while ((ret = de265_get_warning (dec->ctx)) != DE265_OK) {
    GST_ELEMENT_WARNING (decoder, STREAM, DECODE,
        ("%s (code=%d)", de265_get_error_text (ret), ret), (NULL));
  }

  img = de265_get_next_picture (dec->ctx);
  if (img == NULL) {
    /* need more data */
    return GST_FLOW_OK;
  }

  return _gst_libde265_return_image (decoder, frame, img);

error_input:
  gst_buffer_unmap (frame->input_buffer, &info);
  return GST_FLOW_ERROR;
}
