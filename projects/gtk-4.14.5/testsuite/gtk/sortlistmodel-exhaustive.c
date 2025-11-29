/*
 * Copyright © 2020 Benjamin Otte
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

#define ensure_updated() G_STMT_START{ \
  while (g_main_context_pending (NULL)) \
    g_main_context_iteration (NULL, TRUE); \
}G_STMT_END

#define assert_model_equal(model1, model2) G_STMT_START{ \
  guint _i, _n; \
  g_assert_cmpint (g_list_model_get_n_items (model1), ==, g_list_model_get_n_items (model2)); \
  _n = g_list_model_get_n_items (model1); \
  for (_i = 0; _i < _n; _i++) \
    { \
      gpointer o1 = g_list_model_get_item (model1, _i); \
      gpointer o2 = g_list_model_get_item (model2, _i); \
\
      if (o1 != o2) \
        { \
          char *_s = g_strdup_printf ("Objects differ at index %u out of %u", _i, _n); \
          g_print ("%s\n", model_to_string (model1)); \
          g_print ("%s\n", model_to_string (model2)); \
         g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, _s); \
          g_free (_s); \
        } \
\
      g_object_unref (o1); \
      g_object_unref (o2); \
    } \
}G_STMT_END

#define assert_model_sections(model) G_STMT_START{ \
  guint _i, _start, _end; \
  _start = 0; \
  _end = 0; \
  for (_i = 0; _i < G_MAXUINT; _i = _end) \
    { \
      gtk_section_model_get_section (GTK_SECTION_MODEL (model), _i, &_start, &_end); \
\
      g_assert_cmpint (_start, ==, _i); \
      g_assert_cmpint (_end, >, _i); \
    } \
}G_STMT_END

G_GNUC_UNUSED static char *
model_to_string (GListModel *model)
{
  GString *string;
  guint i, n;

  n = g_list_model_get_n_items (model);
  string = g_string_new (NULL);

  /* Check that all unchanged items are indeed unchanged */
  for (i = 0; i < n; i++)
    {
      gpointer item, model_item = g_list_model_get_item (model, i);
      if (GTK_IS_TREE_LIST_ROW (model_item))
        item = gtk_tree_list_row_get_item (model_item);
      else
        item = model_item;

      if (i > 0)
        g_string_append (string, ", ");
      if (G_IS_LIST_MODEL (item))
        g_string_append (string, "*");
      else
        g_string_append (string, gtk_string_object_get_string (item));
      g_object_unref (model_item);
    }

  return g_string_free (string, FALSE);
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

  g_assert_cmpuint (g_list_model_get_n_items (model), ==, g_list_model_get_n_items (compare) - removed + added);
  n_items = g_list_model_get_n_items (model);

  if (position != 0 || removed != n_items)
    {
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
    }

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

static GtkSortListModel *
sort_list_model_new (GListModel *source,
                     GtkSorter  *sorter)
{
  GtkSortListModel *model;
  GListStore *check;
  guint i;

  model = gtk_sort_list_model_new (source, sorter);
  check = g_list_store_new (G_TYPE_OBJECT);
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (model)); i++)
    {
      gpointer item = g_list_model_get_item (G_LIST_MODEL (model), i);
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

  return model;
}

#define N_MODELS 8

static char *
create_test_name (guint id)
{
  GString *s = g_string_new ("");

  if (id & (1 << 0))
    g_string_append (s, "set-model");
  else
    g_string_append (s, "construct-with-model");

  if (id & (1 << 1))
    g_string_append (s, "/set-sorter");
  else
    g_string_append (s, "/construct-with-sorter");

  if (id & (1 << 2))
    g_string_append (s, "/incremental");
  else
    g_string_append (s, "/non-incremental");

  return g_string_free (s, FALSE);
}

static GtkSortListModel *
create_sort_list_model (gconstpointer  model_id,
                        gboolean       track_changes,
                        GListModel    *source,
                        GtkSorter     *sorter)
{
  GtkSortListModel *model;
  guint id = GPOINTER_TO_UINT (model_id);

  if (track_changes)
    model = sort_list_model_new (((id & 1) || !source) ? NULL : g_object_ref (source), ((id & 2) || !sorter) ? NULL : g_object_ref (sorter));
  else
    model = gtk_sort_list_model_new (((id & 1) || !source) ? NULL : g_object_ref (source), ((id & 2) || !sorter) ? NULL : g_object_ref (sorter));

  switch (id >> 2)
  {
    case 0:
      break;

    case 1:
      gtk_sort_list_model_set_incremental (model, TRUE);
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  if (id & 1)
    gtk_sort_list_model_set_model (model, source);
  if (id & 2)
    gtk_sort_list_model_set_sorter (model, sorter);

  return model;
}

static GListModel *
create_source_model (guint min_size, guint max_size)
{
  const char *strings[] = { "A", "a", "B", "b" };
  GtkStringList *list;
  guint i, size;

  size = g_test_rand_int_range (min_size, max_size + 1);
  list = gtk_string_list_new (NULL);

  for (i = 0; i < size; i++)
    gtk_string_list_append (list, strings[g_test_rand_int_range (0, G_N_ELEMENTS (strings))]);

  return G_LIST_MODEL (list);
}

#define N_SORTERS 3

static GtkSorter *
create_sorter (gsize id)
{
  GtkSorter *sorter;

  switch (id)
  {
    case 0:
      return GTK_SORTER (gtk_string_sorter_new (NULL));

    case 1:
    case 2:
      /* match all As, Bs and nothing */
      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
      if (id == 1)
        gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), FALSE);
      return sorter;

    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static GtkSorter *
create_random_sorter (gboolean allow_null)
{
  guint n;

  if (allow_null)
    n = g_test_rand_int_range (0, N_SORTERS + 1);
  else
    n = g_test_rand_int_range (0, N_SORTERS);
  
  if (n >= N_SORTERS)
    return NULL;

  return create_sorter (n);
}

/* Compare this:
 *   source => sorter1 => sorter2
 * with:
 *   source => multisorter(sorter1, sorter2)
 * and randomly change the source and sorters and see if the
 * two continue agreeing.
 */
static void
test_two_sorters (gconstpointer model_id)
{
  GtkSortListModel *compare;
  GtkSortListModel *model1, *model2;
  GListModel *source;
  GtkSorter *every, *sorter;
  guint i, j, k;

  source = create_source_model (10, 10);
  model2 = create_sort_list_model (model_id, TRUE, source, NULL);
  /* can't track changes from a sortmodel, where the same items get reordered */
  model1 = create_sort_list_model (model_id, FALSE, G_LIST_MODEL (model2), NULL);
  every = GTK_SORTER (gtk_multi_sorter_new ());
  compare = create_sort_list_model (model_id, TRUE, source, every);
  g_object_unref (every);
  g_object_unref (source);

  for (i = 0; i < N_SORTERS; i++)
    {
      sorter = create_sorter (i);
      gtk_sort_list_model_set_sorter (model1, sorter);
      gtk_multi_sorter_append (GTK_MULTI_SORTER (every), sorter);

      for (j = 0; j < N_SORTERS; j++)
        {
          sorter = create_sorter (i);
          gtk_sort_list_model_set_sorter (model2, sorter);
          gtk_multi_sorter_append (GTK_MULTI_SORTER (every), sorter);

          ensure_updated ();
          assert_model_equal (G_LIST_MODEL (model2), G_LIST_MODEL (compare));

          for (k = 0; k < 10; k++)
            {
              source = create_source_model (0, 1000);
              gtk_sort_list_model_set_model (compare, source);
              gtk_sort_list_model_set_model (model2, source);
              g_object_unref (source);

              ensure_updated ();
              assert_model_equal (G_LIST_MODEL (model1), G_LIST_MODEL (compare));
            }

          gtk_multi_sorter_remove (GTK_MULTI_SORTER (every), 1);
        }

      gtk_multi_sorter_remove (GTK_MULTI_SORTER (every), 0);
    }

  g_object_unref (compare);
  g_object_unref (model2);
  g_object_unref (model1);
}

/* Run:
 *   source => sorter1 => sorter2
 * and randomly add/remove sources and change the sorters and
 * see if the two sorters stay identical
 */
static void
test_stability (gconstpointer model_id)
{
  GListStore *store;
  GtkFlattenListModel *flatten;
  GtkSortListModel *sort1, *sort2;
  GtkSorter *sorter;
  gsize i;

  sorter = create_random_sorter (TRUE);

  store = g_list_store_new (G_TYPE_OBJECT);
  flatten = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  sort1 = create_sort_list_model (model_id, TRUE, G_LIST_MODEL (flatten), sorter);
  sort2 = create_sort_list_model (model_id, FALSE, G_LIST_MODEL (sort1), sorter);
  g_clear_object (&sorter);

  for (i = 0; i < 500; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position;

      switch (g_test_rand_int_range (0, 4))
      {
        case 0:
          /* change the sorter */
          sorter = create_random_sorter (TRUE);
          gtk_sort_list_model_set_sorter (sort1, sorter);
          gtk_sort_list_model_set_sorter (sort2, sorter);
          g_clear_object (&sorter);
          break;

        case 1:
          /* remove a model */
          remove = TRUE;
          break;

        case 2:
          /* add a model */
          add = TRUE;
          break;

        case 3:
          /* replace a model */
          remove = TRUE;
          add = TRUE;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the sorters will see no changes */
          GListModel *source = create_source_model (1, 50);
          g_list_store_splice (store,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store, position);
        }

      if (g_test_rand_bit ())
        {
          ensure_updated ();
          assert_model_equal (G_LIST_MODEL (sort1), G_LIST_MODEL (sort2));
        }
    }

  g_object_unref (sort2);
  g_object_unref (sort1);
  g_object_unref (flatten);
}

static gboolean
string_is_lowercase (GtkStringObject *o)
{
  return g_ascii_islower (*gtk_string_object_get_string (o));
}

/* Run:
 *   source => section-sorter
 *   source => sorter
 * and set a section sorter on the section sorter that is a subsort of
 * the real sorter.
 *
 * And then randomly add/remove sources and change the sorters and
 * see if the two sorters stay identical
 */
static void
test_section_sorters (gconstpointer model_id)
{
  GListStore *store;
  GtkFlattenListModel *flatten;
  GtkSortListModel *sort1, *sort2;
  GtkSorter *sorter;
  gsize i;

  store = g_list_store_new (G_TYPE_OBJECT);
  flatten = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  sort1 = create_sort_list_model (model_id, FALSE, G_LIST_MODEL (flatten), NULL);
  sort2 = create_sort_list_model (model_id, FALSE, G_LIST_MODEL (flatten), NULL);

  for (i = 0; i < 500; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position;

      switch (g_test_rand_int_range (0, 4))
      {
        case 0:
          /* set the same sorter, once as section sorter, once as sorter */
          sorter = create_random_sorter (TRUE);
          gtk_sort_list_model_set_section_sorter (sort1, sorter);
          gtk_sort_list_model_set_sorter (sort1, NULL);
          gtk_sort_list_model_set_sorter (sort2, sorter);
          g_clear_object (&sorter);
          break;

        case 1:
          /* use a section sorter that is a more generic version of the sorter */
          sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
          gtk_string_sorter_set_ignore_case (GTK_STRING_SORTER (sorter), FALSE);
          gtk_sort_list_model_set_sorter (sort1, sorter);
          gtk_sort_list_model_set_sorter (sort2, sorter);
          g_clear_object (&sorter);
          sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_cclosure_expression_new (G_TYPE_BOOLEAN, NULL, 0, NULL, G_CALLBACK (string_is_lowercase), NULL, NULL)));
          gtk_sort_list_model_set_section_sorter (sort1, sorter);
          g_clear_object (&sorter);
          break;

        case 2:
          /* remove a model */
          remove = TRUE;
          break;

        case 3:
          /* add a model */
          add = TRUE;
          break;

        case 4:
          /* replace a model */
          remove = TRUE;
          add = TRUE;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the sorters will see no changes */
          GListModel *source = create_source_model (1, 50);
          g_list_store_splice (store,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store, position);
        }

      if (g_test_rand_bit ())
        {
          ensure_updated ();
          assert_model_equal (G_LIST_MODEL (sort1), G_LIST_MODEL (sort2));
        }

      if (g_test_rand_bit ())
        assert_model_sections (G_LIST_MODEL (sort1));
    }

  g_object_unref (sort2);
  g_object_unref (sort1);
  g_object_unref (flatten);
}

/* Run:
 *   source => sorter
 * And then randomly add/remove sources and change the sorters and
 * see if the invariants for sections keep correct.
 */
static void
test_sections (gconstpointer model_id)
{
  GListStore *store;
  GtkFlattenListModel *flatten;
  GtkSortListModel *sort;
  GtkSorter *sorter;
  gsize i;

  store = g_list_store_new (G_TYPE_OBJECT);
  flatten = gtk_flatten_list_model_new (G_LIST_MODEL (store));
  sort = create_sort_list_model (model_id, FALSE, G_LIST_MODEL (flatten), NULL);

  for (i = 0; i < 500; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position;

      switch (g_test_rand_int_range (0, 4))
      {
        case 0:
          /* set the same sorter, once as section sorter, once as sorter */
          sorter = create_random_sorter (TRUE);
          gtk_sort_list_model_set_sorter (sort, sorter);
          g_clear_object (&sorter);
          break;

        case 1:
          /* set the same sorter, once as section sorter, once as sorter */
          sorter = create_random_sorter (TRUE);
          gtk_sort_list_model_set_section_sorter (sort, sorter);
          g_clear_object (&sorter);
          break;

        case 2:
          /* remove a model */
          remove = TRUE;
          break;

        case 3:
          /* add a model */
          add = TRUE;
          break;

        case 4:
          /* replace a model */
          remove = TRUE;
          add = TRUE;
          break;

        default:
          g_assert_not_reached ();
          break;
      }

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the sorters will see no changes */
          GListModel *source = create_source_model (1, 50);
          g_list_store_splice (store,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store, position);
        }

      if (g_test_rand_bit ())
        ensure_updated ();

      if (g_test_rand_bit ())
        {
          guint start, end, pos, n, sec_start, sec_end;
          gpointer prev_item, item;

          n = g_list_model_get_n_items (G_LIST_MODEL (sort));
          sorter = gtk_sort_list_model_get_section_sorter (sort);
          start = end = 0;
          prev_item = item = NULL;

          for (pos = 0; pos < n; pos++)
            {
              gtk_section_model_get_section (GTK_SECTION_MODEL (sort), pos, &sec_start, &sec_end);
              prev_item = item;
              item = g_list_model_get_item (G_LIST_MODEL (sort), pos);
              if (end <= pos)
                {
                  g_assert_cmpint (pos, ==, end);
                  /* there should be a new section */
                  g_assert_cmpint (sec_start, ==, end);
                  g_assert_cmpint (sec_end, >, sec_start);
                  g_assert_cmpint (sec_end, <=, n);
                  start = sec_start;
                  end = sec_end;
                  if (prev_item)
                    {
                      g_assert_nonnull (sorter);
                      g_assert_cmpint (gtk_sorter_compare (sorter, prev_item, item), !=, GTK_ORDERING_EQUAL);
                    }
                }
              else
                {
                  /* the old section keeps on going */
                  g_assert_cmpint (sec_start, ==, start);
                  g_assert_cmpint (sec_end, ==, end);
                  if (prev_item && sorter)
                    g_assert_cmpint (gtk_sorter_compare (sorter, prev_item, item), ==, GTK_ORDERING_EQUAL);
                }
              g_clear_object (&prev_item);
            }

          g_clear_object (&item);

          /* for good measure, check the error condition */
          if (n < G_MAXINT32)
            {
              gtk_section_model_get_section (GTK_SECTION_MODEL (sort), g_test_rand_int_range (n, G_MAXINT32), &sec_start, &sec_end);
              g_assert_cmpint (sec_start, ==, n);
              g_assert_cmpint (sec_end, ==, G_MAXUINT);
            }
          sorter = NULL;
        }
    }

  g_object_unref (sort);
  g_object_unref (flatten);
}

static void
add_test_for_all_models (const char    *name,
                         GTestDataFunc  test_func)
{
  guint i;
  char *test;

  for (i = 0; i < N_MODELS; i++)
    {
      test = create_test_name (i);
      char *path = g_strdup_printf ("/sorterlistmodel/%s/%s", test, name);
      g_test_add_data_func (path, GUINT_TO_POINTER (i), test_func);
      g_free (path);
      g_free (test);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  add_test_for_all_models ("two-sorters", test_two_sorters);
  add_test_for_all_models ("stability", test_stability);
  add_test_for_all_models ("section-sorters", test_section_sorters);
  add_test_for_all_models ("sections", test_sections);

  return g_test_run ();
}
