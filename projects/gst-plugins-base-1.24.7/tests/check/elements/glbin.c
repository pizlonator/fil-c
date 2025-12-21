/* GStreamer
 *
 * Unit tests for glimagesink
 *
 * Copyright (C) 2014 Julien Isorce <j.isorce@samsung.com>
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>

typedef void (*ElementOperation) (GstElement * e, gpointer user_data);
typedef GstElement *(*CreateElement) (GstElement * src, gpointer unused);

#define CREATE_ELEMENT(e,c,d) \
    g_signal_connect (e, "create-element", G_CALLBACK (c), d)

static GstElement *
_create_element_floating_cb (GstElement * src, const gchar * name)
{
  return gst_element_factory_make (name, NULL);
}

static GstElement *
_create_element_full_cb (GstElement * src, const gchar * name)
{
  return gst_object_ref_sink (gst_element_factory_make (name, NULL));
}

struct src_data
{
  const gchar *prop;
  const gchar *element_name;
};

static void
_set_element_floating (GstElement * e, struct src_data *d /* static */ )
{
  g_object_set (e, d->prop, _create_element_floating_cb (e, d->element_name),
      NULL);
}

static void
_set_element_full (GstElement * e, struct src_data *d /* static */ )
{
  GstElement *element = _create_element_full_cb (e, d->element_name);
  g_object_set (e, d->prop, element, NULL);
  gst_object_unref (element);
}

static void
_set_element_floating_floating (GstElement * e,
    struct src_data *d /* static */ )
{
  _set_element_floating (e, d);
  _set_element_floating (e, d);
}

static void
_set_element_floating_full (GstElement * e, struct src_data *d /* static */ )
{
  _set_element_floating (e, d);
  _set_element_full (e, d);
}

static void
_set_element_full_full (GstElement * e, struct src_data *d /* static */ )
{
  _set_element_full (e, d);
  _set_element_full (e, d);
}

static void
_set_element_full_floating (GstElement * e, struct src_data *d /* static */ )
{
  _set_element_full (e, d);
  _set_element_floating (e, d);
}

static void
_create_element_floating (GstElement * e, const gchar * name /* static */ )
{
  CREATE_ELEMENT (e, _create_element_floating_cb, (gchar *) name);
}

static void
_create_element_full (GstElement * e, const gchar * name /* static */ )
{
  CREATE_ELEMENT (e, _create_element_full_cb, (gchar *) name);
}

static void
_test_glsrcbin (ElementOperation op, gpointer user_data)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("glsrcbin", NULL);
  GstElement *sink = gst_element_factory_make ("glimagesink", NULL);

  gst_bin_add_many (GST_BIN (pipe), src, sink, NULL);
  gst_element_link (src, sink);

  op (src, user_data);

  gst_element_set_state (pipe, GST_STATE_READY);
  gst_element_set_state (pipe, GST_STATE_NULL);

  gst_object_unref (pipe);
}

GST_START_TEST (test_glsrcbin_set_element_floating)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_set_element_full)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_set_element_floating_floating)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_floating_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_set_element_floating_full)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_floating_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_set_element_full_floating)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_full_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_set_element_full_full)
{
  struct src_data d = { "src", "gltestsrc" };
  _test_glsrcbin ((ElementOperation) _set_element_full_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_create_element_floating)
{
  _test_glsrcbin ((ElementOperation) _create_element_floating,
      (gchar *) "gltestsrc");
}

GST_END_TEST;

GST_START_TEST (test_glsrcbin_create_element_full)
{
  _test_glsrcbin ((ElementOperation) _create_element_full,
      (gchar *) "gltestsrc");
}

GST_END_TEST;

static void
_test_glsinkbin (ElementOperation op, gpointer user_data)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("gltestsrc", NULL);
  GstElement *sink = gst_element_factory_make ("glsinkbin", NULL);

  gst_bin_add_many (GST_BIN (pipe), src, sink, NULL);
  gst_element_link (src, sink);

  op (sink, user_data);

  gst_element_set_state (pipe, GST_STATE_READY);
  gst_element_set_state (pipe, GST_STATE_NULL);

  gst_object_unref (pipe);
}

GST_START_TEST (test_glsinkbin_set_element_floating)
{
  struct src_data d = { "sink", "glimagesinkelement" };

  _test_glsinkbin ((ElementOperation) _set_element_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_set_element_full)
{
  struct src_data d = { "sink", "glimagesinkelement" };

  _test_glsinkbin ((ElementOperation) _set_element_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_create_element_floating)
{
  _test_glsinkbin ((ElementOperation) _create_element_floating,
      (gchar *) "glimagesinkelement");
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_create_element_full)
{
  _test_glsinkbin ((ElementOperation) _create_element_full,
      (gchar *) "glimagesinkelement");
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_set_element_floating_floating)
{
  struct src_data d = { "sink", "glimagesinkelement" };
  _test_glsinkbin ((ElementOperation) _set_element_floating_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_set_element_floating_full)
{
  struct src_data d = { "sink", "glimagesinkelement" };
  _test_glsinkbin ((ElementOperation) _set_element_floating_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_set_element_full_floating)
{
  struct src_data d = { "sink", "glimagesinkelement" };
  _test_glsinkbin ((ElementOperation) _set_element_full_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glsinkbin_set_element_full_full)
{
  struct src_data d = { "sink", "glimagesinkelement" };
  _test_glsinkbin ((ElementOperation) _set_element_full_full, &d);
}

GST_END_TEST;

static void
_test_glfilterbin (ElementOperation op, gpointer user_data)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("gltestsrc", NULL);
  GstElement *filter = gst_element_factory_make ("glfilterbin", NULL);
  GstElement *sink = gst_element_factory_make ("glimagesinkelement", NULL);

  gst_bin_add_many (GST_BIN (pipe), src, filter, sink, NULL);
  gst_element_link_many (src, filter, sink, NULL);

  op (filter, user_data);

  gst_element_set_state (pipe, GST_STATE_READY);
  gst_element_set_state (pipe, GST_STATE_NULL);

  gst_object_unref (pipe);
}

GST_START_TEST (test_glfilterbin_set_element_floating)
{
  struct src_data d = { "filter", "gleffects_identity" };

  _test_glfilterbin ((ElementOperation) _set_element_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_set_element_full)
{
  struct src_data d = { "filter", "gleffects_identity" };

  _test_glfilterbin ((ElementOperation) _set_element_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_create_element_floating)
{
  _test_glfilterbin ((ElementOperation) _create_element_floating,
      (gchar *) "gleffects_identity");
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_create_element_full)
{
  _test_glfilterbin ((ElementOperation) _create_element_full,
      (gchar *) "gleffects_identity");
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_set_element_floating_floating)
{
  struct src_data d = { "filter", "gleffects_identity" };
  _test_glfilterbin ((ElementOperation) _set_element_floating_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_set_element_floating_full)
{
  struct src_data d = { "filter", "gleffects_identity" };
  _test_glfilterbin ((ElementOperation) _set_element_floating_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_set_element_full_floating)
{
  struct src_data d = { "filter", "gleffects_identity" };
  _test_glfilterbin ((ElementOperation) _set_element_full_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glfilterbin_set_element_full_full)
{
  struct src_data d = { "filter", "gleffects_identity" };
  _test_glfilterbin ((ElementOperation) _set_element_full_full, &d);
}

GST_END_TEST;

#if 0
/* FIXME: add when gl mixers are added to base */
static void
_test_glmixerbin (ElementOperation op, gpointer user_data)
{
  GstElement *pipe = gst_pipeline_new (NULL);
  GstElement *src = gst_element_factory_make ("gltestsrc", NULL);
  GstElement *mixer = gst_element_factory_make ("glmixerbin", NULL);
  GstElement *sink = gst_element_factory_make ("glimagesinkelement", NULL);

  gst_bin_add_many (GST_BIN (pipe), src, mixer, sink, NULL);
  gst_element_link_many (src, mixer, sink, NULL);

  op (mixer, user_data);

  gst_element_set_state (pipe, GST_STATE_READY);
  gst_element_set_state (pipe, GST_STATE_NULL);

  gst_object_unref (pipe);
}

GST_START_TEST (test_glmixerbin_set_element_floating)
{
  struct src_data d = { "mixer", "glvideomixerelement" };

  _test_glmixerbin ((ElementOperation) _set_element_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_set_element_full)
{
  struct src_data d = { "mixer", "glvideomixerelement" };

  _test_glmixerbin ((ElementOperation) _set_element_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_create_element_floating)
{
  _test_glmixerbin ((ElementOperation) _create_element_floating,
      (gchar *) "glvideomixerelement");
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_create_element_full)
{
  _test_glmixerbin ((ElementOperation) _create_element_full,
      (gchar *) "glvideomixerelement");
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_set_element_floating_floating)
{
  struct src_data d = { "mixer", "glvideomixerelement" };
  _test_glmixerbin ((ElementOperation) _set_element_floating_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_set_element_floating_full)
{
  struct src_data d = { "mixer", "glvideomixerelement" };
  _test_glmixerbin ((ElementOperation) _set_element_floating_full, &d);
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_set_element_full_floating)
{
  struct src_data d = { "mixer", "glvideomixerelement" };
  _test_glmixerbin ((ElementOperation) _set_element_full_floating, &d);
}

GST_END_TEST;

GST_START_TEST (test_glmixerbin_set_element_full_full)
{
  struct src_data d = { "mixer", "glvideomixerelement" };
  _test_glmixerbin ((ElementOperation) _set_element_full_full, &d);
}

GST_END_TEST;
#endif
static Suite *
glbin_suite (void)
{
  Suite *s = suite_create ("glbin");
  TCase *tc;

  tc = tcase_create ("glsrcbin");
  tcase_add_test (tc, test_glsrcbin_create_element_floating);
  tcase_add_test (tc, test_glsrcbin_create_element_full);
  tcase_add_test (tc, test_glsrcbin_set_element_floating);
  tcase_add_test (tc, test_glsrcbin_set_element_full);
  tcase_add_test (tc, test_glsrcbin_set_element_floating_floating);
  tcase_add_test (tc, test_glsrcbin_set_element_full_floating);
  tcase_add_test (tc, test_glsrcbin_set_element_floating_full);
  tcase_add_test (tc, test_glsrcbin_set_element_full_full);
  suite_add_tcase (s, tc);

  tc = tcase_create ("glsinkbin");
  tcase_add_test (tc, test_glsinkbin_create_element_floating);
  tcase_add_test (tc, test_glsinkbin_create_element_full);
  tcase_add_test (tc, test_glsinkbin_set_element_floating);
  tcase_add_test (tc, test_glsinkbin_set_element_full);
  tcase_add_test (tc, test_glsinkbin_set_element_floating_floating);
  tcase_add_test (tc, test_glsinkbin_set_element_full_floating);
  tcase_add_test (tc, test_glsinkbin_set_element_floating_full);
  tcase_add_test (tc, test_glsinkbin_set_element_full_full);
  suite_add_tcase (s, tc);

  tc = tcase_create ("glfilterbin");
  tcase_add_test (tc, test_glfilterbin_create_element_floating);
  tcase_add_test (tc, test_glfilterbin_create_element_full);
  tcase_add_test (tc, test_glfilterbin_set_element_floating);
  tcase_add_test (tc, test_glfilterbin_set_element_full);
  tcase_add_test (tc, test_glfilterbin_set_element_floating_floating);
  tcase_add_test (tc, test_glfilterbin_set_element_full_floating);
  tcase_add_test (tc, test_glfilterbin_set_element_floating_full);
  tcase_add_test (tc, test_glfilterbin_set_element_full_full);
  suite_add_tcase (s, tc);

#if 0
  tc = tcase_create ("glmixerbin");
  tcase_add_test (tc, test_glmixerbin_create_element_floating);
  tcase_add_test (tc, test_glmixerbin_create_element_full);
  tcase_add_test (tc, test_glmixerbin_set_element_floating);
  tcase_add_test (tc, test_glmixerbin_set_element_full);
  tcase_add_test (tc, test_glmixerbin_set_element_floating_floating);
  tcase_add_test (tc, test_glmixerbin_set_element_full_floating);
  tcase_add_test (tc, test_glmixerbin_set_element_floating_full);
  tcase_add_test (tc, test_glmixerbin_set_element_full_full);
  suite_add_tcase (s, tc);
#endif
  return s;
}

GST_CHECK_MAIN (glbin)
