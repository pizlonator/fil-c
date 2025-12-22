/* GStreamer
 *
 * Copyright (C) 2023 Igalia, S.L.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/vulkan/vulkan.h>

static GstVulkanInstance *instance;
static GstVulkanDevice *device;
static GstVulkanQueue *queue = NULL;

static void
setup (void)
{
  instance = gst_vulkan_instance_new ();
  fail_unless (gst_vulkan_instance_open (instance, NULL));

  device = gst_vulkan_device_new_with_index (instance, 0);
  fail_unless (gst_vulkan_device_open (device, NULL));

}

static void
teardown (void)
{
  gst_clear_object (&queue);
  gst_clear_object (&device);
  gst_object_unref (instance);
}

static void
setup_queue (guint expected_flags)
{
  queue = gst_vulkan_device_select_queue (device, VK_QUEUE_COMPUTE_BIT);

  fail_unless (GST_IS_VULKAN_QUEUE (queue));
}

static GstBufferPool *
create_buffer_pool (const char *format, VkImageUsageFlags usage,
    VkImageLayout initial_layout, guint64 initial_access, GstCaps * dec_caps)
{
  GstCaps *caps;
  GstBufferPool *pool;
  GstStructure *config;

  caps = gst_caps_new_simple ("video/x-raw", "format", G_TYPE_STRING, format,
      "width", G_TYPE_INT, 1024, "height", G_TYPE_INT, 780, NULL);
  gst_caps_set_features_simple (caps,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_VULKAN_IMAGE, NULL));

  pool = gst_vulkan_image_buffer_pool_new (device);

  config = gst_buffer_pool_get_config (pool);

  gst_buffer_pool_config_set_params (config, caps, 1024, 1, 0);
  gst_caps_unref (caps);

  gst_vulkan_image_buffer_pool_config_set_allocation_params (config,
      usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, initial_layout,
      initial_access);

  if (dec_caps)
    gst_vulkan_image_buffer_pool_config_set_decode_caps (config, dec_caps);

  fail_unless (gst_buffer_pool_set_config (pool, config));
  gst_buffer_pool_set_active (pool, TRUE);

  return pool;
}

GST_START_TEST (test_image)
{
  GstBufferPool *pool;
  GstFlowReturn ret;
  GstBuffer *buffer = NULL;

  setup_queue (VK_QUEUE_COMPUTE_BIT);
  pool = create_buffer_pool ("NV12", 0, VK_IMAGE_LAYOUT_UNDEFINED, 0, NULL);

  ret = gst_buffer_pool_acquire_buffer (pool, &buffer, NULL);
  fail_unless (ret == GST_FLOW_OK);
  gst_buffer_unref (buffer);

  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);
}

GST_END_TEST;

#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
GST_START_TEST (test_vulkan_profiles)
{
  GstCaps *caps;
  /* *INDENT-OFF* */
  GstVulkanVideoProfile profile2 = { { 0, }, }, profile = {
    .profile = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
      .pNext = &profile.codec,
      .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR,
      .chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
      .chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
      .lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR,
    },
    .codec.h265dec = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR,
      .stdProfileIdc = STD_VIDEO_H265_PROFILE_IDC_MAIN_10,
    }
  };
  /* *INDENT-ON* */

  caps = gst_vulkan_video_profile_to_caps (&profile);
  fail_unless (caps);

  fail_unless (gst_vulkan_video_profile_from_caps (&profile2, caps,
          GST_VULKAN_VIDEO_OPERATION_DECODE));
  gst_caps_unref (caps);
  fail_unless (profile2.profile.sType
      == VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR);
  fail_unless (profile2.profile.videoCodecOperation
      == profile.profile.videoCodecOperation);
  fail_unless (profile2.codec.h265dec.stdProfileIdc
      == profile.codec.h265dec.stdProfileIdc);
}

GST_END_TEST;

GST_START_TEST (test_decoding_image)
{
  GstBufferPool *pool;
  GstCaps *dec_caps;
  GstBuffer *buffer = NULL;
  GstFlowReturn ret;
  /* *INDENT-OFF* */
  GstVulkanVideoProfile profile = {
    .profile = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR,
      .pNext = &profile.codec,
      .videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR,
      .chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR,
      .chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
      .lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR,
    },
    .codec.h264dec = {
      .sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H264_PROFILE_INFO_KHR,
      .stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_MAIN,
      .pictureLayout = VK_VIDEO_DECODE_H264_PICTURE_LAYOUT_PROGRESSIVE_KHR,
    }
  };
  /* *INDENT-ON* */

  /* force to use a queue with decoding support */
  if (queue && (device->physical_device->queue_family_ops[queue->family].video
          & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) == 0)
    gst_clear_object (&queue);

  if (!queue) {
    queue =
        gst_vulkan_device_select_queue (device, VK_QUEUE_VIDEO_DECODE_BIT_KHR);
  }

  if (!queue)
    return;

  if ((device->physical_device->queue_family_ops[queue->family].video
          & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR) == 0)
    return;

  dec_caps = gst_vulkan_video_profile_to_caps (&profile);
  fail_unless (dec_caps);

  pool = create_buffer_pool ("NV12", VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR,
      VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR, VK_ACCESS_TRANSFER_WRITE_BIT,
      dec_caps);

  gst_caps_unref (dec_caps);

  ret = gst_buffer_pool_acquire_buffer (pool, &buffer, NULL);
  fail_unless (ret == GST_FLOW_OK);
  gst_buffer_unref (buffer);

  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);
}

GST_END_TEST;
#endif

static Suite *
vkimagebufferpool_suite (void)
{
  Suite *s = suite_create ("vkimagebufferpool");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);
  tcase_add_checked_fixture (tc_basic, setup, teardown);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_image);
#if GST_VULKAN_HAVE_VIDEO_EXTENSIONS
    tcase_add_test (tc_basic, test_decoding_image);
    tcase_add_test (tc_basic, test_vulkan_profiles);
#endif
  }

  return s;
}

GST_CHECK_MAIN (vkimagebufferpool);
