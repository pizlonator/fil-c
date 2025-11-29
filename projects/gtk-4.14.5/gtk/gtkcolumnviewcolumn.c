/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcolumnviewcolumnprivate.h"
#include "gtkcolumnviewsorterprivate.h"

#include "gtkcolumnviewprivate.h"
#include "gtkcolumnviewrowwidgetprivate.h"
#include "gtkcolumnviewtitleprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkmain.h"
#include "gtkprivate.h"
#include "gtkrbtreeprivate.h"
#include "gtksizegroup.h"
#include "gtkwidgetprivate.h"
#include "gtksorter.h"

/**
 * GtkColumnViewColumn:
 *
 * `GtkColumnViewColumn` represents the columns being added to a `GtkColumnView`.
 *
 * The main ingredient for a `GtkColumnViewColumn` is the `GtkListItemFactory`
 * that tells the columnview how to create cells for this column from items in
 * the model.
 *
 * Columns have a title, and can optionally have a header menu set
 * with [method@Gtk.ColumnViewColumn.set_header_menu].
 *
 * A sorter can be associated with a column using
 * [method@Gtk.ColumnViewColumn.set_sorter], to let users influence sorting
 * by clicking on the column header.
 */

struct _GtkColumnViewColumn
{
  GObject parent_instance;

  GtkListItemFactory *factory;
  char *title;
  char *id;
  GtkSorter *sorter;

  /* data for the view */
  GtkColumnView *view;
  GtkWidget *header;

  int minimum_size_request;
  int natural_size_request;
  int allocation_offset;
  int allocation_size;
  int header_position;

  int fixed_width;

  guint visible     : 1;
  guint resizable   : 1;
  guint expand      : 1;

  GMenuModel *menu;

  /* This list isn't sorted - this is just caching for performance */
  GtkColumnViewCellWidget *first_cell; /* no reference, just caching */
};

struct _GtkColumnViewColumnClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_0,
  PROP_COLUMN_VIEW,
  PROP_FACTORY,
  PROP_TITLE,
  PROP_SORTER,
  PROP_VISIBLE,
  PROP_HEADER_MENU,
  PROP_RESIZABLE,
  PROP_EXPAND,
  PROP_FIXED_WIDTH,
  PROP_ID,

  N_PROPS
};

G_DEFINE_TYPE (GtkColumnViewColumn, gtk_column_view_column, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_column_view_column_dispose (GObject *object)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  g_assert (self->view == NULL); /* would hold a ref otherwise */
  g_assert (self->first_cell == NULL); /* no view = no children */

  g_clear_object (&self->factory);
  g_clear_object (&self->sorter);
  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->menu);
  g_clear_pointer (&self->id, g_free);

  G_OBJECT_CLASS (gtk_column_view_column_parent_class)->dispose (object);
}

static void
gtk_column_view_column_get_property (GObject    *object,
                                     guint       property_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  switch (property_id)
    {
    case PROP_COLUMN_VIEW:
      g_value_set_object (value, self->view);
      break;

    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    case PROP_SORTER:
      g_value_set_object (value, self->sorter);
      break;

    case PROP_VISIBLE:
      g_value_set_boolean (value, self->visible);
      break;

    case PROP_HEADER_MENU:
      g_value_set_object (value, self->menu);
      break;

    case PROP_RESIZABLE:
      g_value_set_boolean (value, self->resizable);
      break;

    case PROP_EXPAND:
      g_value_set_boolean (value, self->expand);
      break;

    case PROP_FIXED_WIDTH:
      g_value_set_int (value, self->fixed_width);
      break;

    case PROP_ID:
      g_value_set_string (value, self->id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_column_set_property (GObject      *object,
                                     guint         property_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkColumnViewColumn *self = GTK_COLUMN_VIEW_COLUMN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_column_view_column_set_factory (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      gtk_column_view_column_set_title (self, g_value_get_string (value));
      break;

    case PROP_SORTER:
      gtk_column_view_column_set_sorter (self, g_value_get_object (value));
      break;

    case PROP_VISIBLE:
      gtk_column_view_column_set_visible (self, g_value_get_boolean (value));
      break;

    case PROP_HEADER_MENU:
      gtk_column_view_column_set_header_menu (self, g_value_get_object (value));
      break;

    case PROP_RESIZABLE:
      gtk_column_view_column_set_resizable (self, g_value_get_boolean (value));
      break;

    case PROP_EXPAND:
      gtk_column_view_column_set_expand (self, g_value_get_boolean (value));
      break;

    case PROP_FIXED_WIDTH:
      gtk_column_view_column_set_fixed_width (self, g_value_get_int (value));
      break;

    case PROP_ID:
      gtk_column_view_column_set_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_column_view_column_class_init (GtkColumnViewColumnClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_column_view_column_dispose;
  gobject_class->get_property = gtk_column_view_column_get_property;
  gobject_class->set_property = gtk_column_view_column_set_property;

  /**
   * GtkColumnViewColumn:column-view: (attributes org.gtk.Property.get=gtk_column_view_column_get_column_view)
   *
   * The `GtkColumnView` this column is a part of.
   */
  properties[PROP_COLUMN_VIEW] =
    g_param_spec_object ("column-view", NULL, NULL,
                         GTK_TYPE_COLUMN_VIEW,
                         G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:factory: (attributes org.gtk.Property.get=gtk_column_view_column_get_factory org.gtk.Property.set=gtk_column_view_column_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:title: (attributes org.gtk.Property.get=gtk_column_view_column_get_title org.gtk.Property.set=gtk_column_view_column_set_title)
   *
   * Title displayed in the header.
   */
  properties[PROP_TITLE] =
    g_param_spec_string ("title", NULL, NULL,
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkColumnViewColumn:sorter: (attributes org.gtk.Property.get=gtk_column_view_column_get_sorter org.gtk.Property.set=gtk_column_view_column_set_sorter)
   *
   * Sorter for sorting items according to this column.
   */
  properties[PROP_SORTER] =
    g_param_spec_object ("sorter", NULL, NULL,
                         GTK_TYPE_SORTER,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:visible: (attributes org.gtk.Property.get=gtk_column_view_column_get_visible org.gtk.Property.set=gtk_column_view_column_set_visible)
   *
   * Whether this column is visible.
   */
  properties[PROP_VISIBLE] =
    g_param_spec_boolean ("visible", NULL, NULL,
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:header-menu: (attributes org.gtk.Property.get=gtk_column_view_column_get_header_menu org.gtk.Property.set=gtk_column_view_column_set_header_menu)
   *
   * Menu model used to create the context menu for the column header.
   */
  properties[PROP_HEADER_MENU] =
    g_param_spec_object ("header-menu", NULL, NULL,
                         G_TYPE_MENU_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:resizable: (attributes org.gtk.Property.get=gtk_column_view_column_get_resizable org.gtk.Property.set=gtk_column_view_column_set_resizable)
   *
   * Whether this column is resizable.
   */
  properties[PROP_RESIZABLE] =
    g_param_spec_boolean ("resizable", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:expand: (attributes org.gtk.Property.get=gtk_column_view_column_get_expand org.gtk.Property.set=gtk_column_view_column_set_expand)
   *
   * Column gets share of extra width allocated to the view.
   */
  properties[PROP_EXPAND] =
    g_param_spec_boolean ("expand", NULL, NULL,
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:fixed-width: (attributes org.gtk.Property.get=gtk_column_view_column_get_fixed_width org.gtk.Property.set=gtk_column_view_column_set_fixed_width)
   *
   * If not -1, this is the width that the column is allocated,
   * regardless of the size of its content.
   */
  properties[PROP_FIXED_WIDTH] =
    g_param_spec_int ("fixed-width", NULL, NULL,
                      -1, G_MAXINT, -1,
                      G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewColumn:id: (attributes org.gtk.Property.get=gtk_column_view_column_get_id org.gtk.Property.set=gtk_column_view_column_set_id)
   *
   * An ID for the column.
   *
   * GTK is not currently using the ID for anything, but
   * it can be used by applications when saving column view
   * configurations.
   *
   * It is up to applications to ensure uniqueness of IDs.
   *
   * Since: 4.10
   */
  properties[PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_column_view_column_init (GtkColumnViewColumn *self)
{
  self->minimum_size_request = -1;
  self->natural_size_request = -1;
  self->visible = TRUE;
  self->resizable = FALSE;
  self->expand = FALSE;
  self->fixed_width = -1;
}

/**
 * gtk_column_view_column_new:
 * @title: (nullable): Title to use for this column
 * @factory: (transfer full) (nullable): The factory to populate items with
 *
 * Creates a new `GtkColumnViewColumn` that uses the given @factory for
 * mapping items to widgets.
 *
 * You most likely want to call [method@Gtk.ColumnView.append_column] next.
 *
 * The function takes ownership of the argument, so you can write code like:
 *
 * ```c
 * column = gtk_column_view_column_new (_("Name"),
 *   gtk_builder_list_item_factory_new_from_resource ("/name.ui"));
 * ```
 *
 * Returns: a new `GtkColumnViewColumn` using the given @factory
 */
GtkColumnViewColumn *
gtk_column_view_column_new (const char         *title,
                            GtkListItemFactory *factory)
{
  GtkColumnViewColumn *result;

  g_return_val_if_fail (factory == NULL || GTK_IS_LIST_ITEM_FACTORY (factory), NULL);

  result = g_object_new (GTK_TYPE_COLUMN_VIEW_COLUMN,
                         "factory", factory,
                         "title", title,
                         NULL);

  g_clear_object (&factory);

  return result;
}

GtkColumnViewCellWidget *
gtk_column_view_column_get_first_cell (GtkColumnViewColumn *self)
{
  return self->first_cell;
}

void
gtk_column_view_column_add_cell (GtkColumnViewColumn *self,
                                 GtkColumnViewCellWidget   *cell)
{
  self->first_cell = cell;

  gtk_widget_set_visible (GTK_WIDGET (cell), self->visible);
  gtk_column_view_column_queue_resize (self);
}

void
gtk_column_view_column_remove_cell (GtkColumnViewColumn *self,
                                    GtkColumnViewCellWidget   *cell)
{
  if (cell == self->first_cell)
    self->first_cell = gtk_column_view_cell_widget_get_next (cell);

  gtk_column_view_column_queue_resize (self);
  gtk_widget_queue_resize (GTK_WIDGET (cell));
}

void
gtk_column_view_column_queue_resize (GtkColumnViewColumn *self)
{
  GtkColumnViewCellWidget *cell;

  if (self->minimum_size_request < 0)
    return;

  self->minimum_size_request = -1;
  self->natural_size_request = -1;

  if (self->header)
    gtk_widget_queue_resize (self->header);

  for (cell = self->first_cell; cell; cell = gtk_column_view_cell_widget_get_next (cell))
    {
      gtk_widget_queue_resize (GTK_WIDGET (cell));
    }
}

void
gtk_column_view_column_measure (GtkColumnViewColumn *self,
                                int                 *minimum,
                                int                 *natural)
{
  if (self->fixed_width > -1)
    {
      self->minimum_size_request  = self->fixed_width;
      self->natural_size_request  = self->fixed_width;
    }

  if (self->minimum_size_request < 0)
    {
      GtkColumnViewCellWidget *cell;
      int min, nat, cell_min, cell_nat;

      if (self->header)
        {
          gtk_widget_measure (self->header, GTK_ORIENTATION_HORIZONTAL, -1, &min, &nat, NULL, NULL);
        }
      else
        {
          min = 0;
          nat = 0;
        }

      for (cell = self->first_cell; cell; cell = gtk_column_view_cell_widget_get_next (cell))
        {
          gtk_widget_measure (GTK_WIDGET (cell),
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &cell_min, &cell_nat,
                              NULL, NULL);

          min = MAX (min, cell_min);
          nat = MAX (nat, cell_nat);
        }

      self->minimum_size_request = min;
      self->natural_size_request = nat;
    }

  *minimum = self->minimum_size_request;
  *natural = self->natural_size_request;
}

void
gtk_column_view_column_allocate (GtkColumnViewColumn *self,
                                 int                  offset,
                                 int                  size)
{
  self->allocation_offset = offset;
  self->allocation_size = size;
  self->header_position = offset;
}

void
gtk_column_view_column_get_allocation (GtkColumnViewColumn *self,
                                       int                 *offset,
                                       int                 *size)
{
  if (offset)
    *offset = self->allocation_offset;
  if (size)
    *size = self->allocation_size;
}

static void
gtk_column_view_column_create_cells (GtkColumnViewColumn *self)
{
  GtkListView *list;
  GtkWidget *row;

  if (self->first_cell)
    return;

  list = gtk_column_view_get_list_view (GTK_COLUMN_VIEW (self->view));
  for (row = gtk_widget_get_first_child (GTK_WIDGET (list));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      GtkColumnViewRowWidget *list_item;
      GtkListItemBase *base;
      GtkWidget *cell;

      list_item = GTK_COLUMN_VIEW_ROW_WIDGET (row);
      base = GTK_LIST_ITEM_BASE (row);
      cell = gtk_column_view_cell_widget_new (self, gtk_column_view_is_inert (self->view));
      gtk_column_view_row_widget_add_child (list_item, cell);
      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (cell),
                                 gtk_list_item_base_get_position (base),
                                 gtk_list_item_base_get_item (base),
                                 gtk_list_item_base_get_selected (base));
    }
}

static void
gtk_column_view_column_remove_cells (GtkColumnViewColumn *self)
{
  while (self->first_cell)
    gtk_column_view_cell_widget_remove (self->first_cell);
}

static void
gtk_column_view_column_create_header (GtkColumnViewColumn *self)
{
  if (self->header != NULL)
    return;

  self->header = gtk_column_view_title_new (self);
  gtk_widget_set_visible (self->header, self->visible);
  gtk_column_view_row_widget_add_child (gtk_column_view_get_header_widget (self->view),
                                  self->header);
  gtk_column_view_column_queue_resize (self);
}

static void
gtk_column_view_column_remove_header (GtkColumnViewColumn *self)
{
  if (self->header == NULL)
    return;

  gtk_column_view_row_widget_remove_child (gtk_column_view_get_header_widget (self->view),
                                     self->header);
  self->header = NULL;
  gtk_column_view_column_queue_resize (self);
}

static void
gtk_column_view_column_ensure_cells (GtkColumnViewColumn *self)
{
  if (self->view && gtk_column_view_column_get_visible (self))
    gtk_column_view_column_create_cells (self);
  else
    gtk_column_view_column_remove_cells (self);

  if (self->view)
    gtk_column_view_column_create_header (self);
  else
    gtk_column_view_column_remove_header (self);
}

/**
 * gtk_column_view_column_get_column_view: (attributes org.gtk.Method.get_property=column-view)
 * @self: a `GtkColumnViewColumn`
 *
 * Gets the column view that's currently displaying this column.
 *
 * If @self has not been added to a column view yet, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The column view displaying @self.
 */
GtkColumnView *
gtk_column_view_column_get_column_view (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->view;
}

void
gtk_column_view_column_set_column_view (GtkColumnViewColumn *self,
                                        GtkColumnView       *view)
{
  if (self->view == view)
    return;

  gtk_column_view_column_remove_cells (self);
  gtk_column_view_column_remove_header (self);

  self->view = view;

  gtk_column_view_column_ensure_cells (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COLUMN_VIEW]);
}

void
gtk_column_view_column_set_position (GtkColumnViewColumn *self,
                                     guint                position)
{
  GtkColumnViewCellWidget *cell;

  gtk_column_view_row_widget_reorder_child (gtk_column_view_get_header_widget (self->view),
                                      self->header,
                                      position);

  for (cell = self->first_cell; cell; cell = gtk_column_view_cell_widget_get_next (cell))
    {
      GtkColumnViewRowWidget *list_item;

      list_item = GTK_COLUMN_VIEW_ROW_WIDGET (gtk_widget_get_parent (GTK_WIDGET (cell)));
      gtk_column_view_row_widget_reorder_child (list_item, GTK_WIDGET (cell), position);
    }
}

/**
 * gtk_column_view_column_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkColumnViewColumn`
 *
 * Gets the factory that's currently used to populate list items for
 * this column.
 *
 * Returns: (nullable) (transfer none): The factory in use
 **/
GtkListItemFactory *
gtk_column_view_column_get_factory (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->factory;
}

void
gtk_column_view_column_update_factory (GtkColumnViewColumn *self,
                                       gboolean             inert)
{
  GtkListItemFactory *factory;
  GtkColumnViewCellWidget *cell;

  if (self->factory == NULL)
    return;

  if (inert)
    factory = NULL;
  else
    factory = self->factory;

  for (cell = self->first_cell;
       cell;
       cell = gtk_column_view_cell_widget_get_next (cell))
    {
      gtk_list_factory_widget_set_factory (GTK_LIST_FACTORY_WIDGET (cell), factory);
    }
}

/**
 * gtk_column_view_column_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkColumnViewColumn`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items for this
 * column.
 */
void
gtk_column_view_column_set_factory (GtkColumnViewColumn *self,
                                    GtkListItemFactory  *factory)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (self->factory && !factory)
    gtk_column_view_column_update_factory (self, TRUE);

  if (!g_set_object (&self->factory, factory))
    return;

  if (self->view && !gtk_column_view_is_inert (self->view))
    gtk_column_view_column_update_factory (self, FALSE);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_column_view_column_set_title: (attributes org.gtk.Method.set_property=title)
 * @self: a `GtkColumnViewColumn`
 * @title: (nullable): Title to use for this column
 *
 * Sets the title of this column.
 *
 * The title is displayed in the header of a `GtkColumnView`
 * for this column and is therefore user-facing text that should
 * be translated.
 */
void
gtk_column_view_column_set_title (GtkColumnViewColumn *self,
                                  const char          *title)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (g_strcmp0 (self->title, title) == 0)
    return;

  g_free (self->title);
  self->title = g_strdup (title);

  if (self->header)
    gtk_column_view_title_set_title (GTK_COLUMN_VIEW_TITLE (self->header), title);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

/**
 * gtk_column_view_column_get_title: (attributes org.gtk.Method.get_property=title)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns the title set with gtk_column_view_column_set_title().
 *
 * Returns: (nullable): The column's title
 */
const char *
gtk_column_view_column_get_title (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), FALSE);

  return self->title;
}

static void
gtk_column_view_column_remove_from_sorter (GtkColumnViewColumn *self)
{
  if (self->view == NULL)
    return;
  
  gtk_column_view_sorter_remove_column (GTK_COLUMN_VIEW_SORTER (gtk_column_view_get_sorter (self->view)), self);
}

/**
 * gtk_column_view_column_set_sorter: (attributes org.gtk.Method.set_property=sorter)
 * @self: a `GtkColumnViewColumn`
 * @sorter: (nullable): the `GtkSorter` to associate with @column
 *
 * Associates a sorter with the column.
 *
 * If @sorter is %NULL, the column will not let users change
 * the sorting by clicking on its header.
 *
 * This sorter can be made active by clicking on the column
 * header, or by calling [method@Gtk.ColumnView.sort_by_column].
 *
 * See [method@Gtk.ColumnView.get_sorter] for the necessary steps
 * for setting up customizable sorting for [class@Gtk.ColumnView].
 */
void
gtk_column_view_column_set_sorter (GtkColumnViewColumn *self,
                                   GtkSorter           *sorter)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));
  g_return_if_fail (sorter == NULL || GTK_IS_SORTER (sorter));

  if (!g_set_object (&self->sorter, sorter))
    return;

  gtk_column_view_column_remove_from_sorter (self);

  if (self->header)
    gtk_column_view_title_update_sort (GTK_COLUMN_VIEW_TITLE (self->header));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SORTER]);
}

/**
 * gtk_column_view_column_get_sorter: (attributes org.gtk.Method.get_property=sorter)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns the sorter that is associated with the column.
 *
 * Returns: (nullable) (transfer none): the `GtkSorter` of @self
 */
GtkSorter *
gtk_column_view_column_get_sorter (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->sorter;
}

void
gtk_column_view_column_notify_sort (GtkColumnViewColumn *self)
{
  if (self->header)
    gtk_column_view_title_update_sort (GTK_COLUMN_VIEW_TITLE (self->header));
}

/**
 * gtk_column_view_column_set_visible: (attributes org.gtk.Method.set_property=visible)
 * @self: a `GtkColumnViewColumn`
 * @visible: whether this column should be visible
 *
 * Sets whether this column should be visible in views.
 */
void
gtk_column_view_column_set_visible (GtkColumnViewColumn *self,
                                    gboolean             visible)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (self->visible == visible)
    return;

  self->visible = visible;

  self->minimum_size_request = -1;
  self->natural_size_request = -1;

  if (self->header)
    gtk_widget_set_visible (GTK_WIDGET (self->header), visible);

  gtk_column_view_column_ensure_cells (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VISIBLE]);
}

/**
 * gtk_column_view_column_get_visible: (attributes org.gtk.Method.get_property=visible)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns whether this column is visible.
 *
 * Returns: %TRUE if this column is visible
 */
gboolean
gtk_column_view_column_get_visible (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), TRUE);

  return self->visible;
}

/**
 * gtk_column_view_column_set_header_menu: (attributes org.gtk.Method.set_property=header-menu)
 * @self: a `GtkColumnViewColumn`
 * @menu: (nullable): a `GMenuModel`
 *
 * Sets the menu model that is used to create the context menu
 * for the column header.
 */
void
gtk_column_view_column_set_header_menu (GtkColumnViewColumn *self,
                                        GMenuModel          *menu)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));
  g_return_if_fail (menu == NULL || G_IS_MENU_MODEL (menu));

  if (!g_set_object (&self->menu, menu))
    return;

  if (self->header)
    gtk_column_view_title_set_menu (GTK_COLUMN_VIEW_TITLE (self->header), menu);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADER_MENU]);
}

/**
 * gtk_column_view_column_get_header_menu: (attributes org.gtk.Method.get_property=header-menu)
 * @self: a `GtkColumnViewColumn`
 *
 * Gets the menu model that is used to create the context menu
 * for the column header.
 *
 * Returns: (transfer none) (nullable): the `GMenuModel`
 */
GMenuModel *
gtk_column_view_column_get_header_menu (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->menu;
}

/**
 * gtk_column_view_column_set_expand: (attributes org.gtk.Method.set_property=expand)
 * @self: a `GtkColumnViewColumn`
 * @expand: %TRUE if this column should expand to fill available sace
 *
 * Sets the column to take available extra space.
 *
 * The extra space is shared equally amongst all columns that
 * have the expand set to %TRUE.
 */
void
gtk_column_view_column_set_expand (GtkColumnViewColumn *self,
                                   gboolean             expand)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (self->expand == expand)
    return;

  self->expand = expand;

  if (self->visible && self->view)
    gtk_widget_queue_resize (GTK_WIDGET (self->view));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPAND]);
}

/**
 * gtk_column_view_column_get_expand: (attributes org.gtk.Method.get_property=expand)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns whether this column should expand.
 *
 * Returns: %TRUE if this column expands
 */
gboolean
gtk_column_view_column_get_expand (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), TRUE);

  return self->expand;
}

/**
 * gtk_column_view_column_set_resizable: (attributes org.gtk.Method.set_property=resizable)
 * @self: a `GtkColumnViewColumn`
 * @resizable: whether this column should be resizable
 *
 * Sets whether this column should be resizable by dragging.
 */
void
gtk_column_view_column_set_resizable (GtkColumnViewColumn *self,
                                      gboolean             resizable)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (self->resizable == resizable)
    return;

  self->resizable = resizable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_RESIZABLE]);
}

/**
 * gtk_column_view_column_get_resizable: (attributes org.gtk.Method.get_property=resizable)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns whether this column is resizable.
 *
 * Returns: %TRUE if this column is resizable
 */
gboolean
gtk_column_view_column_get_resizable (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), TRUE);

  return self->resizable;
}

/**
 * gtk_column_view_column_set_fixed_width: (attributes org.gtk.Method.set_property=fixed-width)
 * @self: a `GtkColumnViewColumn`
 * @fixed_width: the new fixed width, or -1
 *
 * If @fixed_width is not -1, sets the fixed width of @column;
 * otherwise unsets it.
 *
 * Setting a fixed width overrides the automatically calculated
 * width. Interactive resizing also sets the “fixed-width” property.
 */
void
gtk_column_view_column_set_fixed_width (GtkColumnViewColumn *self,
                                        int                  fixed_width)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));
  g_return_if_fail (fixed_width >= -1);

  if (self->fixed_width == fixed_width)
    return;

  self->fixed_width = fixed_width;

  gtk_column_view_column_queue_resize (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FIXED_WIDTH]);
}

/**
 * gtk_column_view_column_get_fixed_width: (attributes org.gtk.Method.get_property=fixed-width)
 * @self: a `GtkColumnViewColumn`
 *
 * Gets the fixed width of the column.
 *
 * Returns: the fixed with of the column
 */
int
gtk_column_view_column_get_fixed_width (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), -1);

  return self->fixed_width;
}

GtkWidget *
gtk_column_view_column_get_header (GtkColumnViewColumn *self)
{
  return self->header;
}

void
gtk_column_view_column_set_header_position (GtkColumnViewColumn *self,
                                            int                  offset)
{
  self->header_position = offset;
}

void
gtk_column_view_column_get_header_allocation (GtkColumnViewColumn *self,
                                              int                 *offset,
                                              int                 *size)
{
  if (offset)
    *offset = self->header_position;

  if (size)
    *size = self->allocation_size;
}

/**
 * gtk_column_view_column_set_id: (attributes org.gtk.Method.set_property=id)
 * @self: a `GtkColumnViewColumn`
 * @id: (nullable): ID to use for this column
 *
 * Sets the id of this column.
 *
 * GTK makes no use of this, but applications can use it when
 * storing column view configuration.
 *
 * It is up to callers to ensure uniqueness of IDs.
 *
 * Since: 4.10
 */
void
gtk_column_view_column_set_id (GtkColumnViewColumn *self,
                               const char          *id)
{
  g_return_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self));

  if (g_strcmp0 (self->id, id) == 0)
    return;

  g_free (self->id);
  self->id = g_strdup (id);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ID]);
}

/**
 * gtk_column_view_column_get_id: (attributes org.gtk.Method.get_property=id)
 * @self: a `GtkColumnViewColumn`
 *
 * Returns the ID set with gtk_column_view_column_set_id().
 *
 * Returns: (nullable): The column's ID
 *
 * Since: 4.10
 */
const char *
gtk_column_view_column_get_id (GtkColumnViewColumn *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (self), NULL);

  return self->id;
}

