/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d12basefilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_base_filter_debug);
#define GST_CAT_DEFAULT gst_d3d12_base_filter_debug

#define META_TAG_VIDEO meta_tag_video_quark
static GQuark meta_tag_video_quark;

#define gst_d3d12_base_filter_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstD3D12BaseFilter, gst_d3d12_base_filter,
    GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_d3d12_base_filter_debug,
        "d3d12basefilter", 0, "d3d12 basefilter"));

static void gst_d3d12_base_filter_dispose (GObject * object);
static void gst_d3d12_base_filter_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_d3d12_base_filter_start (GstBaseTransform * trans);
static gboolean gst_d3d12_base_filter_stop (GstBaseTransform * trans);
static gboolean gst_d3d12_base_filter_set_caps (GstBaseTransform * trans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean
gst_d3d12_base_filter_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query);
static void gst_d3d12_base_filter_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer);
static gboolean gst_d3d12_base_filter_transform_meta (GstBaseTransform * trans,
    GstBuffer * outbuf, GstMeta * meta, GstBuffer * inbuf);

static void
gst_d3d12_base_filter_class_init (GstD3D12BaseFilterClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gobject_class->dispose = gst_d3d12_base_filter_dispose;

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_set_context);

  trans_class->start = GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_start);
  trans_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_stop);
  trans_class->set_caps = GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_set_caps);
  trans_class->query = GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_query);
  trans_class->before_transform =
      GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_before_transform);
  trans_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_d3d12_base_filter_transform_meta);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_BASE_FILTER,
      (GstPluginAPIFlags) 0);
  meta_tag_video_quark = g_quark_from_static_string (GST_META_TAG_VIDEO_STR);
}

static void
gst_d3d12_base_filter_init (GstD3D12BaseFilter * filter)
{
}

static void
gst_d3d12_base_filter_dispose (GObject * object)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (object);

  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_d3d12_base_filter_set_context (GstElement * element, GstContext * context)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (element);

  gst_d3d12_handle_set_context (element, context, -1, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d12_base_filter_start (GstBaseTransform * trans)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (trans);

  if (!gst_d3d12_ensure_element_data (GST_ELEMENT_CAST (self),
          -1, &self->device)) {
    GST_ERROR_OBJECT (self, "Failed to get D3D12 device");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_d3d12_base_filter_stop (GstBaseTransform * trans)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (trans);

  gst_clear_object (&self->device);

  return TRUE;
}

static gboolean
gst_d3d12_base_filter_set_caps (GstBaseTransform * trans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (trans);
  GstVideoInfo in_info, out_info;
  GstD3D12BaseFilterClass *klass;
  gboolean res;

  if (!self->device) {
    GST_ERROR_OBJECT (self, "No available D3D12 device");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&in_info, incaps)) {
    GST_ERROR_OBJECT (self, "Invalid input caps %" GST_PTR_FORMAT, incaps);
    return FALSE;
  }

  if (!gst_video_info_from_caps (&out_info, outcaps)) {
    GST_ERROR_OBJECT (self, "Invalid output caps %" GST_PTR_FORMAT, outcaps);
    return FALSE;
  }

  klass = GST_D3D12_BASE_FILTER_GET_CLASS (self);
  if (klass->set_info)
    res = klass->set_info (self, incaps, &in_info, outcaps, &out_info);
  else
    res = TRUE;

  if (res) {
    self->in_info = in_info;
    self->out_info = out_info;
  }

  return res;
}

static gboolean
gst_d3d12_base_filter_query (GstBaseTransform * trans,
    GstPadDirection direction, GstQuery * query)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (trans);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
    {
      if (gst_d3d12_handle_context_query (GST_ELEMENT (self), query,
              self->device)) {
        return TRUE;
      }
      break;
    }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (trans, direction,
      query);
}

static void
gst_d3d12_base_filter_before_transform (GstBaseTransform * trans,
    GstBuffer * buffer)
{
  GstD3D12BaseFilter *self = GST_D3D12_BASE_FILTER (trans);
  GstD3D12Memory *dmem;
  GstMemory *mem;
  GstCaps *in_caps = nullptr;
  GstCaps *out_caps = nullptr;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d12_memory (mem))
    return;

  dmem = GST_D3D12_MEMORY_CAST (mem);
  /* d3d12 devices are singletons per adapter */
  if (dmem->device == self->device)
    return;

  GST_INFO_OBJECT (self, "Updating device %" GST_PTR_FORMAT " -> %"
      GST_PTR_FORMAT, self->device, dmem->device);

  gst_object_unref (self->device);
  self->device = (GstD3D12Device *) gst_object_ref (dmem->device);

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
  gst_d3d12_base_filter_set_caps (trans, in_caps, out_caps);

  /* Mark reconfigure so that we can update pool */
  gst_base_transform_reconfigure_src (trans);

out:
  gst_clear_caps (&in_caps);
  gst_clear_caps (&out_caps);

  return;
}

static gboolean
gst_d3d12_base_filter_transform_meta (GstBaseTransform * trans,
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
