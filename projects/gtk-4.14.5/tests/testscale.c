/* testscale.c - scale mark demo
 * Copyright (C) 2009 Red Hat, Inc.
 * Author: Matthias Clasen
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


GSList *scales;
GtkWidget *flipbox;
GtkWidget *extra_scale;

static void
invert (GtkButton *button)
{
  GSList *l;

  for (l = scales; l; l = l->next)
    {
      GtkRange *range = l->data;
      gtk_range_set_inverted (range, !gtk_range_get_inverted (range));
    }
}

static void
flip (GtkButton *button)
{
  GSList *l;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (flipbox), 1 - gtk_orientable_get_orientation (GTK_ORIENTABLE (flipbox)));

  for (l = scales; l; l = l->next)
    {
      GtkOrientable *o = l->data;
      gtk_orientable_set_orientation (o, 1 - gtk_orientable_get_orientation (o));
    }
}

static void
trough (GtkToggleButton *button)
{
  GSList *l;
  gboolean value;

  value = gtk_toggle_button_get_active (button);

  for (l = scales; l; l = l->next)
    {
      GtkRange *range = l->data;
      gtk_range_set_range (range, 0., value ? 100.0 : 0.);
    }
}

double marks[3] = { 0.0, 50.0, 100.0 };
double extra_marks[2] = { 20.0, 40.0 };

static void
extra (GtkToggleButton *button)
{
  gboolean value;

  value = gtk_toggle_button_get_active (button);

  if (value)
    {
      gtk_scale_add_mark (GTK_SCALE (extra_scale), extra_marks[0], GTK_POS_TOP, NULL);
      gtk_scale_add_mark (GTK_SCALE (extra_scale), extra_marks[1], GTK_POS_TOP, NULL);
    }
  else
    {
      gtk_scale_clear_marks (GTK_SCALE (extra_scale));
      gtk_scale_add_mark (GTK_SCALE (extra_scale), marks[0], GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (extra_scale), marks[1], GTK_POS_BOTTOM, NULL);
      gtk_scale_add_mark (GTK_SCALE (extra_scale), marks[2], GTK_POS_BOTTOM, NULL);
    }
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *button;
  GtkWidget *frame;
  GtkWidget *scale;
  const char *labels[3] = {
    "<small>Left</small>",
    "<small>Middle</small>",
    "<small>Right</small>"
  };

  double bath_marks[4] = { 0.0, 33.3, 66.6, 100.0 };
  const char *bath_labels[4] = {
    "<span color='blue' size='small'>Cold</span>",
    "<span size='small'>Baby bath</span>",
    "<span size='small'>Hot tub</span>",
    "<span color='Red' size='small'>Hot</span>"
  };

  double pos_marks[4] = { 0.0, 33.3, 66.6, 100.0 };
  const char *pos_labels[4] = { "Left", "Right", "Top", "Bottom" };
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Ranges with marks");
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  box1 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  flipbox = box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_widget_set_hexpand (flipbox, TRUE);
  gtk_widget_set_vexpand (flipbox, TRUE);
  gtk_box_append (GTK_BOX (box1), box);
  gtk_window_set_child (GTK_WINDOW (window), box1);

  frame = gtk_frame_new ("No marks");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("With fill level");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_range_set_show_fill_level (GTK_RANGE (scale), TRUE);
  gtk_range_set_fill_level (GTK_RANGE (scale), 50);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Simple marks");
  extra_scale = scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_BOTTOM, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_BOTTOM, NULL);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Simple marks up");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_TOP, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_TOP, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_TOP, NULL);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Labeled marks");
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_BOTTOM, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_BOTTOM, labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_BOTTOM, labels[2]);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Some labels");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[0], GTK_POS_TOP, labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[1], GTK_POS_TOP, NULL);
  gtk_scale_add_mark (GTK_SCALE (scale), marks[2], GTK_POS_TOP, labels[2]);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Above and below");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[0], GTK_POS_TOP, bath_labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[1], GTK_POS_BOTTOM, bath_labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[2], GTK_POS_BOTTOM, bath_labels[2]);
  gtk_scale_add_mark (GTK_SCALE (scale), bath_marks[3], GTK_POS_TOP, bath_labels[3]);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  frame = gtk_frame_new ("Positions");
  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  scales = g_slist_prepend (scales, scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_scale_add_mark (GTK_SCALE (scale), pos_marks[0], GTK_POS_LEFT, pos_labels[0]);
  gtk_scale_add_mark (GTK_SCALE (scale), pos_marks[1], GTK_POS_RIGHT, pos_labels[1]);
  gtk_scale_add_mark (GTK_SCALE (scale), pos_marks[2], GTK_POS_TOP, pos_labels[2]);
  gtk_scale_add_mark (GTK_SCALE (scale), pos_marks[3], GTK_POS_BOTTOM, pos_labels[3]);
  gtk_frame_set_child (GTK_FRAME (frame), scale);
  gtk_box_append (GTK_BOX (box), frame);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_box_append (GTK_BOX (box1), box2);
  button = gtk_button_new_with_label ("Flip");
  g_signal_connect (button, "clicked", G_CALLBACK (flip), NULL);
  gtk_box_append (GTK_BOX (box2), button);

  button = gtk_button_new_with_label ("Invert");
  g_signal_connect (button, "clicked", G_CALLBACK (invert), NULL);
  gtk_box_append (GTK_BOX (box2), button);

  button = gtk_toggle_button_new_with_label ("Trough");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
  g_signal_connect (button, "toggled", G_CALLBACK (trough), NULL);
  gtk_box_append (GTK_BOX (box2), button);
  gtk_window_present (GTK_WINDOW (window));

  button = gtk_toggle_button_new_with_label ("Extra");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  g_signal_connect (button, "toggled", G_CALLBACK (extra), NULL);
  gtk_box_append (GTK_BOX (box2), button);
  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}


