#include <gtk/gtk.h>

static void
check_finalized (gpointer data,
                 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
popover (void)
{
  GtkWidget *button = gtk_menu_button_new ();
  GtkWidget *p = gtk_popover_new ();
  gboolean finalized = FALSE;

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), p);

  /* GtkButton is a normal widget and thus floating */
  g_assert_true (g_object_is_floating (button));
  /* GtkPopver sinks itself */
  g_assert_true (!g_object_is_floating (p));

  g_object_weak_ref (G_OBJECT (p), check_finalized, &finalized);

  g_object_ref_sink (button);
  g_object_unref (button);
  /* We do NOT unref p since the only reference held to it gets
   * removed when the button gets disposed. */
  g_assert_true (finalized);
}

static void
popover2 (void)
{
  GtkWidget *button = gtk_menu_button_new ();
  GtkWidget *p = gtk_popover_new ();
  gboolean finalized = FALSE;

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), p);

  g_assert_true (g_object_is_floating (button));
  g_assert_true (!g_object_is_floating (p));

  g_object_weak_ref (G_OBJECT (p), check_finalized, &finalized);

  g_object_ref_sink (button);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), NULL);

  g_assert_true (finalized);

  g_object_unref (button);
}

static void
filechooserwidget (void)
{
  /* We use GtkFileChooserWidget simply because it's a complex widget, that's it. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkWidget *w = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_OPEN);
G_GNUC_END_IGNORE_DEPRECATIONS
  gboolean finalized = FALSE;

  g_assert_true (g_object_is_floating (w));
  g_object_ref_sink (w);
  g_object_weak_ref (G_OBJECT (w), check_finalized, &finalized);

  g_object_unref (w);

  g_assert_true (finalized);
}

static void
window (void)
{
  GtkWidget *w = gtk_window_new ();
  gboolean finalized = FALSE;

  /* GTK holds a ref */
  g_assert_true (!g_object_is_floating (w));
  g_object_weak_ref (G_OBJECT (w), check_finalized, &finalized);

  gtk_window_destroy (GTK_WINDOW (w));

  g_assert_true (finalized);
}

int
main (int argc, char **argv)
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/gtk/widget-refcount/popover", popover);
  g_test_add_func ("/gtk/widget-refcount/popover2", popover2);
  g_test_add_func ("/gtk/widget-refcount/filechoosewidget", filechooserwidget);
  g_test_add_func ("/gtk/widget-refcount/window", window);

  return g_test_run ();
}
