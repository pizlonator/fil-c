/* GtkSorter tests.
 *
 * Copyright (C) 2019, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Matthias Clasen <mclasen@redhat.com>
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

#include <locale.h>

#include <gtk/gtk.h>

static GQuark number_quark;

static guint
get_number (GObject *object)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
}

static guint
get_with_parents (GObject *object)
{
  guint number;

  if (object == NULL)
    return 0;

  if (GTK_IS_TREE_LIST_ROW (object))
    {
      GtkTreeListRow *r;

      r = GTK_TREE_LIST_ROW (object);
      object = gtk_tree_list_row_get_item (r);
      number = 10 * get_with_parents (G_OBJECT (gtk_tree_list_row_get_parent (r)));
      g_object_unref (r);
    }
  else
    number = 0;
  
  number += get_number (object);
  g_object_unref (object);

  return number;
}

static char *
model_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      GObject *object = g_list_model_get_item (model, i);

      if (i > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get_with_parents (object));
      /* no unref since get_with_parents consumes the ref */
    }

  return g_string_free (string, FALSE);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step);

static void
add (GListStore *store,
     guint       number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_append (store, object);
  g_object_unref (object);
}

#define assert_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define assert_not_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
  if (g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " != " #expected, s, "!=", expected); \
  g_free (s); \
}G_STMT_END

/* This could be faster by foreach()ing through the models and comparing
 * the item pointers */
#define assert_model_equal(model1, model2) G_STMT_START{\
  char *s1 = model_to_string (G_LIST_MODEL (model1)); \
  char *s2 = model_to_string (G_LIST_MODEL (model2)); \
  if (!g_str_equal (s1, s2)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model1 " != " #model2, s1, "==", s2); \
  g_free (s2); \
  g_free (s1); \
}G_STMT_END

static GListStore *
new_empty_store (void)
{
  return g_list_store_new (G_TYPE_OBJECT);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step)
{
  GListStore *store = new_empty_store ();
  guint i;

  for (i = start; i <= end; i += step)
    add (store, i);

  return store;
}

static GListModel *
new_child_model (gpointer item,
                 gpointer unused)
{
  guint n = get_number (item);

  if (n == 1)
    return NULL;

  return G_LIST_MODEL (new_store (1, n - 1, 1));
}

static GListModel *
new_model (guint size)
{
  return G_LIST_MODEL (gtk_tree_list_model_new (G_LIST_MODEL (new_store (1, size, 1)),
                                                FALSE,
                                                TRUE,
                                                new_child_model,
                                                NULL, NULL));
}

static void
test_simple (void)
{
  GListModel *model;
  GtkSortListModel *sort;
  GtkSorter *sorter;

  model = new_model (3);
  assert_model (model, "1 2 21 3 31 32 321");

  sorter = GTK_SORTER (gtk_tree_list_row_sorter_new (NULL));
  sort = gtk_sort_list_model_new (model, sorter);
  assert_model (sort, "1 2 21 3 31 32 321");

  g_object_unref (sort);
}

static GtkSorter *
new_numeric_sorter (void)
{
  return GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_UINT, NULL, 0, NULL, (GCallback)get_number, NULL, NULL)));
}

static void
test_compare_total_order (void)
{
  GListModel *model;
  GtkSorter *sorter;
  guint i, j, n;

  model = new_model (3);
  assert_model (model, "1 2 21 3 31 32 321");

  sorter = GTK_SORTER (gtk_tree_list_row_sorter_new (new_numeric_sorter ()));

  n = g_list_model_get_n_items (model);
  for (i = 0; i < n; i++)
    {
      gpointer item1 = g_list_model_get_item (model, i);
      for (j = 0; j < n; j++)
        {
          gpointer item2 = g_list_model_get_item (model, j);

          g_assert_cmpint (gtk_sorter_compare (sorter, item1, item2), ==, gtk_ordering_from_cmpfunc ((int) i - j));
          g_object_unref (item2);
        }
      g_object_unref (item1);
    }

  g_object_unref (sorter);
  g_object_unref (model);
}

static void
test_compare_no_order (void)
{
  GListModel *model;
  GtkSorter *sorter;
  guint i, j, n;

  model = new_model (3);
  assert_model (model, "1 2 21 3 31 32 321");

  sorter = GTK_SORTER (gtk_tree_list_row_sorter_new (NULL));

  n = g_list_model_get_n_items (model);
  for (i = 0; i < n; i++)
    {
      gpointer item1 = g_list_model_get_item (model, i);
      for (j = 0; j < n; j++)
        {
          gpointer item2 = g_list_model_get_item (model, j);

          g_assert_cmpint (gtk_sorter_compare (sorter, item1, item2), ==, gtk_ordering_from_cmpfunc ((int) i - j));
          g_object_unref (item2);
        }
      g_object_unref (item1);
    }

  g_object_unref (sorter);
  g_object_unref (model);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Like a trashcan fire in a prison cell");

  g_test_add_func ("/sorter/simple", test_simple);
  g_test_add_func ("/sorter/compare-total-order", test_compare_total_order);
  g_test_add_func ("/sorter/compare-no-order", test_compare_no_order);

  return g_test_run ();
}
