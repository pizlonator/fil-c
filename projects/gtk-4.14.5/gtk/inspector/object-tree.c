/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include <string.h>

#include "object-tree.h"
#include "prop-list.h"
#include "window.h"

#include "gtkbuildable.h"
#include "gtkbutton.h"
#include "deprecated/gtkcelllayout.h"
#include "gtkcolumnview.h"
#include "deprecated/gtkcomboboxprivate.h"
#include "gtkfilterlistmodel.h"
#include "gtkcustomfilter.h"
#include "gtkflattenlistmodel.h"
#include "gtkbuiltiniconprivate.h"
#include "deprecated/gtkiconview.h"
#include "gtkinscription.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkpopover.h"
#include "gtksettings.h"
#include "gtksingleselection.h"
#include "gtksignallistitemfactory.h"
#include "gtktextview.h"
#include "gtktogglebutton.h"
#include "gtktreeexpander.h"
#include "gtktreelistmodel.h"
#include "deprecated/gtktreeview.h"
#include "deprecated/gtktreeselection.h"
#include "deprecated/gtktreemodelsort.h"
#include "deprecated/gtktreemodelfilter.h"
#include "gtkwidgetprivate.h"
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "gtkeventcontrollerkey.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

enum
{
  OBJECT_SELECTED,
  OBJECT_ACTIVATED,
  LAST_SIGNAL
};

struct _GtkInspectorObjectTreePrivate
{
  GtkColumnView *list;
  GtkTreeListModel *tree_model;
  GtkSingleSelection *selection;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
};

typedef struct _ObjectTreeClassFuncs ObjectTreeClassFuncs;
typedef void (* ObjectTreeForallFunc) (GObject    *object,
                                       const char *name,
                                       gpointer    data);

struct _ObjectTreeClassFuncs {
  GType         (* get_type)            (void);
  GObject *     (* get_parent)          (GObject                *object);
  GListModel *  (* get_children)        (GObject                *object);
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorObjectTree, gtk_inspector_object_tree, GTK_TYPE_BOX)

static GObject *
object_tree_get_parent_default (GObject *object)
{
  return g_object_get_data (object, "inspector-object-tree-parent");
}

static GListModel *
object_tree_get_children_default (GObject *object)
{
  return NULL;
}

static GObject *
object_tree_widget_get_parent (GObject *object)
{
  return G_OBJECT (gtk_widget_get_parent (GTK_WIDGET (object)));
}

static GListModel *
object_tree_widget_get_children (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GListStore *list;
  GListModel *sublist;

  list = g_list_store_new (G_TYPE_LIST_MODEL);

  sublist = gtk_widget_observe_children (widget);
  g_list_store_append (list, sublist);
  g_object_unref (sublist);

  sublist = gtk_widget_observe_controllers (widget);
  g_list_store_append (list, sublist);
  g_object_unref (sublist);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (list)));
}

static GListModel *
object_tree_tree_model_sort_get_children (GObject *object)
{
  GListStore *store;

  store = g_list_store_new (G_TYPE_OBJECT);
  g_list_store_append (store, gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (object)));

  return G_LIST_MODEL (store);
}

static GListModel *
object_tree_tree_model_filter_get_children (GObject *object)
{
  GListStore *store;

  store = g_list_store_new (G_TYPE_OBJECT);
  g_list_store_append (store, gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (object)));

  return G_LIST_MODEL (store);
}

static void
update_list_store (GListStore *store,
                   GObject    *object,
                   const char *property)
{
  gpointer value;

  g_object_get (object, property, &value, NULL);
  if (value)
    {
      g_list_store_splice (store,
                           0,
                           g_list_model_get_n_items (G_LIST_MODEL (store)),
                           &value,
                           1);
      g_object_unref (value);
    }
  else
    {
      g_list_store_remove_all (store);
    }
}

static void
list_model_for_property_notify_cb (GObject    *object,
                                   GParamSpec *pspec,
                                   GListStore *store)
{
  update_list_store (store, object, pspec->name);
}

static GListModel *
list_model_for_property (GObject    *object,
                         const char *property)
{
  GListStore *store = g_list_store_new (G_TYPE_OBJECT);

  /* g_signal_connect_object ("notify::property") */
  g_signal_connect_closure_by_id (object,
                                  g_signal_lookup ("notify", G_OBJECT_TYPE (object)),
                                  g_quark_from_static_string (property),
                                  g_cclosure_new_object (G_CALLBACK (list_model_for_property_notify_cb), G_OBJECT (store)),
                                  FALSE);
  update_list_store (store, object, property);

  return G_LIST_MODEL (store);
}

static GListModel *
list_model_for_properties (GObject     *object,
                           const char **props)
{
  GListStore *concat;
  guint i;

  if (props[1] == NULL)
    return list_model_for_property (object, props[0]);

  concat = g_list_store_new (G_TYPE_LIST_MODEL);
  for (i = 0; props[i]; i++)
    {
      GListModel *tmp = list_model_for_property (object, props[i]);
      g_list_store_append (concat, tmp);
      g_object_unref (tmp);
    }

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (concat)));
}

static GListModel *
object_tree_combo_box_get_children (GObject *object)
{
  return list_model_for_properties (object, (const char *[2]) { "model", NULL });
}

static void
treeview_columns_changed (GtkTreeView *treeview,
                          GListModel  *store)
{
  GtkTreeViewColumn *column, *item;
  guint i, n_columns, n_items;

  n_columns = gtk_tree_view_get_n_columns (treeview);
  n_items = g_list_model_get_n_items (store);

  for (i = 0; i < MAX (n_columns, n_items); i++)
    {
      column = gtk_tree_view_get_column (treeview, i);
      item = g_list_model_get_item (store, i);
      g_object_unref (item);

      if (column == item)
        continue;

      if (n_columns < n_items)
        {
          /* column removed */
          g_assert (n_columns + 1 == n_items);
          g_list_store_remove (G_LIST_STORE (store), i);
          return;
        }
      else if (n_columns > n_items)
        {
          /* column added */
          g_assert (n_columns - 1 == n_items);
          g_list_store_insert (G_LIST_STORE (store), i, column);
          return;
        }
      else
        {
          guint j;
          /* column reordered */
          for (j = n_columns - 1; j > i; j--)
            {
              column = gtk_tree_view_get_column (treeview, j);
              item = g_list_model_get_item (store, j);
              g_object_unref (item);

              if (column != item)
                break;
            }
          g_assert (j > i);
          column = gtk_tree_view_get_column (treeview, i);
          item = g_list_model_get_item (store, j);
          g_object_unref (item);

          if (item == column)
            {
              /* column was removed from position j and put into position i */
              g_list_store_remove (G_LIST_STORE (store), j);
              g_list_store_insert (G_LIST_STORE (store), i, column);
            }
          else
            {
              /* column was removed from position i and put into position j */
              column = gtk_tree_view_get_column (treeview, j);
              g_list_store_remove (G_LIST_STORE (store), i);
              g_list_store_insert (G_LIST_STORE (store), j, column);
            }
        }
    }
}

static GListModel *
object_tree_tree_view_get_children (GObject *object)
{
  GtkTreeView *treeview = GTK_TREE_VIEW (object);
  GListStore *columns, *selection, *result_list;
  GListModel *props;
  guint i;

  props = list_model_for_properties (object, (const char *[2]) { "model", NULL });

  columns = g_list_store_new (GTK_TYPE_TREE_VIEW_COLUMN);
  g_signal_connect_object (treeview, "columns-changed", G_CALLBACK (treeview_columns_changed), columns, 0);
  for (i = 0; i < gtk_tree_view_get_n_columns (treeview); i++)
    g_list_store_append (columns, gtk_tree_view_get_column (treeview, i));

  selection = g_list_store_new (GTK_TYPE_TREE_SELECTION);
  g_list_store_append (selection, gtk_tree_view_get_selection (treeview));

  result_list = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (result_list, props);
  g_object_unref (props);
  g_list_store_append (result_list, selection);
  g_object_unref (selection);
  g_list_store_append (result_list, columns);
  g_object_unref (columns);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (result_list)));
}

static GListModel *
object_tree_column_view_get_children (GObject *object)
{
  GtkColumnView *view = GTK_COLUMN_VIEW (object);
  GListStore *result_list;
  GListModel *columns, *sublist;

  result_list = g_list_store_new (G_TYPE_LIST_MODEL);

  columns = gtk_column_view_get_columns (view);
  g_list_store_append (result_list, columns);

  sublist = object_tree_widget_get_children (object);
  g_list_store_append (result_list, sublist);
  g_object_unref (sublist);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (result_list)));
}

static GListModel *
object_tree_icon_view_get_children (GObject *object)
{
  return list_model_for_properties (object, (const char *[2]) { "model", NULL });
}

static gboolean
object_tree_cell_area_add_child (GtkCellRenderer  *renderer,
                                 gpointer          store)
{
  gpointer cell_layout;

  cell_layout = g_object_get_data (store, "gtk-inspector-cell-layout");
  g_object_set_data (G_OBJECT (renderer), "gtk-inspector-cell-layout", cell_layout);

  g_list_store_append (store, renderer);

  return FALSE;
}

static GListModel *
object_tree_cell_area_get_children (GObject *object)
{
  GListStore *store;
  gpointer cell_layout;

  cell_layout = g_object_get_data (object, "gtk-inspector-cell-layout");
  store = g_list_store_new (GTK_TYPE_CELL_RENDERER);
  g_object_set_data (G_OBJECT (store), "gtk-inspector-cell-layout", cell_layout);
  /* XXX: no change notification for cell areas */
  gtk_cell_area_foreach (GTK_CELL_AREA (object), object_tree_cell_area_add_child, store);

  return G_LIST_MODEL (store);
}

static GListModel *
object_tree_cell_layout_get_children (GObject *object)
{
  GListStore *store;
  GtkCellArea *area;

  /* cell areas handle their own stuff */
  if (GTK_IS_CELL_AREA (object))
    return NULL;

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (object));
  if (!area)
    return NULL;

  g_object_set_data (G_OBJECT (area), "gtk-inspector-cell-layout", object);
  /* XXX: are cell areas immutable? */
  store = g_list_store_new (G_TYPE_OBJECT);
  g_list_store_append (store, area);
  return G_LIST_MODEL (store);
}

static GListModel *
object_tree_text_view_get_children (GObject *object)
{
  return list_model_for_properties (object, (const char *[2]) { "buffer", NULL });
}

static GListModel *
object_tree_text_buffer_get_children (GObject *object)
{
  return list_model_for_properties (object, (const char *[2]) { "tag-table", NULL });
}

static void
text_tag_added (GtkTextTagTable *table,
                GtkTextTag      *tag,
                GListStore      *store)
{
  g_list_store_append (store, tag);
}

static void
text_tag_removed (GtkTextTagTable *table,
                  GtkTextTag      *tag,
                  GListStore      *store)
{
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (store)); i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (store), i);
      g_object_unref (item);

      if (tag == item)
        {
          g_list_store_remove (store, i);
          return;
        }
    }
}

static void
text_tag_foreach (GtkTextTag *tag,
                  gpointer    store)
{
  g_list_store_append (store, tag);
}

static GListModel *
object_tree_text_tag_table_get_children (GObject *object)
{
  GListStore *store = g_list_store_new (GTK_TYPE_TEXT_TAG);

  g_signal_connect_object (object, "tag-added", G_CALLBACK (text_tag_added), store, 0);
  g_signal_connect_object (object, "tag-removed", G_CALLBACK (text_tag_removed), store, 0);
  gtk_text_tag_table_foreach (GTK_TEXT_TAG_TABLE (object), text_tag_foreach, store);

  return NULL;
}

static GListModel *
object_tree_application_get_children (GObject *object)
{
  return list_model_for_properties (object, (const char *[2]) { "menubar", NULL });
}

static GObject *
object_tree_event_controller_get_parent (GObject *object)
{
  return G_OBJECT (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (object)));
}

/* Note:
 * This tree must be sorted with the most specific types first.
 * We iterate over it top to bottom and return the first match
 * using g_type_is_a ()
 */
static const ObjectTreeClassFuncs object_tree_class_funcs[] = {
  {
    gtk_application_get_type,
    object_tree_get_parent_default,
    object_tree_application_get_children
  },
  {
    gtk_text_tag_table_get_type,
    object_tree_get_parent_default,
    object_tree_text_tag_table_get_children
  },
  {
    gtk_text_buffer_get_type,
    object_tree_get_parent_default,
    object_tree_text_buffer_get_children
  },
  {
    gtk_text_view_get_type,
    object_tree_widget_get_parent,
    object_tree_text_view_get_children
  },
  {
    gtk_icon_view_get_type,
    object_tree_widget_get_parent,
    object_tree_icon_view_get_children
  },
  {
    gtk_tree_view_get_type,
    object_tree_widget_get_parent,
    object_tree_tree_view_get_children
  },
  {
    gtk_column_view_get_type,
    object_tree_widget_get_parent,
    object_tree_column_view_get_children
  },
  {
    gtk_combo_box_get_type,
    object_tree_widget_get_parent,
    object_tree_combo_box_get_children
  },
  {
    gtk_widget_get_type,
    object_tree_widget_get_parent,
    object_tree_widget_get_children
  },
  {
    gtk_tree_model_filter_get_type,
    object_tree_get_parent_default,
    object_tree_tree_model_filter_get_children
  },
  {
    gtk_tree_model_sort_get_type,
    object_tree_get_parent_default,
    object_tree_tree_model_sort_get_children
  },
  {
    gtk_cell_area_get_type,
    object_tree_get_parent_default,
    object_tree_cell_area_get_children
  },
  {
    gtk_cell_layout_get_type,
    object_tree_get_parent_default,
    object_tree_cell_layout_get_children
  },
  {
    gtk_event_controller_get_type,
    object_tree_event_controller_get_parent,
    object_tree_get_children_default
  },
  {
    g_object_get_type,
    object_tree_get_parent_default,
    object_tree_get_children_default
  },
};

static const ObjectTreeClassFuncs *
find_class_funcs (GObject *object)
{
  GType object_type;
  guint i;

  object_type = G_OBJECT_TYPE (object);

  for (i = 0; i < G_N_ELEMENTS (object_tree_class_funcs); i++)
    {
      if (g_type_is_a (object_type, object_tree_class_funcs[i].get_type ()))
        return &object_tree_class_funcs[i];
    }

  g_assert_not_reached ();

  return NULL;
}

static GObject *
object_get_parent (GObject *object)
{
  const ObjectTreeClassFuncs *funcs;

  funcs = find_class_funcs (object);
  
  return funcs->get_parent (object);
}

static GListModel *
object_get_children (GObject *object)
{
  GType object_type;
  GListModel *children;
  GListStore *result_list;
  guint i;

  object_type = G_OBJECT_TYPE (object);
  result_list = NULL;

  for (i = 0; i < G_N_ELEMENTS (object_tree_class_funcs); i++)
    {
      if (!g_type_is_a (object_type, object_tree_class_funcs[i].get_type ()))
        continue;

      children = object_tree_class_funcs[i].get_children (object);
      if (children == NULL)
        continue;

      if (!result_list)
        result_list = g_list_store_new (G_TYPE_LIST_MODEL);

      g_list_store_append (result_list, children);
      g_object_unref (children);
    }

  if (result_list)
    return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (result_list)));
  else
    return NULL;
}

static const char *
gtk_inspector_get_object_name (GObject *object)
{
  if (GTK_IS_WIDGET (object))
    {
      const char *id;

      id = gtk_widget_get_name (GTK_WIDGET (object));
      if (id != NULL && g_strcmp0 (id, G_OBJECT_TYPE_NAME (object)) != 0)
        return id;
    }

  if (GTK_IS_BUILDABLE (object))
    {
      const char *id;

      id = gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));
      if (id != NULL && !g_str_has_prefix (id, "___object_"))
        return id;
    }

  if (GTK_IS_EVENT_CONTROLLER (object))
    {
      return gtk_event_controller_get_name (GTK_EVENT_CONTROLLER (object));
    }

  return NULL;
}

char *
gtk_inspector_get_object_title (GObject *object)
{
  const char *name = gtk_inspector_get_object_name (object);

  if (name == NULL)
    return g_strdup (G_OBJECT_TYPE_NAME (object));
  else
    return g_strconcat (G_OBJECT_TYPE_NAME (object), " — ", name, NULL);
}

void
gtk_inspector_object_tree_activate_object (GtkInspectorObjectTree *wt,
                                           GObject                *object)
{
  gtk_inspector_object_tree_select_object (wt, object);
  g_signal_emit (wt, signals[OBJECT_ACTIVATED], 0, object);
}

static void
on_row_activated (GtkColumnView          *view,
                  guint                   pos,
                  GtkInspectorObjectTree *wt)
{
  GtkTreeListRow *item;
  GObject *object;

  item = g_list_model_get_item (G_LIST_MODEL (wt->priv->tree_model), pos);
  object = gtk_tree_list_row_get_item (item);

  gtk_inspector_object_tree_activate_object (wt, object);

  g_object_unref (item);
  g_object_unref (object);
}

GObject *
gtk_inspector_object_tree_get_selected (GtkInspectorObjectTree *wt)
{
  GtkTreeListRow *selected_item;
  GObject *object;

  selected_item = gtk_single_selection_get_selected_item (wt->priv->selection);
  if (selected_item == NULL)
    return NULL;

  object = gtk_tree_list_row_get_item (selected_item);

  g_object_unref (object); /* ahem */
  return object;
}

static void
widget_mapped (GtkWidget *widget,
               GtkWidget *label)
{
  gtk_widget_remove_css_class (label, "dim-label");
}

static void
widget_unmapped (GtkWidget *widget,
                 GtkWidget *label)
{
  gtk_widget_add_css_class (label, "dim-label");
}

static gboolean
search (GtkInspectorObjectTree *wt,
        gboolean                forward,
        gboolean                force_progress);

static gboolean
key_pressed (GtkEventController     *controller,
             guint                   keyval,
             guint                   keycode,
             GdkModifierType         state,
             GtkInspectorObjectTree *wt)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (wt)))
    {
      GdkModifierType default_accel;
      gboolean search_started;

      search_started = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar));
      default_accel = GDK_CONTROL_MASK;

      if (search_started &&
          (keyval == GDK_KEY_Return ||
           keyval == GDK_KEY_ISO_Enter ||
           keyval == GDK_KEY_KP_Enter))
        {
          gtk_widget_activate (GTK_WIDGET (wt->priv->list));
          return GDK_EVENT_PROPAGATE;
        }
      else if (search_started &&
               (keyval == GDK_KEY_Escape))
        {
          gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar), FALSE);
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((state & (default_accel | GDK_SHIFT_MASK)) == (default_accel | GDK_SHIFT_MASK)) &&
               (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
        {
          if (!search (wt, TRUE, TRUE))
            gtk_widget_error_bell (GTK_WIDGET (wt));
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((state & (default_accel | GDK_SHIFT_MASK)) == default_accel) &&
               (keyval == GDK_KEY_g || keyval == GDK_KEY_G))
        {
          if (!search (wt, TRUE, TRUE))
            gtk_widget_error_bell (GTK_WIDGET (wt));
          return GDK_EVENT_STOP;
        }
    }

  return GDK_EVENT_PROPAGATE;
}

static void
destroy_controller (GtkEventController *controller)
{
  gtk_widget_remove_controller (gtk_event_controller_get_widget (controller), controller);
}

static gboolean toplevel_filter_func (gpointer item,
                                      gpointer data);

static void
map (GtkWidget *widget)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (widget);
  GtkEventController *controller;
  GtkWidget *toplevel;

  GTK_WIDGET_CLASS (gtk_inspector_object_tree_parent_class)->map (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  controller = gtk_event_controller_key_new ();
  g_object_set_data_full (G_OBJECT (toplevel), "object-controller", controller, (GDestroyNotify)destroy_controller);
  g_signal_connect (controller, "key-pressed", G_CALLBACK (key_pressed), widget);
  gtk_widget_add_controller (toplevel, controller);

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (wt->priv->search_bar), toplevel);
}

static void
unmap (GtkWidget *widget)
{
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  g_object_set_data (G_OBJECT (toplevel), "object-controller", NULL);

  GTK_WIDGET_CLASS (gtk_inspector_object_tree_parent_class)->unmap (widget);
}

static gboolean
match_string (const char *string,
              const char *text)
{
  char *lower;
  gboolean match = FALSE;

  if (string)
    {
      lower = g_ascii_strdown (string, -1);
      match = g_str_has_prefix (lower, text);
      g_free (lower);
    }

  return match;
}

static gboolean
match_object (GObject    *object,
              const char *text)
{
  char *address;
  gboolean ret = FALSE;

  if (match_string (G_OBJECT_TYPE_NAME (object), text) ||
      match_string (gtk_inspector_get_object_name (object), text))
    return TRUE;

  if (GTK_IS_LABEL (object))
    return match_string (gtk_label_get_label (GTK_LABEL (object)), text);
  if (GTK_IS_INSCRIPTION (object))
    return match_string (gtk_inscription_get_text (GTK_INSCRIPTION (object)), text);
  else if (GTK_IS_BUTTON (object))
    return match_string (gtk_button_get_label (GTK_BUTTON (object)), text);
  else if (GTK_IS_WINDOW (object))
    return match_string (gtk_window_get_title (GTK_WINDOW (object)), text);
  else if (GTK_IS_TREE_VIEW_COLUMN (object))
    return match_string (gtk_tree_view_column_get_title (GTK_TREE_VIEW_COLUMN (object)), text);
  else if (GTK_IS_EDITABLE (object))
    return match_string (gtk_editable_get_text (GTK_EDITABLE (object)), text);

  address = g_strdup_printf ("%p", object);
  ret = match_string (address, text);
  g_free (address);

  return ret;
}

static GObject *
search_children (GObject    *object,
                 const char *text,
                 gboolean    forward)
{
  GListModel *children;
  GObject *child, *result;
  guint i, n;

  children = object_get_children (object);
  if (children == NULL)
    return NULL;

  n = g_list_model_get_n_items (children);
  for (i = 0; i < n; i++)
    {
      child = g_list_model_get_item (children, forward ? i : n - i - 1);
      if (match_object (child, text))
        return child;

      result = search_children (child, text, forward);
      g_object_unref (child);
      if (result)
        return result;
    }

  return NULL;
}

static gboolean
search (GtkInspectorObjectTree *wt,
        gboolean                forward,
        gboolean                force_progress)
{
  GtkInspectorObjectTreePrivate *priv = wt->priv;
  GListModel *model = G_LIST_MODEL (priv->tree_model);
  GtkTreeListRow *row_item;
  GObject *child, *result;
  guint i, selected, n, row;
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (priv->search_entry));
  selected = gtk_single_selection_get_selected (priv->selection);
  n = g_list_model_get_n_items (model);
  if (selected >= n)
    selected = 0;

  for (i = 0; i < n; i++)
    {
      row = (selected + (forward ? i : n - i - 1)) % n;
      row_item = g_list_model_get_item (model, row);
      child = gtk_tree_list_row_get_item (row_item);
      if (i > 0 || !force_progress)
        {
          if (match_object (child, text))
            {
              gtk_column_view_scroll_to (GTK_COLUMN_VIEW (wt->priv->list),
                                         row,
                                         NULL,
                                         GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS,
                                         NULL);
              g_object_unref (child);
              g_object_unref (row_item);
              return TRUE;
            }
        }

      if (!gtk_tree_list_row_get_expanded (row_item))
        {
          result = search_children (child, text, forward);
          if (result)
            {
              gtk_inspector_object_tree_select_object (wt, result);
              g_object_unref (result);
              g_object_unref (child);
              g_object_unref (row_item);
              return TRUE;
            }
        }
      g_object_unref (child);
      g_object_unref (row_item);
    }

  return FALSE;
}

static void
on_search_changed (GtkSearchEntry         *entry,
                   GtkInspectorObjectTree *wt)
{
  if (!search (wt, TRUE, FALSE))
    gtk_widget_error_bell (GTK_WIDGET (wt));
}

static void
next_match (GtkButton              *button,
            GtkInspectorObjectTree *wt)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar)))
    {
      if (!search (wt, TRUE, TRUE))
        gtk_widget_error_bell (GTK_WIDGET (wt));
    }
}

static void
previous_match (GtkButton              *button,
                GtkInspectorObjectTree *wt)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar)))
    {
      if (!search (wt, FALSE, TRUE))
        gtk_widget_error_bell (GTK_WIDGET (wt));
    }
}

static void
stop_search (GtkWidget              *entry,
             GtkInspectorObjectTree *wt)
{
  gtk_editable_set_text (GTK_EDITABLE (wt->priv->search_entry), "");
  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (wt->priv->search_bar), FALSE);
}

static void
setup_type_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *expander, *inscription;

  /* expander */
  expander = gtk_tree_expander_new ();
  gtk_list_item_set_child (list_item, expander);

  /* label */
  inscription = gtk_inscription_new (NULL);
  gtk_inscription_set_nat_chars (GTK_INSCRIPTION (inscription), 30);
  gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), inscription);
}

static void
bind_type_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *expander, *inscription;
  GtkTreeListRow *list_row;
  gpointer item;

  list_row = gtk_list_item_get_item (list_item);
  expander = gtk_list_item_get_child (list_item);
  gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), list_row);
  item = gtk_tree_list_row_get_item (list_row);
  inscription = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));

  gtk_inscription_set_text (GTK_INSCRIPTION (inscription), G_OBJECT_TYPE_NAME (item));

  if (GTK_IS_WIDGET (item))
    {
      g_signal_connect (item, "map", G_CALLBACK (widget_mapped), inscription);
      g_signal_connect (item, "unmap", G_CALLBACK (widget_unmapped), inscription);
      if (!gtk_widget_get_mapped (item))
        widget_unmapped (item, inscription);
      g_object_set_data (G_OBJECT (inscription), "binding", g_object_ref (item));
    }

  g_object_unref (item);
}

static void
unbind_type_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *expander, *label;
  gpointer item;

  expander = gtk_list_item_get_child (list_item);
  label = gtk_tree_expander_get_child (GTK_TREE_EXPANDER (expander));
  item = g_object_steal_data (G_OBJECT (label), "binding");
  if (item)
    {
      g_signal_handlers_disconnect_by_func (item, widget_mapped, label);
      g_signal_handlers_disconnect_by_func (item, widget_unmapped, label);

      g_object_unref (item);
    }
}

static void
setup_name_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *inscription;

  inscription = gtk_inscription_new (NULL);
  gtk_inscription_set_nat_chars (GTK_INSCRIPTION (inscription), 15);
  gtk_list_item_set_child (list_item, inscription);
}

static void
bind_name_cb (GtkSignalListItemFactory *factory,
              GtkListItem              *list_item)
{
  GtkWidget *inscription;
  gpointer item;

  item = gtk_tree_list_row_get_item (gtk_list_item_get_item (list_item));
  inscription = gtk_list_item_get_child (list_item);

  gtk_inscription_set_text (GTK_INSCRIPTION (inscription), gtk_inspector_get_object_name (item));

  g_object_unref (item);
}

static void
setup_label_cb (GtkSignalListItemFactory *factory,
                GtkListItem              *list_item)
{
  GtkWidget *inscription;

  inscription = gtk_inscription_new (NULL);
  gtk_inscription_set_nat_chars (GTK_INSCRIPTION (inscription), 25);
  gtk_list_item_set_child (list_item, inscription);
}

static void
bind_label_cb (GtkSignalListItemFactory *factory,
               GtkListItem              *list_item)
{
  GtkWidget *inscription;
  gpointer item;
  GBinding *binding = NULL;

  item = gtk_tree_list_row_get_item (gtk_list_item_get_item (list_item));
  inscription = gtk_list_item_get_child (list_item);

  if (GTK_IS_LABEL (item))
    binding = g_object_bind_property (item, "label", inscription, "text", G_BINDING_SYNC_CREATE);
  if (GTK_IS_INSCRIPTION (item))
    binding = g_object_bind_property (item, "text", inscription, "text", G_BINDING_SYNC_CREATE);
  else if (GTK_IS_BUTTON (item))
    binding = g_object_bind_property (item, "label", inscription, "text", G_BINDING_SYNC_CREATE);
  else if (GTK_IS_WINDOW (item))
    binding = g_object_bind_property (item, "title", inscription, "text", G_BINDING_SYNC_CREATE);
  else if (GTK_IS_TREE_VIEW_COLUMN (item))
    binding = g_object_bind_property (item, "title", inscription, "text", G_BINDING_SYNC_CREATE);
  else if (GTK_IS_EDITABLE (item))
    binding = g_object_bind_property (item, "text", inscription, "text", G_BINDING_SYNC_CREATE);
  else
    gtk_inscription_set_text (GTK_INSCRIPTION (inscription), NULL);

  g_object_unref (item);

  if (binding)
    g_object_set_data (G_OBJECT (inscription), "binding", binding);
}

static void
unbind_label_cb (GtkSignalListItemFactory *factory,
                 GtkListItem              *list_item)
{
  GtkWidget *inscription;
  GBinding *binding;

  inscription = gtk_list_item_get_child (list_item);
  binding = g_object_steal_data (G_OBJECT (inscription), "binding");
  if (binding)
    g_binding_unbind (binding);
}

static GListModel *
create_model_for_object (gpointer object,
                         gpointer user_data)
{
  return object_get_children (object);
}

static gboolean
toplevel_filter_func (gpointer item,
                      gpointer data)
{
  GdkDisplay *display = data;
  gpointer iw;

  if (!GTK_IS_WINDOW (item))
    return FALSE;

  if (gtk_widget_get_display (item) != display)
    return FALSE;

  iw = g_object_get_data (G_OBJECT (display), "-gtk-inspector");
  if (iw == item)
    return FALSE;

  return TRUE;
}

static GListModel *
create_root_model (GdkDisplay *display)
{
  GtkFilter *filter;
  GtkFilterListModel *filter_model;
  GListModel *model;
  GListStore *list, *special;
  gpointer item;

  list = g_list_store_new (G_TYPE_LIST_MODEL);

  special = g_list_store_new (G_TYPE_OBJECT);
  item = g_application_get_default ();
  if (item)
    g_list_store_append (special, item);
  g_list_store_append (special, gtk_settings_get_for_display (display));
  g_list_store_append (list, special);
  g_object_unref (special);

  filter = GTK_FILTER (gtk_custom_filter_new (toplevel_filter_func, display, NULL));
  model = gtk_window_get_toplevels ();
  filter_model = gtk_filter_list_model_new (g_object_ref (model), filter);
  g_list_store_append (list, filter_model);
  g_object_unref (filter_model);

  return G_LIST_MODEL (gtk_flatten_list_model_new (G_LIST_MODEL (list)));
}

static void
gtk_inspector_object_tree_init (GtkInspectorObjectTree *wt)
{
  wt->priv = gtk_inspector_object_tree_get_instance_private (wt);
  gtk_widget_init_template (GTK_WIDGET (wt));

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (wt->priv->search_bar),
                                GTK_EDITABLE (wt->priv->search_entry));
}

static void
gtk_inspector_object_tree_dispose (GObject *object)
{
  GtkInspectorObjectTree *wt = GTK_INSPECTOR_OBJECT_TREE (object);

  g_clear_object (&wt->priv->tree_model);
  g_clear_object (&wt->priv->selection);

  G_OBJECT_CLASS (gtk_inspector_object_tree_parent_class)->dispose (object);
}

static void
gtk_inspector_object_tree_class_init (GtkInspectorObjectTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_inspector_object_tree_dispose;

  widget_class->map = map;
  widget_class->unmap = unmap;

  signals[OBJECT_ACTIVATED] =
      g_signal_new ("object-activated",
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET (GtkInspectorObjectTreeClass, object_activated),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1, G_TYPE_OBJECT);

  signals[OBJECT_SELECTED] =
      g_signal_new ("object-selected",
                    G_OBJECT_CLASS_TYPE (klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
                    G_STRUCT_OFFSET (GtkInspectorObjectTreeClass, object_selected),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1, G_TYPE_OBJECT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/object-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, list);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectTree, search_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, next_match);
  gtk_widget_class_bind_template_callback (widget_class, previous_match);
  gtk_widget_class_bind_template_callback (widget_class, stop_search);
  gtk_widget_class_bind_template_callback (widget_class, setup_type_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_type_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_type_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, setup_label_cb);
  gtk_widget_class_bind_template_callback (widget_class, bind_label_cb);
  gtk_widget_class_bind_template_callback (widget_class, unbind_label_cb);
}

static guint
model_get_item_index (GListModel *model,
                      gpointer    item)
{
  gpointer cmp;
  guint i;

  for (i = 0; (cmp = g_list_model_get_item (model, i)); i++)
    {
      if (cmp == item)
        {
          g_object_unref (cmp);
          return i;
        }
      g_object_unref (cmp);
    }

  return G_MAXUINT;
}

static GtkTreeListRow *
find_and_expand_object (GtkTreeListModel *model,
                        GObject          *object)
{
  GtkTreeListRow *result;
  GObject *parent;
  guint pos;

  parent = object_get_parent (object);
  if (parent)
    {
      GtkTreeListRow *parent_row = find_and_expand_object (model, parent);
      if (parent_row == NULL)
        return NULL;

      gtk_tree_list_row_set_expanded (parent_row, TRUE);
      pos = model_get_item_index (gtk_tree_list_row_get_children (parent_row), object);
      result = gtk_tree_list_row_get_child_row (parent_row, pos);
      g_object_unref (parent_row);
    }
  else
    {
      pos = model_get_item_index (gtk_tree_list_model_get_model (model), object);
      result = gtk_tree_list_model_get_child_row (model, pos);
    }
  
  return result;
}

void
gtk_inspector_object_tree_select_object (GtkInspectorObjectTree *wt,
                                         GObject                *object)
{
  GtkTreeListRow *row_item;

  row_item = find_and_expand_object (wt->priv->tree_model, object);
  if (row_item == NULL)
    return;

  gtk_column_view_scroll_to (GTK_COLUMN_VIEW (wt->priv->list),
                             gtk_tree_list_row_get_position (row_item),
                             NULL,
                             GTK_LIST_SCROLL_SELECT | GTK_LIST_SCROLL_FOCUS,
                             NULL);

  g_signal_emit (wt, signals[OBJECT_SELECTED], 0, object);
  g_object_unref (row_item);
}

static void
on_selected_item (GtkSingleSelection     *selection,
                  GParamSpec             *pspec,
                  GtkInspectorObjectTree *wt)
{
  GObject *selected = gtk_single_selection_get_selected_item (selection);
  GtkTreeListRow *row = GTK_TREE_LIST_ROW (selected);
  GObject *object = gtk_tree_list_row_get_item (row);
  g_signal_emit (wt, signals[OBJECT_SELECTED], 0, object);
  g_object_unref (object);
}

void
gtk_inspector_object_tree_set_display (GtkInspectorObjectTree *wt,
                                       GdkDisplay *display)
{
  wt->priv->tree_model = gtk_tree_list_model_new (create_root_model (display),
                                                  FALSE,
                                                  FALSE,
                                                  create_model_for_object,
                                                  NULL,
                                                  NULL);
  wt->priv->selection = gtk_single_selection_new (g_object_ref (G_LIST_MODEL (wt->priv->tree_model)));
  gtk_column_view_set_model (GTK_COLUMN_VIEW (wt->priv->list),
                             GTK_SELECTION_MODEL (wt->priv->selection));
  g_signal_connect (wt->priv->selection, "notify::selected-item", G_CALLBACK (on_selected_item), wt);
}
