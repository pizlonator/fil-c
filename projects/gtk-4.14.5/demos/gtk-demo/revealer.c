/* Revealer
 *
 * GtkRevealer is a container that animates showing and hiding
 * of its sole child with nice transitions.
 */

#include <gtk/gtk.h>

static int count = 0;
static guint timeout = 0;

static void
change_direction (GtkRevealer *revealer)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (revealer)))
    {
      gboolean revealed;

      revealed = gtk_revealer_get_child_revealed (revealer);
      gtk_revealer_set_reveal_child (revealer, !revealed);
    }
}

static gboolean
reveal_one (gpointer data)
{
  GtkWidget *window = data;
  char *name;
  GtkRevealer *revealer;

  name = g_strdup_printf ("revealer%d", count);
  revealer = GTK_REVEALER (g_object_get_data (G_OBJECT (window), name));
  g_free (name);

  gtk_revealer_set_reveal_child (revealer, TRUE);

  g_signal_connect (revealer, "notify::child-revealed",
                    G_CALLBACK (change_direction), NULL);
  count++;

  if (count >= 9)
    {
      timeout = 0;
      return FALSE;
    }
  else
    return TRUE;
}

static GtkWidget *window = NULL;

static void
on_destroy (gpointer data)
{
  window = NULL;
  if (timeout != 0)
    {
      g_source_remove (timeout);
      timeout = 0;
    }

}

GtkWidget *
do_revealer (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;
      int i;

      builder = gtk_builder_new_from_resource ("/revealer/revealer.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      for (i = 0; i < 10; i++)
        {
          char *name = g_strdup_printf ("revealer%d", i);
          GtkWidget *revealer = GTK_WIDGET (gtk_builder_get_object (builder, name));
          g_object_set_data (G_OBJECT (window), name, revealer);
          g_free (name);
        }
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    {
      count = 0;
      timeout = g_timeout_add (690, reveal_one, window);
      gtk_widget_set_visible (window, TRUE);
    }
  else
    {
      gtk_window_destroy (GTK_WINDOW (window));
    }


  return window;
}
