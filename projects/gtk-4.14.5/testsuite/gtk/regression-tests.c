/* Regression tests
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

#include <gtk/gtk.h>

static void
test_9d6da33ff5c5e41e3521e1afd63d2d67bc915753 (void)
{
  GtkWidget *window, *label;

  window = gtk_window_new ();
  label = gtk_label_new ("I am sensitive.");
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_set_sensitive (label, FALSE);
  gtk_widget_set_sensitive (window, FALSE);
  gtk_widget_set_sensitive (label, TRUE);
  gtk_widget_set_sensitive (window, TRUE);

  g_assert_true (gtk_widget_get_sensitive (label));

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_94f00eb04dd1433cf1cc9a3341f485124e38abd1 (void)
{
  GtkWidget *window, *label;

  window = gtk_window_new ();
  label = gtk_label_new ("I am insensitive.");
  gtk_window_set_child (GTK_WINDOW (window), label);

  gtk_widget_set_sensitive (window, FALSE);
  gtk_widget_set_sensitive (label, FALSE);
  gtk_widget_set_sensitive (label, TRUE);

  g_assert_false (gtk_widget_is_sensitive (label));

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/regression/94f00eb04dd1433cf1cc9a3341f485124e38abd1", test_94f00eb04dd1433cf1cc9a3341f485124e38abd1);
  g_test_add_func ("/regression/9d6da33ff5c5e41e3521e1afd63d2d67bc915753", test_9d6da33ff5c5e41e3521e1afd63d2d67bc915753);

  return g_test_run ();
}
