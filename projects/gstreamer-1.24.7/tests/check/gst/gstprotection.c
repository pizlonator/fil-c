/* GStreamer
 *
 * Unit tests for protection library.
 *
 * Copyright (C) <2015> YouView TV Ltd.
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

#include <gst/check/gstcheck.h>
#include <gst/gstprotection.h>

#ifndef GST_PACKAGE_NAME
#define GST_PACKAGE_NAME "gstreamer"
#endif

#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://developer.gnome.org/gstreamer/"
#endif

static GType gst_protection_test_get_type (void);

#define GST_TYPE_PROTECTION_TEST            (gst_protection_test_get_type ())
#define GST_PROTECTION_TEST(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PROTECTION_TEST, GstProtectionTest))
#define GST_PROTECTION_TEST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PROTECTION_TEST, GstProtectionTestClass))
#define GST_IS_PROTECTION_TEST(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PROTECTION_TEST))
#define GST_IS_PROTECTION_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PROTECTION_TEST))
#define GST_PROTECTION_TEST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PROTECTION_TEST, GstProtectionTestClass))
#define GST_PROTECTION_TEST_NAME            "protection-test"

#define CLEARKEY_SYSTEM_ID "78f32170-d883-11e0-9572-0800200c9a66"

typedef struct _GstProtectionTest
{
  GstElement parent;

  gint test;
} GstProtectionTest;

typedef struct _GstProtectionTestClass
{
  GstElementClass parent_class;
} GstProtectionTestClass;

typedef struct _PluginInitContext
{
  const gchar *name;
  guint rank;
  GType type;
} PluginInitContext;

static GstStaticPadTemplate gst_decrypt_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("application/x-cenc, original-media-type=(string)video/x-h264, "
        GST_PROTECTION_SYSTEM_ID_CAPS_FIELD "=(string)" CLEARKEY_SYSTEM_ID)
    );

static void
gst_protection_test_class_init (GObjectClass * klass)
{
}

static void
gst_protection_test_base_init (GstProtectionTestClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_static_pad_template (element_class,
      &gst_decrypt_sink_template);

  gst_element_class_set_metadata (element_class,
      "Decryptor element for unit tests",
      GST_ELEMENT_FACTORY_KLASS_DECRYPTOR,
      "Use in unit tests", "Alex Ashley <alex.ashley@youview.com>");
}

static GType
gst_protection_test_get_type (void)
{
  static gpointer protection_test_type = 0;

  if (g_once_init_enter_pointer (&protection_test_type)) {
    GType type;
    const GTypeInfo info = {
      sizeof (GstProtectionTestClass),
      (GBaseInitFunc) gst_protection_test_base_init,    /* base_init */
      NULL,                     /* base_finalize */
      (GClassInitFunc) gst_protection_test_class_init,  /* class_init */
      NULL,                     /* class_finalize */
      NULL,                     /* class_data */
      sizeof (GstProtectionTest),
      0,                        /* n_preallocs */
      NULL,                     /* instance_init */
      NULL                      /* value_table */
    };
    type =
        g_type_register_static (GST_TYPE_ELEMENT, "GstProtectionTest", &info,
        0);
    g_once_init_leave_pointer (&protection_test_type, type);
  }
  return protection_test_type;
}

static gboolean
protection_plugin_init_func (GstPlugin * plugin, gpointer user_data)
{
  PluginInitContext *context = (PluginInitContext *) user_data;
  gboolean ret;

  ret =
      gst_element_register (plugin, context->name, context->rank,
      context->type);
  return ret;
}

static gboolean
protection_create_plugin (GstRegistry * registry, const gchar * name,
    GType type)
{
  gboolean ret;
  PluginInitContext context;

  context.name = name;
  context.rank = GST_RANK_MARGINAL;
  context.type = type;
  ret = gst_plugin_register_static_full (GST_VERSION_MAJOR,     /* version */
      GST_VERSION_MINOR,        /* version */
      name,                     /* name */
      "Protection unit test",   /* description */
      protection_plugin_init_func,      /* init function */
      "0.0.0",                  /* version string */
      GST_LICENSE_UNKNOWN,      /* license */
      __FILE__,                 /* source */
      GST_PACKAGE_NAME,         /* package */
      GST_PACKAGE_ORIGIN,       /* origin */
      &context                  /* user_data */
      );
  return ret;
}

static void
test_setup (void)
{
  GstRegistry *registry;

  registry = gst_registry_get ();
  protection_create_plugin (registry, GST_PROTECTION_TEST_NAME,
      GST_TYPE_PROTECTION_TEST);
}

static void
test_teardown (void)
{
}


GST_START_TEST (test_decryptor_element_class)
{
  GstElement *elem;
  const gchar *selected_id;
  const gchar *sys_ids[] = {
    CLEARKEY_SYSTEM_ID,
    "69f908af-4816-46ea-910c-cd5dcccb0a3a",
    "5e629af5-38da-4063-8977-97ffbd9902d4",
    NULL
  };

#ifdef DEBUG_PLUGINS
  GList *list, *walk;

  list = gst_registry_get_plugin_list (gst_registry_get ());
  for (walk = list; walk; walk = g_list_next (walk)) {
    GstPlugin *plugin = (GstPlugin *) walk->data;
    g_print ("Element %s\n", gst_plugin_get_name (plugin));
  }
#endif

  elem = gst_element_factory_make (GST_PROTECTION_TEST_NAME, NULL);
  fail_unless (GST_IS_ELEMENT (elem));

  selected_id = gst_protection_select_system (sys_ids);
  fail_if (selected_id == NULL);

  selected_id = gst_protection_select_system (&sys_ids[1]);
  fail_unless (selected_id == NULL);

  selected_id = gst_protection_select_system (&sys_ids[3]);
  fail_unless (selected_id == NULL);

  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (test_protection_metadata)
{
  GstBuffer *buf = NULL;
  GstBuffer *iv, *kid;
  GstBuffer *fetched_iv = NULL, *fetched_key_id = NULL;
  GstStructure *meta_info;
  GstProtectionMeta *meta = NULL;
  const GstMetaInfo *info = NULL;
  const GValue *value;

  /* Check correct type info is returned */
  info = gst_protection_meta_get_info ();
  fail_unless (info != NULL);
  fail_unless (info->api == GST_PROTECTION_META_API_TYPE);

  iv = gst_buffer_new_allocate (NULL, 16, NULL);
  gst_buffer_memset (iv, 0, 'i', 16);
  ASSERT_MINI_OBJECT_REFCOUNT (iv, "iv", 1);
  kid = gst_buffer_new_allocate (NULL, 16, NULL);
  gst_buffer_memset (kid, 0, 'k', 16);
  ASSERT_MINI_OBJECT_REFCOUNT (kid, "kid", 1);
  meta_info = gst_structure_new ("application/x-cenc",
      "encrypted", G_TYPE_BOOLEAN, TRUE,
      "iv", GST_TYPE_BUFFER, iv,
      "iv_size", G_TYPE_UINT, 16, "kid", GST_TYPE_BUFFER, kid, NULL);
  ASSERT_MINI_OBJECT_REFCOUNT (kid, "kid", 2);
  ASSERT_MINI_OBJECT_REFCOUNT (iv, "iv", 2);

  buf = gst_buffer_new_allocate (NULL, 1024, NULL);
  /* Test attaching protection metadata to buffer */
  meta = gst_buffer_add_protection_meta (buf, meta_info);
  fail_unless (meta != NULL);
  /* gst_buffer_new_allocate takes ownership of info GstStructure */
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "Buffer", 1);

  /* Test detaching protection metadata from buffer, and check that
   * contained data is correct */
  meta = NULL;
  meta = gst_buffer_get_protection_meta (buf);
  fail_unless (meta != NULL);
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "Buffer", 1);
  value = gst_structure_get_value (meta->info, "iv");
  fail_unless (value != NULL);
  fetched_iv = gst_value_get_buffer (value);
  fail_unless (fetched_iv != NULL);
  fail_unless (gst_buffer_get_size (fetched_iv) == 16);
  value = gst_structure_get_value (meta->info, "kid");
  fail_unless (value != NULL);
  fetched_key_id = gst_value_get_buffer (value);
  fail_unless (fetched_key_id != NULL);
  fail_unless (gst_buffer_get_size (fetched_key_id) == 16);

  gst_buffer_remove_meta (buf, (GstMeta *) meta);

  /* Check that refcounts are decremented after metadata is freed */
  ASSERT_MINI_OBJECT_REFCOUNT (buf, "Buffer", 1);
  ASSERT_MINI_OBJECT_REFCOUNT (iv, "IV", 1);
  ASSERT_MINI_OBJECT_REFCOUNT (kid, "KID", 1);

  gst_buffer_unref (buf);
  gst_buffer_unref (iv);
  gst_buffer_unref (kid);
}

GST_END_TEST;

static Suite *
protection_suite (void)
{
  Suite *s = suite_create ("protection library");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_decryptor_element_class);
  tcase_add_test (tc_chain, test_protection_metadata);
  tcase_add_unchecked_fixture (tc_chain, test_setup, test_teardown);

  return s;
}

GST_CHECK_MAIN (protection);
