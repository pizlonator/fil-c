#include <gtk/gtk.h>

static void
fill_text_view (GtkWidget *tv, const char *text)
{
  int i;
  GString *s;

  s = g_string_new ("");
  for (i = 0; i < 200; i++)
    g_string_append_printf (s, "%s %d\n", text, i);

  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv)),
                            g_string_free (s, FALSE), -1); 
}

int
main (int argc, char *argv[])
{
  GtkWidget *win, *box, *tv, *sw, *sb;
  GtkAdjustment *adj;

  gtk_init ();

  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 640, 480);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_window_set_child (GTK_WINDOW (win), box);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Left");
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);

  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sw), adj);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Middle");
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sw), adj);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_EXTERNAL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_box_append (GTK_BOX (box), sw);
  tv = gtk_text_view_new ();
  fill_text_view (tv, "Right");
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);

  sb = gtk_scrollbar_new (GTK_ORIENTATION_VERTICAL, adj);

  gtk_box_append (GTK_BOX (box), sb);

  gtk_window_present (GTK_WINDOW (win));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
