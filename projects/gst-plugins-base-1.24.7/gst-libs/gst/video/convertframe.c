/* Small helper element for format conversion
 * Copyright (C) 2005 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2010 Brandon Lewis <brandon.lewis@collabora.co.uk>
 * Copyright (C) 2010 Edward Hervey <edward.hervey@collabora.co.uk>
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

#include <string.h>
#include "video.h"
#ifdef HAVE_GL
#include <gst/gl/gstglmemory.h>
#endif

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("video-frame-converter", 0,
        "video-frame-converter object");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */

static gboolean
caps_are_raw (const GstCaps * caps)
{
  guint i, len;

  len = gst_caps_get_size (caps);

  for (i = 0; i < len; i++) {
    GstStructure *st = gst_caps_get_structure (caps, i);
    if (gst_structure_has_name (st, "video/x-raw"))
      return TRUE;
  }

  return FALSE;
}

static gboolean
create_element (const gchar * factory_name, GstElement ** element,
    GError ** err)
{
  *element = gst_element_factory_make (factory_name, NULL);
  if (*element)
    return TRUE;

  if (err && *err == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "cannot create element '%s' - please check your GStreamer installation",
        factory_name);
  }

  return FALSE;
}

static GstElement *
get_encoder (const GstCaps * caps, GError ** err)
{
  GList *encoders = NULL;
  GList *filtered = NULL;
  GstElementFactory *factory = NULL;
  GstElement *encoder = NULL;

  encoders =
      gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER |
      GST_ELEMENT_FACTORY_TYPE_MEDIA_IMAGE, GST_RANK_NONE);

  if (encoders == NULL) {
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "Cannot find any image encoder");
    goto fail;
  }

  GST_INFO ("got factory list %p", encoders);
  gst_plugin_feature_list_debug (encoders);

  filtered =
      gst_element_factory_list_filter (encoders, caps, GST_PAD_SRC, FALSE);
  GST_INFO ("got filtered list %p", filtered);

  if (filtered == NULL) {
    gchar *tmp = gst_caps_to_string (caps);
    *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_MISSING_PLUGIN,
        "Cannot find any image encoder for caps %s", tmp);
    g_free (tmp);
    goto fail;
  }

  gst_plugin_feature_list_debug (filtered);

  factory = (GstElementFactory *) filtered->data;

  GST_INFO ("got factory %p", factory);
  encoder = gst_element_factory_create (factory, NULL);

  GST_INFO ("created encoder element %p, %s", encoder,
      GST_ELEMENT_NAME (encoder));

fail:
  if (encoders)
    gst_plugin_feature_list_free (encoders);
  if (filtered)
    gst_plugin_feature_list_free (filtered);

  return encoder;
}

static GstElement *
build_convert_frame_pipeline_d3d11 (GstElement ** src_element,
    GstElement ** sink_element, GstCaps * from_caps, GstCaps * to_caps,
    GError ** err)
{
  GstElement *pipeline = NULL;
  GstElement *appsrc = NULL;
  GstElement *d3d11_convert = NULL;
  GstElement *d3d11_download = NULL;
  GstElement *convert = NULL;
  GstElement *enc = NULL;
  GstElement *appsink = NULL;
  GError *error = NULL;

  if (!create_element ("appsrc", &appsrc, &error) ||
      !create_element ("d3d11convert", &d3d11_convert, &error) ||
      !create_element ("d3d11download", &d3d11_download, &error) ||
      !create_element ("videoconvert", &convert, &error) ||
      !create_element ("appsink", &appsink, &error)) {
    GST_ERROR ("Could not create element");
    goto failed;
  }

  if (caps_are_raw (to_caps)) {
    if (!create_element ("identity", &enc, &error)) {
      GST_ERROR ("Could not create identity element");
      goto failed;
    }
  } else {
    enc = get_encoder (to_caps, &error);
    if (!enc) {
      GST_ERROR ("Could not create encoder");
      goto failed;
    }
  }

  g_object_set (appsrc, "caps", from_caps, "emit-signals", TRUE,
      "format", GST_FORMAT_TIME, NULL);
  g_object_set (appsink, "caps", to_caps, "emit-signals", TRUE, NULL);

  pipeline = gst_pipeline_new ("d3d11-convert-frame-pipeline");
  gst_bin_add_many (GST_BIN (pipeline), appsrc, d3d11_convert, d3d11_download,
      convert, enc, appsink, NULL);

  if (!gst_element_link_many (appsrc,
          d3d11_convert, d3d11_download, convert, enc, appsink, NULL)) {
    /* Now pipeline takes ownership of all elements, so only top-level
     * pipeline should be cleared */
    appsrc = d3d11_convert = convert = enc = appsink = NULL;

    error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_NEGOTIATION,
        "Could not configure pipeline for conversion");
  }

  *src_element = appsrc;
  *sink_element = appsink;

  return pipeline;

failed:
  if (err)
    *err = error;
  else
    g_clear_error (&error);

  gst_clear_object (&pipeline);
  gst_clear_object (&appsrc);
  gst_clear_object (&d3d11_convert);
  gst_clear_object (&d3d11_download);
  gst_clear_object (&convert);
  gst_clear_object (&enc);
  gst_clear_object (&appsink);

  return NULL;
}

static GstElement *
build_convert_frame_pipeline (GstElement ** src_element,
    GstElement ** sink_element, GstCaps * from_caps,
    GstVideoCropMeta * cmeta, GstCaps * to_caps, GError ** err)
{
  GstElement *vcrop = NULL, *csp = NULL, *csp2 = NULL, *vscale = NULL;
  GstElement *src = NULL, *sink = NULL, *encoder = NULL, *pipeline;
  GstElement *dl = NULL;
  GstVideoInfo info;
  GError *error = NULL;
  GstCapsFeatures *features;

  features = gst_caps_get_features (from_caps, 0);
  if (features && gst_caps_features_contains (features, "memory:D3D11Memory")) {
    return build_convert_frame_pipeline_d3d11 (src_element, sink_element,
        from_caps, to_caps, err);
  }
#ifdef HAVE_GL
  if (features &&
      gst_caps_features_contains (features, GST_CAPS_FEATURE_MEMORY_GL_MEMORY))
    if (!create_element ("gldownload", &dl, &error))
      goto no_elements;
#endif

  if (cmeta) {
    if (!create_element ("videocrop", &vcrop, &error)) {
      g_error_free (error);
      g_warning
          ("build_convert_frame_pipeline: Buffer has crop metadata but videocrop element is not found. Cropping will be disabled");
    } else {
      if (!create_element ("videoconvert", &csp2, &error))
        goto no_elements;
    }
  }

  /* videoscale is here to correct for the pixel-aspect-ratio for us */
  GST_DEBUG ("creating elements");
  if (!create_element ("appsrc", &src, &error) ||
      !create_element ("videoconvert", &csp, &error) ||
      !create_element ("videoscale", &vscale, &error) ||
      !create_element ("appsink", &sink, &error))
    goto no_elements;

  pipeline = gst_pipeline_new ("videoconvert-pipeline");
  if (pipeline == NULL)
    goto no_pipeline;

  /* Add black borders if necessary to keep the DAR */
  g_object_set (vscale, "add-borders", TRUE, NULL);

  GST_DEBUG ("adding elements");
  gst_bin_add_many (GST_BIN (pipeline), src, csp, vscale, sink, NULL);
  if (vcrop)
    gst_bin_add_many (GST_BIN (pipeline), vcrop, csp2, NULL);
  if (dl)
    gst_bin_add (GST_BIN (pipeline), dl);

  /* set caps */
  g_object_set (src, "caps", from_caps, NULL);
  if (vcrop) {
    gst_video_info_from_caps (&info, from_caps);
    g_object_set (vcrop, "left", cmeta->x, NULL);
    g_object_set (vcrop, "top", cmeta->y, NULL);
    g_object_set (vcrop, "right", GST_VIDEO_INFO_WIDTH (&info) - cmeta->width,
        NULL);
    g_object_set (vcrop, "bottom",
        GST_VIDEO_INFO_HEIGHT (&info) - cmeta->height, NULL);
    GST_DEBUG ("crop meta [x,y,width,height]: %d %d %d %d", cmeta->x, cmeta->y,
        cmeta->width, cmeta->height);
  }
  g_object_set (sink, "caps", to_caps, NULL);

  /* FIXME: linking is still way too expensive, profile this properly */
  if (vcrop) {
    if (!dl) {
      GST_DEBUG ("linking src->csp2");
      if (!gst_element_link_pads (src, "src", csp2, "sink"))
        goto link_failed;
    } else {
      GST_DEBUG ("linking src->dl");
      if (!gst_element_link_pads (src, "src", dl, "sink"))
        goto link_failed;

      GST_DEBUG ("linking dl->csp2");
      if (!gst_element_link_pads (dl, "src", csp2, "sink"))
        goto link_failed;
    }

    GST_DEBUG ("linking csp2->vcrop");
    if (!gst_element_link_pads (csp2, "src", vcrop, "sink"))
      goto link_failed;

    GST_DEBUG ("linking vcrop->csp");
    if (!gst_element_link_pads (vcrop, "src", csp, "sink"))
      goto link_failed;
  } else {
    GST_DEBUG ("linking src->csp");
    if (!dl) {
      if (!gst_element_link_pads (src, "src", csp, "sink"))
        goto link_failed;
    } else {
      GST_DEBUG ("linking src->dl");
      if (!gst_element_link_pads (src, "src", dl, "sink"))
        goto link_failed;

      GST_DEBUG ("linking dl->csp");
      if (!gst_element_link_pads (dl, "src", csp, "sink"))
        goto link_failed;
    }
  }

  GST_DEBUG ("linking csp->vscale");
  if (!gst_element_link_pads_full (csp, "src", vscale, "sink",
          GST_PAD_LINK_CHECK_NOTHING))
    goto link_failed;

  if (caps_are_raw (to_caps)) {
    GST_DEBUG ("linking vscale->sink");

    if (!gst_element_link_pads_full (vscale, "src", sink, "sink",
            GST_PAD_LINK_CHECK_NOTHING))
      goto link_failed;
  } else {
    encoder = get_encoder (to_caps, &error);
    if (!encoder)
      goto no_encoder;
    gst_bin_add (GST_BIN (pipeline), encoder);

    GST_DEBUG ("linking vscale->encoder");
    if (!gst_element_link (vscale, encoder))
      goto link_failed;

    GST_DEBUG ("linking encoder->sink");
    if (!gst_element_link_pads (encoder, "src", sink, "sink"))
      goto link_failed;
  }

  g_object_set (src, "emit-signals", TRUE, NULL);
  g_object_set (sink, "emit-signals", TRUE, NULL);

  *src_element = src;
  *sink_element = sink;

  return pipeline;
  /* ERRORS */
no_encoder:
  {
    gst_object_unref (pipeline);

    GST_ERROR ("could not find an encoder for provided caps");
    if (err)
      *err = error;
    else
      g_error_free (error);

    return NULL;
  }
no_elements:
  {
    if (src)
      gst_object_unref (src);
    if (vcrop)
      gst_object_unref (vcrop);
    if (csp)
      gst_object_unref (csp);
    if (csp2)
      gst_object_unref (csp2);
    if (vscale)
      gst_object_unref (vscale);
    if (sink)
      gst_object_unref (sink);
    GST_ERROR ("Could not convert video frame: %s", error->message);
    if (err)
      *err = error;
    else
      g_error_free (error);
    return NULL;
  }
no_pipeline:
  {
    gst_object_unref (src);
    if (vcrop)
      gst_object_unref (vcrop);
    gst_object_unref (csp);
    if (csp2)
      gst_object_unref (csp2);
    gst_object_unref (vscale);
    gst_object_unref (sink);

    GST_ERROR ("Could not convert video frame: no pipeline (unknown error)");
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
          "Could not convert video frame: no pipeline (unknown error)");
    return NULL;
  }
link_failed:
  {
    gst_object_unref (pipeline);

    GST_ERROR ("Could not convert video frame: failed to link elements");
    if (err)
      *err = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_NEGOTIATION,
          "Could not convert video frame: failed to link elements");
    return NULL;
  }
}

/**
 * gst_video_convert_sample:
 * @sample: a #GstSample
 * @to_caps: the #GstCaps to convert to
 * @timeout: the maximum amount of time allowed for the processing.
 * @error: pointer to a #GError. Can be %NULL.
 *
 * Converts a raw video buffer into the specified output caps.
 *
 * The output caps can be any raw video formats or any image formats (jpeg, png, ...).
 *
 * The width, height and pixel-aspect-ratio can also be specified in the output caps.
 *
 * Returns: (nullable) (transfer full): The converted #GstSample, or %NULL if an error happened (in which case @err
 * will point to the #GError).
 */
GstSample *
gst_video_convert_sample (GstSample * sample, const GstCaps * to_caps,
    GstClockTime timeout, GError ** error)
{
  GstMessage *msg;
  GstBuffer *buf;
  GstSample *result = NULL;
  GError *err = NULL;
  GstBus *bus;
  GstCaps *from_caps, *to_caps_copy = NULL;
  GstFlowReturn ret;
  GstElement *pipeline, *src, *sink;
  guint i, n;

  g_return_val_if_fail (sample != NULL, NULL);
  g_return_val_if_fail (to_caps != NULL, NULL);

  buf = gst_sample_get_buffer (sample);
  g_return_val_if_fail (buf != NULL, NULL);

  from_caps = gst_sample_get_caps (sample);
  g_return_val_if_fail (from_caps != NULL, NULL);

  to_caps_copy = gst_caps_new_empty ();
  n = gst_caps_get_size (to_caps);
  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (to_caps, i);

    s = gst_structure_copy (s);
    gst_structure_remove_field (s, "framerate");
    gst_caps_append_structure (to_caps_copy, s);
  }

  pipeline =
      build_convert_frame_pipeline (&src, &sink, from_caps,
      gst_buffer_get_video_crop_meta (buf), to_caps_copy, &err);
  if (!pipeline)
    goto no_pipeline;

  /* now set the pipeline to the paused state, after we push the buffer into
   * appsrc, this should preroll the converted buffer in appsink */
  GST_DEBUG ("running conversion pipeline to caps %" GST_PTR_FORMAT,
      to_caps_copy);
  if (gst_element_set_state (pipeline,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    goto state_change_failed;

  /* feed buffer in appsrc */
  GST_DEBUG ("feeding buffer %p, size %" G_GSIZE_FORMAT ", caps %"
      GST_PTR_FORMAT, buf, gst_buffer_get_size (buf), from_caps);
  g_signal_emit_by_name (src, "push-buffer", buf, &ret);

  /* now see what happens. We either got an error somewhere or the pipeline
   * prerolled */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus,
      timeout, GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE);

  if (msg) {
    switch (GST_MESSAGE_TYPE (msg)) {
      case GST_MESSAGE_ASYNC_DONE:
      {
        /* we're prerolled, get the frame from appsink */
        g_signal_emit_by_name (sink, "pull-preroll", &result);

        if (result) {
          GST_DEBUG ("conversion successful: result = %p", result);
        } else {
          GST_ERROR ("prerolled but no result frame?!");
        }
        break;
      }
      case GST_MESSAGE_ERROR:{
        gchar *dbg = NULL;

        gst_message_parse_error (msg, &err, &dbg);
        if (err) {
          GST_ERROR ("Could not convert video frame: %s", err->message);
          GST_DEBUG ("%s [debug: %s]", err->message, GST_STR_NULL (dbg));
          if (error)
            *error = err;
          else
            g_error_free (err);
        }
        g_free (dbg);
        break;
      }
      default:{
        g_return_val_if_reached (NULL);
      }
    }
    gst_message_unref (msg);
  } else {
    GST_ERROR ("Could not convert video frame: timeout during conversion");
    if (error)
      *error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
          "Could not convert video frame: timeout during conversion");
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (bus);
  gst_object_unref (pipeline);
  gst_caps_unref (to_caps_copy);

  return result;

  /* ERRORS */
no_pipeline:
state_change_failed:
  {
    gst_caps_unref (to_caps_copy);

    if (error)
      *error = err;
    else
      g_error_free (err);

    return NULL;
  }
}

typedef struct
{
  gint ref_count;
  GMutex mutex;
  GstElement *pipeline;
  GstVideoConvertSampleCallback callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
  GMainContext *context;
  GstSample *sample;
  GSource *timeout_source;
  gboolean finished;

  /* Results */
  GstSample *converted_sample;
  GError *error;
} GstVideoConvertSampleContext;

static GstVideoConvertSampleContext *
gst_video_convert_frame_context_ref (GstVideoConvertSampleContext * ctx)
{
  g_atomic_int_inc (&ctx->ref_count);

  return ctx;
}

static void
gst_video_convert_frame_context_unref (GstVideoConvertSampleContext * ctx)
{
  if (!g_atomic_int_dec_and_test (&ctx->ref_count))
    return;

  g_mutex_clear (&ctx->mutex);
  if (ctx->timeout_source)
    g_source_destroy (ctx->timeout_source);
  if (ctx->sample)
    gst_sample_unref (ctx->sample);
  if (ctx->converted_sample)
    gst_sample_unref (ctx->converted_sample);
  g_clear_error (&ctx->error);
  g_main_context_unref (ctx->context);

  /* The pipeline was already destroyed in finish() earlier and we
   * must not end up here without finish() being called */
  g_warn_if_fail (ctx->pipeline == NULL);

  g_free (ctx);
}

static gboolean
convert_frame_dispatch_callback (GstVideoConvertSampleContext * ctx)
{
  GstSample *sample;
  GError *error;

  g_return_val_if_fail (ctx->converted_sample != NULL
      || ctx->error != NULL, FALSE);

  sample = ctx->converted_sample;
  error = ctx->error;
  ctx->converted_sample = NULL;
  ctx->error = NULL;

  ctx->callback (sample, error, ctx->user_data);

  if (ctx->destroy_notify)
    ctx->destroy_notify (ctx->user_data);

  return FALSE;
}

static void
convert_frame_stop_pipeline (GstElement * element, gpointer user_data)
{
  gst_element_set_state (element, GST_STATE_NULL);
}

static void
convert_frame_finish (GstVideoConvertSampleContext * context,
    GstSample * sample, GError * error)
{
  GSource *source;

  g_return_if_fail (!context->finished);
  g_return_if_fail (sample != NULL || error != NULL);

  context->finished = TRUE;
  context->converted_sample = sample;
  context->error = error;

  if (context->timeout_source)
    g_source_destroy (context->timeout_source);
  context->timeout_source = NULL;

  source = g_timeout_source_new (0);
  g_source_set_callback (source,
      (GSourceFunc) convert_frame_dispatch_callback,
      gst_video_convert_frame_context_ref (context),
      (GDestroyNotify) gst_video_convert_frame_context_unref);
  g_source_attach (source, context->context);
  g_source_unref (source);

  /* Asynchronously stop the pipeline here: this will set its
   * state to NULL and get rid of its last reference, which in turn
   * will get rid of all remaining references to our context and free
   * it too. We can't do this directly here as we might be called from
   * a streaming thread.
   *
   * We don't use the main loop here because the user might shut down it
   * immediately after getting the result of the conversion above.
   */
  if (context->pipeline) {
    gst_element_call_async (context->pipeline, convert_frame_stop_pipeline,
        NULL, NULL);
    gst_object_unref (context->pipeline);
    context->pipeline = NULL;
  }
}

static gboolean
convert_frame_timeout_callback (GstVideoConvertSampleContext * context)
{
  GError *error;

  g_mutex_lock (&context->mutex);

  if (context->finished)
    goto done;

  GST_ERROR ("Could not convert video frame: timeout");

  error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
      "Could not convert video frame: timeout");

  convert_frame_finish (context, NULL, error);

done:
  g_mutex_unlock (&context->mutex);
  return FALSE;
}

static gboolean
convert_frame_bus_callback (GstBus * bus, GstMessage * message,
    GstVideoConvertSampleContext * context)
{
  g_mutex_lock (&context->mutex);

  if (context->finished)
    goto done;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:{
      GError *error;
      gchar *dbg = NULL;

      gst_message_parse_error (message, &error, &dbg);

      GST_ERROR ("Could not convert video frame: %s", error->message);
      GST_DEBUG ("%s [debug: %s]", error->message, GST_STR_NULL (dbg));

      convert_frame_finish (context, NULL, error);

      g_free (dbg);
      break;
    }
    default:
      break;
  }

done:
  g_mutex_unlock (&context->mutex);

  return FALSE;
}

static void
convert_frame_need_data_callback (GstElement * src, guint size,
    GstVideoConvertSampleContext * context)
{
  GstFlowReturn ret = GST_FLOW_ERROR;
  GError *error;
  GstBuffer *buffer;

  g_mutex_lock (&context->mutex);

  if (context->finished)
    goto done;

  buffer = gst_sample_get_buffer (context->sample);
  g_signal_emit_by_name (src, "push-buffer", buffer, &ret);
  gst_sample_unref (context->sample);
  context->sample = NULL;

  if (ret != GST_FLOW_OK) {
    GST_ERROR ("Could not push video frame: %s", gst_flow_get_name (ret));

    error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
        "Could not push video frame: %s", gst_flow_get_name (ret));

    convert_frame_finish (context, NULL, error);
  }

done:
  g_mutex_unlock (&context->mutex);

  g_signal_handlers_disconnect_by_func (src, convert_frame_need_data_callback,
      context);
}

static GstFlowReturn
convert_frame_new_preroll_callback (GstElement * sink,
    GstVideoConvertSampleContext * context)
{
  GstSample *sample = NULL;
  GError *error = NULL;

  g_mutex_lock (&context->mutex);

  if (context->finished)
    goto done;

  g_signal_emit_by_name (sink, "pull-preroll", &sample);

  if (!sample) {
    error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_FAILED,
        "Could not get converted video sample");
  }
  convert_frame_finish (context, sample, error);

done:
  g_mutex_unlock (&context->mutex);

  g_signal_handlers_disconnect_by_func (sink, convert_frame_need_data_callback,
      context);

  return GST_FLOW_OK;
}

/**
 * gst_video_convert_sample_async:
 * @sample: a #GstSample
 * @to_caps: the #GstCaps to convert to
 * @timeout: the maximum amount of time allowed for the processing.
 * @callback: %GstVideoConvertSampleCallback that will be called after conversion.
 * @user_data: extra data that will be passed to the @callback
 * @destroy_notify: %GDestroyNotify to be called after @user_data is not needed anymore
 *
 * Converts a raw video buffer into the specified output caps.
 *
 * The output caps can be any raw video formats or any image formats (jpeg, png, ...).
 *
 * The width, height and pixel-aspect-ratio can also be specified in the output caps.
 *
 * @callback will be called after conversion, when an error occurred or if conversion didn't
 * finish after @timeout. @callback will always be called from the thread default
 * %GMainContext, see g_main_context_get_thread_default(). If GLib before 2.22 is used,
 * this will always be the global default main context.
 *
 * @destroy_notify will be called after the callback was called and @user_data is not needed
 * anymore.
 */
void
gst_video_convert_sample_async (GstSample * sample,
    const GstCaps * to_caps, GstClockTime timeout,
    GstVideoConvertSampleCallback callback, gpointer user_data,
    GDestroyNotify destroy_notify)
{
  GMainContext *context = NULL;
  GError *error = NULL;
  GstBus *bus;
  GstBuffer *buf;
  GstCaps *from_caps, *to_caps_copy = NULL;
  GstElement *pipeline, *src, *sink;
  guint i, n;
  GSource *source;
  GstVideoConvertSampleContext *ctx;

  g_return_if_fail (sample != NULL);
  buf = gst_sample_get_buffer (sample);
  g_return_if_fail (buf != NULL);

  g_return_if_fail (to_caps != NULL);

  from_caps = gst_sample_get_caps (sample);
  g_return_if_fail (from_caps != NULL);
  g_return_if_fail (callback != NULL);

  context = g_main_context_get_thread_default ();

  if (!context)
    context = g_main_context_default ();

  to_caps_copy = gst_caps_new_empty ();
  n = gst_caps_get_size (to_caps);
  for (i = 0; i < n; i++) {
    GstStructure *s = gst_caps_get_structure (to_caps, i);

    s = gst_structure_copy (s);
    gst_structure_remove_field (s, "framerate");
    gst_caps_append_structure (to_caps_copy, s);
  }

  /* There's a reference cycle between the context and the pipeline, which is
   * broken up once the finish() is called on the context. At latest when the
   * timeout triggers the context will be freed */
  ctx = g_new0 (GstVideoConvertSampleContext, 1);
  ctx->ref_count = 1;
  g_mutex_init (&ctx->mutex);
  ctx->sample = gst_sample_ref (sample);
  ctx->callback = callback;
  ctx->user_data = user_data;
  ctx->destroy_notify = destroy_notify;
  ctx->context = g_main_context_ref (context);
  ctx->finished = FALSE;

  pipeline =
      build_convert_frame_pipeline (&src, &sink, from_caps,
      gst_buffer_get_video_crop_meta (buf), to_caps_copy, &error);
  if (!pipeline)
    goto no_pipeline;
  ctx->pipeline = pipeline;

  bus = gst_element_get_bus (pipeline);

  if (timeout != GST_CLOCK_TIME_NONE) {
    ctx->timeout_source = g_timeout_source_new (timeout / GST_MSECOND);
    g_source_set_callback (ctx->timeout_source,
        (GSourceFunc) convert_frame_timeout_callback,
        gst_video_convert_frame_context_ref (ctx),
        (GDestroyNotify) gst_video_convert_frame_context_unref);
    g_source_attach (ctx->timeout_source, context);
  }

  g_signal_connect_data (src, "need-data",
      G_CALLBACK (convert_frame_need_data_callback),
      gst_video_convert_frame_context_ref (ctx),
      (GClosureNotify) gst_video_convert_frame_context_unref, 0);
  g_signal_connect_data (sink, "new-preroll",
      G_CALLBACK (convert_frame_new_preroll_callback),
      gst_video_convert_frame_context_ref (ctx),
      (GClosureNotify) gst_video_convert_frame_context_unref, 0);

  source = gst_bus_create_watch (bus);
  g_source_set_callback (source, (GSourceFunc) convert_frame_bus_callback,
      gst_video_convert_frame_context_ref (ctx),
      (GDestroyNotify) gst_video_convert_frame_context_unref);
  g_source_attach (source, context);
  g_source_unref (source);
  gst_object_unref (bus);

  if (gst_element_set_state (pipeline,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE)
    goto state_change_failed;

  gst_caps_unref (to_caps_copy);

  gst_video_convert_frame_context_unref (ctx);

  return;
  /* ERRORS */
no_pipeline:
  {
    gst_caps_unref (to_caps_copy);

    g_mutex_lock (&ctx->mutex);
    convert_frame_finish (ctx, NULL, error);
    g_mutex_unlock (&ctx->mutex);
    gst_video_convert_frame_context_unref (ctx);

    return;
  }
state_change_failed:
  {
    gst_caps_unref (to_caps_copy);

    error = g_error_new (GST_CORE_ERROR, GST_CORE_ERROR_STATE_CHANGE,
        "failed to change state to PLAYING");

    g_mutex_lock (&ctx->mutex);
    convert_frame_finish (ctx, NULL, error);
    g_mutex_unlock (&ctx->mutex);
    gst_video_convert_frame_context_unref (ctx);

    return;
  }
}
