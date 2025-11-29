/* buildertest.c
 * Copyright (C) 2006-2007 Async Open Source
 * Authors: Johan Dahlin
 *          Henrique Romano
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#ifdef G_OS_WIN32
# define _BUILDER_TEST_EXPORT __declspec(dllexport)
#else
# define _BUILDER_TEST_EXPORT __attribute__((visibility("default")))
#endif

/* exported for GtkBuilder */
_BUILDER_TEST_EXPORT void signal_normal (GtkWindow *window, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_after (GtkWindow *window, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_object (GtkButton *button, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_object_after (GtkButton *button, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_first (GtkButton *button, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_second (GtkButton *button, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_extra (GtkButton *button, GParamSpec *spec);
_BUILDER_TEST_EXPORT void signal_extra2 (GtkButton *button, GParamSpec *spec);


static GtkBuilder *
builder_new_from_string (const char *buffer,
                         gsize length,
                         const char *domain)
{
  GtkBuilder *builder;
  GError *error = NULL;

  builder = gtk_builder_new ();
  if (domain)
    gtk_builder_set_translation_domain (builder, domain);
  gtk_builder_add_from_string (builder, buffer, length, &error);
  if (error)
    {
      g_print ("ERROR: %s", error->message);
      g_error_free (error);
    }

  return builder;
}

static void
test_parser (void)
{
  GtkBuilder *builder;
  GError *error;

  builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_string (builder, "<xxx/>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_UNHANDLED_TAG);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface invalid=\"X\"/>", -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><child/></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_TAG);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkBox\" id=\"a\"><object class=\"GtkBox\" id=\"b\"/></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_TAG);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"Unknown\" id=\"a\"></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkWidget\" id=\"a\" constructor=\"none\"></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><child internal-child=\"foobar\"><object class=\"GtkButton\" id=\"int\"/></child></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"></object><object class=\"GtkButton\" id=\"a\"/></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_DUPLICATE_ID);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><property name=\"deafbeef\"></property></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_PROPERTY);
  g_error_free (error);

  error = NULL;
  gtk_builder_add_from_string (builder, "<interface><object class=\"GtkButton\" id=\"a\"><signal name=\"deafbeef\" handler=\"gtk_true\"/></object></interface>", -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_SIGNAL);
  g_error_free (error);

  g_object_unref (builder);
}

static int normal = 0;
static int after = 0;
static int object = 0;
static int object_after = 0;

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_normal (GtkWindow *window, GParamSpec *spec)
{
  g_assert_true (GTK_IS_WINDOW (window));
  g_assert_true (normal == 0);
  g_assert_true (after == 0);

  normal++;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_after (GtkWindow *window, GParamSpec *spec)
{
  g_assert_true (GTK_IS_WINDOW (window));
  g_assert_true (normal == 1);
  g_assert_true (after == 0);

  after++;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_object (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (GTK_IS_BUTTON (button));
  g_assert_true (object == 0);
  g_assert_true (object_after == 0);

  object++;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_object_after (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (GTK_IS_BUTTON (button));
  g_assert_true (object == 1);
  g_assert_true (object_after == 0);

  object_after++;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_first (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (normal == 0);
  normal = 10;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_second (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (normal == 10);
  normal = 20;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_extra (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (normal == 20);
  normal = 30;
}

_BUILDER_TEST_EXPORT
void /* exported for GtkBuilder */
signal_extra2 (GtkButton *button, GParamSpec *spec)
{
  g_assert_true (normal == 30);
  normal = 40;
}

static void
test_connect_signals (void)
{
  GtkBuilder *builder;
  GObject *window;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_after\" after=\"yes\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object\""
    "            object=\"button\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_object_after\""
    "            object=\"button\" after=\"yes\"/>"
    "  </object>"
    "</interface>";
  const char buffer_order[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <signal name=\"notify::title\" handler=\"signal_first\"/>"
    "    <signal name=\"notify::title\" handler=\"signal_second\"/>"
    "  </object>"
    "</interface>";
  const char buffer_extra[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window2\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra\"/>"
    "  </object>"
    "</interface>";
  const char buffer_extra2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window3\">"
    "    <signal name=\"notify::title\" handler=\"signal_extra2\"/>"
    "  </object>"
    "</interface>";
  const char buffer_after_child[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\"/>"
    "    </child>"
    "    <signal name=\"notify::title\" handler=\"signal_normal\"/>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_set_title (GTK_WINDOW (window), "test");

  g_assert_cmpint (normal, ==, 1);
  g_assert_cmpint (after, ==, 1);
  g_assert_cmpint (object, ==, 1);
  g_assert_cmpint (object_after, ==, 1);

  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer_order, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  normal = 0;
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert_true (normal == 20);

  gtk_window_destroy (GTK_WINDOW (window));

  gtk_builder_add_from_string (builder, buffer_extra,
			       strlen (buffer_extra), NULL);
  gtk_builder_add_from_string (builder, buffer_extra2,
			       strlen (buffer_extra2), NULL);
  window = gtk_builder_get_object (builder, "window2");
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert_true (normal == 30);

  gtk_window_destroy (GTK_WINDOW (window));
  window = gtk_builder_get_object (builder, "window3");
  gtk_window_set_title (GTK_WINDOW (window), "test");
  g_assert_true (normal == 40);
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);

  /* new test, reset globals */
  after = 0;
  normal = 0;

  builder = builder_new_from_string (buffer_after_child, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  gtk_window_set_title (GTK_WINDOW (window), "test");

  g_assert_true (normal == 1);
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);
}

static void
test_domain (void)
{
  GtkBuilder *builder;
  const char buffer1[] = "<interface/>";
  const char buffer2[] = "<interface domain=\"domain\"/>";
  const char *domain;

  builder = builder_new_from_string (buffer1, -1, NULL);
  domain = gtk_builder_get_translation_domain (builder);
  g_assert_true (domain == NULL);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer1, -1, "domain-1");
  domain = gtk_builder_get_translation_domain (builder);
  g_assert_nonnull (domain);
  g_assert_true (strcmp (domain, "domain-1") == 0);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  domain = gtk_builder_get_translation_domain (builder);
  g_assert_null (domain);
  g_object_unref (builder);
}

#if 0
static void
test_translation (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label\">"
    "        <property name=\"label\" translatable=\"yes\">File</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GtkLabel *window, *label;

  setlocale (LC_ALL, "sv_SE");
  textdomain ("builder");
  bindtextdomain ("builder", "tests");

  builder = builder_new_from_string (buffer, -1, NULL);
  label = GTK_LABEL (gtk_builder_get_object (builder, "label"));
  g_assert_true (strcmp (gtk_label_get_text (label), "Arkiv") == 0);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);
}
#endif

static void
test_sizegroup (void)
{
  GtkBuilder * builder;
  const char buffer1[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">horizontal</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"vbox1\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">horizontal</property>"
    "    <widgets>"
    "    </widgets>"
    "   </object>"
    "</interface>";
  const char buffer3[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup1\">"
    "    <property name=\"mode\">horizontal</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup2\">"
    "    <property name=\"mode\">horizontal</property>"
    "    <widgets>"
    "      <widget name=\"radio1\"/>"
    "      <widget name=\"radio2\"/>"
    "    </widgets>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"vbox1\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"radio1\"/>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"radio2\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *sizegroup;
  GSList *widgets;

  builder = builder_new_from_string (buffer1, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_assert_cmpint (g_slist_length (widgets), ==, 2);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_assert_cmpint (g_slist_length (widgets), ==, 0);
  g_slist_free (widgets);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  sizegroup = gtk_builder_get_object (builder, "sizegroup1");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_assert_cmpint (g_slist_length (widgets), ==, 2);
  g_slist_free (widgets);
  sizegroup = gtk_builder_get_object (builder, "sizegroup2");
  widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (sizegroup));
  g_assert_cmpint (g_slist_length (widgets), ==, 2);
  g_slist_free (widgets);

#if 0
  {
    GObject *window;
    window = gtk_builder_get_object (builder, "window1");
    gtk_window_destroy (GTK_WINDOW (window));
  }
#endif
  g_object_unref (builder);
}

static void
test_list_store (void)
{
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">Johan</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"2\">50</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  const char buffer3[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">50</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"0\">Johan</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">19</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  GtkTreeIter iter;
  char *surname, *lastname;
  int age;

  builder = builder_new_from_string (buffer1, -1, NULL);
  store = gtk_builder_get_object (builder, "liststore1");
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 2);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_UINT);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  store = gtk_builder_get_object (builder, "liststore1");
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 3);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2), ==, G_TYPE_INT);

  g_assert_cmpint (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter), ==, TRUE);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "John");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Doe");
  g_free (lastname);
  g_assert_cmpint (age, ==, 25);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "Johan");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Dole");
  g_free (lastname);
  g_assert_cmpint (age, ==, 50);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  store = gtk_builder_get_object (builder, "liststore1");
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 3);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2), ==, G_TYPE_INT);

  g_assert_true (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "John");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Doe");
  g_free (lastname);
  g_assert_cmpint (age, ==, 25);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "Johan");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Dole");
  g_free (lastname);
  g_assert_cmpint (age, ==, 50);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_null (surname);
  g_assert_null (lastname);
  g_assert_cmpint (age, ==, 19);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  g_object_unref (builder);
}

static void
test_tree_store (void)
{
  const char buffer[] =
    "<interface domain=\"test\">"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"guint\"/>"
    "    </columns>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">Johan</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"2\">50</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  const char buffer3[] =
    "<interface>"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">50</col>"
    "        <col id=\"1\">Dole</col>"
    "        <col id=\"0\">Johan</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">19</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  const char buffer4[] =
    "<interface>"
    "  <object class=\"GtkTreeStore\" id=\"treestore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"gint\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"1\" context=\"foo\">Doe</col>"
    "        <col id=\"0\" translatable=\"yes\">John</col>"
    "        <col id=\"2\" comments=\"foobar\">25</col>"
    "        <row>"
    "          <col id=\"2\">50</col>"
    "          <col id=\"1\">Dole</col>"
    "          <col id=\"0\">Johan</col>"
    "        </row>"
    "      </row>"
    "      <row>"
    "        <col id=\"2\">19</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>";
  GtkBuilder *builder;
  GObject *store;
  GtkTreeIter iter, parent;
  char *surname, *lastname;
  int age;

  builder = builder_new_from_string (buffer, -1, NULL);
  store = gtk_builder_get_object (builder, "treestore1");
  g_assert_true (GTK_IS_TREE_STORE (store));
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 2);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_UINT);
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  store = gtk_builder_get_object (builder, "treestore1");
  g_assert_true (GTK_IS_TREE_STORE (store));
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 3);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2), ==, G_TYPE_INT);

  g_assert_cmpint (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter), ==, TRUE);
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "John");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Doe");
  g_free (lastname);
  g_assert_cmpint (age, ==, 25);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "Johan");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Dole");
  g_free (lastname);
  g_assert_cmpint (age, ==, 50);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  store = gtk_builder_get_object (builder, "treestore1");
  g_assert_true (GTK_IS_TREE_STORE (store));
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 3);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2), ==, G_TYPE_INT);

  g_assert_true (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "John");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Doe");
  g_free (lastname);
  g_assert_cmpint (age, ==, 25);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "Johan");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Dole");
  g_free (lastname);
  g_assert_cmpint (age, ==, 50);
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_null (surname);
  g_assert_null (lastname);
  g_assert_cmpint (age, ==, 19);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  g_object_unref (builder);

  builder = builder_new_from_string (buffer4, -1, NULL);
  store = gtk_builder_get_object (builder, "treestore1");
  g_assert_true (GTK_IS_TREE_STORE (store));
  g_assert_cmpint (gtk_tree_model_get_n_columns (GTK_TREE_MODEL (store)), ==, 3);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 0), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 1), ==, G_TYPE_STRING);
  g_assert_cmpint (gtk_tree_model_get_column_type (GTK_TREE_MODEL (store), 2), ==, G_TYPE_INT);

  g_assert_true (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "John");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Doe");
  g_free (lastname);
  g_assert_cmpint (age, ==, 25);
  parent = iter;
  g_assert_true (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &iter, &parent));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_cmpstr (surname, ==, "Johan");
  g_free (surname);
  g_assert_cmpstr (lastname, ==, "Dole");
  g_free (lastname);
  g_assert_cmpint (age, ==, 50);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));
  iter = parent;
  g_assert_true (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
                      0, &surname,
                      1, &lastname,
                      2, &age,
                      -1);
  g_assert_null (surname);
  g_assert_null (lastname);
  g_assert_cmpint (age, ==, 19);
  g_assert_false (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter));

  g_object_unref (builder);
}

static void
test_types (void)
{
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkBox\" id=\"box\"/>"
    "  <object class=\"GtkButton\" id=\"button\"/>"
    "  <object class=\"GtkCheckButton\" id=\"checkbutton\"/>"
    "  <object class=\"GtkDialog\" id=\"dialog\"/>"
    "  <object class=\"GtkDrawingArea\" id=\"drawingarea\"/>"
    "  <object class=\"GtkEntry\" id=\"entry\"/>"
    "  <object class=\"GtkFontButton\" id=\"fontbutton\"/>"
    "  <object class=\"GtkImage\" id=\"image\"/>"
    "  <object class=\"GtkLabel\" id=\"label\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore\"/>"
    "  <object class=\"GtkNotebook\" id=\"notebook\"/>"
    "  <object class=\"GtkProgressBar\" id=\"progressbar\"/>"
    "  <object class=\"GtkSizeGroup\" id=\"sizegroup\"/>"
    "  <object class=\"GtkScrolledWindow\" id=\"scrolledwindow\"/>"
    "  <object class=\"GtkSpinButton\" id=\"spinbutton\"/>"
    "  <object class=\"GtkStatusbar\" id=\"statusbar\"/>"
    "  <object class=\"GtkTextView\" id=\"textview\"/>"
    "  <object class=\"GtkToggleButton\" id=\"togglebutton\"/>"
    "  <object class=\"GtkTreeStore\" id=\"treestore\"/>"
    "  <object class=\"GtkTreeView\" id=\"treeview\"/>"
    "  <object class=\"GtkViewport\" id=\"viewport\"/>"
    "  <object class=\"GtkWindow\" id=\"window\"/>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" type-func=\"gtk_window_get_type\" id=\"window\"/>"
    "</interface>";
  const char buffer3[] =
    "<interface>"
    "  <object class=\"XXXInvalidType\" type-func=\"gtk_window_get_type\" id=\"window\"/>"
    "</interface>";
  const char buffer4[] =
    "<interface>"
    "  <object class=\"GtkWindow\" type-func=\"xxx_invalid_get_type_function\" id=\"window\"/>"
    "</interface>";
  const char buffer5[] =
    "<interface>"
    "  <object type-func=\"gtk_window_get_type\" id=\"window\"/>"
    "</interface>";
  GtkBuilder *builder;
  GObject *window;
  GError *error;

  builder = builder_new_from_string (buffer, -1, NULL);
  gtk_window_destroy (GTK_WINDOW (gtk_builder_get_object (builder, "dialog")));
  gtk_window_destroy (GTK_WINDOW (gtk_builder_get_object (builder, "window")));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window = gtk_builder_get_object (builder, "window");
  g_assert_true (GTK_IS_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);
  window = gtk_builder_get_object (builder, "window");
  g_assert_true (GTK_IS_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);

  error = NULL;
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer4, -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_TYPE_FUNCTION);
  g_error_free (error);
  g_object_unref (builder);

  error = NULL;
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer5, -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_MISSING_ATTRIBUTE);
  g_error_free (error);
  g_object_unref (builder);
}

static void
test_spin_button (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "<object class=\"GtkAdjustment\" id=\"adjustment1\">"
    "<property name=\"lower\">0</property>"
    "<property name=\"upper\">10</property>"
    "<property name=\"step-increment\">2</property>"
    "<property name=\"page-increment\">3</property>"
    "<property name=\"page-size\">0</property>"
    "<property name=\"value\">1</property>"
    "</object>"
    "<object class=\"GtkSpinButton\" id=\"spinbutton1\">"
    "<property name=\"visible\">True</property>"
    "<property name=\"adjustment\">adjustment1</property>"
    "</object>"
    "</interface>";
  GObject *obj;
  GtkAdjustment *adjustment;
  double value;

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = gtk_builder_get_object (builder, "spinbutton1");
  g_assert_true (GTK_IS_SPIN_BUTTON (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (obj));
  g_assert_true (GTK_IS_ADJUSTMENT (adjustment));
  g_object_get (adjustment, "value", &value, NULL);
  g_assert_cmpint (value, ==, 1);
  g_object_get (adjustment, "lower", &value, NULL);
  g_assert_cmpint (value, ==, 0);
  g_object_get (adjustment, "upper", &value, NULL);
  g_assert_cmpint (value, ==, 10);
  g_object_get (adjustment, "step-increment", &value, NULL);
  g_assert_cmpint (value, ==, 2);
  g_object_get (adjustment, "page-increment", &value, NULL);
  g_assert_cmpint (value, ==, 3);
  g_object_get (adjustment, "page-size", &value, NULL);
  g_assert_cmpint (value, ==, 0);

  g_object_unref (builder);
}

static void
test_notebook (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkNotebook\" id=\"notebook1\">"
    "    <child>"
    "      <object class=\"GtkNotebookPage\">"
    "        <property name=\"child\">"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"label\">label1</property>"
    "          </object>"
    "        </property>"
    "        <property name=\"tab\">"
    "          <object class=\"GtkLabel\" id=\"tablabel1\">"
    "           <property name=\"label\">tab_label1</property>"
    "          </object>"
    "        </property>"
    "      </object>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkNotebookPage\">"
    "        <property name=\"child\">"
    "          <object class=\"GtkLabel\" id=\"label2\">"
    "            <property name=\"label\">label2</property>"
    "          </object>"
    "        </property>"
    "        <property name=\"tab\">"
    "          <object class=\"GtkLabel\" id=\"tablabel2\">"
    "            <property name=\"label\">tab_label2</property>"
    "          </object>"
    "        </property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *notebook;
  GtkWidget *label;

  builder = builder_new_from_string (buffer, -1, NULL);
  notebook = gtk_builder_get_object (builder, "notebook1");
  g_assert_nonnull (notebook);
  g_assert_cmpint (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)), ==, 2);

  label = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0);
  g_assert_true (GTK_IS_LABEL (label));
  g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, "label1");
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), label);
  g_assert_true (GTK_IS_LABEL (label));
  g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, "tab_label1");

  label = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 1);
  g_assert_true (GTK_IS_LABEL (label));
  g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, "label2");
  label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (notebook), label);
  g_assert_true (GTK_IS_LABEL (label));
  g_assert_cmpstr (gtk_label_get_label (GTK_LABEL (label)), ==, "tab_label2");

  g_object_unref (builder);
}

static void
test_construct_only_property (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <property name=\"css-name\">amazing</property>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkTextTagTable\" id=\"tagtable1\"/>"
    "  <object class=\"GtkTextBuffer\" id=\"textbuffer1\">"
    "    <property name=\"tag-table\">tagtable1</property>"
    "  </object>"
    "</interface>";
  GObject *widget, *tagtable, *textbuffer;

  builder = builder_new_from_string (buffer, -1, NULL);
  widget = gtk_builder_get_object (builder, "window1");
  g_assert_cmpstr (gtk_widget_get_css_name (GTK_WIDGET (widget)), ==, "amazing");

  gtk_window_destroy (GTK_WINDOW (widget));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  textbuffer = gtk_builder_get_object (builder, "textbuffer1");
  g_assert_nonnull (textbuffer);
  g_object_get (textbuffer, "tag-table", &tagtable, NULL);
  g_assert_true (tagtable == gtk_builder_get_object (builder, "tagtable1"));
  g_object_unref (tagtable);
  g_object_unref (builder);
}

static void
test_object_properties (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"vbox\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"mnemonic-widget\">spinbutton1</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkSpinButton\" id=\"spinbutton1\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window2\"/>"
    "</interface>";
  GObject *label, *spinbutton, *window;

  builder = builder_new_from_string (buffer, -1, NULL);
  label = gtk_builder_get_object (builder, "label1");
  g_assert_nonnull (label);
  spinbutton = gtk_builder_get_object (builder, "spinbutton1");
  g_assert_nonnull (spinbutton);
  g_assert_true (spinbutton == (GObject*)gtk_label_get_mnemonic_widget (GTK_LABEL (label)));

  gtk_builder_add_from_string (builder, buffer2, -1, NULL);
  window = gtk_builder_get_object (builder, "window2");
  g_assert_nonnull (window);
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);
}

static void
test_children (void)
{
  GtkBuilder * builder;
  GtkWidget *content_area;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "        <property name=\"label\">Hello</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <property name=\"use_header_bar\">1</property>"
    "    <child internal-child=\"content_area\">"
    "      <object class=\"GtkBox\" id=\"dialog1-vbox\">"
    "        <property name=\"orientation\">vertical</property>"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkBox\" id=\"dialog1-action_area\">"
    "              <property name=\"orientation\">horizontal</property>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *window, *button;
  GObject *dialog, *vbox, *action_area;
  GtkWidget *child;
  int count;

  builder = builder_new_from_string (buffer1, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  g_assert_true (GTK_IS_WINDOW (window));

  button = gtk_builder_get_object (builder, "button1");
  g_assert_true (GTK_IS_BUTTON (button));
  g_assert_nonnull (gtk_widget_get_parent (GTK_WIDGET(button)));
  g_assert_cmpstr (gtk_buildable_get_buildable_id (GTK_BUILDABLE (gtk_widget_get_parent (GTK_WIDGET (button)))), ==, "window1");

  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  dialog = gtk_builder_get_object (builder, "dialog1");
  g_assert_true (GTK_IS_DIALOG (dialog));
  count = 0;
  for (child = gtk_widget_get_first_child (GTK_WIDGET (dialog));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    count++;
  g_assert_cmpint (count, ==, 2);

  vbox = gtk_builder_get_object (builder, "dialog1-vbox");
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  g_assert_true (GTK_IS_BOX (vbox));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (vbox)), ==, GTK_ORIENTATION_VERTICAL);
  g_assert_cmpstr (gtk_buildable_get_buildable_id (GTK_BUILDABLE (content_area)), ==, "dialog1-vbox");

  action_area = gtk_builder_get_object (builder, "dialog1-action_area");
  g_assert_true (GTK_IS_BOX (action_area));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (action_area)), ==, GTK_ORIENTATION_HORIZONTAL);
  g_assert_nonnull (gtk_widget_get_parent (GTK_WIDGET (action_area)));
  g_assert_nonnull (gtk_buildable_get_buildable_id (GTK_BUILDABLE (action_area)));
  gtk_window_destroy (GTK_WINDOW (dialog));
  g_object_unref (builder);
}

static void
test_layout_properties (void)
{
  GtkBuilder * builder;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkGrid\" id=\"grid1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\">"
    "        <layout>"
    "          <property name=\"column\">1</property>"
    "        </layout>"
    "      </object>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\">"
    "        <layout>"
    "          <property name=\"column\">0</property>"
    "        </layout>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *label, *vbox;

  builder = builder_new_from_string (buffer1, -1, NULL);
  vbox = gtk_builder_get_object (builder, "grid1");
  g_assert_true (GTK_IS_GRID (vbox));

  label = gtk_builder_get_object (builder, "label1");
  g_assert_true (GTK_IS_LABEL (label));

  label = gtk_builder_get_object (builder, "label2");
  g_assert_true (GTK_IS_LABEL (label));

  g_object_unref (builder);
}

static void
test_treeview_column (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "<object class=\"GtkListStore\" id=\"liststore1\">"
    "  <columns>"
    "    <column type=\"gchararray\"/>"
    "    <column type=\"guint\"/>"
    "  </columns>"
    "  <data>"
    "    <row>"
    "      <col id=\"0\">John</col>"
    "      <col id=\"1\">25</col>"
    "    </row>"
    "  </data>"
    "</object>"
    "<object class=\"GtkWindow\" id=\"window1\">"
    "  <child>"
    "    <object class=\"GtkTreeView\" id=\"treeview1\">"
    "      <property name=\"visible\">True</property>"
    "      <property name=\"model\">liststore1</property>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column1\">"
    "          <property name=\"title\">Test</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "      <child>"
    "        <object class=\"GtkTreeViewColumn\" id=\"column2\">"
    "          <property name=\"title\">Number</property>"
    "          <child>"
    "            <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "          </child>"
    "        </object>"
    "      </child>"
    "    </object>"
    "  </child>"
    "</object>"
    "</interface>";
  GObject *window, *treeview;
  GtkTreeViewColumn *column;
  GList *renderers;
  GObject *renderer;

  builder = builder_new_from_string (buffer, -1, NULL);
  treeview = gtk_builder_get_object (builder, "treeview1");
  g_assert_true (GTK_IS_TREE_VIEW (treeview));
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (treeview), 0);
  g_assert_true (GTK_IS_TREE_VIEW_COLUMN (column));
  g_assert_cmpstr (gtk_tree_view_column_get_title (column), ==, "Test");

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
  g_assert_cmpint (g_list_length (renderers), ==, 1);
  renderer = g_list_nth_data (renderers, 0);
  g_assert_true (GTK_IS_CELL_RENDERER_TEXT (renderer));
  g_list_free (renderers);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);
}

static void
test_icon_view (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "      <column type=\"GdkPixbuf\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkIconView\" id=\"iconview1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"text-column\">0</property>"
    "        <property name=\"pixbuf-column\">1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *iconview;

  builder = builder_new_from_string (buffer, -1, NULL);
  iconview = gtk_builder_get_object (builder, "iconview1");
  g_assert_true (GTK_IS_ICON_VIEW (iconview));

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);
}

static void
test_combo_box (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBox\" id=\"combobox1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">1</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = gtk_builder_get_object (builder, "combobox1");
  g_assert_nonnull (combobox);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);
}

#if 0
static void
test_combo_box_entry (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"guint\"/>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">1</col>"
    "        <col id=\"1\">Foo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\">2</col>"
    "        <col id=\"1\">Bar</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkComboBox\" id=\"comboboxentry1\">"
    "        <property name=\"model\">liststore1</property>"
    "        <property name=\"has-entry\">True</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">0</attribute>"
    "            </attributes>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer2\"/>"
    "            <attributes>"
    "              <attribute name=\"text\">1</attribute>"
    "            </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *combobox, *renderer;
  char *text;

  builder = builder_new_from_string (buffer, -1, NULL);
  combobox = gtk_builder_get_object (builder, "comboboxentry1");
  g_assert_nonnull (combobox);

  renderer = gtk_builder_get_object (builder, "renderer2");
  g_assert_nonnull (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert_cmpstr (text, ==, "Bar");
  g_free (text);

  renderer = gtk_builder_get_object (builder, "renderer1");
  g_assert_nonnull (renderer);
  g_object_get (renderer, "text", &text, NULL);
  g_assert_cmpstr (text, ==, "2");
  g_free (text);

  window = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);
}
#endif

static void
test_cell_view (void)
{
  GtkBuilder *builder;
  const char *buffer =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\">test</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkCellView\" id=\"cellview1\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"model\">liststore1</property>"
    "        <child>"
    "          <object class=\"GtkCellRendererText\" id=\"renderer1\"/>"
    "          <attributes>"
    "            <attribute name=\"text\">0</attribute>"
    "          </attributes>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *cellview;
  GObject *model, *window;
  GtkTreePath *path;
  GList *renderers;

  builder = builder_new_from_string (buffer, -1, NULL);
  cellview = gtk_builder_get_object (builder, "cellview1");
  g_assert_nonnull (builder);
  g_assert_true (GTK_IS_CELL_VIEW (cellview));
  g_object_get (cellview, "model", &model, NULL);
  g_assert_true (GTK_IS_TREE_MODEL (model));
  g_object_unref (model);
  path = gtk_tree_path_new_first ();
  gtk_cell_view_set_displayed_row (GTK_CELL_VIEW (cellview), path);
  gtk_tree_path_free (path);

  renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (cellview));
  g_assert_cmpint (g_list_length (renderers), ==, 1);
  g_list_free (renderers);

  window = gtk_builder_get_object (builder, "window1");
  g_assert_nonnull (window);
  gtk_window_destroy (GTK_WINDOW (window));

  g_object_unref (builder);
}

static void
test_dialog (void)
{
  GtkBuilder * builder;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkDialog\" id=\"dialog1\">"
    "    <child internal-child=\"content_area\">"
    "      <object class=\"GtkBox\" id=\"dialog1-vbox\">"
    "        <property name=\"orientation\">vertical</property>"
    "          <child internal-child=\"action_area\">"
    "            <object class=\"GtkBox\" id=\"dialog1-action_area\">"
    "              <property name=\"orientation\">horizontal</property>"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_cancel\"/>"
    "              </child>"
    "              <child>"
    "                <object class=\"GtkButton\" id=\"button_ok\"/>"
    "              </child>"
    "            </object>"
    "          </child>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"3\">button_ok</action-widget>"
    "      <action-widget response=\"-5\">button_cancel</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  GObject *dialog1;
  GObject *button_ok;
  GObject *button_cancel;

  builder = builder_new_from_string (buffer1, -1, NULL);
  dialog1 = gtk_builder_get_object (builder, "dialog1");
  button_ok = gtk_builder_get_object (builder, "button_ok");
  g_assert_cmpint (gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog1), GTK_WIDGET (button_ok)), ==, 3);
  button_cancel = gtk_builder_get_object (builder, "button_cancel");
  g_assert_cmpint (gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog1), GTK_WIDGET (button_cancel)), ==, -5);

  gtk_window_destroy (GTK_WINDOW (dialog1));
  g_object_unref (builder);
}

static void
test_message_dialog (void)
{
  GtkBuilder * builder;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkMessageDialog\" id=\"dialog1\">"
    "    <child internal-child=\"message_area\">"
    "      <object class=\"GtkBox\" id=\"dialog-message-area\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkExpander\" id=\"expander\"/>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *dialog1;
  GObject *expander;

  builder = builder_new_from_string (buffer1, -1, NULL);
  dialog1 = gtk_builder_get_object (builder, "dialog1");
  expander = gtk_builder_get_object (builder, "expander");
  g_assert_true (GTK_IS_EXPANDER (expander));
  g_assert_true (gtk_widget_get_parent (GTK_WIDGET (expander)) == gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog1)));

  gtk_window_destroy (GTK_WINDOW (dialog1));
  g_object_unref (builder);
}

static void
test_widget (void)
{
  const char *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <property name=\"focus-widget\">button1</property>"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  const char *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkButton\" id=\"button1\">"
    "         <property name=\"receives-default\">True</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  const char *buffer3 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"vbox1\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button1\">"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char *buffer4 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\">"
    "         <property name=\"label\">Thelabel</property>"
    "      </object>"
    "    </child>"
    "  </object>"
   "</interface>";
  GtkBuilder *builder;
  GObject *window1, *button1, *label1;

  builder = builder_new_from_string (buffer, -1, NULL);
  button1 = gtk_builder_get_object (builder, "button1");

  g_assert_true (gtk_widget_is_focus (GTK_WIDGET (button1)));
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window1));

  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  button1 = gtk_builder_get_object (builder, "button1");

  g_assert_true (gtk_widget_get_receives_default (GTK_WIDGET (button1)));

  g_object_unref (builder);

  builder = builder_new_from_string (buffer3, -1, NULL);

  window1 = gtk_builder_get_object (builder, "window1");
  label1 = gtk_builder_get_object (builder, "label1");
  g_assert_true (GTK_IS_LABEL (label1));

  gtk_window_destroy (GTK_WINDOW (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer4, -1, NULL);
  label1 = gtk_builder_get_object (builder, "label1");
  g_assert_true (GTK_IS_LABEL (label1));

  g_object_unref (builder);
}

static void
test_window (void)
{
  const char *buffer1 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "     <property name=\"title\"></property>"
    "  </object>"
   "</interface>";
  const char *buffer2 =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "  </object>"
   "</interface>";
  GtkBuilder *builder;
  GObject *window1;
  char *title;

  builder = builder_new_from_string (buffer1, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  g_object_get (window1, "title", &title, NULL);
  g_assert_cmpstr (title, ==, "");
  g_free (title);
  gtk_window_destroy (GTK_WINDOW (window1));
  g_object_unref (builder);

  builder = builder_new_from_string (buffer2, -1, NULL);
  window1 = gtk_builder_get_object (builder, "window1");
  gtk_window_destroy (GTK_WINDOW (window1));
  g_object_unref (builder);
}

static void
test_value_from_string (void)
{
  GValue value = G_VALUE_INIT;
  GError *error = NULL;
  GtkBuilder *builder;

  builder = gtk_builder_new ();

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_STRING, "test", &value, &error));
  g_assert_true (G_VALUE_HOLDS_STRING (&value));
  g_assert_cmpstr (g_value_get_string (&value), ==, "test");
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "true", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_true (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "false", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_false (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yes", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_true (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "no", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_false (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "0", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_false (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "1", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_true (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "tRuE", &value, &error));
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (&value));
  g_assert_true (g_value_get_boolean (&value));
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "blaurgh", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "yess", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "trueee", &value, &error) == FALSE);
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_false (gtk_builder_value_from_string_type (builder, G_TYPE_BOOLEAN, "", &value, &error));
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_INT, "12345", &value, &error));
  g_assert_true (G_VALUE_HOLDS_INT (&value));
  g_assert_cmpint (g_value_get_int (&value), ==, 12345);
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_LONG, "9912345", &value, &error));
  g_assert_true (G_VALUE_HOLDS_LONG (&value));
  g_assert_cmpint (g_value_get_long (&value), ==, 9912345);
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_UINT, "2345", &value, &error));
  g_assert_true (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpint (g_value_get_uint (&value), ==, 2345);
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_INT64, "-2345", &value, &error));
  g_assert_true (G_VALUE_HOLDS_INT64 (&value));
  g_assert_cmpint (g_value_get_int64 (&value), ==, -2345);
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_UINT64, "2345", &value, &error));
  g_assert_true (G_VALUE_HOLDS_UINT64 (&value));
  g_assert_cmpint (g_value_get_uint64 (&value), ==, 2345);
  g_value_unset (&value);

  g_assert_true (gtk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "1.454", &value, &error));
  g_assert_true (G_VALUE_HOLDS_FLOAT (&value));
  g_assert_cmpfloat (fabs (g_value_get_float (&value) - 1.454), <, 0.00001);
  g_value_unset (&value);

  g_assert_false (gtk_builder_value_from_string_type (builder, G_TYPE_FLOAT, "abc", &value, &error));
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_false (gtk_builder_value_from_string_type (builder, G_TYPE_INT, "/-+,abc", &value, &error));
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_true (gtk_builder_value_from_string_type (builder, GTK_TYPE_TEXT_DIRECTION, "rtl", &value, &error));
  g_assert_true (G_VALUE_HOLDS_ENUM (&value));
  g_assert_cmpint (g_value_get_enum (&value), ==, GTK_TEXT_DIR_RTL);
  g_value_unset (&value);

  g_assert_false (gtk_builder_value_from_string_type (builder, GTK_TYPE_TEXT_DIRECTION, "sliff", &value, &error));
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_assert_false (gtk_builder_value_from_string_type (builder, GTK_TYPE_TEXT_DIRECTION, "foobar", &value, &error));
  g_value_unset (&value);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE);
  g_error_free (error);
  error = NULL;

  g_object_unref (builder);
}

static gboolean model_freed = FALSE;

static void
model_weakref (gpointer data,
               GObject *model)
{
  model_freed = TRUE;
}

static void
test_reference_counting (void)
{
  GtkBuilder *builder;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkListStore\" id=\"liststore1\"/>"
    "  <object class=\"GtkListStore\" id=\"liststore2\"/>"
    "  <object class=\"GtkWindow\" id=\"window1\">"
    "    <child>"
    "      <object class=\"GtkTreeView\" id=\"treeview1\">"
    "        <property name=\"model\">liststore1</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkBox\" id=\"vbox1\">"
    "        <property name=\"orientation\">vertical</property>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\"/>"
    "    </child>"
    "  </object>"
    "</interface>";
  GObject *window, *treeview, *model;

  builder = builder_new_from_string (buffer1, -1, NULL);
  window = gtk_builder_get_object (builder, "window1");
  treeview = gtk_builder_get_object (builder, "treeview1");
  model = gtk_builder_get_object (builder, "liststore1");
  g_object_unref (builder);

  g_object_weak_ref (model, (GWeakNotify)model_weakref, NULL);

  g_assert_false (model_freed);
  gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), NULL);
  g_assert_true (model_freed);

  gtk_window_destroy (GTK_WINDOW (window));

  builder = builder_new_from_string (buffer2, -1, NULL);
  g_object_unref (builder);
}

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef struct {
  gboolean weight;
  gboolean foreground;
  gboolean underline;
  gboolean size;
  gboolean font_desc;
  gboolean language;
} FoundAttrs;

static gboolean
filter_pango_attrs (PangoAttribute *attr,
		    gpointer        data)
{
  FoundAttrs *found = (FoundAttrs *)data;

  if (attr->klass->type == PANGO_ATTR_WEIGHT)
    found->weight = TRUE;
  else if (attr->klass->type == PANGO_ATTR_FOREGROUND)
    found->foreground = TRUE;
  else if (attr->klass->type == PANGO_ATTR_UNDERLINE)
    found->underline = TRUE;
  /* Make sure optional start/end properties are working */
  else if (attr->klass->type == PANGO_ATTR_SIZE &&
	   attr->start_index == 5 &&
	   attr->end_index   == 10)
    found->size = TRUE;
  else if (attr->klass->type == PANGO_ATTR_FONT_DESC)
    found->font_desc = TRUE;
  else if (attr->klass->type == PANGO_ATTR_LANGUAGE)
    found->language = TRUE;

  return TRUE;
}

static void
test_pango_attributes (void)
{
  GtkBuilder *builder;
  FoundAttrs found = { 0, };
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\" value=\"PANGO_WEIGHT_BOLD\"/>"
    "      <attribute name=\"foreground\" value=\"DarkSlateGray\"/>"
    "      <attribute name=\"underline\" value=\"True\"/>"
    "      <attribute name=\"size\" value=\"4\" start=\"5\" end=\"10\"/>"
    "      <attribute name=\"font-desc\" value=\"Sans Italic 22\"/>"
    "      <attribute name=\"language\" value=\"pt_BR\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";
  const char err_buffer1[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";
  const char err_buffer2[] =
    "<interface>"
    "  <object class=\"GtkLabel\" id=\"label1\">"
    "    <attributes>"
    "      <attribute name=\"weight\" value=\"PANGO_WEIGHT_BOLD\" unrecognized=\"True\"/>"
    "    </attributes>"
    "  </object>"
    "</interface>";

  GObject *label;
  GError  *error = NULL;
  PangoAttrList *attrs, *filtered;

  /* Test attributes are set */
  builder = builder_new_from_string (buffer, -1, NULL);
  label = gtk_builder_get_object (builder, "label1");
  g_assert_nonnull (label);

  attrs = gtk_label_get_attributes (GTK_LABEL (label));
  g_assert_nonnull (attrs);

  filtered = pango_attr_list_filter (attrs, filter_pango_attrs, &found);
  g_assert_true (filtered);
  pango_attr_list_unref (filtered);

  g_assert_true (found.weight);
  g_assert_true (found.foreground);
  g_assert_true (found.underline);
  g_assert_true (found.size);
  g_assert_true (found.language);
  g_assert_true (found.font_desc);

  g_object_unref (builder);

  /* Test errors are set */
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, err_buffer1, -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE);
  g_object_unref (builder);
  g_error_free (error);
  error = NULL;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, err_buffer2, -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE);
  g_object_unref (builder);
  g_error_free (error);
}

static void
test_requires (void)
{
  GtkBuilder *builder;
  GError     *error = NULL;
  char       *buffer;
  const char buffer_fmt[] =
    "<interface>"
    "  <requires lib=\"gtk\" version=\"%d.%d\"/>"
    "</interface>";

  buffer = g_strdup_printf (buffer_fmt, GTK_MAJOR_VERSION, GTK_MINOR_VERSION + 1);
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_VERSION_MISMATCH);
  g_object_unref (builder);
  g_error_free (error);
  g_free (buffer);
}

static void
test_add_objects (void)
{
  GtkBuilder *builder;
  GError *error;
  int ret;
  GObject *obj;
  const char *objects[2] = {"mainbox", NULL};
  const char *objects2[3] = {"mainbox", "window2", NULL};
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"mainbox\">"
    "        <property name=\"orientation\">vertical</property>"
    "        <property name=\"visible\">True</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"no\">first label</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label2\">"
    "            <property name=\"visible\">True</property>"
    "            <property name=\"label\" translatable=\"no\">second label</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "  <object class=\"GtkWindow\" id=\"window2\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label3\">"
    "        <property name=\"label\" translatable=\"no\">second label</property>"
    "      </object>"
    "    </child>"
    "  </object>"
    "<interface/>";

  error = NULL;
  builder = gtk_builder_new ();
  ret = gtk_builder_add_objects_from_string (builder, buffer, -1, objects, &error);
  g_assert_no_error (error);
  obj = gtk_builder_get_object (builder, "window");
  g_assert_null (obj);
  obj = gtk_builder_get_object (builder, "window2");
  g_assert_null (obj);
  obj = gtk_builder_get_object (builder, "mainbox");
  g_assert_true (GTK_IS_WIDGET (obj));
  g_object_unref (builder);

  error = NULL;
  builder = gtk_builder_new ();
  ret = gtk_builder_add_objects_from_string (builder, buffer, -1, objects2, &error);
  g_assert_true (ret);
  g_assert_null (error);
  obj = gtk_builder_get_object (builder, "window");
  g_assert_null (obj);
  obj = gtk_builder_get_object (builder, "window2");
  g_assert_true (GTK_IS_WINDOW (obj));
  gtk_window_destroy (GTK_WINDOW (obj));
  obj = gtk_builder_get_object (builder, "mainbox");
  g_assert_true (GTK_IS_WIDGET (obj));
  g_object_unref (builder);
}

static void
test_message_area (void)
{
  GtkBuilder *builder;
  GObject *obj, *obj1;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkInfoBar\" id=\"infobar1\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"contentarea1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"content\">"
    "            <property name=\"label\" translatable=\"yes\">Message</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <child type=\"action\">"
    "      <object class=\"GtkButton\" id=\"button_ok\">"
    "        <property name=\"label\">gtk-ok</property>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"1\">button_ok</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = gtk_builder_get_object (builder, "infobar1");
  g_assert_true (GTK_IS_INFO_BAR (obj));
  obj1 = gtk_builder_get_object (builder, "content");
  g_assert_true (GTK_IS_LABEL (obj1));

  obj1 = gtk_builder_get_object (builder, "button_ok");
  g_assert_true (GTK_IS_BUTTON (obj1));

  g_object_unref (builder);
}

static void
test_gmenu (void)
{
  GtkBuilder *builder;
  GObject *obj, *obj1;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "  </object>"
    "  <menu id='edit-menu'>"
    "    <section>"
    "      <item>"
    "        <attribute name='label'>Undo</attribute>"
    "        <attribute name='action'>undo</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Redo</attribute>"
    "        <attribute name='action'>redo</attribute>"
    "      </item>"
    "    </section>"
    "    <section></section>"
    "    <section>"
    "      <attribute name='label'>Copy &amp; Paste</attribute>"
    "      <item>"
    "        <attribute name='label'>Cut</attribute>"
    "        <attribute name='action'>cut</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Copy</attribute>"
    "        <attribute name='action'>copy</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label'>Paste</attribute>"
    "        <attribute name='action'>paste</attribute>"
    "      </item>"
    "    </section>"
    "    <item><link name='section' id='blargh'>"
    "      <item>"
    "        <attribute name='label'>Bold</attribute>"
    "        <attribute name='action'>bold</attribute>"
    "      </item>"
    "      <submenu>"
    "        <attribute name='label'>Language</attribute>"
    "        <item>"
    "          <attribute name='label'>Latin</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'latin'</attribute>"
    "        </item>"
    "        <item>"
    "          <attribute name='label'>Greek</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'greek'</attribute>"
    "        </item>"
    "        <item>"
    "          <attribute name='label'>Urdu</attribute>"
    "          <attribute name='action'>lang</attribute>"
    "          <attribute name='target'>'urdu'</attribute>"
    "        </item>"
    "      </submenu>"
    "    </link></item>"
    "  </menu>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = gtk_builder_get_object (builder, "window");
  g_assert_true (GTK_IS_WINDOW (obj));
  obj1 = gtk_builder_get_object (builder, "edit-menu");
  g_assert_true (G_IS_MENU_MODEL (obj1));
  obj1 = gtk_builder_get_object (builder, "blargh");
  g_assert_true (G_IS_MENU_MODEL (obj1));
  g_object_unref (builder);
}

static void
test_level_bar (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *obj, *obj1;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "        <property name=\"value\">4.70</property>"
    "        <property name=\"min-value\">2</property>"
    "        <property name=\"max-value\">5</property>"
    "        <offsets>"
    "          <offset name=\"low\" value=\"2.25\"/>"
    "          <offset name=\"custom\" value=\"3\"/>"
    "          <offset name=\"high\" value=\"3\"/>"
    "        </offsets>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";
  const char buffer2[] =
    "<interface>"
    "  <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "    <offsets>"
    "      <offset name=\"low\" bogus_attr=\"foo\"/>"
    "    </offsets>"
    "  </object>"
    "</interface>";
  const char buffer3[] =
    "<interface>"
    "  <object class=\"GtkLevelBar\" id=\"levelbar\">"
    "    <offsets>"
    "      <offset name=\"low\" value=\"1\"/>"
    "    </offsets>"
    "    <bogus_tag>"
    "    </bogus_tag>"
    "  </object>"
    "</interface>";

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer1, -1, &error);
  g_assert_true (error == NULL);

  obj = gtk_builder_get_object (builder, "window");
  g_assert_true (GTK_IS_WINDOW (obj));
  obj1 = gtk_builder_get_object (builder, "levelbar");
  g_assert_true (GTK_IS_LEVEL_BAR (obj1));
  g_object_unref (builder);

  error = NULL;
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer2, -1, &error);
  g_assert_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE);
  g_error_free (error);
  g_object_unref (builder);

  error = NULL;
  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer3, -1, &error);
  g_assert_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_UNHANDLED_TAG);
  g_error_free (error);
  g_object_unref (builder);
}

static void
test_expose_object (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GtkWidget *menu;
  GObject *obj;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkMenuButton\" id=\"button\">"
    "    <property name=\"popover\">external_menu</property>"
    "  </object>"
    "</interface>";

  menu = gtk_popover_new ();
  builder = gtk_builder_new ();
  gtk_builder_expose_object (builder, "external_menu", G_OBJECT (menu));
  gtk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert_no_error (error);

  obj = gtk_builder_get_object (builder, "button");
  g_assert_true (GTK_IS_MENU_BUTTON (obj));

  g_assert_true (gtk_menu_button_get_popover (GTK_MENU_BUTTON (obj)) == GTK_POPOVER (menu));

  g_object_unref (builder);
}

static void
test_no_ids (void)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *obj;
  const char buffer[] =
    "<interface>"
    "  <object class=\"GtkInfoBar\">"
    "    <child>"
    "      <object class=\"GtkBox\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <child>"
    "          <object class=\"GtkLabel\">"
    "            <property name=\"label\" translatable=\"yes\">Message</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "    <child type=\"action\">"
    "      <object class=\"GtkButton\" id=\"button_ok\">"
    "        <property name=\"label\">gtk-ok</property>"
    "      </object>"
    "    </child>"
    "    <action-widgets>"
    "      <action-widget response=\"1\">button_ok</action-widget>"
    "    </action-widgets>"
    "  </object>"
    "</interface>";

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, buffer, -1, &error);
  g_assert_null (error);

  obj = gtk_builder_get_object (builder, "button_ok");
  g_assert_true (GTK_IS_BUTTON (obj));

  g_object_unref (builder);
}

static void
test_property_bindings (void)
{
  const char *buffer =
    "<interface>"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"vbox\">"
    "        <property name=\"visible\">True</property>"
    "        <property name=\"orientation\">vertical</property>"
    "        <child>"
    "          <object class=\"GtkCheckButton\" id=\"checkbutton\">"
    "            <property name=\"active\">false</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button\">"
    "            <property name=\"sensitive\" bind-source=\"checkbutton\" bind-property=\"active\" bind-flags=\"sync-create\">false</property>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button2\">"
    "            <property name=\"sensitive\" bind-source=\"checkbutton\" bind-property=\"active\" />"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkButton\" id=\"button3\">"
    "            <property name=\"sensitive\" bind-source=\"button\" bind-flags=\"sync-create\" />"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GtkBuilder *builder;
  GObject *checkbutton, *button, *button2, *button3, *window;

  builder = builder_new_from_string (buffer, -1, NULL);

  checkbutton = gtk_builder_get_object (builder, "checkbutton");
  g_assert_true (GTK_IS_CHECK_BUTTON (checkbutton));
  g_assert_true (!gtk_check_button_get_active (GTK_CHECK_BUTTON (checkbutton)));

  button = gtk_builder_get_object (builder, "button");
  g_assert_true (GTK_IS_BUTTON (button));
  g_assert_false (gtk_widget_get_sensitive (GTK_WIDGET (button)));

  button2 = gtk_builder_get_object (builder, "button2");
  g_assert_true (GTK_IS_BUTTON (button2));
  g_assert_true (gtk_widget_get_sensitive (GTK_WIDGET (button2)));

  button3 = gtk_builder_get_object (builder, "button3");
  g_assert_true (GTK_IS_BUTTON (button3));
  g_assert_false (gtk_widget_get_sensitive (GTK_WIDGET (button3)));

  gtk_check_button_set_active (GTK_CHECK_BUTTON (checkbutton), TRUE);
  g_assert_true (gtk_widget_get_sensitive (GTK_WIDGET (button)));
  g_assert_true (gtk_widget_get_sensitive (GTK_WIDGET (button2)));
  g_assert_true (gtk_widget_get_sensitive (GTK_WIDGET (button3)));

  window = gtk_builder_get_object (builder, "window");
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (builder);
}

#define MY_GTK_GRID_TEMPLATE "\
<interface>\n\
 <template class=\"MyGtkGrid\" parent=\"GtkGrid\">\n\
   <property name=\"visible\">True</property>\n\
    <child>\n\
     <object class=\"GtkLabel\" id=\"label\">\n\
       <property name=\"visible\">True</property>\n\
     </object>\n\
  </child>\n\
 </template>\n\
</interface>\n"

#define MY_TYPE_GTK_GRID             (my_gtk_grid_get_type ())
#define MY_IS_GTK_GRID(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_GTK_GRID))

typedef struct
{
  GtkGridClass parent_class;
} MyGtkGridClass;

typedef struct
{
  GtkLabel *label;
} MyGtkGridPrivate;

typedef struct
{
  GtkGrid parent_instance;
  GtkLabel *label;
  MyGtkGridPrivate *priv;
} MyGtkGrid;

G_DEFINE_TYPE_WITH_PRIVATE (MyGtkGrid, my_gtk_grid, GTK_TYPE_GRID);

static void
my_gtk_grid_init (MyGtkGrid *grid)
{
  grid->priv = my_gtk_grid_get_instance_private (grid);
  gtk_widget_init_template (GTK_WIDGET (grid));
}

static void
my_gtk_grid_class_init (MyGtkGridClass *klass)
{
  GBytes *template = g_bytes_new_static (MY_GTK_GRID_TEMPLATE, strlen (MY_GTK_GRID_TEMPLATE));
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template (widget_class, template);
  gtk_widget_class_bind_template_child (widget_class, MyGtkGrid, label);
  gtk_widget_class_bind_template_child_private (widget_class, MyGtkGrid, label);

  g_bytes_unref (template);
}

static void
test_template (void)
{
  MyGtkGrid *my_gtk_grid;

  /* make sure the type we are trying to register does not exist */
  g_assert_false (g_type_from_name ("MyGtkGrid"));

  /* create the template object */
  my_gtk_grid = g_object_new (MY_TYPE_GTK_GRID, NULL);

  /* Check everything is fine */
  g_assert_true (g_type_from_name ("MyGtkGrid"));
  g_assert_true (MY_IS_GTK_GRID (my_gtk_grid));
  g_assert_true (my_gtk_grid->label == my_gtk_grid->priv->label);
  g_assert_true (GTK_IS_LABEL (my_gtk_grid->label));
  g_assert_true (GTK_IS_LABEL (my_gtk_grid->priv->label));

  g_object_ref_sink (my_gtk_grid);
  g_object_unref (my_gtk_grid);
}

_BUILDER_TEST_EXPORT void
on_cellrenderertoggle1_toggled (GtkCellRendererToggle *cell)
{
}

static void
test_anaconda_signal (void)
{
  GtkBuilder *builder;
  const char buffer[] =
    "<?xml version='1.0' encoding='UTF-8'?>"
    "<interface>"
    "  <requires lib='gtk' version='4.0'/>"
    "  <object class='GtkListStore' id='liststore1'>"
    "    <columns>"
    "      <!-- column-name use -->"
    "      <column type='gboolean'/>"
    "    </columns>"
    "  </object>"
    "  <object class='GtkWindow' id='window1'>"
    "    <child>"
    "      <object class='GtkTreeView' id='treeview1'>"
    "        <property name='visible'>True</property>"
    "        <property name='model'>liststore1</property>"
    "        <child internal-child='selection'>"
    "          <object class='GtkTreeSelection' id='treeview-selection1'/>"
    "        </child>"
    "        <child>"
    "          <object class='GtkTreeViewColumn' id='treeviewcolumn1'>"
    "            <property name='title' translatable='yes'>column</property>"
    "            <child>"
    "              <object class='GtkCellRendererToggle' id='cellrenderertoggle1'>"
    "                <signal name='toggled' handler='on_cellrenderertoggle1_toggled' swapped='no'/>"
    "              </object>"
    "              <attributes>"
    "                <attribute name='active'>0</attribute>"
    "              </attributes>"
    "            </child>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);

  g_object_unref (builder);
}

static void
test_file_filter (void)
{
  GtkBuilder *builder;
  GObject *obj;
  GtkFileFilter *filter;

  const char buffer[] =
    "<interface>"
    "  <object class='GtkFileFilter' id='filter1'>"
    "    <property name='name'>Text and Images</property>"
    "    <mime-types>"
    "      <mime-type>text/plain</mime-type>"
    "      <mime-type>image/*</mime-type>"
    "    </mime-types>"
    "    <patterns>"
    "      <pattern>*.txt</pattern>"
    "      <pattern>*.png</pattern>"
    "    </patterns>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = gtk_builder_get_object (builder, "filter1");
  g_assert_true (GTK_IS_FILE_FILTER (obj));
  filter = GTK_FILE_FILTER (obj);
  g_assert_cmpstr (gtk_file_filter_get_name (filter), ==, "Text and Images");
  g_assert_true (g_strv_contains (gtk_file_filter_get_attributes (filter), G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));
  g_assert_true (g_strv_contains (gtk_file_filter_get_attributes (filter), G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME));

  g_object_unref (builder);
}

static void
test_shortcuts (void)
{
  GtkBuilder *builder;
  GObject *obj;

  const char buffer[] =
    "<interface>"
    "  <object class='GtkBox' id='box'>"
    "    <child>"
    "      <object class='GtkShortcutController' id='controller'>"
    "        <property name='scope'>managed</property>"
    "        <child>"
    "          <object class='GtkShortcut'>"
    "            <property name='trigger'>&lt;Control&gt;k</property>"
    "            <property name='action'>activate</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  builder = builder_new_from_string (buffer, -1, NULL);
  obj = gtk_builder_get_object (builder, "controller");
  g_assert_true (GTK_IS_SHORTCUT_CONTROLLER (obj));
  g_object_unref (builder);
}

static void
test_transforms (void)
{
  GtkBuilder * builder;
  const char buffer1[] =
    "<interface>"
    "  <object class=\"GtkFixed\" id=\"fixed1\">"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label1\">"
    "        <layout>"
    "          <property name=\"transform\">rotateX(45.0)</property>"
    "        </layout>"
    "      </object>"
    "    </child>"
    "    <child>"
    "      <object class=\"GtkLabel\" id=\"label2\">"
    "        <layout>"
    "          <property name=\"transform\">scale3d(1,2,3)translate3d(2,3,0)</property>"
    "        </layout>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>";

  GObject *label, *vbox;

  builder = builder_new_from_string (buffer1, -1, NULL);
  vbox = gtk_builder_get_object (builder, "fixed1");
  g_assert_true (GTK_IS_FIXED (vbox));

  label = gtk_builder_get_object (builder, "label1");
  g_assert_true (GTK_IS_LABEL (label));

  label = gtk_builder_get_object (builder, "label2");
  g_assert_true (GTK_IS_LABEL (label));

  g_object_unref (builder);
}

G_MODULE_EXPORT char *
builder_get_search (gpointer item)
{
  return g_strdup (gtk_string_filter_get_search (item));
}

G_MODULE_EXPORT char *
builder_copy_arg (gpointer item, const char *arg)
{
  return g_strdup (arg);
}

static void
test_expressions (void)
{
  const char *tests[] = {
    "<interface>"
    "  <object class='GtkStringFilter' id='filter'>"
    "    <property name='search'>Hello World</property>"
    "    <property name='expression'><constant type='gchararray'>Hello World</constant></property>"
    "  </object>"
    "</interface>",
    "<interface>"
    "  <object class='GtkStringFilter' id='filter'>"
    "    <property name='search'>Hello World</property>"
    "    <property name='expression'><closure type='gchararray' function='builder_get_search'></closure></property>"
    "  </object>"
    "</interface>",
    "<interface>"
    "  <object class='GtkStringFilter' id='filter'>"
    "    <property name='search'>Hello World</property>"
    "    <property name='expression'><lookup type='GtkStringFilter' name='search'></lookup></property>"
    "  </object>"
    "</interface>",
    "<interface>"
    "  <object class='GtkStringFilter' id='filter'>"
    "    <property name='search'>Hello World</property>"
    "    <property name='expression'><closure type='gchararray' function='builder_copy_arg'>"
    "      <constant type='gchararray'>Hello World</constant>"
    "    </closure></property>"
    "  </object>"
    "</interface>",
  };
  GtkBuilder *builder;
  GObject *obj;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      builder = builder_new_from_string (tests[i], -1, NULL);
      obj = gtk_builder_get_object (builder, "filter");
      g_assert_true (GTK_IS_FILTER (obj));
      g_assert_true (gtk_filter_match (GTK_FILTER (obj), obj));

      g_object_unref (builder);
    }
}

#define MY_GTK_BOX_TEMPLATE "\
<interface>\n\
 <template class=\"MyGtkBox\" parent=\"GtkWidget\">\n\
  <child>\n\
   <object class=\"GtkLabel\" id=\"label\">\n\
    <property name=\"label\">First</property>\n\
   </object>\n\
  </child>\n\
  <child>\n\
   <object class=\"GtkLabel\" id=\"label2\">\n\
    <property name=\"label\">Second</property>\n\
   </object>\n\
  </child>\n\
 </template>\n\
</interface>\n"

#define MY_TYPE_GTK_BOX (my_gtk_box_get_type ())
G_DECLARE_FINAL_TYPE    (MyGtkBox, my_gtk_box, MY, GTK_BOX, GtkWidget)

struct _MyGtkBox
{
  GtkWidget  parent_instance;
  GtkLabel  *label;
  GtkLabel  *label2;
};

G_DEFINE_TYPE (MyGtkBox, my_gtk_box, GTK_TYPE_WIDGET);

static void
my_gtk_box_init (MyGtkBox *grid)
{
  gtk_widget_init_template (GTK_WIDGET (grid));
}

static void
my_gtk_box_dispose (GObject *obj)
{
  MyGtkBox  *my_gtk_box = MY_GTK_BOX (obj);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (my_gtk_box))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (my_gtk_box_parent_class)->dispose (obj);
}

static void
my_gtk_box_class_init (MyGtkBoxClass *klass)
{
  GBytes *template = g_bytes_new_static (MY_GTK_BOX_TEMPLATE, strlen (MY_GTK_BOX_TEMPLATE));
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template (widget_class, template);
  gtk_widget_class_bind_template_child (widget_class, MyGtkBox, label);
  gtk_widget_class_bind_template_child (widget_class, MyGtkBox, label2);

  G_OBJECT_CLASS (klass)->dispose = my_gtk_box_dispose;

  g_bytes_unref (template);
}

typedef struct
{
  MyGtkBox *my_gtk_box;
  guint     destroy_count;
} BoxDestroyData;

static void
my_label_destroy (GtkLabel *label, BoxDestroyData *data)
{
  g_assert_true (MY_IS_GTK_BOX (data->my_gtk_box));
  /* Make sure the other label is null if it was disposed first */
  g_assert_true (!data->my_gtk_box->label2 || GTK_IS_LABEL (data->my_gtk_box->label2));
  data->destroy_count++;
}

static void
my_label2_destroy (GtkLabel *label2, BoxDestroyData *data)
{
  g_assert_true (MY_IS_GTK_BOX (data->my_gtk_box));
  /* Make sure the other label is null if it was disposed first */
  g_assert_true (!data->my_gtk_box->label || GTK_IS_LABEL (data->my_gtk_box->label));
  data->destroy_count++;
}

static void
test_child_dispose_order (void)
{
  BoxDestroyData data = { 0, };

  /* make sure the type we are trying to register does not exist */
  g_assert_false (g_type_from_name ("MyGtkBox"));

  /* create the template object */
  data.my_gtk_box = g_object_ref_sink (g_object_new (MY_TYPE_GTK_BOX, NULL));

  /* Check everything is fine */
  g_assert_true (g_type_from_name ("MyGtkBox"));
  g_assert_true (MY_IS_GTK_BOX (data.my_gtk_box));
  g_assert_true (GTK_IS_LABEL (data.my_gtk_box->label));
  g_assert_true (GTK_IS_LABEL (data.my_gtk_box->label2));

  /* Check if both labels are destroyed */
  g_signal_connect (data.my_gtk_box->label, "destroy", G_CALLBACK (my_label_destroy), &data);
  g_signal_connect (data.my_gtk_box->label2, "destroy", G_CALLBACK (my_label2_destroy), &data);
  g_object_unref (data.my_gtk_box);
  g_assert_cmpuint (data.destroy_count, ==, 2);
}

#define MY_GTK_BUILDABLE_TEMPLATE "\
<interface>\n\
 <template class=\"MyGtkBuildable\" parent=\"GtkWidget\">\n\
  <custom/>\n\
  <custom>\n\
    <custom/>\n\
  </custom>\n\
 </template>\n\
</interface>\n"

#define MY_TYPE_GTK_BUILDABLE (my_gtk_buildable_get_type ())
G_DECLARE_FINAL_TYPE          (MyGtkBuildable, my_gtk_buildable, MY, GTK_BUILDABLE, GtkWidget)

struct _MyGtkBuildable
{
  GtkWidget parent_instance;
};

static void my_gtk_buildable_buildable_init (GtkBuildableIface *iface);
static GtkBuildableIface *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (MyGtkBuildable, my_gtk_buildable, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                my_gtk_buildable_buildable_init));

static void
my_gtk_buildable_init (MyGtkBuildable *buildable)
{
  gtk_widget_init_template (GTK_WIDGET (buildable));
}

static void
my_gtk_buildable_class_init (MyGtkBuildableClass *klass)
{
  GBytes *template = g_bytes_new_static (MY_GTK_BUILDABLE_TEMPLATE, strlen (MY_GTK_BUILDABLE_TEMPLATE));
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template (widget_class, template);

  g_bytes_unref (template);
}

static const GtkBuildableParser custom_parser = {
  NULL,
  NULL,
  NULL,
  NULL
};

static gboolean
my_gtk_buildable_custom_tag_start (GtkBuildable       *buildable,
                                   GtkBuilder         *builder,
                                   GObject            *child,
                                   const char         *tagname,
                                   GtkBuildableParser *parser,
                                   gpointer           *parser_data)
{
  if (strcmp (tagname, "custom") == 0) {
    *parser = custom_parser;
    *parser_data = NULL;

    return TRUE;
  }

  return FALSE;
}

static void
my_gtk_buildable_custom_finished (GtkBuildable *buildable,
                                  GtkBuilder   *builder,
                                  GObject      *child,
                                  const char   *tagname,
                                  gpointer      user_data)
{
  if (strcmp (tagname, "custom") == 0)
    return;

  parent_buildable_iface->custom_finished (buildable, builder, child,
                                           tagname, user_data);
}

static void
my_gtk_buildable_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->custom_tag_start = my_gtk_buildable_custom_tag_start;
  iface->custom_finished = my_gtk_buildable_custom_finished;
}

static void
test_buildable (void)
{
  MyGtkBuildable *my_gtk_buildable;

  /* make sure the type we are trying to register does not exist */
  g_assert_false (g_type_from_name ("MyGtkBuildable"));

  /* create the template object */
  my_gtk_buildable = g_object_new (MY_TYPE_GTK_BUILDABLE, NULL);

  /* Check everything is fine */
  g_assert_true (g_type_from_name ("MyGtkBuildable"));
  g_assert_true (MY_IS_GTK_BUILDABLE (my_gtk_buildable));

  g_object_ref_sink (my_gtk_buildable);
  g_object_unref (my_gtk_buildable);
}

int
main (int argc, char **argv)
{
  /* initialize test program */
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/Builder/Parser", test_parser);
  g_test_add_func ("/Builder/Types", test_types);
  g_test_add_func ("/Builder/Construct-Only Properties", test_construct_only_property);
  g_test_add_func ("/Builder/Children", test_children);
  g_test_add_func ("/Builder/Layout Properties", test_layout_properties);
  g_test_add_func ("/Builder/Object Properties", test_object_properties);
  g_test_add_func ("/Builder/Notebook", test_notebook);
  g_test_add_func ("/Builder/Domain", test_domain);
  g_test_add_func ("/Builder/Signal Autoconnect", test_connect_signals);
  g_test_add_func ("/Builder/Spin Button", test_spin_button);
  g_test_add_func ("/Builder/SizeGroup", test_sizegroup);
  g_test_add_func ("/Builder/ListStore", test_list_store);
  g_test_add_func ("/Builder/TreeStore", test_tree_store);
  g_test_add_func ("/Builder/TreeView Column", test_treeview_column);
  g_test_add_func ("/Builder/IconView", test_icon_view);
  g_test_add_func ("/Builder/ComboBox", test_combo_box);
#if 0
  g_test_add_func ("/Builder/ComboBox Entry", test_combo_box_entry);
#endif
  g_test_add_func ("/Builder/CellView", test_cell_view);
  g_test_add_func ("/Builder/Dialog", test_dialog);
  g_test_add_func ("/Builder/Widget", test_widget);
  g_test_add_func ("/Builder/Value From String", test_value_from_string);
  g_test_add_func ("/Builder/Reference Counting", test_reference_counting);
  g_test_add_func ("/Builder/Window", test_window);
  g_test_add_func ("/Builder/PangoAttributes", test_pango_attributes);
  g_test_add_func ("/Builder/Requires", test_requires);
  g_test_add_func ("/Builder/AddObjects", test_add_objects);
  g_test_add_func ("/Builder/MessageArea", test_message_area);
  g_test_add_func ("/Builder/MessageDialog", test_message_dialog);
  g_test_add_func ("/Builder/GMenu", test_gmenu);
  g_test_add_func ("/Builder/LevelBar", test_level_bar);
  g_test_add_func ("/Builder/Expose Object", test_expose_object);
  g_test_add_func ("/Builder/Template", test_template);
  g_test_add_func ("/Builder/No IDs", test_no_ids);
  g_test_add_func ("/Builder/Property Bindings", test_property_bindings);
  g_test_add_func ("/Builder/anaconda-signal", test_anaconda_signal);
  g_test_add_func ("/Builder/FileFilter", test_file_filter);
  g_test_add_func ("/Builder/Shortcuts", test_shortcuts);
  g_test_add_func ("/Builder/Transforms", test_transforms);
  g_test_add_func ("/Builder/Expressions", test_expressions);
  g_test_add_func ("/Builder/Child Dispose Order", test_child_dispose_order);
  g_test_add_func ("/Builder/Buildable", test_buildable);

  return g_test_run();
}

