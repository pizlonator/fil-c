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
static GstVulkanQueue *queue;

static void
setup (void)
{
  instance = gst_vulkan_instance_new ();
  fail_unless (gst_vulkan_instance_open (instance, NULL));
  device = gst_vulkan_device_new_with_index (instance, 0);
  fail_unless (gst_vulkan_device_open (device, NULL));
  /* family and id may be wrong! */
  queue = gst_vulkan_device_get_queue (device, 0, 0);
  fail_unless (GST_IS_VULKAN_QUEUE (queue));
}

static void
teardown (void)
{
  gst_object_unref (instance);
  gst_object_unref (device);
  gst_object_unref (queue);
}

GST_START_TEST (test_new)
{
  GstVulkanCommandPool *pool =
      gst_vulkan_queue_create_command_pool (queue, NULL);
  fail_unless (GST_IS_VULKAN_COMMAND_POOL (pool));
  gst_object_unref (pool);
}

GST_END_TEST;

static void
buffer_destroy_notify (gpointer ptr)
{
  gint *counter = ptr;

  GST_DEBUG ("buffer destroyed");

  *counter += 1;
}

/* Track when a buffer is destroyed. The counter will be increased if the
 * buffer is finalized (but not if it was re-surrected in dispose and put
 * back into the buffer pool. */
static void
buffer_track_destroy (GstVulkanCommandBuffer * buf, gint * counter)
{
  gst_mini_object_set_qdata (GST_MINI_OBJECT (buf),
      g_quark_from_static_string ("TestTracker"),
      counter, buffer_destroy_notify);
}

GST_START_TEST (test_recycle)
{
  GstVulkanCommandPool *pool =
      gst_vulkan_queue_create_command_pool (queue, NULL);
  GstVulkanCommandBuffer *cmd;
  gint dcount = 0;

  fail_unless (GST_IS_VULKAN_COMMAND_POOL (pool));
  cmd = gst_vulkan_command_pool_create (pool, NULL);
  fail_unless (cmd != NULL);
  buffer_track_destroy (cmd, &dcount);

  gst_vulkan_command_buffer_unref (cmd);
  /* buffer should have been recycled */
  fail_unless (dcount == 0);

  gst_object_unref (pool);
}

GST_END_TEST;

static Suite *
vkcommandpool_suite (void)
{
  Suite *s = suite_create ("vkcommandpool");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);
  tcase_add_checked_fixture (tc_basic, setup, teardown);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_new);
    tcase_add_test (tc_basic, test_recycle);
  }

  return s;
}

GST_CHECK_MAIN (vkcommandpool);
