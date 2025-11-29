/* Treelistmodel tests.
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
prepend (GListStore *store,
         guint       number,
         guint       step)
{
  GObject *object;

  /* 0 cannot be differentiated from NULL, so don't use it */
  g_assert_cmpint (number, !=, 0);

  if (step / 10)
    object = G_OBJECT (new_store (number - 9 * step / 10, number, step / 10));
  else
    object = g_object_new (G_TYPE_OBJECT, NULL);
  g_object_set_qdata (object, number_quark, GUINT_TO_POINTER (number));
  g_list_store_insert (store, 0, object);
  g_object_unref (object);
}

static void
assert_items_changed_correctly (GListModel *model,
                                guint       position,
                                guint       removed,
                                guint       added,
                                GListModel *compare)
{
  guint i, n_items;

  //sanity check that we got all notifies
  g_assert_cmpuint (g_list_model_get_n_items (compare), ==, GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (compare), "last-notified-n-items")));

  //g_print ("%s => %u -%u +%u => %s\n", model_to_string (compare), position, removed, added, model_to_string (model));

  g_assert_cmpint (g_list_model_get_n_items (model), ==, g_list_model_get_n_items (compare) - removed + added);
  n_items = g_list_model_get_n_items (model);

  /* Check that all unchanged items are indeed unchanged */
  for (i = 0; i < position; i++)
    {
      gpointer o1 = g_list_model_get_item (model, i);
      gpointer o2 = g_list_model_get_item (compare, i);
      g_assert_cmphex (GPOINTER_TO_SIZE (o1), ==, GPOINTER_TO_SIZE (o2));
      g_object_unref (o1);
      g_object_unref (o2);
    }
  for (i = position + added; i < n_items; i++)
    {
      gpointer o1 = g_list_model_get_item (model, i);
      gpointer o2 = g_list_model_get_item (compare, i - added + removed);
      g_assert_cmphex (GPOINTER_TO_SIZE (o1), ==, GPOINTER_TO_SIZE (o2));
      g_object_unref (o1);
      g_object_unref (o2);
    }

  /*doesn't hold with passthrough, because we reuse the same children */
#if 0
  /* Check that the first and last added item are different from
   * first and last removed item.
   * Otherwise we could have kept them as-is
   */
  if (removed > 0 && added > 0)
    {
      gpointer o1 = g_list_model_get_item (model, position);
      gpointer o2 = g_list_model_get_item (compare, position);
      g_assert_cmphex (GPOINTER_TO_SIZE (o1), !=, GPOINTER_TO_SIZE (o2));
      g_object_unref (o1);
      g_object_unref (o2);

      o1 = g_list_model_get_item (model, position + added - 1);
      o2 = g_list_model_get_item (compare, position + removed - 1);
      g_assert_cmphex (GPOINTER_TO_SIZE (o1), !=, GPOINTER_TO_SIZE (o2));
      g_object_unref (o1);
      g_object_unref (o2);
    }
#endif

  /* Finally, perform the same change as the signal indicates */
  g_list_store_splice (G_LIST_STORE (compare), position, removed, NULL, 0);
  for (i = position; i < position + added; i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (model), i);
      g_list_store_insert (G_LIST_STORE (compare), i, item);
      g_object_unref (item);
    }
}

static void
assert_n_items_notified_properly (GListModel *model,
                                  GParamSpec *pspec,
                                  GListModel *compare)
{
  g_assert_cmpuint (g_list_model_get_n_items (model), !=, GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (compare), "last-notified-n-items")));

  /* These should hve been updated in items-changed, which should have been emitted first */
  g_assert_cmpuint (g_list_model_get_n_items (model), ==, g_list_model_get_n_items (compare));

  g_object_set_data (G_OBJECT (compare),
                     "last-notified-n-items",
                     GUINT_TO_POINTER (g_list_model_get_n_items (model)));
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

static void
check_model_changes (GListModel *model)
{
  GListStore *check;
  gsize i;

  check = g_list_store_new (g_list_model_get_item_type (model));

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      gpointer item = g_list_model_get_item (model, i);
      g_list_store_append (check, item);
      g_object_unref (item);
    }
  g_signal_connect_data (model,
                         "items-changed",
                         G_CALLBACK (assert_items_changed_correctly), 
                         check,
                         (GClosureNotify) g_object_unref,
                         0);

  g_object_set_data (G_OBJECT (check),
                     "last-notified-n-items",
                     GUINT_TO_POINTER (g_list_model_get_n_items (G_LIST_MODEL (check))));
  g_signal_connect_data (model,
                         "notify::n-items",
                         G_CALLBACK (assert_n_items_notified_properly), 
                         g_object_ref (check),
                         (GClosureNotify) g_object_unref,
                         0);
}

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
    prepend (store, i, step);

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

static GListModel *
create_sub_model_cb (gpointer item,
                     gpointer unused)
{
  if (G_IS_LIST_MODEL (item))
    return g_object_ref (item);

  return NULL;
}

static GtkTreeListModel *
new_model (guint    size,
           gboolean expanded)
{
  GtkTreeListModel *tree;
  GString *changes;

  tree = gtk_tree_list_model_new (G_LIST_MODEL (new_store (size, size, size)), TRUE, expanded, create_sub_model_cb, NULL, NULL);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(tree), changes_quark, changes, free_changes);
  g_signal_connect (tree, "items-changed", G_CALLBACK (items_changed), changes);
  g_signal_connect (tree, "notify::n-items", G_CALLBACK (notify_n_items), changes);

  return tree;
}

static void
test_expand (void)
{
  GtkTreeListModel *tree = new_model (100, FALSE);
  guint i;

  assert_model (tree, "100");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 90 80 70 60 50 40 30 20 10");
  assert_changes (tree, "1+10*");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "11+10*, 10+10*, 9+10*, 8+10*, 7+10*, 6+10*, 5+10*, 4+10*, 3+10*, 2+10*");

  for (i = g_list_model_get_n_items (G_LIST_MODEL (tree)); i > 0; i--)
    {
      GtkTreeListRow *row = gtk_tree_list_model_get_row (tree, i - 1);
      gtk_tree_list_row_set_expanded (row, TRUE);
      g_object_unref (row);
    }
  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "");

  g_object_unref (tree);
}

static void
test_remove_some (void)
{
  GtkTreeListModel *tree = new_model (100, TRUE);
  gpointer item;

  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 1);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 3);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "-5*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 0);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 3);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "33-11*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 88);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 9);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2");
  assert_changes (tree, "-98*");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 0);
  g_assert_true (G_IS_LIST_MODEL (item));
  g_list_store_remove (item, 8);
  assert_model (tree, "100 100 100 99 98 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11");
  assert_changes (tree, "88-10*");

  g_object_unref (tree);
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
test_splice (void)
{
  GtkTreeListModel *tree = new_model (100, TRUE);
  gpointer item;

  assert_model (tree, "100 100 100 99 98 97 96 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "");

  item = g_list_model_get_item (G_LIST_MODEL (tree), 1);
  g_assert_true (G_IS_LIST_MODEL (item));
  splice (item, 0, 5, (guint[5]) { 300, 301, 302, 303, 304 }, 5);
  /* expected */
  assert_model (tree, "100 100 300 301 302 303 304 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  /* real outcome */
  // assert_model (tree, "100 100 304 303 302 301 300 95 94 93 92 91 90 90 89 88 87 86 85 84 83 82 81 80 80 79 78 77 76 75 74 73 72 71 70 70 69 68 67 66 65 64 63 62 61 60 60 59 58 57 56 55 54 53 52 51 50 50 49 48 47 46 45 44 43 42 41 40 40 39 38 37 36 35 34 33 32 31 30 30 29 28 27 26 25 24 23 22 21 20 20 19 18 17 16 15 14 13 12 11 10 10 9 8 7 6 5 4 3 2 1");
  assert_changes (tree, "2-5+5");

  g_object_unref (tree);
}

/* Test for https://gitlab.gnome.org/GNOME/gtk/-/issues/4595 */
typedef struct _DemoNode DemoNode;

struct _DemoNode {
  GObject parent_instance;
  char *value;
  GListStore *children;
};

G_DECLARE_FINAL_TYPE (DemoNode, demo_node, DEMO, NODE, GObject);

G_DEFINE_TYPE (DemoNode, demo_node, G_TYPE_OBJECT);

static void
demo_node_init (DemoNode *node)
{
}

static void
demo_node_finalize (GObject *object)
{
  g_free (DEMO_NODE (object)->value);

  G_OBJECT_CLASS (demo_node_parent_class)->finalize (object);
}

static void
demo_node_class_init (DemoNodeClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = demo_node_finalize;
}

static DemoNode *
demo_node_new (const char *value,
               GListStore *children)
{
  DemoNode *result;

  result = g_object_new (demo_node_get_type(), NULL);
  result->value = g_strdup (value);
  if (children)
    result->children = g_object_ref (children);

  return result;
}

static GListStore *
create_model (void)
{
  DemoNode *aa, *a, *b, *c;
  GListStore *a_children, *root;

  aa = demo_node_new ("aa", NULL);

  a_children = g_list_store_new (demo_node_get_type ());
  g_list_store_append (a_children, aa);

  a = demo_node_new ("a", a_children);
  b = demo_node_new ("b", NULL);
  c = demo_node_new ("c", NULL);

  root = g_list_store_new (demo_node_get_type ());
  g_list_store_append (root, a);
  g_list_store_append (root, b);
  g_list_store_append (root, c);

  g_object_unref (aa);
  g_object_unref (a_children);
  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (c);

  return root;
}

static GListModel *
demo_node_children (gpointer item,
                    gpointer unused)
{
  GListStore *children;

  children = DEMO_NODE (item)->children;
  if (children)
    return G_LIST_MODEL (g_object_ref (children));

  return NULL;
}

static void
test_collapse_change (void)
{
  GListStore *model;
  GtkTreeListModel *treemodel;
  DemoNode *a, *ab;
  GtkTreeListRow *row;

  model = create_model ();
  a = g_list_model_get_item (G_LIST_MODEL (model), 0);

  treemodel = gtk_tree_list_model_new (G_LIST_MODEL (model),
                                       FALSE,
                                       FALSE,
                                       demo_node_children,
                                       NULL,
                                       NULL);

  row = gtk_tree_list_model_get_row (treemodel, 0);
  gtk_tree_list_row_set_expanded (row, TRUE);
  gtk_tree_list_row_set_expanded (row, FALSE);
  g_object_unref (row);

  ab = demo_node_new ("ab", NULL);
  g_list_store_append (a->children, ab);
  g_object_unref (ab);

  g_object_unref (treemodel);
  g_object_unref (a);
}

static void
test_same_child_model (void)
{
#define N_MODELS 4
  static const guint N_ITEMS_PER_MODEL = 3;
  static const guint N_RUNS = 100;

  GListStore *models[N_MODELS];
  GtkTreeListModel *treelist;
  DemoNode *d;
  guint i, j;
  char str[2] = { 0, };

  i = N_MODELS;
  while (i --> 0)
    {
      models[i] = g_list_store_new (demo_node_get_type ());
      for (j = 0; j < N_ITEMS_PER_MODEL; j++)
        {
          str[0] = 'A' + j;
          d = demo_node_new (str, i + 1 < N_MODELS ? models[i + 1] : NULL);
          g_list_store_append (models[i], d);
        }
    }

  treelist = gtk_tree_list_model_new (g_object_ref (G_LIST_MODEL (models[0])),
                                      g_test_rand_bit (), TRUE,
                                      demo_node_children,
                                      NULL, NULL);
  check_model_changes (G_LIST_MODEL (treelist));

  for (i = 0; i < N_RUNS; i++)
    {
      guint model_id = g_test_rand_int_range (0, N_MODELS);
      GListStore *model = models[model_id];
      guint n_items = g_list_model_get_n_items (G_LIST_MODEL (model));

      switch (g_test_rand_int_range (0, 3))
        {
          case 0:
            if (n_items < 10)
              {
                str[0] = 'A' + (i % 26);
                d = demo_node_new (str, model_id + 1 < N_MODELS ? models[model_id + 1] : NULL);
                g_list_store_insert (model,
                                     g_test_rand_int_range (0, n_items + 1),
                                     d);
                g_object_unref (d);
              }
            break;
          case 1:
            if (n_items > 0)
              g_list_store_remove (model, g_test_rand_int_range (0, n_items));
            break;
          case 2:
            str[0] = 'A' + (i % 26);
            d = demo_node_new (str, model_id + 1 < N_MODELS ? models[model_id + 1] : NULL);
            g_list_store_splice (model,
                                 n_items ? g_test_rand_int_range (0, n_items) : 0,
                                 n_items ? 1 : 0,
                                 (gpointer[1]) { d }, 1);
            g_object_unref (d);
            break;
          default:
            g_assert_not_reached ();
            break;
        }
  }

  g_object_unref (treelist);
  for (i = 0; i < G_N_ELEMENTS (models); i++)
    g_object_unref (models[i]);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");
  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/treelistmodel/expand", test_expand);
  g_test_add_func ("/treelistmodel/remove_some", test_remove_some);
  g_test_add_func ("/treelistmodel/remove_splice", test_splice);
  g_test_add_func ("/treelistmodel/collapse-change", test_collapse_change);
  g_test_add_func ("/treelistmodel/same-child-model", test_same_child_model);

  return g_test_run ();
}
