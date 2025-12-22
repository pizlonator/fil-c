/* GStreamer
 *
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

static void
check_size (GstMemory * mem, gsize at_least)
{
  gsize size, maxsize, offset;

  size = gst_memory_get_sizes (mem, &offset, &maxsize);
  fail_unless (size <= maxsize);
  fail_unless (size >= at_least);
}

GST_START_TEST (test_buffer_mem_allocate)
{
  VkBufferUsageFlags usage;
  GstVulkanBufferMemory *vk_mem;
  gsize orig_size = 1024, size, offset;
  GstMemory *mem;

  usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  mem =
      gst_vulkan_buffer_memory_alloc (device, orig_size,
      usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  fail_unless (gst_is_vulkan_buffer_memory (mem));
  vk_mem = (GstVulkanBufferMemory *) mem;

  fail_unless (vk_mem->device == device);
  fail_unless (vk_mem->usage == usage);
  fail_unless (vk_mem->vk_mem != NULL);

  size = gst_memory_get_sizes (mem, &offset, NULL);
  fail_unless (offset == 0);
  check_size (mem, orig_size);
  fail_unless (vk_mem->requirements.size >= size);

  size = gst_memory_get_sizes ((GstMemory *) vk_mem->vk_mem, &offset, NULL);
  fail_unless (offset == 0);
  check_size ((GstMemory *) vk_mem->vk_mem, orig_size);

  gst_memory_unref (mem);
}

GST_END_TEST;

static Suite *
vkmemory_suite (void)
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
    tcase_add_test (tc_basic, test_buffer_mem_allocate);
  }

  return s;
}


GST_CHECK_MAIN (vkmemory);
