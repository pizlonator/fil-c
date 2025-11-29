/* testspinbutton.c
 * Copyright (C) 2004 Morten Welinder
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

#include "config.h"
#include <gtk/gtk.h>

static int num_windows = 0;

static gboolean done = FALSE;

static gboolean
on_delete (GtkWindow *w)
{
  num_windows--;
  if (num_windows == 0)
    {
      done = TRUE;
      g_main_context_wakeup (NULL);
    }

  return FALSE;
}

static void
prepare_window_for_orientation (GtkOrientation orientation)
{
  GtkWidget *window, *mainbox, *wrap_button;
  int max;

  window = gtk_window_new ();
  g_signal_connect (window, "close-request", G_CALLBACK (on_delete), NULL);

  mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL ^ orientation, 2);
  gtk_window_set_child (GTK_WINDOW (window), mainbox);

  wrap_button = gtk_toggle_button_new_with_label ("Wrap");
  gtk_box_append (GTK_BOX (mainbox), wrap_button);

  for (max = 9; max <= 999999999; max = max * 10 + 9)
    {
      GtkAdjustment *adj = gtk_adjustment_new (max,
                                               1, max,
                                               1,
                                               (max + 1) / 10,
                                               0.0);

      GtkWidget *spin = gtk_spin_button_new (adj, 1.0, 0);
      gtk_orientable_set_orientation (GTK_ORIENTABLE (spin), orientation);
      gtk_widget_set_halign (GTK_WIDGET (spin), GTK_ALIGN_CENTER);

      g_object_bind_property (wrap_button, "active", spin, "wrap", G_BINDING_SYNC_CREATE);

      GtkWidget *hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_box_append (GTK_BOX (hbox), spin);
      gtk_box_append (GTK_BOX (mainbox), hbox);
    }

  gtk_window_present (GTK_WINDOW (window));
  num_windows++;
}

int
main (int argc, char **argv)
{
  gtk_init ();

  prepare_window_for_orientation (GTK_ORIENTATION_HORIZONTAL);
  prepare_window_for_orientation (GTK_ORIENTATION_VERTICAL);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
