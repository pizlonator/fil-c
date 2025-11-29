/* Gtk+ default value tests
 * Copyright (C) 2007 Christian Persch
 *               2007 Johan Dahlin
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

#include <string.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

static void
check_property (const char *output,
	        GParamSpec *pspec,
		GValue *value)
{
  GValue default_value = G_VALUE_INIT;
  char *v, *dv, *msg;

  if (g_param_value_defaults (pspec, value))
      return;

  g_value_init (&default_value, G_PARAM_SPEC_VALUE_TYPE (pspec));
  g_param_value_set_default (pspec, &default_value);

  v = g_strdup_value_contents (value);
  dv = g_strdup_value_contents (&default_value);

  msg = g_strdup_printf ("%s %s.%s: %s != %s\n",
			 output,
			 g_type_name (pspec->owner_type),
			 pspec->name,
			 dv, v);
  g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__,
		       G_STRFUNC, msg);
  g_free (msg);

  g_free (v);
  g_free (dv);
  g_value_unset (&default_value);
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

  /* These can't be freely constructed/destroyed */
  if (g_type_is_a (type, GTK_TYPE_APPLICATION) ||
      g_type_is_a (type, GDK_TYPE_PIXBUF_LOADER) ||
      g_type_is_a (type, GTK_TYPE_LAYOUT_CHILD) ||
      g_type_is_a (type, GTK_TYPE_STACK_PAGE) ||
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

  /* This throws a critical when the connection is dropped */
  if (g_type_is_a (type, GTK_TYPE_APP_CHOOSER_DIALOG))
    return;

  /* These leak their GDBusConnections */
  if (g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_DIALOG) ||
      g_type_is_a (type, GTK_TYPE_FILE_CHOOSER_WIDGET) ||
      g_str_equal (g_type_name (type), "GtkPlacesSidebar"))
    return;
 
  if (g_type_is_a (type, GTK_TYPE_SHORTCUT_TRIGGER) ||
      g_type_is_a (type, GTK_TYPE_SHORTCUT_ACTION))
    return;

  klass = g_type_class_ref (type);

  if (g_type_is_a (type, GTK_TYPE_SETTINGS))
    instance = G_OBJECT (g_object_ref (gtk_settings_get_default ()));
  else if (g_type_is_a (type, GDK_TYPE_SURFACE))
    {
      instance = G_OBJECT (g_object_ref (gdk_surface_new_toplevel (display)));
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
  else if (g_type_is_a (type, GSK_TYPE_GL_SHADER))
    {
      GBytes *bytes = g_bytes_new_static ("", 0);
      instance = g_object_new (type, "source", bytes, NULL);
      g_bytes_unref (bytes);
    }
  else if (g_type_is_a (type, GDK_TYPE_CLIPBOARD) ||
           g_str_equal (g_type_name (type), "GdkX11Cursor"))
    instance = g_object_new (type, "display", display, NULL);
  else
    instance = g_object_new (type, NULL);

  if (g_type_is_a (type, G_TYPE_INITIALLY_UNOWNED))
    g_object_ref_sink (instance);

  pspecs = g_object_class_list_properties (klass, &n_pspecs);
  for (i = 0; i < n_pspecs; ++i)
    {
      GParamSpec *pspec = pspecs[i];
      GValue value = G_VALUE_INIT;
      gboolean check = TRUE;

      if (pspec->owner_type != type)
	continue;

      if ((pspec->flags & G_PARAM_READABLE) == 0)
	continue;

      /* This is set by the treelistmodel, plain 
       * g_object_new() will crash here */
      if (g_type_is_a (type, GTK_TYPE_TREE_LIST_ROW) &&
	  (strcmp (pspec->name, "item") == 0))
	continue;

      /* This is set via class_init, and we have a11y tests to verify it */
      if (g_type_is_a (type, GTK_TYPE_ACCESSIBLE) &&
          strcmp (pspec->name, "accessible-role") == 0)
        check = FALSE;

      /* This is set via construct property */
      if (g_type_is_a (type, GTK_TYPE_BUILDER) &&
          strcmp (pspec->name, "scope") == 0)
        check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_CLIPBOARD) &&
	  strcmp (pspec->name, "display") == 0)
	check = FALSE;

      /* These are set in init() */
      if ((g_type_is_a (type, GDK_TYPE_CLIPBOARD) ||
           g_type_is_a (type, GDK_TYPE_CONTENT_PROVIDER) ||
           g_type_is_a (type, GTK_TYPE_DROP_TARGET)) &&
	  strcmp (pspec->name, "formats") == 0)
	check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_CONTENT_PROVIDER) &&
	  strcmp (pspec->name, "storable-formats") == 0)
	check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_DMABUF_TEXTURE_BUILDER) &&
	  strcmp (pspec->name, "display") == 0)
	check = FALSE;

      /* set in the constructor */
      if (g_type_is_a (type, GSK_TYPE_GL_SHADER) &&
	  strcmp (pspec->name, "source") == 0)
	check = FALSE;

      /* This one has a special-purpose default value */
      if (g_type_is_a (type, GTK_TYPE_DIALOG) &&
	  (strcmp (pspec->name, "use-header-bar") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_ASSISTANT) &&
	  (strcmp (pspec->name, "use-header-bar") == 0 ||
           strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_STACK) &&
	  (strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_NOTEBOOK) &&
	  (strcmp (pspec->name, "pages") == 0)) /* pages always gets a non-NULL value */
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_COMBO_BOX) &&
          (strcmp (pspec->name, "child") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_POPOVER) &&
	  (strcmp (pspec->name, "pointing-to") == 0))
	check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_DISPLAY_MANAGER) &&
	  (strcmp (pspec->name, "default-display") == 0))
	check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_DISPLAY) &&
	  (strcmp (pspec->name, "dmabuf-formats") == 0))
	check = FALSE;

      if (g_type_is_a (type, GDK_TYPE_MONITOR) &&
          (strcmp (pspec->name, "geometry") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_ABOUT_DIALOG) &&
	  (strcmp (pspec->name, "program-name") == 0))
	check = FALSE;

      /* These are set to the current date */
      if (g_type_is_a (type, GTK_TYPE_CALENDAR) &&
	  (strcmp (pspec->name, "year") == 0 ||
	   strcmp (pspec->name, "month") == 0 ||
	   strcmp (pspec->name, "day") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_CELL_AREA_CONTEXT) &&
	  (strcmp (pspec->name, "minimum-width") == 0 ||
	   strcmp (pspec->name, "minimum-height") == 0 ||
	   strcmp (pspec->name, "natural-width") == 0 ||
	   strcmp (pspec->name, "natural-height") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_CELL_RENDERER_TEXT) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "background-rgba") == 0 ||
	   strcmp (pspec->name, "foreground-rgba") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_CELL_VIEW) &&
	  (strcmp (pspec->name, "background-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "foreground-rgba") == 0 ||
	   strcmp (pspec->name, "background-rgba") == 0 ||
           strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_COLOR_BUTTON) &&
	  (strcmp (pspec->name, "color") == 0 ||
	   strcmp (pspec->name, "rgba") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_COLUMN_VIEW) &&
          (strcmp (pspec->name, "columns") == 0 ||
	   strcmp (pspec->name, "sorter") == 0))
	check = FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_COMBO_BOX) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	check = FALSE;

G_GNUC_END_IGNORE_DEPRECATIONS

      /* Default invisible char is determined at runtime,
       * and buffer gets created on-demand
       */
      if (g_type_is_a (type, GTK_TYPE_ENTRY) &&
	  (strcmp (pspec->name, "invisible-char") == 0 ||
           strcmp (pspec->name, "buffer") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TEXT) &&
	  (strcmp (pspec->name, "invisible-char") == 0 ||
           strcmp (pspec->name, "buffer") == 0))
	check = FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_ENTRY_COMPLETION) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	check = FALSE;

G_GNUC_END_IGNORE_DEPRECATIONS

      if ((g_type_is_a (type, GTK_TYPE_FILTER_LIST_MODEL) ||
           g_type_is_a (type, GTK_TYPE_NO_SELECTION) ||
           g_type_is_a (type, GTK_TYPE_SINGLE_SELECTION) ||
           g_type_is_a (type, GTK_TYPE_MULTI_SELECTION)) &&
          strcmp (pspec->name, "model") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TREE_LIST_MODEL) &&
	  (strcmp (pspec->name, "item-type") == 0)) /* might be a treelistrow */
	check = FALSE;

      /* This is set in init() */
      if (g_type_is_a (type, GTK_TYPE_FONT_CHOOSER_WIDGET) &&
          strcmp (pspec->name, "tweak-action") == 0)
        check = FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_ICON_VIEW) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	check = FALSE;

G_GNUC_END_IGNORE_DEPRECATIONS

      if (g_type_is_a (type, GTK_TYPE_MESSAGE_DIALOG) &&
          (strcmp (pspec->name, "image") == 0 ||
           strcmp (pspec->name, "message-area") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_PANED) &&
	  strcmp (pspec->name, "max-position") == 0)
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_PRINT_OPERATION) &&
	  strcmp (pspec->name, "job-name") == 0)
	check = FALSE;

#ifdef G_OS_UNIX
      if (g_type_is_a (type, GTK_TYPE_PRINT_UNIX_DIALOG) &&
	  (strcmp (pspec->name, "page-setup") == 0 ||
	   strcmp (pspec->name, "print-settings") == 0))
	check = FALSE;
#endif

      if (g_type_is_a (type, GTK_TYPE_PROGRESS_BAR) &&
          strcmp (pspec->name, "adjustment") == 0)
        check = FALSE;

      /* filename value depends on $HOME */
      if (g_type_is_a (type, GTK_TYPE_RECENT_MANAGER) &&
          (strcmp (pspec->name, "filename") == 0 ||
	   strcmp (pspec->name, "size") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_SCALE_BUTTON) &&
          strcmp (pspec->name, "adjustment") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_SCROLLED_WINDOW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_SETTINGS))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_SHORTCUT) &&
	  (strcmp (pspec->name, "action") == 0 ||
           strcmp (pspec->name, "trigger") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_SPIN_BUTTON) &&
          (strcmp (pspec->name, "adjustment") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_STYLE_CONTEXT) &&
           strcmp (pspec->name, "display") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TEXT_BUFFER) &&
          (strcmp (pspec->name, "tag-table") == 0 ||
           strcmp (pspec->name, "copy-target-list") == 0 ||
           strcmp (pspec->name, "paste-target-list") == 0))
        check = FALSE;

      /* language depends on the current locale */
      if (g_type_is_a (type, GTK_TYPE_TEXT_TAG) &&
          (strcmp (pspec->name, "background-gdk") == 0 ||
           strcmp (pspec->name, "foreground-gdk") == 0 ||
	   strcmp (pspec->name, "language") == 0 ||
	   strcmp (pspec->name, "font") == 0 ||
	   strcmp (pspec->name, "font-desc") == 0))
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TEXT_VIEW) &&
          strcmp (pspec->name, "buffer") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_TREE_VIEW_COLUMN) &&
	  (strcmp (pspec->name, "cell-area") == 0 ||
           strcmp (pspec->name, "cell-area-context") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_VIEWPORT) &&
	  (strcmp (pspec->name, "hadjustment") == 0 ||
           strcmp (pspec->name, "vadjustment") == 0))
	check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_WIDGET) &&
          (strcmp (pspec->name, "name") == 0 ||
	   strcmp (pspec->name, "display") == 0 ||
	   strcmp (pspec->name, "style") == 0))
	check = FALSE;

      /* resize-grip-visible is determined at runtime */
      if (g_type_is_a (type, GTK_TYPE_WINDOW) &&
          strcmp (pspec->name, "resize-grip-visible") == 0)
        check = FALSE;

      /* show-desktop depends on desktop environment */
      if (g_str_equal (g_type_name (type), "GtkPlacesSidebar") &&
          strcmp (pspec->name, "show-desktop") == 0)
        check = FALSE;

      /* GtkRange constructs an adjustment on its own if NULL is set and
       * the property is a CONSTRUCT one, so the returned value is never NULL. */
      if (g_type_is_a (type, GTK_TYPE_RANGE) &&
          strcmp (pspec->name, "adjustment") == 0)
        check = FALSE;

      /* ... and GtkScrollbar wraps that property. */
      if (g_type_is_a (type, GTK_TYPE_SCROLLBAR) &&
          strcmp (pspec->name, "adjustment") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_DROP_DOWN) &&
          strcmp (pspec->name, "factory") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_BOOKMARK_LIST) &&
          (strcmp (pspec->name, "filename") == 0 ||
           strcmp (pspec->name, "loading") == 0))
        check = FALSE;

      /* All the icontheme properties depend on the environment */
      if (g_type_is_a (type, GTK_TYPE_ICON_THEME))
        check = FALSE;

      /* Non-NULL default */
      if (g_type_is_a (type, GTK_TYPE_COLOR_DIALOG_BUTTON) &&
          strcmp (pspec->name, "rgba") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_FONT_DIALOG_BUTTON) &&
          strcmp (pspec->name, "font-desc") == 0)
        check = FALSE;

      if (g_type_is_a (type, GTK_TYPE_FONT_DIALOG) &&
          strcmp (pspec->name, "language") == 0)
        check = FALSE;

      if (g_test_verbose ())
        {
          g_print ("Property %s:%s%s\n",
                   g_type_name (pspec->owner_type),
                   pspec->name,
                   check ? "" : " (no check)");
        }

      g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
      g_object_get_property (instance, pspec->name, &value);
      if (check)
        check_property ("Property", pspec, &value);
      g_value_unset (&value);
    }
  g_free (pspecs);

  if (g_type_is_a (type, GDK_TYPE_SURFACE))
    gdk_surface_destroy (GDK_SURFACE (instance));
  else
    g_object_unref (instance);

  g_type_class_unref (klass);
}

static gboolean
dbind_warning_handler (const char     *log_domain,
                       GLogLevelFlags  log_level,
                       const char     *message,
                       gpointer        user_data)
{
  if (strcmp (log_domain, "dbind") == 0 &&
      log_level == (G_LOG_LEVEL_WARNING|G_LOG_FLAG_FATAL))
    return FALSE;

  return TRUE;
}

int
main (int argc, char **argv)
{
  const GType *otypes;
  guint i;
  const char *display, *x_r_d;

  /* These must be set before gtk_test_init */
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("G_ENABLE_DIAGNOSTIC", "0", TRUE);

  /* g_test_dbus_up() helpfully clears these, so we have to re-set it */
  display = g_getenv ("DISPLAY");
  x_r_d = g_getenv ("XDG_RUNTIME_DIR");

  if (display)
    g_setenv ("DISPLAY", display, TRUE);
  if (x_r_d)
    g_setenv ("XDG_RUNTIME_DIR", x_r_d, TRUE);

  g_test_log_set_fatal_handler (dbind_warning_handler, NULL);

  gtk_test_init (&argc, &argv);
  gtk_test_register_all_types();

  otypes = gtk_test_list_all_types (NULL);
  for (i = 0; otypes[i]; i++)
    {
      char *testname;

      if (otypes[i] == GTK_TYPE_FILE_CHOOSER_NATIVE)
        continue;

      testname = g_strdup_printf ("/Default Values/%s",
				  g_type_name (otypes[i]));
      g_test_add_data_func (testname,
                            &otypes[i],
			    test_type);
      g_free (testname);
    }

  return g_test_run();
}
