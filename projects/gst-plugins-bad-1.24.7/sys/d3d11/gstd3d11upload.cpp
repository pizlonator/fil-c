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
 * SECTION:element-d3d11upload
 * @title: d3d11upload
 *
 * Upload video frame to Direct3D11 texture memory
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! d3d11upload ! d3d11videosinkelement
 * ```
 *   This pipeline will upload video test frame (system memory) into Direct3D11
 * textures and d3d11videosinkelement will display frames on screen.
 *
 * Since: 1.18
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstd3d11upload.h"

#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_d3d11_upload_debug);
#define GST_CAT_DEFAULT gst_d3d11_upload_debug

static GstStaticCaps sink_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY,
        GST_D3D11_ALL_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY
        "," GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS));

static GstStaticCaps src_template_caps =
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS) ";"
    GST_VIDEO_CAPS_MAKE (GST_D3D11_ALL_FORMATS) "; "
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES
    (GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY ","
        GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
        GST_D3D11_ALL_FORMATS));

struct _GstD3D11Upload
{
  GstD3D11BaseFilter parent;
};

#define gst_d3d11_upload_parent_class parent_class
G_DEFINE_TYPE (GstD3D11Upload, gst_d3d11_upload, GST_TYPE_D3D11_BASE_FILTER);

static GstCaps *gst_d3d11_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_d3d11_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query);
static gboolean gst_d3d11_upload_decide_allocation (GstBaseTransform * trans,
    GstQuery * query);
static GstFlowReturn gst_d3d11_upload_transform (GstBaseTransform * trans,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_d3d11_upload_class_init (GstD3D11UploadClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstCaps *caps;

  caps = gst_d3d11_get_updated_template_caps (&sink_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  caps = gst_d3d11_get_updated_template_caps (&src_template_caps);
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, caps));
  gst_caps_unref (caps);

  gst_element_class_set_static_metadata (element_class,
      "Direct3D11 Uploader", "Filter/Video",
      "Uploads data into Direct3D11 texture memory",
      "Seungha Yang <seungha.yang@navercorp.com>");

  trans_class->passthrough_on_same_caps = TRUE;

  trans_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_transform_caps);
  trans_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_propose_allocation);
  trans_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d11_upload_decide_allocation);
  trans_class->transform = GST_DEBUG_FUNCPTR (gst_d3d11_upload_transform);

  GST_DEBUG_CATEGORY_INIT (gst_d3d11_upload_debug,
      "d3d11upload", 0, "d3d11upload Element");
}

static void
gst_d3d11_upload_init (GstD3D11Upload * upload)
{
}

static GstCaps *
_set_caps_features (const GstCaps * caps, const gchar * feature_name)
{
  guint i, j, m, n;
  GstCaps *tmp;
  GstCapsFeatures *overlay_feature =
      gst_caps_features_from_string
      (GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);

  tmp = gst_caps_new_empty ();

  n = gst_caps_get_size (caps);
  for (i = 0; i < n; i++) {
    GstCapsFeatures *features, *orig_features;
    GstStructure *s = gst_caps_get_structure (caps, i);

    orig_features = gst_caps_get_features (caps, i);
    features = gst_caps_features_new (feature_name, NULL);

    if (gst_caps_features_is_any (orig_features)) {
      gst_caps_append_structure_full (tmp, gst_structure_copy (s),
          gst_caps_features_copy (features));

      if (!gst_caps_features_contains (features,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION))
        gst_caps_features_add (features,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
    } else {
      m = gst_caps_features_get_size (orig_features);
      for (j = 0; j < m; j++) {
        const gchar *feature = gst_caps_features_get_nth (orig_features, j);

        /* if we already have the features */
        if (gst_caps_features_contains (features, feature))
          continue;

        if (g_strcmp0 (feature, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY) == 0)
          continue;

        if (gst_caps_features_contains (overlay_feature, feature)) {
          gst_caps_features_add (features, feature);
        }
      }
    }

    gst_caps_append_structure_full (tmp, gst_structure_copy (s), features);
  }

  gst_caps_features_free (overlay_feature);

  return tmp;
}

static GstCaps *
gst_d3d11_upload_transform_caps (GstBaseTransform * trans,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  GST_DEBUG_OBJECT (trans,
      "Transforming caps %" GST_PTR_FORMAT " in direction %s", caps,
      (direction == GST_PAD_SINK) ? "sink" : "src");

  if (direction == GST_PAD_SINK) {
    tmp = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY);
    tmp = gst_caps_merge (gst_caps_ref (caps), tmp);
  } else {
    GstCaps *newcaps;
    tmp = gst_caps_ref (caps);
    newcaps = _set_caps_features (caps, GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY);
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
gst_d3d11_upload_propose_allocation (GstBaseTransform * trans,
    GstQuery * decide_query, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;
  gboolean is_d3d11 = FALSE;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->propose_allocation (trans,
          decide_query, query))
    return FALSE;

  /* passthrough, we're done */
  if (decide_query == NULL)
    return TRUE;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstCapsFeatures *features;
    GstStructure *config;
    features = gst_caps_get_features (caps, 0);

    if (features && gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
      GST_DEBUG_OBJECT (filter, "upstream support d3d11 memory");
      pool = gst_d3d11_buffer_pool_new (filter->device);
      is_d3d11 = TRUE;
    } else {
      pool = gst_video_buffer_pool_new ();
    }

    config = gst_buffer_pool_get_config (pool);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_META);
    if (!is_d3d11) {
      gst_buffer_pool_config_add_option (config,
          GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    }

    size = GST_VIDEO_INFO_SIZE (&info);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);

    if (!gst_buffer_pool_set_config (pool, config))
      goto config_failed;

    /* d3d11 buffer pool will update buffer size based on allocated texture,
     * get size from config again */
    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_get_params (config,
        nullptr, &size, nullptr, nullptr);
    gst_structure_free (config);

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
  }

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_query_add_allocation_meta (query,
      GST_VIDEO_OVERLAY_COMPOSITION_META_API_TYPE, NULL);

  return TRUE;

  /* ERRORS */
config_failed:
  {
    GST_ERROR_OBJECT (filter, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}

static gboolean
gst_d3d11_upload_decide_allocation (GstBaseTransform * trans, GstQuery * query)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);
  GstCaps *outcaps = NULL;
  GstBufferPool *pool = NULL;
  guint size, min, max;
  GstStructure *config;
  gboolean update_pool = FALSE;
  GstVideoInfo vinfo;
  GstD3D11Format d3d11_format;
  GstD3D11AllocationParams *d3d11_params;
  guint bind_flags = 0;
  DXGI_FORMAT dxgi_format = DXGI_FORMAT_UNKNOWN;
  UINT supported = 0;
  HRESULT hr;
  ID3D11Device *device_handle;

  gst_query_parse_allocation (query, &outcaps, NULL);

  if (!outcaps)
    return FALSE;

  gst_video_info_from_caps (&vinfo, outcaps);

  if (!gst_d3d11_device_get_format (filter->device,
          GST_VIDEO_INFO_FORMAT (&vinfo), &d3d11_format)) {
    GST_ERROR_OBJECT (filter, "Unknown format caps %" GST_PTR_FORMAT, outcaps);
    return FALSE;
  }

  if (d3d11_format.dxgi_format == DXGI_FORMAT_UNKNOWN) {
    dxgi_format = d3d11_format.resource_format[0];
  } else {
    dxgi_format = d3d11_format.dxgi_format;
  }

  device_handle = gst_d3d11_device_get_device_handle (filter->device);
  hr = device_handle->CheckFormatSupport (dxgi_format, &supported);
  if (gst_d3d11_result (hr, filter->device)) {
    if ((supported & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) ==
        D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) {
      bind_flags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if ((supported & D3D11_FORMAT_SUPPORT_RENDER_TARGET) ==
        D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }
  }

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    if (pool) {
      if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
        gst_clear_object (&pool);
      } else {
        GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
        if (dpool->device != filter->device)
          gst_clear_object (&pool);
      }
    }

    update_pool = TRUE;
  } else {
    size = GST_VIDEO_INFO_SIZE (&vinfo);
    min = max = 0;
  }

  if (!pool) {
    GST_DEBUG_OBJECT (trans, "create our pool");

    pool = gst_d3d11_buffer_pool_new (filter->device);
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);

  d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
  if (!d3d11_params) {
    d3d11_params = gst_d3d11_allocation_params_new (filter->device, &vinfo,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  } else {
    gst_d3d11_allocation_params_set_bind_flags (d3d11_params, bind_flags);
  }

  gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
  gst_d3d11_allocation_params_free (d3d11_params);

  gst_buffer_pool_set_config (pool, config);

  /* d3d11 buffer pool will update buffer size based on allocated texture,
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
gst_d3d11_upload_transform (GstBaseTransform * trans, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstD3D11BaseFilter *filter = GST_D3D11_BASE_FILTER (trans);

  if (!gst_d3d11_buffer_copy_into (outbuf, inbuf, &filter->in_info)) {
    GST_ERROR_OBJECT (filter, "Failed to copy buffer");
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}
