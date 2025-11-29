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
static GQuark selection_quark;

static guint
get (GListModel *model,
     guint       position)
{
  GObject *object = g_list_model_get_item (model, position);
  guint ret;
  g_assert_nonnull (object);
  ret = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
  g_object_unref (object);
  return ret;
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
  guint i, s, e, n;

  if (!GTK_IS_SECTION_MODEL (model))
    return model_to_string (model);

  string = g_string_new (NULL);
  n = g_list_model_get_n_items (model);

  i = 0;
  while (i < n)
    {
      gtk_section_model_get_section (GTK_SECTION_MODEL (model), i, &s, &e);

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

static char *
selection_to_string (GListModel *model)
{
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      if (!gtk_selection_model_is_selected (GTK_SELECTION_MODEL (model), i))
        continue;

      if (string->len > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%u", get (model, i));
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
  GObject **objects;
  guint i;

  objects = g_new0 (GObject *, added);

  for (i = 0; i < added; i++)
    objects[i] = make_object (numbers[i]);

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);

  g_free (objects);
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

#define ignore_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_selection(model, expected) G_STMT_START{ \
  char *s = selection_to_string (G_LIST_MODEL (model)); \
  if (!g_str_equal (s, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, s, "==", expected); \
  g_free (s); \
}G_STMT_END

#define ignore_selection_changes(model) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
  g_string_set_size (changes, 0); \
}G_STMT_END

#define assert_selection_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), selection_quark); \
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
selection_changed (GListModel *model,
                   guint       position,
                   guint       n_items,
                   GString    *changes)
{
  if (changes->len)
    g_string_append (changes, ", ");

  g_string_append_printf (changes, "%u:%u", position, n_items);
}

static void
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkSelectionModel *
new_model (GListModel *store)
{
  GtkSelectionModel *result;
  GString *changes;

  result = GTK_SELECTION_MODEL (gtk_multi_selection_new (g_object_ref (store)));

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT (result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "sections-changed", G_CALLBACK (sections_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), selection_quark, changes, free_changes);
  g_signal_connect (result, "selection-changed", G_CALLBACK (selection_changed), changes);

  return result;
}

static GtkSelectionFilterModel *
new_filter_model (GtkSelectionModel *model)
{
  GtkSelectionFilterModel *result;
  GString *changes;

  result = gtk_selection_filter_model_new (model);

  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static void
test_create (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  guint start, end;

  store = new_store (1, 5, 2);
  selection = new_model (G_LIST_MODEL (store));

  assert_model (selection, "1 3 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_object_unref (store);
  assert_model (selection, "1 3 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (selection)) == G_TYPE_OBJECT);
  g_assert_true (gtk_multi_selection_get_model (GTK_MULTI_SELECTION (selection)) == G_LIST_MODEL (store));
  gtk_section_model_get_section (GTK_SECTION_MODEL (selection), 0, &start, &end);
  g_assert_cmpint (start, ==, 0);
  g_assert_cmpint (end, ==, G_MAXUINT);

  g_object_unref (selection);
}

static void
test_create_empty (void)
{
  GtkMultiSelection *selection;

  selection = gtk_multi_selection_new (NULL);
  g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (selection)), ==, 0);

  g_object_unref (selection);
}

static void
test_changes (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);
  selection = new_model (G_LIST_MODEL (store));
  assert_model (selection, "1 2 3 4 5");
  assert_changes (selection, "");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  g_list_store_remove (store, 3);
  assert_model (selection, "1 2 3 5");
  assert_changes (selection, "-3*");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  insert (store, 3, 99);
  assert_model (selection, "1 2 3 99 5");
  assert_changes (selection, "+3*");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  splice (store, 3, 2, (guint[]) { 97 }, 1);
  assert_model (selection, "1 2 3 97");
  assert_changes (selection, "3-2+1*");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 1, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 3");
  assert_selection_changes (selection, "1:2");

  insert (store, 2, 22);
  assert_model (selection, "1 2 22 3 97");
  assert_changes (selection, "+2*");
  assert_selection (selection, "2 3");
  assert_selection_changes (selection, "");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_selection (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;
  
  store = new_store (1, 5, 1);
  selection = new_model (G_LIST_MODEL (store));
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_item (selection, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "4");
  assert_selection_changes (selection, "3:1");

  ret = gtk_selection_model_unselect_item (selection, 3);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "3:1");

  ret = gtk_selection_model_select_item (selection, 1, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "1:1");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 4 5");
  assert_selection_changes (selection, "3:2");

  ret = gtk_selection_model_unselect_range (selection, 3, 2);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "3:2");

  ret = gtk_selection_model_select_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "1 2 3 4 5");
  assert_selection_changes (selection, "0:5");

  ret = gtk_selection_model_unselect_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "0:5");

  g_object_unref (store);
  g_object_unref (selection);
}

/* Verify that select_range with exclusive = TRUE
 * sends a selection-changed signal that covers
 * preexisting items that got unselected
 */
static void
test_select_range (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);
  selection = new_model (G_LIST_MODEL (store));
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 2, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "3 4");
  assert_selection_changes (selection, "2:2");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "3 4 5");
  assert_selection_changes (selection, "4:1");

  ret = gtk_selection_model_select_range (selection, 0, 1, TRUE);
  g_assert_true (ret);
  assert_selection (selection, "1");
  assert_selection_changes (selection, "0:5");

  g_object_unref (store);
  g_object_unref (selection);
}

/* Test that removing and readding items
 * doesn't clear the selected state.
 */
static void
test_readd (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);

  selection = new_model (G_LIST_MODEL (store));
  assert_model (selection, "1 2 3 4 5");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 2, 2, FALSE);
  g_assert_true (ret);
  assert_model (selection, "1 2 3 4 5");
  assert_selection (selection, "3 4");
  assert_selection_changes (selection, "2:2");

  g_list_model_items_changed (G_LIST_MODEL (store), 1, 3, 3);
  assert_changes (selection, "1-3+3");
  assert_selection (selection, "3 4");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_set_selection (void)
{
  GtkSelectionModel *selection;
  gboolean ret;
  GListStore *store;
  GtkBitset *selected, *mask;

  store = new_store (1, 10, 1);

  selection = new_model (G_LIST_MODEL (store));
  assert_model (selection, "1 2 3 4 5 6 7 8 9 10");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  selected = gtk_bitset_new_empty ();
  gtk_bitset_add_range (selected, 2, 3);
  gtk_bitset_add_range (selected, 6, 3);
  mask = gtk_bitset_new_empty ();
  gtk_bitset_add_range (mask, 0, 100); /* too big on purpose */
  ret = gtk_selection_model_set_selection (selection, selected, mask);
  g_assert_true (ret);
  gtk_bitset_unref (selected);
  gtk_bitset_unref (mask);
  assert_selection (selection, "3 4 5 7 8 9");
  assert_selection_changes (selection, "2:7");

  selected = gtk_bitset_new_empty ();
  mask = gtk_bitset_new_empty ();
  gtk_bitset_add (mask, 3);
  gtk_bitset_add (mask, 7);
  ret = gtk_selection_model_set_selection (selection, selected, mask);
  g_assert_true (ret);
  gtk_bitset_unref (selected);
  gtk_bitset_unref (mask);
  assert_selection (selection, "3 5 7 9");
  assert_selection_changes (selection, "3:5");

  g_object_unref (store);
  g_object_unref (selection);
}

static void
test_selection_filter (void)
{
  GtkSelectionModel *selection;
  GtkSelectionFilterModel *filter;
  GListStore *store;
  gboolean ret;

  store = new_store (1, 5, 1);
  selection = new_model (G_LIST_MODEL (store));
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  filter = new_filter_model (selection);
  assert_model (filter, "");
  assert_changes (filter, "");

  ret = gtk_selection_model_select_item (selection, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "4");
  assert_selection_changes (selection, "3:1");
  assert_model (filter, "4");
  assert_changes (filter, "+0*");

  ret = gtk_selection_model_unselect_item (selection, 3);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "3:1");
  assert_model (filter, "");
  assert_changes (filter, "-0*");

  ret = gtk_selection_model_select_item (selection, 1, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "1:1");
  assert_model (filter, "2");
  assert_changes (filter, "+0*");

  ret = gtk_selection_model_select_item (selection, 0, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "1 2");
  assert_selection_changes (selection, "0:1");
  assert_model (filter, "1 2");
  assert_changes (filter, "+0*");

  ret = gtk_selection_model_unselect_item (selection, 0);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "0:1");
  assert_model (filter, "2");
  assert_changes (filter, "-0*");

  ret = gtk_selection_model_select_range (selection, 3, 2, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 4 5");
  assert_selection_changes (selection, "3:2");
  assert_model (filter, "2 4 5");
  assert_changes (filter, "1+2*");

  ret = gtk_selection_model_unselect_range (selection, 3, 2);
  g_assert_true (ret);
  assert_selection (selection, "2");
  assert_selection_changes (selection, "3:2");
  assert_model (filter, "2");
  assert_changes (filter, "1-2*");

  ret = gtk_selection_model_select_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "1 2 3 4 5");
  assert_selection_changes (selection, "0:5");
  assert_model (filter, "1 2 3 4 5");
  assert_changes (filter, "0-1+5*");

  ret = gtk_selection_model_unselect_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "");
  assert_selection_changes (selection, "0:5");
  assert_model (filter, "");
  assert_changes (filter, "0-5*");

  ret = gtk_selection_model_select_range (selection, 1, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 3 4");
  assert_selection_changes (selection, "1:3");
  assert_model (filter, "2 3 4");
  assert_changes (filter, "0+3*");

  insert (store, 2, 22);
  assert_model (selection, "1 2 22 3 4 5");
  assert_changes (selection, "+2*");
  assert_selection (selection, "2 3 4");
  assert_selection_changes (selection, "");
  assert_model (filter, "2 3 4");
  assert_changes (filter, "");

  g_list_store_remove (store, 2);
  assert_model (selection, "1 2 3 4 5");
  assert_changes (selection, "-2*");
  assert_selection (selection, "2 3 4");
  assert_selection_changes (selection, "");
  assert_model (filter, "2 3 4");
  assert_changes (filter, "");

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (filter)) == G_TYPE_OBJECT);
  g_assert_true (gtk_selection_filter_model_get_model (filter) == selection);

  g_object_unref (store);
  g_object_unref (selection);
  g_object_unref (filter);
}

static void
test_set_model (void)
{
  GtkSelectionModel *selection;
  GListStore *store;
  GListModel *m1, *m2;
  gboolean ret;
  
  store = new_store (1, 5, 1);
  m1 = G_LIST_MODEL (store);
  m2 = G_LIST_MODEL (gtk_slice_list_model_new (g_object_ref (m1), 0, 3));
  selection = new_model (G_LIST_MODEL (store));
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_range (selection, 1, 3, FALSE);
  g_assert_true (ret);
  assert_selection (selection, "2 3 4");
  assert_selection_changes (selection, "1:3");

  /* we retain the selected item across model changes */
  gtk_multi_selection_set_model (GTK_MULTI_SELECTION (selection), m2);
  assert_changes (selection, "0-5+3*");
  assert_selection (selection, "2 3");
  assert_selection_changes (selection, "");

  gtk_multi_selection_set_model (GTK_MULTI_SELECTION (selection), NULL);
  assert_changes (selection, "0-3*");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  gtk_multi_selection_set_model (GTK_MULTI_SELECTION (selection), m2);
  assert_changes (selection, "0+3*");
  assert_selection (selection, "");
  assert_selection_changes (selection, "");

  ret = gtk_selection_model_select_all (selection);
  g_assert_true (ret);
  assert_selection (selection, "1 2 3");
  assert_selection_changes (selection, "0:3");

  /* we retain no selected item across model changes */
  gtk_multi_selection_set_model (GTK_MULTI_SELECTION (selection), m1);
  assert_changes (selection, "0-3+5*");
  assert_selection (selection, "1 2 3");
  assert_selection_changes (selection, "");

  g_object_unref (m2);
  g_object_unref (m1);
  g_object_unref (selection);
}

static void
test_empty (void)
{
  GtkMultiSelection *selection;
  GListStore *store;

  selection = gtk_multi_selection_new (NULL);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (selection)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (selection), 11));

  store = g_list_store_new (G_TYPE_OBJECT);
  gtk_multi_selection_set_model (selection, G_LIST_MODEL (store));
  g_object_unref (store);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (selection)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (selection), 11));

  g_object_unref (selection);
}

static void
test_empty_filter (void)
{
  GtkStringList *stringlist;
  GtkMultiSelection *selection;
  GtkSelectionFilterModel *selection_filter;

  stringlist = gtk_string_list_new (NULL);
  gtk_string_list_append (stringlist, "first item");

  selection = gtk_multi_selection_new (G_LIST_MODEL (stringlist));
  selection_filter = gtk_selection_filter_model_new (GTK_SELECTION_MODEL (selection));

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (selection_filter)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (selection_filter), 11));

  g_object_unref (selection_filter);
  g_object_unref (selection);
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
  GtkSelectionModel *selection;
  GtkSorter *sorter;

  store = new_store (1, 10, 1);
  sorted = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                    GTK_SORTER (gtk_custom_sorter_new (compare, NULL, NULL)));
  selection = new_model (G_LIST_MODEL (sorted));
  assert_model (selection, "1 2 3 4 5 6 7 8 9 10");
  assert_section_model (selection, "[1 2 3 4 5 6 7 8 9 10]");
  assert_changes (selection, "");

  sorter = GTK_SORTER (gtk_custom_sorter_new (by_n, GUINT_TO_POINTER (3), NULL));
  gtk_sort_list_model_set_section_sorter (sorted, sorter);
  g_object_unref (sorter);

  assert_section_model (selection, "[1 2] [3 4 5] [6 7 8] [9 10]");
  assert_changes (selection, "s0:10");

  gtk_section_model_sections_changed (GTK_SECTION_MODEL (sorted), 0, 3);
  assert_changes (selection, "s0:3");

  gtk_section_model_sections_changed (GTK_SECTION_MODEL (sorted), 5, 3);
  assert_changes (selection, "s5:3");

  g_object_unref (selection);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");
  selection_quark = g_quark_from_static_string ("Mana mana, badibidibi");

  g_test_add_func ("/multiselection/create", test_create);
  g_test_add_func ("/multiselection/create-empty", test_create_empty);
  g_test_add_func ("/multiselection/changes", test_changes);
  g_test_add_func ("/multiselection/selection", test_selection);
  g_test_add_func ("/multiselection/select-range", test_select_range);
  g_test_add_func ("/multiselection/readd", test_readd);
  g_test_add_func ("/multiselection/set_selection", test_set_selection);
  g_test_add_func ("/multiselection/selection-filter", test_selection_filter);
  g_test_add_func ("/multiselection/set-model", test_set_model);
  g_test_add_func ("/multiselection/empty", test_empty);
  g_test_add_func ("/multiselection/selection-filter/empty", test_empty_filter);
  g_test_add_func ("/multiselection/sections", test_sections);

  return g_test_run ();
}
