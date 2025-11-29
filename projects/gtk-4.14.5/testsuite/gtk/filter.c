/* Filter tests.
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
get_string (gpointer object)
{
  return g_strdup_printf ("%u", GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark)));
}

static void
append_digit (GString *s,
              guint    digit)
{
  static const char *names[10] = { NULL, "one", "two", "three", "four", "five", "six", "seven", "eight", "nine" };

  if (digit == 0)
    return;

  g_assert_cmpint (digit, <, 10);

  if (s->len)
    g_string_append_c (s, ' ');
  g_string_append (s, names[digit]);
}

static void
append_below_thousand (GString *s,
                       guint    n)
{
  if (n >= 100)
    {
      append_digit (s, n / 100);
      g_string_append (s, " hundred");
      n %= 100;
    }

  if (n >= 20)
    {
      const char *names[10] = { NULL, NULL, "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety" };
      if (s->len)
        g_string_append_c (s, ' ');
      g_string_append (s, names [n / 10]);
      n %= 10;
    }

  if (n >= 10)
    {
      const char *names[10] = { "ten", "eleven", "twelve", "thirteen", "fourteen",
                                "fifteen", "sixteen", "seventeen", "eighteen", "nineteen" };
      if (s->len)
        g_string_append_c (s, ' ');
      g_string_append (s, names [n - 10]);
    }
  else
    {
      append_digit (s, n);
    }
}

static char *
get_spelled_out (gpointer object)
{
  guint n = GPOINTER_TO_UINT (g_object_get_qdata (object, number_quark));
  GString *s;

  g_assert_cmpint (n, <, 1000000);

  if (n == 0)
    return g_strdup ("Zero");

  s = g_string_new (NULL);

  if (n >= 1000)
    {
      append_below_thousand (s, n / 1000);
      g_string_append (s, " thousand");
      n %= 1000;
    }

  append_below_thousand (s, n);

  /* Capitalize first letter so we can do case-sensitive matching */
  s->str[0] = g_ascii_toupper (s->str[0]);

  return g_string_free (s, FALSE);
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

static GtkFilterListModel *
new_model (guint      size,
           GtkFilter *filter)
{
  GtkFilterListModel *result;

  result = gtk_filter_list_model_new (g_object_ref (G_LIST_MODEL (new_store (1, size, 1))), g_object_ref (filter));

  return result;
}

static gboolean
divisible_by (gpointer item,
              gpointer data)
{
  return GPOINTER_TO_UINT (g_object_get_qdata (item, number_quark)) % GPOINTER_TO_UINT (data) == 0;
}

static void
test_simple (void)
{
  GtkFilterListModel *model;
  GtkFilter *filter;

  filter = GTK_FILTER (gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (3), NULL));
  model = new_model (20, filter);
  g_object_unref (filter);
  assert_model (model, "3 6 9 12 15 18");
  g_object_unref (model);
}

static void
test_any_simple (void)
{
  GtkFilterListModel *model;
  GtkFilter *any, *filter1, *filter2;
  gpointer item;

  any = GTK_FILTER (gtk_any_filter_new ());
  filter1 = GTK_FILTER (gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (3), NULL));
  filter2 = GTK_FILTER (gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (5), NULL));

  model = new_model (20, any);
  assert_model (model, "");

  gtk_multi_filter_append (GTK_MULTI_FILTER (any), filter1);
  assert_model (model, "3 6 9 12 15 18");

  gtk_multi_filter_append (GTK_MULTI_FILTER (any), filter2);
  assert_model (model, "3 5 6 9 10 12 15 18 20");

  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (any)) == GTK_TYPE_FILTER);
  g_assert_cmpuint (2, ==, g_list_model_get_n_items (G_LIST_MODEL (any)));
  item = g_list_model_get_item (G_LIST_MODEL (any), 1);
  g_assert_true (GTK_FILTER (item) == filter2);
  g_object_unref (item);

  gtk_multi_filter_remove (GTK_MULTI_FILTER (any), 0);
  assert_model (model, "5 10 15 20");

  /* doesn't exist */
  gtk_multi_filter_remove (GTK_MULTI_FILTER (any), 10);
  assert_model (model, "5 10 15 20");

  gtk_multi_filter_remove (GTK_MULTI_FILTER (any), 0);
  assert_model (model, "");

  g_object_unref (model);
  g_object_unref (any);
}

static void
test_string_simple (void)
{
  GtkFilterListModel *model;
  GtkFilter *filter;

  filter = GTK_FILTER (gtk_string_filter_new (
               gtk_cclosure_expression_new (G_TYPE_STRING,
                                            NULL,
                                            0, NULL,
                                            G_CALLBACK (get_string),
                                            NULL, NULL)));

  model = new_model (20, filter);
  assert_model (model, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20");

  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "1");
  assert_model (model, "1 10 11 12 13 14 15 16 17 18 19");

  g_object_unref (model);
  g_object_unref (filter);
}

static void
test_string_properties (void)
{
  GtkFilterListModel *model;
  GtkFilter *filter;
  GtkExpression *expr;

  expr = gtk_cclosure_expression_new (G_TYPE_STRING,
                                      NULL,
                                      0, NULL,
                                      G_CALLBACK (get_spelled_out),
                                      NULL, NULL);
  filter = GTK_FILTER (gtk_string_filter_new (expr));
  g_assert_true (expr == gtk_string_filter_get_expression (GTK_STRING_FILTER (filter)));

  model = new_model (1000, filter);
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "thirte");
  assert_model (model, "13 113 213 313 413 513 613 713 813 913");

  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "thirteen");
  assert_model (model, "13 113 213 313 413 513 613 713 813 913");

  gtk_string_filter_set_ignore_case (GTK_STRING_FILTER (filter), FALSE);
  assert_model (model, "113 213 313 413 513 613 713 813 913");

  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "Thirteen");
  assert_model (model, "13");

  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
  assert_model (model, "13");

  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_EXACT);
  assert_model (model, "13");

  gtk_string_filter_set_ignore_case (GTK_STRING_FILTER (filter), TRUE);
  assert_model (model, "13");

  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_PREFIX);
  assert_model (model, "13");

  gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), GTK_STRING_FILTER_MATCH_MODE_SUBSTRING);
  assert_model (model, "13 113 213 313 413 513 613 713 813 913");

  g_object_unref (model);
  g_object_unref (filter);
}

static void
test_bool_simple (void)
{
  GtkFilterListModel *model;
  GtkExpression *expr;
  GtkFilter *filter;

  filter = GTK_FILTER (gtk_bool_filter_new (
               gtk_cclosure_expression_new (G_TYPE_BOOLEAN,
                                            NULL,
                                            0, NULL,
                                            G_CALLBACK (divisible_by),
                                            GUINT_TO_POINTER (3), NULL)));
  model = new_model (20, filter);
  assert_model (model, "3 6 9 12 15 18");

  gtk_bool_filter_set_invert (GTK_BOOL_FILTER (filter), TRUE);
  g_assert_true (gtk_bool_filter_get_invert (GTK_BOOL_FILTER (filter)));
  assert_model (model, "1 2 4 5 7 8 10 11 13 14 16 17 19 20");

  gtk_bool_filter_set_invert (GTK_BOOL_FILTER (filter), FALSE);
  g_assert_false (gtk_bool_filter_get_invert (GTK_BOOL_FILTER (filter)));
  assert_model (model, "3 6 9 12 15 18");

  expr = gtk_cclosure_expression_new (G_TYPE_BOOLEAN,
                                      NULL,
                                      0, NULL,
                                      G_CALLBACK (divisible_by),
                                      GUINT_TO_POINTER (5), NULL);
  gtk_bool_filter_set_expression (GTK_BOOL_FILTER (filter), expr);
  g_assert_true (expr == gtk_bool_filter_get_expression (GTK_BOOL_FILTER (filter)));
  gtk_expression_unref (expr);
  assert_model (model, "5 10 15 20");

  gtk_bool_filter_set_invert (GTK_BOOL_FILTER (filter), TRUE);
  assert_model (model, "1 2 3 4 6 7 8 9 11 12 13 14 16 17 18 19");

  gtk_bool_filter_set_expression (GTK_BOOL_FILTER (filter), NULL);
  assert_model (model, "");

  gtk_bool_filter_set_invert (GTK_BOOL_FILTER (filter), FALSE);
  assert_model (model, "");

  g_object_unref (filter);
  g_object_unref (model);
}

static void
test_every_dispose (void)
{
  GtkFilter *filter, *filter1, *filter2;

  filter = GTK_FILTER (gtk_every_filter_new ());

  filter1 = GTK_FILTER (gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (3), NULL));
  filter2 = GTK_FILTER (gtk_custom_filter_new (divisible_by, GUINT_TO_POINTER (5), NULL));

  g_object_ref (filter1);
  g_object_ref (filter2);

  gtk_multi_filter_append (GTK_MULTI_FILTER (filter), filter1);
  gtk_multi_filter_append (GTK_MULTI_FILTER (filter), filter2);

  g_object_unref (filter);

  g_object_unref (filter1);
  g_object_unref (filter2);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  number_quark = g_quark_from_static_string ("Hell and fire was spawned to be released.");

  g_test_add_func ("/filter/simple", test_simple);
  g_test_add_func ("/filter/any/simple", test_any_simple);
  g_test_add_func ("/filter/string/simple", test_string_simple);
  g_test_add_func ("/filter/string/properties", test_string_properties);
  g_test_add_func ("/filter/bool/simple", test_bool_simple);
  g_test_add_func ("/filter/every/dispose", test_every_dispose);

  return g_test_run ();
}
