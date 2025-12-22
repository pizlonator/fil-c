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
 * SECTION:element-vulkanimageidentity
 * @title: vulkanimgeidentity
 *
 * vulkanimageidentity produces a vulkan image that is a copy of the input image.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstvulkanelements.h"
#include "vkimageidentity.h"

#include "shaders/identity.vert.h"
#include "shaders/identity.frag.h"

GST_DEBUG_CATEGORY (gst_debug_vulkan_image_identity);
#define GST_CAT_DEFAULT gst_debug_vulkan_image_identity

static gboolean gst_vulkan_image_identity_start (GstBaseTransform * bt);
static gboolean gst_vulkan_image_identity_stop (GstBaseTransform * bt);

static GstFlowReturn gst_vulkan_image_identity_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static gboolean gst_vulkan_image_identity_set_caps (GstBaseTransform * bt,
    GstCaps * in_caps, GstCaps * out_caps);

#define IMAGE_FORMATS " { BGRA }"

static GstStaticPadTemplate gst_vulkan_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS)));

static GstStaticPadTemplate gst_vulkan_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE,
            IMAGE_FORMATS)));

enum
{
  PROP_0,
};

enum
{
  SIGNAL_0,
  LAST_SIGNAL
};

/* static guint gst_vulkan_image_identity_signals[LAST_SIGNAL] = { 0 }; */

#define gst_vulkan_image_identity_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstVulkanImageIdentity, gst_vulkan_image_identity,
    GST_TYPE_VULKAN_VIDEO_FILTER,
    GST_DEBUG_CATEGORY_INIT (gst_debug_vulkan_image_identity,
        "vulkanimageidentity", 0, "Vulkan Image identity"));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (vulkanimageidentity,
    "vulkanimageidentity", GST_RANK_NONE, GST_TYPE_VULKAN_IMAGE_IDENTITY,
    vulkan_element_init (plugin));

static void
gst_vulkan_image_identity_class_init (GstVulkanImageIdentityClass * klass)
{
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  gst_element_class_set_metadata (gstelement_class, "Vulkan Image Identity",
      "Filter/Video", "A Vulkan image copier",
      "Matthew Waters <matthew@centricular.com>");

  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &gst_vulkan_src_template);

  gstbasetransform_class->start =
      GST_DEBUG_FUNCPTR (gst_vulkan_image_identity_start);
  gstbasetransform_class->stop =
      GST_DEBUG_FUNCPTR (gst_vulkan_image_identity_stop);
  gstbasetransform_class->set_caps = gst_vulkan_image_identity_set_caps;
  gstbasetransform_class->transform = gst_vulkan_image_identity_transform;
}

static void
gst_vulkan_image_identity_init (GstVulkanImageIdentity * vk_identity)
{
}

static gboolean
gst_vulkan_image_identity_set_caps (GstBaseTransform * bt, GstCaps * in_caps,
    GstCaps * out_caps)
{
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (bt);
  GstVulkanImageIdentity *vk_identity = GST_VULKAN_IMAGE_IDENTITY (bt);

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->set_caps (bt, in_caps,
          out_caps))
    return FALSE;

  if (!gst_vulkan_full_screen_quad_set_info (vk_identity->quad,
          &vfilter->in_info, &vfilter->out_info))
    return FALSE;

  return TRUE;
}

static gboolean
gst_vulkan_image_identity_start (GstBaseTransform * bt)
{
  GstVulkanImageIdentity *vk_identity = GST_VULKAN_IMAGE_IDENTITY (bt);
  GstVulkanVideoFilter *vfilter = GST_VULKAN_VIDEO_FILTER (vk_identity);
  GstVulkanHandle *vert, *frag;
  GError *error = NULL;

  if (!GST_BASE_TRANSFORM_CLASS (parent_class)->start (bt))
    return FALSE;

  vk_identity->quad = gst_vulkan_full_screen_quad_new (vfilter->queue);

  if (!(vert = gst_vulkan_create_shader (vfilter->device, identity_vert,
              identity_vert_size, &error)))
    goto error;
  if (!(frag = gst_vulkan_create_shader (vfilter->device, identity_frag,
              identity_frag_size, &error))) {
    gst_vulkan_handle_unref (vert);
    goto error;
  }
  gst_vulkan_full_screen_quad_set_shaders (vk_identity->quad, vert, frag);

  gst_vulkan_handle_unref (vert);
  gst_vulkan_handle_unref (frag);

  return TRUE;

error:
  GST_ELEMENT_ERROR (bt, RESOURCE, NOT_FOUND, ("%s", error->message), (NULL));
  return FALSE;
}

static gboolean
gst_vulkan_image_identity_stop (GstBaseTransform * bt)
{
  GstVulkanImageIdentity *vk_identity = GST_VULKAN_IMAGE_IDENTITY (bt);

  gst_clear_object (&vk_identity->quad);

  return GST_BASE_TRANSFORM_CLASS (parent_class)->stop (bt);
}

static GstFlowReturn
gst_vulkan_image_identity_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstVulkanImageIdentity *vk_identity = GST_VULKAN_IMAGE_IDENTITY (bt);
  GError *error = NULL;

  if (!gst_vulkan_full_screen_quad_set_input_buffer (vk_identity->quad, inbuf,
          &error))
    goto error;
  if (!gst_vulkan_full_screen_quad_set_output_buffer (vk_identity->quad, outbuf,
          &error))
    goto error;

  if (!gst_vulkan_full_screen_quad_draw (vk_identity->quad, &error))
    goto error;

  return GST_FLOW_OK;

error:
  GST_ELEMENT_ERROR (bt, LIBRARY, FAILED, ("%s", error->message), (NULL));
  g_clear_error (&error);
  return GST_FLOW_ERROR;
}
