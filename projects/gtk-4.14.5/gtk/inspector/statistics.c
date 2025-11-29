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

#include "statistics.h"

#include "graphdata.h"
#include "graphrenderer.h"

#include "gtklabel.h"
#include "gtksearchbar.h"
#include "gtkstack.h"
#include "gtktogglebutton.h"
#include "gtkmain.h"
#include "gtkcolumnview.h"
#include "gtkcolumnviewcolumn.h"
#include "gtksingleselection.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitem.h"
#include "gtkstringsorter.h"
#include "gtknumericsorter.h"
#include "gtksortlistmodel.h"
#include "gtksearchentry.h"

#include <glib/gi18n-lib.h>

/* {{{ TypeData object */

typedef struct _TypeData TypeData;

G_DECLARE_FINAL_TYPE (TypeData, type_data, TYPE, DATA, GObject);

struct _TypeData {
  GObject parent;

  GType type;
  GraphData *self;
  GraphData *cumulative;
};

enum {
  TYPE_DATA_PROP_NAME = 1,
  TYPE_DATA_PROP_SELF1,
  TYPE_DATA_PROP_CUMULATIVE1,
  TYPE_DATA_PROP_SELF2,
  TYPE_DATA_PROP_CUMULATIVE2,
  TYPE_DATA_PROP_SELF,
  TYPE_DATA_PROP_CUMULATIVE,
};

G_DEFINE_TYPE (TypeData, type_data, G_TYPE_OBJECT);

static void
type_data_init (TypeData *self)
{
}

static void
type_data_finalize (GObject *object)
{
  TypeData *self = TYPE_DATA (object);

  g_object_unref (self->self);
  g_object_unref (self->cumulative);

  G_OBJECT_CLASS (type_data_parent_class)->finalize (object);
}

static void
type_data_get_property (GObject    *object,
                        guint       property_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  TypeData *self = TYPE_DATA (object);

  switch (property_id)
    {
    case TYPE_DATA_PROP_NAME:
      g_value_set_string (value, g_type_name (self->type));
      break;

    case TYPE_DATA_PROP_SELF1:
      g_value_set_int (value, (int) graph_data_get_value (self->self, 1));
      break;

    case TYPE_DATA_PROP_CUMULATIVE1:
      g_value_set_int (value, (int) graph_data_get_value (self->cumulative, 1));
      break;

    case TYPE_DATA_PROP_SELF2:
      g_value_set_int (value, (int) graph_data_get_value (self->self, 0));
      break;

    case TYPE_DATA_PROP_CUMULATIVE2:
      g_value_set_int (value, (int) graph_data_get_value (self->cumulative, 0));
      break;

    case TYPE_DATA_PROP_SELF:
      g_value_set_object (value, self->self);
      break;

    case TYPE_DATA_PROP_CUMULATIVE:
      g_value_set_object (value, self->cumulative);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
type_data_class_init (TypeDataClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = type_data_finalize;
  object_class->get_property = type_data_get_property;

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_NAME,
                                   g_param_spec_string ("name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_SELF1,
                                   g_param_spec_int ("self1", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_CUMULATIVE1,
                                   g_param_spec_int ("cumulative1", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_SELF2,
                                   g_param_spec_int ("self2", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_CUMULATIVE2,
                                   g_param_spec_int ("cumulative2", NULL, NULL,
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_SELF,
                                   g_param_spec_object ("self", NULL, NULL,
                                                        graph_data_get_type (),
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
                                   TYPE_DATA_PROP_CUMULATIVE,
                                   g_param_spec_object ("cumulative", NULL, NULL,
                                                        graph_data_get_type (),
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));
}

static TypeData *
type_data_new (GType type)
{
  TypeData *self;

  self = g_object_new (type_data_get_type (), NULL);

  self->type = type;
  self->self = graph_data_new (60);
  self->cumulative = graph_data_new (60);

  return self;
}

static void
type_data_update (TypeData *data,
                  int       self,
                  int       cumulative)
{
  int value;

  g_object_freeze_notify (G_OBJECT (data));

  value = graph_data_get_value (data->self, 0);
  if (value != self)
    g_object_notify (G_OBJECT (data), "self2");
  if (value != graph_data_get_value (data->self, 1))
    g_object_notify (G_OBJECT (data), "self1");

  g_object_notify (G_OBJECT (data), "self");
  graph_data_prepend_value (data->self, self);

  value = graph_data_get_value (data->cumulative, 0);
  if (value != cumulative)
    g_object_notify (G_OBJECT (data), "cumulative2");
  if (value != graph_data_get_value (data->cumulative, 1))
    g_object_notify (G_OBJECT (data), "cumulative1");

  g_object_notify (G_OBJECT (data), "cumulative");
  graph_data_prepend_value (data->cumulative, cumulative);

  g_object_thaw_notify (G_OBJECT (data));
}

/* }}} */

enum
{
  PROP_0,
  PROP_BUTTON
};

struct _GtkInspectorStatisticsPrivate
{
  GtkWidget *stack;
  GtkWidget *excuse;
  GtkWidget *view;
  GtkWidget *button;
  GListStore *data;
  GtkSingleSelection *selection;
  GHashTable *types;
  guint update_source_id;
  GtkWidget *search_entry;
  GtkWidget *search_bar;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorStatistics, gtk_inspector_statistics, GTK_TYPE_BOX)

static int
add_type_count (GtkInspectorStatistics *sl, GType type)
{
  int cumulative;
  int self;
  GType *children;
  guint n_children;
  int i;
  guint idx;
  TypeData *data;

  cumulative = 0;

  children = g_type_children (type, &n_children);
  for (i = 0; i < n_children; i++)
    cumulative += add_type_count (sl, children[i]);

  idx = GPOINTER_TO_UINT (g_hash_table_lookup (sl->priv->types, GSIZE_TO_POINTER (type)));
  if (idx == 0)
    {
      g_list_store_append (sl->priv->data, type_data_new (type));
      idx = g_list_model_get_n_items (G_LIST_MODEL (sl->priv->data));
      g_hash_table_insert (sl->priv->types, GSIZE_TO_POINTER (type), GUINT_TO_POINTER (idx));
    }

  data = g_list_model_get_item (G_LIST_MODEL (sl->priv->data), idx - 1);

  g_assert (data->type == type);

  self = g_type_get_instance_count (type);
  cumulative += self;

  type_data_update (data, self, cumulative);

  g_object_unref (data);

  return cumulative;
}

static gboolean
update_type_counts (gpointer data)
{
  GtkInspectorStatistics *sl = data;
  GType type;

  for (type = G_TYPE_INTERFACE; type <= G_TYPE_FUNDAMENTAL_MAX; type += (1 << G_TYPE_FUNDAMENTAL_SHIFT))
    {
      if (!G_TYPE_IS_INSTANTIATABLE (type))
        continue;

      add_type_count (sl, type);
    }

  return TRUE;
}

static void
toggle_record (GtkToggleButton        *button,
               GtkInspectorStatistics *sl)
{
  if (gtk_toggle_button_get_active (button) == (sl->priv->update_source_id != 0))
    return;

  if (gtk_toggle_button_get_active (button))
    {
      sl->priv->update_source_id = g_timeout_add_seconds (1, update_type_counts, sl);
      update_type_counts (sl);
    }
  else
    {
      g_source_remove (sl->priv->update_source_id);
      sl->priv->update_source_id = 0;
    }
}

static gboolean
has_instance_counts (void)
{
  return g_type_get_instance_count (GTK_TYPE_LABEL) > 0;
}

static gboolean
instance_counts_enabled (void)
{
  const char *string;
  guint flags = 0;

  string = g_getenv ("GOBJECT_DEBUG");
  if (string != NULL)
    {
      GDebugKey debug_keys[] = {
        { "objects", 1 },
        { "instance-count", 2 },
        { "signals", 4 }
      };

     flags = g_parse_debug_string (string, debug_keys, G_N_ELEMENTS (debug_keys));
    }

  return (flags & 2) != 0;
}

static void
search_changed (GtkSearchEntry         *entry,
                GtkInspectorStatistics *sl)
{
  const char *text;
  GListModel *model;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  model = gtk_single_selection_get_model (sl->priv->selection);

  for (guint i = 0; i < g_list_model_get_n_items (model); i++)
    {
      TypeData *data = g_list_model_get_item (model, i);
      char *string;

      g_object_unref (data);

      string = g_ascii_strdown (g_type_name (data->type), -1);
      if (g_str_has_prefix (string, text))
        {
          g_free (string);
          gtk_single_selection_set_selected (sl->priv->selection, i);
          return;
        }

       g_free (string);
    }

  gtk_single_selection_set_selected (sl->priv->selection, GTK_INVALID_LIST_POSITION);
}

static void
root (GtkWidget *widget)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (widget);
  GtkWidget *toplevel;

  GTK_WIDGET_CLASS (gtk_inspector_statistics_parent_class)->root (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (sl->priv->search_bar), toplevel);
}

static void
unroot (GtkWidget *widget)
{
  GTK_WIDGET_CLASS (gtk_inspector_statistics_parent_class)->unroot (widget);
}

static void
setup_label (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_name (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  data = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);
  gtk_label_set_text (GTK_LABEL (label), g_type_name (data->type));
}

static void
set_self1 (TypeData   *data,
           GParamSpec *pspec,
           GtkWidget  *label)
{
  int count;
  char *text;

  g_object_get (data, "self1", &count, NULL);
  text = g_strdup_printf ("%d", count);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);
}

static void
bind_self1 (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  set_self1 (data, NULL, label);
  g_signal_connect (data, "notify::self1", G_CALLBACK (set_self1), label);
}

static void
unbind_self1 (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_self1), label);
}

static void
set_cumulative1 (TypeData   *data,
                 GParamSpec *pspec,
                 GtkWidget  *label)
{
  int count;
  char *text;

  g_object_get (data, "cumulative1", &count, NULL);
  text = g_strdup_printf ("%d", count);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);
}

static void
bind_cumulative1 (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  set_cumulative1 (data, NULL, label);
  g_signal_connect (data, "notify::cumulative1", G_CALLBACK (set_cumulative1), label);
}

static void
unbind_cumulative1 (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_cumulative1), label);
}

static void
set_self2 (TypeData   *data,
           GParamSpec *pspec,
           GtkWidget  *label)
{
  int count1;
  int count2;
  char *text;

  g_object_get (data, "self1", &count1, NULL);
  g_object_get (data, "self2", &count2, NULL);
  if (count2 > count1)
    text = g_strdup_printf ("%d (↗ %d)", count2, count2 - count1);
  else if (count2 < count1)
    text = g_strdup_printf ("%d (↘ %d)", count2, count1 - count2);
  else
    text = g_strdup_printf ("%d", count2);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);
}

static void
bind_self2 (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  set_self2 (data, NULL, label);
  g_signal_connect (data, "notify::self1", G_CALLBACK (set_self2), label);
  g_signal_connect (data, "notify::self2", G_CALLBACK (set_self2), label);
}

static void
unbind_self2 (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_self2), label);
}

static void
set_cumulative2 (TypeData   *data,
                 GParamSpec *pspec,
                 GtkWidget  *label)
{
  int count1;
  int count2;
  char *text;

  g_object_get (data, "cumulative1", &count1, NULL);
  g_object_get (data, "cumulative2", &count2, NULL);
  if (count2 > count1)
    text = g_strdup_printf ("%d (↗ %d)", count2, count2 - count1);
  else if (count2 < count1)
    text = g_strdup_printf ("%d (↘ %d)", count2, count1 - count2);
  else
    text = g_strdup_printf ("%d", count2);
  gtk_label_set_text (GTK_LABEL (label), text);
  g_free (text);
}

static void
bind_cumulative2 (GtkSignalListItemFactory *factory,
                  GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  set_cumulative2 (data, NULL, label);
  g_signal_connect (data, "notify::cumulative1", G_CALLBACK (set_cumulative2), label);
  g_signal_connect (data, "notify::cumulative2", G_CALLBACK (set_cumulative2), label);
}

static void
unbind_cumulative2 (GtkSignalListItemFactory *factory,
                    GtkListItem              *list_item)
{
  GtkWidget *label;
  TypeData *data;

  label = gtk_list_item_get_child (list_item);
  data = gtk_list_item_get_item (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_cumulative2), label);
}

static void
setup_graph (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item)
{
  gtk_list_item_set_child (list_item, GTK_WIDGET (graph_renderer_new ()));
}

static void
set_graph_self (TypeData   *data,
                GParamSpec *pspec,
                GtkWidget  *graph)
{
  graph_renderer_set_data (GRAPH_RENDERER (graph), data->self);
}

static void
bind_graph_self (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *graph;
  TypeData *data;

  data = gtk_list_item_get_item (list_item);
  graph = gtk_list_item_get_child (list_item);

  set_graph_self (data, NULL, graph);
  g_signal_connect (data, "notify::self", G_CALLBACK (set_graph_self), graph);
}

static void
unbind_graph_self (GtkSignalListItemFactory *factory,
                   GtkListItem              *list_item)
{
  GtkWidget *graph;
  TypeData *data;

  data = gtk_list_item_get_item (list_item);
  graph = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_graph_self), graph);
}

static void
set_graph_cumulative (TypeData   *data,
                      GParamSpec *pspec,
                      GtkWidget  *graph)
{
  graph_renderer_set_data (GRAPH_RENDERER (graph), data->cumulative);
}

static void
bind_graph_cumulative (GtkSignalListItemFactory *factory,
                       GtkListItem              *list_item)
{
  GtkWidget *graph;
  TypeData *data;

  data = gtk_list_item_get_item (list_item);
  graph = gtk_list_item_get_child (list_item);

  set_graph_cumulative (data, NULL, graph);
  g_signal_connect (data, "notify::cumulative", G_CALLBACK (set_graph_cumulative), graph);
}

static void
unbind_graph_cumulative (GtkSignalListItemFactory *factory,
                         GtkListItem              *list_item)
{
  GtkWidget *graph;
  TypeData *data;

  data = gtk_list_item_get_item (list_item);
  graph = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (data, G_CALLBACK (set_graph_cumulative), graph);
}

static void
gtk_inspector_statistics_init (GtkInspectorStatistics *sl)
{
  GtkColumnViewColumn *column;
  GtkListItemFactory *factory;
  GtkSorter *sorter;
  GtkSortListModel *sort_model;

  sl->priv = gtk_inspector_statistics_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
  sl->priv->types = g_hash_table_new (NULL, NULL);

  sl->priv->data = g_list_store_new (type_data_get_type ());

  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (sl->priv->data),
                                        g_object_ref (gtk_column_view_get_sorter (GTK_COLUMN_VIEW (sl->priv->view))));

  sl->priv->selection = gtk_single_selection_new (G_LIST_MODEL (sort_model));
  gtk_single_selection_set_can_unselect (sl->priv->selection, TRUE);

  gtk_column_view_set_model (GTK_COLUMN_VIEW (sl->priv->view), GTK_SELECTION_MODEL (sl->priv->selection));

  g_object_unref (sl->priv->selection);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 0);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_name), NULL);

  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (type_data_get_type (), NULL, "name")));
  gtk_column_view_column_set_sorter (column, sorter);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 1);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_self1), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_self1), NULL);

  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (type_data_get_type (), NULL, "self1")));
  gtk_column_view_column_set_sorter (column, sorter);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 2);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_cumulative1), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cumulative1), NULL);

  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (type_data_get_type (), NULL, "cumulative1")));
  gtk_column_view_column_set_sorter (column, sorter);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 3);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_self2), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_self2), NULL);

  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (type_data_get_type (), NULL, "self2")));
  gtk_column_view_column_set_sorter (column, sorter);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 4);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_label), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_cumulative2), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cumulative2), NULL);

  gtk_column_view_column_set_factory (column, factory);
  sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (type_data_get_type (), NULL, "cumulative2")));
  gtk_column_view_column_set_sorter (column, sorter);
  g_object_unref (sorter);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 5);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_graph), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_graph_self), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_graph_self), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);

  column = g_list_model_get_item (gtk_column_view_get_columns (GTK_COLUMN_VIEW (sl->priv->view)), 6);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_graph), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_graph_cumulative), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_graph_cumulative), NULL);

  gtk_column_view_column_set_factory (column, factory);
  g_object_unref (factory);
  g_object_unref (column);
}

static void
constructed (GObject *object)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (object);

  g_signal_connect (sl->priv->button, "toggled", G_CALLBACK (toggle_record), sl);

  if (has_instance_counts ())
    update_type_counts (sl);
  else
    {
      if (instance_counts_enabled ())
        gtk_label_set_text (GTK_LABEL (sl->priv->excuse), _("GLib must be configured with -Dbuildtype=debug"));
      gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "excuse");
      gtk_widget_set_sensitive (sl->priv->button, FALSE);
    }
}

static void
finalize (GObject *object)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (object);

  if (sl->priv->update_source_id)
    g_source_remove (sl->priv->update_source_id);

  g_hash_table_unref (sl->priv->types);

  G_OBJECT_CLASS (gtk_inspector_statistics_parent_class)->finalize (object);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      g_value_take_object (value, sl->priv->button);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorStatistics *sl = GTK_INSPECTOR_STATISTICS (object);

  switch (param_id)
    {
    case PROP_BUTTON:
      sl->priv->button = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_statistics_class_init (GtkInspectorStatisticsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;

  widget_class->root = root;
  widget_class->unroot = unroot;

  g_object_class_install_property (object_class, PROP_BUTTON,
      g_param_spec_object ("button", NULL, NULL,
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/statistics.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorStatistics, excuse);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
}

/* vim:set foldmethod=marker expandtab: */
