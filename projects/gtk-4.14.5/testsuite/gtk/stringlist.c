/* GtkStringList tests
 *
 * Copyright (C) 2020, Red Hat, Inc.
 * Authors: Matthias Clasen
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

#include <gtk/gtk.h>

static GQuark changes_quark;

static char *
model_to_string (GListModel *model)
{
  GtkStringList *self = GTK_STRING_LIST (model);
  GString *string = g_string_new (NULL);
  guint i;

  for (i = 0; i < g_list_model_get_n_items (model); i++)
    {
      if (i > 0)
        g_string_append (string, " ");
      g_string_append_printf (string, "%s", gtk_string_list_get_string (self, i));
    }

  return g_string_free (string, FALSE);
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
free_changes (gpointer data)
{
  GString *changes = data;

  /* all changes must have been checked via assert_changes() before */
  g_assert_cmpstr (changes->str, ==, "");

  g_string_free (changes, TRUE);
}

static GtkStringList *
new_model (const char **strings)
{
  GtkStringList *result;
  GString *changes;

  result = gtk_string_list_new (strings);
  changes = g_string_new ("");
  g_object_set_qdata_full (G_OBJECT(result), changes_quark, changes, free_changes);
  g_signal_connect (result, "items-changed", G_CALLBACK (items_changed), changes);

  return result;
}

static void
test_string_object (void)
{
  GtkStringObject *so;

  so = gtk_string_object_new ("Hello");
  g_assert_cmpstr (gtk_string_object_get_string (so), ==, "Hello");
  g_object_unref (so);
}

static void
test_create_empty (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ NULL });

  g_assert_true (g_type_is_a (g_list_model_get_item_type (G_LIST_MODEL (list)), G_TYPE_OBJECT));

  assert_model (list, "");
  assert_changes (list, "");

  g_object_unref (list);
}

static void
test_create_strv (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ "a", "b", "c", NULL });

  assert_model (list, "a b c");
  assert_changes (list, "");

  g_object_unref (list);
}

static void
test_create_builder (void)
{
  const char *ui =
"<interface>"
"  <object class=\"GtkStringList\" id=\"list\">"
"    <items>"
"      <item translatable=\"yes\" context=\"ctx\" comments=\"none\">a</item>"
"      <item>b</item>"
"      <item>c</item>"
"    </items>"
"  </object>"
"</interface>";
  GtkBuilder *builder;
  GtkStringList *list;

  builder = gtk_builder_new_from_string (ui, -1);
  list = GTK_STRING_LIST (gtk_builder_get_object (builder, "list"));
  assert_model (list, "a b c");

  g_object_unref (builder);
}

static void
test_create_builder2 (void)
{
  const char *ui =
"<interface>"
"  <object class=\"GtkStringList\" id=\"list\">"
"    <property name=\"strings\">a\nb\nc</property>"
"  </object>"
"</interface>";
  GtkBuilder *builder;
  GtkStringList *list;

  builder = gtk_builder_new_from_string (ui, -1);
  list = GTK_STRING_LIST (gtk_builder_get_object (builder, "list"));
  assert_model (list, "a b c");

  g_object_unref (builder);
}

static void
test_get_string (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ "a", "b", "c", NULL });

  assert_model (list, "a b c");

  g_assert_cmpstr (gtk_string_list_get_string (list, 0), ==, "a");
  g_assert_cmpstr (gtk_string_list_get_string (list, 1), ==, "b");
  g_assert_cmpstr (gtk_string_list_get_string (list, 2), ==, "c");
  g_assert_null (gtk_string_list_get_string (list, 3));

  g_object_unref (list);
}

static void
test_splice (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ "a", "b", "c", "d", "e", NULL });

  assert_model (list, "a b c d e");

  gtk_string_list_splice (list, 2, 2, (const char *[]){ "x", "y", "z", NULL });

  assert_model (list, "a b x y z e");
  assert_changes (list, "2-2+3");

  g_object_unref (list);
}

static void
test_add_remove (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ "a", "b", "c", "d", "e", NULL });

  assert_model (list, "a b c d e");

  gtk_string_list_remove (list, 2);

  assert_model (list, "a b d e");
  assert_changes (list, "-2");

  gtk_string_list_append (list, "x");

  assert_model (list, "a b d e x");
  assert_changes (list, "+4");

  g_object_unref (list);
}

static void
test_take (void)
{
  GtkStringList *list;

  list = new_model ((const char *[]){ NULL });

  assert_model (list, "");

  gtk_string_list_take (list, g_strdup_printf ("%d dollars", (int)1e6));
  assert_model (list, "1000000 dollars");
  assert_changes (list, "+0");

  g_object_unref (list);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/stringobject/basic", test_string_object);
  g_test_add_func ("/stringlist/create/empty", test_create_empty);
  g_test_add_func ("/stringlist/create/strv", test_create_strv);
  g_test_add_func ("/stringlist/create/builder", test_create_builder);
  g_test_add_func ("/stringlist/create/builder2", test_create_builder2);
  g_test_add_func ("/stringlist/get_string", test_get_string);
  g_test_add_func ("/stringlist/splice", test_splice);
  g_test_add_func ("/stringlist/add_remove", test_add_remove);
  g_test_add_func ("/stringlist/take", test_take);

  return g_test_run ();
}
