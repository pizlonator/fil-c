#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
horizontal_policy_changed (GtkComboBox *combo_box,
			   GtkViewport *viewport)
{
  GtkScrollablePolicy policy = gtk_combo_box_get_active (combo_box);

  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (viewport), policy);
}

static void
vertical_policy_changed (GtkComboBox *combo_box,
			 GtkViewport *viewport)
{
  GtkScrollablePolicy policy = gtk_combo_box_get_active (combo_box);

  gtk_scrollable_set_vscroll_policy (GTK_SCROLLABLE (viewport), policy);
}

static void
content_width_changed (GtkSpinButton *spin_button,
                       gpointer       data)
{
  GtkScrolledWindow *swindow = data;
  double value;

  value = gtk_spin_button_get_value (spin_button);
  gtk_scrolled_window_set_min_content_width (swindow, (int)value);
}

static void
content_height_changed (GtkSpinButton *spin_button,
                        gpointer       data)
{
  GtkScrolledWindow *swindow = data;
  double value;

  value = gtk_spin_button_get_value (spin_button);
  gtk_scrolled_window_set_min_content_height (swindow, (int)value);
}

static void
kinetic_scrolling_changed (GtkToggleButton *toggle_button,
                           gpointer         data)
{
  GtkScrolledWindow *swindow = data;
  gboolean enabled = gtk_toggle_button_get_active (toggle_button);

  gtk_scrolled_window_set_kinetic_scrolling (swindow, enabled);
}

static void
add_row (GtkButton  *button,
         GtkListBox *listbox)
{
  GtkWidget *row;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW, NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), gtk_label_new ("test"));
  gtk_list_box_insert (GTK_LIST_BOX (listbox), row, -1);
}

static void
remove_row (GtkButton  *button,
            GtkListBox *listbox)
{
  GtkWidget *last;

  last = gtk_widget_get_last_child (GTK_WIDGET (listbox));
  if (last)
    gtk_list_box_remove (GTK_LIST_BOX (listbox), last);
}

static void
scrollable_policy (void)
{
  GtkWidget *window, *swindow, *hbox, *vbox, *frame, *cntl, *listbox;
  GtkWidget *viewport, *label, *expander, *widget, *popover;

  window = gtk_window_new ();
  hbox   = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  vbox   = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  gtk_window_set_child (GTK_WINDOW (window), hbox);
  gtk_box_append (GTK_BOX (hbox), vbox);

  frame = gtk_frame_new ("Scrolled Window");
  gtk_widget_set_hexpand (frame, TRUE);
  gtk_box_append (GTK_BOX (hbox), frame);

  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_frame_set_child (GTK_FRAME (frame), swindow);

  viewport = gtk_viewport_new (NULL, NULL);
  label = gtk_label_new ("Here is a wrapping label with a minimum width-chars of 40 and "
			 "a natural max-width-chars of 100 to demonstrate the usage of "
			 "scrollable widgets \"hscroll-policy\" and \"vscroll-policy\" "
			 "properties. Note also that when playing with the window height, "
			 "one can observe that the vscrollbar disappears as soon as there "
			 "is enough height to fit the content vertically if the window were "
			 "to be allocated a width without a vscrollbar present");

  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars  (GTK_LABEL (label), 40);
  gtk_label_set_max_width_chars  (GTK_LABEL (label), 100);

  gtk_viewport_set_child (GTK_VIEWPORT (viewport), label);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), viewport);

  /* Add controls here */
  expander = gtk_expander_new ("Controls");
  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
  cntl = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  gtk_expander_set_child (GTK_EXPANDER (expander), cntl);
  gtk_box_append (GTK_BOX (vbox), expander);

  /* Add Horizontal policy control here */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("hscroll-policy");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Minimum");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Natural");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_hexpand (widget, TRUE);

  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (horizontal_policy_changed), viewport);

  /* Add Vertical policy control here */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("vscroll-policy");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Minimum");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Natural");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_widget_set_hexpand (widget, TRUE);

  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (vertical_policy_changed), viewport);

  /* Content size controls */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_spin_button_new_with_range (100.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (content_width_changed), swindow);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_spin_button_new_with_range (100.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (cntl), hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (content_height_changed), swindow);

  /* Add Kinetic scrolling control here */
  widget = gtk_check_button_new_with_label ("Kinetic scrolling");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (cntl), widget);
  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (kinetic_scrolling_changed), swindow);

  gtk_window_present (GTK_WINDOW (window));

  /* Popover */
  popover = gtk_popover_new ();

  widget = gtk_menu_button_new ();
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (widget), popover);
  gtk_menu_button_set_label (GTK_MENU_BUTTON (widget), "Popover");
  gtk_box_append (GTK_BOX (cntl), widget);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_popover_set_child (GTK_POPOVER (popover), vbox);

  /* Popover's scrolled window */
  swindow = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_box_append (GTK_BOX (vbox), swindow);

  /* Listbox */
  listbox = gtk_list_box_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (swindow), listbox);

  /* Min content */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("min-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_spin_button_new_with_range (0.0, 150.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "min-content-width",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  widget = gtk_label_new ("min-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);


  widget = gtk_spin_button_new_with_range (0.0, 150.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "min-content-height",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Max content */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_label_new ("max-content-width");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_spin_button_new_with_range (250.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "max-content-width",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  widget = gtk_label_new ("max-content-height");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  widget = gtk_spin_button_new_with_range (250.0, 1000.0, 10.0);
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_object_bind_property (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget)),
                          "value",
                          swindow,
                          "max-content-height",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  /* Add and Remove buttons */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  widget = gtk_button_new_with_label ("Remove");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);

  g_signal_connect (widget, "clicked",
                    G_CALLBACK (remove_row), listbox);

  widget = gtk_button_new_with_label ("Add");
  gtk_widget_set_hexpand (widget, TRUE);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_signal_connect (widget, "clicked",
                    G_CALLBACK (add_row), listbox);
}


int
main (int argc, char *argv[])
{
  gtk_init ();

  scrollable_policy ();

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
