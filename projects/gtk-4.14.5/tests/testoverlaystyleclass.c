#include <gtk/gtk.h>

static gboolean
overlay_get_child_position (GtkOverlay *overlay,
                            GtkWidget *child,
                            GdkRectangle *allocation,
                            gpointer user_data)
{
  GtkWidget *custom_child = user_data;
  GtkRequisition req;

  if (child != custom_child)
    return FALSE;

  gtk_widget_get_preferred_size (child, NULL, &req);

  allocation->x = 120;
  allocation->y = 0;
  allocation->width = req.width;
  allocation->height = req.height;

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *win, *overlay, *grid, *main_child, *child, *label, *sw;
  GtkCssProvider *provider;
  char *str;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (provider,
    "label { border: 3px solid black; border-radius: 5px; padding: 2px; }"
    ".top { border-top-style: none; border-top-right-radius: 0px; border-top-left-radius: 0px; }"
    ".bottom { border-bottom-style: none; border-bottom-right-radius: 0px; border-bottom-left-radius: 0px; }"
    ".left { border-left-style: none; border-top-left-radius: 0px; border-bottom-left-radius: 0px; }"
    ".right { border-right-style: none; border-top-right-radius: 0px; border-bottom-right-radius: 0px; }");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 600, 600);

  grid = gtk_grid_new ();
  label = gtk_label_new ("Out of overlay");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_widget_set_vexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  overlay = gtk_overlay_new ();
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_ALWAYS,
                                  GTK_POLICY_ALWAYS);
  gtk_overlay_set_child (GTK_OVERLAY (overlay), sw);

  main_child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), main_child);
  gtk_widget_set_hexpand (main_child, TRUE);
  gtk_widget_set_vexpand (main_child, TRUE);
  label = gtk_label_new ("Main child");
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (main_child), label);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Bottom/Right child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_END);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Left/Top child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Right/Center child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_END);
  gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  gtk_widget_set_margin_start (child, 55);
  gtk_widget_set_margin_top (child, 4);
  g_print ("Left/Top margined child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  child = gtk_label_new (NULL);
  str = g_strdup_printf ("%p", child);
  gtk_label_set_text (GTK_LABEL (child), str);
  g_free (str);
  g_print ("Custom get-child-position child: %p\n", child);
  gtk_widget_set_halign (child, GTK_ALIGN_START);
  gtk_widget_set_valign (child, GTK_ALIGN_START);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), child);

  g_signal_connect (overlay, "get-child-position",
                    G_CALLBACK (overlay_get_child_position), child);

  gtk_grid_attach (GTK_GRID (grid), overlay, 1, 0, 1, 3);
  gtk_window_set_child (GTK_WINDOW (win), grid);

  g_print ("\n");

  gtk_window_present (GTK_WINDOW (win));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
