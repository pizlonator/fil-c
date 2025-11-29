/* Maplistmodel tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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
static GQuark changes_quark;

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  guint number;
  g_assert_nonnull (object);
  number = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
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
      if (i > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get (model, i));
    }

  return g_string_free (string, FALSE);
}

static char *
section_model_to_string (GListModel *model)
{
  GString *string;
  guint i, s, e;

  if (!GTK_IS_SECTION_MODEL (model))
    return model_to_string (model);

  string = g_string_new (NULL);

  i = 0;
  while (i < g_list_model_get_n_items (model))
    {
      gtk_section_model_get_section (GTK_SECTION_MODEL (model), i, &s, &e);
      g_assert (s == i);

      if (i > 0)
        g_string_append (string, " ");

      g_string_append (string, "[");

      for (; i < e; i++)
        {
          if (i > s)
            g_string_append (string, " ");

          g_string_append_printf (string, "%u", get (model, i));
        }

      g_string_append (string, "]");
      i = e;
    }

  return g_string_free (string, FALSE);
}

static GListStore *
new_store (guint start,
           guint end,
           guint step);

static void
splice (GListStore *store,
        guint       pos,
        guint       removed,
        guint      *numbers,
        guint       added)
{
  GObject **objects;
  guint i;

  objects = g_newa (GObject*, added);

  for (i = 0; i < added; i++)
    {
      /* 0 cannot be differentiated from NULL, so don't use it */
      g_assert_true (numbers[i] != 0);
      objects[i] = g_object_new (G_TYPE_OBJECT, NULL);
      g_object_set_qdata (objects[i], number_quark, GUINT_TO_POINTER (numbers[i]));
    }

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}

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

#define assert_section_model(model, expected) G_STMT_START{ \
  char *s = section_model_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
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

static void
items_changed (GListModel *model,
               guint       position,
               guint       removed,
               guint       added,
               GString    *changes)
{
  g_assert_true (removed != 0 || added != 0);

  if (changes->len)
    g_string_append (changes, ", ");

  if (removed == 1 && added == 0)
    {
      g_string_append_printf (changes, "-%u", position);
    }
  else if (removed == 0 && added == 1)
    {
      g_string_append_printf (changes, "+%u", position);
    }
  else
    {
      g_string_append_printf (changes, "%u", position);
      if (removed > 0)
        g_string_append_printf (changes, "-%u", removed);
      if (added > 0)
        g_string_append_printf (changes, "+%u", added);
    }
}


static void
sections_changed (GListModel *model,
                  guint       position,
                  guint       n_items,
                  GString    *changes)
{
  g_assert_true (n_items != 0);

  if (changes->len)
    g_string_append (changes, ", ");

  g_string_append_printf (changes, "s%u:%u", position, n_items);
}

static void
notify_n_items (GObject    *object,
                GParamSpec *pspec,
                GString    *changes)
{
  g_string_append_c (changes, '*');
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static gpointer
map_multiply (gpointer item,
              gpointer factor)
{
  GObject *object;
  guint num;

  object = g_object_new (G_TYPE_OBJECT, NULL);
  num = GPOINTER_TO_UINT (g_object_get_qdata (item, number_quark));
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (GPOINTER_TO_UINT (factor) * num));
  g_object_unref (item);

  return object;
}

static GtkMapListModel *
new_model (GListModel *store)
{
  GtkMapListModel *result;
  GString *changes;

  if (store)
    g_object_ref (store);
  result = gtk_map_list_model_new (store, map_multiply, GUINT_TO_POINTER (2), NULL);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "sections-changed", G_CALLBACK (sections_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static void
test_create_empty (void)
{
  GtkMapListModel *map;

  map = new_model (NULL);
  assert_model (map, "");
  assert_changes (map, "");

  g_object_unref (map);
}

static void
test_create (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  map = new_model (G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  g_object_unref (store);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  g_object_unref (map);
}

static void
test_set_model (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  map = new_model (NULL);
  assert_model (map, "");
  assert_changes (map, "");

  store = new_store (1, 5, 1);
  gtk_map_list_model_set_model (map, G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "0+5*");

  gtk_map_list_model_set_model (map, NULL);
  assert_model (map, "");
  assert_changes (map, "0-5*");

  g_object_unref (store);
  g_object_unref (map);
}

static void
test_set_map_func (void)
{
  GtkMapListModel *map;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  map = new_model (G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  gtk_map_list_model_set_map_func (map, map_multiply, GUINT_TO_POINTER (3), NULL);
  assert_model (map, "3 6 9 12 15");
  assert_changes (map, "0-5+5");

  gtk_map_list_model_set_map_func (map, NULL, NULL, NULL);
  assert_model (map, "1 2 3 4 5");
  assert_changes (map, "0-5+5");

  gtk_map_list_model_set_map_func (map, map_multiply, GUINT_TO_POINTER (2), NULL);
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "0-5+5");

  g_object_unref (store);
  g_object_unref (map);
}

static void
test_add_items (void)
{
  GtkMapListModel *map;
  GListStore *store;

  store = new_store (1, 5, 1);
  map = new_model (G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  add (store, 6);
  assert_model (map, "2 4 6 8 10 12");
  assert_changes (map, "+5*");
}

static void
test_remove_items (void)
{
  GtkMapListModel *map;
  GListStore *store;

  store = new_store (1, 5, 1);
  map = new_model (G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  g_list_store_remove (store, 2);
  assert_model (map, "2 4 8 10");
  assert_changes (map, "-2*");
}

static void
test_splice (void)
{
  GtkMapListModel *map;
  GListStore *store;

  store = new_store (1, 5, 1);
  map = new_model (G_LIST_MODEL (store));
  assert_model (map, "2 4 6 8 10");
  assert_changes (map, "");

  splice (store, 2, 2, (guint[]){ 4, 3 }, 2);
  assert_model (map, "2 4 8 6 10");
  assert_changes (map, "2-2+2");
}

static int
by_n (gconstpointer p1,
      gconstpointer p2,
      gpointer      data)
{
  guint n1 = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (p1), number_quark));
  guint n2 = GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (p2), number_quark));
  unsigned int n = GPOINTER_TO_UINT (data);

  n1 = n1 / n;
  n2 = n2 / n;

  if (n1 < n2)
    return -1;
  else if (n1 > n2)
    return 1;
  else
    return 0;
}

static int
compare (gconstpointer first,
         gconstpointer second,
         gpointer      unused)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (first), number_quark))
      -  GPOINTER_TO_UINT (g_object_get_qdata (G_OBJECT (second), number_quark));
}

static void
test_sections (void)
{
  GtkMapListModel *map;
  GListStore *store;
  GtkSortListModel *sorted;
  GtkSorter *sorter;

  store = new_store (1, 10, 1);
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                    GTK_SORTER (gtk_custom_sorter_new (compare, NULL, NULL)));
  map = new_model (G_LIST_MODEL (sorted));
  assert_model (map, "2 4 6 8 10 12 14 16 18 20");
  assert_section_model (map, "[2 4 6 8 10 12 14 16 18 20]");
  assert_changes (map, "");

  sorter = GTK_SORTER (gtk_custom_sorter_new (by_n, GUINT_TO_POINTER (3), NULL));
  gtk_sort_list_model_set_section_sorter (sorted, sorter);
  g_object_unref (sorter);

  assert_section_model (map, "[2 4] [6 8 10] [12 14 16] [18 20]");
  assert_changes (map, "s0:10");

  g_object_unref (map);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/maplistmodel/create_empty", test_create_empty);
  g_test_add_func ("/maplistmodel/create", test_create);
  g_test_add_func ("/maplistmodel/set-model", test_set_model);
  g_test_add_func ("/maplistmodel/set-map-func", test_set_map_func);
  g_test_add_func ("/maplistmodel/add_items", test_add_items);
  g_test_add_func ("/maplistmodel/remove_items", test_remove_items);
  g_test_add_func ("/maplistmodel/splice", test_splice);
  g_test_add_func ("/maplistmodel/sections", test_sections);

  return g_test_run ();
}
