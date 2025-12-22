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
#include <config.h>
#endif

#include "gstd3d12download.h"
#include "gstd3d12pluginutils.h"

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_download_debug);
#define GST_CAT_DEFAULT gst_d3d12_download_debug

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D12_ALL_FORMATS)));

static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY, GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE (GST_D3D12_ALL_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            GST_D3D12_ALL_FORMATS)));

struct _GstD3D12Download
{
  GstD3D12BaseFilter parent;
};

#define gst_d3d12_download_parent_class parent_class
G_DEFINE_TYPE (GstD3D12Download, gst_d3d12_download,
    GST_TYPE_D3D12_BASE_FILTER);

static GstCaps *gst_d3d12_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_d3d12_download_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_d3d12_download_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static GstFlowReturn gst_d3d12_download_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_d3d12_download_class_init (GstD3D12DownloadClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D12 Downloader", "Filter/Video",
      "Downloads Direct3D12 texture memory into system memory",
      "Seungha Yang <seungha@centricular.com>");

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d12_download_transform_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_download_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_download_decide_allocation);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_d3d12_download_transform);

  GST_DEBUG_CATEGORY_INIT (gst_d3d12_download_debug,
      "d3d12download", 0, "d3d12download Element");
}

static void
gst_d3d12_download_init (GstD3D12Download * self)
{
}

static GstCaps *
_set_caps_features (const GstCaps * caps, const gchar * feature_name)
{
  GstCaps *tmp = gst_caps_copy (caps);
  guint n = gst_caps_get_size (tmp);
  guint i = 0;

  for (i = 0; i < n; i++) {
    gst_caps_set_features (tmp, i,
        gst_caps_features_from_string (feature_name));
  }

  return tmp;
}

static GstCaps *
gst_d3d12_download_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
    tmp = gst_caps_merge (gst_caps_ref (caps), tmp);
  } else {
    GstCaps *newcaps;
    tmp = gst_caps_ref (caps);

    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY);
    tmp = gst_caps_merge (tmp, newcaps);
  }

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, result);

  return result;
}

static gboolean
gst_d3d12_download_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D12BaseFilter *filter = GST_D3D12_BASE_FILTER (trans);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;
  gboolean is_d3d12 = FALSE;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (!decide_query)
    return TRUE;

  gst_query_parse_allocation (query, &caps, nullptr);

  if (!caps) {
    GST_WARNING_OBJECT (filter, "Allocation query without caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstCapsFeatures *features;
    GstStructure *config;

    features = gst_caps_get_features (caps, 0);

    if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY)) {
      GST_DEBUG_OBJECT (filter, "upstream support d3d12 memory");
      pool = gst_d3d12_buffer_pool_new (filter->device);
      is_d3d12 = TRUE;
    } else {
      pool = gst_video_buffer_pool_new ();
    }

    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!is_d3d12) {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config)) {
      GST_ERROR_OBJECT (filter, "Bufferpool config failed");
      gst_object_unref (pool);
      return FALSE;
    }

    /* d3d12 buffer pool will update buffer size based on allocated texture,
     * get size from config again */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config,
        nullptr, &size, nullptr, nullptr);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, nullptr);

  return TRUE;
}

static gboolean
gst_d3d12_download_decide_allocation (GstBaseTransform * trans,
    GstQuery * query)
{
  GstBufferPool *pool = nullptr;
  GstStructure *config;
  guint min, max, size;
  gboolean update_pool;
  GstCaps *outcaps = nullptr;

  gst_query_parse_allocation (query, &outcaps, nullptr);

  if (!outcaps) {
    GST_WARNING_OBJECT (trans, "Allocation query without caps");
    return FALSE;
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_from_caps (&vinfo, outcaps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool) {
    pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_set_config (pool, config);

  /* d3d12 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->decide_allocation (trans,
      query);
}

static GstFlowReturn
gst_d3d12_download_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstD3D12BaseFilter *filter = GST_D3D12_BASE_FILTER (trans);
  GstVideoFrame in_frame, out_frame;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!gst_video_frame_map (&in_frame, &filter->in_info, inbuf, GST_MAP_READ)) {
    GST_ERROR_OBJECT (filter, "Couldn't map input frame");
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&out_frame,
          &filter->out_info, outbuf, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (filter, "Couldn't map output frame");
    gst_video_frame_unmap (&in_frame);
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_copy (&out_frame, &in_frame)) {
    GST_ERROR_OBJECT (filter, "Copy failed");
    ret = GST_FLOW_ERROR;
  }

  gst_video_frame_unmap (&out_frame);
  gst_video_frame_unmap (&in_frame);

  return ret;
}
