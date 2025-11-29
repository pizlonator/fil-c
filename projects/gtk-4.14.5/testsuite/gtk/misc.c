/* Copyright (C) 2022 Red Hat, Inc.
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
test_color_dialog_button_new (void)
{
  GtkWidget *button;

  /* check that the constructor accepts NULL */
  button = gtk_color_dialog_button_new (NULL);
  g_object_ref_sink (button);
  g_object_unref (button);
}

static void
test_font_dialog_button_new (void)
{
  GtkWidget *button;

  /* check that the constructor accepts NULL */
  button = gtk_color_dialog_button_new (NULL);
  g_object_ref_sink (button);
  g_object_unref (button);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/colordialogbutton/new", test_color_dialog_button_new);
  g_test_add_func ("/fontdialogbutton/new", test_font_dialog_button_new);

  return g_test_run();
}
