/*
 * gtkappchooserwidget.c: an app-chooser widget
 *
 * Copyright (C) 2004 Novell, Inc.
 * Copyright (C) 2007, 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Dave Camp <dave@novell.com>
 *          Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <ccecchi@redhat.com>
 */

#include "config.h"

#include "gtkappchooserwidget.h"

#include "gtkmarshalers.h"
#include "gtkappchooserwidget.h"
#include "gtkappchooserprivate.h"
#include "gtkliststore.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtktreemodelsort.h"
#include "gtkorientable.h"
#include "gtkscrolledwindow.h"
#include "gtklabel.h"
#include "gtkgestureclick.h"
#include "gtkwidgetprivate.h"
#include "gtkprivate.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/**
 * GtkAppChooserWidget:
 *
 * `GtkAppChooserWidget` is a widget for selecting applications.
 *
 * It is the main building block for [class@Gtk.AppChooserDialog].
 * Most applications only need to use the latter; but you can use
 * this widget as part of a larger widget if you have special needs.
 *
 * `GtkAppChooserWidget` offers detailed control over what applications
 * are shown, using the
 * [property@Gtk.AppChooserWidget:show-default],
 * [property@Gtk.AppChooserWidget:show-recommended],
 * [property@Gtk.AppChooserWidget:show-fallback],
 * [property@Gtk.AppChooserWidget:show-other] and
 * [property@Gtk.AppChooserWidget:show-all] properties. See the
 * [iface@Gtk.AppChooser] documentation for more information about these
 * groups of applications.
 *
 * To keep track of the selected application, use the
 * [signal@Gtk.AppChooserWidget::application-selected] and
 * [signal@Gtk.AppChooserWidget::application-activated] signals.
 *
 * ## CSS nodes
 *
 * `GtkAppChooserWidget` has a single CSS node with name appchooser.
 *
 * Deprecated: 4.10: The application selection widgets should be
 *   implemented according to the design of each platform and/or
 *   application requiring them.
 */

typedef struct _GtkAppChooserWidgetClass   GtkAppChooserWidgetClass;

struct _GtkAppChooserWidget {
  GtkWidget parent_instance;

  GAppInfo *selected_app_info;

  GtkWidget *overlay;

  char *content_type;
  char *default_text;

  guint show_default     : 1;
  guint show_recommended : 1;
  guint show_fallback    : 1;
  guint show_other       : 1;
  guint show_all         : 1;

  GtkWidget *program_list;
  GtkListStore *program_list_store;
  GtkWidget *no_apps_label;
  GtkWidget *no_apps;

  GtkTreeViewColumn *column;
  GtkCellRenderer *padding_renderer;
  GtkCellRenderer *secondary_padding;

  GAppInfoMonitor *monitor;

  GtkWidget *popup_menu;
};

struct _GtkAppChooserWidgetClass {
  GtkWidgetClass parent_class;

  void (* application_selected)  (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);

  void (* application_activated) (GtkAppChooserWidget *self,
                                  GAppInfo            *app_info);
};

enum {
  COLUMN_APP_INFO,
  COLUMN_GICON,
  COLUMN_NAME,
  COLUMN_DESC,
  COLUMN_EXEC,
  COLUMN_DEFAULT,
  COLUMN_HEADING,
  COLUMN_HEADING_TEXT,
  COLUMN_RECOMMENDED,
  COLUMN_FALLBACK,
  NUM_COLUMNS
};


enum {
  PROP_CONTENT_TYPE = 1,
  PROP_GFILE,
  PROP_SHOW_DEFAULT,
  PROP_SHOW_RECOMMENDED,
  PROP_SHOW_FALLBACK,
  PROP_SHOW_OTHER,
  PROP_SHOW_ALL,
  PROP_DEFAULT_TEXT,
  N_PROPERTIES
};

enum {
  SIGNAL_APPLICATION_SELECTED,
  SIGNAL_APPLICATION_ACTIVATED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static void gtk_app_chooser_widget_iface_init (GtkAppChooserIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkAppChooserWidget, gtk_app_chooser_widget, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_APP_CHOOSER,
                                                gtk_app_chooser_widget_iface_init));

static void
refresh_and_emit_app_selected (GtkAppChooserWidget *self,
                               GtkTreeSelection    *selection)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GAppInfo *info = NULL;
  gboolean should_emit = FALSE;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, COLUMN_APP_INFO, &info, -1);

  if (info == NULL)
    return;

  if (self->selected_app_info)
    {
      if (!g_app_info_equal (self->selected_app_info, info))
        {
          should_emit = TRUE;
          g_set_object (&self->selected_app_info, info);
        }
    }
  else
    {
      should_emit = TRUE;
      g_set_object (&self->selected_app_info, info);
    }

  g_object_unref (info);

  if (should_emit)
    g_signal_emit (self, signals[SIGNAL_APPLICATION_SELECTED], 0,
                   self->selected_app_info);
}

static gboolean
path_is_heading (GtkTreeView *view,
                 GtkTreePath *path)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean res;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      COLUMN_HEADING, &res,
                      -1);

  return res;
}

static void
program_list_selection_activated (GtkTreeView       *view,
                                  GtkTreePath       *path,
                                  GtkTreeViewColumn *column,
                                  gpointer           user_data)
{
  GtkAppChooserWidget *self = user_data;
  GtkTreeSelection *selection;

  if (path_is_heading (view, path))
    return;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->program_list));

  refresh_and_emit_app_selected (self, selection);

  g_signal_emit (self, signals[SIGNAL_APPLICATION_ACTIVATED], 0,
                 self->selected_app_info);
}

static gboolean
gtk_app_chooser_search_equal_func (GtkTreeModel *model,
                                   int           column,
                                   const char   *key,
                                   GtkTreeIter  *iter,
                                   gpointer      user_data)
{
  char *name;
  char *exec_name;
  gboolean ret;

  if (key != NULL)
    {
      ret = TRUE;

      gtk_tree_model_get (model, iter,
                          COLUMN_NAME, &name,
                          COLUMN_EXEC, &exec_name,
                          -1);

      if ((name != NULL && g_str_match_string (key, name, TRUE)) ||
          (exec_name != NULL && g_str_match_string (key, exec_name, FALSE)))
        ret = FALSE;

      g_free (name);
      g_free (exec_name);

      return ret;
    }
  else
    {
      return TRUE;
    }
}

static int
gtk_app_chooser_sort_func (GtkTreeModel *model,
                           GtkTreeIter  *a,
                           GtkTreeIter  *b,
                           gpointer      user_data)
{
  gboolean a_recommended, b_recommended;
  gboolean a_fallback, b_fallback;
  gboolean a_heading, b_heading;
  gboolean a_default, b_default;
  char *a_name, *b_name, *a_casefold, *b_casefold;
  int retval = 0;

  /* this returns:
   * - <0 if a should show before b
   * - =0 if a is the same as b
   * - >0 if a should show after b
   */

  gtk_tree_model_get (model, a,
                      COLUMN_NAME, &a_name,
                      COLUMN_RECOMMENDED, &a_recommended,
                      COLUMN_FALLBACK, &a_fallback,
                      COLUMN_HEADING, &a_heading,
                      COLUMN_DEFAULT, &a_default,
                      -1);

  gtk_tree_model_get (model, b,
                      COLUMN_NAME, &b_name,
                      COLUMN_RECOMMENDED, &b_recommended,
                      COLUMN_FALLBACK, &b_fallback,
                      COLUMN_HEADING, &b_heading,
                      COLUMN_DEFAULT, &b_default,
                      -1);

  /* the default one always wins */
  if (a_default && !b_default)
    {
      retval = -1;
      goto out;
    }

  if (b_default && !a_default)
    {
      retval = 1;
      goto out;
    }

  /* the recommended one always wins */
  if (a_recommended && !b_recommended)
    {
      retval = -1;
      goto out;
    }

  if (b_recommended && !a_recommended)
    {
      retval = 1;
      goto out;
    }

  /* the recommended one always wins */
  if (a_fallback && !b_fallback)
    {
      retval = -1;
      goto out;
    }

  if (b_fallback && !a_fallback)
    {
      retval = 1;
      goto out;
    }

  /* they're both recommended/fallback or not, so if one is a heading, wins */
  if (a_heading)
    {
      retval = -1;
      goto out;
    }

  if (b_heading)
    {
      retval = 1;
      goto out;
    }

  /* don't order by name recommended applications, but use GLib's ordering */
  if (!a_recommended)
    {
      a_casefold = a_name != NULL ?
        g_utf8_casefold (a_name, -1) : NULL;
      b_casefold = b_name != NULL ?
        g_utf8_casefold (b_name, -1) : NULL;

      retval = g_strcmp0 (a_casefold, b_casefold);

      g_free (a_casefold);
      g_free (b_casefold);
    }

 out:
  g_free (a_name);
  g_free (b_name);

  return retval;
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
                            GtkCellRenderer   *cell,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
{
  gboolean heading;

  gtk_tree_model_get (model, iter,
                      COLUMN_HEADING, &heading,
                      -1);
  if (heading)
    g_object_set (cell,
                  "visible", FALSE,
                  "xpad", 0,
                  "ypad", 0,
                  NULL);
  else
    g_object_set (cell,
                  "visible", TRUE,
                  "xpad", 3,
                  "ypad", 3,
                  NULL);
}

static gboolean
gtk_app_chooser_selection_func (GtkTreeSelection *selection,
                                GtkTreeModel     *model,
                                GtkTreePath      *path,
                                gboolean          path_currently_selected,
                                gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean heading;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      COLUMN_HEADING, &heading,
                      -1);

  return !heading;
}

static int
compare_apps_func (gconstpointer a,
                   gconstpointer b)
{
  return !g_app_info_equal (G_APP_INFO (a), G_APP_INFO (b));
}

static gboolean
gtk_app_chooser_widget_add_section (GtkAppChooserWidget *self,
                                    const char          *heading_title,
                                    gboolean             show_headings,
                                    gboolean             recommended,
                                    gboolean             fallback,
                                    GList               *applications,
                                    GList               *exclude_apps)
{
  gboolean heading_added, unref_icon;
  GtkTreeIter iter;
  GAppInfo *app;
  char *app_string, *bold_string;
  GIcon *icon;
  GList *l;
  gboolean retval;

  retval = FALSE;
  heading_added = FALSE;
  bold_string = g_strdup_printf ("<b>%s</b>", heading_title);

  for (l = applications; l != NULL; l = l->next)
    {
      app = l->data;

      if (self->content_type != NULL &&
          !g_app_info_supports_uris (app) &&
          !g_app_info_supports_files (app))
        continue;

      if (g_list_find_custom (exclude_apps, app,
                              (GCompareFunc) compare_apps_func))
        continue;

      if (!heading_added && show_headings)
        {
          gtk_list_store_append (self->program_list_store, &iter);
          gtk_list_store_set (self->program_list_store, &iter,
                              COLUMN_HEADING_TEXT, bold_string,
                              COLUMN_HEADING, TRUE,
                              COLUMN_RECOMMENDED, recommended,
                              COLUMN_FALLBACK, fallback,
                              -1);

          heading_added = TRUE;
        }

      app_string = g_markup_printf_escaped ("%s",
                                            g_app_info_get_name (app) != NULL ?
                                            g_app_info_get_name (app) : "");

      icon = g_app_info_get_icon (app);
      unref_icon = FALSE;
      if (icon == NULL)
        {
          icon = g_themed_icon_new ("application-x-executable");
          unref_icon = TRUE;
        }

      gtk_list_store_append (self->program_list_store, &iter);
      gtk_list_store_set (self->program_list_store, &iter,
                          COLUMN_APP_INFO, app,
                          COLUMN_GICON, icon,
                          COLUMN_NAME, g_app_info_get_name (app),
                          COLUMN_DESC, app_string,
                          COLUMN_EXEC, g_app_info_get_executable (app),
                          COLUMN_HEADING, FALSE,
                          COLUMN_RECOMMENDED, recommended,
                          COLUMN_FALLBACK, fallback,
                          -1);

      retval = TRUE;

      g_free (app_string);
      if (unref_icon)
        g_object_unref (icon);
    }

  g_free (bold_string);

  return retval;
}


static void
gtk_app_chooser_add_default (GtkAppChooserWidget *self,
                             GAppInfo            *app)
{
  GtkTreeIter iter;
  GIcon *icon;
  char *string;
  gboolean unref_icon;

  unref_icon = FALSE;
  string = g_strdup_printf ("<b>%s</b>", _("Default App"));

  gtk_list_store_append (self->program_list_store, &iter);
  gtk_list_store_set (self->program_list_store, &iter,
                      COLUMN_HEADING_TEXT, string,
                      COLUMN_HEADING, TRUE,
                      COLUMN_DEFAULT, TRUE,
                      -1);

  g_free (string);

  string = g_markup_printf_escaped ("%s",
                                    g_app_info_get_name (app) != NULL ?
                                    g_app_info_get_name (app) : "");

  icon = g_app_info_get_icon (app);
  if (icon == NULL)
    {
      icon = g_themed_icon_new ("application-x-executable");
      unref_icon = TRUE;
    }

  gtk_list_store_append (self->program_list_store, &iter);
  gtk_list_store_set (self->program_list_store, &iter,
                      COLUMN_APP_INFO, app,
                      COLUMN_GICON, icon,
                      COLUMN_NAME, g_app_info_get_name (app),
                      COLUMN_DESC, string,
                      COLUMN_EXEC, g_app_info_get_executable (app),
                      COLUMN_HEADING, FALSE,
                      COLUMN_DEFAULT, TRUE,
                      -1);

  g_free (string);

  if (unref_icon)
    g_object_unref (icon);
}

static void
update_no_applications_label (GtkAppChooserWidget *self)
{
  char *text = NULL, *desc = NULL;
  const char *string;

  if (self->default_text == NULL)
    {
      if (self->content_type)
	desc = g_content_type_get_description (self->content_type);

      string = text = g_strdup_printf (_("No apps found for “%s”."), desc);
      g_free (desc);
    }
  else
    {
      string = self->default_text;
    }

  gtk_label_set_text (GTK_LABEL (self->no_apps_label), string);

  g_free (text);
}

static void
gtk_app_chooser_widget_select_first (GtkAppChooserWidget *self)
{
  GtkTreeIter iter;
  GAppInfo *info = NULL;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->program_list));
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  while (info == NULL)
    {
      gtk_tree_model_get (model, &iter,
                          COLUMN_APP_INFO, &info,
                          -1);

      if (info != NULL)
        break;

      if (!gtk_tree_model_iter_next (model, &iter))
        break;
    }

  if (info != NULL)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->program_list));
      gtk_tree_selection_select_iter (selection, &iter);

      g_object_unref (info);
    }
}

static void
gtk_app_chooser_widget_real_add_items (GtkAppChooserWidget *self)
{
  GList *all_applications = NULL;
  GList *recommended_apps = NULL;
  GList *fallback_apps = NULL;
  GList *exclude_apps = NULL;
  GAppInfo *default_app = NULL;
  gboolean show_headings;
  gboolean apps_added;

  show_headings = TRUE;
  apps_added = FALSE;

  if (self->show_all)
    show_headings = FALSE;

  if (self->show_default && self->content_type)
    {
      default_app = g_app_info_get_default_for_type (self->content_type, FALSE);

      if (default_app != NULL)
        {
          gtk_app_chooser_add_default (self, default_app);
          apps_added = TRUE;
          exclude_apps = g_list_prepend (exclude_apps, default_app);
        }
    }

#ifndef G_OS_WIN32
  if ((self->content_type && self->show_recommended) || self->show_all)
    {
      if (self->content_type)
	recommended_apps = g_app_info_get_recommended_for_type (self->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Recommended Apps"),
                                                        show_headings,
                                                        !self->show_all, /* mark as recommended */
                                                        FALSE, /* mark as fallback */
                                                        recommended_apps, exclude_apps);

      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (recommended_apps));
    }

  if ((self->content_type && self->show_fallback) || self->show_all)
    {
      if (self->content_type)
	fallback_apps = g_app_info_get_fallback_for_type (self->content_type);

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Related Apps"),
                                                        show_headings,
                                                        FALSE, /* mark as recommended */
                                                        !self->show_all, /* mark as fallback */
                                                        fallback_apps, exclude_apps);
      exclude_apps = g_list_concat (exclude_apps,
                                    g_list_copy (fallback_apps));
    }
#endif

  if (self->show_other || self->show_all)
    {
      all_applications = g_app_info_get_all ();

      apps_added |= gtk_app_chooser_widget_add_section (self, _("Other Apps"),
                                                        show_headings,
                                                        FALSE,
                                                        FALSE,
                                                        all_applications, exclude_apps);
    }

  if (!apps_added)
    update_no_applications_label (self);

  gtk_widget_set_visible (self->no_apps, !apps_added);

  gtk_app_chooser_widget_select_first (self);

  if (default_app != NULL)
    g_object_unref (default_app);

  g_list_free_full (all_applications, g_object_unref);
  g_list_free_full (recommended_apps, g_object_unref);
  g_list_free_full (fallback_apps, g_object_unref);
  g_list_free (exclude_apps);
}

static void
gtk_app_chooser_widget_initialize_items (GtkAppChooserWidget *self)
{
  /* initial padding */
  g_object_set (self->padding_renderer,
                "xpad", self->show_all ? 0 : 6,
                NULL);

  /* populate the widget */
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
}

static void
app_info_changed (GAppInfoMonitor     *monitor,
                  GtkAppChooserWidget *self)
{
  gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
}

static void
gtk_app_chooser_widget_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      self->content_type = g_value_dup_string (value);
      break;
    case PROP_SHOW_DEFAULT:
      gtk_app_chooser_widget_set_show_default (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_RECOMMENDED:
      gtk_app_chooser_widget_set_show_recommended (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_FALLBACK:
      gtk_app_chooser_widget_set_show_fallback (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_OTHER:
      gtk_app_chooser_widget_set_show_other (self, g_value_get_boolean (value));
      break;
    case PROP_SHOW_ALL:
      gtk_app_chooser_widget_set_show_all (self, g_value_get_boolean (value));
      break;
    case PROP_DEFAULT_TEXT:
      gtk_app_chooser_widget_set_default_text (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_widget_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  switch (property_id)
    {
    case PROP_CONTENT_TYPE:
      g_value_set_string (value, self->content_type);
      break;
    case PROP_SHOW_DEFAULT:
      g_value_set_boolean (value, self->show_default);
      break;
    case PROP_SHOW_RECOMMENDED:
      g_value_set_boolean (value, self->show_recommended);
      break;
    case PROP_SHOW_FALLBACK:
      g_value_set_boolean (value, self->show_fallback);
      break;
    case PROP_SHOW_OTHER:
      g_value_set_boolean (value, self->show_other);
      break;
    case PROP_SHOW_ALL:
      g_value_set_boolean (value, self->show_all);
      break;
    case PROP_DEFAULT_TEXT:
      g_value_set_string (value, self->default_text);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_app_chooser_widget_constructed (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->constructed (object);

  gtk_app_chooser_widget_initialize_items (self);
}

static void
gtk_app_chooser_widget_finalize (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  g_free (self->content_type);
  g_free (self->default_text);
  g_signal_handlers_disconnect_by_func (self->monitor, app_info_changed, self);
  g_object_unref (self->monitor);

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->finalize (object);
}

static void
gtk_app_chooser_widget_dispose (GObject *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  g_clear_object (&self->selected_app_info);

  if (self->overlay)
    {
      gtk_widget_unparent (self->overlay);
      self->overlay = NULL;
    }

  G_OBJECT_CLASS (gtk_app_chooser_widget_parent_class)->dispose (object);
}

static void
gtk_app_chooser_widget_measure (GtkWidget       *widget,
                                GtkOrientation  orientation,
                                int             for_size,
                                int            *minimum,
                                int            *natural,
                                int            *minimum_baseline,
                                int            *natural_baseline)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);

  gtk_widget_measure (self->overlay, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_app_chooser_widget_snapshot (GtkWidget   *widget,
                                 GtkSnapshot *snapshot)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);

  gtk_widget_snapshot_child (widget, self->overlay, snapshot);
}

static void
gtk_app_chooser_widget_size_allocate (GtkWidget *widget,
                                      int        width,
                                      int        height,
                                      int        baseline)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (widget);

  GTK_WIDGET_CLASS (gtk_app_chooser_widget_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_widget_size_allocate (self->overlay,
                            &(GtkAllocation) {
                              0, 0,
                              width, height
                            },baseline);
}

static void
gtk_app_chooser_widget_class_init (GtkAppChooserWidgetClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *gobject_class;
  GParamSpec *pspec;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = gtk_app_chooser_widget_dispose;
  gobject_class->finalize = gtk_app_chooser_widget_finalize;
  gobject_class->set_property = gtk_app_chooser_widget_set_property;
  gobject_class->get_property = gtk_app_chooser_widget_get_property;
  gobject_class->constructed = gtk_app_chooser_widget_constructed;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->measure = gtk_app_chooser_widget_measure;
  widget_class->size_allocate = gtk_app_chooser_widget_size_allocate;
  widget_class->snapshot = gtk_app_chooser_widget_snapshot;

  g_object_class_override_property (gobject_class, PROP_CONTENT_TYPE, "content-type");

  /**
   * GtkAppChooserWidget:show-default: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_show_default org.gtk.Property.set=gtk_app_chooser_widget_set_show_default)
   *
   * Determines whether the app chooser should show the default
   * handler for the content type in a separate section.
   *
   * If %FALSE, the default handler is listed among the recommended
   * applications.
   */
  pspec = g_param_spec_boolean ("show-default", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_DEFAULT, pspec);

  /**
   * GtkAppChooserWidget:show-recommended: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_show_recommended org.gtk.Property.set=gtk_app_chooser_widget_set_show_recommended)
   *
   * Determines whether the app chooser should show a section
   * for recommended applications.
   *
   * If %FALSE, the recommended applications are listed
   * among the other applications.
   */
  pspec = g_param_spec_boolean ("show-recommended", NULL, NULL,
                                TRUE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_RECOMMENDED, pspec);

  /**
   * GtkAppChooserWidget:show-fallback: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_show_fallback org.gtk.Property.set=gtk_app_chooser_widget_set_show_fallback)
   *
   * Determines whether the app chooser should show a section
   * for fallback applications.
   *
   * If %FALSE, the fallback applications are listed among the
   * other applications.
   */
  pspec = g_param_spec_boolean ("show-fallback", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_FALLBACK, pspec);

  /**
   * GtkAppChooserWidget:show-other: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_show_other org.gtk.Property.set=gtk_app_chooser_widget_set_show_other)
   *
   * Determines whether the app chooser should show a section
   * for other applications.
   */
  pspec = g_param_spec_boolean ("show-other", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_OTHER, pspec);

  /**
   * GtkAppChooserWidget:show-all: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_show_all org.gtk.Property.set=gtk_app_chooser_widget_set_show_all)
   *
   * If %TRUE, the app chooser presents all applications
   * in a single list, without subsections for default,
   * recommended or related applications.
   */
  pspec = g_param_spec_boolean ("show-all", NULL, NULL,
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_SHOW_ALL, pspec);

  /**
   * GtkAppChooserWidget:default-text: (attributes org.gtk.Property.get=gtk_app_chooser_widget_get_default_text org.gtk.Property.set=gtk_app_chooser_widget_set_default_text)
   *
   * The text that appears in the widget when there are no applications
   * for the given content type.
   */
  pspec = g_param_spec_string ("default-text", NULL, NULL,
                               NULL,
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);
  g_object_class_install_property (gobject_class, PROP_DEFAULT_TEXT, pspec);

  /**
   * GtkAppChooserWidget::application-selected:
   * @self: the object which received the signal
   * @application: the selected `GAppInfo`
   *
   * Emitted when an application item is selected from the widget's list.
   */
  signals[SIGNAL_APPLICATION_SELECTED] =
    g_signal_new (I_("application-selected"),
                  GTK_TYPE_APP_CHOOSER_WIDGET,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkAppChooserWidgetClass, application_selected),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_APP_INFO);

  /**
   * GtkAppChooserWidget::application-activated:
   * @self: the object which received the signal
   * @application: the activated `GAppInfo`
   *
   * Emitted when an application item is activated from the widget's list.
   *
   * This usually happens when the user double clicks an item, or an item
   * is selected and the user presses one of the keys Space, Shift+Space,
   * Return or Enter.
   */
  signals[SIGNAL_APPLICATION_ACTIVATED] =
    g_signal_new (I_("application-activated"),
                  GTK_TYPE_APP_CHOOSER_WIDGET,
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkAppChooserWidgetClass, application_activated),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, G_TYPE_APP_INFO);

  /* Bind class to template
   */
  gtk_widget_class_set_template_from_resource (widget_class,
					       "/org/gtk/libgtk/ui/gtkappchooserwidget.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, program_list);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, program_list_store);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, column);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, padding_renderer);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, secondary_padding);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, no_apps_label);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, no_apps);
  gtk_widget_class_bind_template_child (widget_class, GtkAppChooserWidget, overlay);
  gtk_widget_class_bind_template_callback (widget_class, refresh_and_emit_app_selected);
  gtk_widget_class_bind_template_callback (widget_class, program_list_selection_activated);

  gtk_widget_class_set_css_name (widget_class, I_("appchooser"));
}

static void
gtk_app_chooser_widget_init (GtkAppChooserWidget *self)
{
  GtkTreeSelection *selection;
  GtkTreeModel *sort;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Various parts of the GtkTreeView code need custom code to setup, mostly
   * because we lack signals to connect to, or properties to set.
   */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->program_list));
  gtk_tree_selection_set_select_function (selection, gtk_app_chooser_selection_func,
                                          self, NULL);

  sort = gtk_tree_view_get_model (GTK_TREE_VIEW (self->program_list));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort),
                                   COLUMN_NAME,
                                   gtk_app_chooser_sort_func,
                                   self, NULL);

  gtk_tree_view_set_search_column (GTK_TREE_VIEW (self->program_list), COLUMN_NAME);
  gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW (self->program_list),
                                       gtk_app_chooser_search_equal_func,
                                       NULL, NULL);

  gtk_tree_view_column_set_cell_data_func (self->column,
					   self->secondary_padding,
                                           padding_cell_renderer_func,
                                           NULL, NULL);

  self->monitor = g_app_info_monitor_get ();
  g_signal_connect (self->monitor, "changed",
		    G_CALLBACK (app_info_changed), self);
}

static GAppInfo *
gtk_app_chooser_widget_get_app_info (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (self->selected_app_info == NULL)
    return NULL;

  return g_object_ref (self->selected_app_info);
}

static void
gtk_app_chooser_widget_refresh (GtkAppChooser *object)
{
  GtkAppChooserWidget *self = GTK_APP_CHOOSER_WIDGET (object);

  if (self->program_list_store != NULL)
    {
      gtk_list_store_clear (self->program_list_store);

      /* don't add additional xpad if we don't have headings */
      g_object_set (self->padding_renderer,
                    "visible", !self->show_all,
                    NULL);

      gtk_app_chooser_widget_real_add_items (self);
    }
}

static void
gtk_app_chooser_widget_iface_init (GtkAppChooserIface *iface)
{
  iface->get_app_info = gtk_app_chooser_widget_get_app_info;
  iface->refresh = gtk_app_chooser_widget_refresh;
}

/**
 * gtk_app_chooser_widget_new:
 * @content_type: the content type to show applications for
 *
 * Creates a new `GtkAppChooserWidget` for applications
 * that can handle content of the given type.
 *
 * Returns: a newly created `GtkAppChooserWidget`
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
GtkWidget *
gtk_app_chooser_widget_new (const char *content_type)
{
  return g_object_new (GTK_TYPE_APP_CHOOSER_WIDGET,
                       "content-type", content_type,
                       NULL);
}

/**
 * gtk_app_chooser_widget_set_show_default: (attributes org.gtk.Method.set_property=show-default)
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-default]
 *
 * Sets whether the app chooser should show the default handler
 * for the content type in a separate section.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_default (GtkAppChooserWidget *self,
                                         gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_default != setting)
    {
      self->show_default = setting;

      g_object_notify (G_OBJECT (self), "show-default");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_default: (attributes org.gtk.Method.get_property=show-default)
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show the default handler
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-default]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_app_chooser_widget_get_show_default (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_default;
}

/**
 * gtk_app_chooser_widget_set_show_recommended: (attributes org.gtk.Method.set_property=show-recommended)
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-recommended]
 *
 * Sets whether the app chooser should show recommended applications
 * for the content type in a separate section.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_recommended (GtkAppChooserWidget *self,
                                             gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_recommended != setting)
    {
      self->show_recommended = setting;

      g_object_notify (G_OBJECT (self), "show-recommended");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_recommended: (attributes org.gtk.Method.get_property=show-recommended)
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show recommended applications
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-recommended]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_app_chooser_widget_get_show_recommended (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_recommended;
}

/**
 * gtk_app_chooser_widget_set_show_fallback: (attributes org.gtk.Method.set_property=show-fallback)
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-fallback]
 *
 * Sets whether the app chooser should show related applications
 * for the content type in a separate section.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_fallback (GtkAppChooserWidget *self,
                                          gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_fallback != setting)
    {
      self->show_fallback = setting;

      g_object_notify (G_OBJECT (self), "show-fallback");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_fallback: (attributes org.gtk.Method.get_property=show-fallback)
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show related applications
 * for the content type in a separate section.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-fallback]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_app_chooser_widget_get_show_fallback (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_fallback;
}

/**
 * gtk_app_chooser_widget_set_show_other: (attributes org.gtk.Method.set_property=show-other)
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-other]
 *
 * Sets whether the app chooser should show applications
 * which are unrelated to the content type.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_other (GtkAppChooserWidget *self,
                                       gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_other != setting)
    {
      self->show_other = setting;

      g_object_notify (G_OBJECT (self), "show-other");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_other: (attributes org.gtk.Method.get_property=show-other)
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show applications
 * which are unrelated to the content type.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-other]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_app_chooser_widget_get_show_other (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_other;
}

/**
 * gtk_app_chooser_widget_set_show_all: (attributes org.gtk.Method.set_property=show-all)
 * @self: a `GtkAppChooserWidget`
 * @setting: the new value for [property@Gtk.AppChooserWidget:show-all]
 *
 * Sets whether the app chooser should show all applications
 * in a flat list.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_show_all (GtkAppChooserWidget *self,
                                     gboolean             setting)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (self->show_all != setting)
    {
      self->show_all = setting;

      g_object_notify (G_OBJECT (self), "show-all");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_show_all: (attributes org.gtk.Method.get_property=show-all)
 * @self: a `GtkAppChooserWidget`
 *
 * Gets whether the app chooser should show all applications
 * in a flat list.
 *
 * Returns: the value of [property@Gtk.AppChooserWidget:show-all]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
gboolean
gtk_app_chooser_widget_get_show_all (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), FALSE);

  return self->show_all;
}

/**
 * gtk_app_chooser_widget_set_default_text: (attributes org.gtk.Method.set_property=default-text)
 * @self: a `GtkAppChooserWidget`
 * @text: the new value for [property@Gtk.AppChooserWidget:default-text]
 *
 * Sets the text that is shown if there are not applications
 * that can handle the content type.
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
void
gtk_app_chooser_widget_set_default_text (GtkAppChooserWidget *self,
                                         const char          *text)
{
  g_return_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self));

  if (g_strcmp0 (text, self->default_text) != 0)
    {
      g_free (self->default_text);
      self->default_text = g_strdup (text);

      g_object_notify (G_OBJECT (self), "default-text");

      gtk_app_chooser_refresh (GTK_APP_CHOOSER (self));
    }
}

/**
 * gtk_app_chooser_widget_get_default_text: (attributes org.gtk.Method.get_property=default-text)
 * @self: a `GtkAppChooserWidget`
 *
 * Returns the text that is shown if there are not applications
 * that can handle the content type.
 *
 * Returns: (nullable): the value of [property@Gtk.AppChooserWidget:default-text]
 *
 * Deprecated: 4.10: This widget will be removed in GTK 5
 */
const char *
gtk_app_chooser_widget_get_default_text (GtkAppChooserWidget *self)
{
  g_return_val_if_fail (GTK_IS_APP_CHOOSER_WIDGET (self), NULL);

  return self->default_text;
}

void
_gtk_app_chooser_widget_set_search_entry (GtkAppChooserWidget *self,
                                          GtkEditable         *entry)
{
  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (self->program_list), entry);

  g_object_bind_property (self->no_apps, "visible",
                          entry, "sensitive",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
}
