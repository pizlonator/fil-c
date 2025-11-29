/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

/* test that attach_next_to picks the places
 * we expect it to pick, when there is any choice
 */
static void
test_attach (void)
{
  GtkGrid *g;
  GtkWidget *child, *sibling, *z, *A, *B;
  int left, top, width, height;

  g = (GtkGrid *)gtk_grid_new ();

  child = gtk_label_new ("a");
  gtk_grid_attach_next_to (g, child, NULL, GTK_POS_LEFT, 1, 1);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, -1);
  g_assert_cmpint (top,    ==, 0);
  g_assert_cmpint (width,  ==, 1);
  g_assert_cmpint (height, ==, 1);

  sibling = child;
  child = gtk_label_new ("b");
  gtk_grid_attach_next_to (g, child, sibling, GTK_POS_RIGHT, 2, 2);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 0);
  g_assert_cmpint (top,    ==, 0);
  g_assert_cmpint (width,  ==, 2);
  g_assert_cmpint (height, ==, 2);

  /* this one should just be ignored */
  z = gtk_label_new ("z");
  gtk_grid_attach (g, z, 4, 4, 1, 1);

  child = gtk_label_new ("c");
  gtk_grid_attach_next_to (g, child, sibling, GTK_POS_BOTTOM, 3, 1);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, -1);
  g_assert_cmpint (top,    ==, 1);
  g_assert_cmpint (width,  ==, 3);
  g_assert_cmpint (height, ==, 1);

  child = gtk_label_new ("u");
  gtk_grid_attach_next_to (g, child, z, GTK_POS_LEFT, 2, 1);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 2);
  g_assert_cmpint (top,    ==, 4);
  g_assert_cmpint (width,  ==, 2);
  g_assert_cmpint (height, ==, 1);

  child = gtk_label_new ("v");
  gtk_grid_attach_next_to (g, child, z, GTK_POS_RIGHT, 2, 1);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 5);
  g_assert_cmpint (top,    ==, 4);
  g_assert_cmpint (width,  ==, 2);
  g_assert_cmpint (height, ==, 1);

  child = gtk_label_new ("x");
  gtk_grid_attach_next_to (g, child, z, GTK_POS_TOP, 1, 2);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 4);
  g_assert_cmpint (top,    ==, 2);
  g_assert_cmpint (width,  ==, 1);
  g_assert_cmpint (height, ==, 2);

  child = gtk_label_new ("x");
  gtk_grid_attach_next_to (g, child, z, GTK_POS_TOP, 1, 2);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 4);
  g_assert_cmpint (top,    ==, 2);
  g_assert_cmpint (width,  ==, 1);
  g_assert_cmpint (height, ==, 2);

  child = gtk_label_new ("y");
  gtk_grid_attach_next_to (g, child, z, GTK_POS_BOTTOM, 1, 2);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 4);
  g_assert_cmpint (top,    ==, 5);
  g_assert_cmpint (width,  ==, 1);
  g_assert_cmpint (height, ==, 2);

  A = gtk_label_new ("A");
  gtk_grid_attach (g, A, 10, 10, 1, 1);
  B = gtk_label_new ("B");
  gtk_grid_attach (g, B, 10, 12, 1, 1);

  child  = gtk_label_new ("D");
  gtk_grid_attach_next_to (g, child, A, GTK_POS_RIGHT, 1, 3);
  gtk_grid_query_child (g, child,
                        &left, &top,
                        &width, &height);
  g_assert_cmpint (left,   ==, 11);
  g_assert_cmpint (top,    ==, 10);
  g_assert_cmpint (width,  ==,  1);
  g_assert_cmpint (height, ==,  3);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/grid/attach", test_attach);

  return g_test_run();
}
