/* GStreamer
 * Copyright (C) <2019> Seungha Yang <seungha.yang@navercorp.com>
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
 * GstCudaBaseTransform:
 *
 * Base class for CUDA transformers
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstcudabasetransform.h"

GST_DEBUG_CATEGORY_STATIC (gst_cuda_base_transform_debug);
#define GST_CAT_DEFAULT gst_cuda_base_transform_debug

/* cached quark to avoid contention on the global quark table lock */
#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

enum
{
  PROP_0,
  PROP_DEVICE_ID,
};

#define DEFAULT_DEVICE_ID -1

#define gst_cuda_base_transform_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstCudaBaseTransform, gst_cuda_base_transform,
    GST_TYPE_BASE_TRANSFORM);

static void gst_cuda_base_transform_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_cuda_base_transform_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_cuda_base_transform_dispose (GObject * object);
static void gst_cuda_base_transform_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_cuda_base_transform_start (GstBaseTransform * trans);
static gboolean gst_cuda_base_transform_stop (GstBaseTransform * trans);
static gboolean gst_cuda_base_transform_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_cuda_base_transform_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static gboolean gst_cuda_base_transform_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static void gst_cuda_base_transform_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);
static gboolean
gst_cuda_base_transform_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);

static void
gst_cuda_base_transform_class_init (GstCudaBaseTransformClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstBaseTransformClass *trans_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_cuda_base_transform_set_property;
  gobject_class->get_property = gst_cuda_base_transform_get_property;
  gobject_class->dispose = gst_cuda_base_transform_dispose;

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_int ("cuda-device-id",
          "Cuda Device ID",
          "Set the GPU device to use for operations (-1 = auto)",
          -1, G_MAXINT, DEFAULT_DEVICE_ID,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_cuda_base_transform_set_context);

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->start = GST_DEBUG_FUNCPTR (gst_cuda_base_transform_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_cuda_base_transform_stop);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_cuda_base_transform_set_caps);
  trans_class->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_cuda_base_transform_get_unit_size);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_cuda_base_transform_query);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_cuda_base_transform_before_transform);
  trans_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_cuda_base_transform_transform_meta);

  GST_DEBUG_CATEGORY_INIT (gst_cuda_base_transform_debug,
      "cudabasefilter", 0, "cudabasefilter Element");

  gst_type_mark_as_plugin_api (GST_TYPE_CUDA_BASE_TRANSFORM, 0);
  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static void
gst_cuda_base_transform_init (GstCudaBaseTransform * filter)
{
  filter->device_id = DEFAULT_DEVICE_ID;
}

static void
gst_cuda_base_transform_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (object);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      filter->device_id = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cuda_base_transform_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (object);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      g_value_set_int (value, filter->device_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_cuda_base_transform_dispose (GObject * object)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (object);

  gst_clear_object (&filter->context);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_cuda_base_transform_set_context (GstElement * element, GstContext * context)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (element);

  gst_cuda_handle_set_context (element,
      context, filter->device_id, &filter->context);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_cuda_base_transform_start (GstBaseTransform * trans)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (trans);

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (filter),
          filter->device_id, &filter->context)) {
    GST_ERROR_OBJECT (filter, "Failed to get CUDA context");
    return FALSE;
  }

  filter->stream = gst_cuda_stream_new (filter->context);
  if (!filter->stream) {
    GST_WARNING_OBJECT (filter,
        "Could not create cuda stream, will use default stream");
  }

  return TRUE;
}

static gboolean
gst_cuda_base_transform_stop (GstBaseTransform * trans)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (trans);

  gst_clear_cuda_stream (&filter->stream);
  gst_clear_object (&filter->context);

  return TRUE;
}

static gboolean
gst_cuda_base_transform_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (trans);
  GstVideoInfo in_info, out_info;
  GstCudaBaseTransformClass *klass;
  gboolean res;

  if (!filter->context) {
    GST_ERROR_OBJECT (filter, "No available CUDA context");
    return FALSE;
  }

  /* input caps */
  if (!gst_video_info_from_caps (&in_info, incaps)) {
    GST_ERROR_OBJECT (filter, "invalid incaps %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }

  /* output caps */
  if (!gst_video_info_from_caps (&out_info, outcaps)) {
    GST_ERROR_OBJECT (filter, "invalid incaps %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }

  klass = GST_CUDA_BASE_TRANSFORM_GET_CLASS (filter);
  if (klass->set_info)
    res = klass->set_info (filter, incaps, &in_info, outcaps, &out_info);
  else
    res = TRUE;

  if (res) {
    filter->in_info = in_info;
    filter->out_info = out_info;
  }

  return res;
}

static gboolean
gst_cuda_base_transform_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    gsize * size)
{
  gboolean ret = FALSE;
  GstVideoInfo info;

  ret = gst_video_info_from_caps (&info, caps);
  if (ret)
    *size = GST_VIDEO_INFO_SIZE (&info);

  return TRUE;
}

static gboolean
gst_cuda_base_transform_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstCudaBaseTransform *filter = GST_CUDA_BASE_TRANSFORM (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      gboolean ret;
      ret = gst_cuda_handle_context_query (GST_ELEMENT (filter), query,
          filter->context);
      if (ret)
        return TRUE;
      break;
    }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static void
gst_cuda_base_transform_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstCudaBaseTransform *self = GST_CUDA_BASE_TRANSFORM (trans);
  GstCudaMemory *cmem;
  GstMemory *mem;
  gboolean update_context = FALSE;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;

  in_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SINK_PAD (trans));
  if (!in_caps) {
    GST_WARNING_OBJECT (self, "sinkpad has null caps");
    goto out;
  }

  out_caps = gst_pad_get_current_caps (GST_BASE_TRANSFORM_SRC_PAD (trans));
  if (!out_caps) {
    GST_WARNING_OBJECT (self, "Has no configured output caps");
    goto out;
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  /* Can happens (e.g., d3d11upload) */
  if (!gst_is_cuda_memory (mem))
    goto out;

  cmem = GST_CUDA_MEMORY_CAST (mem);
  /* Same context, nothing to do */
  if (self->context == cmem->context)
    goto out;

  /* Can accept any device, update */
  if (self->device_id < 0) {
    update_context = TRUE;
  } else {
    guint device_id = 0;

    g_object_get (cmem->context, "cuda-device-id", &device_id, NULL);
    /* The same GPU as what user wanted, update */
    if (device_id == (guint) self->device_id)
      update_context = TRUE;
  }

  if (!update_context)
    goto out;

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->context, cmem->context);

  gst_clear_cuda_stream (&self->stream);
  gst_object_unref (self->context);
  self->context = gst_object_ref (cmem->context);

  self->stream = gst_cuda_stream_new (self->context);

  /* subclass will update internal object.
   * Note that gst_base_transform_reconfigure() might not trigger this
   * unless caps was changed meanwhile */
  gst_cuda_base_transform_set_caps (trans, in_caps, out_caps);

  /* Mark reconfigure so that we can update pool */
  gst_base_transform_reconfigure_src (trans);

out:
  gst_clear_caps (&in_caps);
  gst_clear_caps (&out_caps);

  return;
}

static gboolean
gst_cuda_base_transform_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags || (g_strv_length ((gchar **) tags) == 1
          && gst_meta_api_type_has_tag (info->api, META_TAG_VIDEO)))
    return TRUE;

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (trans, outbuf,
      meta, inbuf);
}
