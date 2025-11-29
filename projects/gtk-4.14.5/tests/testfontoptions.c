#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkWidget *antialias;
static GtkWidget *subpixel;
static GtkWidget *hintstyle;

static void
set_font_options (GtkWidget *label)
{
  cairo_antialias_t aa;
  cairo_subpixel_order_t sp;
  cairo_hint_style_t hs;
  cairo_font_options_t *options;

  aa = gtk_combo_box_get_active (GTK_COMBO_BOX (antialias));
  sp = gtk_combo_box_get_active (GTK_COMBO_BOX (subpixel));
  hs = gtk_combo_box_get_active (GTK_COMBO_BOX (hintstyle));

  options = cairo_font_options_create ();
  cairo_font_options_set_antialias (options, aa);
  cairo_font_options_set_subpixel_order (options, sp);
  cairo_font_options_set_hint_style (options, hs);

  gtk_widget_set_font_options (label, options);
  cairo_font_options_destroy (options);

  gtk_widget_queue_draw (label);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *label, *grid, *demo;

  gtk_init ();

  window = gtk_window_new ();
  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 10);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 10);
  gtk_window_set_child (GTK_WINDOW (window), grid);
  label = gtk_label_new ("Default font options");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 2, 1);
  demo = gtk_label_new ("Custom font options");
  gtk_grid_attach (GTK_GRID (grid), demo, 0, 1, 2, 1);

  antialias = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Default");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "None");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Gray");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Subpixel");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Fast");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Good");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (antialias), "Best");
  g_signal_connect_swapped (antialias, "changed", G_CALLBACK (set_font_options), demo);
  label = gtk_label_new ("Antialias");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), antialias, 1, 2, 1, 1);

  subpixel = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (subpixel), "Default");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (subpixel), "RGB");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (subpixel), "BGR");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (subpixel), "Vertical RGB");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (subpixel), "Vertical BGR");
  g_signal_connect_swapped (subpixel, "changed", G_CALLBACK (set_font_options), demo);
  label = gtk_label_new ("Subpixel");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 3, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), subpixel, 1, 3, 1, 1);

  hintstyle = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hintstyle), "Default");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hintstyle), "None");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hintstyle), "Slight");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hintstyle), "Medium");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (hintstyle), "Full");
  g_signal_connect_swapped (hintstyle, "changed", G_CALLBACK (set_font_options), demo);
  label = gtk_label_new ("Hintstyle");
  gtk_grid_attach (GTK_GRID (grid), label, 0, 4, 1, 1);
  gtk_grid_attach (GTK_GRID (grid), hintstyle, 1, 4, 1, 1);

  gtk_combo_box_set_active (GTK_COMBO_BOX (antialias), 0);
  gtk_combo_box_set_active (GTK_COMBO_BOX (subpixel), 0);
  gtk_combo_box_set_active (GTK_COMBO_BOX (hintstyle), 0);

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
