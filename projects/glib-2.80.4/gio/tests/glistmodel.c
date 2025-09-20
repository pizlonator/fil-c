/*
 * Copyright 2015 Lars Uebernickel
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#include <gio/gio.h>

#include <string.h>

/* Wrapper around g_list_model_get_item() and g_list_model_get_object() which
 * checks they return the same thing. */
static gpointer
list_model_get (GListModel *model,
                guint       position)
{
  GObject *item = g_list_model_get_item (model, position);
  GObject *object = g_list_model_get_object (model, position);

  g_assert_true (item == object);

  g_clear_object (&object);
  return g_steal_pointer (&item);
}

#define assert_cmpitems(store, cmp, n_items) G_STMT_START{ \
  guint tmp; \
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), cmp, n_items); \
  g_object_get (store, "n-items", &tmp, NULL); \
  g_assert_cmpuint (tmp, cmp, n_items); \
}G_STMT_END

/* Test that constructing/getting/setting properties on a #GListStore works. */
static void
test_store_properties (void)
{
  GListStore *store = NULL;
  GType item_type;

  store = g_list_store_new (G_TYPE_MENU_ITEM);
  g_object_get (store, "item-type", &item_type, NULL);
  g_assert_cmpint ((uintptr_t) item_type, ==, (uintptr_t) G_TYPE_MENU_ITEM);

  g_clear_object (&store);
}

/* Test that #GListStore rejects non-GObject item types. */
static void
test_store_non_gobjects (void)
{
  if (g_test_subprocess ())
    {
      /* We have to use g_object_new() since g_list_store_new() checks the item
       * type. We want to check the property setter code works properly. */
      g_object_new (G_TYPE_LIST_STORE, "item-type", G_TYPE_LONG, NULL);
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*value * of type 'GType' is invalid or "
                             "out of range for property 'item-type'*");
}

static void
test_store_boundaries (void)
{
  GListStore *store;
  GMenuItem *item;

  store = g_list_store_new (G_TYPE_MENU_ITEM);

  item = g_menu_item_new (NULL, NULL);

  /* remove an item from an empty list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_remove (store, 0);
  g_test_assert_expected_messages ();

  /* don't allow inserting an item past the end ... */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_insert (store, 1, item);
  assert_cmpitems (store, ==, 0);
  g_test_assert_expected_messages ();

  /* ... except exactly at the end */
  g_list_store_insert (store, 0, item);
  assert_cmpitems (store, ==, 1);

  /* remove a non-existing item at exactly the end of the list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_remove (store, 1);
  g_test_assert_expected_messages ();

  g_list_store_remove (store, 0);
  assert_cmpitems (store, ==, 0);

  /* splice beyond the end of the list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 1, 0, NULL, 0);
  g_test_assert_expected_messages ();

  /* remove items from an empty list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 0, 1, NULL, 0);
  g_test_assert_expected_messages ();

  g_list_store_append (store, item);
  g_list_store_splice (store, 0, 1, (gpointer *) &item, 1);
  assert_cmpitems (store, ==, 1);

  /* remove more items than exist */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 0, 5, NULL, 0);
  g_test_assert_expected_messages ();
  assert_cmpitems (store, ==, 1);

  g_object_unref (store);
  g_assert_finalize_object (item);
}

static void
test_store_refcounts (void)
{
  GListStore *store;
  GMenuItem *items[10];
  GMenuItem *tmp;
  guint i;
  guint n_items;

  store = g_list_store_new (G_TYPE_MENU_ITEM);

  assert_cmpitems (store, ==, 0);
  g_assert_null (list_model_get (G_LIST_MODEL (store), 0));

  n_items = G_N_ELEMENTS (items);
  for (i = 0; i < n_items; i++)
    {
      items[i] = g_menu_item_new (NULL, NULL);
      g_object_add_weak_pointer (G_OBJECT (items[i]), (gpointer *) &items[i]);
      g_list_store_append (store, items[i]);

      g_object_unref (items[i]);
      g_assert_nonnull (items[i]);
    }

  assert_cmpitems (store, ==, n_items);
  g_assert_null (list_model_get (G_LIST_MODEL (store), n_items));

  tmp = list_model_get (G_LIST_MODEL (store), 3);
  g_assert_true (tmp == items[3]);
  g_object_unref (tmp);

  g_list_store_remove (store, 4);
  g_assert_null (items[4]);
  n_items--;
  assert_cmpitems (store, ==, n_items);
  g_assert_null (list_model_get (G_LIST_MODEL (store), n_items));

  g_object_unref (store);
  for (i = 0; i < G_N_ELEMENTS (items); i++)
    g_assert_null (items[i]);
}

static gchar *
make_random_string (void)
{
  gchar *str = g_malloc (10);
  gint i;

  for (i = 0; i < 9; i++)
    str[i] = g_test_rand_int_range ('a', 'z');
  str[i] = '\0';

  return str;
}

static gint
compare_items (gconstpointer a_p,
               gconstpointer b_p,
               gpointer      user_data)
{
  GObject *a_o = (GObject *) a_p;
  GObject *b_o = (GObject *) b_p;

  gchar *a = g_object_get_data (a_o, "key");
  gchar *b = g_object_get_data (b_o, "key");

  g_assert (user_data == GUINT_TO_POINTER(0x1234u));

  return strcmp (a, b);
}

static void
insert_string (GListStore  *store,
               const gchar *str)
{
  GObject *obj;

  obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_data_full (obj, "key", g_strdup (str), g_free);

  g_list_store_insert_sorted (store, obj, compare_items, GUINT_TO_POINTER(0x1234u));

  g_object_unref (obj);
}

static void
test_store_sorted (void)
{
  GListStore *store;
  guint i;

  store = g_list_store_new (G_TYPE_OBJECT);

  for (i = 0; i < 1000; i++)
    {
      gchar *str = make_random_string ();
      insert_string (store, str);
      insert_string (store, str); /* multiple copies of the same are OK */
      g_free (str);
    }

  assert_cmpitems (store, ==, 2000);

  for (i = 0; i < 1000; i++)
    {
      GObject *a, *b;

      /* should see our two copies */
      a = list_model_get (G_LIST_MODEL (store), i * 2);
      b = list_model_get (G_LIST_MODEL (store), i * 2 + 1);

      g_assert (compare_items (a, b, GUINT_TO_POINTER(0x1234)) == 0);
      g_assert (a != b);

      if (i)
        {
          GObject *c;

          c = list_model_get (G_LIST_MODEL (store), i * 2 - 1);
          g_assert (c != a);
          g_assert (c != b);

          g_assert (compare_items (b, c, GUINT_TO_POINTER(0x1234)) > 0);
          g_assert (compare_items (a, c, GUINT_TO_POINTER(0x1234)) > 0);

          g_object_unref (c);
        }

      g_object_unref (a);
      g_object_unref (b);
    }

  g_object_unref (store);
}

/* Test that using splice() to replace the middle element in a list store works. */
static void
test_store_splice_replace_middle (void)
{
  GListStore *store;
  GListModel *model;
  GAction *item;
  GPtrArray *array;

  g_test_bug ("https://bugzilla.gnome.org/show_bug.cgi?id=795307");

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  array = g_ptr_array_new_full (0, g_object_unref);
  g_ptr_array_add (array, g_simple_action_new ("1", NULL));
  g_ptr_array_add (array, g_simple_action_new ("2", NULL));
  g_ptr_array_add (array, g_simple_action_new ("3", NULL));
  g_ptr_array_add (array, g_simple_action_new ("4", NULL));
  g_ptr_array_add (array, g_simple_action_new ("5", NULL));

  /* Add three items through splice */
  g_list_store_splice (store, 0, 0, array->pdata, 3);
  assert_cmpitems (store, ==, 3);

  item = list_model_get (model, 0);
  g_assert_cmpstr (g_action_get_name (item), ==, "1");
  g_object_unref (item);
  item = list_model_get (model, 1);
  g_assert_cmpstr (g_action_get_name (item), ==, "2");
  g_object_unref (item);
  item = list_model_get (model, 2);
  g_assert_cmpstr (g_action_get_name (item), ==, "3");
  g_object_unref (item);

  /* Replace the middle one with two new items */
  g_list_store_splice (store, 1, 1, array->pdata + 3, 2);
  assert_cmpitems (store, ==, 4);

  item = list_model_get (model, 0);
  g_assert_cmpstr (g_action_get_name (item), ==, "1");
  g_object_unref (item);
  item = list_model_get (model, 1);
  g_assert_cmpstr (g_action_get_name (item), ==, "4");
  g_object_unref (item);
  item = list_model_get (model, 2);
  g_assert_cmpstr (g_action_get_name (item), ==, "5");
  g_object_unref (item);
  item = list_model_get (model, 3);
  g_assert_cmpstr (g_action_get_name (item), ==, "3");
  g_object_unref (item);

  g_ptr_array_unref (array);
  g_object_unref (store);
}

/* Test that using splice() to replace the whole list store works. */
static void
test_store_splice_replace_all (void)
{
  GListStore *store;
  GListModel *model;
  GPtrArray *array;
  GAction *item;

  g_test_bug ("https://bugzilla.gnome.org/show_bug.cgi?id=795307");

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  array = g_ptr_array_new_full (0, g_object_unref);
  g_ptr_array_add (array, g_simple_action_new ("1", NULL));
  g_ptr_array_add (array, g_simple_action_new ("2", NULL));
  g_ptr_array_add (array, g_simple_action_new ("3", NULL));
  g_ptr_array_add (array, g_simple_action_new ("4", NULL));

  /* Add the first two */
  g_list_store_splice (store, 0, 0, array->pdata, 2);

  assert_cmpitems (store, ==, 2);
  item = list_model_get (model, 0);
  g_assert_cmpstr (g_action_get_name (item), ==, "1");
  g_object_unref (item);
  item = list_model_get (model, 1);
  g_assert_cmpstr (g_action_get_name (item), ==, "2");
  g_object_unref (item);

  /* Replace all with the last two */
  g_list_store_splice (store, 0, 2, array->pdata + 2, 2);

  assert_cmpitems (store, ==, 2);
  item = list_model_get (model, 0);
  g_assert_cmpstr (g_action_get_name (item), ==, "3");
  g_object_unref (item);
  item = list_model_get (model, 1);
  g_assert_cmpstr (g_action_get_name (item), ==, "4");
  g_object_unref (item);

  g_ptr_array_unref (array);
  g_object_unref (store);
}

/* Test that using splice() without removing or adding anything works */
static void
test_store_splice_noop (void)
{
  GListStore *store;
  GListModel *model;
  GAction *item;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  /* splice noop with an empty list */
  g_list_store_splice (store, 0, 0, NULL, 0);
  assert_cmpitems (store, ==, 0);

  /* splice noop with a non-empty list */
  item = G_ACTION (g_simple_action_new ("1", NULL));
  g_list_store_append (store, item);
  g_object_unref (item);

  g_list_store_splice (store, 0, 0, NULL, 0);
  assert_cmpitems (store, ==, 1);

  g_list_store_splice (store, 1, 0, NULL, 0);
  assert_cmpitems (store, ==, 1);

  item = list_model_get (model, 0);
  g_assert_cmpstr (g_action_get_name (item), ==, "1");
  g_object_unref (item);

  g_object_unref (store);
}

static gboolean
model_array_equal (GListModel *model, GPtrArray *array)
{
  guint i;

  if (g_list_model_get_n_items (model) != array->len)
    return FALSE;

  for (i = 0; i < array->len; i++)
    {
      GObject *ptr;
      gboolean ptrs_equal;

      ptr = list_model_get (model, i);
      ptrs_equal = (g_ptr_array_index (array, i) == ptr);
      g_object_unref (ptr);
      if (!ptrs_equal)
        return FALSE;
    }

  return TRUE;
}

/* Test that using splice() to remove multiple items at different
 * positions works */
static void
test_store_splice_remove_multiple (void)
{
  GListStore *store;
  GListModel *model;
  GPtrArray *array;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  array = g_ptr_array_new_full (0, g_object_unref);
  g_ptr_array_add (array, g_simple_action_new ("1", NULL));
  g_ptr_array_add (array, g_simple_action_new ("2", NULL));
  g_ptr_array_add (array, g_simple_action_new ("3", NULL));
  g_ptr_array_add (array, g_simple_action_new ("4", NULL));
  g_ptr_array_add (array, g_simple_action_new ("5", NULL));
  g_ptr_array_add (array, g_simple_action_new ("6", NULL));
  g_ptr_array_add (array, g_simple_action_new ("7", NULL));
  g_ptr_array_add (array, g_simple_action_new ("8", NULL));
  g_ptr_array_add (array, g_simple_action_new ("9", NULL));
  g_ptr_array_add (array, g_simple_action_new ("10", NULL));

  /* Add all */
  g_list_store_splice (store, 0, 0, array->pdata, array->len);
  g_assert_true (model_array_equal (model, array));

  /* Remove the first two */
  g_list_store_splice (store, 0, 2, NULL, 0);
  g_assert_false (model_array_equal (model, array));
  g_ptr_array_remove_range (array, 0, 2);
  g_assert_true (model_array_equal (model, array));
  assert_cmpitems (store, ==, 8);

  /* Remove two in the middle */
  g_list_store_splice (store, 2, 2, NULL, 0);
  g_assert_false (model_array_equal (model, array));
  g_ptr_array_remove_range (array, 2, 2);
  g_assert_true (model_array_equal (model, array));
  assert_cmpitems (store, ==, 6);

  /* Remove two at the end */
  g_list_store_splice (store, 4, 2, NULL, 0);
  g_assert_false (model_array_equal (model, array));
  g_ptr_array_remove_range (array, 4, 2);
  g_assert_true (model_array_equal (model, array));
  assert_cmpitems (store, ==, 4);

  g_ptr_array_unref (array);
  g_object_unref (store);
}

/* Test that using splice() to add multiple items at different
 * positions works */
static void
test_store_splice_add_multiple (void)
{
  GListStore *store;
  GListModel *model;
  GPtrArray *array;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  array = g_ptr_array_new_full (0, g_object_unref);
  g_ptr_array_add (array, g_simple_action_new ("1", NULL));
  g_ptr_array_add (array, g_simple_action_new ("2", NULL));
  g_ptr_array_add (array, g_simple_action_new ("3", NULL));
  g_ptr_array_add (array, g_simple_action_new ("4", NULL));
  g_ptr_array_add (array, g_simple_action_new ("5", NULL));
  g_ptr_array_add (array, g_simple_action_new ("6", NULL));

  /* Add two at the beginning */
  g_list_store_splice (store, 0, 0, array->pdata, 2);

  /* Add two at the end */
  g_list_store_splice (store, 2, 0, array->pdata + 4, 2);

  /* Add two in the middle */
  g_list_store_splice (store, 2, 0, array->pdata + 2, 2);

  g_assert_true (model_array_equal (model, array));

  g_ptr_array_unref (array);
  g_object_unref (store);
}

/* Test that get_item_type() returns the right type */
static void
test_store_item_type (void)
{
  GListStore *store;
  GType gtype;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  gtype = g_list_model_get_item_type (G_LIST_MODEL (store));
  g_assert (gtype == G_TYPE_SIMPLE_ACTION);

  g_object_unref (store);
}

/* Test that remove_all() removes all items */
static void
test_store_remove_all (void)
{
  GListStore *store;
  GSimpleAction *item;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);

  /* Test with an empty list */
  g_list_store_remove_all (store);
  assert_cmpitems (store, ==, 0);

  /* Test with a non-empty list */
  item = g_simple_action_new ("42", NULL);
  g_list_store_append (store, item);
  g_list_store_append (store, item);
  g_object_unref (item);
  assert_cmpitems (store, ==, 2);
  g_list_store_remove_all (store);
  assert_cmpitems (store, ==, 0);

  g_object_unref (store);
}

/* Test that splice() logs an error when passed the wrong item type */
static void
test_store_splice_wrong_type (void)
{
  GListStore *store;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);

  g_test_expect_message (G_LOG_DOMAIN,
                         G_LOG_LEVEL_CRITICAL,
                         "*GListStore instead of a GSimpleAction*");
  g_list_store_splice (store, 0, 0, (gpointer)&store, 1);

  g_object_unref (store);
}

static gint
cmp_action_by_name (GAction *a, GAction *b, gpointer user_data)
{
  return g_strcmp0 (g_action_get_name (a), g_action_get_name (b));
}

/* Test if sort() works */
static void
test_store_sort (void)
{
  GListStore *store;
  GListModel *model;
  GPtrArray *array;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  array = g_ptr_array_new_full (0, g_object_unref);
  g_ptr_array_add (array, g_simple_action_new ("2", NULL));
  g_ptr_array_add (array, g_simple_action_new ("3", NULL));
  g_ptr_array_add (array, g_simple_action_new ("9", NULL));
  g_ptr_array_add (array, g_simple_action_new ("4", NULL));
  g_ptr_array_add (array, g_simple_action_new ("5", NULL));
  g_ptr_array_add (array, g_simple_action_new ("8", NULL));
  g_ptr_array_add (array, g_simple_action_new ("6", NULL));
  g_ptr_array_add (array, g_simple_action_new ("7", NULL));
  g_ptr_array_add (array, g_simple_action_new ("1", NULL));

  /* Sort an empty list */
  g_list_store_sort (store, (GCompareDataFunc) cmp_action_by_name, NULL);

  /* Add all */
  g_list_store_splice (store, 0, 0, array->pdata, array->len);
  g_assert_true (model_array_equal (model, array));

  /* Sort both and check if the result is the same */
  g_ptr_array_sort_values (array, (GCompareFunc)cmp_action_by_name);
  g_assert_false (model_array_equal (model, array));
  g_list_store_sort (store, (GCompareDataFunc) cmp_action_by_name, NULL);
  g_assert_true (model_array_equal (model, array));

  g_ptr_array_unref (array);
  g_object_unref (store);
}

/* Test the cases where the item store tries to speed up item access by caching
 * the last iter/position */
static void
test_store_get_item_cache (void)
{
  GListStore *store;
  GListModel *model;
  GSimpleAction *item1, *item2, *temp;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  /* Add two */
  item1 = g_simple_action_new ("1", NULL);
  g_list_store_append (store, item1);
  item2 = g_simple_action_new ("2", NULL);
  g_list_store_append (store, item2);

  /* Clear the cache */
  g_assert_null (list_model_get (model, 42));

  /* Access the same position twice */
  temp = list_model_get (model, 1);
  g_assert (temp == item2);
  g_object_unref (temp);
  temp = list_model_get (model, 1);
  g_assert (temp == item2);
  g_object_unref (temp);

  g_assert_null (list_model_get (model, 42));

  /* Access forwards */
  temp = list_model_get (model, 0);
  g_assert (temp == item1);
  g_object_unref (temp);
  temp = list_model_get (model, 1);
  g_assert (temp == item2);
  g_object_unref (temp);

  g_assert_null (list_model_get (model, 42));

  /* Access backwards */
  temp = list_model_get (model, 1);
  g_assert (temp == item2);
  g_object_unref (temp);
  temp = list_model_get (model, 0);
  g_assert (temp == item1);
  g_object_unref (temp);

  g_object_unref (item1);
  g_object_unref (item2);
  g_object_unref (store);
}

struct ItemsChangedData
{
  guint position;
  guint removed;
  guint added;
  gboolean called;
  gboolean notified;
};

static void
expect_items_changed (struct ItemsChangedData *expected,
                      guint position,
                      guint removed,
                      guint added)
{
  expected->position = position;
  expected->removed = removed;
  expected->added = added;
  expected->called = FALSE;
  expected->notified = FALSE;
}

static void
on_items_changed (GListModel *model,
                  guint position,
                  guint removed,
                  guint added,
                  struct ItemsChangedData *expected)
{
  g_assert_false (expected->called);
  g_assert_cmpuint (expected->position, ==, position);
  g_assert_cmpuint (expected->removed, ==, removed);
  g_assert_cmpuint (expected->added, ==, added);
  expected->called = TRUE;
}

static void
on_notify_n_items (GListModel *model,
                   GParamSpec *pspec,
                   struct ItemsChangedData *expected)
{
  g_assert_false (expected->notified);
  expected->notified = TRUE;
}

/* Test that all operations on the list emit the items-changed signal */
static void
test_store_signal_items_changed (void)
{
  GListStore *store;
  GListModel *model;
  GSimpleAction *item;
  struct ItemsChangedData expected = {0};

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  g_object_connect (model, "signal::items-changed",
                    on_items_changed, &expected, NULL);
  g_object_connect (model, "signal::notify::n-items",
                    on_notify_n_items, &expected, NULL);

  /* Emit the signal manually */
  expect_items_changed (&expected, 0, 0, 0);
  g_list_model_items_changed (model, 0, 0, 0);
  g_assert_true (expected.called);
  g_assert_false (expected.notified);

  /* Append an item */
  expect_items_changed (&expected, 0, 0, 1);
  item = g_simple_action_new ("2", NULL);
  g_list_store_append (store, item);
  g_object_unref (item);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  /* Insert an item */
  expect_items_changed (&expected, 1, 0, 1);
  item = g_simple_action_new ("1", NULL);
  g_list_store_insert (store, 1, item);
  g_object_unref (item);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  /* Sort the list */
  expect_items_changed (&expected, 0, 2, 2);
  g_list_store_sort (store,
                     (GCompareDataFunc) cmp_action_by_name,
                     NULL);
  g_assert_true (expected.called);
  g_assert_false (expected.notified);

  /* Insert sorted */
  expect_items_changed (&expected, 2, 0, 1);
  item = g_simple_action_new ("3", NULL);
  g_list_store_insert_sorted (store,
                              item,
                              (GCompareDataFunc) cmp_action_by_name,
                              NULL);
  g_object_unref (item);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  /* Remove an item */
  expect_items_changed (&expected, 1, 1, 0);
  g_list_store_remove (store, 1);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  /* Splice */
  expect_items_changed (&expected, 0, 2, 1);
  item = g_simple_action_new ("4", NULL);
  assert_cmpitems (store, >=, 2);
  g_list_store_splice (store, 0, 2, (gpointer)&item, 1);
  g_object_unref (item);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  /* Splice to replace */
  expect_items_changed (&expected, 0, 1, 1);
  item = g_simple_action_new ("5", NULL);
  assert_cmpitems (store, >=, 1);
  g_list_store_splice (store, 0, 1, (gpointer)&item, 1);
  g_object_unref (item);
  g_assert_true (expected.called);
  g_assert_false (expected.notified);

  /* Remove all */
  expect_items_changed (&expected, 0, 1, 0);
  assert_cmpitems (store, ==, 1);
  g_list_store_remove_all (store);
  g_assert_true (expected.called);
  g_assert_true (expected.notified);

  g_object_unref (store);
}

/* Due to an overflow in the list store last-iter optimization,
 * the sequence 'lookup 0; lookup MAXUINT' was returning the
 * same item twice, and not NULL for the second lookup.
 * See #1639.
 */
static void
test_store_past_end (void)
{
  GListStore *store;
  GListModel *model;
  GSimpleAction *item;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);
  model = G_LIST_MODEL (store);

  item = g_simple_action_new ("2", NULL);
  g_list_store_append (store, item);
  g_object_unref (item);

  assert_cmpitems (store, ==, 1);
  item = g_list_model_get_item (model, 0);
  g_assert_nonnull (item);
  g_object_unref (item);
  item = g_list_model_get_item (model, G_MAXUINT);
  g_assert_null (item);

  g_object_unref (store);
}

static gboolean
list_model_casecmp_action_by_name (gconstpointer a,
                                   gconstpointer b)
{
  return g_ascii_strcasecmp (g_action_get_name (G_ACTION (a)),
                             g_action_get_name (G_ACTION (b))) == 0;
}

static gboolean
list_model_casecmp_action_by_name_full (gconstpointer a,
                                        gconstpointer b,
                                        gpointer      user_data)
{
  char buf[4];
  const char *suffix = user_data;

  g_snprintf (buf, sizeof buf, "%s%s", g_action_get_name (G_ACTION (b)), suffix);
  return g_strcmp0 (g_action_get_name (G_ACTION (a)), buf) == 0;
}

/* Test if find() and find_with_equal_func() works */
static void
test_store_find (void)
{
  GListStore *store;
  guint position = 100;
  const gchar *item_strs[4] = { "aaa", "bbb", "xxx", "ccc" };
  GSimpleAction *items[4] = { NULL, };
  GSimpleAction *other_item;
  guint i;

  store = g_list_store_new (G_TYPE_SIMPLE_ACTION);

  for (i = 0; i < G_N_ELEMENTS (item_strs); i++)
    items[i] = g_simple_action_new (item_strs[i], NULL);

  /* Shouldn't crash on an empty list, or change the position pointer */
  g_assert_false (g_list_store_find (store, items[0], NULL));
  g_assert_false (g_list_store_find (store, items[0], &position));
  g_assert_cmpint (position, ==, 100);

  for (i = 0; i < G_N_ELEMENTS (item_strs); i++)
    g_list_store_append (store, items[i]);

  /* Check whether it could still find the the elements */
  for (i = 0; i < G_N_ELEMENTS (item_strs); i++)
    {
      g_assert_true (g_list_store_find (store, items[i], &position));
      g_assert_cmpint (position, ==, i);
      /* Shouldn't try to write to position pointer if NULL given */
      g_assert_true (g_list_store_find (store, items[i], NULL));
    }

  /* try to find element not part of the list */
  other_item = g_simple_action_new ("111", NULL);
  g_assert_false (g_list_store_find (store, other_item, NULL));
  g_clear_object (&other_item);

  /* Re-add item; find() should only return the first position */
  g_list_store_append (store, items[0]);
  g_assert_true (g_list_store_find (store, items[0], &position));
  g_assert_cmpint (position, ==, 0);

  /* try to find element which should only work with custom equality check */
  other_item = g_simple_action_new ("XXX", NULL);
  g_assert_false (g_list_store_find (store, other_item, NULL));
  g_assert_true (g_list_store_find_with_equal_func (store,
                                                    other_item,
                                                    list_model_casecmp_action_by_name,
                                                    &position));
  g_assert_cmpint (position, ==, 2);
  g_clear_object (&other_item);

  /* try to find element which should only work with custom equality check and string concat */
  other_item = g_simple_action_new ("c", NULL);
  g_assert_false (g_list_store_find (store, other_item, NULL));
  g_assert_true (g_list_store_find_with_equal_func_full (store,
                                                         other_item,
                                                         list_model_casecmp_action_by_name_full,
                                                         "cc",
                                                         &position));
  g_assert_cmpint (position, ==, 3);
  g_clear_object (&other_item);

  for (i = 0; i < G_N_ELEMENTS (item_strs); i++)
    g_clear_object(&items[i]);
  g_clear_object (&store);
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glistmodel/store/properties", test_store_properties);
  g_test_add_func ("/glistmodel/store/non-gobjects", test_store_non_gobjects);
  g_test_add_func ("/glistmodel/store/boundaries", test_store_boundaries);
  g_test_add_func ("/glistmodel/store/refcounts", test_store_refcounts);
  g_test_add_func ("/glistmodel/store/sorted", test_store_sorted);
  g_test_add_func ("/glistmodel/store/splice-replace-middle",
                   test_store_splice_replace_middle);
  g_test_add_func ("/glistmodel/store/splice-replace-all",
                   test_store_splice_replace_all);
  g_test_add_func ("/glistmodel/store/splice-noop", test_store_splice_noop);
  g_test_add_func ("/glistmodel/store/splice-remove-multiple",
                   test_store_splice_remove_multiple);
  g_test_add_func ("/glistmodel/store/splice-add-multiple",
                   test_store_splice_add_multiple);
  g_test_add_func ("/glistmodel/store/splice-wrong-type",
                   test_store_splice_wrong_type);
  g_test_add_func ("/glistmodel/store/item-type",
                   test_store_item_type);
  g_test_add_func ("/glistmodel/store/remove-all",
                   test_store_remove_all);
  g_test_add_func ("/glistmodel/store/sort",
                   test_store_sort);
  g_test_add_func ("/glistmodel/store/get-item-cache",
                   test_store_get_item_cache);
  g_test_add_func ("/glistmodel/store/items-changed",
                   test_store_signal_items_changed);
  g_test_add_func ("/glistmodel/store/past-end", test_store_past_end);
  g_test_add_func ("/glistmodel/store/find", test_store_find);

  return g_test_run ();
}
