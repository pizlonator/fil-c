/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-tz-dialog.c
 *
 * Copyright 2022 Purism SPC
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-tz-dialog"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _GNU_SOURCE
#include <string.h>
#include <glib/gi18n.h>

#include "cc-tz-dialog.h"
#include "tz.h"

struct _CcTzDialog
{
  AdwWindow           parent_instance;

  GtkSearchEntry     *location_entry;

  GtkStack           *main_stack;
  AdwStatusPage      *empty_page;
  GtkScrolledWindow  *tz_page;
  GtkListView        *tz_view;

  TzDB               *tz_db;
  GListStore         *tz_store;
  GtkFilterListModel *tz_filtered_model;
  GtkNoSelection     *tz_selection_model;

  CcTzItem           *selected_item;
};

G_DEFINE_TYPE (CcTzDialog, cc_tz_dialog, ADW_TYPE_WINDOW)

enum {
  TZ_SELECTED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static gboolean
match_tz_item (CcTzItem   *item,
               CcTzDialog *self)
{
  g_auto(GStrv) strv = NULL;
  g_autofree char *country = NULL;
  g_autofree char *name = NULL;
  g_autofree char *zone = NULL;
  const char *search_terms;

  g_assert (CC_IS_TZ_ITEM (item));
  g_assert (CC_IS_TZ_DIALOG (self));

  search_terms = gtk_editable_get_text (GTK_EDITABLE (self->location_entry));

  if (!search_terms || !*search_terms)
    return TRUE;

  g_object_get (item,
                "country", &country,
                "name", &name,
                "zone", &zone,
                NULL);

  if (!name || !zone || !country)
    return FALSE;

  /* Search for each word separated by spaces */
  strv = g_strsplit (search_terms, " ", 0);

  /*
   * List the item only if the value contain each word.
   * ie, for a search "as kol" it will match "Asia/Kolkata"
   * not "Asia/Karachi"
   */
  for (guint i = 0; strv[i]; i++)
    {
      const char *str = strv[i];

      if (!str || !*str)
        continue;

      if (!strcasestr (name, str) &&
          !strcasestr (zone, str) &&
          !strcasestr (country, str))
        return FALSE;
    }

  return TRUE;
}

static void
load_tz (CcTzDialog *self)
{
  GPtrArray *locations;

  g_assert (CC_IS_TZ_DIALOG (self));

  self->tz_db = tz_load_db ();
  g_assert (self->tz_db);

  locations = tz_get_locations (self->tz_db);
  g_assert (locations);

  for (guint i = 0; i < locations->len; i++)
    {
      g_autoptr(CcTzItem) item = NULL;
      TzLocation *location;

      location = locations->pdata[i];
      item = cc_tz_item_new (location);

      g_list_store_append (self->tz_store, item);
    }
}

static void
tz_selection_model_changed_cb (CcTzDialog *self)
{
  guint n_items;

  g_assert (CC_IS_TZ_DIALOG (self));

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->tz_selection_model));

  if (n_items)
    gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->tz_page));
  else
    gtk_stack_set_visible_child (self->main_stack, GTK_WIDGET (self->empty_page));
}

static void
tz_dialog_search_changed_cb (CcTzDialog *self)
{
  GtkFilter *filter;

  g_assert (CC_IS_TZ_DIALOG (self));

  filter = gtk_filter_list_model_get_filter (self->tz_filtered_model);

  gtk_filter_changed (filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
tz_dialog_search_stopped_cb (CcTzDialog *self)
{
  const char *search_text;
  search_text = gtk_editable_get_text (GTK_EDITABLE (self->location_entry));

  if (search_text && g_strcmp0 (search_text, "") != 0)
    gtk_editable_set_text (GTK_EDITABLE (self->location_entry), "");
  else
    gtk_window_close (GTK_WINDOW (self));
}

static void
tz_dialog_row_activated_cb (CcTzDialog *self,
                            guint       position)
{
  GListModel *model;

  g_assert (CC_IS_TZ_DIALOG (self));

  g_clear_object (&self->selected_item);

  model = G_LIST_MODEL (self->tz_selection_model);
  self->selected_item = g_list_model_get_item (model, position);

  gtk_window_close (GTK_WINDOW (self));
  g_signal_emit (self, signals[TZ_SELECTED], 0);
}

static void
cc_tz_dialog_map (GtkWidget *widget)
{
  CcTzDialog *self = (CcTzDialog *)widget;

  gtk_editable_set_text (GTK_EDITABLE (self->location_entry), "");
  gtk_widget_grab_focus (GTK_WIDGET (self->location_entry));

  GTK_WIDGET_CLASS (cc_tz_dialog_parent_class)->map (widget);
}

static void
cc_tz_dialog_finalize (GObject *object)
{
  CcTzDialog *self = (CcTzDialog *)object;

  g_clear_object (&self->tz_store);
  g_clear_pointer (&self->tz_db, tz_db_free);

  G_OBJECT_CLASS (cc_tz_dialog_parent_class)->finalize (object);
}

static void
cc_tz_dialog_class_init (CcTzDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_tz_dialog_finalize;

  widget_class->map = cc_tz_dialog_map;

  signals[TZ_SELECTED] =
    g_signal_new ("tz-selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "system/datetime/cc-tz-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTzDialog, location_entry);

  gtk_widget_class_bind_template_child (widget_class, CcTzDialog, main_stack);
  gtk_widget_class_bind_template_child (widget_class, CcTzDialog, empty_page);
  gtk_widget_class_bind_template_child (widget_class, CcTzDialog, tz_page);
  gtk_widget_class_bind_template_child (widget_class, CcTzDialog, tz_view);

  gtk_widget_class_bind_template_callback (widget_class, tz_dialog_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, tz_dialog_search_stopped_cb);
  gtk_widget_class_bind_template_callback (widget_class, tz_dialog_row_activated_cb);
}

static void
cc_tz_dialog_init (CcTzDialog *self)
{
  GtkSortListModel *tz_sorted_model;
  GtkExpression *expression;
  GtkSorter *sorter;
  GtkFilter *filter;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->tz_store = g_list_store_new (CC_TYPE_TZ_ITEM);
  load_tz (self);

  /* Sort items by name */
  expression = gtk_property_expression_new (CC_TYPE_TZ_ITEM, NULL, "name");
  sorter = GTK_SORTER (gtk_string_sorter_new (expression));
  tz_sorted_model = gtk_sort_list_model_new (G_LIST_MODEL (self->tz_store), sorter);

  filter = (GtkFilter *)gtk_custom_filter_new ((GtkCustomFilterFunc) match_tz_item, self, NULL);
  self->tz_filtered_model = gtk_filter_list_model_new (G_LIST_MODEL (tz_sorted_model), filter);
  self->tz_selection_model = gtk_no_selection_new (G_LIST_MODEL (self->tz_filtered_model));

  g_signal_connect_object (self->tz_selection_model, "items-changed",
                           G_CALLBACK (tz_selection_model_changed_cb),
                           self, G_CONNECT_SWAPPED);
  tz_selection_model_changed_cb (self);

  gtk_list_view_set_model (self->tz_view, GTK_SELECTION_MODEL (self->tz_selection_model));
}

GtkWidget *
cc_tz_dialog_new (void)
{
  return g_object_new (CC_TYPE_TZ_DIALOG, NULL);
}

gboolean
cc_tz_dialog_set_tz (CcTzDialog *self,
                     const char *timezone)
{
  g_autofree gchar *tz = NULL;
  guint n_items;

  g_return_val_if_fail (CC_IS_TZ_DIALOG (self), FALSE);
  g_return_val_if_fail (timezone && *timezone, FALSE);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (self->tz_store));
  tz = tz_info_get_clean_name (self->tz_db, timezone);

  for (guint i = 0; i < n_items; i++)
    {
      g_autoptr(CcTzItem) item = NULL;
      TzLocation *loc;

      item = g_list_model_get_item (G_LIST_MODEL (self->tz_store), i);
      loc = cc_tz_item_get_location (item);

      if (g_strcmp0 (loc->zone, tz ? tz : timezone) == 0)
        {
          g_set_object (&self->selected_item, item);

          return TRUE;
        }
    }

  return FALSE;
}

CcTzItem *
cc_tz_dialog_get_selected_tz (CcTzDialog *self)
{
  g_return_val_if_fail (CC_IS_TZ_DIALOG (self), NULL);

  return self->selected_item;
}

TzLocation *
cc_tz_dialog_get_selected_location (CcTzDialog *self)
{
  g_return_val_if_fail (CC_IS_TZ_DIALOG (self), NULL);

  if (!self->selected_item)
    return NULL;

  return cc_tz_item_get_location (self->selected_item);
}
