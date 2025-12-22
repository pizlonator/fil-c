/* GStreamer
 *
 * unit test for state changes on all elements
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
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

#include <unistd.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include <gst/check/gstcheck.h>

static GList *elements = NULL;

static void
setup (void)
{
  GList *features, *f;
  GList *plugins, *p;
  gchar **ignorelist = NULL;
  const gchar *STATE_IGNORE_ELEMENTS = NULL;

  GST_DEBUG ("getting elements for package %s", PACKAGE);
  STATE_IGNORE_ELEMENTS = g_getenv ("GST_STATE_IGNORE_ELEMENTS");
  if (!g_getenv ("GST_NO_STATE_IGNORE_ELEMENTS") && STATE_IGNORE_ELEMENTS) {
    GST_DEBUG ("Will ignore element factories: '%s'", STATE_IGNORE_ELEMENTS);
    ignorelist = g_strsplit (STATE_IGNORE_ELEMENTS, " ", 0);
  }

  plugins = gst_registry_get_plugin_list (gst_registry_get ());

  for (p = plugins; p; p = p->next) {
    GstPlugin *plugin = p->data;

    if (strcmp (gst_plugin_get_source (plugin), PACKAGE) != 0)
      continue;

    features =
        gst_registry_get_feature_list_by_plugin (gst_registry_get (),
        gst_plugin_get_name (plugin));

    for (f = features; f; f = f->next) {
      GstPluginFeature *feature = f->data;
      const gchar *name = gst_plugin_feature_get_name (feature);
      gboolean ignore = FALSE;

      if (!GST_IS_ELEMENT_FACTORY (feature))
        continue;

      if (ignorelist) {
        gchar **s;

        for (s = ignorelist; s && *s; ++s) {
          if (g_str_has_prefix (name, *s)) {
            GST_DEBUG ("ignoring element %s", name);
            ignore = TRUE;
          }
        }
        if (ignore)
          continue;
      }

      GST_DEBUG ("adding element %s", name);
      elements = g_list_prepend (elements, (gpointer) g_strdup (name));
    }
    gst_plugin_feature_list_free (features);
  }
  gst_plugin_list_free (plugins);
  g_strfreev (ignorelist);
}

static void
teardown (void)
{
  GList *e;

  for (e = elements; e; e = e->next) {
    g_free (e->data);
  }
  g_list_free (elements);
  elements = NULL;
}


GST_START_TEST (test_state_changes_up_and_down_seq)
{
  GstElement *element;
  GstStateChangeReturn sret;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_DEBUG ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    sret = gst_element_set_state (element, GST_STATE_READY);
    if (sret != GST_STATE_CHANGE_FAILURE) {
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_PLAYING);
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);
      gst_element_set_state (element, GST_STATE_NULL);

      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);
      gst_element_set_state (element, GST_STATE_PLAYING);
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_NULL);
    }
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;

GST_START_TEST (test_state_changes_up_seq)
{
  GstElement *element;
  GstStateChangeReturn sret;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_DEBUG ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    sret = gst_element_set_state (element, GST_STATE_READY);
    if (sret != GST_STATE_CHANGE_FAILURE) {
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);

      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_PLAYING);
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);

      gst_element_set_state (element, GST_STATE_NULL);
    }
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;

GST_START_TEST (test_state_changes_down_seq)
{
  GstElement *element;
  GstStateChangeReturn sret;
  GList *e;

  for (e = elements; e; e = e->next) {
    const gchar *name = e->data;

    GST_DEBUG ("testing element %s", name);
    element = gst_element_factory_make (name, name);
    fail_if (element == NULL, "Could not make element from factory %s", name);

    if (GST_IS_PIPELINE (element)) {
      GST_DEBUG ("element %s is a pipeline", name);
    }

    sret = gst_element_set_state (element, GST_STATE_READY);
    if (sret != GST_STATE_CHANGE_FAILURE) {
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_PLAYING);

      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_PLAYING);

      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);
      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_PLAYING);

      gst_element_set_state (element, GST_STATE_PAUSED);
      gst_element_set_state (element, GST_STATE_READY);
      gst_element_set_state (element, GST_STATE_NULL);
    }
    gst_object_unref (GST_OBJECT (element));
  }
}

GST_END_TEST;


static Suite *
states_suite (void)
{
  Suite *s = suite_create ("states_bad");
  TCase *tc_chain = tcase_create ("general");

#if defined(HAVE_LADSPA) || defined(HAVE_LV2)
  /* timeout after 60s, not the default 3
   * we have wrapper plugins enabled
   */
  tcase_set_timeout (tc_chain, 60);
#endif

  suite_add_tcase (s, tc_chain);
  tcase_add_checked_fixture (tc_chain, setup, teardown);
  tcase_add_test (tc_chain, test_state_changes_up_and_down_seq);
  tcase_add_test (tc_chain, test_state_changes_up_seq);
  tcase_add_test (tc_chain, test_state_changes_down_seq);

  return s;
}

int
main (int argc, char **argv)
{
  Suite *s;
  GModule *libx11;

  libx11 =
      g_module_open ("libX11.so.6", G_MODULE_BIND_LOCAL | G_MODULE_BIND_LAZY);
  if (libx11) {
    void (*xinitthreads) (void);
    if (g_module_symbol (libx11, "XInitThreads", (gpointer *) & xinitthreads)) {
      xinitthreads ();
    }
    g_module_close (libx11);
  }
  gst_check_init (&argc, &argv);
  s = states_suite ();
  return gst_check_run_suite (s, "states_bad", __FILE__);
}
