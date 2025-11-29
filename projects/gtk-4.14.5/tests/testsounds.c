#include <gtk/gtk.h>

static void
ended (GObject *object)
{
  g_object_unref (object);
}

static void
play (const char *name)
{
  char *path;
  GtkMediaStream *stream;

  path = g_build_filename ("tests", name, NULL);

  stream = gtk_media_file_new_for_filename (path);
  gtk_media_stream_set_volume (stream, 1.0);

  gtk_media_stream_play (stream);

  g_signal_connect (stream, "notify::ended", G_CALLBACK (ended), NULL);

  g_free (path);
}

static void
enter (GtkButton *button)
{
  play ("service-login.oga");
}

static void
leave (GtkButton *button)
{
  play ("service-logout.oga");
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;

  gtk_init ();

  window = gtk_window_new ();

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (box, GTK_ALIGN_CENTER);
  gtk_window_set_child (GTK_WINDOW (window), box);

  button = gtk_button_new_with_label ("Α");
  g_signal_connect (button, "clicked", G_CALLBACK (enter), NULL);
  gtk_box_append (GTK_BOX (box), button);

  button = gtk_button_new_with_label ("Ω");
  g_signal_connect (button, "clicked", G_CALLBACK (leave), NULL);
  gtk_box_append (GTK_BOX (box), button);

  gtk_window_present (GTK_WINDOW (window));

  while (1)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
