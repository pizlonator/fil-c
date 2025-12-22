/* GStreamer Vulkan device provider unit test
 *
 * Copyright (C) 2019 Matthew Wayers <matthew@centricular.com>
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

#include <gst/check/gstcheck.h>
#include <gst/vulkan/vulkan.h>

static GstDeviceMonitor *
vulkan_sink_device_provider (void)
{
  GstDeviceMonitor *monitor = gst_device_monitor_new ();
  GstCaps *caps;

  caps = gst_caps_from_string ("video/x-raw(memory:VulkanImage)");
  gst_device_monitor_add_filter (monitor, "Video/Sink", caps);
  gst_caps_unref (caps);

  gst_device_monitor_start (monitor);

  return monitor;
}

GST_START_TEST (vulkan_provider_creation)
{
  GstDeviceMonitor *monitor;
  GList *devices, *l;
  gboolean found_vulkan_device = FALSE;

  monitor = vulkan_sink_device_provider ();
  devices = gst_device_monitor_get_devices (monitor);

  for (l = devices; l; l = g_list_next (l)) {
    GstDevice *device = GST_DEVICE (l->data);
    GstVulkanPhysicalDevice *vk_phys_device;
    GstVulkanDevice *elem_device;
    GstElement *pipeline, *sink, *upload, *src;
    GParamSpec *pspec;

    if (!(pspec =
            g_object_class_find_property (G_OBJECT_GET_CLASS (device),
                "physical-device")))
      continue;
    if (G_PARAM_SPEC_VALUE_TYPE (pspec) != GST_TYPE_VULKAN_PHYSICAL_DEVICE)
      continue;

    found_vulkan_device = TRUE;
    g_object_get (device, "physical-device", &vk_phys_device, NULL);
    fail_unless (GST_IS_VULKAN_PHYSICAL_DEVICE (vk_phys_device));

    pipeline = gst_pipeline_new ("vkdeviceprovider");
    src = gst_element_factory_make ("videotestsrc", NULL);
    upload = gst_element_factory_make ("vulkanupload", NULL);
    sink = gst_device_create_element (device, NULL);

    fail_unless (gst_bin_add (GST_BIN (pipeline), src));
    fail_unless (gst_bin_add (GST_BIN (pipeline), upload));
    fail_unless (gst_bin_add (GST_BIN (pipeline), sink));
    fail_unless (gst_element_link_many (src, upload, sink, NULL));
    gst_element_set_state (pipeline, GST_STATE_READY);

    g_object_get (sink, "device", &elem_device, NULL);
    fail_unless (GST_IS_VULKAN_DEVICE (elem_device));

    GST_DEBUG ("%p =? %p", vk_phys_device, elem_device->physical_device);
    fail_unless (vk_phys_device == elem_device->physical_device);

    gst_object_unref (vk_phys_device);
    gst_object_unref (elem_device);

    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
  }
  g_list_free_full (devices, gst_object_unref);
  gst_device_monitor_stop (monitor);
  gst_object_unref (monitor);

  if (!found_vulkan_device)
    GST_WARNING ("No vulkan devices found");
}

GST_END_TEST;

static Suite *
vkdeviceprovider_suite (void)
{
  Suite *s = suite_create ("vkdeviceprovider");
  TCase *tc_basic = tcase_create ("general");
  GstVulkanInstance *instance;
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, vulkan_provider_creation);
  }

  return s;
}

GST_CHECK_MAIN (vkdeviceprovider);
