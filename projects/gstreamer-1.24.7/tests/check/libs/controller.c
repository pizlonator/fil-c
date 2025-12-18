/* GStreamer
 *
 * unit test for the controller library
 *
 * Copyright (C) <2005> Stefan Kost <ensonic at users dot sf dot net>
 * Copyright (C) <2006-2007> Sebastian Dröge <slomo@circular-chaos.org>
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
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstlfocontrolsource.h>
#include <gst/controller/gsttriggercontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>
#include <gst/controller/gstproxycontrolbinding.h>

/* enum for text element */

#define GST_TYPE_TEST_ENUM (gst_test_enum_get_type ())

typedef enum
{
  ENUM_V0 = 0,
  ENUM_V10 = 10,
  ENUM_V11,
  ENUM_V12,
  ENUM_V255 = 255
} GstTestEnum;

static GType
gst_test_enum_get_type (void)
{
  static gpointer gtype = 0;
  static const GEnumValue values[] = {
    {ENUM_V0, "ENUM_V0", "0"},
    {ENUM_V10, "ENUM_V10", "10"},
    {ENUM_V11, "ENUM_V11", "11"},
    {ENUM_V12, "ENUM_V12", "12"},
    {ENUM_V255, "ENUM_V255", "255"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter_pointer (&gtype)) {
    GType tmp = g_enum_register_static ("GstTestEnum", values);
    g_once_init_leave_pointer (&gtype, tmp);
  }

  return (GType) gtype;
}

/* local test element */

enum
{
  PROP_INT = 1,
  PROP_FLOAT,
  PROP_DOUBLE,
  PROP_BOOLEAN,
  PROP_ENUM,
  PROP_READONLY,
  PROP_STATIC,
  PROP_CONSTRUCTONLY,
  PROP_COUNT
};

#define GST_TYPE_TEST_OBJ            (gst_test_obj_get_type ())
#define GST_TEST_OBJ(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_TEST_OBJ, GstTestObj))
#define GST_TEST_OBJ_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_TEST_OBJ, GstTestObjClass))
#define GST_IS_TEST_OBJ(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_TEST_OBJ))
#define GST_IS_TEST_OBJ_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_TEST_OBJ))
#define GST_TEST_OBJ_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_TEST_OBJ, GstTestObjClass))

typedef struct _GstTestObj GstTestObj;
typedef struct _GstTestObjClass GstTestObjClass;

struct _GstTestObj
{
  GstElement parent;
  gint val_int;
  gfloat val_float;
  gdouble val_double;
  gboolean val_boolean;
  GstTestEnum val_enum;
};
struct _GstTestObjClass
{
  GstElementClass parent_class;
};

static GType gst_test_obj_get_type (void);

static void
gst_test_obj_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec)
{
  GstTestObj *self = GST_TEST_OBJ (object);

  switch (property_id) {
    case PROP_INT:
      g_value_set_int (value, self->val_int);
      break;
    case PROP_FLOAT:
      g_value_set_float (value, self->val_float);
      break;
    case PROP_DOUBLE:
      g_value_set_double (value, self->val_double);
      break;
    case PROP_BOOLEAN:
      g_value_set_boolean (value, self->val_boolean);
      break;
    case PROP_ENUM:
      g_value_set_enum (value, self->val_enum);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_test_obj_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec)
{
  GstTestObj *self = GST_TEST_OBJ (object);

  switch (property_id) {
    case PROP_INT:
      self->val_int = g_value_get_int (value);
      GST_DEBUG ("test value int=%d", self->val_int);
      break;
    case PROP_FLOAT:
      self->val_float = g_value_get_float (value);
      GST_DEBUG ("test value float=%f", self->val_float);
      break;
    case PROP_DOUBLE:
      self->val_double = g_value_get_double (value);
      GST_DEBUG ("test value double=%lf", self->val_double);
      break;
    case PROP_BOOLEAN:
      self->val_boolean = g_value_get_boolean (value);
      GST_DEBUG ("test value boolean=%d", self->val_boolean);
      break;
    case PROP_ENUM:
      self->val_enum = g_value_get_enum (value);
      GST_DEBUG ("test value enum=%d", self->val_enum);
      break;
    case PROP_CONSTRUCTONLY:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_test_obj_class_init (GstTestObjClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_test_obj_set_property;
  gobject_class->get_property = gst_test_obj_get_property;

  g_object_class_install_property (gobject_class, PROP_INT,
      g_param_spec_int ("int",
          "int prop",
          "int number parameter",
          0, 100, 0, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_FLOAT,
      g_param_spec_float ("float",
          "float prop",
          "float number parameter",
          0.0, 100.0, 0.0, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_DOUBLE,
      g_param_spec_double ("double",
          "double prop",
          "double number parameter",
          0.0, 100.0, 0.0, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_BOOLEAN,
      g_param_spec_boolean ("boolean",
          "boolean prop",
          "boolean parameter",
          FALSE, G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_ENUM,
      g_param_spec_enum ("enum",
          "enum prop",
          "enum parameter",
          GST_TYPE_TEST_ENUM, ENUM_V0,
          G_PARAM_READWRITE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_READONLY,
      g_param_spec_int ("readonly",
          "readonly prop",
          "readonly parameter",
          0, G_MAXINT, 0, G_PARAM_READABLE | GST_PARAM_CONTROLLABLE));

  g_object_class_install_property (gobject_class, PROP_STATIC,
      g_param_spec_int ("static",
          "static prop",
          "static parameter", 0, G_MAXINT, 0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CONSTRUCTONLY,
      g_param_spec_int ("construct-only",
          "construct-only prop",
          "construct-only parameter",
          0, G_MAXINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
gst_test_obj_base_init (GstTestObjClass * klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_metadata (element_class,
      "test object for unit tests",
      "Test", "Use in unit tests", "Stefan Sauer <ensonic@users.sf.net>");
}

static GType
gst_test_obj_get_type (void)
{
  static gpointer test_obj_type = 0;

  if (g_once_init_enter_pointer (&test_obj_type)) {
    GType type;
    static const GTypeInfo info = {
      (guint16) sizeof (GstTestObjClass),
      (GBaseInitFunc) gst_test_obj_base_init,   // base_init
      NULL,                     // base_finalize
      (GClassInitFunc) gst_test_obj_class_init, // class_init
      NULL,                     // class_finalize
      NULL,                     // class_data
      (guint16) sizeof (GstTestObj),
      0,                        // n_preallocs
      NULL,                     // instance_init
      NULL                      // value_table
    };
    type = g_type_register_static (GST_TYPE_ELEMENT, "GstTestObj", &info, 0);
    g_once_init_leave_pointer (&test_obj_type, type);
  }
  return test_obj_type;
}

static void
setup (void)
{
  gst_element_register (NULL, "testobj", GST_RANK_NONE, GST_TYPE_TEST_OBJ);
}

static void
teardown (void)
{
}


/* TESTS */

/* tests if we don't fail on empty interpolation controlsources */
GST_START_TEST (controller_controlsource_empty1)
{
  GstElement *elem;
  GstControlSource *cs;

  elem = gst_element_factory_make ("testobj", NULL);

  cs = gst_interpolation_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* don't fail on empty control point lists */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);

  /* unref objects */
  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* tests if we don't fail on interpolation controlsources that are empty again */
GST_START_TEST (controller_controlsource_empty2)
{
  GstElement *elem;
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;

  elem = gst_element_factory_make ("testobj", NULL);

  cs = gst_interpolation_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set control values */
  tvcs = (GstTimedValueControlSource *) cs;
  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0);

  /* ... and unset the value */
  gst_timed_value_control_source_unset (tvcs, 0 * GST_SECOND);

  /* don't fail on empty control point lists */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);

  /* unref objects */
  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value handling without interpolation */
GST_START_TEST (controller_interpolation_none)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;
  gdouble v;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));

  /* check values on control source directly */
  fail_unless (gst_control_source_get_value (cs, 0 * GST_SECOND, &v));
  fail_unless_equals_float (v, 0.0);
  fail_unless (gst_control_source_get_value (cs, 1 * GST_SECOND, &v));
  fail_unless_equals_float (v, 0.0);
  fail_unless (gst_control_source_get_value (cs, 2 * GST_SECOND, &v));
  fail_unless_equals_float (v, 1.0);
  fail_unless (gst_control_source_get_value (cs, 3 * GST_SECOND, &v));
  fail_unless_equals_float (v, 1.0);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value handling with linear interpolation */
GST_START_TEST (controller_interpolation_linear)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value handling with cubic interpolation */
GST_START_TEST (controller_interpolation_cubic)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "double", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 0.5));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 0.2));
  fail_unless (gst_timed_value_control_source_set (tvcs, 4 * GST_SECOND, 0.8));

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 50.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless (GST_TEST_OBJ (elem)->val_double > 20.0 &&
      GST_TEST_OBJ (elem)->val_double < 80.0, NULL);
  gst_object_sync_values (GST_OBJECT (elem), 4 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 80.0);
  gst_object_sync_values (GST_OBJECT (elem), 5 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 80.0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value handling with cubic interpolation */
GST_START_TEST (controller_interpolation_cubic_too_few_cp)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "double", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);

  /* set 2 control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 0.4));

  /* now pull in values for some timestamps and verify that it used linear
   * interpolation as we don't gave enough control points
   */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 40.0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test _unset() */
GST_START_TEST (controller_interpolation_unset)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 1.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 0.5));

  /* verify values */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);

  /* unset second */
  fail_unless (gst_timed_value_control_source_unset (tvcs, 1 * GST_SECOND));

  /* verify value again */
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);

  /* unset all values, reset and try to unset again */
  fail_unless (gst_timed_value_control_source_unset (tvcs, 0 * GST_SECOND));
  fail_unless (gst_timed_value_control_source_unset (tvcs, 2 * GST_SECOND));
  gst_timed_value_control_source_unset_all (tvcs);
  fail_if (gst_timed_value_control_source_unset (tvcs, 2 * GST_SECOND));

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test _unset_all() */
GST_START_TEST (controller_interpolation_unset_all)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 1.0));

  /* verify values */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  /* unset all */
  gst_timed_value_control_source_unset_all (tvcs);
  GST_TEST_OBJ (elem)->val_int = 0;

  /* verify value again */
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test retrieval of an array of values with get_value_array() */
GST_START_TEST (controller_interpolation_linear_absolute_value_array)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;
  gdouble *raw_values;
  GValue *g_values;
  gint *values;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new_absolute (GST_OBJECT (elem), "int",
              cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 100));

  /* now pull in raw-values for some timestamps */
  raw_values = g_new (gdouble, 3);

  fail_unless (gst_control_source_get_value_array (cs,
          0, GST_SECOND / 2, 3, raw_values));
  fail_unless_equals_float ((raw_values)[0], 0);
  fail_unless_equals_float ((raw_values)[1], 50);
  fail_unless_equals_float ((raw_values)[2], 100);

  g_free (raw_values);

  /* now pull in mapped GValues for some timestamps */
  g_values = g_new0 (GValue, 3);

  fail_unless (gst_object_get_g_value_array (GST_OBJECT (elem), "int",
          0, GST_SECOND / 2, 3, g_values));
  fail_unless_equals_int (g_value_get_int (&g_values[0]), 0);
  fail_unless_equals_int (g_value_get_int (&g_values[1]), 50);
  fail_unless_equals_int (g_value_get_int (&g_values[2]), 100);

  g_free (g_values);

  /* now pull in mapped values for some timestamps */
  values = g_new0 (gint, 3);

  fail_unless (gst_object_get_value_array (GST_OBJECT (elem), "int",
          0, GST_SECOND / 2, 3, values));
  fail_unless_equals_int (values[0], 0);
  fail_unless_equals_int (values[1], 50);
  fail_unless_equals_int (values[2], 100);

  g_free (values);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test retrieval of an array of values with get_value_array() */
GST_START_TEST (controller_interpolation_linear_value_array)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;
  gdouble *raw_values;
  GValue *g_values;
  gint *values;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 1.0));

  /* now pull in raw-values for some timestamps */
  raw_values = g_new (gdouble, 3);

  fail_unless (gst_control_source_get_value_array (cs,
          0, GST_SECOND / 2, 3, raw_values));
  fail_unless_equals_float ((raw_values)[0], 0.0);
  fail_unless_equals_float ((raw_values)[1], 0.5);
  fail_unless_equals_float ((raw_values)[2], 1.0);

  g_free (raw_values);

  /* now pull in mapped GValues for some timestamps */
  g_values = g_new0 (GValue, 3);

  fail_unless (gst_object_get_g_value_array (GST_OBJECT (elem), "int",
          0, GST_SECOND / 2, 3, g_values));
  fail_unless_equals_int (g_value_get_int (&g_values[0]), 0);
  fail_unless_equals_int (g_value_get_int (&g_values[1]), 50);
  fail_unless_equals_int (g_value_get_int (&g_values[2]), 100);

  g_free (g_values);

  /* now pull in mapped values for some timestamps */
  values = g_new0 (gint, 3);

  fail_unless (gst_object_get_value_array (GST_OBJECT (elem), "int",
          0, GST_SECOND / 2, 3, values));
  fail_unless_equals_int (values[0], 0);
  fail_unless_equals_int (values[1], 50);
  fail_unless_equals_int (values[2], 100);

  g_free (values);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test if values below minimum and above maximum are clipped */
GST_START_TEST (controller_interpolation_linear_invalid_values)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "float", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 2.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 4 * GST_SECOND, -2.0));

  /* now pull in values for some timestamps and see if clipping works */
  /* 200.0 */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 100.0);
  /* 100.0 */
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 100.0);
  /* 50.0 */
  gst_object_sync_values (GST_OBJECT (elem),
      1 * GST_SECOND + 500 * GST_MSECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 50.0);
  /* 0.0 */
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 0.0);
  /* -100.0 */
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 0.0);
  /* -200.0 */
  gst_object_sync_values (GST_OBJECT (elem), 4 * GST_SECOND);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_float, 0.0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (controller_interpolation_linear_default_values)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* Should fail if no value was set yet
   * FIXME: will not fail, as interpolation assumes val[0]=default_value if
   * nothing else is set.
   fail_if (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, &val_int));
   */

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 3 * GST_SECOND, 1.0));

  /* now pull in values for some timestamps */
  /* should give the value of the first control point for timestamps before it */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));

  /* unset the old ones */
  fail_unless (gst_timed_value_control_source_unset (tvcs, 1 * GST_SECOND));
  fail_unless (gst_timed_value_control_source_unset (tvcs, 3 * GST_SECOND));

  /* now pull in values for some timestamps */
  /* should now give our value for timestamp 0 */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test gst_controller_set_disabled() with linear interpolation */
GST_START_TEST (controller_interpolation_linear_disabled)
{
  GstControlSource *cs1, *cs2;
  GstTimedValueControlSource *tvcs1, *tvcs2;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs1 = gst_interpolation_control_source_new ();
  tvcs1 = (GstTimedValueControlSource *) cs1;

  cs2 = gst_interpolation_control_source_new ();
  tvcs2 = (GstTimedValueControlSource *) cs2;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs1)));
  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "double", cs2)));

  /* set interpolation mode */
  g_object_set (cs1, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
  g_object_set (cs2, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs1, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs1, 2 * GST_SECOND, 1.0));

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs2, 0 * GST_SECOND, 0.2));
  fail_unless (gst_timed_value_control_source_set (tvcs2, 2 * GST_SECOND, 0.4));

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 30.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 40.0);

  /* now pull in values for some timestamps, prop double disabled */
  GST_TEST_OBJ (elem)->val_int = 0;
  GST_TEST_OBJ (elem)->val_double = 0.0;
  gst_object_set_control_binding_disabled (GST_OBJECT (elem), "double", TRUE);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);

  /* now pull in values for some timestamps, after enabling double again */
  GST_TEST_OBJ (elem)->val_int = 0;
  GST_TEST_OBJ (elem)->val_double = 0.0;
  gst_object_set_control_binding_disabled (GST_OBJECT (elem), "double", FALSE);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 30.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 40.0);

  /* now pull in values for some timestamps, after disabling all props */
  GST_TEST_OBJ (elem)->val_int = 0;
  GST_TEST_OBJ (elem)->val_double = 0.0;
  gst_object_set_control_bindings_disabled (GST_OBJECT (elem), TRUE);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 0.0);

  /* now pull in values for some timestamps, enabling double again */
  GST_TEST_OBJ (elem)->val_int = 0;
  GST_TEST_OBJ (elem)->val_double = 0.0;
  gst_object_set_control_binding_disabled (GST_OBJECT (elem), "double", FALSE);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 30.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 40.0);

  /* now pull in values for some timestamps, enabling all */
  GST_TEST_OBJ (elem)->val_int = 0;
  GST_TEST_OBJ (elem)->val_double = 0.0;
  gst_object_set_control_bindings_disabled (GST_OBJECT (elem), FALSE);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 20.0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 30.0);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  fail_unless_equals_float (GST_TEST_OBJ (elem)->val_double, 40.0);

  gst_object_unref (cs1);
  gst_object_unref (cs2);
  gst_object_unref (elem);
}

GST_END_TEST;


GST_START_TEST (controller_interpolation_set_from_list)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstTimedValue *tval;
  GstElement *elem;
  GSList *list = NULL;

  /* test that an invalid timestamp throws a warning of some sort */
  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control value */
  tval = g_new0 (GstTimedValue, 1);
  tval->timestamp = GST_CLOCK_TIME_NONE;
  tval->value = 0.0;

  list = g_slist_append (list, tval);

  fail_if (gst_timed_value_control_source_set_from_list (tvcs, list));

  /* try again with a valid stamp, should work now */
  tval->timestamp = 0;
  fail_unless (gst_timed_value_control_source_set_from_list (tvcs, list));

  /* allocated GstTimedValue now belongs to the controller, but list not */
  g_free (tval);
  g_slist_free (list);
  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;


/* test linear interpolation for ts < first control point */
GST_START_TEST (controller_interpolation_linear_before_ts0)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 4 * GST_SECOND, 0.0));

  /* now pull in values for some timestamps after first control point */
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 4 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  /* now pull in values for some timestamps before first control point */
  GST_TEST_OBJ (elem)->val_int = 25;
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test linear interpolation of enums */
GST_START_TEST (controller_interpolation_linear_enums)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "enum", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 4 * GST_SECOND, 1.0));

  /* now pull in values going over the enum values */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_enum, ENUM_V0);
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_enum, ENUM_V10);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_enum, ENUM_V11);
  gst_object_sync_values (GST_OBJECT (elem), 3 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_enum, ENUM_V12);
  gst_object_sync_values (GST_OBJECT (elem), 4 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_enum, ENUM_V255);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value counts */
GST_START_TEST (controller_timed_value_count)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* set interpolation mode */
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);

  fail_unless (gst_timed_value_control_source_get_count (tvcs) == 0);

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_get_count (tvcs) == 1);
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));
  fail_unless (gst_timed_value_control_source_get_count (tvcs) == 2);

  /* unset control values */
  fail_unless (gst_timed_value_control_source_unset (tvcs, 2 * GST_SECOND));
  fail_unless (gst_timed_value_control_source_get_count (tvcs) == 1);
  fail_unless (gst_timed_value_control_source_unset (tvcs, 0 * GST_SECOND));
  fail_unless (gst_timed_value_control_source_get_count (tvcs) == 0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;


/* test lfo control source with sine waveform */
GST_START_TEST (controller_lfo_sine)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SINE,
      "frequency", 1.0, "timeshift", (GstClockTime) 0,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test lfo control source with sine waveform and timeshift */
GST_START_TEST (controller_lfo_sine_timeshift)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SINE,
      "frequency", 1.0, "timeshift", 250 * GST_MSECOND,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test lfo control source with square waveform */
GST_START_TEST (controller_lfo_square)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SQUARE,
      "frequency", 1.0, "timeshift", (GstClockTime) 0,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test lfo control source with saw waveform */
GST_START_TEST (controller_lfo_saw)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SAW,
      "frequency", 1.0, "timeshift", (GstClockTime) 0,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test lfo control source with reverse saw waveform */
GST_START_TEST (controller_lfo_rsaw)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_REVERSE_SAW,
      "frequency", 1.0, "timeshift", (GstClockTime) 0,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 25);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 75);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test lfo control source with saw waveform */
GST_START_TEST (controller_lfo_triangle)
{
  GstControlSource *cs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new lfo control source */
  cs = gst_lfo_control_source_new ();

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  /* configure lfo */
  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_TRIANGLE,
      "frequency", 1.0, "timeshift", (GstClockTime) 0,
      "amplitude", 0.5, "offset", 0.5, NULL);

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 1000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);
  gst_object_sync_values (GST_OBJECT (elem), 2000 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1250 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 1500 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 1750 * GST_MSECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

/* test timed value handling in trigger mode */
GST_START_TEST (controller_trigger_exact)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;
  gdouble raw_val;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_trigger_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  fail_if (gst_control_source_get_value (cs, 0 * GST_SECOND, &raw_val));

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.5));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));

  /* now pull in values for some timestamps */
  fail_unless (gst_control_source_get_value (cs, 0 * GST_SECOND, &raw_val));

  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);

  GST_TEST_OBJ (elem)->val_int = 0;
  fail_if (gst_control_source_get_value (cs, 1 * GST_SECOND, &raw_val));
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  fail_unless (gst_control_source_get_value (cs, 2 * GST_SECOND, &raw_val));
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (controller_trigger_tolerance)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;
  gdouble raw_val;

  elem = gst_element_factory_make ("testobj", NULL);

  /* new interpolation control source */
  cs = gst_trigger_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new (GST_OBJECT (elem), "int", cs)));

  g_object_set (cs, "tolerance", G_GINT64_CONSTANT (10), NULL);

  fail_if (gst_control_source_get_value (cs, 0 * GST_SECOND, &raw_val));

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.5));
  fail_unless (gst_timed_value_control_source_set (tvcs, 2 * GST_SECOND, 1.0));

  /* now pull in values for some timestamps */
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);
  gst_object_sync_values (GST_OBJECT (elem), 0 * GST_SECOND + 5);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 50);

  GST_TEST_OBJ (elem)->val_int = 0;
  gst_object_sync_values (GST_OBJECT (elem), 1 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND - 5);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);
  gst_object_sync_values (GST_OBJECT (elem), 2 * GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (controller_first_value_in_range)
{
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem;

  elem = gst_element_factory_make ("testobj", NULL);

  /* set object values */
  GST_TEST_OBJ (elem)->val_int = 32;

  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;
  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem),
          gst_direct_control_binding_new_absolute (GST_OBJECT (elem), "int",
              cs)));

  /* Add 1 value of 0 (which is the default) */
  fail_unless (gst_timed_value_control_source_set (tvcs, GST_SECOND, 0));
  /* First sync outside interpolation range */
  gst_object_sync_values (GST_OBJECT (elem), 0);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 32);

  gst_object_sync_values (GST_OBJECT (elem), GST_SECOND);
  fail_unless_equals_int (GST_TEST_OBJ (elem)->val_int, 0);

  gst_object_unref (cs);
  gst_object_unref (elem);
}

GST_END_TEST;

GST_START_TEST (controller_proxy)
{
  GstControlBinding *cb, *cb2;
  GstControlSource *cs;
  GstTimedValueControlSource *tvcs;
  GstElement *elem, *elem2;
  GstClockTime time;
  gint int1, int2;
  GValue gval1 = G_VALUE_INIT, gval2 = G_VALUE_INIT;
  GValue *val1, *val2;

  elem = gst_element_factory_make ("testobj", NULL);
  elem2 = gst_element_factory_make ("testobj", NULL);

  /* proxy control binding from elem to elem2 */
  cb = gst_proxy_control_binding_new (GST_OBJECT (elem), "int",
      GST_OBJECT (elem2), "int");
  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem), cb));

  /* test that no proxy does nothing */
  val1 = gst_control_binding_get_value (cb, 0);
  fail_unless (val1 == NULL);
  fail_if (gst_control_binding_get_value_array (cb, 0, 0, 1, &int1));
  fail_if (gst_control_binding_get_g_value_array (cb, 0, 0, 1, &gval1));

  /* new interpolation control source */
  cs = gst_trigger_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  cb2 = gst_direct_control_binding_new (GST_OBJECT (elem2), "int", cs);
  fail_unless (gst_object_add_control_binding (GST_OBJECT (elem2), cb2));

  /* set control values */
  fail_unless (gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0));
  fail_unless (gst_timed_value_control_source_set (tvcs, 1 * GST_SECOND, 1.0));

  /* now pull in values for some timestamps */
  time = 0 * GST_SECOND;
  gst_object_sync_values (GST_OBJECT (elem), time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, 0);
  val1 = gst_control_binding_get_value (cb, time);
  val2 = gst_control_binding_get_value (cb2, time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (val1));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (val2));
  fail_unless (gst_control_binding_get_value_array (cb, time, 0, 1, &int1));
  fail_unless (gst_control_binding_get_value_array (cb2, time, 0, 1, &int2));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, int1);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, int2);
  fail_unless (gst_control_binding_get_g_value_array (cb, time, 0, 1, &gval1));
  fail_unless (gst_control_binding_get_g_value_array (cb2, time, 0, 1, &gval2));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (&gval1));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (&gval2));
  g_value_unset (val1);
  g_value_unset (val2);
  g_free (val1);
  g_free (val2);
  g_value_unset (&gval1);
  g_value_unset (&gval2);

  time = 1 * GST_SECOND;
  gst_object_sync_values (GST_OBJECT (elem), time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, 100);
  val1 = gst_control_binding_get_value (cb, time);
  val2 = gst_control_binding_get_value (cb2, time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (val1));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (val2));
  fail_unless (gst_control_binding_get_value_array (cb, time, 0, 1, &int1));
  fail_unless (gst_control_binding_get_value_array (cb2, time, 0, 1, &int2));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, int1);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, int2);
  fail_unless (gst_control_binding_get_g_value_array (cb, time, 0, 1, &gval1));
  fail_unless (gst_control_binding_get_g_value_array (cb2, time, 0, 1, &gval2));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (&gval1));
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int,
      g_value_get_int (&gval2));
  g_value_unset (val1);
  g_value_unset (val2);
  g_free (val1);
  g_free (val2);
  g_value_unset (&gval1);
  g_value_unset (&gval2);

  /* test syncing on the original control binding */
  time = 0 * GST_SECOND;
  gst_object_sync_values (GST_OBJECT (elem2), time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, 0);

  time = 1 * GST_SECOND;
  gst_object_sync_values (GST_OBJECT (elem2), time);
  fail_unless_equals_int (GST_TEST_OBJ (elem2)->val_int, 100);

  gst_object_unref (cs);
  gst_object_unref (elem);
  gst_object_unref (elem2);
}

GST_END_TEST;


static Suite *
gst_controller_suite (void)
{
  Suite *s = suite_create ("Controller");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);
  tcase_add_checked_fixture (tc, setup, teardown);
  tcase_add_test (tc, controller_controlsource_empty1);
  tcase_add_test (tc, controller_controlsource_empty2);
  tcase_add_test (tc, controller_interpolation_none);
  tcase_add_test (tc, controller_interpolation_linear);
  tcase_add_test (tc, controller_interpolation_cubic);
  tcase_add_test (tc, controller_interpolation_cubic_too_few_cp);
  tcase_add_test (tc, controller_interpolation_unset);
  tcase_add_test (tc, controller_interpolation_unset_all);
  tcase_add_test (tc, controller_interpolation_linear_absolute_value_array);
  tcase_add_test (tc, controller_interpolation_linear_value_array);
  tcase_add_test (tc, controller_interpolation_linear_invalid_values);
  tcase_add_test (tc, controller_interpolation_linear_default_values);
  tcase_add_test (tc, controller_interpolation_linear_disabled);
  tcase_add_test (tc, controller_interpolation_set_from_list);
  tcase_add_test (tc, controller_interpolation_linear_before_ts0);
  tcase_add_test (tc, controller_interpolation_linear_enums);
  tcase_add_test (tc, controller_timed_value_count);
  tcase_add_test (tc, controller_lfo_sine);
  tcase_add_test (tc, controller_lfo_sine_timeshift);
  tcase_add_test (tc, controller_lfo_square);
  tcase_add_test (tc, controller_lfo_saw);
  tcase_add_test (tc, controller_lfo_rsaw);
  tcase_add_test (tc, controller_lfo_triangle);
  tcase_add_test (tc, controller_trigger_exact);
  tcase_add_test (tc, controller_first_value_in_range);
  tcase_add_test (tc, controller_trigger_tolerance);
  tcase_add_test (tc, controller_proxy);

  return s;
}

GST_CHECK_MAIN (gst_controller);
