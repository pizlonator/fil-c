/*
 * GStreamer
 * Copyright (C) 2022 Matthew Waters <matthew@centricular.com>
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
 * SECTION:element-vulkanoverlaycompositor
 * @title: vulkanoverlaycompositor
 *
 * `vulkanoverlaycompositor` overlays upstream `GstVideoOverlayCompositonMeta`
 * onto the video stream.
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvulkanelements.h"
#include "vkoverlaycompositor.h"

#include "shaders/identity.vert.h"
#include "shaders/swizzle.frag.h"

GST_DEBUG_CATEGORY (gst_debug_vulkan_overlay_compositor);
#define GST_CAT_DEFAULT gst_debug_vulkan_overlay_compositor

struct vk_overlay
{
  GstBuffer *buffer;
  GstVideoOverlayComposition *composition;
  GstVideoOverlayRectangle *rectangle;
  GstVulkanFullScreenQuad *quad;
};

static void
vk_overlay_clear (struct vk_overlay *overlay)
{
  gst_clear_buffer (&overlay->buffer);
  overlay->rectangle = NULL;
  if (overlay->composition)
    gst_video_overlay_composition_unref (overlay->composition);
  overlay->composition = NULL;

  gst_clear_object (&overlay->quad);
}

static void
vk_overlay_init (struct vk_overlay *overlay, GstVulkanQueue * queue,
    GstBuffer * buffer, GstVideoOverlayComposition * comp,
    GstVideoOverlayRectangle * rectangle, GstVulkanHandle * vert,
    GstVulkanHandle * frag)
{
  GstVideoOverlayFormatFlags flags;

  memset (overlay, 0, sizeof (*overlay));

  flags = gst_video_overlay_rectangle_get_flags (rectangle);

  overlay->buffer = gst_buffer_ref (buffer);
  overlay->composition = gst_video_overlay_composition_ref (comp);
  overlay->rectangle = rectangle;
  overlay->quad = gst_vulkan_full_screen_quad_new (queue);
  gst_vulkan_full_screen_quad_enable_clear (overlay->quad, FALSE);
  gst_vulkan_full_screen_quad_set_shaders (overlay->quad, vert, frag);
  gst_vulkan_full_screen_quad_enable_blend (overlay->quad, TRUE);
  gst_vulkan_full_screen_quad_set_blend_operation (overlay->quad,
      VK_BLEND_OP_ADD, VK_BLEND_OP_ADD);
  if (flags & GST_VIDEO_OVERLAY_FORMAT_FLAG_PREMULTIPLIED_ALPHA) {
    gst_vulkan_full_screen_quad_set_blend_factors (overlay->quad,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
  } else {
    gst_vulkan_full_screen_quad_set_blend_factors (overlay->quad,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
  }
}

struct Vertex
{
  float x, y, z;
  float s, t;
};

struct swizzle_uniforms
{
  int in_reorder_index[4];
  int out_reorder_index[4];
};

static gboolean
vk_overlay_upload (struct vk_overlay *overlay, GstVideoInfo * out_info,
    GError ** error)
{
  GstBuffer *overlay_buffer, *vk_gst_buffer = NULL;
  GstVideoMeta *vmeta;
  GstVideoInfo vinfo;
  GstVideoFrame vframe;
  GstVulkanBufferMemory *buf_mem;
  GstVulkanImageMemory *img_mem;
  GstMemory *vkbuffer = NULL, *vkimage = NULL, *vkvertices = NULL;
  GstMemory *vkuniforms = NULL;
  VkFormat vk_format;
  GstMapInfo map_info;
  GstVulkanFence *fence = NULL;
  GstVulkanCommandBuffer *cmd_buf = NULL;
  VkBufferMemoryBarrier buffer_memory_barrier;
  VkImageMemoryBarrier image_memory_barrier;
  VkBufferImageCopy region;
  VkResult err;
  struct Vertex vertices[4];
  struct swizzle_uniforms uniforms;

  overlay_buffer =
      gst_video_overlay_rectangle_get_pixels_unscaled_argb
      (overlay->rectangle, GST_VIDEO_OVERLAY_FORMAT_FLAG_NONE);

  vmeta = gst_buffer_get_video_meta (overlay_buffer);
  gst_video_info_set_format (&vinfo, vmeta->format, vmeta->width,
      vmeta->height);
  vinfo.stride[0] = vmeta->stride[0];

  if (!gst_vulkan_full_screen_quad_set_info (overlay->quad, out_info, out_info))
    goto error;

  if (!gst_video_frame_map (&vframe, &vinfo, overlay_buffer, GST_MAP_READ)) {
    g_set_error_literal (error, GST_TYPE_RESOURCE_ERROR,
        GST_RESOURCE_ERROR_READ, "Cannot map overlay buffer for reading");
    return FALSE;
  }

  vkbuffer =
      gst_vulkan_buffer_memory_alloc (overlay->quad->queue->device,
      GST_VIDEO_INFO_COMP_STRIDE (&vinfo, 0) *
      GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 0),
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  buf_mem = (GstVulkanBufferMemory *) vkbuffer;

  if (!gst_memory_map (vkbuffer, &map_info, GST_MAP_WRITE)) {
    g_set_error_literal (error, GST_TYPE_RESOURCE_ERROR,
        GST_RESOURCE_ERROR_WRITE,
        "Cannot map staging vulkan buffer for writing");
    gst_video_frame_unmap (&vframe);
    goto error;
  }

  memcpy (map_info.data, vframe.data[0], vframe.info.size);

  gst_memory_unmap (vkbuffer, &map_info);
  gst_video_frame_unmap (&vframe);

  vk_format = gst_vulkan_format_from_video_info (&vinfo, 0);
  vkimage =
      gst_vulkan_image_memory_alloc (overlay->quad->queue->device, vk_format,
      GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 0),
      GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 0),
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  img_mem = (GstVulkanImageMemory *) vkimage;

  /* *INDENT-OFF* */
  region = (VkBufferImageCopy) {
      .bufferOffset = 0,
      .bufferRowLength = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 0),
      .bufferImageHeight = GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 0),
      .imageSubresource = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
      .imageOffset = { .x = 0, .y = 0, .z = 0, },
      .imageExtent = {
          .width = GST_VIDEO_INFO_COMP_WIDTH (&vinfo, 0),
          .height = GST_VIDEO_INFO_COMP_HEIGHT (&vinfo, 0),
          .depth = 1,
      }
  };

  buffer_memory_barrier = (VkBufferMemoryBarrier) {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = buf_mem->barrier.parent.access_flags,
      .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
      /* FIXME: implement exclusive transfers */
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = buf_mem->buffer,
      .offset = region.bufferOffset,
      .size = region.bufferRowLength * region.bufferImageHeight,
  };

  image_memory_barrier = (VkImageMemoryBarrier) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = img_mem->barrier.parent.access_flags,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = img_mem->barrier.image_layout,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      /* FIXME: implement exclusive transfers */
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = img_mem->image,
      .subresourceRange = img_mem->barrier.subresource_range,
  };
  /* *INDENT-ON* */

  if (!(cmd_buf =
          gst_vulkan_command_pool_create (overlay->quad->cmd_pool, error)))
    goto error;

  {
    /* *INDENT-OFF* */
    VkCommandBufferBeginInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL
    };
    /* *INDENT-ON* */

    gst_vulkan_command_buffer_lock (cmd_buf);
    err = vkBeginCommandBuffer (cmd_buf->cmd, &cmd_buf_info);
    if (gst_vulkan_error_to_g_error (err, error, "vkBeginCommandBuffer") < 0) {
      gst_vulkan_command_buffer_unlock (cmd_buf);
      goto error;
    }
  }

  vkCmdPipelineBarrier (cmd_buf->cmd,
      buf_mem->barrier.parent.pipeline_stages | img_mem->barrier.
      parent.pipeline_stages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 1,
      &buffer_memory_barrier, 1, &image_memory_barrier);

  buf_mem->barrier.parent.pipeline_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
  buf_mem->barrier.parent.access_flags = buffer_memory_barrier.dstAccessMask;

  img_mem->barrier.parent.pipeline_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
  img_mem->barrier.parent.access_flags = image_memory_barrier.dstAccessMask;
  img_mem->barrier.image_layout = image_memory_barrier.newLayout;

  vkCmdCopyBufferToImage (cmd_buf->cmd, buf_mem->buffer, img_mem->image,
      img_mem->barrier.image_layout, 1, &region);

  err = vkEndCommandBuffer (cmd_buf->cmd);
  gst_vulkan_command_buffer_unlock (cmd_buf);
  if (gst_vulkan_error_to_g_error (err, error, "vkEndCommandBuffer") < 0) {
    goto error;
  }

  {
    VkSubmitInfo submit_info = { 0, };

    /* *INDENT-OFF* */
    submit_info = (VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = NULL,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = NULL,
        .pWaitDstStageMask = NULL,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd_buf->cmd,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = NULL,
    };
    /* *INDENT-ON* */

    fence =
        gst_vulkan_device_create_fence (overlay->quad->queue->device, error);
    if (!fence)
      goto error;

    gst_vulkan_queue_submit_lock (overlay->quad->queue);
    err =
        vkQueueSubmit (overlay->quad->queue->queue, 1, &submit_info,
        GST_VULKAN_FENCE_FENCE (fence));
    gst_vulkan_queue_submit_unlock (overlay->quad->queue);
    if (gst_vulkan_error_to_g_error (err, error, "vkQueueSubmit") < 0)
      goto error;

    gst_vulkan_trash_list_add (overlay->quad->trash_list,
        gst_vulkan_trash_list_acquire (overlay->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            GST_MINI_OBJECT_CAST (cmd_buf)));
    cmd_buf = NULL;
    gst_vulkan_trash_list_add (overlay->quad->trash_list,
        gst_vulkan_trash_list_acquire (overlay->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            GST_MINI_OBJECT_CAST (vkbuffer)));
    vkbuffer = NULL;
    gst_vulkan_trash_list_add (overlay->quad->trash_list,
        gst_vulkan_trash_list_acquire (overlay->quad->trash_list, fence,
            gst_vulkan_trash_mini_object_unref,
            GST_MINI_OBJECT_CAST (gst_memory_ref (vkimage))));
    gst_vulkan_trash_list_gc (overlay->quad->trash_list);
    gst_vulkan_fence_unref (fence);
    fence = NULL;
  }

  vk_gst_buffer = gst_buffer_new ();
  gst_buffer_append_memory (vk_gst_buffer, vkimage);
  vkimage = NULL;

  if (!gst_vulkan_full_screen_quad_set_input_buffer (overlay->quad,
          vk_gst_buffer, error))
    goto error;

  gst_clear_buffer (&vk_gst_buffer);

  {
    int xpos, ypos;
    guint width, height, out_width, out_height;
    float xl, xr, yt, yb;

    if (!gst_video_overlay_rectangle_get_render_rectangle (overlay->rectangle,
            &xpos, &ypos, &width, &height))
      goto error;

    out_width = GST_VIDEO_INFO_WIDTH (out_info);
    out_height = GST_VIDEO_INFO_HEIGHT (out_info);

    xl = 2.0 * (float) xpos / (float) out_width - 1.0;
    yt = 2.0 * (float) ypos / (float) out_height - 1.0;
    xr = xl + 2.0 * (float) width / (float) out_width;
    yb = yt + 2.0 * (float) height / (float) out_height;

    GST_LOG_OBJECT (overlay->quad, "rectangle %ux%u+%d,%d placed in %ux%u at "
        "%fx%f+%f,%f", width, height, xpos, ypos, out_width, out_height,
        xr - xl, yb - yt, xl, yt);

    /* top-left */
    vertices[0].x = xl;
    vertices[0].y = yt;
    vertices[0].z = 0.0;
    vertices[0].s = 0.0;
    vertices[0].t = 0.0;
    /* top-right */
    vertices[1].x = xr;
    vertices[1].y = yt;
    vertices[1].z = 0.0;
    vertices[1].s = 1.0;
    vertices[1].t = 0.0;
    /* bottom-right */
    vertices[2].x = xr;
    vertices[2].y = yb;
    vertices[2].z = 0.0;
    vertices[2].s = 1.0;
    vertices[2].t = 1.0;
    /* bottom-left */
    vertices[3].x = xl;
    vertices[3].y = yb;
    vertices[3].z = 0.0;
    vertices[3].s = 0.0;
    vertices[3].t = 1.0;
  }

  vkvertices =
      gst_vulkan_buffer_memory_alloc (overlay->quad->queue->device,
      sizeof (vertices),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (!gst_memory_map (vkvertices, &map_info, GST_MAP_WRITE))
    goto error;
  memcpy (map_info.data, vertices, sizeof (vertices));
  gst_memory_unmap (vkvertices, &map_info);

  if (!gst_vulkan_full_screen_quad_set_vertex_buffer (overlay->quad,
          vkvertices, error))
    goto error;

  gst_clear_mini_object ((GstMiniObject **) & vkvertices);

  uniforms.in_reorder_index[0] = 0;
  uniforms.in_reorder_index[1] = 1;
  uniforms.in_reorder_index[2] = 2;
  uniforms.in_reorder_index[3] = 3;
  uniforms.out_reorder_index[0] = 0;
  uniforms.out_reorder_index[1] = 1;
  uniforms.out_reorder_index[2] = 2;
  uniforms.out_reorder_index[3] = 3;

  vkuniforms =
      gst_vulkan_buffer_memory_alloc (overlay->quad->queue->device,
      sizeof (uniforms),
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  if (!gst_memory_map (vkuniforms, &map_info, GST_MAP_WRITE))
    goto error;
  memcpy (map_info.data, &uniforms, sizeof (uniforms));
  gst_memory_unmap (vkuniforms, &map_info);

  if (!gst_vulkan_full_screen_quad_set_uniform_buffer (overlay->quad,
          vkuniforms, error))
    goto error;
  gst_clear_mini_object ((GstMiniObject **) & vkuniforms);

  return TRUE;

error:
  gst_clear_mini_object ((GstMiniObject **) & vkimage);
  gst_clear_mini_object ((GstMiniObject **) & vkbuffer);
  gst_clear_mini_object ((GstMiniObject **) & vkvertices);
  gst_clear_mini_object ((GstMiniObject **) & vkuniforms);
  if (cmd_buf)
    gst_vulkan_command_buffer_unref (cmd_buf);
  if (fence)
    gst_vulkan_fence_unref (fence);
  gst_clear_buffer (&vk_gst_buffer);
  gst_clear_buffer (&overlay_buffer);

  return FALSE;
}

static gboolean gst_vulkan_overlay_compositor_start (GstBaseTransform * bt);
static gboolean gst_vulkan_overlay_compositor_stop (GstBaseTransform * bt);
static GstCaps *gst_vulkan_overlay_compositor_transform_caps (GstBaseTransform *
    bt, GstPadDirection direction, GstCaps * caps, GstCaps * filter);
static gboolean gst_vulkan_overlay_compositor_set_caps (GstBaseTransform *
    bt, GstCaps * incaps, GstCaps * outcaps);
static GstFlowReturn
gst_vulkan_overlay_compositor_transform_ip (GstBaseTransform * bt,
    GstBuffer * inbuf);

#define IMAGE_FORMATS " { BGRA, RGBA }"

static GstStaticPadTemplate gst_vulkan_sink_template =
    GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            IMAGE_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS) "; " GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
            IMAGE_FORMATS)));

static GstStaticPadTemplate gst_vulkan_src_template =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE ","
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION,
            IMAGE_FORMATS) "; "
        GST_VIDEO_CAPS_MAKE_WITH_FEATURES (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS) "; " GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("ANY",
            IMAGE_FORMATS)));

enum
{
  PROP_0,
};

typedef struct _GstVulkanOverlayCompositor GstVulkanOverlayCompositor;

struct _GstVulkanOverlayCompositor
{
  GstVulkanVideoFilter parent;

  GstVulkanHandle *vert;
  GstVulkanHandle *frag;
  GArray *overlays;

  gboolean render_overlays;
};

#define gst_vulkan_overlay_compositor_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanOverlayCompositor,
    gst_vulkan_overlay_compositor, GST_TYPE_VULKAN_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_overlay_compositor,
        "vulkanoverlaycompositor", 0, "Vulkan Overlay Compositor"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanoverlaycompositor,
    "vulkanoverlaycompositor", GST_RANK_NONE,
    GST_TYPE_VULKAN_OVERLAY_COMPOSITOR, vulkan_element_init (plugin));

static void
gst_vulkan_overlay_compositor_class_init (GstVulkanOverlayCompositorClass *
    klass)
{
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gst_element_class_set_metadata (gstelement_class, "Vulkan Overlay Compositor",
      "Filter/Video", "Vulkan Overlay Composition element",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_src_template);

  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_overlay_compositor_start);
  gstbasetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_vulkan_overlay_compositor_stop);
  gstbasetransform_class->transform_caps =
      GST_DEBUG_FUNCPTR (gst_vulkan_overlay_compositor_transform_caps);
  gstbasetransform_class->set_caps =
      GST_DEBUG_FUNCPTR (gst_vulkan_overlay_compositor_set_caps);
  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_vulkan_overlay_compositor_transform_ip);
}

static void
gst_vulkan_overlay_compositor_init (GstVulkanOverlayCompositor * vk_overlay)
{
}

static gboolean
gst_vulkan_overlay_compositor_start (GstBaseTransform * bt)
{
  GstVulkanOverlayCompositor *vk_overlay = GST_VULKAN_OVERLAY_COMPOSITOR (bt);
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (vk_overlay);
  GError *error = NULL;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->start (bt))
    return FALSE;

  if (!(vk_overlay->vert =
          gst_vulkan_create_shader (vfilter->device, identity_vert,
              identity_vert_size, &error)))
    goto error;
  if (!(vk_overlay->frag =
          gst_vulkan_create_shader (vfilter->device, swizzle_frag,
              swizzle_frag_size, &error))) {
    gst_clear_vulkan_handle (&vk_overlay->vert);
    goto error;
  }

  vk_overlay->overlays = g_array_new (FALSE, TRUE, sizeof (struct vk_overlay));
  g_array_set_clear_func (vk_overlay->overlays,
      (GDestroyNotify) vk_overlay_clear);

  return TRUE;

error:
  GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND, ("%s", error->message), (NULL));
  return FALSE;
}

static gboolean
gst_vulkan_overlay_compositor_stop (GstBaseTransform * bt)
{
  GstVulkanOverlayCompositor *vk_overlay = GST_VULKAN_OVERLAY_COMPOSITOR (bt);

  if (vk_overlay->overlays) {
    g_array_set_size (vk_overlay->overlays, 0);
    g_array_unref (vk_overlay->overlays);
  }
  vk_overlay->overlays = NULL;

  gst_clear_vulkan_handle (&vk_overlay->vert);
  gst_clear_vulkan_handle (&vk_overlay->frag);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static struct vk_overlay *
find_by_rectangle (GstVulkanOverlayCompositor * vk_overlay,
    GstVideoOverlayRectangle * rectangle)
{
  int i;

  for (i = 0; i < vk_overlay->overlays->len; i++) {
    struct vk_overlay *over =
        &g_array_index (vk_overlay->overlays, struct vk_overlay, i);

    if (over->rectangle == rectangle)
      return over;
  }

  return NULL;
}

static gboolean
overlay_in_rectangles (struct vk_overlay *over,
    GstVideoOverlayComposition * composition)
{
  int i, n;

  n = gst_video_overlay_composition_n_rectangles (composition);
  for (i = 0; i < n; i++) {
    GstVideoOverlayRectangle *rect;

    rect = gst_video_overlay_composition_get_rectangle (composition, i);

    if (over->rectangle == rect)
      return TRUE;
  }

  return FALSE;
}

static GstCaps *
gst_vulkan_overlay_compositor_transform_caps (GstBaseTransform * bt,
    GstPadDirection direction, GstCaps * caps, GstCaps * filter)
{
  GstCaps *ret;

  /* add/remove the composition overlay meta as necessary */
  if (direction == GST_PAD_SRC) {
    GstCaps *composition_caps;
    int i;

    composition_caps = gst_caps_copy (caps);

    for (i = 0; i < gst_caps_get_size (composition_caps); i++) {
      GstCapsFeatures *f = gst_caps_get_features (composition_caps, i);
      if (!gst_caps_features_is_any (f))
        gst_caps_features_add (f,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
    }

    ret = gst_caps_merge (composition_caps, gst_caps_copy (caps));
  } else {
    guint i, n;
    GstCaps *removed;

    ret = gst_caps_copy (caps);
    removed = gst_caps_copy (caps);
    n = gst_caps_get_size (removed);
    for (i = 0; i < n; i++) {
      GstCapsFeatures *feat = gst_caps_get_features (removed, i);

      if (feat && gst_caps_features_contains (feat,
              GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
        feat = gst_caps_features_copy (feat);
        /* prefer the passthrough case */
        gst_caps_features_remove (feat,
            GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION);
        gst_caps_set_features (removed, i, feat);
      }
    }

    ret = gst_caps_merge (ret, removed);
  }

  if (filter) {
    GstCaps *tmp = gst_caps_intersect (ret, filter);
    gst_clear_caps (&ret);
    ret = tmp;
  }

  return ret;
}

static gboolean
gst_vulkan_overlay_compositor_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstVulkanOverlayCompositor *vk_overlay = GST_VULKAN_OVERLAY_COMPOSITOR (bt);
  GstCapsFeatures *in_features, *out_features;

  GST_DEBUG_OBJECT (bt, " incaps %" GST_PTR_FORMAT, incaps);
  GST_DEBUG_OBJECT (bt, "outcaps %" GST_PTR_FORMAT, outcaps);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, incaps, outcaps))
    return FALSE;

  in_features = gst_caps_get_features (incaps, 0);
  out_features = gst_caps_get_features (outcaps, 0);

  if (gst_caps_features_contains (in_features,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION) &&
      !gst_caps_features_contains (out_features,
          GST_CAPS_FEATURE_META_GST_VIDEO_OVERLAY_COMPOSITION)) {
    GST_INFO_OBJECT (bt, "caps say to render GstVideoOverlayCompositionMeta");
    vk_overlay->render_overlays = TRUE;
  } else {
    GST_INFO_OBJECT (bt,
        "caps say to not render GstVideoOverlayCompositionMeta");
    vk_overlay->render_overlays = FALSE;
  }

  return TRUE;
}

static GstFlowReturn
gst_vulkan_overlay_compositor_transform_ip (GstBaseTransform * bt,
    GstBuffer * buffer)
{
  GstVulkanOverlayCompositor *vk_overlay = GST_VULKAN_OVERLAY_COMPOSITOR (bt);
  GstVideoOverlayCompositionMeta *ometa;
  GstVideoOverlayComposition *comp = NULL;
  GError *error = NULL;
  int i, n;

  if (!vk_overlay->render_overlays) {
    GST_LOG_OBJECT (bt,
        "caps don't say to render GstVideoOverlayCompositionMeta, passthrough");
    return GST_FLOW_OK;
  }

  ometa = gst_buffer_get_video_overlay_composition_meta (buffer);
  if (!ometa) {
    GST_LOG_OBJECT (bt,
        "no GstVideoOverlayCompositionMeta on buffer, passthrough");
    return GST_FLOW_OK;
  }

  comp = gst_video_overlay_composition_ref (ometa->overlay);
  gst_buffer_remove_meta (buffer, (GstMeta *) ometa);
  ometa = NULL;

  n = gst_video_overlay_composition_n_rectangles (comp);
  if (n == 0) {
    GST_LOG_OBJECT (bt,
        "GstVideoOverlayCompositionMeta has 0 rectangles, passthrough");
    return GST_FLOW_OK;
  }

  GST_LOG_OBJECT (bt,
      "rendering GstVideoOverlayCompositionMeta with %u rectangles", n);
  for (i = 0; i < n; i++) {
    GstVideoOverlayRectangle *rectangle;
    struct vk_overlay *over;

    rectangle = gst_video_overlay_composition_get_rectangle (comp, i);

    over = find_by_rectangle (vk_overlay, rectangle);
    if (!over) {
      struct vk_overlay new_overlay = { 0, };

      vk_overlay_init (&new_overlay, vk_overlay->parent.queue, buffer, comp,
          rectangle, vk_overlay->vert, vk_overlay->frag);

      if (!vk_overlay_upload (&new_overlay, &vk_overlay->parent.out_info,
              &error))
        goto error;

      g_array_append_val (vk_overlay->overlays, new_overlay);
    }
  }

  n = vk_overlay->overlays->len;
  for (i = 0; i < n;) {
    struct vk_overlay *over =
        &g_array_index (vk_overlay->overlays, struct vk_overlay, i);

    if (!overlay_in_rectangles (over, comp)) {
      g_array_remove_index (vk_overlay->overlays, i);
      continue;
    }

    if (!gst_vulkan_full_screen_quad_set_output_buffer (over->quad, buffer,
            &error))
      goto error;

    if (!gst_vulkan_full_screen_quad_draw (over->quad, &error))
      goto error;

    i++;
  }

  if (comp)
    gst_video_overlay_composition_unref (comp);

  return GST_FLOW_OK;

error:
  GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("%s", error->message), (NULL));
  g_clear_error (&error);
  if (comp)
    gst_video_overlay_composition_unref (comp);
  return GST_FLOW_ERROR;
}
