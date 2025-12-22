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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstd3d11basefilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_base_filter_debug);
#define GST_CAT_DEFAULT gst_d3d11_base_filter_debug

#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

enum
{
  PROP_0,
  PROP_ADAPTER,
};

#define DEFAULT_ADAPTER -1

#define gst_d3d11_base_filter_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstD3D11BaseFilter, gst_d3d11_base_filter,
    GST_TYPE_BASE_TRANSFORM, GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT,
        "d3d11basefilter", 0, "d3d11 basefilter"));

static void gst_d3d11_base_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d11_base_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_d3d11_base_filter_dispose (GObject * object);
static void gst_d3d11_base_filter_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_d3d11_base_filter_start (GstBaseTransform * trans);
static gboolean gst_d3d11_base_filter_stop (GstBaseTransform * trans);
static gboolean gst_d3d11_base_filter_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_d3d11_base_filter_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, gsize * size);
static gboolean
gst_d3d11_base_filter_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static void gst_d3d11_base_filter_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);
static gboolean gst_d3d11_base_filter_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);

static void
gst_d3d11_base_filter_class_init (GstD3D11BaseFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->set_property = gst_d3d11_base_filter_set_property;
  gobject_class->get_property = gst_d3d11_base_filter_get_property;
  gobject_class->dispose = gst_d3d11_base_filter_dispose;

  /**
   * GstD3D11BaseFilter:adapter:
   *
   * Adapter index for creating device (-1 for default)
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_ADAPTER,
      g_param_spec_int ("adapter", "Adapter",
          "Adapter index for creating device (-1 for default)",
          -1, G_MAXINT32, DEFAULT_ADAPTER,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
              G_PARAM_STATIC_STRINGS)));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_set_context);

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->start = GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_stop);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_set_caps);
  trans_class->get_unit_size =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_get_unit_size);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_query);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_before_transform);
  trans_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_d3d11_base_filter_transform_meta);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D11_BASE_FILTER,
      (GstPluginAPIFlags) 0);
  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static void
gst_d3d11_base_filter_init (GstD3D11BaseFilter * filter)
{
  filter->adapter = DEFAULT_ADAPTER;
}

static void
gst_d3d11_base_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      filter->adapter = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_base_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (object);

  switch (prop_id) {
    case PROP_ADAPTER:
      g_value_set_int (value, filter->adapter);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d11_base_filter_dispose (GObject * object)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (object);

  gst_clear_object (&filter->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d11_base_filter_set_context (GstElement * element, GstContext * context)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (element);

  gst_d3d11_handle_set_context (element,
      context, filter->adapter, &filter->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d11_base_filter_start (GstBaseTransform * trans)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);

  if (!gst_d3d11_ensure_element_data (GST_ELEMENT_CAST (filter),
          filter->adapter, &filter->device)) {
    GST_ERROR_OBJECT (filter, "Failed to get D3D11 device");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d11_base_filter_stop (GstBaseTransform * trans)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);

  gst_clear_object (&filter->device);

  return TRUE;
}

static gboolean
gst_d3d11_base_filter_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstVideoInfo in_info, out_info;
  GstD3D11BaseFilterClass *klass;
  gboolean res;

  if (!filter->device) {
    GST_ERROR_OBJECT (filter, "No available D3D11 device");
    return FALSE;
  }

  /* input caps */
  if (!gst_video_info_from_caps (&in_info, incaps))
    goto invalid_caps;

  /* output caps */
  if (!gst_video_info_from_caps (&out_info, outcaps))
    goto invalid_caps;

  klass = GST_D3D11_BASE_FILTER_GET_CLASS (filter);
  if (klass->set_info)
    res = klass->set_info (filter, incaps, &in_info, outcaps, &out_info);
  else
    res = TRUE;

  if (res) {
    filter->in_info = in_info;
    filter->out_info = out_info;
  }

  return res;

  /* ERRORS */
invalid_caps:
  {
    GST_ERROR_OBJECT (filter, "invalid caps");
    return FALSE;
  }
}

static gboolean
gst_d3d11_base_filter_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
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
gst_d3d11_base_filter_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      gboolean ret;
      ret = gst_d3d11_handle_context_query (GST_ELEMENT (filter), query,
          filter->device);
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
gst_d3d11_base_filter_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstD3D11BaseFilter *self = GST_D3D11_BASE_FILTER (trans);
  GstD3D11Memory *dmem;
  GstMemory *mem;
  gboolean update_device = FALSE;
  GstCaps *in_caps = NULL;
  GstCaps *out_caps = NULL;
  GstClockTime timestamp, stream_time;

  timestamp = GST_BUFFER_PTS (buffer);
  stream_time =
      gst_segment_to_stream_time (&trans->segment, GST_FORMAT_TIME, timestamp);

  if (GST_CLOCK_TIME_IS_VALID (stream_time))
    gst_object_sync_values (GST_OBJECT (self), stream_time);

  mem = gst_buffer_peek_memory (buffer, 0);
  /* Can happens (e.g., d3d11upload) */
  if (!gst_is_d3d11_memory (mem))
    return;

  dmem = GST_D3D11_MEMORY_CAST (mem);
  /* Same device, nothing to do */
  if (dmem->device == self->device)
    return;

  /* Can accept any device, update */
  if (self->adapter < 0) {
    update_device = TRUE;
  } else {
    guint adapter = 0;

    g_object_get (dmem->device, "adapter", &adapter, NULL);
    /* The same GPU as what user wanted, update */
    if (adapter == (guint) self->adapter)
      update_device = TRUE;
  }

  if (!update_device)
    return;

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, dmem->device);

  gst_object_unref (self->device);
  self->device = (GstD3D11Device *) gst_object_ref (dmem->device);

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

  /* subclass will update internal object.
   * Note that gst_base_transform_reconfigure() might not trigger this
   * unless caps was changed meanwhile */
  gst_d3d11_base_filter_set_caps (trans, in_caps, out_caps);

  /* Mark reconfigure so that we can update pool */
  gst_base_transform_reconfigure_src (trans);

out:
  gst_clear_caps (&in_caps);
  gst_clear_caps (&out_caps);

  return;
}

static gboolean
gst_d3d11_base_filter_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf)
{
  const GstMetaInfo *info = meta->info;
  const gchar *const *tags;

  tags = gst_meta_api_type_get_tags (info->api);

  if (!tags || (g_strv_length ((gchar **) tags) == 1
          && gst_meta_api_type_has_tag (info->api, META_TAG_VIDEO))) {
    return TRUE;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->transform_meta (trans, outbuf,
      meta, inbuf);
}
