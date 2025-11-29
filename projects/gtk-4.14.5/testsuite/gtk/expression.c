/*
 * Copyright © 2019 Benjamin Otte
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include <locale.h>

#include <gtk/gtk.h>

static void
inc_counter (gpointer data)
{
  guint *counter = data;

  *counter += 1;
}

static void
test_property (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr;
  GtkExpressionWatch *watch;
  GtkStringFilter *filter;
  guint counter = 0;
  gboolean ret;

  filter = gtk_string_filter_new (NULL);
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");
  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);

  ret = gtk_expression_evaluate (expr, filter, &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, NULL);
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello World");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  ret = gtk_expression_evaluate (expr, filter , &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "Hello World");
  g_value_unset (&value);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 0);

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

static void
test_interface_property (void)
{
  GtkExpression *expr;

  expr = gtk_property_expression_new (GTK_TYPE_ORIENTABLE, NULL, "orientation");
  g_assert_cmpstr (gtk_property_expression_get_pspec (expr)->name, ==, "orientation");
  gtk_expression_unref (expr);
}

static char *
print_filter_info (GtkStringFilter         *filter,
                   const char              *search,
                   gboolean                 ignore_case,
                   GtkStringFilterMatchMode match_mode)
{
  g_assert_cmpstr (search, ==, gtk_string_filter_get_search (filter));
  g_assert_cmpint (ignore_case, ==, gtk_string_filter_get_ignore_case (filter));
  g_assert_cmpint (match_mode, ==, gtk_string_filter_get_match_mode (filter));

  return g_strdup ("OK");
}

static void
test_cclosure (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr, *pexpr[3];
  GtkExpressionWatch *watch;
  GtkStringFilter *filter;
  guint counter = 0;
  gboolean ret;

  filter = GTK_STRING_FILTER (gtk_string_filter_new (NULL));
  pexpr[0] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");
  pexpr[1] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "ignore-case");
  pexpr[2] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "match-mode");
  expr = gtk_cclosure_expression_new (G_TYPE_STRING,
                                      NULL,
                                      3,
                                      pexpr,
                                      G_CALLBACK (print_filter_info),
                                      NULL,
                                      NULL);
  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);

  ret = gtk_expression_evaluate (expr, filter, &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello World");
  g_assert_cmpint (counter, ==, 1);
  ret = gtk_expression_evaluate (expr, filter , &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_ignore_case (filter, FALSE);
  g_assert_cmpint (counter, ==, 2);
  ret = gtk_expression_evaluate (expr, filter , &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "Hello");
  gtk_string_filter_set_ignore_case (filter, TRUE);
  gtk_string_filter_set_match_mode (filter, GTK_STRING_FILTER_MATCH_MODE_EXACT);
  g_assert_cmpint (counter, ==, 5);
  ret = gtk_expression_evaluate (expr, filter , &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "OK");
  g_value_unset (&value);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 5);

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

static char *
make_string (void)
{
  return g_strdup ("Hello");
}

static void
test_closure (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr;
  GClosure *closure;
  gboolean ret;

  closure = g_cclosure_new (G_CALLBACK (make_string), NULL, NULL);
  expr = gtk_closure_expression_new (G_TYPE_STRING, closure, 0, NULL);
  ret = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (ret);
  g_assert_cmpstr (g_value_get_string (&value), ==, "Hello");
  g_value_unset (&value);

  gtk_expression_unref (expr);
}

static void
test_constant (void)
{
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  const GValue *v;
  gboolean res;

  expr = gtk_constant_expression_new (G_TYPE_INT, 22);
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, G_TYPE_INT);
  g_assert_true (gtk_expression_is_static (expr));

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpint (g_value_get_int (&value), ==, 22);

  v = gtk_constant_expression_get_value (expr);
  g_assert_cmpint (g_value_get_int (v), ==, 22);

  gtk_expression_unref (expr);
}

/* Test that object expressions fail to evaluate when
 * the object is gone.
 */
static void
test_object (void)
{
  GtkExpression *expr;
  GObject *obj;
  GValue value = G_VALUE_INIT;
  gboolean res;
  GObject *o;

  obj = G_OBJECT (gtk_string_filter_new (NULL));

  expr = gtk_object_expression_new (obj);
  g_assert_true (!gtk_expression_is_static (expr));
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, GTK_TYPE_STRING_FILTER);

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_true (g_value_get_object (&value) == obj);
  g_value_unset (&value);

  o = gtk_object_expression_get_object (expr);
  g_assert_true (o == obj);

  g_clear_object (&obj);
  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);

  gtk_expression_unref (expr);
}

/* Some basic tests that nested expressions work; in particular test
 * that watching works when things change deeper in the expression tree
 *
 * The setup we use is GtkFilterListModel -> GtkFilter -> "search" property,
 * which gives us an expression tree like
 *
 * GtkPropertyExpression "search"
 *    -> GtkPropertyExpression "filter"
 *         -> GtkObjectExpression listmodel
 *
 * We test setting both the search property and the filter property.
 */
static void
test_nested (void)
{
  GtkExpression *list_expr;
  GtkExpression *filter_expr;
  GtkExpression *expr;
  GtkStringFilter *filter;
  GListModel *list;
  GtkFilterListModel *filtered;
  GValue value = G_VALUE_INIT;
  gboolean res;
  GtkExpressionWatch *watch;
  guint counter = 0;

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "word");
  list = G_LIST_MODEL (g_list_store_new (G_TYPE_OBJECT));
  filtered = gtk_filter_list_model_new (list, g_object_ref (GTK_FILTER (filter)));

  list_expr = gtk_object_expression_new (G_OBJECT (filtered));
  filter_expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL, list_expr, "filter");
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, filter_expr, "search");

  g_assert_true (!gtk_expression_is_static (expr));
  g_assert_cmpint (gtk_expression_get_value_type (expr), ==, G_TYPE_STRING);

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  watch = gtk_expression_watch (expr, NULL, inc_counter, &counter, NULL);
  gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "salad");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "salad");
  g_value_unset (&value);

  gtk_filter_list_model_set_filter (filtered, GTK_FILTER (filter));
  g_assert_cmpint (counter, ==, 0);

  g_clear_object (&filter);
  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "salad");
  gtk_filter_list_model_set_filter (filtered, GTK_FILTER (filter));
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "salad");
  g_value_unset (&value);

  gtk_string_filter_set_search (filter, "bar");
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "bar");
  g_value_unset (&value);

  gtk_filter_list_model_set_filter (filtered, NULL);
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);

  gtk_expression_watch_unwatch (watch);
  g_assert_cmpint (counter, ==, 0);

  g_object_unref (filtered);
  gtk_expression_unref (expr);
}

/* This test uses the same setup as the last test, but
 * passes the filter as the "this" object when creating
 * the watch.
 *
 * So when we set a new filter and the old one gets desroyed,
 * the watch should invalidate itself because its this object
 * is gone.
 */
static void
test_nested_this_destroyed (void)
{
  GtkExpression *list_expr;
  GtkExpression *filter_expr;
  GtkExpression *expr;
  GtkStringFilter *filter;
  GListModel *list;
  GtkFilterListModel *filtered;
  GValue value = G_VALUE_INIT;
  gboolean res;
  GtkExpressionWatch *watch;
  guint counter = 0;

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "word");
  list = G_LIST_MODEL (g_list_store_new (G_TYPE_OBJECT));
  filtered = gtk_filter_list_model_new (list, g_object_ref (GTK_FILTER (filter)));

  list_expr = gtk_object_expression_new (G_OBJECT (filtered));
  filter_expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL, list_expr, "filter");
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, filter_expr, "search");

  watch = gtk_expression_watch (expr, filter, inc_counter, &counter, NULL);
  gtk_expression_watch_ref (watch);
  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  g_clear_object (&filter);
  g_assert_cmpint (counter, ==, 0);

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "salad");
  gtk_filter_list_model_set_filter (filtered, GTK_FILTER (filter));
  g_assert_cmpint (counter, ==, 1);
  counter = 0;

  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_false (res);

  gtk_string_filter_set_search (filter, "bar");
  g_assert_cmpint (counter, ==, 0);

  gtk_filter_list_model_set_filter (filtered, NULL);
  g_assert_cmpint (counter, ==, 0);

  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_false (res);
  g_assert_false (G_IS_VALUE (&value));

  /* We unwatch on purpose here to make sure it doesn't do bad things. */
  gtk_expression_watch_unwatch (watch);
  gtk_expression_watch_unref (watch);
  g_assert_cmpint (counter, ==, 0);

  g_object_unref (filtered);
  g_object_unref (filter);
  gtk_expression_unref (expr);
}

/* Test that property expressions fail to evaluate if the
 * expression evaluates to an object of the wrong type
 */
static void
test_type_mismatch (void)
{
  GtkFilter *filter;
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  gboolean res;

  filter = GTK_FILTER (gtk_any_filter_new ());

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_constant_expression_new (GTK_TYPE_ANY_FILTER, filter), "search");

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_false (res);
  g_assert_false (G_IS_VALUE (&value));

  gtk_expression_unref (expr);
  g_object_unref (filter);
}

/* Some basic tests around 'this' */
static void
test_this (void)
{
  GtkStringFilter *filter;
  GtkStringFilter *filter2;
  GtkExpression *expr;
  GValue value = G_VALUE_INIT;
  gboolean res;

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "word");

  filter2 = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter2, "sausage");

  res = gtk_expression_evaluate (expr, filter, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "word");
  g_value_unset (&value);

  res = gtk_expression_evaluate (expr, filter2, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "sausage");
  g_value_unset (&value);

  gtk_expression_unref (expr);
  g_object_unref (filter2);
  g_object_unref (filter);
}

/* Check that even for static expressions, watches can be created
 * and destroying the "this" argument does invalidate the
 * expression.
 */
static void
test_constant_watch_this_destroyed (void)
{
  GtkExpression *expr;
  GObject *this;
  guint counter = 0;

  this = g_object_new (G_TYPE_OBJECT, NULL);
  expr = gtk_constant_expression_new (G_TYPE_INT, 42);
  gtk_expression_watch (expr, this, inc_counter, &counter, NULL);
  g_assert_cmpint (counter, ==, 0);

  g_clear_object (&this);
  g_assert_cmpint (counter, ==, 1);

  gtk_expression_unref (expr);
}

/* Basic test of gtk_expression_bind */
static void
test_bind (void)
{
  GtkStringFilter *target;
  GtkStringFilter *source;
  GtkExpression *expr;
  GtkExpressionWatch *watch;
  GValue value = G_VALUE_INIT;
  gboolean res;

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, NULL, "search");

  target = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (target, "word");
  g_assert_cmpstr (gtk_string_filter_get_search (target), ==, "word");

  source = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (source, "sausage");

  watch = gtk_expression_bind (expr, target, "search", source);
  gtk_expression_watch_ref (watch);
  g_assert_cmpstr (gtk_string_filter_get_search (target), ==, "sausage");

  gtk_string_filter_set_search (source, "salad");
  g_assert_cmpstr (gtk_string_filter_get_search (target), ==, "salad");
  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "salad"); 
  g_value_unset (&value);

  g_object_unref (source);
  g_assert_cmpstr (gtk_string_filter_get_search (target), ==, "salad");
  res = gtk_expression_watch_evaluate (watch, &value);
  g_assert_false (res);
  g_assert_false (G_IS_VALUE (&value));

  g_object_unref (target);
  gtk_expression_watch_unref (watch);
}

/* Another test of bind, this time we watch ourselves */
static void
test_bind_self (void)
{
  GtkStringFilter *filter;
  GtkExpression *expr;

  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER,
                                      NULL,
                                      "ignore-case");

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "word");
  g_assert_cmpstr (gtk_string_filter_get_search (filter), ==, "word");

  gtk_expression_bind (expr, filter, "search", filter);
  g_assert_cmpstr (gtk_string_filter_get_search (filter), ==, "TRUE");

  g_object_unref (filter);
}

/* Test bind does the right memory management if the target's
 * dispose() kills the source */
static void
test_bind_child (void)
{
  GtkStringFilter *filter;
  GtkFilterListModel *child, *target;
  GtkExpression *expr;

  expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL,
                                      NULL,
                                      "filter");

  filter = gtk_string_filter_new (NULL);
  child = gtk_filter_list_model_new (NULL, GTK_FILTER (filter));
  target = gtk_filter_list_model_new (G_LIST_MODEL (child), NULL);

  gtk_expression_bind (expr, target, "filter", child);
  g_assert_true (gtk_filter_list_model_get_filter (child) == gtk_filter_list_model_get_filter (target));

  filter = gtk_string_filter_new (NULL);
  gtk_filter_list_model_set_filter (child, GTK_FILTER (filter));
  g_assert_true (GTK_FILTER (filter) == gtk_filter_list_model_get_filter (target));
  g_assert_true (gtk_filter_list_model_get_filter (child) == gtk_filter_list_model_get_filter (target));
  g_object_unref (filter);

  g_object_unref (target);
}

/* Another test of gtk_expression_bind that exercises the subwatch code paths */
static void
test_nested_bind (void)
{
  GtkStringFilter *filter;
  GtkStringFilter *filter2;
  GtkStringFilter *filter3;
  GListModel *list;
  GtkFilterListModel *filtered;
  GtkExpression *expr;
  GtkExpression *filter_expr;
  gboolean res;
  GValue value = G_VALUE_INIT;

  filter2 = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter2, "sausage");

  list = G_LIST_MODEL (g_list_store_new (G_TYPE_OBJECT));
  filtered = gtk_filter_list_model_new (list, g_object_ref (GTK_FILTER (filter2)));

  filter_expr = gtk_property_expression_new (GTK_TYPE_FILTER_LIST_MODEL,
                                             gtk_object_expression_new (G_OBJECT (filtered)),
                                             "filter");
  expr = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter_expr), "search");

  filter = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter, "word");
  g_assert_cmpstr (gtk_string_filter_get_search (filter), ==, "word");

  gtk_expression_bind (gtk_expression_ref (expr), filter, "search", NULL);

  gtk_string_filter_set_search (filter2, "sausage");
  g_assert_cmpstr (gtk_string_filter_get_search (filter), ==, "sausage");

  filter3 = gtk_string_filter_new (NULL);
  gtk_string_filter_set_search (filter3, "banana");
  gtk_filter_list_model_set_filter (filtered, GTK_FILTER (filter3));

  /* check that the expressions evaluate correctly */
  res = gtk_expression_evaluate (filter_expr, NULL, &value);
  g_assert_true (res);
  g_assert_true (g_value_get_object (&value) == filter3);
  g_value_unset (&value);

  res = gtk_expression_evaluate (expr, NULL, &value);
  g_assert_true (res);
  g_assert_cmpstr (g_value_get_string (&value), ==, "banana");
  g_value_unset (&value);

  /* and the bind too */
  g_assert_cmpstr (gtk_string_filter_get_search (filter), ==, "banana");

  g_object_unref (filter);
  g_object_unref (filter2);
  g_object_unref (filter3);
  g_object_unref (filtered);

  gtk_expression_unref (expr);
  gtk_expression_unref (filter_expr);
}

static char *
some_cb (gpointer    this,
         const char *search,
         gboolean    ignore_case,
         gpointer    data)
{
  if (!search)
    return NULL;

  if (ignore_case)
    return g_utf8_strdown (search, -1);
  else
    return g_strdup (search);
}

/* Test that things work as expected when the same object is used multiple times in an
 * expression or its subexpressions.
 */
static void
test_double_bind (void)
{
  GtkStringFilter *filter1;
  GtkStringFilter *filter2;
  GtkExpression *expr;
  GtkExpression *filter_expr;
  GtkExpression *params[2];

  filter1 = gtk_string_filter_new (NULL);
  filter2 = gtk_string_filter_new (NULL);

  filter_expr = gtk_object_expression_new (G_OBJECT (filter1));

  params[0] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter_expr), "search");
  params[1] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter_expr), "ignore-case");
  expr = gtk_cclosure_expression_new (G_TYPE_STRING,
                                      NULL,
                                      2, params,
                                      (GCallback)some_cb,
                                      NULL, NULL);

  gtk_expression_bind (gtk_expression_ref (expr), filter2, "search", NULL);

  gtk_string_filter_set_search (filter1, "Banana");
  g_assert_cmpstr (gtk_string_filter_get_search (filter2), ==, "banana");

  gtk_string_filter_set_ignore_case (filter1, FALSE);
  g_assert_cmpstr (gtk_string_filter_get_search (filter2), ==, "Banana");

  gtk_expression_unref (expr);
  gtk_expression_unref (filter_expr);

  g_object_unref (filter1);
  g_object_unref (filter2);
}

/* Test that having multiple binds on the same object works. */
static void
test_binds (void)
{
  GtkStringFilter *filter1;
  GtkStringFilter *filter2;
  GtkStringFilter *filter3;
  GtkExpression *expr;
  GtkExpression *expr2;
  GtkExpression *filter1_expr;
  GtkExpression *filter2_expr;
  GtkExpression *params[2];

  filter1 = gtk_string_filter_new (NULL);
  filter2 = gtk_string_filter_new (NULL);
  filter3 = gtk_string_filter_new (NULL);

  filter1_expr = gtk_object_expression_new (G_OBJECT (filter1));
  filter2_expr = gtk_object_expression_new (G_OBJECT (filter2));

  params[0] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter1_expr), "search");
  params[1] = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter2_expr), "ignore-case");
  expr = gtk_cclosure_expression_new (G_TYPE_STRING,
                                      NULL,
                                      2, params,
                                      (GCallback)some_cb,
                                      NULL, NULL);

  expr2 = gtk_property_expression_new (GTK_TYPE_STRING_FILTER, gtk_expression_ref (filter2_expr), "ignore-case");

  g_assert_true (gtk_property_expression_get_expression (expr2) == filter2_expr);
  g_assert_cmpstr (gtk_property_expression_get_pspec (expr2)->name, ==, "ignore-case");

  gtk_expression_bind (gtk_expression_ref (expr), filter3, "search", NULL);
  gtk_expression_bind (gtk_expression_ref (expr2), filter3, "ignore-case", NULL);

  gtk_string_filter_set_search (filter1, "Banana");
  g_assert_cmpstr (gtk_string_filter_get_search (filter3), ==, "banana");
  g_assert_true (gtk_string_filter_get_ignore_case (filter3));

  gtk_string_filter_set_ignore_case (filter2, FALSE);
  g_assert_cmpstr (gtk_string_filter_get_search (filter3), ==, "Banana");
  g_assert_false (gtk_string_filter_get_ignore_case (filter3));

  /* invalidate the first bind */
  g_object_unref (filter1);

  gtk_string_filter_set_ignore_case (filter2, TRUE);
  g_assert_cmpstr (gtk_string_filter_get_search (filter3), ==, "Banana");
  g_assert_true (gtk_string_filter_get_ignore_case (filter3));

  gtk_expression_unref (expr);
  gtk_expression_unref (expr2);
  gtk_expression_unref (filter1_expr);
  gtk_expression_unref (filter2_expr);

  g_object_unref (filter2);
  g_object_unref (filter3);
}

/* test that binds work ok with object expressions */
static void
test_bind_object (void)
{
  GtkStringFilter *filter;
  GListStore *store;
  GtkFilterListModel *model;
  GtkExpression *expr;

  filter = gtk_string_filter_new (NULL);
  store = g_list_store_new (G_TYPE_OBJECT);
  model = gtk_filter_list_model_new (G_LIST_MODEL (store), NULL);

  expr = gtk_object_expression_new (G_OBJECT (filter));

  gtk_expression_bind (gtk_expression_ref (expr), model, "filter", NULL);

  g_assert_true (gtk_filter_list_model_get_filter (model) == GTK_FILTER (filter));

  g_object_unref (filter);

  g_assert_true (gtk_filter_list_model_get_filter (model) == GTK_FILTER (filter));

  gtk_expression_unref (expr);
  g_object_unref (model);
}

static void
test_value (void)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expr;

  expr = gtk_constant_expression_new (G_TYPE_INT, 22);

  g_value_init (&value, GTK_TYPE_EXPRESSION);
  gtk_value_take_expression (&value, expr);
  g_assert_true (G_VALUE_TYPE (&value) == GTK_TYPE_EXPRESSION);

  expr = gtk_value_dup_expression (&value);
  gtk_expression_unref (expr);

  expr = gtk_constant_expression_new (G_TYPE_INT, 23);
  gtk_value_set_expression (&value, expr);
  gtk_expression_unref (expr);

  g_value_unset (&value);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");

  g_test_add_func ("/expression/property", test_property);
  g_test_add_func ("/expression/interface-property", test_interface_property);
  g_test_add_func ("/expression/cclosure", test_cclosure);
  g_test_add_func ("/expression/closure", test_closure);
  g_test_add_func ("/expression/constant", test_constant);
  g_test_add_func ("/expression/constant-watch-this-destroyed", test_constant_watch_this_destroyed);
  g_test_add_func ("/expression/object", test_object);
  g_test_add_func ("/expression/nested", test_nested);
  g_test_add_func ("/expression/nested-this-destroyed", test_nested_this_destroyed);
  g_test_add_func ("/expression/type-mismatch", test_type_mismatch);
  g_test_add_func ("/expression/this", test_this);
  g_test_add_func ("/expression/bind", test_bind);
  g_test_add_func ("/expression/bind-self", test_bind_self);
  g_test_add_func ("/expression/bind-child", test_bind_child);
  g_test_add_func ("/expression/nested-bind", test_nested_bind);
  g_test_add_func ("/expression/double-bind", test_double_bind);
  g_test_add_func ("/expression/binds", test_binds);
  g_test_add_func ("/expression/bind-object", test_bind_object);
  g_test_add_func ("/expression/value", test_value);

  return g_test_run ();
}
