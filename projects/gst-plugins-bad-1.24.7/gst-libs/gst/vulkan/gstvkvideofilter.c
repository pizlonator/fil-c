/*
 * GStreamer
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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
 * SECTION:vulkanvideofilter
 * @title: GstVulkanVideoFilter
 * @short_description: Vulkan filter base class
 * @see_also: #GstVulkanInstance, #GstVulkanDevice
 *
 * #GstVulkanVideoFilter is a helper base class for retrieving and holding the
 * #GstVulkanInstance, #GstVulkanDevice and #GstVulkanQueue used by an element.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/vulkan/gstvkvideofilter.h>

GST_DEBUG_CATEGORY (gst_debug_vulkan_video_filter);
#define GST_CAT_DEFAULT gst_debug_vulkan_video_filter

static void gst_vulkan_video_filter_finalize (GObject * object);

static gboolean gst_vulkan_video_filter_query (GstBaseTransform * bt,
    GstPadDirection direction, GstQuery * query);
static void gst_vulkan_video_filter_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_vulkan_video_filter_start (GstBaseTransform * bt);
static gboolean gst_vulkan_video_filter_stop (GstBaseTransform * bt);

static gboolean gst_vulkan_video_filter_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);
static GstCaps *gst_vulkan_video_filter_transform_caps (GstBaseTransform *
    bt, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean
gst_vulkan_video_filter_propose_allocation (GstBaseTransform * bt,
    GstQuery * decide_query, GstQuery * query);
static gboolean
gst_vulkan_video_filter_decide_allocation (GstBaseTransform * bt,
    GstQuery * query);

enum
{
  PROP_0,
};

enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* static guint gst_vulkan_video_filter_signals[LAST_SIGNAL] = { 0 }; */

#define gst_vulkan_video_filter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanVideoFilter,
    gst_vulkan_video_filter, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_video_filter,
        "vulkanvideofilter", 0, "Vulkan Video Filter"));

static void
gst_vulkan_video_filter_class_init (GstVulkanVideoFilterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gobject_class->finalize = gst_vulkan_video_filter_finalize;

  gstelement_class->set_context = gst_vulkan_video_filter_set_context;
  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_video_filter_start);
  gstbasetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_vulkan_video_filter_stop);
  gstbasetransform_class->query =
      GST_DEBUG_FUNCPTR (gst_vulkan_video_filter_query);
  gstbasetransform_class->set_caps = gst_vulkan_video_filter_set_caps;
  gstbasetransform_class->transform_caps =
      gst_vulkan_video_filter_transform_caps;
  gstbasetransform_class->propose_allocation =
      gst_vulkan_video_filter_propose_allocation;
  gstbasetransform_class->decide_allocation =
      gst_vulkan_video_filter_decide_allocation;
}

static void
gst_vulkan_video_filter_init (GstVulkanVideoFilter * render)
{
}

static void
gst_vulkan_video_filter_finalize (GObject * object)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (object);

  gst_caps_replace (&render->in_caps, NULL);
  gst_caps_replace (&render->out_caps, NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_vulkan_video_filter_query (GstBaseTransform * bt,
    GstPadDirection direction, GstQuery * query)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (bt);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      if (gst_vulkan_handle_context_query (GST_ELEMENT (render), query,
              NULL, render->instance, render->device))
        return TRUE;

      if (gst_vulkan_queue_handle_context_query (GST_ELEMENT (render),
              query, render->queue))
        return TRUE;

      break;
    }
    default:
      break;
  }

  return GST_BASE_TRANSFORM_CLASS (parent_class)->query (bt, direction, query);
}

static void
gst_vulkan_video_filter_set_context (GstElement * element, GstContext * context)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (element);

  gst_vulkan_handle_set_context (element, context, NULL, &render->instance);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

struct choose_data
{
  GstVulkanVideoFilter *filter;
  GstVulkanQueue *queue;
};

static GstCaps *
gst_vulkan_video_filter_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *result, *tmp;

  tmp = gst_caps_copy (caps);

  if (filter) {
    result = gst_caps_intersect_full (filter, tmp, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (tmp);
  } else {
    result = tmp;
  }

  return result;
}

static gboolean
gst_vulkan_video_filter_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (bt);

  if (!gst_video_info_from_caps (&render->in_info, in_caps))
    return FALSE;
  if (!gst_video_info_from_caps (&render->out_info, out_caps))
    return FALSE;

  gst_caps_replace (&render->in_caps, in_caps);
  gst_caps_replace (&render->out_caps, out_caps);
  GST_DEBUG_OBJECT (bt, "set caps: %" GST_PTR_FORMAT, in_caps);

  return TRUE;
}

static gboolean
gst_vulkan_video_filter_propose_allocation (GstBaseTransform * bt,
    GstQuery * decide_query, GstQuery * query)
{
  /* FIXME: */
  return FALSE;
}

static gboolean
gst_vulkan_video_filter_decide_allocation (GstBaseTransform * bt,
    GstQuery * query)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (bt);
  GstBufferPool *pool = NULL;
  GstStructure *config;
  GstCaps *caps;
  guint min, max, size;
  gboolean update_pool;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps)
    return FALSE;

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

    update_pool = TRUE;
  } else {
    GstVideoInfo vinfo;

    gst_video_info_init (&vinfo);
    gst_video_info_from_caps (&vinfo, caps);
    size = vinfo.size;
    min = max = 0;
    update_pool = FALSE;
  }

  if (!pool || !GST_IS_VULKAN_IMAGE_BUFFER_POOL (pool)) {
    if (pool)
      gst_object_unref (pool);
    pool = gst_vulkan_image_buffer_pool_new (render->device);
  }

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, size, min, max);

  if (!gst_buffer_pool_set_config (pool, config)) {
    gst_object_unref (pool);
    return FALSE;
  }

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_vulkan_video_filter_start (GstBaseTransform * bt)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (bt);

  if (!gst_vulkan_ensure_element_data (GST_ELEMENT (bt), NULL,
          &render->instance)) {
    GST_ELEMENT_ERROR (render, RESOURCE, NOT_FOUND,
        ("Failed to retrieve vulkan instance"), (NULL));
    return FALSE;
  }
  if (!gst_vulkan_device_run_context_query (GST_ELEMENT (render),
          &render->device)) {
    GError *error = NULL;
    GST_DEBUG_OBJECT (render, "No device retrieved from peer elements");
    if (!(render->device =
            gst_vulkan_instance_create_device (render->instance, &error))) {
      GST_ELEMENT_ERROR (render, RESOURCE, NOT_FOUND,
          ("Failed to create vulkan device"), ("%s", error->message));
      g_clear_error (&error);
      return FALSE;
    }
  }

  if (!gst_vulkan_queue_run_context_query (GST_ELEMENT (render),
          &render->queue)) {
    GST_DEBUG_OBJECT (render, "No queue retrieved from peer elements");
    render->queue =
        gst_vulkan_device_select_queue (render->device, VK_QUEUE_GRAPHICS_BIT);
  }
  if (!render->queue)
    return FALSE;

  return TRUE;
}

static gboolean
gst_vulkan_video_filter_stop (GstBaseTransform * bt)
{
  GstVulkanVideoFilter *render = GST_VULKAN_VIDEO_FILTER (bt);

  gst_clear_object (&render->device);
  gst_clear_object (&render->queue);
  gst_clear_object (&render->instance);

  return TRUE;
}
