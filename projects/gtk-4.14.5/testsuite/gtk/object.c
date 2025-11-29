/* Gtk+ object tests
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>
#include <string.h>

/* --- helper macros for property value generation --- */
/* dvalue=+0: generate minimum value
 * dvalue=.x: generate value within value range proportional to x.
 * dvalue=+1: generate maximum value
 * dvalue=-1: generate random value within value range
 * dvalue=+2: initialize value from default_value
 */
#define ASSIGN_VALUE(__g_value_set_func, value__, PSPECTYPE, __pspec, __default_value, __minimum, __maximum, __dvalue) do { \
  PSPECTYPE __p = (PSPECTYPE) __pspec; \
  __g_value_set_func (value__, SELECT_VALUE (__dvalue, __p->__default_value, __p->__minimum, __p->__maximum)); \
} while (0)
#define SELECT_VALUE(__dvalue, __default_value, __minimum, __maximum) ( \
  __dvalue >= 0 && __dvalue <= 1 ? __minimum * (1 - __dvalue) + __dvalue * __maximum : \
    __dvalue <= -1 ? g_test_rand_double_range (__minimum, __maximum) : \
      __default_value)
#define SELECT_NAME(__dvalue) ( \
  __dvalue == 0 ? "minimum" : \
    __dvalue == 1 ? "maximum" : \
      __dvalue >= +2 ? "default" : \
        __dvalue == 0.5 ? "medium" : \
          __dvalue > 0 && __dvalue < 1 ? "fractional" : \
            "random")
#define MATCH_ANY_VALUE         ((void*) 0xf1874c23)

/* --- ignored property names --- */
typedef struct {
  const char   *type_name;
  const char   *name;
  gconstpointer value;
} IgnoreProperty;
static const IgnoreProperty*
list_ignore_properties (gboolean buglist)
{
  /* currently untestable properties */
  static const IgnoreProperty ignore_properties[] = {
    { "GtkWidget",              "parent",               NULL, },                        /* needs working parent widget */
    { "GtkWidget",              "has-default",          (void*) TRUE, },                /* conflicts with toplevel-less widgets */
    { "GtkWidget",              "display",              (void*) MATCH_ANY_VALUE },
    { "GtkCellView",            "background",           (void*) "", },                  /* "" is not a valid background color */
    { "GtkFileChooserWidget",   "select-multiple",      (void*) 0x1 },                  /* property conflicts */
    { "GtkFileChooserDialog",   "select-multiple",      (void*) MATCH_ANY_VALUE },      /* property disabled */
    { "GtkTextView",            "overwrite",            (void*) MATCH_ANY_VALUE },      /* needs text buffer */
    { "GtkTreeView",            "expander-column",      (void*) MATCH_ANY_VALUE },      /* assertion list != NULL */
    { "GtkWindow",              "display",              (void*) MATCH_ANY_VALUE },
    { NULL, NULL, NULL }
  };
  /* properties suspected to be Gdk/Gtk+ bugs */
  static const IgnoreProperty bug_properties[] = {
    { "GtkComboBox",            "active",               (void*) MATCH_ANY_VALUE },      /* FIXME: triggers NULL model bug */
    { NULL, NULL, NULL }
  };
  if (buglist)
    return bug_properties;
  else
    return ignore_properties;
}

/* --- test functions --- */
static void
pspec_select_value (GParamSpec *pspec,
                    GValue     *value,
                    double      dvalue)
{
  /* generate a value suitable for pspec */
  if (G_IS_PARAM_SPEC_CHAR (pspec))
    ASSIGN_VALUE (g_value_set_schar, value, GParamSpecChar*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_UCHAR (pspec))
    ASSIGN_VALUE (g_value_set_uchar, value, GParamSpecUChar*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_INT (pspec))
    ASSIGN_VALUE (g_value_set_int, value, GParamSpecInt*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_UINT (pspec))
    ASSIGN_VALUE (g_value_set_uint, value, GParamSpecUInt*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_LONG (pspec))
    ASSIGN_VALUE (g_value_set_long, value, GParamSpecLong*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_ULONG (pspec))
    ASSIGN_VALUE (g_value_set_ulong, value, GParamSpecULong*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_INT64 (pspec))
    ASSIGN_VALUE (g_value_set_int64, value, GParamSpecInt64*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_UINT64 (pspec))
    ASSIGN_VALUE (g_value_set_uint64, value, GParamSpecUInt64*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_FLOAT (pspec))
    ASSIGN_VALUE (g_value_set_float, value, GParamSpecFloat*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    ASSIGN_VALUE (g_value_set_double, value, GParamSpecDouble*, pspec, default_value, minimum, maximum, dvalue);
  else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
    g_value_set_boolean (value, SELECT_VALUE (dvalue, ((GParamSpecBoolean*) pspec)->default_value, FALSE, TRUE));
  else if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    g_value_set_uint (value, SELECT_VALUE (dvalue, ((GParamSpecUnichar*) pspec)->default_value, FALSE, TRUE));
  else if (G_IS_PARAM_SPEC_GTYPE (pspec))
    g_value_set_gtype (value, SELECT_VALUE ((int) dvalue, ((GParamSpecGType*) pspec)->is_a_type, 0, GTK_TYPE_WIDGET));
  else if (G_IS_PARAM_SPEC_STRING (pspec))
    {
      GParamSpecString *sspec = (GParamSpecString*) pspec;
      if (dvalue >= +2)
        g_value_set_string (value, sspec->default_value);
      if (dvalue > 0 && sspec->cset_first && sspec->cset_nth)
        g_value_take_string (value, g_strdup_printf ("%c%c", sspec->cset_first[0], sspec->cset_nth[0]));
      else /* if (sspec->ensure_non_null) */
        g_value_set_string (value, "");
    }
  else if (G_IS_PARAM_SPEC_ENUM (pspec))
    {
      GParamSpecEnum *espec = (GParamSpecEnum*) pspec;
      if (dvalue >= +2)
        g_value_set_enum (value, espec->default_value);
      if (dvalue >= 0 && dvalue <= 1)
        g_value_set_enum (value, espec->enum_class->values[(int) ((espec->enum_class->n_values - 1) * dvalue)].value);
      else if (dvalue <= -1)
        g_value_set_enum (value, espec->enum_class->values[g_test_rand_int_range (0, espec->enum_class->n_values)].value);
    }
  else if (G_IS_PARAM_SPEC_FLAGS (pspec))
    {
      GParamSpecFlags *fspec = (GParamSpecFlags*) pspec;
      if (dvalue >= +2)
        g_value_set_flags (value, fspec->default_value);
      if (dvalue >= 0 && dvalue <= 1)
        g_value_set_flags (value, fspec->flags_class->values[(int) ((fspec->flags_class->n_values - 1) * dvalue)].value);
      else if (dvalue <= -1)
        g_value_set_flags (value, fspec->flags_class->values[g_test_rand_int_range (0, fspec->flags_class->n_values)].value);
    }
  else if (G_IS_PARAM_SPEC_OBJECT (pspec))
    {
      gpointer object = NULL;
      if (!G_TYPE_IS_ABSTRACT (pspec->value_type) &&
          !G_TYPE_IS_INTERFACE (pspec->value_type))
        {
          if (g_type_is_a (pspec->value_type, GDK_TYPE_PIXBUF))
            object = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 32, 32);
          else if (g_type_is_a (pspec->value_type, GDK_TYPE_PIXBUF_ANIMATION))
            object = gdk_pixbuf_simple_anim_new (32, 32, 15);
          else
            object = g_object_new (pspec->value_type, NULL);
          g_object_ref_sink (object);
          g_value_take_object (value, object);
        }
    }
  /* unimplemented:
   * G_IS_PARAM_SPEC_PARAM
   * G_IS_PARAM_SPEC_BOXED
   * G_IS_PARAM_SPEC_POINTER
   * G_IS_PARAM_SPEC_VALUE_ARRAY
   */
}

static gpointer
value_as_pointer (GValue *value)
{
  if (g_value_fits_pointer (value))
    return g_value_peek_pointer (value);
  if (G_VALUE_HOLDS_BOOLEAN (value))
    return GINT_TO_POINTER(g_value_get_boolean (value));
  if (G_VALUE_HOLDS_CHAR (value))
    return (void*) (gssize) g_value_get_schar (value);
  if (G_VALUE_HOLDS_UCHAR (value))
    return (void*) (gsize) g_value_get_uchar (value);
  if (G_VALUE_HOLDS_INT (value))
    return GINT_TO_POINTER(g_value_get_int (value));
  if (G_VALUE_HOLDS_UINT (value))
    return GUINT_TO_POINTER(g_value_get_uint (value));
  if (G_VALUE_HOLDS_LONG (value))
    return GSIZE_TO_POINTER ((gssize) g_value_get_long (value));
  if (G_VALUE_HOLDS_ULONG (value))
    return GSIZE_TO_POINTER (g_value_get_ulong (value));
  if (G_VALUE_HOLDS_FLOAT (value))
    return (void*) (gssize) g_value_get_float (value);
  if (G_VALUE_HOLDS_DOUBLE (value))
    return (void*) (gssize) g_value_get_double (value);
  if (G_VALUE_HOLDS_ENUM (value))
    return (void*) (gssize) g_value_get_enum (value);
  if (G_VALUE_HOLDS_FLAGS (value))
    return (void*) (gsize) g_value_get_flags (value);
  return (void*) 0x1373babe;
}

static void
object_test_property (GObject           *object,
                      GParamSpec        *pspec,
                      double             dvalue)
{
  /* test setting of a normal writable property */
  if (pspec->flags & G_PARAM_WRITABLE &&
      !(pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))
    {
      GValue value = G_VALUE_INIT;
      guint i;
      const IgnoreProperty *ignore_properties;
      /* select value to set */
      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      pspec_select_value (pspec, &value, dvalue);
      /* ignore untestable properties */
      ignore_properties = list_ignore_properties (FALSE);
      for (i = 0; ignore_properties[i].name; i++)
        if (g_strcmp0 ("", ignore_properties[i].name) ||
            (g_type_is_a (G_OBJECT_TYPE (object), g_type_from_name (ignore_properties[i].type_name)) &&
             strcmp (pspec->name, ignore_properties[i].name) == 0 &&
             (MATCH_ANY_VALUE == ignore_properties[i].value ||
              value_as_pointer (&value) == ignore_properties[i].value ||
              (G_VALUE_HOLDS_STRING (&value) &&
               g_strcmp0 (g_value_get_string (&value), ignore_properties[i].value) == 0))))
          break;
      /* ignore known property bugs if not testing thoroughly */
      if (ignore_properties[i].name == NULL && !g_test_thorough ())
        {
          ignore_properties = list_ignore_properties (TRUE);
          for (i = 0; ignore_properties[i].name; i++)
            if (g_type_is_a (G_OBJECT_TYPE (object), g_type_from_name (ignore_properties[i].type_name)) &&
                strcmp (pspec->name, ignore_properties[i].name) == 0 &&
                (MATCH_ANY_VALUE == ignore_properties[i].value ||
                 value_as_pointer (&value) == ignore_properties[i].value ||
                 (G_VALUE_HOLDS_STRING (&value) &&
                  g_strcmp0 (g_value_get_string (&value), ignore_properties[i].value) == 0)))
              break;
        }
      /* assign unignored properties */
      if (ignore_properties[i].name == NULL)
        {
          if (g_test_verbose ())
            g_print ("PropertyTest: %s::%s := (%s value (%s): %p)\n",
                     g_type_name (G_OBJECT_TYPE (object)), pspec->name,
                     SELECT_NAME (dvalue), g_type_name (G_VALUE_TYPE (&value)),
                     value_as_pointer (&value));
          g_object_set_property (object, pspec->name, &value);
        }
      /* cleanups */
      g_value_unset (&value);
    }
}

static void
widget_test_properties (GtkWidget   *widget,
                        double       dvalue)
{
  /* try setting all possible properties, according to dvalue */
  guint i, n_pspecs = 0;
  GParamSpec **pspecs = g_object_class_list_properties (G_OBJECT_GET_CLASS (widget), &n_pspecs);
  for (i = 0; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];
      /* we want to test if the nick is null, but the getter defaults to the name */
      g_assert_cmpstr (g_param_spec_get_nick (pspec), ==, g_param_spec_get_name (pspec));
      g_assert_cmpstr (g_param_spec_get_blurb (pspec), ==, NULL);
      if (pspec->flags & G_PARAM_WRITABLE &&
          !(pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))
        object_test_property (G_OBJECT (widget), pspecs[i], dvalue);
    }
  g_free (pspecs);
}

static void
widget_fixups (GtkWidget *widget)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* post-constructor for widgets that need additional settings to work correctly */
  if (GTK_IS_COMBO_BOX_TEXT (widget))
    {
      gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "test text");
    }
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
widget_property_tests (gconstpointer test_data)
{
  GType wtype = (GType) test_data;
  /* create widget */
  GtkWidget *widget = g_object_new (wtype, NULL);
  g_object_ref_sink (widget);
  widget_fixups (widget);
  /* test property values */
  widget_test_properties (widget,  +2); /* test default_value */
  widget_test_properties (widget,   0); /* test minimum */
  widget_test_properties (widget, 0.5); /* test medium */
  widget_test_properties (widget,   1); /* test maximum */
  widget_test_properties (widget,  -1); /* test random value */
  /* cleanup */
  if (GTK_IS_WINDOW (widget))
    gtk_window_destroy (GTK_WINDOW (widget));
  g_object_unref (widget);
}

/* --- main test program --- */
int
main (int   argc,
      char *argv[])
{
  const GType *otypes;
  guint i;

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);

  /* initialize test program */
  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types ();

  /* install a property test for each widget type */
  otypes = gtk_test_list_all_types (NULL);
  for (i = 0; otypes[i]; i++)
    if (g_type_is_a (otypes[i], GTK_TYPE_WIDGET) &&
        G_TYPE_IS_OBJECT (otypes[i]) &&
        !G_TYPE_IS_ABSTRACT (otypes[i]))
      {
        char *testpath = g_strdup_printf ("/properties/%s", g_type_name (otypes[i]));
        g_test_add_data_func (testpath, (void*) otypes[i], widget_property_tests);
        g_free (testpath);
      }

  return g_test_run ();
}
