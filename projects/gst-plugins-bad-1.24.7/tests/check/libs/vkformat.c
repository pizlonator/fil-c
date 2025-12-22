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
  gst_object_unref (instance);
  gst_object_unref (device);
}

GST_START_TEST (test_format_from_video_info_2)
{
  GstVulkanPhysicalDevice *phy_dev = device->physical_device;
  GstVideoInfo vinfo;
  VkFormat vk_fmts[GST_VIDEO_MAX_PLANES];
  int n_imgs;
  VkImageUsageFlags supported_usage;

  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_NV12, 620,
          480));

  fail_unless (gst_vulkan_format_from_video_info_2 (phy_dev, &vinfo,
          VK_IMAGE_TILING_OPTIMAL, TRUE, 0, vk_fmts, &n_imgs,
          &supported_usage));

  fail_unless (n_imgs == 2 && vk_fmts[0] == VK_FORMAT_R8_UNORM
      && vk_fmts[1] == VK_FORMAT_R8G8_UNORM);

  fail_unless (gst_vulkan_format_from_video_info_2 (phy_dev, &vinfo,
          VK_IMAGE_TILING_LINEAR, FALSE, 0, vk_fmts, &n_imgs,
          &supported_usage));

  fail_unless (n_imgs == 1 && vk_fmts[0] == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM);

  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGBA, 620,
          480));
  fail_unless (GST_VIDEO_INFO_COLORIMETRY (&vinfo).transfer ==
      GST_VIDEO_TRANSFER_SRGB);

  fail_unless (gst_vulkan_format_from_video_info_2 (phy_dev, &vinfo,
          VK_IMAGE_TILING_LINEAR, TRUE, 0, vk_fmts, &n_imgs, &supported_usage));

  fail_unless (n_imgs == 1 && vk_fmts[0] == VK_FORMAT_R8G8B8A8_SRGB);

  fail_unless (gst_video_info_set_format (&vinfo, GST_VIDEO_FORMAT_RGBA, 620,
          480));
  fail_unless (gst_video_colorimetry_from_string
      (&GST_VIDEO_INFO_COLORIMETRY (&vinfo), "smpte240m"));

  fail_unless (gst_vulkan_format_from_video_info_2 (phy_dev, &vinfo,
          VK_IMAGE_TILING_LINEAR, TRUE, 0, vk_fmts, &n_imgs, &supported_usage));

  fail_unless (n_imgs == 1 && vk_fmts[0] == VK_FORMAT_R8G8B8A8_UNORM);
}

GST_END_TEST;

static Suite *
vkformat_suite (void)
{
  Suite *s = suite_create ("vkmemory");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);
  tcase_add_checked_fixture (tc_basic, setup, teardown);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_format_from_video_info_2);
  }

  return s;
}


GST_CHECK_MAIN (vkformat);
