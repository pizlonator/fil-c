/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "visual.h"

#include "fpsoverlay.h"
#include "a11yoverlay.h"
#include "updatesoverlay.h"
#include "layoutoverlay.h"
#include "focusoverlay.h"
#include "baselineoverlay.h"
#include "subsurfaceoverlay.h"
#include "window.h"

#include "gtkadjustment.h"
#include "gtkbox.h"
#include "gtkbutton.h"
#include "gtkdropdown.h"
#include "gtkcssproviderprivate.h"
#include "gtkdebug.h"
#include "gtkprivate.h"
#include "gtksettings.h"
#include "gtkswitch.h"
#include "gtkscale.h"
#include "gtkwindow.h"
#include "gtklistbox.h"
#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gtknative.h"
#include "gtkbinlayout.h"
#include "gtkeditable.h"
#include "gtkentry.h"
#include "gtkstringlist.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#endif
#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#endif
#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#endif

#include "gdk/gdkdebugprivate.h"

#define EPSILON               1e-10

struct _GtkInspectorVisual
{
  GtkWidget widget;

  GtkWidget *swin;
  GtkWidget *box;

  GtkWidget *visual_box;
  GtkWidget *theme_combo;
  GtkWidget *dark_switch;
  GtkWidget *icon_combo;
  GtkWidget *cursor_combo;
  GtkWidget *cursor_size_spin;
  GtkWidget *direction_combo;
  GtkWidget *font_button;
  GtkWidget *font_aa_switch;
  GtkWidget *font_hinting_combo;
  GtkWidget *metrics_hinting_switch;
  GtkWidget *animation_switch;
  GtkWidget *font_scale_entry;
  GtkAdjustment *font_scale_adjustment;
  GtkAdjustment *slowdown_adjustment;
  GtkWidget *slowdown_entry;
  GtkAdjustment *cursor_size_adjustment;

  GtkWidget *debug_box;
  GtkWidget *fps_switch;
  GtkWidget *updates_switch;
  GtkWidget *cairo_switch;
  GtkWidget *baselines_switch;
  GtkWidget *layout_switch;
  GtkWidget *focus_switch;
  GtkWidget *a11y_switch;
  GtkWidget *subsurface_switch;

  GtkInspectorOverlay *fps_overlay;
  GtkInspectorOverlay *updates_overlay;
  GtkInspectorOverlay *layout_overlay;
  GtkInspectorOverlay *focus_overlay;
  GtkInspectorOverlay *baseline_overlay;
  GtkInspectorOverlay *a11y_overlay;
  GtkInspectorOverlay *subsurface_overlay;

  GdkDisplay *display;
  GtkSettings *settings;
};

typedef struct _GtkInspectorVisualClass
{
  GtkWidgetClass parent_class;
} GtkInspectorVisualClass;

G_DEFINE_TYPE (GtkInspectorVisual, gtk_inspector_visual, GTK_TYPE_WIDGET)

static void
fix_direction_recurse (GtkWidget        *widget,
                       GtkTextDirection  dir)
{
  GtkWidget *child;

  g_object_ref (widget);

  gtk_widget_set_direction (widget, dir);
  for (child = gtk_widget_get_first_child (widget);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
     {
        fix_direction_recurse (child, dir);
     }

  g_object_unref (widget);
}

static GtkTextDirection initial_direction;

static void
fix_direction (GtkWidget *iw)
{
  fix_direction_recurse (iw, initial_direction);
}

static void
direction_changed (GtkDropDown *combo)
{
  GtkWidget *iw;
  guint selected;

  iw = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (combo)));
  if (iw)
    fix_direction (iw);

  selected = gtk_drop_down_get_selected (combo);

  if (selected == 0)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_LTR);
  else
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
}

static void
init_direction (GtkInspectorVisual *vis)
{
  initial_direction = gtk_widget_get_default_direction ();
  if (initial_direction == GTK_TEXT_DIR_LTR)
    gtk_drop_down_set_selected (GTK_DROP_DOWN (vis->direction_combo), 0);
  else
    gtk_drop_down_set_selected (GTK_DROP_DOWN (vis->direction_combo), 1);
}

static void
redraw_everything (void)
{
  GList *toplevels;
  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc) gtk_widget_queue_draw, NULL);
  g_list_free (toplevels);
}

static double
get_dpi_ratio (GtkInspectorVisual *vis)
{
#ifdef GDK_WINDOWING_MACOS
  if (GDK_IS_MACOS_DISPLAY (vis->display))
    return 72.0 * 1024.0;
#endif

  return 96.0 * 1024.0;
}

static double
get_font_scale (GtkInspectorVisual *vis)
{
  int dpi_int;

  g_object_get (vis->settings, "gtk-xft-dpi", &dpi_int, NULL);

  return dpi_int / get_dpi_ratio (vis);
}

static void
update_font_scale (GtkInspectorVisual *vis,
                   double              factor,
                   gboolean            update_adjustment,
                   gboolean            update_entry,
                   gboolean            write_back)
{
  if (write_back)
    g_object_set (vis->settings, "gtk-xft-dpi", (int)(factor * get_dpi_ratio (vis)), NULL);

  if (update_adjustment)
    gtk_adjustment_set_value (vis->font_scale_adjustment, factor);

  if (update_entry)
    {
      char *str = g_strdup_printf ("%0.2f", factor);

      gtk_editable_set_text (GTK_EDITABLE (vis->font_scale_entry), str);
      g_free (str);
    }
}

static void
font_scale_adjustment_changed (GtkAdjustment      *adjustment,
                               GtkInspectorVisual *vis)
{
  double factor;

  factor = gtk_adjustment_get_value (adjustment);
  update_font_scale (vis, factor, FALSE, TRUE, TRUE);
}

static gboolean
get_font_aa (GtkInspectorVisual *vis)
{
  int aa;

  g_object_get (vis->settings, "gtk-xft-antialias", &aa, NULL);

  return aa != 0;
}

static gboolean
get_metrics_hinting (GtkInspectorVisual *vis)
{
  gboolean hinting;

  g_object_get (vis->settings, "gtk-hint-font-metrics", &hinting, NULL);

  return hinting;
}

static unsigned int
get_font_hinting (GtkInspectorVisual *vis)
{
  int hinting;
  char *hint_style_str;
  unsigned int hint_style;

  g_object_get (vis->settings,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &hint_style_str,
                NULL);

  hint_style = 1;
  if (hinting == 0)
    {
      hint_style = 0;
    }
  else if (hint_style_str != NULL)
    {
      if (strcmp (hint_style_str, "hintnone") == 0)
        hint_style = 0;
      else if (strcmp (hint_style_str, "hintslight") == 0)
        hint_style = 1;
      else if (strcmp (hint_style_str, "hintmedium") == 0)
        hint_style = 2;
      else if (strcmp (hint_style_str, "hintfull") == 0)
        hint_style = 3;
    }

  if (hint_style_str != NULL)
    g_free (hint_style_str);

  return hint_style;
}

static void
update_font_hinting (GtkInspectorVisual *vis,
                     unsigned int        hint_style)
{
  const char *styles[] = { "hintnone", "hintslight", "hintmedium", "hintfull" };
  int hinting;
  const char *style;

  g_object_get (vis->settings,
                "gtk-xft-hinting", &hinting,
                "gtk-xft-hintstyle", &style,
                NULL);

  if (hinting != (hint_style != 0) || strcmp (style, styles[hint_style]) != 0)
    g_object_set (vis->settings,
                  "gtk-xft-hinting", hint_style != 0,
                  "gtk-xft-hintstyle", styles[hint_style],
                  NULL);
}

static void
font_aa_activate (GtkSwitch          *sw,
                  GParamSpec         *pspec,
                  GtkInspectorVisual *vis)
{
  int val, new_val;

  g_object_get (vis->settings, "gtk-xft-antialias", &val, NULL);

  new_val = gtk_switch_get_active (sw) ? 1 : 0;

  if (val != new_val)
    g_object_set (vis->settings, "gtk-xft-antialias", new_val, NULL);
}

static void
metrics_hinting_activate (GtkSwitch          *sw,
                          GParamSpec         *pspec,
                          GtkInspectorVisual *vis)
{
  gboolean val, new_val;

  g_object_get (vis->settings, "gtk-hint-font-metrics", &val, NULL);

  new_val = gtk_switch_get_active (sw);

  if (val != new_val)
    g_object_set (vis->settings, "gtk-hint-font-metrics", new_val, NULL);
}

static void
font_scale_entry_activated (GtkEntry           *entry,
                            GtkInspectorVisual *vis)
{
  double factor;
  char *err = NULL;

  factor = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    update_font_scale (vis, factor, TRUE, FALSE, TRUE);
}

static void
fps_activate (GtkSwitch          *sw,
              GParamSpec         *pspec,
              GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean fps;

  fps = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (fps)
    {
      if (vis->fps_overlay == NULL)
        {
          vis->fps_overlay = gtk_fps_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->fps_overlay);
          g_object_unref (vis->fps_overlay);
        }
    }
  else
    {
      if (vis->fps_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->fps_overlay);
          vis->fps_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
a11y_activate (GtkSwitch          *sw,
               GParamSpec         *pspec,
               GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean a11y;

  a11y = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (a11y)
    {
      if (vis->a11y_overlay == NULL)
        {
          vis->a11y_overlay = gtk_a11y_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->a11y_overlay);
          g_object_unref (vis->a11y_overlay);
        }
    }
  else
    {
      if (vis->a11y_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->a11y_overlay);
          vis->a11y_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
subsurface_activate (GtkSwitch          *sw,
                     GParamSpec         *pspec,
                     GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean subsurface;

  subsurface = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (subsurface)
    {
      if (vis->subsurface_overlay == NULL)
        {
          vis->subsurface_overlay = gtk_subsurface_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->subsurface_overlay);
          g_object_unref (vis->subsurface_overlay);
        }
    }
  else
    {
      if (vis->subsurface_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->subsurface_overlay);
          vis->subsurface_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
updates_activate (GtkSwitch          *sw,
                  GParamSpec         *pspec,
                  GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean updates;

  updates = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (updates)
    {
      if (vis->updates_overlay == NULL)
        {
          vis->updates_overlay = gtk_updates_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->updates_overlay);
          g_object_unref (vis->updates_overlay);
        }
    }
  else
    {
      if (vis->updates_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->updates_overlay);
          vis->updates_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
cairo_activate (GtkSwitch          *sw,
                GParamSpec         *pspec,
                GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean active;
  guint flags;
  GList *toplevels, *l;

  active = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  flags = gsk_get_debug_flags ();
  if (active)
    flags = flags | GSK_DEBUG_CAIRO;
  else
    flags = flags & ~GSK_DEBUG_CAIRO;
  gsk_set_debug_flags (flags);

  toplevels = gtk_window_list_toplevels ();
  for (l = toplevels; l; l = l->next)
    {
      GtkWidget *toplevel = l->data;
      GskRenderer *renderer;

      if ((GtkRoot *)toplevel == gtk_widget_get_root (GTK_WIDGET (sw))) /* skip the inspector */
        continue;

      renderer = gtk_native_get_renderer (GTK_NATIVE (toplevel));
      if (!renderer)
        continue;

      gsk_renderer_set_debug_flags (renderer, flags);
    }
  g_list_free (toplevels);

  redraw_everything ();
}

static void
baselines_activate (GtkSwitch          *sw,
                    GParamSpec         *pspec,
                    GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean baselines;

  baselines = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (baselines)
    {
      if (vis->baseline_overlay == NULL)
        {
          vis->baseline_overlay = gtk_baseline_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->baseline_overlay);
          g_object_unref (vis->baseline_overlay);
        }
    }
  else
    {
      if (vis->baseline_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->baseline_overlay);
          vis->baseline_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
layout_activate (GtkSwitch          *sw,
                 GParamSpec         *pspec,
                 GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean draw_layout;

  draw_layout = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (draw_layout)
    {
      if (vis->layout_overlay == NULL)
        {
          vis->layout_overlay = gtk_layout_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->layout_overlay);
          g_object_unref (vis->layout_overlay);
        }
    }
  else
    {
      if (vis->layout_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->layout_overlay);
          vis->layout_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
focus_activate (GtkSwitch          *sw,
                GParamSpec         *pspec,
                GtkInspectorVisual *vis)
{
  GtkInspectorWindow *iw;
  gboolean focus;

  focus = gtk_switch_get_active (sw);
  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));
  if (iw == NULL)
    return;

  if (focus)
    {
      if (vis->focus_overlay == NULL)
        {
          vis->focus_overlay = gtk_focus_overlay_new ();
          gtk_inspector_window_add_overlay (iw, vis->focus_overlay);
          g_object_unref (vis->focus_overlay);
        }
    }
  else
    {
      if (vis->focus_overlay != NULL)
        {
          gtk_inspector_window_remove_overlay (iw, vis->focus_overlay);
          vis->focus_overlay = NULL;
        }
    }

  redraw_everything ();
}

static void
fill_gtk (const char *path,
          GHashTable  *t)
{
  const char *dir_entry;
  GDir *dir = g_dir_open (path, 0, NULL);

  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      char *filename = g_build_filename (path, dir_entry, "gtk-4.0", "gtk.css", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

static char *
get_data_path (const char *subdir)
{
  char *base_datadir, *full_datadir;
#if defined (GDK_WINDOWING_WIN32) || defined (GDK_WINDOWING_MACOS)
  base_datadir = g_strdup (_gtk_get_datadir ());
#else
  base_datadir = g_strdup (GTK_DATADIR);
#endif
  full_datadir = g_build_filename (base_datadir, subdir, NULL);
  g_free (base_datadir);
  return full_datadir;
}

static gboolean
theme_to_pos (GBinding *binding,
              const GValue *from,
              GValue *to,
              gpointer user_data)
{
  GtkStringList *names = user_data;
  const char *theme = g_value_get_string (from);
  guint i, n;

  for (i = 0, n = g_list_model_get_n_items (G_LIST_MODEL (names)); i < n; i++)
    {
      const char *name = gtk_string_list_get_string (names, i);
      if (g_strcmp0 (name, theme) == 0)
        {
          g_value_set_uint (to, i);
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
pos_to_theme (GBinding *binding,
              const GValue *from,
              GValue *to,
              gpointer user_data)
{
  GtkStringList *names = user_data;
  int pos = g_value_get_uint (from);
  g_value_set_string (to, gtk_string_list_get_string (names, pos));
  return TRUE;
}

static void
init_theme (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  char *theme, *path;
  char **builtin_themes;
  GList *list, *l;
  GtkStringList *names;
  guint i;
  const char * const *dirs;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* Builtin themes */
  builtin_themes = g_resources_enumerate_children ("/org/gtk/libgtk/theme", 0, NULL);
  for (i = 0; builtin_themes[i] != NULL; i++)
    {
      if (g_str_has_suffix (builtin_themes[i], "/"))
        g_hash_table_add (t, g_strndup (builtin_themes[i], strlen (builtin_themes[i]) - 1));
    }
  g_strfreev (builtin_themes);

  path = _gtk_get_theme_dir ();
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  path = g_build_filename (g_get_home_dir (), ".themes", NULL);
  fill_gtk (path, t);
  g_free (path);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = g_build_filename (dirs[i], "themes", NULL);
      fill_gtk (path, t);
      g_free (path);
    }

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  names = gtk_string_list_new (NULL);
  for (l = list, i = 0; l; l = l->next, i++)
    gtk_string_list_append (names, (const char *)l->data);

  g_list_free (list);
  g_hash_table_destroy (t);

  gtk_drop_down_set_model (GTK_DROP_DOWN (vis->theme_combo), G_LIST_MODEL (names));

  g_object_bind_property_full (vis->settings, "gtk-theme-name",
                               vis->theme_combo, "selected",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               theme_to_pos, pos_to_theme, names, (GDestroyNotify)g_object_unref);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      GtkWidget *row;

      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->theme_combo, FALSE);
      row = gtk_widget_get_ancestor (vis->theme_combo, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Theme is hardcoded by GTK_THEME"));
    }
}

static void
init_dark (GtkInspectorVisual *vis)
{
  g_object_bind_property (vis->settings, "gtk-application-prefer-dark-theme",
                          vis->dark_switch, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

  if (g_getenv ("GTK_THEME") != NULL)
    {
      GtkWidget *row;

      /* theme is hardcoded, nothing we can do */
      gtk_widget_set_sensitive (vis->dark_switch, FALSE);
      row = gtk_widget_get_ancestor (vis->theme_combo, GTK_TYPE_LIST_BOX_ROW);
      gtk_widget_set_tooltip_text (row, _("Theme is hardcoded by GTK_THEME"));
    }
}

static void
fill_icons (const char *path,
            GHashTable  *t)
{
  const char *dir_entry;
  GDir *dir;

  dir = g_dir_open (path, 0, NULL);
  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      char *filename = g_build_filename (path, dir_entry, "index.theme", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) &&
          g_strcmp0 (dir_entry, "hicolor") != 0 &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

static void
init_icons (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  char *theme, *path;
  GList *list, *l;
  int i;
  GtkStringList *names;
  const char * const *dirs;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = get_data_path ("icons");
  fill_icons (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  fill_icons (path, t);
  g_free (path);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = g_build_filename (dirs[i], "icons", NULL);
      fill_icons (path, t);
      g_free (path);
    }

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  names = gtk_string_list_new (NULL);
  for (l = list, i = 0; l; l = l->next, i++)
    gtk_string_list_append (names, (const char *)l->data);

  g_hash_table_destroy (t);
  g_list_free (list);

  gtk_drop_down_set_model (GTK_DROP_DOWN (vis->icon_combo), G_LIST_MODEL (names));

  g_object_bind_property_full (vis->settings, "gtk-icon-theme-name",
                               vis->icon_combo, "selected",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               theme_to_pos, pos_to_theme, names, (GDestroyNotify)g_object_unref);
}

static void
fill_cursors (const char *path,
              GHashTable  *t)
{
  const char *dir_entry;
  GDir *dir;

  dir = g_dir_open (path, 0, NULL);
  if (!dir)
    return;

  while ((dir_entry = g_dir_read_name (dir)))
    {
      char *filename = g_build_filename (path, dir_entry, "cursors", NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_DIR) &&
          !g_hash_table_contains (t, dir_entry))
        g_hash_table_add (t, g_strdup (dir_entry));

      g_free (filename);
    }

  g_dir_close (dir);
}

static void
init_cursors (GtkInspectorVisual *vis)
{
  GHashTable *t;
  GHashTableIter iter;
  char *theme, *path;
  GList *list, *l;
  GtkStringList *names;
  int i;
  const char * const *dirs;

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  path = get_data_path ("icons");
  fill_cursors (path, t);
  g_free (path);

  path = g_build_filename (g_get_user_data_dir (), "icons", NULL);
  fill_cursors (path, t);
  g_free (path);

  dirs = g_get_system_data_dirs ();
  for (i = 0; dirs[i]; i++)
    {
      path = g_build_filename (dirs[i], "icons", NULL);
      fill_cursors (path, t);
      g_free (path);
    }

  list = NULL;
  g_hash_table_iter_init (&iter, t);
  while (g_hash_table_iter_next (&iter, (gpointer *)&theme, NULL))
    list = g_list_insert_sorted (list, theme, (GCompareFunc)strcmp);

  names = gtk_string_list_new (NULL);
  for (l = list, i = 0; l; l = l->next, i++)
    gtk_string_list_append (names, (const char *)l->data);

  g_hash_table_destroy (t);
  g_list_free (list);

  gtk_drop_down_set_model (GTK_DROP_DOWN (vis->cursor_combo), G_LIST_MODEL (names));

  g_object_bind_property_full (vis->settings, "gtk-cursor-theme-name",
                               vis->cursor_combo, "selected",
                               G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               theme_to_pos, pos_to_theme, names, (GDestroyNotify)g_object_unref);
}

static void
cursor_size_changed (GtkAdjustment *adjustment, GtkInspectorVisual *vis)
{
  int size;

  size = gtk_adjustment_get_value (adjustment);
  g_object_set (vis->settings, "gtk-cursor-theme-size", size, NULL);
}

static void
init_cursor_size (GtkInspectorVisual *vis)
{
  int size;

  g_object_get (vis->settings, "gtk-cursor-theme-size", &size, NULL);
  if (size == 0)
    size = 32;

  gtk_adjustment_set_value (vis->cursor_size_adjustment, (double)size);
  g_signal_connect (vis->cursor_size_adjustment, "value-changed",
                    G_CALLBACK (cursor_size_changed), vis);
}

static gboolean
name_to_desc (GBinding     *binding,
              const GValue *from_value,
              GValue       *to_value,
              gpointer      user_data)
{
  const char *name;
  PangoFontDescription *desc;

  name = g_value_get_string (from_value);
  desc = pango_font_description_from_string (name);

  g_value_take_boxed (to_value, desc);

  return TRUE;
}

static gboolean
name_from_desc (GBinding     *binding,
                const GValue *from_value,
                GValue       *to_value,
                gpointer      user_data)
{
  const char *name;
  PangoFontDescription *desc;

  desc = g_value_get_boxed (from_value);
  name = pango_font_description_get_family (desc);

  g_value_set_string (to_value, name);

  return TRUE;
}

static void
init_font (GtkInspectorVisual *vis)
{
  g_object_bind_property_full (vis->settings, "gtk-font-name",
                               vis->font_button, "font-desc",
                               G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE,
                               name_to_desc,
                               name_from_desc,
                               NULL, NULL);
}

static void
init_font_scale (GtkInspectorVisual *vis)
{
  double scale;

  scale = get_font_scale (vis);
  update_font_scale (vis, scale, TRUE, TRUE, FALSE);
  g_signal_connect (vis->font_scale_adjustment, "value-changed",
                    G_CALLBACK (font_scale_adjustment_changed), vis);
  g_signal_connect (vis->font_scale_entry, "activate",
                    G_CALLBACK (font_scale_entry_activated), vis);
}

static void
init_font_aa (GtkInspectorVisual *vis)
{
  gtk_switch_set_active (GTK_SWITCH (vis->font_aa_switch), get_font_aa (vis));
}

static void
font_hinting_changed (GtkDropDown        *combo,
                      GParamSpec         *pspec,
                      GtkInspectorVisual *vis)
{
  update_font_hinting (vis, gtk_drop_down_get_selected (combo));
}

static void
init_font_hinting (GtkInspectorVisual *vis)
{
  gtk_drop_down_set_selected (GTK_DROP_DOWN (vis->font_hinting_combo), get_font_hinting (vis));
}

static void
init_metrics_hinting (GtkInspectorVisual *vis)
{
  gtk_switch_set_active (GTK_SWITCH (vis->metrics_hinting_switch), get_metrics_hinting (vis));
}

static void
init_animation (GtkInspectorVisual *vis)
{
  g_object_bind_property (vis->settings, "gtk-enable-animations",
                          vis->animation_switch, "active",
                          G_BINDING_BIDIRECTIONAL|G_BINDING_SYNC_CREATE);
}

static void
update_slowdown (GtkInspectorVisual *vis,
                 double slowdown,
                 gboolean update_adjustment,
                 gboolean update_entry)
{
  _gtk_set_slowdown (slowdown);

  if (update_adjustment)
    gtk_adjustment_set_value (vis->slowdown_adjustment,
                              log2 (slowdown));

  if (update_entry)
    {
      char *str = g_strdup_printf ("%0.*f", 2, slowdown);

      gtk_editable_set_text (GTK_EDITABLE (vis->slowdown_entry), str);
      g_free (str);
    }
}

static void
slowdown_adjustment_changed (GtkAdjustment *adjustment,
                             GtkInspectorVisual *vis)
{
  double value = gtk_adjustment_get_value (adjustment);
  double previous = CLAMP (log2 (_gtk_get_slowdown ()),
                            gtk_adjustment_get_lower (adjustment),
                            gtk_adjustment_get_upper (adjustment));

  if (fabs (value - previous) > EPSILON)
    update_slowdown (vis, exp2 (value), FALSE, TRUE);
}

static void
slowdown_entry_activated (GtkEntry *entry,
                          GtkInspectorVisual *vis)
{
  double slowdown;
  char *err = NULL;

  slowdown = g_strtod (gtk_editable_get_text (GTK_EDITABLE (entry)), &err);
  if (err != NULL)
    update_slowdown (vis, slowdown, TRUE, FALSE);
}

static void
init_slowdown (GtkInspectorVisual *vis)
{
  update_slowdown (vis, _gtk_get_slowdown (), TRUE, TRUE);
  g_signal_connect (vis->slowdown_adjustment, "value-changed",
                    G_CALLBACK (slowdown_adjustment_changed), vis);
  g_signal_connect (vis->slowdown_entry, "activate",
                    G_CALLBACK (slowdown_entry_activated), vis);
}

static gboolean
keynav_failed (GtkWidget *widget, GtkDirectionType direction, GtkInspectorVisual *vis)
{
  GtkWidget *next;

  if (direction == GTK_DIR_DOWN &&
      widget == vis->visual_box)
    next = vis->debug_box;
  else if (direction == GTK_DIR_UP &&
           widget == vis->debug_box)
    next = vis->visual_box;
  else
    next = NULL;

  if (next)
    {
      gtk_widget_child_focus (next, direction);
      return TRUE;
    }

  return FALSE;
}

static void
row_activated (GtkListBox         *box,
               GtkListBoxRow      *row,
               GtkInspectorVisual *vis)
{
  if (gtk_widget_is_ancestor (vis->dark_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->dark_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->animation_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->animation_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->fps_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->fps_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->updates_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->updates_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->cairo_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->cairo_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->baselines_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->baselines_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->layout_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->layout_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->focus_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->focus_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->a11y_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->a11y_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
  else if (gtk_widget_is_ancestor (vis->subsurface_switch, GTK_WIDGET (row)))
    {
      GtkSwitch *sw = GTK_SWITCH (vis->subsurface_switch);
      gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
    }
}

static void
init_gl (GtkInspectorVisual *vis)
{
}

static void
inspect_inspector (GtkButton          *button,
                   GtkInspectorVisual *vis)
{
  GtkWidget *inspector_window;

  inspector_window = gtk_inspector_window_get (gtk_widget_get_display (GTK_WIDGET (button)));
  gtk_window_present (GTK_WINDOW (inspector_window));
}

static void
gtk_inspector_visual_init (GtkInspectorVisual *vis)
{
  gtk_widget_init_template (GTK_WIDGET (vis));
}

static void
gtk_inspector_visual_constructed (GObject *object)
{
  GtkInspectorVisual *vis = GTK_INSPECTOR_VISUAL (object);

  G_OBJECT_CLASS (gtk_inspector_visual_parent_class)->constructed (object);

  g_signal_connect (vis->visual_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
  g_signal_connect (vis->debug_box, "keynav-failed", G_CALLBACK (keynav_failed), vis);
  g_signal_connect (vis->visual_box, "row-activated", G_CALLBACK (row_activated), vis);
  g_signal_connect (vis->debug_box, "row-activated", G_CALLBACK (row_activated), vis);
}

static void
gtk_inspector_visual_unroot (GtkWidget *widget)
{
  GtkInspectorVisual *vis = GTK_INSPECTOR_VISUAL (widget);
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_root (GTK_WIDGET (vis)));

  if (vis->layout_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, vis->layout_overlay);
      vis->layout_overlay = NULL;
    }
  if (vis->updates_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, vis->updates_overlay);
      vis->updates_overlay = NULL;
    }
  if (vis->fps_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, vis->fps_overlay);
      vis->fps_overlay = NULL;
    }
  if (vis->focus_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, vis->focus_overlay);
      vis->focus_overlay = NULL;
    }
  if (vis->a11y_overlay)
    {
      gtk_inspector_window_remove_overlay (iw, vis->a11y_overlay);
      vis->a11y_overlay = NULL;
    }

  GTK_WIDGET_CLASS (gtk_inspector_visual_parent_class)->unroot (widget);
}

static void
gtk_inspector_visual_dispose (GObject *object)
{
  GtkInspectorVisual *vis = GTK_INSPECTOR_VISUAL (object);

  gtk_widget_dispose_template (GTK_WIDGET (vis), GTK_TYPE_INSPECTOR_VISUAL);

  G_OBJECT_CLASS (gtk_inspector_visual_parent_class)->dispose (object);
}

static void
gtk_inspector_visual_class_init (GtkInspectorVisualClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gtk_inspector_visual_constructed;
  object_class->dispose = gtk_inspector_visual_dispose;

  widget_class->unroot = gtk_inspector_visual_unroot;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/visual.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, swin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, direction_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, theme_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, dark_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, cursor_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, cursor_size_spin);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, cursor_size_adjustment);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, icon_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, animation_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, slowdown_adjustment);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, slowdown_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, visual_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, font_aa_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, font_hinting_combo);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, metrics_hinting_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, debug_box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, font_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, font_scale_entry);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, font_scale_adjustment);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, fps_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, updates_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, cairo_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, baselines_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, layout_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, focus_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, a11y_switch);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorVisual, subsurface_switch);

  gtk_widget_class_bind_template_callback (widget_class, fps_activate);
  gtk_widget_class_bind_template_callback (widget_class, updates_activate);
  gtk_widget_class_bind_template_callback (widget_class, cairo_activate);
  gtk_widget_class_bind_template_callback (widget_class, direction_changed);
  gtk_widget_class_bind_template_callback (widget_class, font_aa_activate);
  gtk_widget_class_bind_template_callback (widget_class, font_hinting_changed);
  gtk_widget_class_bind_template_callback (widget_class, metrics_hinting_activate);
  gtk_widget_class_bind_template_callback (widget_class, baselines_activate);
  gtk_widget_class_bind_template_callback (widget_class, layout_activate);
  gtk_widget_class_bind_template_callback (widget_class, focus_activate);
  gtk_widget_class_bind_template_callback (widget_class, a11y_activate);
  gtk_widget_class_bind_template_callback (widget_class, subsurface_activate);
  gtk_widget_class_bind_template_callback (widget_class, inspect_inspector);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

void
gtk_inspector_visual_set_display (GtkInspectorVisual *vis,
                                  GdkDisplay         *display)
{
  vis->display = display;
  vis->settings = gtk_settings_get_for_display (display);

  init_direction (vis);
  init_theme (vis);
  init_dark (vis);
  init_icons (vis);
  init_cursors (vis);
  init_cursor_size (vis);
  init_font (vis);
  init_font_scale (vis);
  init_font_aa (vis);
  init_font_hinting (vis);
  init_metrics_hinting (vis);
  init_animation (vis);
  init_slowdown (vis);
  init_gl (vis);
}

// vim: set et sw=2 ts=2:
