/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */

#include <gtk/gtk.h>

#include "frame-stats.h"

double reveal_time = 5;

static GOptionEntry options[] = {
  { "time", 't', 0, G_OPTION_ARG_DOUBLE, &reveal_time, "Reveal time", "SECONDS" },
  { NULL }
};

static void
toggle_reveal (GtkRevealer *revealer)
{
  gtk_revealer_set_reveal_child (revealer, !gtk_revealer_get_reveal_child (revealer));
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main(int argc, char **argv)
{
  GtkWidget *window, *revealer, *grid, *widget;
  GtkCssProvider *cssprovider;
  GError *error = NULL;
  guint x, y;
  gboolean done = FALSE;

  GOptionContext *context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, options, NULL);
  frame_stats_add_options (g_option_context_get_main_group (context));

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("Option parsing failed: %s\n", error->message);
      return 1;
    }

  gtk_init ();

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  frame_stats_ensure (GTK_WINDOW (window));

  revealer = gtk_revealer_new ();
  gtk_widget_set_valign (revealer, GTK_ALIGN_START);
  gtk_revealer_set_transition_type (GTK_REVEALER (revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
  gtk_revealer_set_transition_duration (GTK_REVEALER (revealer), reveal_time * 1000);
  gtk_revealer_set_reveal_child (GTK_REVEALER (revealer), TRUE);
  g_signal_connect_after (revealer, "map", G_CALLBACK (toggle_reveal), NULL);
  g_signal_connect_after (revealer, "notify::child-revealed", G_CALLBACK (toggle_reveal), NULL);
  gtk_window_set_child (GTK_WINDOW (window), revealer);

  grid = gtk_grid_new ();
  gtk_revealer_set_child (GTK_REVEALER (revealer), grid);

  cssprovider = gtk_css_provider_new ();
  gtk_css_provider_load_from_string (cssprovider, "* { padding: 2px; text-shadow: 5px 5px 2px grey; }");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (cssprovider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  for (x = 0; x < 10; x++)
    {
      for (y = 0; y < 20; y++)
        {
          widget = gtk_label_new ("Hello World");
          gtk_grid_attach (GTK_GRID (grid), widget, x, y, 1, 1);
        }
    }

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
