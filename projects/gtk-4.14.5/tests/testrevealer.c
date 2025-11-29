#include <gtk/gtk.h>

int
main (int argc,
      char ** argv)
{
  GtkWidget *window, *revealer, *box, *widget, *entry;

  gtk_init ();

  window = gtk_window_new ();
  gtk_widget_set_size_request (window, 300, 300);

  box = gtk_grid_new ();
  gtk_window_set_child (GTK_WINDOW (window), box);

  widget = gtk_label_new ("Some filler text to avoid\nresizing of the window");
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_widget_set_margin_start (widget, 10);
  gtk_widget_set_margin_end (widget, 10);
  gtk_grid_attach (GTK_GRID (box), widget, 1, 1, 1, 1);

  widget = gtk_label_new ("Some filler text to avoid\nresizing of the window");
  gtk_widget_set_margin_top (widget, 10);
  gtk_widget_set_margin_bottom (widget, 10);
  gtk_widget_set_margin_start (widget, 10);
  gtk_widget_set_margin_end (widget, 10);
  gtk_grid_attach (GTK_GRID (box), widget, 4, 4, 1, 1);

  widget = gtk_toggle_button_new_with_label ("None");
  gtk_grid_attach (GTK_GRID (box), widget, 0, 0, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_START);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "00000");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_NONE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 1, 0, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Fade");
  gtk_grid_attach (GTK_GRID (box), widget, 5, 5, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  gtk_widget_set_valign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "00000");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 4, 5, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Slide");
  gtk_grid_attach (GTK_GRID (box), widget, 0, 2, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "12345");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 1, 2, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Swing");
  gtk_widget_set_valign (widget, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (box), widget, 0, 3, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_START);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "12345");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SWING_RIGHT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 1, 3, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Slide");
  gtk_grid_attach (GTK_GRID (box), widget, 2, 0, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "23456");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 2, 1, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Swing");
  gtk_grid_attach (GTK_GRID (box), widget, 3, 0, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "23456");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SWING_DOWN);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 3, 1, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Slide");
  gtk_grid_attach (GTK_GRID (box), widget, 5, 2, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "34567");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 4, 2, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Swing");
  gtk_widget_set_valign (widget, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (box), widget, 5, 3, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_hexpand (revealer, TRUE);
  gtk_widget_set_halign (revealer, GTK_ALIGN_END);
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "34567");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SWING_LEFT);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 4, 3, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Slide");
  gtk_grid_attach (GTK_GRID (box), widget, 2, 5, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "45678");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 2, 4, 1, 1);

  widget = gtk_toggle_button_new_with_label ("Swing");
  gtk_grid_attach (GTK_GRID (box), widget, 3, 5, 1, 1);
  revealer = gtk_revealer_new ();
  gtk_widget_set_vexpand (revealer, TRUE);
  gtk_widget_set_valign (revealer, GTK_ALIGN_END);
  entry = gtk_entry_new ();
  gtk_editable_set_text (GTK_EDITABLE (entry), "45678");
  gtk_revealer_set_child (GTK_REVEALER (revealer), entry);
  g_object_bind_property (widget, "active", revealer, "reveal-child", 0);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SWING_UP);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), 2000);
  gtk_grid_attach (GTK_GRID (box), revealer, 3, 4, 1, 1);

  gtk_window_present (GTK_WINDOW (window));
  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));

  return 0;
}
