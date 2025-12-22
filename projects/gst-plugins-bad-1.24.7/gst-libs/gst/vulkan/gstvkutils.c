/*
 * GStreamer
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#include "gstvkutils.h"

/**
 * SECTION:vkutils
 * @title: Vulkan Utils
 * @short_description: Vulkan utilities
 * @see_also: #GstVulkanInstance, #GstVulkanDevice
 */

GST_DEBUG_CATEGORY_STATIC (GST_CAT_CONTEXT);

static void
_init_context_debug (void)
{
#ifndef GST_DISABLE_GST_DEBUG
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_GET (GST_CAT_CONTEXT, "GST_CONTEXT");
    g_once_init_leave (&_init, 1);
  }
#endif
}

static gboolean
_vk_pad_query (const GValue * item, GValue * value, gpointer user_data)
{
  GstPad *pad = g_value_get_object (item);
  GstQuery *query = user_data;
  gboolean res;

  _init_context_debug ();

  res = gst_pad_peer_query (pad, query);

  if (res) {
    g_value_set_boolean (value, TRUE);
    return FALSE;
  }

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, pad, "pad peer query failed");
  return TRUE;
}

/**
 * gst_vulkan_run_query:
 * @element: a #GstElement
 * @query: the #GstQuery to perform
 * @direction: the #GstPadDirection to perform query on
 *
 * Returns: whether @query was answered successfully
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_run_query (GstElement * element, GstQuery * query,
    GstPadDirection direction)
{
  GstIterator *it;
  GstIteratorFoldFunction func = _vk_pad_query;
  GValue res = { 0 };

  g_value_init (&res, G_TYPE_BOOLEAN);
  g_value_set_boolean (&res, FALSE);

  /* Ask neighbor */
  if (direction == GST_PAD_SRC)
    it = gst_element_iterate_src_pads (element);
  else
    it = gst_element_iterate_sink_pads (element);

  while (gst_iterator_fold (it, func, &res, query) == GST_ITERATOR_RESYNC)
    gst_iterator_resync (it);

  gst_iterator_free (it);

  return g_value_get_boolean (&res);
}

static GstQuery *
_vulkan_local_context_query (GstElement * element,
    const gchar * context_type, gboolean set_context)
{
  GstQuery *query;
  GstContext *ctxt;

  _init_context_debug ();

  /*  2a) Query downstream with GST_QUERY_CONTEXT for the context and
   *      check if downstream already has a context of the specific type
   *  2b) Query upstream as above.
   */
  query = gst_query_new_context (context_type);
  if (gst_vulkan_run_query (element, query, GST_PAD_SRC)) {
    gst_query_parse_context (query, &ctxt);
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "found context (%p) in downstream query", ctxt);
    if (set_context)
      gst_element_set_context (element, ctxt);
  } else if (gst_vulkan_run_query (element, query, GST_PAD_SINK)) {
    gst_query_parse_context (query, &ctxt);
    GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
        "found context (%p) in upstream query", ctxt);
    if (set_context)
      gst_element_set_context (element, ctxt);
  } else {
    gst_query_unref (query);
    query = NULL;
  }

  return query;
}

/**
 * gst_vulkan_global_context_query:
 * @element: a #GstElement
 * @context_type: the context type to query for
 *
 * Performs the steps necessary for executing a context query including
 * posting a message for the application to respond.
 *
 * Since: 1.18
 */
void
gst_vulkan_global_context_query (GstElement * element,
    const gchar * context_type)
{
  GstQuery *query;
  GstMessage *msg;

  if ((query = _vulkan_local_context_query (element, context_type, TRUE))) {
    gst_query_unref (query);
    return;
  }

  /* 3) Post a GST_MESSAGE_NEED_CONTEXT message on the bus with
   *    the required context type and afterwards check if a
   *    usable context was set now as in 1). The message could
   *    be handled by the parent bins of the element and the
   *    application.
   */
  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
      "posting need context message");
  msg = gst_message_new_need_context (GST_OBJECT_CAST (element), context_type);
  gst_element_post_message (element, msg);

  /*
   * Whomever responds to the need-context message performs a
   * GstElement::set_context() with the required context in which the element
   * is required to update the display_ptr or call gst_vulkan_handle_set_context().
   */
}

/**
 * gst_vulkan_local_context_query:
 * @element: a #GstElement
 * @context_type: the context type to query for
 *
 * Performs the steps necessary for executing a context query between only
 * other elements in the pipeline
 *
 * Since: 1.18
 */
GstQuery *
gst_vulkan_local_context_query (GstElement * element,
    const gchar * context_type)
{
  return _vulkan_local_context_query (element, context_type, FALSE);
}

static void
_vk_display_context_query (GstElement * element,
    GstVulkanDisplay ** display_ptr)
{
  gst_vulkan_global_context_query (element,
      GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR);
}

/*  4) Create a context by itself and post a GST_MESSAGE_HAVE_CONTEXT
 *     message.
 */
/*
 * @element: (transfer none):
 * @context: (transfer full):
 */
static void
_vk_context_propagate (GstElement * element, GstContext * context)
{
  GstMessage *msg;

  _init_context_debug ();

  gst_element_set_context (element, context);

  GST_CAT_INFO_OBJECT (GST_CAT_CONTEXT, element,
      "posting have context (%" GST_PTR_FORMAT ") message", context);
  msg = gst_message_new_have_context (GST_OBJECT_CAST (element), context);
  gst_element_post_message (GST_ELEMENT_CAST (element), msg);
}

/**
 * gst_vulkan_ensure_element_data:
 * @element: a #GstElement
 * @display_ptr: (inout) (optional): the resulting #GstVulkanDisplay
 * @instance_ptr: (inout): the resulting #GstVulkanInstance
 *
 * Perform the steps necessary for retrieving a #GstVulkanInstance and
 * (optionally) an #GstVulkanDisplay from the surrounding elements or from
 * the application using the #GstContext mechanism.
 *
 * If the contents of @display_ptr or @instance_ptr are not %NULL, then no
 * #GstContext query is necessary and no #GstVulkanInstance or #GstVulkanDisplay
 * retrieval is performed.
 *
 * Returns: whether a #GstVulkanInstance exists in @instance_ptr and if
 *          @display_ptr is not %NULL, whether a #GstVulkanDisplay exists in
 *          @display_ptr
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_ensure_element_data (GstElement * element,
    GstVulkanDisplay ** display_ptr, GstVulkanInstance ** instance_ptr)
{
  g_return_val_if_fail (element != NULL, FALSE);
  g_return_val_if_fail (instance_ptr != NULL, FALSE);

  /*  1) Check if the element already has a context of the specific
   *     type.
   */
  if (!*instance_ptr) {
    GError *error = NULL;
    GstContext *context = NULL;

    gst_vulkan_global_context_query (element,
        GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR);

    /* Neighbour found and it updated the display */
    if (!*instance_ptr) {
      /* If no neighboor, or application not interested, use system default */
      *instance_ptr = gst_vulkan_instance_new ();

      context = gst_context_new (GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR, TRUE);
      gst_context_set_vulkan_instance (context, *instance_ptr);
    }

    if (!gst_vulkan_instance_open (*instance_ptr, &error)) {
      GST_ELEMENT_ERROR (element, RESOURCE, NOT_FOUND,
          ("Failed to create vulkan instance"), ("%s", error->message));
      gst_object_unref (*instance_ptr);
      *instance_ptr = NULL;
      g_clear_error (&error);
      return FALSE;
    }

    if (context)
      _vk_context_propagate (element, context);
  }

  /* we don't care about a display */
  if (!display_ptr)
    return *instance_ptr != NULL;

  if (!*display_ptr) {
    _vk_display_context_query (element, display_ptr);

    /* Neighbour found and it updated the display */
    if (!*display_ptr) {
      GstContext *context;

      /* instance is required before the display */
      g_return_val_if_fail (*instance_ptr != NULL, FALSE);

      /* If no neighboor, or application not interested, use system default */
      *display_ptr = gst_vulkan_display_new (*instance_ptr);

      context = gst_context_new (GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR, TRUE);
      gst_context_set_vulkan_display (context, *display_ptr);

      _vk_context_propagate (element, context);
    }
  }

  return *display_ptr != NULL && *instance_ptr != NULL;
}

/**
 * gst_vulkan_handle_set_context:
 * @element: a #GstElement
 * @context: a #GstContext
 * @display: (inout) (transfer full) (optional): location of a #GstVulkanDisplay
 * @instance: (inout) (transfer full): location of a #GstVulkanInstance
 *
 * Helper function for implementing #GstElementClass.set_context() in
 * Vulkan capable elements.
 *
 * Retrieve's the #GstVulkanDisplay or #GstVulkanInstance in @context and places
 * the result in @display or @instance respectively.
 *
 * Returns: whether the @display or @instance could be set successfully
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_handle_set_context (GstElement * element, GstContext * context,
    GstVulkanDisplay ** display, GstVulkanInstance ** instance)
{
  GstVulkanDisplay *display_replacement = NULL;
  GstVulkanInstance *instance_replacement = NULL;
  const gchar *context_type;

  g_return_val_if_fail (instance != NULL, FALSE);

  if (!context)
    return FALSE;

  context_type = gst_context_get_context_type (context);

  if (display
      && g_strcmp0 (context_type, GST_VULKAN_DISPLAY_CONTEXT_TYPE_STR) == 0) {
    if (!gst_context_get_vulkan_display (context, &display_replacement)) {
      GST_WARNING_OBJECT (element, "Failed to get display from context");
      return FALSE;
    }
  } else if (g_strcmp0 (context_type,
          GST_VULKAN_INSTANCE_CONTEXT_TYPE_STR) == 0) {
    if (!gst_context_get_vulkan_instance (context, &instance_replacement)) {
      GST_WARNING_OBJECT (element, "Failed to get instance from context");
      return FALSE;
    }
  }

  if (display_replacement) {
    GstVulkanDisplay *old = *display;
    *display = display_replacement;

    if (old)
      gst_object_unref (old);
  }

  if (instance_replacement) {
    GstVulkanInstance *old = *instance;
    *instance = instance_replacement;

    if (old)
      gst_object_unref (old);
  }

  return TRUE;
}

/**
 * gst_vulkan_handle_context_query:
 * @element: a #GstElement
 * @query: a #GstQuery of type %GST_QUERY_CONTEXT
 * @display: (transfer none) (nullable): a #GstVulkanDisplay
 * @instance: (transfer none) (nullable): a #GstVulkanInstance
 * @device: (transfer none) (nullable): a #GstVulkanInstance
 *
 * Returns: Whether the @query was successfully responded to from the passed
 *          @display, @instance, and @device.
 *
 * Since: 1.18
 */
gboolean
gst_vulkan_handle_context_query (GstElement * element, GstQuery * query,
    GstVulkanDisplay * display, GstVulkanInstance * instance,
    GstVulkanDevice * device)
{
  if (gst_vulkan_display_handle_context_query (element, query, display))
    return TRUE;
  if (gst_vulkan_instance_handle_context_query (element, query, instance))
    return TRUE;
  if (gst_vulkan_device_handle_context_query (element, query, device))
    return TRUE;

  return FALSE;
}

static void
fill_vulkan_image_view_info (VkImage image, VkFormat format,
    VkImageViewCreateInfo * info)
{
  /* *INDENT-OFF* */
  *info = (VkImageViewCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .image = image,
      .format = format,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .flags = 0,
      .components = (VkComponentMapping) {
          VK_COMPONENT_SWIZZLE_R,
          VK_COMPONENT_SWIZZLE_G,
          VK_COMPONENT_SWIZZLE_B,
          VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = (VkImageSubresourceRange) {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }
  };
  /* *INDENT-ON* */
}

static gboolean
find_compatible_view (GstVulkanImageView * view, VkImageViewCreateInfo * info)
{
  return view->create_info.image == info->image
      && view->create_info.format == info->format
      && view->create_info.viewType == info->viewType
      && view->create_info.flags == info->flags
      && view->create_info.components.r == info->components.r
      && view->create_info.components.g == info->components.g
      && view->create_info.components.b == info->components.b
      && view->create_info.components.a == info->components.a
      && view->create_info.subresourceRange.aspectMask ==
      info->subresourceRange.aspectMask
      && view->create_info.subresourceRange.baseMipLevel ==
      info->subresourceRange.baseMipLevel
      && view->create_info.subresourceRange.levelCount ==
      info->subresourceRange.levelCount
      && view->create_info.subresourceRange.baseArrayLayer ==
      info->subresourceRange.baseArrayLayer
      && view->create_info.subresourceRange.layerCount ==
      info->subresourceRange.layerCount;
}

/**
 * gst_vulkan_get_or_create_image_view
 * @image: a #GstVulkanImageMemory
 *
 * Returns: (transfer full): a #GstVulkanImageView for @image matching the
 *                           original layout and format of @image
 *
 * Since: 1.18
 */
GstVulkanImageView *
gst_vulkan_get_or_create_image_view (GstVulkanImageMemory * image)
{
  return gst_vulkan_get_or_create_image_view_with_info (image, NULL);
}

/**
 * gst_vulkan_get_or_create_image_view_with_info
 * @image: a #GstVulkanImageMemory
 * @create_info: (nullable): a VkImageViewCreateInfo
 *
 * Create a new #GstVulkanImageView with a specific @create_info.
 *
 * Returns: (transfer full): a #GstVulkanImageView for @image matching the
 *                           original layout and format of @image
 *
 * Since: 1.24
 */
GstVulkanImageView *
gst_vulkan_get_or_create_image_view_with_info (GstVulkanImageMemory * image,
    VkImageViewCreateInfo * create_info)
{
  VkImageViewCreateInfo _create_info;
  GstVulkanImageView *ret;

  if (!create_info) {
    fill_vulkan_image_view_info (image->image, image->create_info.format,
        &_create_info);
    create_info = &_create_info;
  } else if (!(create_info->format == image->create_info.format
          && create_info->image == image->image)) {
    return NULL;
  }

  ret = gst_vulkan_image_memory_find_view (image,
      (GstVulkanImageMemoryFindViewFunc) find_compatible_view, create_info);
  if (!ret) {
    ret = gst_vulkan_image_view_new (image, create_info);
    gst_vulkan_image_memory_add_view (image, ret);
  }

  return ret;
}

#define SPIRV_MAGIC_NUMBER_NE 0x07230203
#define SPIRV_MAGIC_NUMBER_OE 0x03022307

/**
 * gst_vulkan_create_shader
 * @device: a #GstVulkanDevice
 * @code: the SPIR-V shader byte code
 * @size: length of @code.  Must be a multiple of 4
 * @error: (out) (optional): a #GError to fill on failure
 *
 * Returns: (transfer full): a #GstVulkanHandle for @image matching the
 *                           original layout and format of @image or %NULL
 *
 * Since: 1.18
 */
GstVulkanHandle *
gst_vulkan_create_shader (GstVulkanDevice * device, const gchar * code,
    gsize size, GError ** error)
{
  VkShaderModule shader;
  VkResult res;

  /* *INDENT-OFF* */
  VkShaderModuleCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .codeSize = size,
      .pCode = (const guint32 *) code
  };
  /* *INDENT-ON* */
  guint32 first_word;
  guint32 *new_code = NULL;

  g_return_val_if_fail (size >= 4, VK_NULL_HANDLE);
  g_return_val_if_fail (size % 4 == 0, VK_NULL_HANDLE);

  first_word = code[0] | code[1] << 8 | code[2] << 16 | code[3] << 24;
  g_return_val_if_fail (first_word == SPIRV_MAGIC_NUMBER_NE
      || first_word == SPIRV_MAGIC_NUMBER_OE, VK_NULL_HANDLE);
  if (first_word == SPIRV_MAGIC_NUMBER_OE) {
    /* endianness swap... */
    const guint32 *old_code = (const guint32 *) code;
    gsize i;

    GST_DEBUG ("performaing endianness conversion on spirv shader of size %"
        G_GSIZE_FORMAT, size);
    new_code = g_new0 (guint32, size / 4);

    for (i = 0; i < size / 4; i++) {
      guint32 old = old_code[i];
      guint32 new = 0;

      new |= (old & 0xff) << 24;
      new |= (old & 0xff00) << 8;
      new |= (old & 0xff0000) >> 8;
      new |= (old & 0xff000000) >> 24;
      new_code[i] = new;
    }

    first_word = ((guint32 *) new_code)[0];
    g_assert (first_word == SPIRV_MAGIC_NUMBER_NE);

    info.pCode = new_code;
  }

  res = vkCreateShaderModule (device->device, &info, NULL, &shader);
  g_free (new_code);
  if (gst_vulkan_error_to_g_error (res, error, "VkCreateShaderModule") < 0)
    return NULL;

  return gst_vulkan_handle_new_wrapped (device, GST_VULKAN_HANDLE_TYPE_SHADER,
      (GstVulkanHandleTypedef) shader, gst_vulkan_handle_free_shader, NULL);
}
