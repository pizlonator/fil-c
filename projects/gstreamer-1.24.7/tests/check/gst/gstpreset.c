/* GStreamer
 * Copyright (C) 2008 Nokia Corporation and its subsidiary(-ies)
 *               contact: <stefan.kost@nokia.com>
 *
 * gstpreset.c: Unit test for GstPreset
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
#  include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <gst/check/gstcheck.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static GType gst_preset_test_get_type (void);

#define GST_TYPE_PRESET_TEST            (gst_preset_test_get_type ())
#define GST_PRESET_TEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PRESET_TEST, GstPresetTest))
#define GST_PRESET_TEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PRESET_TEST, GstPresetTestClass))
#define GST_IS_PRESET_TEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PRESET_TEST))
#define GST_IS_PRESET_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PRESET_TEST))
#define GST_PRESET_TEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PRESET_TEST, GstPresetTestClass))
#define GST_PRESET_TEST_NAME            "preset-test"

enum
{
  PROP_TEST = 1,
};

typedef struct _GstPresetTest
{
  GstElement parent;

  gint test;
} GstPresetTest;

typedef struct _GstPresetTestClass
{
  GstElementClass parent_class;
} GstPresetTestClass;

static void
gst_preset_test_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstPresetTest *self = GST_PRESET_TEST (object);

  switch (property_id) {
    case PROP_TEST:
      g_value_set_int (value, self->test);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_preset_test_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPresetTest *self = GST_PRESET_TEST (object);

  switch (property_id) {
    case PROP_TEST:
      self->test = g_value_get_int (value);
      break;
  }
}

static void
gst_preset_test_class_init (GObjectClass * klass)
{
  klass->set_property = gst_preset_test_set_property;
  klass->get_property = gst_preset_test_get_property;

  g_object_class_install_property (klass, PROP_TEST,
      g_param_spec_int ("test",
          "test prop",
          "test parameter for preset test",
          G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
}

static void
gst_preset_test_base_init (GstPresetTestClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_metadata (element_class,
      "Element for unit tests",
      "Testing", "Use in unit tests", "Stefan Kost <stefan.kost@nokia.com>");
}

static GType
gst_preset_test_get_type (void)
{
  static gpointer preset_test_type = 0;

  if (g_once_init_enter_pointer (&preset_test_type)) {
    GType type;
    const GTypeInfo info = {
      sizeof (GstPresetTestClass),
      (GBaseInitFunc) gst_preset_test_base_init,        /* base_init */
      NULL,                     /* base_finalize */
      (GClassInitFunc) gst_preset_test_class_init,      /* class_init */
      NULL,                     /* class_finalize */
      NULL,                     /* class_data */
      sizeof (GstPresetTest),
      0,                        /* n_preallocs */
      NULL,                     /* instance_init */
      NULL                      /* value_table */
    };
    const GInterfaceInfo preset_interface_info = {
      NULL,                     /* interface_init */
      NULL,                     /* interface_finalize */
      NULL                      /* interface_data */
    };
    type = g_type_register_static (GST_TYPE_ELEMENT, "GstPresetTest", &info, 0);
    g_type_add_interface_static (type, GST_TYPE_PRESET, &preset_interface_info);
    g_once_init_leave_pointer (&preset_test_type, type);
  }
  return preset_test_type;
}

static gboolean
gst_preset_test_plugin_init (GstPlugin * plugin)
{
  gst_element_register (plugin, GST_PRESET_TEST_NAME, GST_RANK_NONE,
      GST_TYPE_PRESET_TEST);
  return TRUE;
}


GST_START_TEST (test_check)
{
  GstElement *elem;

  elem = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  fail_unless (GST_IS_PRESET (elem));

  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (test_load)
{
  GstElement *elem;
  gboolean res;

  elem = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  res = gst_preset_load_preset (GST_PRESET (elem), "does-not-exist");
  fail_unless (!res);

  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (test_add)
{
  GstElement *elem;
  gboolean res;
  gint val;

  elem = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  g_object_set (elem, "test", 5, NULL);

  res = gst_preset_save_preset (GST_PRESET (elem), "test");
  fail_unless (res);

  res = gst_preset_load_preset (GST_PRESET (elem), "test");
  fail_unless (res);
  g_object_get (elem, "test", &val, NULL);
  fail_unless (val == 5);

  gst_object_unref (elem);
}

GST_END_TEST;


GST_START_TEST (test_del)
{
  GstElement *elem;
  gboolean res;

  elem = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  res = gst_preset_save_preset (GST_PRESET (elem), "test");
  fail_unless (res);

  res = gst_preset_delete_preset (GST_PRESET (elem), "test");
  fail_unless (res);

  res = gst_preset_load_preset (GST_PRESET (elem), "test");
  fail_unless (!res);

  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (test_two_instances)
{
  GstElement *elem1, *elem2;
  gboolean res;
  gint val;

  elem1 = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  g_object_set (elem1, "test", 5, NULL);

  res = gst_preset_save_preset (GST_PRESET (elem1), "test");
  fail_unless (res);

  elem2 = gst_element_factory_make (GST_PRESET_TEST_NAME, NULL);
  res = gst_preset_load_preset (GST_PRESET (elem2), "test");
  fail_unless (res);
  g_object_get (elem2, "test", &val, NULL);
  fail_unless (val == 5);

  gst_object_unref (elem1);
  gst_object_unref (elem2);
}

GST_END_TEST;


static void
remove_preset_file (void)
{
  gchar *preset_file_name;

  preset_file_name = g_build_filename (g_get_user_data_dir (),
      "gstreamer-" GST_API_VERSION, "presets", "GstPresetTest.prs", NULL);
  g_unlink (preset_file_name);
  g_free (preset_file_name);
}

static void
test_setup (void)
{
  remove_preset_file ();
  gst_plugin_register_static (GST_VERSION_MAJOR,
      GST_VERSION_MINOR,
      "gst-test",
      "preset test plugin",
      gst_preset_test_plugin_init,
      VERSION, GST_LICENSE, PACKAGE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
}

static void
test_teardown (void)
{
  remove_preset_file ();
}


static Suite *
gst_preset_suite (void)
{
  Suite *s = suite_create ("GstPreset");
  TCase *tc = tcase_create ("preset");
  gchar *gst_dir;
  gboolean can_write = FALSE;

  /* check if we can create presets */
  gst_dir = g_build_filename (g_get_user_data_dir (),
      "gstreamer-" GST_API_VERSION, NULL);

#ifdef G_OS_WIN32
  can_write = FALSE;            /* FIXME: test properly using win32 API */
#else
  can_write = (g_access (gst_dir, R_OK | W_OK | X_OK) == 0);
#endif

  g_free (gst_dir);

  suite_add_tcase (s, tc);
  tcase_add_test (tc, test_check);
  tcase_add_test (tc, test_load);
  if (can_write) {
    tcase_add_test (tc, test_add);
    tcase_add_test (tc, test_del);
    tcase_add_test (tc, test_two_instances);
  }
  tcase_add_unchecked_fixture (tc, test_setup, test_teardown);

  return s;
}

GST_CHECK_MAIN (gst_preset);
