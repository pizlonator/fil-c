#include <gtk/gtk.h>

static void
capture_widget_destroy (void)
{
  GtkWidget *searchbar = gtk_search_bar_new ();
  GtkWidget *button = gtk_button_new ();

  g_object_ref_sink (searchbar);
  g_object_ref_sink (button);

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (searchbar), button);

  g_assert_true (gtk_search_bar_get_key_capture_widget (GTK_SEARCH_BAR (searchbar)) == button);

  g_object_unref (button);

  g_assert_null (gtk_search_bar_get_key_capture_widget (GTK_SEARCH_BAR (searchbar)));

  g_object_unref (searchbar);
}

static void
capture_widget_unset (void)
{
  GtkWidget *searchbar = gtk_search_bar_new ();
  GtkWidget *button = gtk_button_new ();

  g_object_ref_sink (searchbar);
  g_object_ref_sink (button);

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (searchbar), button);

  g_assert_true (gtk_search_bar_get_key_capture_widget (GTK_SEARCH_BAR (searchbar)) == button);

  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (searchbar), NULL);

  g_assert_null (gtk_search_bar_get_key_capture_widget (GTK_SEARCH_BAR (searchbar)));

  g_object_unref (searchbar);
  g_object_unref (button);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/searchbar/capture-widget-destroy", capture_widget_destroy);
  g_test_add_func ("/searchbar/capture-widget-unset", capture_widget_unset);

  return g_test_run();
}
