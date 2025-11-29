/* Size Groups
 *
 * GtkSizeGroup provides a mechanism for grouping a number of
 * widgets together so they all request the same amount of space.
 * This is typically useful when you want a column of widgets to
 * have the same size, but you can't use a GtkTable widget.
 *
 * Note that size groups only affect the amount of space requested,
 * not the size that the widgets finally receive. If you want the
 * widgets in a GtkSizeGroup to actually be the same size, you need
 * to pack them in such a way that they get the size they request
 * and not more. For example, if you are packing your widgets
 * into a table, you would not include the GTK_FILL flag.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void
add_row (GtkGrid      *table,
         int           row,
         GtkSizeGroup *size_group,
         const char   *label_text,
         const char  **options)
{
  GtkWidget *dropdown;
  GtkWidget *label;

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE_FILL);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (table, label, 0, row, 1, 1);

  dropdown = gtk_drop_down_new_from_strings (options);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), dropdown);
  gtk_widget_set_halign (dropdown, GTK_ALIGN_END);
  gtk_widget_set_valign (dropdown, GTK_ALIGN_BASELINE_FILL);
  gtk_size_group_add_widget (size_group, dropdown);
  gtk_grid_attach (table, dropdown, 1, row, 1, 1);
}

static void
toggle_grouping (GtkCheckButton *check_button,
                 GtkSizeGroup   *size_group)
{
  GtkSizeGroupMode new_mode;

  /* GTK_SIZE_GROUP_NONE is not generally useful, but is useful
   * here to show the effect of GTK_SIZE_GROUP_HORIZONTAL by
   * contrast.
   */
  if (gtk_check_button_get_active (check_button))
    new_mode = GTK_SIZE_GROUP_HORIZONTAL;
  else
    new_mode = GTK_SIZE_GROUP_NONE;

  gtk_size_group_set_mode (size_group, new_mode);
}

GtkWidget *
do_sizegroup (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *table;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *check_button;
  GtkSizeGroup *size_group;

  static const char *color_options[] = {
    "Red", "Green", "Blue", NULL
  };

  static const char *dash_options[] = {
    "Solid", "Dashed", "Dotted", NULL
  };

  static const char *end_options[] = {
    "Square", "Round", "Double Arrow", NULL
  };

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Size Groups");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_widget_set_margin_start (vbox, 5);
      gtk_widget_set_margin_end (vbox, 5);
      gtk_widget_set_margin_top (vbox, 5);
      gtk_widget_set_margin_bottom (vbox, 5);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
      g_object_set_data_full (G_OBJECT (window), "size-group", size_group, g_object_unref);

      /* Create one frame holding color options */
      frame = gtk_frame_new ("Color Options");
      gtk_box_append (GTK_BOX (vbox), frame);

      table = gtk_grid_new ();
      gtk_widget_set_margin_start (table, 5);
      gtk_widget_set_margin_end (table, 5);
      gtk_widget_set_margin_top (table, 5);
      gtk_widget_set_margin_bottom (table, 5);
      gtk_grid_set_row_spacing (GTK_GRID (table), 5);
      gtk_grid_set_column_spacing (GTK_GRID (table), 10);
      gtk_frame_set_child (GTK_FRAME (frame), table);

      add_row (GTK_GRID (table), 0, size_group, "_Foreground", color_options);
      add_row (GTK_GRID (table), 1, size_group, "_Background", color_options);

      /* And another frame holding line style options */
      frame = gtk_frame_new ("Line Options");
      gtk_box_append (GTK_BOX (vbox), frame);

      table = gtk_grid_new ();
      gtk_widget_set_margin_start (table, 5);
      gtk_widget_set_margin_end (table, 5);
      gtk_widget_set_margin_top (table, 5);
      gtk_widget_set_margin_bottom (table, 5);
      gtk_grid_set_row_spacing (GTK_GRID (table), 5);
      gtk_grid_set_column_spacing (GTK_GRID (table), 10);
      gtk_frame_set_child (GTK_FRAME (frame), table);

      add_row (GTK_GRID (table), 0, size_group, "_Dashing", dash_options);
      add_row (GTK_GRID (table), 1, size_group, "_Line ends", end_options);

      /* And a check button to turn grouping on and off */
      check_button = gtk_check_button_new_with_mnemonic ("_Enable grouping");
      gtk_box_append (GTK_BOX (vbox), check_button);

      gtk_check_button_set_active (GTK_CHECK_BUTTON (check_button), TRUE);
      g_signal_connect (check_button, "toggled",
                        G_CALLBACK (toggle_grouping), size_group);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
