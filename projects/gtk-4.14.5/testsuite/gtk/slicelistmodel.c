/* 
 * Copyright (C) 2019, Red Hat, Inc.
 * Authors: Matthias Clasen <mclasen@redhat.com>
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

static GObject *
make_object (guint number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));

  return object;
}

static void
splice (GListStore *store,
        guint       pos,
        guint       removed,
        guint      *numbers,
        guint       added)
{
  GObject **objects = g_newa (GObject *, added);
  guint i;

  for (i = 0; i < added; i++)
    objects[i] = make_object (numbers[i]);

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}

static void
add (GListStore *store,
     guint       number)
{
  GObject *object = make_object (number);
  g_list_store_append (store, object);
  g_object_unref (object);
}

static void
insert (GListStore *store,
        guint position,
        guint number)
{
  GObject *object = make_object (number);
  g_list_store_insert (store, position, object);
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

static GtkSliceListModel *
new_model (GListModel *store, guint offset, guint size)
{
  GtkSliceListModel *result;
  GString *changes;

  if (store)
    g_object_ref (store);
  result = gtk_slice_list_model_new (store, offset, size);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT (result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "sections-changed", G_CALLBACK (sections_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static void
test_create_empty (void)
{
  GtkSliceListModel *slice;

  slice = new_model (NULL, 0, 0);
  assert_model (slice, "");
  assert_changes (slice, "");

  g_object_unref (slice);
}

static void
test_create (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  store = new_store (1, 5, 2);
  slice = new_model (G_LIST_MODEL (store), 0, 10);
  assert_model (slice, "1 3 5");
  assert_changes (slice, "");

  g_object_unref (store);
  assert_model (slice, "1 3 5");
  assert_changes (slice, "");

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (slice)) == G_TYPE_OBJECT);

  g_object_unref (slice);
}

static void
test_set_model (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  slice = new_model (NULL, 0, 2);
  assert_model (slice, "");
  assert_changes (slice, "");

  store = new_store (1, 7, 2);
  gtk_slice_list_model_set_model (slice, G_LIST_MODEL (store));
  assert_model (slice, "1 3");
  assert_changes (slice, "0+2*");

  gtk_slice_list_model_set_model (slice, NULL);
  assert_model (slice, "");
  assert_changes (slice, "0-2*");

  g_object_unref (store);
  g_object_unref (slice);
}

static void
test_set_slice (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  store = new_store (1, 7, 2);
  slice = new_model (G_LIST_MODEL (store), 0, 3);
  assert_model (slice, "1 3 5");
  assert_changes (slice, "");

  gtk_slice_list_model_set_offset (slice, 1);
  assert_model (slice, "3 5 7");
  assert_changes (slice, "0-3+3");

  gtk_slice_list_model_set_size (slice, 2);
  assert_model (slice, "3 5");
  assert_changes (slice, "-2*");

  gtk_slice_list_model_set_size (slice, 10);
  assert_model (slice, "3 5 7");
  assert_changes (slice, "+2*");

  g_assert_cmpuint (gtk_slice_list_model_get_offset (slice), ==, 1);

  g_object_unref (store);
  g_object_unref (slice);
}

static void
test_changes (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  store = new_store (1, 20, 1);
  slice = new_model (G_LIST_MODEL (store), 10, 5);
  assert_model (slice, "11 12 13 14 15");
  assert_changes (slice, "");

  g_list_store_remove (store, 19);
  assert_changes (slice, "");

  splice (store, 1, 1, (guint[]) { 111 }, 1);
  assert_changes (slice, "");

  splice (store, 18, 1, (guint[]) { 19, 20 }, 2);
  assert_changes (slice, "");

  g_list_store_remove (store, 19);
  assert_changes (slice, "");

  g_list_store_remove (store, 1);
  assert_model (slice, "12 13 14 15 16");
  assert_changes (slice, "0-5+5");

  insert (store, 12, 99);
  assert_model (slice, "12 13 99 14 15");
  assert_changes (slice, "2-3+3");

  splice (store, 13, 6, (guint[]) { 97 }, 1);
  assert_model (slice, "12 13 99 97");
  assert_changes (slice, "3-2+1*");

  splice (store, 13, 1, (guint[]) { 36, 37, 38 }, 3);
  assert_model (slice, "12 13 99 36 37");
  assert_changes (slice, "3-1+2*");

  g_list_store_remove_all (store);
  assert_model (slice, "");
  assert_changes (slice, "0-5*");

  g_object_unref (store);
  g_object_unref (slice);
}

static void
test_bug_added_equals_removed (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  store = new_store (1, 10, 1);
  slice = new_model (G_LIST_MODEL (store), 0, 10);
  assert_model (slice, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (slice, "");

  splice (store, 9, 1, (guint[]) { 11 }, 1);
  assert_model (slice, "1 2 3 4 5 6 7 8 9 11");
  assert_changes (slice, "9-1+1");

  g_object_unref (store);
  g_object_unref (slice);
}

static void
test_bug_skip_amount (void)
{
  GtkSliceListModel *slice;
  GListStore *store;
  
  store = new_store (1, 5, 1);
  slice = new_model (G_LIST_MODEL (store), 2, 2);
  assert_model (slice, "3 4");
  assert_changes (slice, "");

  splice (store, 0, 5, (guint[]) { 11, 12, 13, 14, 15 }, 5);
  assert_model (slice, "13 14");
  assert_changes (slice, "0-2+2");

  g_object_unref (store);
  g_object_unref (slice);
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
  GListStore *store;
  GtkSortListModel *sorted;
  GtkSliceListModel *slice;
  GtkSorter *sorter;

  store = new_store (1, 10, 1);
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                    GTK_SORTER (gtk_custom_sorter_new (compare, NULL, NULL)));
  slice = new_model (G_LIST_MODEL (sorted), 0, 10);
  assert_model (slice, "1 2 3 4 5 6 7 8 9 10");
  assert_section_model (slice, "[1 2 3 4 5 6 7 8 9 10]");
  assert_changes (slice, "");

  sorter = GTK_SORTER (gtk_custom_sorter_new (by_n, GUINT_TO_POINTER (3), NULL));
  gtk_sort_list_model_set_section_sorter (sorted, sorter);
  g_object_unref (sorter);

  assert_section_model (slice, "[1 2] [3 4 5] [6 7 8] [9 10]");
  assert_changes (slice, "s0:10");

  gtk_slice_list_model_set_size (slice, 5);

  assert_section_model (slice, "[1 2] [3 4 5]");
  assert_changes (slice, "5-5*");

  gtk_slice_list_model_set_offset (slice, 1);
  assert_section_model (slice, "[2] [3 4 5] [6]");
  assert_changes (slice, "0-5+5");

  gtk_section_model_sections_changed (GTK_SECTION_MODEL (sorted), 0, 3);
  assert_changes (slice, "s0:2");

  gtk_section_model_sections_changed (GTK_SECTION_MODEL (sorted), 5, 3);
  assert_changes (slice, "s4:1");

  g_object_unref (slice);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/slicelistmodel/create_empty", test_create_empty);
  g_test_add_func ("/slicelistmodel/create", test_create);
  g_test_add_func ("/slicelistmodel/set-model", test_set_model);
  g_test_add_func ("/slicelistmodel/set-slice", test_set_slice);
  g_test_add_func ("/slicelistmodel/changes", test_changes);
  g_test_add_func ("/slicelistmodel/bug/added_equals_removed", test_bug_added_equals_removed);
  g_test_add_func ("/slicelistmodel/bug/skip_amount", test_bug_skip_amount);
  g_test_add_func ("/slicelistmodel/sections", test_sections);

  return g_test_run ();
}
