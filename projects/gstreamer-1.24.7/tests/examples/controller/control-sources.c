/* 
 * control-sources.c
 *
 * Generates a datafile for various control sources.
 *
 * Needs gnuplot for plotting.
 * plot "ctrl_i1.dat" using 1:2 with points title 'none', "" using 1:3 with points title 'linear', "" using 1:4 with points title 'cubic', "ctrl_i2.dat" using 1:2 with lines title 'none', "" using 1:3 with lines title 'linear', "" using 1:4 with lines title 'cubic'
 * plot "ctrl_l1.dat" using 1:2 with points title 'sine', "" using 1:3 with points title 'square', "" using 1:4 with points title 'saw', "" using 1:5 with points title 'revsaw', "" using 1:6 with points title 'triangle', "ctrl_l2.dat" using 1:2 with lines title 'sine', "" using 1:3 with lines title 'square', "" using 1:4 with lines title 'saw', "" using 1:5 with lines title 'revsaw', "" using 1:6 with lines title 'triangle'
 * plot "ctrl_cl1.dat" using 1:2 with points title 'sine', "ctrl_cl2.dat" using 1:2 with lines title 'sine'
 */

#include <stdio.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/controller/gstinterpolationcontrolsource.h>
#include <gst/controller/gstlfocontrolsource.h>
#include <gst/controller/gstdirectcontrolbinding.h>

/* local test element */

enum
{
  PROP_INT = 1,
  PROP_FLOAT,
  PROP_DOUBLE,
  PROP_BOOLEAN,
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
      GST_DEBUG ("test value double=%f", self->val_double);
      break;
    case PROP_BOOLEAN:
      self->val_boolean = g_value_get_boolean (value);
      GST_DEBUG ("test value boolean=%d", self->val_boolean);
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
  static gpointer TEST_OBJ_type = 0;

  if (g_once_init_enter_pointer (&TEST_OBJ_type)) {
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
    g_once_init_leave_pointer (&TEST_OBJ_type, type);
  }
  return TEST_OBJ_type;
}

static void
test_interpolation (void)
{
  GstObject *e;
  GstTimedValueControlSource *tvcs;
  GstControlSource *cs;
  gint t, i1, i2, i3;
  GValue *v1, *v2, *v3;
  gint n_values;
  FILE *f;

  e = (GstObject *) gst_element_factory_make ("testobj", NULL);

  cs = gst_interpolation_control_source_new ();
  tvcs = (GstTimedValueControlSource *) cs;

  gst_object_add_control_binding (e, gst_direct_control_binding_new (e, "int",
          cs));

  gst_timed_value_control_source_set (tvcs, 0 * GST_SECOND, 0.0);
  gst_timed_value_control_source_set (tvcs, 10 * GST_SECOND, 1.0);
  gst_timed_value_control_source_set (tvcs, 20 * GST_SECOND, 0.5);
  gst_timed_value_control_source_set (tvcs, 30 * GST_SECOND, 0.2);

  /* test single values */
  if (!(f = fopen ("ctrl_i1.dat", "w")))
    exit (-1);
  fprintf (f, "# Time None Linear Cubic\n");

  for (t = 0; t < 40; t++) {
    g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i1 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i2 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i3 = GST_TEST_OBJ (e)->val_int;

    fprintf (f, "%4.1f %d %d %d\n", (gfloat) t, i1, i2, i3);
  }

  fclose (f);

  /* test value arrays */
  if (!(f = fopen ("ctrl_i2.dat", "w")))
    exit (-1);
  fprintf (f, "# Time None Linear Cubic\n");
  n_values = 40 * 10;

  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_NONE, NULL);
  v1 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v1);

  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_LINEAR, NULL);
  v2 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v2);

  g_object_set (cs, "mode", GST_INTERPOLATION_MODE_CUBIC, NULL);
  v3 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v3);

  for (t = 0; t < n_values; t++) {
    i1 = g_value_get_int (&v1[t]);
    i2 = g_value_get_int (&v2[t]);
    i3 = g_value_get_int (&v3[t]);
    fprintf (f, "%4.1f %d %d %d\n", (gfloat) t / 10.0, i1, i2, i3);
    g_value_unset (&v1[t]);
    g_value_unset (&v2[t]);
    g_value_unset (&v3[t]);
  }
  g_free (v1);
  g_free (v2);
  g_free (v3);

  fclose (f);

  gst_object_unref (cs);
  gst_object_unref (e);
}

static void
test_lfo (void)
{
  GstObject *e;
  GstControlSource *cs;
  gint t, i1, i2, i3, i4, i5;
  GValue *v1, *v2, *v3, *v4, *v5;
  gint n_values;
  FILE *f;

  e = (GstObject *) gst_element_factory_make ("testobj", NULL);

  cs = gst_lfo_control_source_new ();

  gst_object_add_control_binding (e, gst_direct_control_binding_new (e, "int",
          cs));

  g_object_set (cs,
      "frequency", (gdouble) 0.05,
      "timeshift", (GstClockTime) 0,
      "amplitude", (gdouble) 0.5, "offset", (gdouble) 0.5, NULL);

  /* test single values */
  if (!(f = fopen ("ctrl_l1.dat", "w")))
    exit (-1);
  fprintf (f, "# Time Sine Square Saw RevSaw Triangle\n");

  for (t = 0; t < 40; t++) {
    g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SINE, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i1 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SQUARE, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i2 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SAW, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i3 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "waveform", GST_LFO_WAVEFORM_REVERSE_SAW, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i4 = GST_TEST_OBJ (e)->val_int;

    g_object_set (cs, "waveform", GST_LFO_WAVEFORM_TRIANGLE, NULL);
    gst_object_sync_values (e, t * GST_SECOND);
    i5 = GST_TEST_OBJ (e)->val_int;

    fprintf (f, "%4.1f %d %d %d %d %d\n", (gfloat) t, i1, i2, i3, i4, i5);
  }

  fclose (f);

  /* test value arrays */
  if (!(f = fopen ("ctrl_l2.dat", "w")))
    exit (-1);
  fprintf (f, "# Time Sine Square Saw RevSaw Triangle\n");
  n_values = 40 * 10;

  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SINE, NULL);
  v1 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v1);

  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SQUARE, NULL);
  v2 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v2);

  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_SAW, NULL);
  v3 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v3);

  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_REVERSE_SAW, NULL);
  v4 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v4);

  g_object_set (cs, "waveform", GST_LFO_WAVEFORM_TRIANGLE, NULL);
  v5 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v5);

  for (t = 0; t < n_values; t++) {
    i1 = g_value_get_int (&v1[t]);
    i2 = g_value_get_int (&v2[t]);
    i3 = g_value_get_int (&v3[t]);
    i4 = g_value_get_int (&v4[t]);
    i5 = g_value_get_int (&v5[t]);
    fprintf (f, "%4.1f %d %d %d %d %d\n", (gfloat) t / 10.0, i1, i2, i3, i4,
        i5);
    g_value_unset (&v1[t]);
    g_value_unset (&v2[t]);
    g_value_unset (&v3[t]);
    g_value_unset (&v4[t]);
    g_value_unset (&v5[t]);
  }
  g_free (v1);
  g_free (v2);
  g_free (v3);
  g_free (v4);
  g_free (v5);

  fclose (f);

  gst_object_unref (cs);
  gst_object_unref (e);
}

static void
test_chained_lfo (void)
{
  GstObject *e;
  GstControlSource *cs1, *cs2;
  gint t, i1;
  GValue *v1;
  gint n_values;
  FILE *f;

  e = (GstObject *) gst_element_factory_make ("testobj", NULL);

  cs1 = gst_lfo_control_source_new ();

  gst_object_add_control_binding (e, gst_direct_control_binding_new (e, "int",
          cs1));

  g_object_set (cs1,
      "waveform", GST_LFO_WAVEFORM_SINE,
      "frequency", (gdouble) 0.05,
      "timeshift", (GstClockTime) 0, "offset", (gdouble) 0.5, NULL);

  cs2 = gst_lfo_control_source_new ();

  gst_object_add_control_binding ((GstObject *) cs1,
      gst_direct_control_binding_new ((GstObject *) cs1, "amplitude", cs2));

  g_object_set (cs2,
      "waveform", GST_LFO_WAVEFORM_SINE,
      "frequency", (gdouble) 0.05,
      "timeshift", (GstClockTime) 0,
      "amplitude", (gdouble) 0.5, "offset", (gdouble) 0.5, NULL);

  /* test single values */
  if (!(f = fopen ("ctrl_cl1.dat", "w")))
    exit (-1);
  fprintf (f, "# Time Sine\n");

  for (t = 0; t < 40; t++) {
    gst_object_sync_values (e, t * GST_SECOND);
    i1 = GST_TEST_OBJ (e)->val_int;

    fprintf (f, "%4.1f %d\n", (gfloat) t, i1);
  }

  fclose (f);

  /* test value arrays */
  if (!(f = fopen ("ctrl_cl2.dat", "w")))
    exit (-1);
  fprintf (f, "# Time Sine\n");
  n_values = 40 * 10;

  v1 = g_new0 (GValue, n_values);
  gst_object_get_g_value_array (e, "int", 0, GST_SECOND / 10, n_values, v1);

  for (t = 0; t < n_values; t++) {
    i1 = g_value_get_int (&v1[t]);
    fprintf (f, "%4.1f %d\n", (gfloat) t / 10.0, i1);
    g_value_unset (&v1[t]);
  }
  g_free (v1);

  fclose (f);

  gst_object_unref (cs1);
  gst_object_unref (cs2);
  gst_object_unref (e);
}

gint
main (gint argc, gchar ** argv)
{
  gst_init (&argc, &argv);

  gst_element_register (NULL, "testobj", GST_RANK_NONE, GST_TYPE_TEST_OBJ);

  test_interpolation ();
  test_lfo ();

  test_chained_lfo ();

  return 0;
}
