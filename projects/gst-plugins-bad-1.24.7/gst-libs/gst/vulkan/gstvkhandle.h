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

#ifndef __GST_VULKAN_HANDLE_H__
#define __GST_VULKAN_HANDLE_H__

#include <gst/gst.h>

#include <gst/vulkan/vulkan_fwd.h>
#include <gst/vulkan/gstvkapi.h>

G_BEGIN_DECLS

/**
 * gst_vulkan_handle_get_type:
 *
 * Since: 1.18
 */
GST_VULKAN_API
GType gst_vulkan_handle_get_type (void);
/**
 * GST_TYPE_VULKAN_HANDLE:
 *
 * Since: 1.18
 */
#define GST_TYPE_VULKAN_HANDLE (gst_vulkan_handle_get_type ())

/**
 * GstVulkanHandleTypedef:
 *
 * Since: 1.18
 */
VK_DEFINE_NON_DISPATCHABLE_HANDLE(GstVulkanHandleTypedef)

/**
 * GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT:
 *
 * The printf format specifier for raw Vulkan non dispatchable handles.
 *
 * When redefining VK_DEFINE_NON_DISPATCHABLE_HANDLE, also make sure
 * to redefine a suitable printf format specifier.
 *
 * Since: 1.18
 */
#if !defined(GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT)
# define GST_VULKAN_NON_DISPATCHABLE_HANDLE_FORMAT G_GUINT64_FORMAT
#endif

/**
 * GstVulkanHandleDestroyNotify:
 * @handle: the #GstVulkanHandle
 * @user_data: callback user data
 *
 * Function definition called when the #GstVulkanHandle is no longer in use.
 * All implementations of this callback must free the internal handle stored
 * inside @handle.
 *
 * Since: 1.18
 */
typedef void (*GstVulkanHandleDestroyNotify) (GstVulkanHandle * handle, gpointer user_data);

/**
 * GstVulkanHandleType:
 * @GST_VULKAN_HANDLE_TYPE_DESCRIPTOR_SET_LAYOUT: descripter set layout
 * @GST_VULKAN_HANDLE_TYPE_PIPELINE_LAYOUT: pipeline layout
 * @GST_VULKAN_HANDLE_TYPE_PIPELINE: pipeline
 * @GST_VULKAN_HANDLE_TYPE_RENDER_PASS: render pass
 * @GST_VULKAN_HANDLE_TYPE_SAMPLER: sampler
 * @GST_VULKAN_HANDLE_TYPE_FRAMEBUFFER: framebuffer
 * @GST_VULKAN_HANDLE_TYPE_SHADER: shader
 * @GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION: video session
 * @GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION_PARAMETERS: video session parameters
 * @GST_VULKAN_HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION: sampler with YCBCR conversion
 *
 * Since: 1.18
 */
typedef enum
{
  GST_VULKAN_HANDLE_TYPE_DESCRIPTOR_SET_LAYOUT          = 1,
  GST_VULKAN_HANDLE_TYPE_PIPELINE_LAYOUT                = 2,
  GST_VULKAN_HANDLE_TYPE_PIPELINE                       = 3,
  GST_VULKAN_HANDLE_TYPE_RENDER_PASS                    = 4,
  GST_VULKAN_HANDLE_TYPE_SAMPLER                        = 5,
  GST_VULKAN_HANDLE_TYPE_FRAMEBUFFER                    = 6,
  GST_VULKAN_HANDLE_TYPE_SHADER                         = 7,
  /**
   * GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION:
   *
   * video session
   *
   * Since: 1.24
   */
  GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION                  = 8,
  /**
   * GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION_PARAMETERS:
   *
   * video session parameters
   *
   * Since: 1.24
   */
  GST_VULKAN_HANDLE_TYPE_VIDEO_SESSION_PARAMETERS       = 9,
  /**
   * GST_VULKAN_HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION:
   *
   * sampler with YCBCR conversion
   *
   * Since: 1.24
   */
  GST_VULKAN_HANDLE_TYPE_SAMPLER_YCBCR_CONVERSION      = 10,
} GstVulkanHandleType;

/**
 * GstVulkanHandle:
 * @parent: the parent #GstMiniObject
 * @device: the #GstVulkanDevice for this handle
 * @type: the type of handle
 * @handle: the handle value
 *
 * Holds information about a vulkan non dispatchable handle that only has
 * a vulkan device as a parent and no specific host synchronisation
 * requirements.  Command buffers have extra requirements that are serviced by
 * more specific implementations (#GstVulkanCommandBuffer, #GstVulkanCommandPool).
 *
 * Since: 1.18
 */
struct _GstVulkanHandle
{
  GstMiniObject             parent;

  GstVulkanDevice          *device;

  GstVulkanHandleType       type;
  GstVulkanHandleTypedef    handle;

  /* <protected> */
  GstVulkanHandleDestroyNotify notify;
  gpointer                  user_data;

  /* <private> */
  gpointer _reserved        [GST_PADDING];
};

/**
 * gst_vulkan_handle_ref: (skip)
 * @handle: a #GstVulkanHandle.
 *
 * Increases the refcount of the given handle by one.
 *
 * Returns: (transfer full): @buf
 *
 * Since: 1.18
 */
static inline GstVulkanHandle* gst_vulkan_handle_ref(GstVulkanHandle* handle);
static inline GstVulkanHandle *
gst_vulkan_handle_ref (GstVulkanHandle * handle)
{
  return (GstVulkanHandle *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (handle));
}

/**
 * gst_vulkan_handle_unref: (skip)
 * @handle: (transfer full): a #GstVulkanHandle.
 *
 * Decreases the refcount of the buffer. If the refcount reaches 0, the buffer
 * will be freed.
 *
 * Since: 1.18
 */
static inline void gst_vulkan_handle_unref(GstVulkanHandle* handle);
static inline void
gst_vulkan_handle_unref (GstVulkanHandle * handle)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (handle));
}

/**
 * gst_clear_vulkan_handle: (skip)
 * @handle_ptr: a pointer to a #GstVulkanHandle reference
 *
 * Clears a reference to a #GstVulkanHandle.
 *
 * @handle_ptr must not be %NULL.
 *
 * If the reference is %NULL then this function does nothing. Otherwise, the
 * reference count of the handle is decreased and the pointer is set to %NULL.
 *
 * Since: 1.18
 */
static inline void
gst_clear_vulkan_handle (GstVulkanHandle ** handle_ptr)
{
  gst_clear_mini_object ((GstMiniObject **) handle_ptr);
}

GST_VULKAN_API
GstVulkanHandle *       gst_vulkan_handle_new_wrapped       (GstVulkanDevice *device,
                                                             GstVulkanHandleType type,
                                                             GstVulkanHandleTypedef handle,
                                                             GstVulkanHandleDestroyNotify notify,
                                                             gpointer user_data);

GST_VULKAN_API
void                    gst_vulkan_handle_free_descriptor_set_layout (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_pipeline_layout       (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_pipeline              (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_render_pass           (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_sampler               (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_framebuffer           (GstVulkanHandle * handle,
                                                                      gpointer user_data);
GST_VULKAN_API
void                    gst_vulkan_handle_free_shader                (GstVulkanHandle * handle,
                                                                      gpointer user_data);

G_END_DECLS

#endif /* _GST_VULKAN_HANDLE_H_ */
