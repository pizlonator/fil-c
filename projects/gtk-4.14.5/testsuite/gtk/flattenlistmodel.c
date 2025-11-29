/* Flattenlistmodel tests.
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
  GObject **objects = g_newa (GObject *, added);
  guint i;

  for (i = 0; i < added; i++)
    {
      /* 0 cannot be differentiated from NULL, so don't use it */
      g_assert_cmpint (numbers[i], !=, 0);
      objects[i] = g_object_new (G_TYPE_OBJECT, NULL);
      g_object_set_qdata (objects[i], number_quark, GUINT_TO_POINTER (numbers[i]));
    }

  g_list_store_splice (store, pos, removed, (gpointer *) objects, added);

  for (i = 0; i < added; i++)
    g_object_unref (objects[i]);
}

static void
insert (GListStore *store,
        guint       pos,
        guint       number)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_insert (store, pos, object);
  g_object_unref (object);
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

static GListStore *
add_store (GListStore *store,
           guint       start,
           guint       end,
           guint       step)
{
  GListStore *child;

  child = new_store (start, end, step);
  g_list_store_append (store, child);
  g_object_unref (child);

  return child;
}

#define assert_model(model, expected) G_STMT_START{ \
  char *s = model_to_string (G_LIST_MODEL (model)); \
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

static GtkFlattenListModel *
new_model (GListStore *store)
{
  GtkFlattenListModel *result;
  GString *changes;

  if (store)
    g_object_ref (store);
  result = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (result, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return result;
}

static void
test_create_empty (void)
{
  GtkFlattenListModel *flat;

  flat = new_model (NULL);
  assert_model (flat, "");
  assert_changes (flat, "");

  g_object_unref (flat);
}

static void
test_create (void)
{
  GtkFlattenListModel *flat;
  GListStore *model;
  
  model = g_list_store_new (G_TYPE_LIST_MODEL);
  add_store (model, 1, 3, 1);
  add_store (model, 4, 4, 1);
  add_store (model, 5, 7, 1);
  add_store (model, 8, 10, 1);
  flat = new_model (model);
  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "");

  g_object_unref (model);
  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "");

  g_object_unref (flat);
}

static void
test_model_add (void)
{
  GtkFlattenListModel *flat;
  GListStore *model;
  
  model = g_list_store_new (G_TYPE_LIST_MODEL);
  flat = new_model (model);
  assert_model (flat, "");
  assert_changes (flat, "");

  add_store (model, 1, 3, 1);
  add_store (model, 4, 4, 1);
  add_store (model, 5, 7, 1);
  add_store (model, 8, 10, 1);

  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "0+3*, +3*, 4+3*, 7+3*");

  g_object_unref (model);
  g_object_unref (flat);
}

static void
test_submodel_add (void)
{
  GtkFlattenListModel *flat;
  GListStore *model, *store[4];
  
  model = g_list_store_new (G_TYPE_LIST_MODEL);
  flat = new_model (model);
  assert_model (flat, "");
  assert_changes (flat, "");

  store[0] = add_store (model, 2, 3, 1);
  store[1] = add_store (model, 4, 4, 1);
  store[2] = add_store (model, 5, 4, 1);
  store[3] = add_store (model, 8, 8, 1);
  assert_model (flat, "2 3 4 8");
  assert_changes (flat, "0+2*, +2*, +3*");

  insert (store[0], 0, 1);
  splice (store[2], 0, 0, (guint[3]) { 5, 6, 7 }, 3);
  splice (store[3], 1, 0, (guint[2]) { 9, 10 }, 2);
  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "+0*, 4+3*, 8+2*");

  g_object_unref (model);
  g_object_unref (flat);
}

static void
test_submodel_add2 (void)
{
  GtkFlattenListModel *flat;
  GListStore *model, *store[3];

  model = g_list_store_new (G_TYPE_LIST_MODEL);
  flat = new_model (model);
  assert_model (flat, "");
  assert_changes (flat, "");

  store[0] = add_store (model, 1, 0, 0);
  store[1] = add_store (model, 1, 0, 0);
  store[2] = add_store (model, 1, 0, 0);

  assert_model (flat, "");
  assert_changes (flat, "");

  add (store[0], 1);
  assert_model (flat, "1");
  assert_changes (flat, "+0*");

  add (store[1], 3);
  assert_model (flat, "1 3");
  assert_changes (flat, "+1*");

  add (store[0], 2);
  assert_model (flat, "1 2 3");
  assert_changes (flat, "+1*");

  add (store[1], 4);
  assert_model (flat, "1 2 3 4");
  assert_changes (flat, "+3*");

  g_object_unref (model);
  g_object_unref (flat);
}

static void
test_model_remove (void)
{
  GtkFlattenListModel *flat;
  GListStore *model;
  
  model = g_list_store_new (G_TYPE_LIST_MODEL);
  add_store (model, 1, 3, 1);
  add_store (model, 4, 4, 1);
  add_store (model, 5, 7, 1);
  add_store (model, 8, 10, 1);
  flat = new_model (model);
  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "");

  splice (model, 1, 2, NULL, 0);
  g_list_store_remove (model, 1);
  g_list_store_remove (model, 0);
  g_object_unref (model);
  assert_model (flat, "");
  assert_changes (flat, "3-4*, 3-3*, 0-3*");

  g_object_unref (flat);
}

static void
test_submodel_remove (void)
{
  GtkFlattenListModel *flat;
  GListStore *model, *store[4];
  
  model = g_list_store_new (G_TYPE_LIST_MODEL);
  store[0] = add_store (model, 1, 3, 1);
  store[1] = add_store (model, 4, 4, 1);
  store[2] = add_store (model, 5, 7, 1);
  store[3] = add_store (model, 8, 10, 1);
  flat = new_model (model);
  assert_model (flat, "1 2 3 4 5 6 7 8 9 10");
  assert_changes (flat, "");

  g_list_store_remove (store[0], 0);
  splice (store[2], 0, 3, NULL, 0);
  splice (store[3], 1, 2, NULL, 0);
  g_object_unref (model);

  assert_model (flat, "2 3 4 8");
  assert_changes (flat, "-0*, 3-3*, 4-2*");

  g_object_unref (flat);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/flattenlistmodel/create_empty", test_create_empty);
  g_test_add_func ("/flattenlistmodel/create", test_create);
  g_test_add_func ("/flattenlistmodel/model/add", test_model_add);
  g_test_add_func ("/flattenlistmodel/submodel/add", test_submodel_add);
  g_test_add_func ("/flattenlistmodel/submodel/add2", test_submodel_add2);
  g_test_add_func ("/flattenlistmodel/model/remove", test_model_remove);
  g_test_add_func ("/flattenlistmodel/submodel/remove", test_submodel_remove);

  return g_test_run ();
}
