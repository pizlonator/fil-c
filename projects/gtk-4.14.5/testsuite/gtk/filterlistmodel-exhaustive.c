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
      if (o1 != o2) \
        { \
          char *_s = g_strdup_printf ("Objects differ at index %u out of %u", _i, _n); \
         g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, _s); \
          g_free (_s); \
        } \
      g_object_unref (o1); \
      g_object_unref (o2); \
    } \
}G_STMT_END

#define assert_sections_equal(model1, model2) G_STMT_START{ \
  guint _i, _n, _start1, _end1, _start2, _end2; \
  g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (model1)), ==, g_list_model_get_n_items (G_LIST_MODEL (model2))); \
  _n = g_list_model_get_n_items (G_LIST_MODEL (model1)); \
  for (_i = 0; _i < _n; _i = _end1) \
    { \
      gtk_section_model_get_section (model1, _i, &_start1, &_end1); \
      gtk_section_model_get_section (model2, _i, &_start2, &_end2); \
      g_assert_cmpint (_start1, <, _end1); \
      g_assert_cmpint (_start2, <, _end2); \
      g_assert_cmpint (_start1, ==, _start2); \
      g_assert_cmpint (_end1, ==, _end2); \
      g_assert_cmpint (_i, ==, _start1); \
      g_assert_cmpint (_end1, <=, _n); \
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
      gpointer item = g_list_model_get_item (model, i);

      if (i > 0)
        g_string_append (string, ", ");
      g_string_append (string, gtk_string_object_get_string (item));
      g_object_unref (item);
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

static GtkFilterListModel *
filter_list_model_new (GListModel *source,
                       GtkFilter  *filter)
{
  GtkFilterListModel *model;
  GListStore *check;
  guint i;

  if (source)
    g_object_ref (source);
  if (filter)
    g_object_ref (filter);
  model = gtk_filter_list_model_new (source, filter);
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

static GtkFilterListModel *
create_filter_list_model (gconstpointer  model_id,
                          GListModel    *source,
                          GtkFilter     *filter)
{
  GtkFilterListModel *model;
  guint id = GPOINTER_TO_UINT (model_id);

  model = filter_list_model_new (id & 1 ? NULL : source, id & 2 ? NULL : filter);

  switch (id >> 2)
  {
    case 0:
      break;

    case 1:
      gtk_filter_list_model_set_incremental (model, TRUE);
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  if (id & 1)
    gtk_filter_list_model_set_model (model, source);
  if (id & 2)
    gtk_filter_list_model_set_filter (model, filter);

  return model;
}

static GListModel *
create_source_model (guint min_size, guint max_size)
{
  GtkStringList *list;
  guint i, size;

  size = g_test_rand_int_range (min_size, max_size + 1);
  list = gtk_string_list_new (NULL);

  for (i = 0; i < size; i++)
    gtk_string_list_append (list, g_test_rand_bit () ? "A" : "B");

  return G_LIST_MODEL (list);
}

#define N_FILTERS 5

static GtkFilter *
create_filter (gsize id)
{
  GtkFilter *filter;

  switch (id)
  {
    case 0:
      /* GTK_FILTER_MATCH_ALL */
      return GTK_FILTER (gtk_string_filter_new (NULL));

    case 1:
      /* GTK_FILTER_MATCH_NONE */
      filter = GTK_FILTER (gtk_string_filter_new (NULL));
      gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "does not matter, because no expression");
      return filter;

    case 2:
    case 3:
    case 4:
      /* match all As, Bs and nothing */
      filter = GTK_FILTER (gtk_string_filter_new (gtk_property_expression_new (GTK_TYPE_STRING_OBJECT, NULL, "string")));
      if (id == 2)
        gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "A");
      else if (id == 3)
        gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "B");
      else
        gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "does-not-match");
      return filter;

    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static GtkFilter *
create_random_filter (gboolean allow_null)
{
  guint n;

  if (allow_null)
    n = g_test_rand_int_range (0, N_FILTERS + 1);
  else
    n = g_test_rand_int_range (0, N_FILTERS);
  
  if (n >= N_FILTERS)
    return NULL;

  return create_filter (n);
}

static void
test_no_filter (gconstpointer model_id)
{
  GtkFilterListModel *model;
  GListModel *source;
  GtkFilter *filter;

  source = create_source_model (10, 10);
  model = create_filter_list_model (model_id, source, NULL);
  ensure_updated ();
  assert_model_equal (G_LIST_MODEL (model), source);

  filter = create_random_filter (FALSE);
  gtk_filter_list_model_set_filter (model, filter);
  g_object_unref (filter);
  gtk_filter_list_model_set_filter (model, NULL);
  ensure_updated ();
  assert_model_equal (G_LIST_MODEL (model), source);

  g_object_unref (model);
  g_object_unref (source);
}

/* Compare this:
 *   source => filter1 => filter2
 * with:
 *   source => multifilter(filter1, filter2)
 * and randomly change the source and filters and see if the
 * two continue agreeing.
 */
static void
test_two_filters (gconstpointer model_id)
{
  GtkFilterListModel *compare;
  GtkFilterListModel *model1, *model2;
  GListModel *source;
  GtkFilter *every, *filter;
  guint i, j, k;

  source = create_source_model (10, 10);
  model1 = create_filter_list_model (model_id, source, NULL);
  model2 = create_filter_list_model (model_id, G_LIST_MODEL (model1), NULL);
  every = GTK_FILTER (gtk_every_filter_new ());
  compare = create_filter_list_model (model_id, source, every);
  g_object_unref (every);
  g_object_unref (source);

  for (i = 0; i < N_FILTERS; i++)
    {
      filter = create_filter (i);
      gtk_filter_list_model_set_filter (model1, filter);
      gtk_multi_filter_append (GTK_MULTI_FILTER (every), filter);

      for (j = 0; j < N_FILTERS; j++)
        {
          filter = create_filter (i);
          gtk_filter_list_model_set_filter (model2, filter);
          gtk_multi_filter_append (GTK_MULTI_FILTER (every), filter);

          ensure_updated ();
          assert_model_equal (G_LIST_MODEL (model2), G_LIST_MODEL (compare));

          for (k = 0; k < 10; k++)
            {
              source = create_source_model (0, 1000);
              gtk_filter_list_model_set_model (compare, source);
              gtk_filter_list_model_set_model (model1, source);
              g_object_unref (source);

              ensure_updated ();
              assert_model_equal (G_LIST_MODEL (model2), G_LIST_MODEL (compare));
            }

          gtk_multi_filter_remove (GTK_MULTI_FILTER (every), 1);
        }

      gtk_multi_filter_remove (GTK_MULTI_FILTER (every), 0);
    }

  g_object_unref (compare);
  g_object_unref (model2);
  g_object_unref (model1);
}

/* Compare this:
 *   (source => filter) * => flatten
 * with:
 *   source * => flatten => filter
 * and randomly add/remove sources and change the filters and
 * see if the two agree.
 *
 * We use a multifilter for the top chain so that changing the filter
 * is easy.
 */
static void
test_model_changes (gconstpointer model_id)
{
  GListStore *store1, *store2;
  GtkFlattenListModel *flatten1, *flatten2;
  GtkFilterListModel *model2;
  GtkFilter *multi, *filter;
  gsize i;

  filter = create_random_filter (TRUE);
  multi = GTK_FILTER (gtk_every_filter_new ());
  if (filter)
    gtk_multi_filter_append (GTK_MULTI_FILTER (multi), filter);

  store1 = g_list_store_new (G_TYPE_OBJECT);
  store2 = g_list_store_new (G_TYPE_OBJECT);
  flatten1 = gtk_flatten_list_model_new (G_LIST_MODEL (store1));
  flatten2 = gtk_flatten_list_model_new (G_LIST_MODEL (store2));
  model2 = create_filter_list_model (model_id, G_LIST_MODEL (flatten2), filter);

  for (i = 0; i < 500; i++)
    {
      gboolean add = FALSE, remove = FALSE;
      guint position;

      switch (g_test_rand_int_range (0, 4))
      {
        case 0:
          /* change the filter */
          filter = create_random_filter (TRUE);
          gtk_multi_filter_remove (GTK_MULTI_FILTER (multi), 0); /* no-op if no filter */
          if (filter)
            gtk_multi_filter_append (GTK_MULTI_FILTER (multi), filter);
          gtk_filter_list_model_set_filter (model2, filter);
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

      position = g_test_rand_int_range (0, g_list_model_get_n_items (G_LIST_MODEL (store1)) + 1);
      if (g_list_model_get_n_items (G_LIST_MODEL (store1)) == position)
        remove = FALSE;

      if (add)
        {
          /* We want at least one element, otherwise the filters will see no changes */
          GListModel *source = create_source_model (1, 50);
          GtkFilterListModel *model1 = create_filter_list_model (model_id, source, multi);
          g_list_store_splice (store1,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &model1, 1);
          g_list_store_splice (store2,
                               position,
                               remove ? 1 : 0,
                               (gpointer *) &source, 1);
          g_object_unref (model1);
          g_object_unref (source);
        }
      else if (remove)
        {
          g_list_store_remove (store1, position);
          g_list_store_remove (store2, position);
        }

      if (g_test_rand_bit ())
        {
          ensure_updated ();
          assert_model_equal (G_LIST_MODEL (flatten1), G_LIST_MODEL (model2));
          assert_sections_equal (GTK_SECTION_MODEL (flatten1), GTK_SECTION_MODEL (model2));
        }
    }

  g_object_unref (model2);
  g_object_unref (flatten2);
  g_object_unref (flatten1);
  g_object_unref (multi);
}

static void
add_test_for_all_models (const char    *name,
                         GTestDataFunc  test_func)
{
  guint i;

  for (i = 0; i < N_MODELS; i++)
    {
      char *path = g_strdup_printf ("/filterlistmodel/model%u/%s", i, name);
      g_test_add_data_func (path, GUINT_TO_POINTER (i), test_func);
      g_free (path);
    }
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  add_test_for_all_models ("no-filter", test_no_filter);
  add_test_for_all_models ("two-filters", test_two_filters);
  add_test_for_all_models ("model-changes", test_model_changes);

  return g_test_run ();
}
