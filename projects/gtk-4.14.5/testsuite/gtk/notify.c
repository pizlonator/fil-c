/* Gtk+ property notify tests
 * Copyright (C) 2014 Matthias Clasen
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>
#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/wayland/gdkwayland.h"
#endif

static void
assert_notifies (GObject    *object,
                 const char *property,
                 guint       counted,
                 guint       expected)
{
  if (expected == counted)
    return;

  g_test_message ("ERROR: While testing %s::%s: %u notify emissions expected, but got %u",
                  G_OBJECT_TYPE_NAME (object), property,
                  expected, counted);
  g_test_fail ();
}

typedef struct
{
  const char *name;
  int count;
} NotifyData;

static void
count_notify (GObject *obj, GParamSpec *pspec, NotifyData *data)
{
  if (g_strcmp0 (data->name, pspec->name) == 0)
    data->count++;
}

/* Check that we get notifications when properties change.
 * Also check that we don't emit redundant notifications for
 * enum, flags, booleans, ints. We allow redundant notifications
 * for strings, and floats
 */
static void
check_property (GObject *instance, GParamSpec *pspec)
{
  g_test_message ("Checking %s:%s", G_OBJECT_TYPE_NAME (instance), pspec->name);

  if (G_TYPE_IS_ENUM (pspec->value_type))
    {
      GEnumClass *class;
      int i;
      NotifyData data;
      gulong id;
      int first;
      int value;
      int current_count;

      class = g_type_class_ref (pspec->value_type);

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      g_object_set (instance, pspec->name, value, NULL);

      assert_notifies (instance, pspec->name, data.count, 0);

      if (class->values[0].value == value)
        first = 1;
      else
        first = 0;

      for (i = first; i < class->n_values; i++)
        {
          /* skip duplicates */
          if (i > 0 && class->values[i].value == class->values[i - 1].value)
            continue;

          current_count = data.count + 1;
          g_object_set (instance, pspec->name, class->values[i].value, NULL);

          assert_notifies (instance, pspec->name, data.count, current_count);

          if (current_count == 10) /* just test a few */
            break;
        }

      g_signal_handler_disconnect (instance, id);
      g_type_class_unref (class);
    }
  else if (G_TYPE_IS_FLAGS (pspec->value_type))
    {
      GFlagsClass *class;
      int i;
      NotifyData data;
      gulong id;
      guint value;
      int current_count;

      class = g_type_class_ref (pspec->value_type);

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      g_object_set (instance, pspec->name, value, NULL);

      assert_notifies (instance, pspec->name, data.count, 0);

      for (i = 0; i < class->n_values; i++)
        {
          /* some flags have a 'none' member, skip it */
          if (class->values[i].value == 0)
            continue;

          if ((value & class->values[i].value) != 0)
            continue;

          value |= class->values[i].value;
          current_count = data.count + 1;
          g_object_set (instance, pspec->name, value, NULL);
          assert_notifies (instance, pspec->name, data.count, current_count);

          if (current_count == 10) /* just test a few */
            break;
        }

      g_signal_handler_disconnect (instance, id);
      g_type_class_unref (class);
    }
  else if (pspec->value_type == G_TYPE_BOOLEAN)
    {
      NotifyData data;
      gboolean value;
      gulong id;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      g_object_set (instance, pspec->name, value, NULL);

      assert_notifies (instance, pspec->name, data.count, 0);

      g_object_set (instance, pspec->name, 1 - value, NULL);

      assert_notifies (instance, pspec->name, data.count, 1);

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_INT)
    {
      GParamSpecInt *p = G_PARAM_SPEC_INT (pspec);
      int i;
      NotifyData data;
      gulong id;
      int value;
      int current_count;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      g_object_set (instance, pspec->name, value, NULL);

      assert_notifies (instance, pspec->name, data.count, 0);

      for (i = p->minimum; i <= p->maximum; i++)
        {
          g_object_get (instance, pspec->name, &value, NULL);
          if (value == i)
            continue;

          current_count = data.count + 1;
          g_object_set (instance, pspec->name, i, NULL);
          assert_notifies (instance, pspec->name, data.count, current_count);

          if (current_count == 10) /* just test a few */
            break;
        }

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_UINT)
    {
      guint i;
      NotifyData data;
      gulong id;
      guint value;
      int current_count;
      guint minimum, maximum;

      if (G_IS_PARAM_SPEC_UINT (pspec))
        {
          minimum = G_PARAM_SPEC_UINT (pspec)->minimum;
          maximum = G_PARAM_SPEC_UINT (pspec)->maximum;
        }
      else /* random */
        {
          minimum = 0;
          maximum = 1000;
        }

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      g_object_set (instance, pspec->name, value, NULL);

      assert_notifies (instance, pspec->name, data.count, 0);

      for (i = minimum; i <= maximum; i++)
        {
          g_object_get (instance, pspec->name, &value, NULL);
          if (value == i)
            continue;

          current_count = data.count + 1;
          g_object_set (instance, pspec->name, i, NULL);
          assert_notifies (instance, pspec->name, data.count, current_count);

          if (current_count == 10) /* just test a few */
            break;
        }

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_STRING)
    {
      NotifyData data;
      gulong id;
      char *value;
      char *new_value;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_get (instance, pspec->name, &value, NULL);
      /* don't check redundant notifications */

      new_value = g_strconcat ("(", value, ".", value, ")", NULL);

      g_object_set (instance, pspec->name, new_value, NULL);

      assert_notifies (instance, pspec->name, data.count, 1);

      g_free (value);
      g_free (new_value);

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_STRV)
    {
      NotifyData data;
      gulong id;
      const char *value[] = { "bla", "bla", NULL };

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      value[1] = "foo";

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 2);

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_DOUBLE)
    {
      GParamSpecDouble *p = G_PARAM_SPEC_DOUBLE (pspec);
      guint i;
      NotifyData data;
      gulong id;
      double value;
      double new_value;
      int current_count;
      double delta;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      /* don't check redundant notifications */
      g_object_get (instance, pspec->name, &value, NULL);

      if (p->maximum > 100 || p->minimum < -100)
        delta = M_PI;
      else
        delta = (p->maximum - p->minimum) / 10.0;

      new_value = p->minimum;
      for (i = 0; i < 10; i++)
        {
          new_value += delta;

          if (fabs (value - new_value) < p->epsilon)
            continue;

          if (new_value > p->maximum)
            break;

          current_count = data.count + 1;
          g_object_set (instance, pspec->name, new_value, NULL);
          assert_notifies (instance, pspec->name, data.count, current_count);
        }

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_FLOAT)
    {
      GParamSpecFloat *p = G_PARAM_SPEC_FLOAT (pspec);
      guint i;
      NotifyData data;
      gulong id;
      float value;
      float new_value;
      int current_count;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      /* don't check redundant notifications */
      g_object_get (instance, pspec->name, &value, NULL);

      new_value = p->minimum;
      for (i = 0; i < 10; i++)
        {
          if (fabs (value - new_value) < p->epsilon)
            continue;

          current_count = data.count + 1;
          new_value += (p->maximum - p->minimum) / 10.0;

          if (new_value > p->maximum)
            break;

          g_object_set (instance, pspec->name, new_value, NULL);
          assert_notifies (instance, pspec->name, data.count, current_count);
        }

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == G_TYPE_LIST_MODEL)
    {
      NotifyData data;
      gulong id;
      GListStore *value;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      value = g_list_store_new (GTK_TYPE_WIDGET);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, NULL, NULL);
      assert_notifies (instance, pspec->name, data.count, 2);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 3);

      g_object_unref (value);

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == GTK_TYPE_ADJUSTMENT)
    {
      NotifyData data;
      gulong id;
      GtkAdjustment *value;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      value = gtk_adjustment_new (100, 0, 200, 1, 1, 10);
      g_object_ref_sink (value);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, NULL, NULL);
      assert_notifies (instance, pspec->name, data.count, 2);

      g_object_unref (value);

      g_signal_handler_disconnect (instance, id);
    }
  else if (pspec->value_type == GTK_TYPE_WIDGET)
    {
      NotifyData data;
      gulong id;
      GtkWidget *value;

      data.name = pspec->name;
      data.count = 0;
      id = g_signal_connect (instance, "notify", G_CALLBACK (count_notify), &data);

      value = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      g_object_ref_sink (value);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, value, NULL);
      assert_notifies (instance, pspec->name, data.count, 1);

      g_object_set (instance, pspec->name, NULL, NULL);
      assert_notifies (instance, pspec->name, data.count, 2);

      g_object_unref (value);

      g_signal_handler_disconnect (instance, id);
    }
  else
    {
      if (g_test_verbose ())
        g_print ("Skipping property %s.%s of type %s\n", g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->value_type));
    }
}

static void
test_type (gconstpointer data)
{
  GObjectClass *klass;
  GObject *instance;
  GParamSpec **pspecs;
  guint n_pspecs, i;
  GType type;
  GdkDisplay *display;

  type = * (GType *) data;

  display = gdk_display_get_default ();

  if (!G_TYPE_IS_CLASSED (type))
    return;

  if (G_TYPE_IS_ABSTRACT (type))
    return;

  if (!g_type_is_a (type, G_TYPE_OBJECT))
    return;

  /* non-GTK */
  if (g_str_equal (g_type_name (type), "GdkPixbufSimpleAnim"))
    return;

  /* These can't be freely constructed/destroyed */
  if (g_type_is_a (type, GTK_TYPE_APPLICATION) ||
      g_type_is_a (type, GDK_TYPE_PIXBUF_LOADER) ||
      g_type_is_a (type, GTK_TYPE_LAYOUT_CHILD) ||
#ifdef G_OS_UNIX
      g_type_is_a (type, GTK_TYPE_PRINT_JOB) ||
#endif
      g_type_is_a (type, gdk_pixbuf_simple_anim_iter_get_type ()) ||
      g_str_equal (g_type_name (type), "GdkX11DeviceManagerXI2") ||
      g_str_equal (g_type_name (type), "GdkX11DeviceManagerCore") ||
      g_str_equal (g_type_name (type), "GdkX11Display") ||
      g_str_equal (g_type_name (type), "GdkX11Screen") ||
      g_str_equal (g_type_name (type), "GdkX11GLContext"))
    return;

  /* These leak their GDBusConnections */
  if (g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_DIALOG) ||
      g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_WIDGET) ||
      g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_NATIVE))
    return;

  if (g_type_is_a (type, GTK_TYPE_STACK_PAGE))
    return;

  /* these assert in constructed */
 if (g_type_is_a (type, GTK_TYPE_ALTERNATIVE_TRIGGER) ||
     g_type_is_a (type, GTK_TYPE_SIGNAL_ACTION) ||
     g_type_is_a (type, GTK_TYPE_NAMED_ACTION))
    return;

  /* needs a surface */
  if (g_type_is_a (type, GTK_TYPE_DRAG_ICON))
    return;

  /* Needs debugging */
  if (g_type_is_a (type, GTK_TYPE_SHORTCUTS_WINDOW))
    return;

  klass = g_type_class_ref (type);

  if (g_type_is_a (type, GTK_TYPE_SETTINGS))
    instance = G_OBJECT (g_object_ref (gtk_settings_get_default ()));
  else if (g_type_is_a (type, GDK_TYPE_SURFACE))
    {
      instance = G_OBJECT (g_object_ref (gdk_surface_new_toplevel (display)));
    }
  else if (g_str_equal (g_type_name (type), "GdkX11Cursor"))
    instance = g_object_new (type, "display", display, NULL);
  else if (g_str_equal (g_type_name (type), "GdkClipboard"))
    instance = g_object_new (type, "display", display, NULL);
  else if (g_str_equal (g_type_name (type), "GdkDrag"))
    {
      GdkContentFormats *formats = gdk_content_formats_new_for_gtype (G_TYPE_STRING);
      instance = g_object_new (type,
                               "device", gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())),
                               "formats", formats,
                               NULL);
      gdk_content_formats_unref (formats);
    }
  else if (g_str_equal (g_type_name (type), "GdkDrop"))
    {
      GdkContentFormats *formats = gdk_content_formats_new_for_gtype (G_TYPE_STRING);
      instance = g_object_new (type,
                               "device", gdk_seat_get_pointer (gdk_display_get_default_seat (gdk_display_get_default ())),
                               "formats", formats,
                               NULL);
      gdk_content_formats_unref (formats);
    }
  else if (g_type_is_a (type, GSK_TYPE_GL_SHADER))
    {
      GBytes *bytes = g_bytes_new_static ("", 0);
      instance = g_object_new (type, "source", bytes, NULL);
      g_bytes_unref (bytes);
    }
  else if (g_type_is_a (type, GTK_TYPE_FILTER_LIST_MODEL) ||
           g_type_is_a (type, GTK_TYPE_NO_SELECTION) ||
           g_type_is_a (type, GTK_TYPE_SINGLE_SELECTION) ||
           g_type_is_a (type, GTK_TYPE_MULTI_SELECTION))
    {
      GListStore *list_store = g_list_store_new (G_TYPE_OBJECT);
      instance = g_object_new (type,
                               "model", list_store,
                               NULL);
      g_object_unref (list_store);
    }
  else if (g_type_is_a (type, GTK_TYPE_CALENDAR))
    {
      /* avoid day 30 and 31, since they don't exist in February */
      instance = g_object_new (type,
                               "year", 1984,
                               "day", 05,
                               "month", 10,
                               NULL);
    }
  /* special casing for singletons */
  else if (g_type_is_a (type, GTK_TYPE_NEVER_TRIGGER))
    instance = (GObject *) g_object_ref (gtk_never_trigger_get ());
  else if (g_type_is_a (type, GTK_TYPE_NOTHING_ACTION))
    instance = (GObject *) g_object_ref (gtk_nothing_action_get ());
  else if (g_type_is_a (type, GTK_TYPE_ACTIVATE_ACTION))
    instance = (GObject *) g_object_ref (gtk_activate_action_get ());
  else if (g_type_is_a (type, GTK_TYPE_MNEMONIC_ACTION))
    instance = (GObject *) g_object_ref (gtk_mnemonic_action_get ());
  else
    instance = g_object_new (type, NULL);

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];

      if ((pspec->flags & G_PARAM_READABLE) == 0)
        continue;

      if ((pspec->flags & G_PARAM_WRITABLE) == 0)
        continue;

      if ((pspec->flags & G_PARAM_CONSTRUCT_ONLY) != 0)
        continue;

      /* non-GTK */
      if (g_str_equal (g_type_name (pspec->owner_type), "GdkPixbufSimpleAnim") ||
          g_str_equal (g_type_name (pspec->owner_type), "GMountOperation"))
        continue;

      /* set properties are best skipped */
      if (pspec->value_type == G_TYPE_BOOLEAN &&
          g_str_has_suffix (pspec->name, "-set"))
        continue;

      /* These are special */
      if (g_type_is_a (pspec->owner_type, GTK_TYPE_WIDGET) &&
          (g_str_equal (pspec->name, "has-focus") ||
           g_str_equal (pspec->name, "has-default") ||
           g_str_equal (pspec->name, "focus-widget") ||
           g_str_equal (pspec->name, "is-focus") ||
           g_str_equal (pspec->name, "hexpand") ||
           g_str_equal (pspec->name, "vexpand") ||
           g_str_equal (pspec->name, "visible")))
        continue;

      if (g_type_is_a (type, GTK_TYPE_ACCESSIBLE) &&
          g_str_equal (pspec->name, "accessible-role"))
        continue;

      if (pspec->owner_type == GTK_TYPE_ENTRY &&
          g_str_equal (pspec->name, "im-module"))
        continue;

      if (type == GTK_TYPE_SETTINGS)
        continue;

      if (g_type_is_a (pspec->owner_type, GTK_TYPE_ENTRY_COMPLETION) &&
          g_str_equal (pspec->name, "text-column"))
        continue;

      if (g_type_is_a (pspec->owner_type, GTK_TYPE_COLOR_CHOOSER) &&
          g_str_equal (pspec->name, "show-editor"))
        continue;

      if (g_type_is_a (pspec->owner_type, GTK_TYPE_NOTEBOOK) &&
          g_str_equal (pspec->name, "page"))
        continue;

      /* Too many special cases involving -set properties */
      if (pspec->owner_type == GTK_TYPE_CELL_RENDERER_TEXT ||
          pspec->owner_type == GTK_TYPE_TEXT_TAG)
        continue;

      /* Most things assume a model is set */
      if (pspec->owner_type == GTK_TYPE_COMBO_BOX)
        continue;

      /* Can only be set on unmapped windows */
      if (pspec->owner_type == GTK_TYPE_WINDOW &&
          g_str_equal (pspec->name, "type-hint"))
        continue;

      if (pspec->owner_type == GTK_TYPE_ENTRY_COMPLETION &&
          g_str_equal (pspec->name, "text-column"))
        continue;

      if (pspec->owner_type == GTK_TYPE_PRINT_OPERATION &&
          (g_str_equal (pspec->name, "current-page") ||
           g_str_equal (pspec->name, "n-pages")))
        continue;

      if (pspec->owner_type == GTK_TYPE_RANGE &&
          g_str_equal (pspec->name, "fill-level"))
        continue;

      if (pspec->owner_type == GTK_TYPE_SPIN_BUTTON &&
          g_str_equal (pspec->name, "value"))
        continue;

      if (pspec->owner_type == GTK_TYPE_STACK &&
          (g_str_equal (pspec->name, "visible-child-name") ||
           g_str_equal (pspec->name, "visible-child")))
        continue;

      if (pspec->owner_type == GTK_TYPE_STACK_PAGE && /* Can't change position without a stack */
          g_str_equal (pspec->name, "position"))
        continue;

      /* Can't realize a popover without a parent */
      if (g_type_is_a (type, GTK_TYPE_POPOVER) &&
          g_str_equal (pspec->name, "visible"))
        continue;

      if (pspec->owner_type == GTK_TYPE_POPOVER_MENU &&
          g_str_equal (pspec->name, "visible-submenu"))
        continue;

      if (pspec->owner_type == GTK_TYPE_TEXT_VIEW &&
          g_str_equal (pspec->name, "im-module"))
        continue;

      if (pspec->owner_type == GTK_TYPE_TREE_SELECTION &&
          g_str_equal (pspec->name, "mode")) /* requires a treeview */
        continue;

      if (pspec->owner_type == GTK_TYPE_TREE_VIEW &&
          g_str_equal (pspec->name, "headers-clickable")) /* requires columns */
        continue;

      /* This one has a special-purpose default value */
      if (g_type_is_a (type, GTK_TYPE_DIALOG) &&
          g_str_equal (pspec->name, "use-header-bar"))
        continue;

      if (g_type_is_a (type, GTK_TYPE_ASSISTANT) &&
          g_str_equal (pspec->name, "use-header-bar"))
        continue;

      if (g_type_is_a (type, GTK_TYPE_SHORTCUTS_SHORTCUT) &&
          g_str_equal (pspec->name, "accelerator"))
        continue;

      if (g_type_is_a (type, GTK_TYPE_SHORTCUT_LABEL) &&
          g_str_equal (pspec->name, "accelerator"))
        continue;

      if (g_type_is_a (type, GTK_TYPE_FONT_CHOOSER) &&
          g_str_equal (pspec->name, "font"))
        continue;

      /* these depend on the min-content- properties in a way that breaks our test */
      if (g_type_is_a (type, GTK_TYPE_SCROLLED_WINDOW) &&
          (g_str_equal (pspec->name, "max-content-width") ||
           g_str_equal (pspec->name, "max-content-height")))
        continue;

      /* expanding only works if rows are expandable */
      if (g_type_is_a (type, GTK_TYPE_TREE_LIST_ROW) &&
          g_str_equal (pspec->name, "expanded"))
        continue;

      /* can't select items without an underlying, populated model */
      if (g_type_is_a (type, GTK_TYPE_SINGLE_SELECTION) &&
          (g_str_equal (pspec->name, "selected") ||
           g_str_equal (pspec->name, "selected-item")))
        continue;

      /* can't select items without an underlying, populated model */
      if (g_type_is_a (type, GTK_TYPE_DROP_DOWN) &&
          g_str_equal (pspec->name, "selected"))
        continue;

       /* can't set position without a notebook */
      if (g_type_is_a (type, GTK_TYPE_NOTEBOOK_PAGE) &&
          g_str_equal (pspec->name, "position"))
        continue;

      if (pspec->owner_type == GTK_TYPE_TREE_VIEW_COLUMN &&
          g_str_equal (pspec->name, "widget"))
        continue;

      /* Interface does not do explicit notify, so we can't fix it */
      if (pspec->owner_type == GTK_TYPE_SCROLLABLE &&
          (g_str_equal (pspec->name, "hadjustment") ||
           g_str_equal (pspec->name, "vadjustment")))
        continue;

      /* deprecated, not getting fixed */
      if (pspec->owner_type == GTK_TYPE_CELL_RENDERER_SPIN &&
          g_str_equal (pspec->name, "adjustment"))
        continue;

      if (g_test_verbose ())
        g_print ("Property %s.%s\n", g_type_name (pspec->owner_type), pspec->name);

      check_property (instance, pspec);
    }
  g_free (pspecs);

  if (g_type_is_a (type, GDK_TYPE_SURFACE))
    gdk_surface_destroy (GDK_SURFACE (instance));
  else
    g_object_unref (instance);

  g_type_class_unref (klass);
}

int
main (int argc, char **argv)
{
  const GType *otypes;
  guint i;
  int result;

  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types();

  otypes = gtk_test_list_all_types (NULL);
  for (i = 0; otypes[i]; i++)
    {
      char *testname;

      testname = g_strdup_printf ("/Notification/%s", g_type_name (otypes[i]));
      g_test_add_data_func (testname, &otypes[i], test_type);
      g_free (testname);
    }

  result = g_test_run ();

  return result;
}
