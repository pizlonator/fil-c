#include <gtk/gtk.h>

static void
set_insensitive (GtkButton *b, GtkWidget *w)
{
  gtk_widget_set_sensitive (w, FALSE);
}

static void
state_flags_changed (GtkWidget *widget)
{
  GtkStateFlags flags;
  const char *sep;

  g_print ("state changed: \n");

  flags = gtk_widget_get_state_flags (widget);
  sep = "";
  if (flags & GTK_STATE_FLAG_ACTIVE)
    {
      g_print ("%sactive", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_PRELIGHT)
    {
      g_print ("%sprelight", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_SELECTED)
    {
      g_print ("%sselected", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_INSENSITIVE)
    {
      g_print ("%sinsensitive", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_INCONSISTENT)
    {
      g_print ("%sinconsistent", sep);
      sep = "|";
    }
  if (flags & GTK_STATE_FLAG_FOCUSED)
    {
      g_print ("%sfocused", sep);
      sep = "|";
    }
  if (sep[0] == 0)
    g_print ("normal");
  g_print ("\n");
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *bu;
  GtkWidget *w, *c;

  gtk_init ();

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_window_set_child (GTK_WINDOW (window), box);

  w = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 15);
  gtk_box_append (GTK_BOX (box), w);
  gtk_box_append (GTK_BOX (w), gtk_entry_new ());
  bu = gtk_button_new_with_label ("Bu");
  gtk_box_append (GTK_BOX (w), bu);
  c = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (c), TRUE);
  gtk_widget_set_halign (c, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (c, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (box), c);
  g_signal_connect (bu, "clicked", G_CALLBACK (set_insensitive), w);
  g_signal_connect (bu, "state-flags-changed", G_CALLBACK (state_flags_changed), NULL);

  g_object_bind_property (c, "active", w, "sensitive", G_BINDING_BIDIRECTIONAL);

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
