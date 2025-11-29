/* simple.c
 * Copyright (C) 1997  Red Hat, Inc
 * Author: Elliot Lee
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
#include <gtk/gtk.h>
#include <string.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static char *
get_content (void)
{
  GString *s;
  int i;

  s = g_string_new ("");
  for (i = 1; i <= 150; i++)
    g_string_append_printf (s, "Line %d\n", i);

  return g_string_free (s, FALSE);
}

static void
mode_changed (GtkComboBox *combo, GtkScrolledWindow *sw)
{
  int active = gtk_combo_box_get_active (combo);

  gtk_scrolled_window_set_overlay_scrolling (sw, active == 1);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  char *content;
  GtkWidget *box;
  GtkWidget *sw;
  GtkWidget *tv;
  GtkWidget *sb2;
  GtkWidget *combo;
  GtkAdjustment *adj;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_window_set_child (GTK_WINDOW (window), box);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);

  gtk_widget_set_hexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);

  content = get_content ();

  tv = gtk_text_view_new ();
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tv), GTK_WRAP_WORD);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);
  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)),
                            content, -1);
  g_free (content);

  adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (tv));

  combo = gtk_combo_box_text_new ();
  gtk_widget_set_valign (combo, GTK_ALIGN_START);
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Traditional");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Overlay");
  g_signal_connect (combo, "changed", G_CALLBACK (mode_changed), sw);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);

  gtk_box_append (GTK_BOX (box), combo);

  sb2 = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adj);
  gtk_box_append (GTK_BOX (box), sb2);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
