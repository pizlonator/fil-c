/* GStreamer
 *
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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

GST_START_TEST (test_instance_new)
{
  GstVulkanInstance *instance;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  gst_object_unref (instance);
}

GST_END_TEST;

GST_START_TEST (test_instance_open)
{
  GstVulkanInstance *instance;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  fail_unless (gst_vulkan_instance_open (instance, NULL));
  gst_object_unref (instance);
}

GST_END_TEST;

GST_START_TEST (test_instance_version_before_open)
{
  GstVulkanInstance *instance;
  guint major, minor, patch;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  gst_vulkan_instance_get_version (instance, &major, &minor, &patch);
  gst_object_unref (instance);
}

GST_END_TEST;

GST_START_TEST (test_instance_default_max_version)
{
  GstVulkanInstance *instance;
  guint major, minor, patch;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  gst_vulkan_instance_get_version (instance, &major, &minor, &patch);
  fail_unless (gst_vulkan_instance_open (instance, NULL));
  fail_unless (gst_vulkan_instance_check_version (instance, 1, 0, 0));
  fail_unless (gst_vulkan_instance_check_version (instance, major, minor,
          patch));
  fail_unless (!gst_vulkan_instance_check_version (instance, major, minor,
          patch + 1));
  fail_unless (!gst_vulkan_instance_check_version (instance, major, minor + 1,
          patch));
  gst_object_unref (instance);
}

GST_END_TEST;

GST_START_TEST (test_instance_request_version)
{
  GstVulkanInstance *instance;
  guint major, minor;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  gst_vulkan_instance_get_version (instance, &major, &minor, NULL);

  if (major > 1 || minor > 0) {
    g_object_set (instance, "requested-api-major", 1, "requested_api_minor", 0,
        NULL);
    fail_unless (gst_vulkan_instance_open (instance, NULL));
    fail_unless (gst_vulkan_instance_check_version (instance, 1, 0, 0));
    fail_unless (!gst_vulkan_instance_check_version (instance, major, minor,
            0));
    fail_unless (!gst_vulkan_instance_check_version (instance, major, minor + 1,
            0));
  }
  gst_object_unref (instance);
}

GST_END_TEST;

GST_START_TEST (test_instance_enable_extension)
{
  GstVulkanInstance *instance;
  /* test with a very common extension */
  const gchar *test_ext_name = VK_KHR_SURFACE_EXTENSION_NAME;

  instance = gst_vulkan_instance_new ();
  fail_unless (instance != NULL);
  fail_unless (gst_vulkan_instance_fill_info (instance, NULL));

  /* only run the test if the extension is available. otherwise, skip. */
  if (gst_vulkan_instance_get_extension_info (instance, test_ext_name, NULL)) {
    /* ensure it has been disabled */
    if (gst_vulkan_instance_is_extension_enabled (instance, test_ext_name))
      gst_vulkan_instance_disable_extension (instance, test_ext_name);

    fail_unless (gst_vulkan_instance_enable_extension (instance,
            test_ext_name));
    fail_unless (gst_vulkan_instance_is_extension_enabled (instance,
            test_ext_name));
    fail_unless (gst_vulkan_instance_disable_extension (instance,
            test_ext_name));
    fail_unless (!gst_vulkan_instance_is_extension_enabled (instance,
            test_ext_name));

    fail_unless (gst_vulkan_instance_enable_extension (instance,
            test_ext_name));
    fail_unless (gst_vulkan_instance_open (instance, NULL));
    fail_unless (gst_vulkan_instance_is_extension_enabled (instance,
            test_ext_name));
  }

  gst_object_unref (instance);
}

GST_END_TEST;

static Suite *
vkinstance_suite (void)
{
  Suite *s = suite_create ("vkinstance");
  TCase *tc_basic = tcase_create ("general");
  GstVulkanInstance *instance;
  gboolean have_instance;

  suite_add_tcase (s, tc_basic);

  tcase_add_test (tc_basic, test_instance_new);
  tcase_add_test (tc_basic, test_instance_version_before_open);

  /* FIXME: CI doesn't have a software vulkan renderer (and none exists currently) */
  instance = gst_vulkan_instance_new ();
  have_instance = gst_vulkan_instance_open (instance, NULL);
  gst_object_unref (instance);
  if (have_instance) {
    tcase_add_test (tc_basic, test_instance_open);
    tcase_add_test (tc_basic, test_instance_default_max_version);
    tcase_add_test (tc_basic, test_instance_request_version);
    tcase_add_test (tc_basic, test_instance_enable_extension);
  }

  return s;
}

GST_CHECK_MAIN (vkinstance);
