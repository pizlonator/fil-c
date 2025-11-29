/* floatingtest.c - test floating flag uses
 * Copyright (C) 2005 Tim Janik
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

static gboolean destroyed = FALSE;
static void
destroy (void)
{
  destroyed = TRUE;
}

static void
floating_tests (void)
{
  GtkWidget *widget = g_object_new (GTK_TYPE_LABEL, NULL);
  g_object_connect (widget, "signal::destroy", destroy, NULL, NULL);

  g_assert_true (g_object_is_floating (widget));

  g_object_ref_sink (widget);
  g_assert_false (g_object_is_floating (widget));

  g_object_force_floating (G_OBJECT (widget));
  g_assert_true (g_object_is_floating (widget));

  g_object_ref_sink (widget);
  g_assert_false (g_object_is_floating (widget));

  g_assert_false (destroyed);
  g_object_unref (widget);
  g_assert_true (destroyed);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);
  g_test_add_func ("/floatingtest", floating_tests);
  return g_test_run();
}
