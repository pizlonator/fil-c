/* Builder
 * #Keywords: GMenu, GtkPopoverMenuBar, GtkBuilder, GtkShortcutController, toolbar
 *
 * Demonstrates a traditional interface, loaded from a XML description,
 * and shows how to connect actions to the menu items and toolbar buttons.
 */

#include <gtk/gtk.h>

static void
quit_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *window = user_data;

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
about_activate (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkWidget *window = user_data;
  GtkWidget *about_dlg;

  about_dlg = GTK_WIDGET (g_object_get_data (G_OBJECT (window), "about"));
  gtk_window_present (GTK_WINDOW (about_dlg));
}

static void
remove_timeout (gpointer data)
{
  guint id = GPOINTER_TO_UINT (data);

  g_source_remove (id);
}

static int
pop_message (gpointer data)
{
  GtkWidget *status = data;

  gtk_label_set_label (GTK_LABEL (status), "");
  g_object_set_data (G_OBJECT (status), "timeout", GUINT_TO_POINTER (0));

  return G_SOURCE_REMOVE;
}

static void
status_message (GtkWidget  *status,
                const char *text)
{
  guint id;

  id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (status), "timeout"));
  if (id)
    g_source_remove (id);

  gtk_label_set_text (GTK_LABEL (status), text);

  id = g_timeout_add (5000, pop_message, status);

  g_object_set_data_full (G_OBJECT (status), "timeout", GUINT_TO_POINTER (id), remove_timeout);
}

static void
help_activate (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  GtkWidget *status;

  status = GTK_WIDGET (g_object_get_data (G_OBJECT (user_data), "status"));
  status_message (status, "Help not available");
}

static void
not_implemented (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  GtkWidget *status;
  char *text;

  text = g_strdup_printf ("Action “%s” not implemented", g_action_get_name (G_ACTION (action)));
  status = GTK_WIDGET (g_object_get_data (G_OBJECT (user_data), "status"));
  status_message (status, text);
  g_free (text);
}

static GActionEntry win_entries[] = {
  { "new", not_implemented, NULL, NULL, NULL },
  { "open", not_implemented, NULL, NULL, NULL },
  { "save", not_implemented, NULL, NULL, NULL },
  { "save-as", not_implemented, NULL, NULL, NULL },
  { "copy", not_implemented, NULL, NULL, NULL },
  { "cut", not_implemented, NULL, NULL, NULL },
  { "paste", not_implemented, NULL, NULL, NULL },
  { "quit", quit_activate, NULL, NULL, NULL },
  { "about", about_activate, NULL, NULL, NULL },
  { "help", help_activate, NULL, NULL, NULL }
};

GtkWidget *
do_builder (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GActionGroup *actions;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *about;
      GtkWidget *status;
      GtkEventController *controller;

      builder = gtk_builder_new_from_resource ("/builder/demo.ui");

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      actions = (GActionGroup*)g_simple_action_group_new ();
      g_action_map_add_action_entries (G_ACTION_MAP (actions),
                                       win_entries, G_N_ELEMENTS (win_entries),
                                       window);
      gtk_widget_insert_action_group (window, "win", actions);

      controller = gtk_shortcut_controller_new ();
      gtk_shortcut_controller_set_scope (GTK_SHORTCUT_CONTROLLER (controller),
                                         GTK_SHORTCUT_SCOPE_GLOBAL);
      gtk_widget_add_controller (window, controller);
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_n, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.new")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_o, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.open")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_s, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.save")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_s, GDK_CONTROL_MASK|GDK_SHIFT_MASK),
                             gtk_named_action_new ("win.save-as")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_q, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.quit")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_c, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.copy")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_x, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.cut")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_v, GDK_CONTROL_MASK),
                             gtk_named_action_new ("win.paste")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F1, 0),
                             gtk_named_action_new ("win.help")));
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller),
           gtk_shortcut_new (gtk_keyval_trigger_new (GDK_KEY_F7, 0),
                             gtk_named_action_new ("win.about")));

      about = GTK_WIDGET (gtk_builder_get_object (builder, "aboutdialog1"));
      gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (window));
      gtk_window_set_hide_on_close (GTK_WINDOW (about), TRUE);
      g_object_set_data_full (G_OBJECT (window), "about",
                              about, (GDestroyNotify)gtk_window_destroy);

      status = GTK_WIDGET (gtk_builder_get_object (builder, "statusbar1"));
      g_object_set_data (G_OBJECT (window), "status", status);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
