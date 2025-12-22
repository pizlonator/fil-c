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
#include <gst/check/gstharness.h>
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

static GstVulkanImageMemory *
create_image_mem (GstVideoInfo * v_info)
{
  GstVulkanImageMemory *vk_mem;
  VkImageUsageFlags usage;
  VkFormat vk_format;
  GstMemory *mem;

  vk_format = gst_vulkan_format_from_video_info (v_info, 0);

  usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
      | VK_IMAGE_USAGE_SAMPLED_BIT;
  mem =
      gst_vulkan_image_memory_alloc (device, vk_format,
      GST_VIDEO_INFO_COMP_WIDTH (v_info, 0),
      GST_VIDEO_INFO_COMP_HEIGHT (v_info, 0), VK_IMAGE_TILING_LINEAR,
      usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  fail_unless (gst_is_vulkan_image_memory (mem));
  vk_mem = (GstVulkanImageMemory *) mem;
  fail_unless (vk_mem->usage == usage);
  return vk_mem;
}

GST_START_TEST (test_image_new)
{
  GstVulkanImageMemory *vk_mem;
  GstVideoInfo v_info;
  gsize offset, size;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 16, 16);
  vk_mem = create_image_mem (&v_info);

  fail_unless (vk_mem->device == device);
  fail_unless (vk_mem->vk_mem != NULL);

  size = gst_memory_get_sizes ((GstMemory *) vk_mem, &offset, NULL);
  fail_unless (offset == 0);
  check_size ((GstMemory *) vk_mem, v_info.size);
  fail_unless (vk_mem->requirements.size >= size);

  size = gst_memory_get_sizes ((GstMemory *) vk_mem->vk_mem, &offset, NULL);
  fail_unless (offset == 0);
  check_size ((GstMemory *) vk_mem->vk_mem, v_info.size);

  gst_memory_unref ((GstMemory *) vk_mem);
}

GST_END_TEST;

GST_START_TEST (test_image_view_new)
{
  GstVulkanImageMemory *vk_mem;
  GstVulkanImageView *view;
  GstVideoInfo v_info;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 16, 16);
  vk_mem = create_image_mem (&v_info);

  view = gst_vulkan_get_or_create_image_view (vk_mem);

  gst_vulkan_image_view_unref (view);
  gst_memory_unref ((GstMemory *) vk_mem);
}

GST_END_TEST;

GST_START_TEST (test_image_view_get)
{
  GstVulkanImageMemory *vk_mem;
  GstVulkanImageView *view;
  GstVideoInfo v_info;

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 16, 16);
  vk_mem = create_image_mem (&v_info);

  view = gst_vulkan_get_or_create_image_view (vk_mem);
  gst_vulkan_image_view_unref (view);
  view = gst_vulkan_get_or_create_image_view (vk_mem);
  gst_vulkan_image_view_unref (view);

  gst_memory_unref ((GstMemory *) vk_mem);
}

GST_END_TEST;

#define N_THREADS 2
#define N_MEMORY 4
#define N_OPS 512

struct view_stress
{
  GMutex lock;
  GCond cond;
  gboolean ready;
  int n_ops;
  GQueue *memories;
  GstHarnessThread *threads[N_THREADS];
};

static void
wait_for_ready (GstHarnessThread * thread, struct view_stress *stress)
{
  g_mutex_lock (&stress->lock);
  while (!stress->ready)
    g_cond_wait (&stress->cond, &stress->lock);
  g_mutex_unlock (&stress->lock);
}

static void
get_unref_image_view (GstHarnessThread * thread, struct view_stress *stress)
{
  int rand = g_random_int_range (0, N_MEMORY);
  GstVulkanImageMemory *mem;
  GstVulkanImageView *view;

  mem = g_queue_peek_nth (stress->memories, rand);
  view = gst_vulkan_get_or_create_image_view (mem);
  gst_vulkan_image_view_unref (view);

  g_atomic_int_inc (&stress->n_ops);
  if (g_atomic_int_get (&stress->n_ops) > N_OPS)
    g_usleep (100);
}

GST_START_TEST (test_image_view_stress)
{
  GstHarness *h = gst_harness_new_empty ();
  struct view_stress stress;
  GstVideoInfo v_info;
  int i;

  g_mutex_init (&stress.lock);
  g_cond_init (&stress.cond);
  stress.ready = FALSE;
  g_atomic_int_set (&stress.n_ops, 0);
  stress.memories = g_queue_new ();

  gst_video_info_set_format (&v_info, GST_VIDEO_FORMAT_RGBA, 16, 16);
  for (i = 0; i < N_MEMORY; i++) {
    g_queue_push_head (stress.memories, create_image_mem (&v_info));
  }

  g_mutex_lock (&stress.lock);
  for (i = 0; i < N_THREADS; i++) {
    stress.threads[i] = gst_harness_stress_custom_start (h,
        (GFunc) wait_for_ready, (GFunc) get_unref_image_view, &stress, 10);
  }
  stress.ready = TRUE;
  g_cond_broadcast (&stress.cond);
  g_mutex_unlock (&stress.lock);

  while (g_atomic_int_get (&stress.n_ops) < N_OPS)
    g_usleep (10000);

  for (i = 0; i < N_THREADS; i++) {
    gst_harness_stress_thread_stop (stress.threads[i]);
  }

  g_mutex_clear (&stress.lock);
  g_cond_clear (&stress.cond);
  g_queue_free_full (stress.memories, (GDestroyNotify) gst_memory_unref);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
vkimage_suite (void)
{
  Suite *s = suite_create ("vkimage");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);
  tcase_add_checked_fixture (tc_basic, setup, teardown);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_image_new);
    tcase_add_test (tc_basic, test_image_view_new);
    tcase_add_test (tc_basic, test_image_view_get);
    tcase_add_test (tc_basic, test_image_view_stress);
  }

  return s;
}


GST_CHECK_MAIN (vkimage);
