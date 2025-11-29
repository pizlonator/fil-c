#include <gtk/gtk.h>

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
  GtkWidget *box;
  GtkWidget *child;
  gboolean done = FALSE;

  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new ();
  box = gtk_center_box_new ();
  gtk_window_set_child (GTK_WINDOW (window), box);

  child = gtk_label_new ("Start Start Start Start Start");
  gtk_label_set_ellipsize (GTK_LABEL (child), PANGO_ELLIPSIZE_END);
  gtk_center_box_set_start_widget (GTK_CENTER_BOX (box), child);

  child = gtk_label_new ("Center");
  gtk_label_set_ellipsize (GTK_LABEL (child), PANGO_ELLIPSIZE_END);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (box), child);

  child = gtk_label_new ("End");
  gtk_label_set_ellipsize (GTK_LABEL (child), PANGO_ELLIPSIZE_END);
  gtk_center_box_set_end_widget (GTK_CENTER_BOX (box), child);

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
