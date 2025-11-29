/* Shortcuts
 * #Keywords: GtkShortcutController
 *
 * GtkShortcut is the abstraction used by GTK to handle shortcuts from
 * keyboard or other input devices.
 *
 * Shortcut triggers can be used to weave complex sequences of key
 * presses into sophisticated mechanisms to activate shortcuts.
 *
 * This demo code shows creative ways to do that.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static gboolean
shortcut_activated (GtkWidget *widget,
                    GVariant  *unused,
                    gpointer   row)
{
  g_print ("activated %s\n", gtk_label_get_label (row));
  return TRUE;
}

static GtkShortcutTrigger *
create_ctrl_g (void)
{
  return gtk_keyval_trigger_new (GDK_KEY_g, GDK_CONTROL_MASK);
}

static GtkShortcutTrigger *
create_x (void)
{
  return gtk_keyval_trigger_new (GDK_KEY_x, 0);
}

struct {
  const char *description;
  GtkShortcutTrigger * (* create_trigger_func) (void);
} shortcuts[] = {
  { "Press Ctrl-G", create_ctrl_g },
  { "Press X", create_x },
};

GtkWidget *
do_shortcut_triggers (GtkWidget *do_widget)
{
  guint i;

  if (!window)
    {
      GtkWidget *list;
      GtkEventController *controller;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Shortcuts");
      gtk_window_set_default_size (GTK_WINDOW (window), 200, -1);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      list = gtk_list_box_new ();
      gtk_widget_set_margin_top (list, 6);
      gtk_widget_set_margin_bottom (list, 6);
      gtk_widget_set_margin_start (list, 6);
      gtk_widget_set_margin_end (list, 6);
      gtk_window_set_child (GTK_WINDOW (window), list);

      for (i = 0; i < G_N_ELEMENTS (shortcuts); i++)
        {
          GtkShortcut *shortcut;
          GtkWidget *row;

          row = gtk_label_new (shortcuts[i].description);
          gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);

          controller = gtk_shortcut_controller_new ();
          gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller), GTK_SHORTCUT_SCOPE_GLOBAL);
          gtk_widget_add_controller (row, controller);

          shortcut = gtk_shortcut_new (shortcuts[i].create_trigger_func(),
                                       gtk_callback_action_new (shortcut_activated, row, NULL));
          gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
        }
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
