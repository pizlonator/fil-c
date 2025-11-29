/*
 * Copyright © 2019 Matthias Clasen
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkcolumnviewsorterprivate.h"

#include "gtkcolumnviewcolumnprivate.h"
#include "gtktypebuiltins.h"

/* {{{ GObject implementation */

typedef struct
{
  GtkColumnViewColumn *column;
  GtkSorter *sorter;
  gboolean   inverted;
  gulong     changed_id;
} Sorter;

static void
free_sorter (gpointer data)
{
  Sorter *s = data;

  g_signal_handler_disconnect (s->sorter, s->changed_id);
  g_object_unref (s->sorter);
  g_object_unref (s->column);
  g_free (s);
}

/**
 * GtkColumnViewSorter:
 *
 * `GtkColumnViewSorter` is a sorter implementation that
 * is geared towards the needs of `GtkColumnView`.
 *
 * The sorter returned by [method@Gtk.ColumnView.get_sorter] is
 * a `GtkColumnViewSorter`.
 *
 * In column views, sorting can be configured by associating
 * sorters with columns, and users can invert sort order by clicking
 * on column headers. The API of `GtkColumnViewSorter` is designed
 * to allow saving and restoring this configuration.
 *
 * If you are only interested in the primary sort column (i.e. the
 * column where a sort indicator is shown in the header), then
 * you can just look at [property@Gtk.ColumnViewSorter:primary-sort-column]
 * and [property@Gtk.ColumnViewSorter:primary-sort-order].
 *
 * If you want to store the full sort configuration, including
 * secondary sort columns that are used for tie breaking, then
 * you can use [method@Gtk.ColumnViewSorter.get_nth_sort_column].
 * To get notified about changes, use [signal@Gtk.Sorter::changed].
 *
 * To restore a saved sort configuration on a `GtkColumnView`,
 * use code like:
 *
 * ```
 * sorter = gtk_column_view_get_sorter (view);
 * for (i = gtk_column_view_sorter_get_n_sort_columns (sorter) - 1; i >= 0; i--)
 *   {
 *     column = gtk_column_view_sorter_get_nth_sort_column (sorter, i, &order);
 *     gtk_column_view_sort_by_column (view, column, order);
 *   }
 * ```
 *
 * Since: 4.10
 */
struct _GtkColumnViewSorter
{
  GtkSorter parent_instance;

  GSequence *sorters;
};

enum
{
  PROP_PRIMARY_SORT_COLUMN = 1,
  PROP_PRIMARY_SORT_ORDER,

  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

G_DEFINE_TYPE (GtkColumnViewSorter, gtk_column_view_sorter, GTK_TYPE_SORTER)

static GtkOrdering
gtk_column_view_sorter_compare (GtkSorter *sorter,
                                gpointer   item1,
                                gpointer   item2)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (sorter);
  GtkOrdering result = GTK_ORDERING_EQUAL;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (self->sorters);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      Sorter *s = g_sequence_get (iter);

      result = gtk_sorter_compare (s->sorter, item1, item2);
      if (s->inverted)
        result = - result;

      if (result != GTK_ORDERING_EQUAL)
        break;
    }

  return result;
}

static GtkSorterOrder
gtk_column_view_sorter_get_order (GtkSorter *sorter)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (sorter);
  GtkSorterOrder result = GTK_SORTER_ORDER_NONE;
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (self->sorters);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      Sorter *s = g_sequence_get (iter);

      switch (gtk_sorter_get_order (s->sorter))
        {
          case GTK_SORTER_ORDER_PARTIAL:
            result = GTK_SORTER_ORDER_PARTIAL;
            break;
          case GTK_SORTER_ORDER_NONE:
            break;
          case GTK_SORTER_ORDER_TOTAL:
            return GTK_SORTER_ORDER_TOTAL;
          default:
            g_assert_not_reached ();
            break;
        }
    }

  return result;
}

static void
gtk_column_view_sorter_dispose (GObject *object)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (object);

  /* The sorter is owned by the columview and is unreffed
   * after the columns, so the sequence must be empty at
   * this point.
   * The sorter can outlive the columview it comes from
   * (the model might still have a ref), but that does
   * not change the fact that all columns will be gone.
   */
  g_assert (g_sequence_is_empty (self->sorters));
  g_clear_pointer (&self->sorters, g_sequence_free);

  G_OBJECT_CLASS (gtk_column_view_sorter_parent_class)->dispose (object);
}

static void
gtk_column_view_sorter_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkColumnViewSorter *self = GTK_COLUMN_VIEW_SORTER (object);

  switch (prop_id)
    {
    case PROP_PRIMARY_SORT_COLUMN:
      g_value_set_object (value, gtk_column_view_sorter_get_primary_sort_column (self));
      break;

    case PROP_PRIMARY_SORT_ORDER:
      g_value_set_enum (value, gtk_column_view_sorter_get_primary_sort_order (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_column_view_sorter_class_init (GtkColumnViewSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_column_view_sorter_compare;
  sorter_class->get_order = gtk_column_view_sorter_get_order;

  object_class->dispose = gtk_column_view_sorter_dispose;
  object_class->get_property = gtk_column_view_sorter_get_property;

  /**
   * GtkColumnViewSorter:primary-sort-column: (attributes org.gtk.Property.get=gtk_column_view_sorter_get_primary_sort_column)
   *
   * The primary sort column.
   *
   * The primary sort column is the one that displays the triangle
   * in a column view header.
   *
   * Since: 4.10
   */
  properties[PROP_PRIMARY_SORT_COLUMN] =
    g_param_spec_object ("primary-sort-column", NULL, NULL,
                         GTK_TYPE_COLUMN_VIEW_COLUMN,
                         G_PARAM_READABLE|G_PARAM_STATIC_STRINGS);

  /**
   * GtkColumnViewSorter:primary-sort-order: (attributes org.gtk.Property.get=gtk_column_view_sorter_get_primary_sort_order)
   *
   * The primary sort order.
   *
   * The primary sort order determines whether the triangle displayed
   * in the column view header of the primary sort column points upwards
   * or downwards.
   *
   * Since: 4.10
   */
  properties[PROP_PRIMARY_SORT_ORDER] =
    g_param_spec_enum ("primary-sort-order", NULL, NULL,
                       GTK_TYPE_SORT_TYPE,
                       GTK_SORT_ASCENDING,
                       G_PARAM_READABLE|G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
gtk_column_view_sorter_init (GtkColumnViewSorter *self)
{
  self->sorters = g_sequence_new (free_sorter);
}

/* }}} */
/* {{{ Private API */

GtkColumnViewSorter *
gtk_column_view_sorter_new (void)
{
  return g_object_new (GTK_TYPE_COLUMN_VIEW_SORTER, NULL);
}

static void
gtk_column_view_sorter_changed_cb (GtkSorter *sorter, int change, gpointer data)
{
  gtk_sorter_changed (GTK_SORTER (data), GTK_SORTER_CHANGE_DIFFERENT);
}

static gboolean
remove_column (GtkColumnViewSorter *self,
               GtkColumnViewColumn *column)
{
  GSequenceIter *iter;

  for (iter = g_sequence_get_begin_iter (self->sorters);
       !g_sequence_iter_is_end (iter);
       iter = g_sequence_iter_next (iter))
    {
      Sorter *s = g_sequence_get (iter);

      if (s->column == column)
        {
          g_sequence_remove (iter);
          return TRUE;
        }
    }

  return FALSE;
}

gboolean
gtk_column_view_sorter_add_column (GtkColumnViewSorter *self,
                                   GtkColumnViewColumn *column)
{
  GSequenceIter *iter;
  GtkSorter *sorter;
  Sorter *s, *first;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);

  sorter = gtk_column_view_column_get_sorter (column);
  if (sorter == NULL)
    return FALSE;

  iter = g_sequence_get_begin_iter (self->sorters);
  if (!g_sequence_iter_is_end (iter))
    {
      first = g_sequence_get (iter);
      if (first->column == column)
        {
          first->inverted = !first->inverted;
          goto out;
        }
    }
  else
    first = NULL;

  remove_column (self, column);

  s = g_new (Sorter, 1);
  s->column = g_object_ref (column);
  s->sorter = g_object_ref (sorter);
  s->changed_id = g_signal_connect (sorter, "changed", G_CALLBACK (gtk_column_view_sorter_changed_cb), self);
  s->inverted = FALSE;

  g_sequence_insert_before (iter, s);

  /* notify the previous first column to stop drawing an arrow */
  if (first)
    gtk_column_view_column_notify_sort (first->column);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_COLUMN]);
out:
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_ORDER]);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  gtk_column_view_column_notify_sort (column);

  return TRUE;
}

gboolean
gtk_column_view_sorter_remove_column (GtkColumnViewSorter *self,
                                      GtkColumnViewColumn *column)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);

  if (remove_column (self, column))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_COLUMN]);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_ORDER]);

      gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);
      gtk_column_view_column_notify_sort (column);
      return TRUE;
    }

  return FALSE;
}

gboolean
gtk_column_view_sorter_set_column (GtkColumnViewSorter *self,
                                   GtkColumnViewColumn *column,
                                   gboolean             inverted)
{
  GtkSorter *sorter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), FALSE);
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_COLUMN (column), FALSE);

  sorter = gtk_column_view_column_get_sorter (column);
  if (sorter == NULL)
    return FALSE;

  g_object_ref (column);

  g_sequence_remove_range (g_sequence_get_begin_iter (self->sorters),
                           g_sequence_get_end_iter (self->sorters));

  s = g_new (Sorter, 1);
  s->column = g_object_ref (column);
  s->sorter = g_object_ref (sorter);
  s->changed_id = g_signal_connect (sorter, "changed", G_CALLBACK (gtk_column_view_sorter_changed_cb), self);
  s->inverted = inverted;

  g_sequence_prepend (self->sorters, s);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_COLUMN]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_ORDER]);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  gtk_column_view_column_notify_sort (column);

  g_object_unref (column);

  return TRUE;
}

void
gtk_column_view_sorter_clear (GtkColumnViewSorter *self)
{
  GSequenceIter *iter;
  Sorter *s;
  GtkColumnViewColumn *column;

  g_return_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self));

  if (g_sequence_is_empty (self->sorters))
    return;

  iter = g_sequence_get_begin_iter (self->sorters);
  s = g_sequence_get (iter);

  column = g_object_ref (s->column);

  g_sequence_remove_range (iter, g_sequence_get_end_iter (self->sorters));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_COLUMN]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_SORT_ORDER]);

  gtk_sorter_changed (GTK_SORTER (self), GTK_SORTER_CHANGE_DIFFERENT);

  gtk_column_view_column_notify_sort (column);

  g_object_unref (column);
}

GtkColumnViewColumn *
gtk_column_view_sorter_get_sort_column (GtkColumnViewSorter *self,
                                        gboolean            *inverted)
{
  GSequenceIter *iter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), NULL);

  if (g_sequence_is_empty (self->sorters))
    return NULL;

  iter = g_sequence_get_begin_iter (self->sorters);
  s = g_sequence_get (iter);

  *inverted = s->inverted;

  return s->column;
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_column_view_sorter_get_primary_sort_column:
 * @self: a `GtkColumnViewSorter`
 *
 * Returns the primary sort column.
 *
 * The primary sort column is the one that displays the triangle
 * in a column view header.
 *
 * Returns: (nullable) (transfer none): the primary sort column
 *
 * Since: 4.10
 */
GtkColumnViewColumn *
gtk_column_view_sorter_get_primary_sort_column (GtkColumnViewSorter *self)
{
  GSequenceIter *iter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), NULL);

  iter = g_sequence_get_begin_iter (self->sorters);
  if (g_sequence_iter_is_end (iter))
    return NULL;

  s = g_sequence_get (iter);

  return s->column;
}

/**
 * gtk_column_view_sorter_get_primary_sort_order:
 * @self: a `GtkColumnViewSorter`
 *
 * Returns the primary sort order.
 *
 * The primary sort order determines whether the triangle displayed
 * in the column view header of the primary sort column points upwards
 * or downwards.
 *
 * If there is no primary sort column, then this function returns
 * `GTK_SORT_ASCENDING`.
 *
 * Returns: the primary sort order
 *
 * Since: 4.10
 */
GtkSortType
gtk_column_view_sorter_get_primary_sort_order (GtkColumnViewSorter *self)
{
  GSequenceIter *iter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), GTK_SORT_ASCENDING);

  iter = g_sequence_get_begin_iter (self->sorters);
  if (g_sequence_iter_is_end (iter))
    return GTK_SORT_ASCENDING;

  s = g_sequence_get (iter);

  return s->inverted ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
}

/**
 * gtk_column_view_sorter_get_n_sort_columns:
 * @self: a `GtkColumnViewSorter`
 *
 * Returns the number of columns by which the sorter sorts.
 *
 * If the sorter of the primary sort column does not determine
 * a total order, then the secondary sorters are consulted to
 * break the ties.
 *
 * Use the [signal@Gtk.Sorter::changed] signal to get notified
 * when the number of sort columns changes.
 *
 * Returns: the number of sort columns
 *
 * Since: 4.10
 */
guint
gtk_column_view_sorter_get_n_sort_columns (GtkColumnViewSorter *self)
{
  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), 0);

  return (guint) g_sequence_get_length (self->sorters);
}

/**
 * gtk_column_view_sorter_get_nth_sort_column:
 * @self: a `GtkColumnViewSorter`
 * @position: the position of the sort column to retrieve (0 for the
 *     primary sort column)
 * @sort_order: (out): return location for the sort order
 *
 * Gets the @position'th sort column and its associated sort order.
 *
 * Use the [signal@Gtk.Sorter::changed] signal to get notified
 * when sort columns change.
 *
 * Returns: (nullable) (transfer none): the @positions sort column
 *
 * Since: 4.10
 */
GtkColumnViewColumn *
gtk_column_view_sorter_get_nth_sort_column (GtkColumnViewSorter *self,
                                            guint                position,
                                            GtkSortType         *sort_order)
{
  GSequenceIter *iter;
  Sorter *s;

  g_return_val_if_fail (GTK_IS_COLUMN_VIEW_SORTER (self), NULL);

  iter = g_sequence_get_iter_at_pos (self->sorters, (int) position);

  if (g_sequence_iter_is_end (iter))
    {
      *sort_order = GTK_SORT_ASCENDING;
      return NULL;
    }

  s = g_sequence_get (iter);

  *sort_order = s->inverted ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;

  return s->column;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
