/* GStreamer
 * Copyright (C) 2010, 2013 Ole André Vadla Ravnås <oleavr@soundrop.com>
 * Copyright (C) 2012-2016 Alessandro Decina <alessandro.d@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-vtdec
 * @title: vtdec
 *
 * Apple VideoToolbox based decoder which might use a HW or a SW
 * implementation depending on the device.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=file.mov ! qtdemux ! queue ! h264parse ! vtdec ! videoconvert ! autovideosink
 * ]|
 * Decode h264 video from a mov file.
 *
 */

/**
 * SECTION:element-vtdec_hw
 * @title: vtdec_hw
 *
 * Apple VideoToolbox based HW-only decoder.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=file.mov ! qtdemux ! queue ! h264parse ! vtdec_hw ! videoconvert ! autovideosink
 * ]|
 * Decode h264 video from a mov file.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideodecoder.h>
#include <gst/gl/gstglcontext.h>
#include "vtdec.h"
#include "vtutil.h"
#include "helpers.h"
#include "corevideobuffer.h"
#include "coremediabuffer.h"
#include "videotexturecache-gl.h"
#if defined(APPLEMEDIA_MOLTENVK)
#include "videotexturecache-vulkan.h"
#endif

GST_DEBUG_CATEGORY_STATIC (gst_vtdec_debug_category);
#define GST_CAT_DEFAULT gst_vtdec_debug_category

enum
{
  /* leave some headroom for new GstVideoCodecFrameFlags flags */
  VTDEC_FRAME_FLAG_SKIP = (1 << 10),
  VTDEC_FRAME_FLAG_DROP = (1 << 11),
  VTDEC_FRAME_FLAG_ERROR = (1 << 12),
};

static void gst_vtdec_finalize (GObject * object);

static gboolean gst_vtdec_start (GstVideoDecoder * decoder);
static gboolean gst_vtdec_stop (GstVideoDecoder * decoder);
static void gst_vtdec_output_loop (GstVtdec * self);
static gboolean gst_vtdec_negotiate (GstVideoDecoder * decoder);
static gboolean gst_vtdec_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_vtdec_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_vtdec_finish (GstVideoDecoder * decoder);
static gboolean gst_vtdec_sink_event (GstVideoDecoder * decoder,
    GstEvent * event);
static GstStateChangeReturn gst_vtdec_change_state (GstElement * element,
    GstStateChange transition);
static GstFlowReturn gst_vtdec_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);

static OSStatus gst_vtdec_create_session (GstVtdec * vtdec,
    GstVideoFormat format, gboolean enable_hardware);
static void gst_vtdec_invalidate_session (GstVtdec * vtdec);
static CMSampleBufferRef cm_sample_buffer_from_gst_buffer (GstVtdec * vtdec,
    GstBuffer * buf);
static GstFlowReturn gst_vtdec_drain_decoder (GstVideoDecoder * decoder,
    gboolean flush);
static CMFormatDescriptionRef create_format_description (GstVtdec * vtdec,
    CMVideoCodecType cm_format);
static CMFormatDescriptionRef
create_format_description_from_codec_data (GstVtdec * vtdec,
    CMVideoCodecType cm_format, GstBuffer * codec_data);
static void gst_vtdec_session_output_callback (void
    *decompression_output_ref_con, void *source_frame_ref_con, OSStatus status,
    VTDecodeInfoFlags info_flags, CVImageBufferRef image_buffer, CMTime pts,
    CMTime duration);
static gboolean compute_h264_decode_picture_buffer_size (GstVtdec * vtdec,
    GstBuffer * codec_data, int *length);
static gboolean compute_hevc_decode_picture_buffer_size (GstVtdec * vtdec,
    GstBuffer * codec_data, int *length);
static gboolean gst_vtdec_compute_dpb_size (GstVtdec * vtdec,
    CMVideoCodecType cm_format, GstBuffer * codec_data);
static void gst_vtdec_set_latency (GstVtdec * vtdec);
static void gst_vtdec_set_context (GstElement * element, GstContext * context);

static GstStaticPadTemplate gst_vtdec_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, stream-format=avc, alignment=au,"
        " width=(int)[1, MAX], height=(int)[1, MAX];"
        "video/x-h265, stream-format=(string){ hev1, hvc1 }, alignment=au,"
        " width=(int)[1, MAX], height=(int)[1, MAX];"
        "video/mpeg, mpegversion=2, systemstream=false, parsed=true;"
        "image/jpeg;"
        "video/x-prores, variant = { (string)standard, (string)hq, (string)lt,"
        " (string)proxy, (string)4444, (string)4444xq };")
    );

/* define EnableHardwareAcceleratedVideoDecoder in < 10.9 */
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED < 1090
const CFStringRef
    kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder =
CFSTR ("EnableHardwareAcceleratedVideoDecoder");
const CFStringRef
    kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder =
CFSTR ("RequireHardwareAcceleratedVideoDecoder");
#endif

#define VIDEO_SRC_CAPS_FORMATS "{ NV12, AYUV64, ARGB64_BE }"

#define VIDEO_SRC_CAPS_NATIVE                                           \
    GST_VIDEO_CAPS_MAKE(VIDEO_SRC_CAPS_FORMATS) ";"                     \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_GL_MEMORY,\
        VIDEO_SRC_CAPS_FORMATS) ", "                                    \
    "texture-target = (string) rectangle "

#if defined(APPLEMEDIA_MOLTENVK)
#define VIDEO_SRC_CAPS VIDEO_SRC_CAPS_NATIVE "; "                           \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES(GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, \
        VIDEO_SRC_CAPS_FORMATS)
#else
#define VIDEO_SRC_CAPS VIDEO_SRC_CAPS_NATIVE
#endif

G_DEFINE_TYPE (GstVtdec, gst_vtdec, GST_TYPE_VIDEO_DECODER);

static void
gst_vtdec_class_init (GstVtdecClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *video_decoder_class = GST_VIDEO_DECODER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (element_class,
      &gst_vtdec_sink_template);

  {
    GstCaps *caps = gst_caps_from_string (VIDEO_SRC_CAPS);
    /* RGBA64_LE is kCVPixelFormatType_64RGBALE, only available on macOS 11.3+ */
    if (GST_APPLEMEDIA_HAVE_64RGBALE)
      caps = gst_vtutil_caps_append_video_format (caps, "RGBA64_LE");
    gst_element_class_add_pad_template (element_class,
        gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  }

  gst_element_class_set_static_metadata (element_class,
      "Apple VideoToolbox decoder",
      "Codec/Decoder/Video/Hardware",
      "Apple VideoToolbox Decoder",
      "Ole André Vadla Ravnås <oleavr@soundrop.com>; "
      "Alessandro Decina <alessandro.d@gmail.com>");

  gobject_class->finalize = gst_vtdec_finalize;
  element_class->set_context = gst_vtdec_set_context;
  element_class->change_state = gst_vtdec_change_state;
  video_decoder_class->start = GST_DEBUG_FUNCPTR (gst_vtdec_start);
  video_decoder_class->stop = GST_DEBUG_FUNCPTR (gst_vtdec_stop);
  video_decoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_vtdec_negotiate);
  video_decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_vtdec_set_format);
  video_decoder_class->flush = GST_DEBUG_FUNCPTR (gst_vtdec_flush);
  video_decoder_class->finish = GST_DEBUG_FUNCPTR (gst_vtdec_finish);
  video_decoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_vtdec_handle_frame);
  video_decoder_class->sink_event = GST_DEBUG_FUNCPTR (gst_vtdec_sink_event);
}

static void
gst_vtdec_init (GstVtdec * vtdec)
{
  g_mutex_init (&vtdec->queue_mutex);
  g_cond_init (&vtdec->queue_cond);
}

void
gst_vtdec_finalize (GObject * object)
{
  GstVtdec *vtdec = GST_VTDEC (object);

  GST_DEBUG_OBJECT (vtdec, "finalize");

  g_mutex_clear (&vtdec->queue_mutex);
  g_cond_clear (&vtdec->queue_cond);

  G_OBJECT_CLASS (gst_vtdec_parent_class)->finalize (object);
}

static gboolean
gst_vtdec_start (GstVideoDecoder * decoder)
{
  GstVtdec *vtdec = GST_VTDEC (decoder);

  GST_DEBUG_OBJECT (vtdec, "start");

  vtdec->is_flushing = FALSE;
  vtdec->is_draining = FALSE;
  vtdec->downstream_ret = GST_FLOW_OK;
  vtdec->reorder_queue = gst_queue_array_new (0);

  /* Create the output task, but pause it immediately */
  vtdec->pause_task = TRUE;
  if (!gst_pad_start_task (GST_VIDEO_DECODER_SRC_PAD (decoder),
          (GstTaskFunction) gst_vtdec_output_loop, vtdec, NULL)) {
    GST_ERROR_OBJECT (vtdec, "failed to start output thread");
    return FALSE;
  }
  /* This blocks until the loop actually pauses */
  gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (decoder));
  vtdec->pause_task = FALSE;

  if (!vtdec->ctxh)
    vtdec->ctxh = gst_gl_context_helper_new (GST_ELEMENT (decoder));

  return TRUE;
}

static gboolean
gst_vtdec_stop (GstVideoDecoder * decoder)
{
  GstVideoCodecFrame *frame;
  GstVtdec *vtdec = GST_VTDEC (decoder);

  GST_DEBUG_OBJECT (vtdec, "stop");

  gst_vtdec_drain_decoder (GST_VIDEO_DECODER_CAST (vtdec), TRUE);
  vtdec->downstream_ret = GST_FLOW_FLUSHING;

  while ((frame = gst_queue_array_pop_head (vtdec->reorder_queue))) {
    gst_video_decoder_release_frame (decoder, frame);
  }
  gst_queue_array_free (vtdec->reorder_queue);
  vtdec->reorder_queue = NULL;

  gst_pad_stop_task (GST_VIDEO_DECODER_SRC_PAD (decoder));

  if (vtdec->input_state)
    gst_video_codec_state_unref (vtdec->input_state);
  vtdec->input_state = NULL;

  if (vtdec->session)
    gst_vtdec_invalidate_session (vtdec);

  if (vtdec->texture_cache)
    g_object_unref (vtdec->texture_cache);
  vtdec->texture_cache = NULL;

  if (vtdec->ctxh)
    gst_gl_context_helper_free (vtdec->ctxh);
  vtdec->ctxh = NULL;

  if (vtdec->format_description)
    CFRelease (vtdec->format_description);
  vtdec->format_description = NULL;

#if defined(APPLEMEDIA_MOLTENVK)
  gst_clear_object (&vtdec->device);
  gst_clear_object (&vtdec->instance);
#endif

  return TRUE;
}

static void
gst_vtdec_output_loop (GstVtdec * vtdec)
{
  GstVideoCodecFrame *frame;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoDecoder *decoder = GST_VIDEO_DECODER (vtdec);
  gboolean is_flushing;

  g_mutex_lock (&vtdec->queue_mutex);
  while (gst_queue_array_is_empty (vtdec->reorder_queue)
      && !vtdec->pause_task && !vtdec->is_flushing && !vtdec->is_draining) {
    g_cond_wait (&vtdec->queue_cond, &vtdec->queue_mutex);
  }

  if (vtdec->pause_task) {
    g_mutex_unlock (&vtdec->queue_mutex);
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (decoder));
    return;
  }

  /* push a buffer if there are enough frames to guarantee 
   * that we push in PTS order, or if we're draining/flushing */
  while ((gst_queue_array_get_length (vtdec->reorder_queue) >=
          vtdec->dbp_size) || vtdec->is_flushing || vtdec->is_draining) {
    frame = gst_queue_array_pop_head (vtdec->reorder_queue);
    is_flushing = vtdec->is_flushing;
    g_cond_signal (&vtdec->queue_cond);
    g_mutex_unlock (&vtdec->queue_mutex);

    /* we need to check this in case dpb_size=0 (jpeg for
     * example) or we're draining/flushing */
    if (frame) {
      GST_VIDEO_DECODER_STREAM_LOCK (vtdec);

      if (frame->flags & VTDEC_FRAME_FLAG_ERROR) {
        GST_VIDEO_DECODER_ERROR (vtdec, 1, STREAM, DECODE,
            ("Got frame %d with an error flag", frame->system_frame_number),
            (NULL), ret);
        gst_video_decoder_release_frame (decoder, frame);
      } else if (is_flushing || (frame->flags & VTDEC_FRAME_FLAG_SKIP)) {
        GST_LOG_OBJECT (vtdec, "flushing frame %d", frame->system_frame_number);
        gst_video_decoder_release_frame (decoder, frame);
      } else if (frame->flags & VTDEC_FRAME_FLAG_DROP) {
        GST_LOG_OBJECT (vtdec, "dropping frame %d", frame->system_frame_number);
        gst_video_decoder_drop_frame (decoder, frame);
      } else {
        GST_TRACE_OBJECT (vtdec, "pushing frame %d",
            frame->system_frame_number);
        ret = gst_video_decoder_finish_frame (decoder, frame);
      }

      GST_VIDEO_DECODER_STREAM_UNLOCK (vtdec);
    }

    g_mutex_lock (&vtdec->queue_mutex);
    if (!frame || ret != GST_FLOW_OK)
      break;
  }

  g_mutex_unlock (&vtdec->queue_mutex);
  GST_VIDEO_DECODER_STREAM_LOCK (vtdec);
  vtdec->downstream_ret = ret;

  /* We need to empty the queue immediately so that session_output_callback() 
   * can push out the current buffer, otherwise it can deadlock */
  if (ret != GST_FLOW_OK) {
    g_mutex_lock (&vtdec->queue_mutex);

    while ((frame = gst_queue_array_pop_head (vtdec->reorder_queue))) {
      GST_LOG_OBJECT (vtdec, "flushing frame %d", frame->system_frame_number);
      gst_video_decoder_release_frame (decoder, frame);
    }

    g_cond_signal (&vtdec->queue_cond);
    g_mutex_unlock (&vtdec->queue_mutex);
  }

  GST_VIDEO_DECODER_STREAM_UNLOCK (vtdec);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (vtdec, "pausing output task: %s",
        gst_flow_get_name (ret));
    gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (decoder));
  }
}

static gboolean
gst_vtdec_ensure_output_loop (GstVtdec * vtdec)
{
  GstPad *pad = GST_VIDEO_DECODER_SRC_PAD (vtdec);
  GstTask *task = GST_PAD_TASK (pad);

  return gst_task_resume (task);
}

static void
gst_vtdec_pause_output_loop (GstVtdec * vtdec)
{
  g_mutex_lock (&vtdec->queue_mutex);
  vtdec->pause_task = TRUE;
  g_cond_signal (&vtdec->queue_cond);
  g_mutex_unlock (&vtdec->queue_mutex);

  gst_pad_pause_task (GST_VIDEO_DECODER_SRC_PAD (vtdec));
  GST_DEBUG_OBJECT (vtdec, "paused output thread");

  g_mutex_lock (&vtdec->queue_mutex);
  vtdec->pause_task = FALSE;
  g_mutex_unlock (&vtdec->queue_mutex);
}

static void
setup_texture_cache (GstVtdec * vtdec, GstVideoFormat format)
{
  GstVideoCodecState *output_state;

  GST_INFO_OBJECT (vtdec, "setting up texture cache");
  output_state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (vtdec));
  gst_video_texture_cache_set_format (vtdec->texture_cache, format,
      output_state->caps);
  gst_video_codec_state_unref (output_state);
}

/*
 * Unconditionally output a high bit-depth + alpha format when decoding Apple
 * ProRes video if downstream supports it.
 * TODO: read src_pix_fmt to get the preferred output format
 * https://wiki.multimedia.cx/index.php/Apple_ProRes#Frame_header
 */
static GstVideoFormat
get_preferred_video_format (GstStructure * s, gboolean prores)
{
  const GValue *list = gst_structure_get_value (s, "format");
  guint i, size = gst_value_list_get_size (list);
  for (i = 0; i < size; i++) {
    const GValue *value = gst_value_list_get_value (list, i);
    const char *fmt = g_value_get_string (value);
    GstVideoFormat vfmt = gst_video_format_from_string (fmt);
    switch (vfmt) {
      case GST_VIDEO_FORMAT_NV12:
        if (!prores)
          return vfmt;
        break;
      case GST_VIDEO_FORMAT_AYUV64:
      case GST_VIDEO_FORMAT_ARGB64_BE:
        if (prores)
          return vfmt;
        break;
      case GST_VIDEO_FORMAT_RGBA64_LE:
        if (GST_APPLEMEDIA_HAVE_64RGBALE) {
          if (prores)
            return vfmt;
        } else {
          /* Codepath will never be hit on macOS older than Big Sur (11.3) */
          g_warn_if_reached ();
        }
        break;
      default:
        break;
    }
  }
  return GST_VIDEO_FORMAT_UNKNOWN;
}

static gboolean
gst_vtdec_negotiate (GstVideoDecoder * decoder)
{
  GstVideoCodecState *output_state = NULL;
  GstCaps *peercaps = NULL, *caps = NULL, *templcaps = NULL, *prevcaps = NULL;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVtdec *vtdec;
  OSStatus err = noErr;
  GstCapsFeatures *features = NULL;
  gboolean output_textures = FALSE;
#if defined(APPLEMEDIA_MOLTENVK)
  gboolean output_vulkan = FALSE;
#endif

  vtdec = GST_VTDEC (decoder);
  if (vtdec->session)
    gst_vtdec_drain_decoder (GST_VIDEO_DECODER_CAST (vtdec), FALSE);

  output_state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (vtdec));
  if (output_state) {
    prevcaps = gst_caps_ref (output_state->caps);
    gst_video_codec_state_unref (output_state);
  }

  templcaps =
      gst_pad_get_pad_template_caps (GST_VIDEO_DECODER_SRC_PAD (decoder));
  peercaps =
      gst_pad_peer_query_caps (GST_VIDEO_DECODER_SRC_PAD (vtdec), templcaps);
  gst_caps_unref (templcaps);

  if (gst_caps_is_empty (peercaps)) {
    GST_INFO_OBJECT (vtdec, "empty peer caps, can't negotiate");

    gst_caps_unref (peercaps);
    if (prevcaps)
      gst_caps_unref (prevcaps);

    return FALSE;
  }

  if (prevcaps && gst_caps_can_intersect (prevcaps, peercaps)) {
    /* The hardware decoder can become (temporarily) unavailable across
     * VTDecompressionSessionCreate/Destroy calls. So if the currently configured
     * caps are still accepted by downstream we keep them so we don't have to
     * destroy and recreate the session.
     */
    GST_INFO_OBJECT (vtdec,
        "current and peer caps are compatible, keeping current caps");
    caps = gst_caps_ref (prevcaps);
    gst_caps_unref (peercaps);
  } else {
    caps = peercaps;
  }

  caps = gst_caps_truncate (gst_caps_make_writable (caps));

  /* Try to use whatever video format downstream prefers */
  {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    if (gst_structure_has_field_typed (s, "format", GST_TYPE_LIST)) {
      GstStructure *is = gst_caps_get_structure (vtdec->input_state->caps, 0);
      const char *name = gst_structure_get_name (is);
      format = get_preferred_video_format (s,
          g_strcmp0 (name, "video/x-prores") == 0);
    }

    if (format == GST_VIDEO_FORMAT_UNKNOWN) {
      const char *fmt;
      gst_structure_fixate_field (s, "format");
      fmt = gst_structure_get_string (s, "format");
      if (fmt)
        format = gst_video_format_from_string (fmt);
      else
        /* If all fails, just use NV12 */
        format = GST_VIDEO_FORMAT_NV12;
    }
  }

  features = gst_caps_get_features (caps, 0);
  if (features)
    features = gst_caps_features_copy (features);

  output_state = gst_video_decoder_set_output_state (GST_VIDEO_DECODER (vtdec),
      format, vtdec->video_info.width, vtdec->video_info.height,
      vtdec->input_state);
  output_state->caps = gst_video_info_to_caps (&output_state->info);
  if (features) {
    gst_caps_set_features (output_state->caps, 0, features);
    output_textures =
        gst_caps_features_contains (features,
        GST_CAPS_FEATURE_MEMORY_GL_MEMORY);
    if (output_textures)
      gst_caps_set_simple (output_state->caps, "texture-target", G_TYPE_STRING,
#if !HAVE_IOS
          GST_GL_TEXTURE_TARGET_RECTANGLE_STR,
#else
          GST_GL_TEXTURE_TARGET_2D_STR,
#endif
          NULL);

#if defined(APPLEMEDIA_MOLTENVK)
    output_vulkan =
        gst_caps_features_contains (features,
        GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE);
#endif
  }
  gst_caps_unref (caps);

  if (!prevcaps || !gst_caps_is_equal (prevcaps, output_state->caps)) {
    gboolean renegotiating = vtdec->session != NULL;

    GST_INFO_OBJECT (vtdec,
        "negotiated output format %" GST_PTR_FORMAT " previous %"
        GST_PTR_FORMAT, output_state->caps, prevcaps);

    if (vtdec->session)
      gst_vtdec_invalidate_session (vtdec);

    err = gst_vtdec_create_session (vtdec, format, TRUE);
    if (err == noErr) {
      GST_INFO_OBJECT (vtdec, "using hardware decoder");
    } else if (err == kVTVideoDecoderNotAvailableNowErr && renegotiating) {
      GST_WARNING_OBJECT (vtdec, "hw decoder not available anymore");
      err = gst_vtdec_create_session (vtdec, format, FALSE);
    }

    if (err != noErr) {
      GST_ELEMENT_ERROR (vtdec, RESOURCE, FAILED, (NULL),
          ("VTDecompressionSessionCreate returned %d", (int) err));
    }
  }

  if (vtdec->texture_cache != NULL
      && ((GST_IS_VIDEO_TEXTURE_CACHE_GL (vtdec->texture_cache)
              && !output_textures)
#if defined(APPLEMEDIA_MOLTENVK)
          || (GST_IS_VIDEO_TEXTURE_CACHE_VULKAN (vtdec->texture_cache)
              && !output_vulkan)
#endif
      )) {
    g_object_unref (vtdec->texture_cache);
    vtdec->texture_cache = NULL;
  }

  if (err == noErr) {
    if (output_textures) {
      GstVideoTextureCacheGL *cache_gl = NULL;

      if (vtdec->texture_cache)
        cache_gl = GST_VIDEO_TEXTURE_CACHE_GL (vtdec->texture_cache);

      /* call this regardless of whether caps have changed or not since a new
       * local context could have become available
       */
      if (!vtdec->ctxh)
        vtdec->ctxh = gst_gl_context_helper_new (GST_ELEMENT (vtdec));
      gst_gl_context_helper_ensure_context (vtdec->ctxh);

      GST_INFO_OBJECT (vtdec, "pushing GL textures, context %p old context %p",
          vtdec->ctxh->context, cache_gl ? cache_gl->ctx : NULL);

      if (cache_gl && cache_gl->ctx != vtdec->ctxh->context) {
        g_object_unref (vtdec->texture_cache);
        vtdec->texture_cache = NULL;
      }
      if (!vtdec->texture_cache) {
        vtdec->texture_cache =
            gst_video_texture_cache_gl_new (vtdec->ctxh->context);
        setup_texture_cache (vtdec, format);
      }
    }
#if defined(APPLEMEDIA_MOLTENVK)
    if (output_vulkan) {
      GstVideoTextureCacheVulkan *cache_vulkan = NULL;

      if (vtdec->texture_cache)
        cache_vulkan = GST_VIDEO_TEXTURE_CACHE_VULKAN (vtdec->texture_cache);

      gst_vulkan_ensure_element_data (GST_ELEMENT (vtdec), NULL,
          &vtdec->instance);

      if (!gst_vulkan_device_run_context_query (GST_ELEMENT (vtdec),
              &vtdec->device)) {
        GError *error = NULL;
        GST_DEBUG_OBJECT (vtdec, "No device retrieved from peer elements");
        if (!(vtdec->device =
                gst_vulkan_instance_create_device (vtdec->instance, &error))) {
          GST_ELEMENT_ERROR (vtdec, RESOURCE, NOT_FOUND,
              ("Failed to create vulkan device"), ("%s", error->message));
          g_clear_error (&error);
          return FALSE;
        }
      }

      GST_INFO_OBJECT (vtdec, "pushing vulkan images, device %" GST_PTR_FORMAT
          " old device %" GST_PTR_FORMAT, vtdec->device,
          cache_vulkan ? cache_vulkan->device : NULL);

      if (cache_vulkan && cache_vulkan->device != vtdec->device) {
        g_object_unref (vtdec->texture_cache);
        vtdec->texture_cache = NULL;
      }
      if (!vtdec->texture_cache) {
        vtdec->texture_cache =
            gst_video_texture_cache_vulkan_new (vtdec->device);
        setup_texture_cache (vtdec, format);
      }
    }
#endif
  }

  if (prevcaps)
    gst_caps_unref (prevcaps);

  if (err != noErr)
    return FALSE;

  return GST_VIDEO_DECODER_CLASS (gst_vtdec_parent_class)->negotiate (decoder);
}

static gboolean
gst_vtdec_set_format (GstVideoDecoder * decoder, GstVideoCodecState * state)
{
  GstStructure *structure;
  CMVideoCodecType cm_format = 0;
  CMFormatDescriptionRef format_description = NULL;
  const char *caps_name;
  GstVtdec *vtdec = GST_VTDEC (decoder);

  GST_DEBUG_OBJECT (vtdec, "set_format");

  structure = gst_caps_get_structure (state->caps, 0);
  caps_name = gst_structure_get_name (structure);
  if (!strcmp (caps_name, "video/x-h264")) {
    cm_format = kCMVideoCodecType_H264;
  } else if (!strcmp (caps_name, "video/x-h265")) {
    cm_format = kCMVideoCodecType_HEVC;
  } else if (!strcmp (caps_name, "video/mpeg")) {
    cm_format = kCMVideoCodecType_MPEG2Video;
  } else if (!strcmp (caps_name, "image/jpeg")) {
    cm_format = kCMVideoCodecType_JPEG;
  } else if (!strcmp (caps_name, "video/x-prores")) {
    const char *variant = gst_structure_get_string (structure, "variant");

    if (variant)
      cm_format = gst_vtutil_codec_type_from_prores_variant (variant);

    if (cm_format == GST_kCMVideoCodecType_Some_AppleProRes) {
      GST_ERROR_OBJECT (vtdec, "Invalid ProRes variant %s", variant);
      return FALSE;
    }
  }

  if ((cm_format == kCMVideoCodecType_H264
          || cm_format == kCMVideoCodecType_HEVC)
      && state->codec_data == NULL) {
    GST_INFO_OBJECT (vtdec, "no codec data, wait for one");
    return TRUE;
  }

  gst_video_info_from_caps (&vtdec->video_info, state->caps);

  if (!gst_vtdec_compute_dpb_size (vtdec, cm_format, state->codec_data))
    return FALSE;
  gst_vtdec_set_latency (vtdec);

  if (state->codec_data) {
    format_description = create_format_description_from_codec_data (vtdec,
        cm_format, state->codec_data);
  } else {
    format_description = create_format_description (vtdec, cm_format);
  }

  if (vtdec->format_description)
    CFRelease (vtdec->format_description);
  vtdec->format_description = format_description;

  if (vtdec->input_state)
    gst_video_codec_state_unref (vtdec->input_state);
  vtdec->input_state = gst_video_codec_state_ref (state);

  return gst_vtdec_negotiate (decoder);
}

static gboolean
gst_vtdec_flush (GstVideoDecoder * decoder)
{
  GstVtdec *vtdec = GST_VTDEC (decoder);

  GST_DEBUG_OBJECT (vtdec, "flush");

  return gst_vtdec_drain_decoder (GST_VIDEO_DECODER_CAST (vtdec),
      TRUE) == GST_FLOW_OK;
}

static GstFlowReturn
gst_vtdec_finish (GstVideoDecoder * decoder)
{
  GstVtdec *vtdec = GST_VTDEC (decoder);

  GST_DEBUG_OBJECT (vtdec, "finish");

  return gst_vtdec_drain_decoder (GST_VIDEO_DECODER_CAST (vtdec), FALSE);
}

static gboolean
gst_vtdec_sink_event (GstVideoDecoder * decoder, GstEvent * event)
{
  GstVtdec *vtdec = GST_VTDEC (decoder);
  GstEventType type = GST_EVENT_TYPE (event);
  gboolean ret;

  switch (type) {
    case GST_EVENT_FLUSH_START:
      GST_DEBUG_OBJECT (vtdec, "flush start received, setting flushing flag");

      g_mutex_lock (&vtdec->queue_mutex);
      vtdec->is_flushing = TRUE;
      g_cond_signal (&vtdec->queue_cond);
      g_mutex_unlock (&vtdec->queue_mutex);
      break;
    default:
      break;
  }

  ret =
      GST_VIDEO_DECODER_CLASS (gst_vtdec_parent_class)->sink_event (decoder,
      event);

  switch (type) {
    case GST_EVENT_FLUSH_STOP:
      /* The base class handles this event and calls _flush().
       * We can then safely reset the flushing flag. */
      GST_DEBUG_OBJECT (vtdec, "flush stop received, removing flushing flag");

      g_mutex_lock (&vtdec->queue_mutex);
      vtdec->is_flushing = FALSE;
      g_mutex_unlock (&vtdec->queue_mutex);
      break;
    default:
      break;
  }

  return ret;
}

static GstStateChangeReturn
gst_vtdec_change_state (GstElement * element, GstStateChange transition)
{
  GstVtdec *self = GST_VTDEC (element);

  if (transition == GST_STATE_CHANGE_PAUSED_TO_READY) {
    GST_DEBUG_OBJECT (self, "pausing output loop on PAUSED->READY");
    gst_vtdec_pause_output_loop (self);
  }

  return GST_ELEMENT_CLASS (gst_vtdec_parent_class)->change_state (element,
      transition);
}

static GstFlowReturn
gst_vtdec_handle_frame (GstVideoDecoder * decoder, GstVideoCodecFrame * frame)
{
  OSStatus status;
  CMSampleBufferRef cm_sample_buffer = NULL;
  VTDecodeFrameFlags input_flags;
  GstVtdec *vtdec = GST_VTDEC (decoder);
  GstFlowReturn ret = GST_FLOW_OK;
  int decode_frame_number = frame->decode_frame_number;
  GstTaskState task_state;
  gboolean is_flushing;

  if (vtdec->format_description == NULL) {
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto drop;
  }

  /* Negotiate now so that we know whether we need to use the GL upload meta or not. 
   * gst_vtenc_negotiate() will drain before attempting to negotiate. */
  if (gst_pad_check_reconfigure (decoder->srcpad)) {
    if (!gst_vtdec_negotiate (decoder)) {
      gst_pad_mark_reconfigure (decoder->srcpad);
      if (GST_PAD_IS_FLUSHING (decoder->srcpad))
        ret = GST_FLOW_FLUSHING;
      else
        ret = GST_FLOW_NOT_NEGOTIATED;
      goto drop;
    }
  }

  task_state = gst_pad_get_task_state (GST_VIDEO_DECODER_SRC_PAD (vtdec));
  if (task_state == GST_TASK_STOPPED || task_state == GST_TASK_PAUSED) {
    /* Abort if our loop failed to push frames downstream... */
    if (vtdec->downstream_ret != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (vtdec,
          "Output loop stopped because of %s, ignoring frame",
          gst_flow_get_name (vtdec->downstream_ret));

      ret = vtdec->downstream_ret;
      goto drop;
    }

    /* ...or if it stopped because of the flushing flag while the queue
     * was empty, in which case we didn't get GST_FLOW_FLUSHING... */
    g_mutex_lock (&vtdec->queue_mutex);
    is_flushing = vtdec->is_flushing;
    g_mutex_unlock (&vtdec->queue_mutex);
    if (is_flushing) {
      GST_DEBUG_OBJECT (vtdec, "Flushing flag set, ignoring frame");
      ret = GST_FLOW_FLUSHING;
      goto drop;
    }

    /* .. or if it refuses to resume - e.g. it was stopped instead of paused */
    if (!gst_vtdec_ensure_output_loop (vtdec)) {
      GST_ERROR_OBJECT (vtdec, "Output loop failed to resume");
      ret = GST_FLOW_ERROR;
      goto drop;
    }
  }

  GST_LOG_OBJECT (vtdec, "got input frame %d", decode_frame_number);

  /* don't bother enabling kVTDecodeFrame_EnableTemporalProcessing at all since
   * it's not mandatory for the underlying VT codec to respect it. KISS and do
   * reordering ourselves. */
  input_flags = kVTDecodeFrame_EnableAsynchronousDecompression;

  cm_sample_buffer =
      cm_sample_buffer_from_gst_buffer (vtdec, frame->input_buffer);

  /* We need to unlock the stream lock here because
   * the decode call can wait until gst_vtdec_session_output_callback()
   * is finished, which in turn can wait until there's space in the 
   * output queue, which is being handled by the output loop, 
   * which also uses the stream lock... */
  GST_VIDEO_DECODER_STREAM_UNLOCK (vtdec);
  status = VTDecompressionSessionDecodeFrame (vtdec->session, cm_sample_buffer,
      input_flags, frame, NULL);
  GST_VIDEO_DECODER_STREAM_LOCK (vtdec);

  if (status != noErr) {
    GST_VIDEO_DECODER_ERROR (vtdec, 1, STREAM, DECODE,
        ("Failed to decode frame"),
        ("VTDecompressionSessionDecodeFrame returned %d", (int) status), ret);
    goto out;
  }

  GST_LOG_OBJECT (vtdec, "submitted input frame %d", decode_frame_number);
  frame = NULL;

out:
  if (cm_sample_buffer)
    CFRelease (cm_sample_buffer);
  return ret;

drop:
  gst_video_decoder_release_frame (decoder, frame);
  goto out;
}

static void
gst_vtdec_invalidate_session (GstVtdec * vtdec)
{
  g_return_if_fail (vtdec->session);

  VTDecompressionSessionInvalidate (vtdec->session);
  CFRelease (vtdec->session);
  vtdec->session = NULL;
}

static OSStatus
gst_vtdec_create_session (GstVtdec * vtdec, GstVideoFormat format,
    gboolean enable_hardware)
{
  CFMutableDictionaryRef output_image_buffer_attrs;
  VTDecompressionOutputCallbackRecord callback;
  CFMutableDictionaryRef videoDecoderSpecification;
  OSStatus status;
  guint32 cv_format = gst_video_format_to_cvpixelformat (format);

  g_return_val_if_fail (vtdec->session == NULL, FALSE);

  videoDecoderSpecification =
      CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  /* This is the default on iOS and the key does not exist there */
#ifndef HAVE_IOS
  gst_vtutil_dict_set_boolean (videoDecoderSpecification,
      kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder,
      enable_hardware);
  if (enable_hardware && vtdec->require_hardware)
    gst_vtutil_dict_set_boolean (videoDecoderSpecification,
        kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder,
        TRUE);
#endif

  output_image_buffer_attrs =
      CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  gst_vtutil_dict_set_i32 (output_image_buffer_attrs,
      kCVPixelBufferPixelFormatTypeKey, cv_format);
  gst_vtutil_dict_set_i32 (output_image_buffer_attrs, kCVPixelBufferWidthKey,
      vtdec->video_info.width);
  gst_vtutil_dict_set_i32 (output_image_buffer_attrs, kCVPixelBufferHeightKey,
      vtdec->video_info.height);

  callback.decompressionOutputCallback = gst_vtdec_session_output_callback;
  callback.decompressionOutputRefCon = vtdec;

  status = VTDecompressionSessionCreate (NULL, vtdec->format_description,
      videoDecoderSpecification, output_image_buffer_attrs, &callback,
      &vtdec->session);

  if (videoDecoderSpecification)
    CFRelease (videoDecoderSpecification);

  CFRelease (output_image_buffer_attrs);

  return status;
}

static CMFormatDescriptionRef
create_format_description (GstVtdec * vtdec, CMVideoCodecType cm_format)
{
  OSStatus status;
  CMFormatDescriptionRef format_description;

  status = CMVideoFormatDescriptionCreate (NULL,
      cm_format, vtdec->video_info.width, vtdec->video_info.height,
      NULL, &format_description);
  if (status != noErr)
    return NULL;

  return format_description;
}

static CMFormatDescriptionRef
create_format_description_from_codec_data (GstVtdec * vtdec,
    CMVideoCodecType cm_format, GstBuffer * codec_data)
{
  CMFormatDescriptionRef fmt_desc;
  CFMutableDictionaryRef extensions, par, atoms;
  GstMapInfo map;
  OSStatus status;

  /* Extensions dict */
  extensions =
      CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  gst_vtutil_dict_set_string (extensions,
      CFSTR ("CVImageBufferChromaLocationBottomField"), "left");
  gst_vtutil_dict_set_string (extensions,
      CFSTR ("CVImageBufferChromaLocationTopField"), "left");
  gst_vtutil_dict_set_boolean (extensions, CFSTR ("FullRangeVideo"), FALSE);

  /* CVPixelAspectRatio dict */
  par = CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  gst_vtutil_dict_set_i32 (par, CFSTR ("HorizontalSpacing"),
      vtdec->video_info.par_n);
  gst_vtutil_dict_set_i32 (par, CFSTR ("VerticalSpacing"),
      vtdec->video_info.par_d);
  gst_vtutil_dict_set_object (extensions, CFSTR ("CVPixelAspectRatio"),
      (CFTypeRef *) par);

  /* SampleDescriptionExtensionAtoms dict */
  gst_buffer_map (codec_data, &map, GST_MAP_READ);
  atoms = CFDictionaryCreateMutable (NULL, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);

  if (cm_format == kCMVideoCodecType_HEVC)
    gst_vtutil_dict_set_data (atoms, CFSTR ("hvcC"), map.data, map.size);
  else
    gst_vtutil_dict_set_data (atoms, CFSTR ("avcC"), map.data, map.size);

  gst_vtutil_dict_set_object (extensions,
      CFSTR ("SampleDescriptionExtensionAtoms"), (CFTypeRef *) atoms);
  gst_buffer_unmap (codec_data, &map);

  status = CMVideoFormatDescriptionCreate (NULL,
      cm_format, vtdec->video_info.width, vtdec->video_info.height,
      extensions, &fmt_desc);

  if (extensions)
    CFRelease (extensions);

  if (status == noErr)
    return fmt_desc;
  else
    return NULL;
}

/* Custom FreeBlock function for CMBlockBuffer */
static void
cm_block_buffer_freeblock (void *refCon, void *doomedMemoryBlock,
    size_t sizeInBytes)
{
  GstMapInfo *info = (GstMapInfo *) refCon;

  gst_memory_unmap (info->memory, info);
  gst_memory_unref (info->memory);
  g_slice_free (GstMapInfo, info);
}

static CMBlockBufferRef
cm_block_buffer_from_gst_buffer (GstBuffer * buf, GstMapFlags flags)
{
  OSStatus status;
  CMBlockBufferRef bbuf;
  CMBlockBufferCustomBlockSource blockSource;
  guint memcount, i;

  /* Initialize custom block source structure */
  blockSource.version = kCMBlockBufferCustomBlockSourceVersion;
  blockSource.AllocateBlock = NULL;
  blockSource.FreeBlock = cm_block_buffer_freeblock;

  /* Determine number of memory blocks */
  memcount = gst_buffer_n_memory (buf);
  status = CMBlockBufferCreateEmpty (NULL, memcount, 0, &bbuf);
  if (status != kCMBlockBufferNoErr) {
    GST_ERROR ("CMBlockBufferCreateEmpty returned %d", (int) status);
    return NULL;
  }

  /* Go over all GstMemory objects and add them to the CMBlockBuffer */
  for (i = 0; i < memcount; ++i) {
    GstMemory *mem;
    GstMapInfo *info;

    mem = gst_buffer_get_memory (buf, i);

    info = g_slice_new (GstMapInfo);
    if (!gst_memory_map (mem, info, flags)) {
      GST_ERROR ("failed mapping memory");
      g_slice_free (GstMapInfo, info);
      gst_memory_unref (mem);
      CFRelease (bbuf);
      return NULL;
    }

    blockSource.refCon = info;
    status =
        CMBlockBufferAppendMemoryBlock (bbuf, info->data, info->size, NULL,
        &blockSource, 0, info->size, 0);
    if (status != kCMBlockBufferNoErr) {
      GST_ERROR ("CMBlockBufferAppendMemoryBlock returned %d", (int) status);
      gst_memory_unmap (mem, info);
      g_slice_free (GstMapInfo, info);
      gst_memory_unref (mem);
      CFRelease (bbuf);
      return NULL;
    }
  }

  return bbuf;
}

static CMSampleBufferRef
cm_sample_buffer_from_gst_buffer (GstVtdec * vtdec, GstBuffer * buf)
{
  OSStatus status;
  CMBlockBufferRef bbuf = NULL;
  CMSampleBufferRef sbuf = NULL;
  CMSampleTimingInfo sample_timing;
  CMSampleTimingInfo time_array[1];

  g_return_val_if_fail (vtdec->format_description, NULL);

  /* create a block buffer */
  bbuf = cm_block_buffer_from_gst_buffer (buf, GST_MAP_READ);
  if (bbuf == NULL) {
    GST_ELEMENT_ERROR (vtdec, RESOURCE, FAILED, (NULL),
        ("failed creating CMBlockBuffer"));
    return NULL;
  }

  /* create a sample buffer */
  if (GST_BUFFER_DURATION_IS_VALID (buf))
    sample_timing.duration = CMTimeMake (GST_BUFFER_DURATION (buf), GST_SECOND);
  else
    sample_timing.duration = kCMTimeInvalid;

  if (GST_BUFFER_PTS_IS_VALID (buf))
    sample_timing.presentationTimeStamp =
        CMTimeMake (GST_BUFFER_PTS (buf), GST_SECOND);
  else
    sample_timing.presentationTimeStamp = kCMTimeInvalid;

  if (GST_BUFFER_DTS_IS_VALID (buf))
    sample_timing.decodeTimeStamp =
        CMTimeMake (GST_BUFFER_DTS (buf), GST_SECOND);
  else
    sample_timing.decodeTimeStamp = kCMTimeInvalid;

  time_array[0] = sample_timing;

  status =
      CMSampleBufferCreate (NULL, bbuf, TRUE, 0, 0, vtdec->format_description,
      1, 1, time_array, 0, NULL, &sbuf);
  CFRelease (bbuf);
  if (status != noErr) {
    GST_ELEMENT_ERROR (vtdec, RESOURCE, FAILED, (NULL),
        ("CMSampleBufferCreate returned %d", (int) status));
    return NULL;
  }

  return sbuf;
}

static gint
sort_frames_by_pts (gconstpointer f1, gconstpointer f2, gpointer user_data)
{
  GstVideoCodecFrame *frame1, *frame2;
  GstClockTime pts1, pts2;

  frame1 = (GstVideoCodecFrame *) f1;
  frame2 = (GstVideoCodecFrame *) f2;
  pts1 = pts2 = GST_CLOCK_TIME_NONE;
  if (frame1->output_buffer)
    pts1 = GST_BUFFER_PTS (frame1->output_buffer);
  if (frame2->output_buffer)
    pts2 = GST_BUFFER_PTS (frame2->output_buffer);

  if (!GST_CLOCK_TIME_IS_VALID (pts1) || !GST_CLOCK_TIME_IS_VALID (pts2))
    return 0;

  if (pts1 < pts2)
    return -1;
  else if (pts1 == pts2)
    return 0;
  else
    return 1;
}

static void
gst_vtdec_session_output_callback (void *decompression_output_ref_con,
    void *source_frame_ref_con, OSStatus status, VTDecodeInfoFlags info_flags,
    CVImageBufferRef image_buffer, CMTime pts, CMTime duration)
{
  GstVtdec *vtdec = (GstVtdec *) decompression_output_ref_con;
  GstVideoCodecFrame *frame = (GstVideoCodecFrame *) source_frame_ref_con;
  GstVideoCodecState *state;
  gboolean push_anyway = FALSE;

  GST_LOG_OBJECT (vtdec, "got output frame %p %d and VT buffer %p", frame,
      frame->decode_frame_number, image_buffer);

  frame->output_buffer = NULL;

  if (status != noErr) {
    switch (status) {
      case kVTVideoDecoderReferenceMissingErr:
        /* ReferenceMissingErr is not critical, when it occurs the frame
         * usually has the kVTDecodeInfo_FrameDropped flag set. Log only for debugging purposes. */
        GST_DEBUG_OBJECT (vtdec, "ReferenceMissingErr when decoding frame %d",
            frame->decode_frame_number);
        break;
#ifndef HAVE_IOS
      case codecBadDataErr:    /* SW decoder on macOS uses a different code from the hardware one... */
#endif
      case kVTVideoDecoderBadDataErr:
        /* BadDataErr also shouldn't cause an error to be displayed immediately.
         * Set the error flag so the output loop will log a warning
         * and only error out if this happens too many times. */
        GST_DEBUG_OBJECT (vtdec, "BadDataErr when decoding frame %d",
            frame->decode_frame_number);
        frame->flags |= VTDEC_FRAME_FLAG_ERROR;
        break;
      default:
        GST_ERROR_OBJECT (vtdec, "Error decoding frame %d: %d",
            frame->decode_frame_number, (int) status);
        frame->flags |= VTDEC_FRAME_FLAG_ERROR;
        break;
    }
  }

  if (image_buffer) {
    GstBuffer *buf = NULL;

    /* FIXME: use gst_video_decoder_allocate_output_buffer */
    state = gst_video_decoder_get_output_state (GST_VIDEO_DECODER (vtdec));
    if (state == NULL) {
      GST_WARNING_OBJECT (vtdec, "Output state not configured, release buffer");
      frame->flags &= VTDEC_FRAME_FLAG_SKIP;
    } else {
      buf =
          gst_core_video_buffer_new (image_buffer, &state->info,
          vtdec->texture_cache);
      gst_video_codec_state_unref (state);
      GST_BUFFER_PTS (buf) = pts.value;
      GST_BUFFER_DURATION (buf) = duration.value;
      frame->output_buffer = buf;
    }
  } else {
    if (info_flags & kVTDecodeInfo_FrameDropped) {
      GST_DEBUG_OBJECT (vtdec, "Frame %d dropped by VideoToolbox (%p)",
          frame->decode_frame_number, frame);
      frame->flags |= VTDEC_FRAME_FLAG_DROP;
    } else {
      GST_DEBUG_OBJECT (vtdec, "Decoded frame is NULL");
      frame->flags |= VTDEC_FRAME_FLAG_SKIP;
    }
  }

  /* Limit the amount of frames in our output queue
   * to avoid processing too many frames ahead.
   * The DPB * 2 size limit is completely arbitrary. */
  g_mutex_lock (&vtdec->queue_mutex);
  /* If negotiate() gets called from the output loop (via finish_frame()),
   * it can attempt to drain and call VTDecompressionSessionWaitForAsynchronousFrames,
   * which will lock up if we decide to wait in this callback, creating a deadlock. */
  push_anyway = vtdec->is_flushing || vtdec->is_draining;
  while (!push_anyway
      && gst_queue_array_get_length (vtdec->reorder_queue) >
      vtdec->dbp_size * 2 + 1) {
    g_cond_wait (&vtdec->queue_cond, &vtdec->queue_mutex);
    push_anyway = vtdec->is_flushing || vtdec->is_draining;
  }

  gst_queue_array_push_sorted (vtdec->reorder_queue, frame, sort_frames_by_pts,
      NULL);
  GST_LOG ("pushed frame %d, queue length %d", frame->decode_frame_number,
      gst_queue_array_get_length (vtdec->reorder_queue));
  g_cond_signal (&vtdec->queue_cond);
  g_mutex_unlock (&vtdec->queue_mutex);
}

static GstFlowReturn
gst_vtdec_drain_decoder (GstVideoDecoder * decoder, gboolean flush)
{
  GstVtdec *vtdec = GST_VTDEC (decoder);
  OSStatus vt_status;

  GST_DEBUG_OBJECT (vtdec, "drain_decoder, flushing: %d", flush);

  /* In case of EOS before the first buffer/caps */
  if (vtdec->session == NULL)
    return GST_FLOW_OK;

  /* Only early-return here if we're draining (as that needs to output frames).
   * Flushing doesn't care about errors from downstream. */
  if (!flush && vtdec->downstream_ret != GST_FLOW_OK
      && vtdec->downstream_ret != GST_FLOW_FLUSHING) {
    GST_WARNING_OBJECT (vtdec, "Output loop stopped with error (%s), leaving",
        gst_flow_get_name (vtdec->downstream_ret));
    return vtdec->downstream_ret;
  }

  g_mutex_lock (&vtdec->queue_mutex);
  if (flush)
    vtdec->is_flushing = TRUE;
  else
    vtdec->is_draining = TRUE;
  g_cond_signal (&vtdec->queue_cond);
  g_mutex_unlock (&vtdec->queue_mutex);

  if (!gst_vtdec_ensure_output_loop (vtdec)) {
    GST_ERROR_OBJECT (vtdec, "Output loop failed to resume");
    return GST_FLOW_ERROR;
  }

  GST_VIDEO_DECODER_STREAM_UNLOCK (vtdec);
  vt_status = VTDecompressionSessionWaitForAsynchronousFrames (vtdec->session);
  if (vt_status != noErr) {
    GST_WARNING_OBJECT (vtdec,
        "VTDecompressionSessionWaitForAsynchronousFrames returned %d",
        (int) vt_status);
  }

  gst_vtdec_pause_output_loop (vtdec);
  GST_VIDEO_DECODER_STREAM_LOCK (vtdec);

  /* Only reset the draining flag here,
   * is_flushing will be reset in sink_event() */
  if (vtdec->is_draining)
    vtdec->is_draining = FALSE;

  if (vtdec->downstream_ret == GST_FLOW_OK)
    GST_DEBUG_OBJECT (vtdec, "buffer queue cleaned");
  else
    GST_DEBUG_OBJECT (vtdec,
        "buffer queue not cleaned, output thread returned %s",
        gst_flow_get_name (vtdec->downstream_ret));

  return vtdec->downstream_ret;
}

static int
get_dpb_max_mb_s_from_level (GstVtdec * vtdec, int level)
{
  switch (level) {
    case 10:
      /* 1b?? */
      return 396;
    case 11:
      return 900;
    case 12:
    case 13:
    case 20:
      return 2376;
    case 21:
      return 4752;
    case 22:
    case 30:
      return 8100;
    case 31:
      return 18000;
    case 32:
      return 20480;
    case 40:
    case 41:
      return 32768;
    case 42:
      return 34816;
    case 50:
      return 110400;
    case 51:
    case 52:
      return 184320;
    default:
      GST_ERROR_OBJECT (vtdec, "unknown level %d", level);
      return -1;
  }
}

static gboolean
gst_vtdec_compute_dpb_size (GstVtdec * vtdec,
    CMVideoCodecType cm_format, GstBuffer * codec_data)
{
  if (cm_format == kCMVideoCodecType_H264) {
    if (!compute_h264_decode_picture_buffer_size (vtdec, codec_data,
            &vtdec->dbp_size)) {
      return FALSE;
    }
  } else if (cm_format == kCMVideoCodecType_HEVC) {
    if (!compute_hevc_decode_picture_buffer_size (vtdec, codec_data,
            &vtdec->dbp_size)) {
      return FALSE;
    }
  } else {
    vtdec->dbp_size = 0;
  }

  GST_DEBUG_OBJECT (vtdec, "Calculated DPB size: %d", vtdec->dbp_size);
  return TRUE;
}

static gboolean
parse_h264_decoder_config_record (GstVtdec * vtdec, GstBuffer * codec_data,
    GstH264DecoderConfigRecord ** config)
{
  GstH264NalParser *parser = gst_h264_nal_parser_new ();
  GstMapInfo map;
  gboolean ret = TRUE;

  gst_buffer_map (codec_data, &map, GST_MAP_READ);

  if (gst_h264_parser_parse_decoder_config_record (parser, map.data, map.size,
          config) != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (vtdec, "Failed to parse codec-data");
    ret = FALSE;
  }

  gst_h264_nal_parser_free (parser);
  gst_buffer_unmap (codec_data, &map);
  return ret;
}

static gboolean
get_h264_dpb_size_from_sps (GstVtdec * vtdec, GstH264NalUnit * nalu,
    gint * dpb_size)
{
  GstH264ParserResult result;
  GstH264SPS sps;
  gint width_mb, height_mb;
  gint max_dpb_frames, max_dpb_size, max_dpb_mbs;

  result = gst_h264_parse_sps (nalu, &sps);
  if (result != GST_H264_PARSER_OK) {
    GST_WARNING_OBJECT (vtdec, "Failed to parse SPS, result %d", result);
    return FALSE;
  }

  max_dpb_mbs = get_dpb_max_mb_s_from_level (vtdec, sps.level_idc);
  if (max_dpb_mbs == -1) {
    GST_ELEMENT_ERROR (vtdec, STREAM, DECODE, (NULL),
        ("invalid level found in SPS, could not compute max_dpb_mbs"));
    gst_h264_sps_clear (&sps);
    return FALSE;
  }

  /* This formula is specified in sections A.3.1.h and A.3.2.f of the 2009
   * edition of the standard */
  width_mb = sps.width / 16;
  height_mb = sps.height / 16;
  max_dpb_frames = MIN (max_dpb_mbs / (width_mb * height_mb),
      GST_VTDEC_DPB_MAX_SIZE);

  if (sps.vui_parameters_present_flag
      && sps.vui_parameters.bitstream_restriction_flag)
    max_dpb_frames = MAX (1, sps.vui_parameters.max_dec_frame_buffering);

  /* Some non-conforming H264 streams may request a number of frames 
   * larger than the calculated limit.
   * See https://chromium-review.googlesource.com/c/chromium/src/+/760276/
   */
  max_dpb_size = MAX (max_dpb_frames, sps.num_ref_frames);
  if (max_dpb_size > GST_VTDEC_DPB_MAX_SIZE) {
    GST_WARNING_OBJECT (vtdec, "Too large calculated DPB size %d",
        max_dpb_size);
    max_dpb_size = GST_VTDEC_DPB_MAX_SIZE;
  }

  *dpb_size = max_dpb_size;

  gst_h264_sps_clear (&sps);
  return TRUE;
}

static gboolean
compute_h264_decode_picture_buffer_size (GstVtdec * vtdec,
    GstBuffer * codec_data, gint * length)
{
  GstH264DecoderConfigRecord *config = NULL;
  GstH264NalUnit *nalu;
  guint8 profile, level;
  gboolean ret = TRUE;
  gint new_length;
  guint i;

  *length = 0;

  if (vtdec->video_info.width == 0 || vtdec->video_info.height == 0)
    return FALSE;

  if (!parse_h264_decoder_config_record (vtdec, codec_data, &config))
    return FALSE;

  profile = config->profile_indication;
  level = config->level_indication;
  GST_INFO_OBJECT (vtdec, "parsed profile %d, level %d", profile, level);

  if (profile == 66) {
    /* baseline or constrained-baseline, we don't need to reorder */
    goto out;
  }

  for (i = 0; i < config->sps->len; i++) {
    nalu = &g_array_index (config->sps, GstH264NalUnit, i);

    if (nalu->type != GST_H264_NAL_SPS)
      continue;

    if (!get_h264_dpb_size_from_sps (vtdec, nalu, &new_length))
      GST_WARNING_OBJECT (vtdec, "Failed to get DPB size from SPS");
    else
      *length = MAX (*length, new_length);
  }

out:
  gst_h264_decoder_config_record_free (config);
  return ret;
}

static gboolean
compute_hevc_decode_picture_buffer_size (GstVtdec * vtdec,
    GstBuffer * codec_data, int *length)
{
  /* This value should be level dependent (table A.8)
   * but let's assume the maximum possible one for simplicity. */
  const gint max_luma_ps = 35651584;
  const gint max_dpb_pic_buf = 6;
  gint max_dbp_size, pic_size_samples_y;

  if (vtdec->video_info.width == 0 || vtdec->video_info.height == 0)
    return FALSE;

  /* A.4.2 */
  pic_size_samples_y = vtdec->video_info.width * vtdec->video_info.height;
  if (pic_size_samples_y <= (max_luma_ps >> 2))
    max_dbp_size = max_dpb_pic_buf * 4;
  else if (pic_size_samples_y <= (max_luma_ps >> 1))
    max_dbp_size = max_dpb_pic_buf * 2;
  else if (pic_size_samples_y <= ((3 * max_luma_ps) >> 2))
    max_dbp_size = (max_dpb_pic_buf * 4) / 3;
  else
    max_dbp_size = max_dpb_pic_buf;

  *length = MIN (max_dbp_size, 16);
  return TRUE;
}

static void
gst_vtdec_set_latency (GstVtdec * vtdec)
{
  GstClockTime frame_duration;
  GstClockTime latency;

  if (vtdec->video_info.fps_n == 0) {
    GST_INFO_OBJECT (vtdec, "Framerate not known, can't set latency");
    return;
  }

  frame_duration = gst_util_uint64_scale (GST_SECOND,
      vtdec->video_info.fps_d, vtdec->video_info.fps_n);
  latency = frame_duration * vtdec->dbp_size;

  GST_INFO_OBJECT (vtdec, "setting latency frames:%d time:%" GST_TIME_FORMAT,
      vtdec->dbp_size, GST_TIME_ARGS (latency));
  gst_video_decoder_set_latency (GST_VIDEO_DECODER (vtdec), latency, latency);
}

static void
gst_vtdec_set_context (GstElement * element, GstContext * context)
{
  GstVtdec *vtdec = GST_VTDEC (element);

  GST_INFO_OBJECT (element, "setting context %s",
      gst_context_get_context_type (context));
  if (!vtdec->ctxh)
    vtdec->ctxh = gst_gl_context_helper_new (element);
  gst_gl_handle_set_context (element, context,
      &vtdec->ctxh->display, &vtdec->ctxh->other_context);

#if defined (APPLEMEDIA_MOLTENVK)
  gst_vulkan_handle_set_context (element, context, NULL, &vtdec->instance);
#endif

  GST_ELEMENT_CLASS (gst_vtdec_parent_class)->set_context (element, context);
}

#ifndef HAVE_IOS
#define GST_TYPE_VTDEC_HW   (gst_vtdec_hw_get_type())
#define GST_VTDEC_HW(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VTDEC_HW,GstVtdecHw))
#define GST_VTDEC_HW_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VTDEC_HW,GstVtdecHwClass))
#define GST_IS_VTDEC_HW(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VTDEC_HW))
#define GST_IS_VTDEC_HW_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VTDEC_HW))

typedef GstVtdec GstVtdecHw;
typedef GstVtdecClass GstVtdecHwClass;

GType gst_vtdec_hw_get_type (void);

G_DEFINE_TYPE (GstVtdecHw, gst_vtdec_hw, GST_TYPE_VTDEC);

static void
gst_vtdec_hw_class_init (GstVtdecHwClass * klass)
{
  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "Apple VideoToolbox decoder (hardware only)",
      "Codec/Decoder/Video/Hardware",
      "Apple VideoToolbox Decoder",
      "Ole André Vadla Ravnås <oleavr@soundrop.com>; "
      "Alessandro Decina <alessandro.d@gmail.com>");
}

static void
gst_vtdec_hw_init (GstVtdecHw * vtdec)
{
  GST_VTDEC (vtdec)->require_hardware = TRUE;
}

#endif

void
gst_vtdec_register_elements (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_vtdec_debug_category, "vtdec", 0,
      "debug category for vtdec element");

#ifdef HAVE_IOS
  gst_element_register (plugin, "vtdec", GST_RANK_PRIMARY, GST_TYPE_VTDEC);
#else
  gst_element_register (plugin, "vtdec_hw", GST_RANK_PRIMARY + 1,
      GST_TYPE_VTDEC_HW);
  gst_element_register (plugin, "vtdec", GST_RANK_SECONDARY, GST_TYPE_VTDEC);
#endif
}
