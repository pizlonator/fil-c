#include <gtk/gtk.h>

static void
action_activated (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  GtkWindow *parent = user_data;
  GtkAlertDialog *dialog;

  dialog = gtk_alert_dialog_new ("Activated action `%s`", g_action_get_name (G_ACTION (action)));
  gtk_alert_dialog_show (dialog, NULL);
  g_object_unref (dialog);
}

static GActionEntry doc_entries[] = {
  { "save", action_activated },
  { "print", action_activated },
  { "share", action_activated }
};

static GActionEntry win_entries[] = {
  { "fullscreen", action_activated },
  { "close", action_activated },
};

const char *menu_ui =
  "<interface>"
  "  <menu id='doc-menu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label'>_Save</attribute>"
  "        <attribute name='action'>save</attribute>"
  "      </item>"
  "      <item>"
  "        <attribute name='label'>_Print</attribute>"
  "        <attribute name='action'>print</attribute>"
  "      </item>"
  "      <item>"
  "        <attribute name='label'>_Share</attribute>"
  "        <attribute name='action'>share</attribute>"
  "      </item>"
  "    </section>"
  "  </menu>"
  "  <menu id='win-menu'>"
  "    <section>"
  "      <item>"
  "        <attribute name='label'>_Fullscreen</attribute>"
  "        <attribute name='action'>fullscreen</attribute>"
  "      </item>"
  "      <item>"
  "        <attribute name='label'>_Close</attribute>"
  "        <attribute name='action'>close</attribute>"
  "      </item>"
  "    </section>"
  "  </menu>"
  "</interface>";

static void
activate (GApplication *app,
          gpointer      user_data)
{
  GtkWidget *win;
  GtkWidget *button;
  GSimpleActionGroup *doc_actions;
  GtkBuilder *builder;
  GMenuModel *doc_menu;
  GMenuModel *win_menu;
  GMenu *button_menu;
  GMenuItem *section;

  if (gtk_application_get_windows (GTK_APPLICATION (app)) != NULL)
    return;

  win = gtk_application_window_new (GTK_APPLICATION (app));
  gtk_window_set_default_size (GTK_WINDOW (win), 200, 300);

  doc_actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (doc_actions), doc_entries, G_N_ELEMENTS (doc_entries), win);

  g_action_map_add_action_entries (G_ACTION_MAP (win), win_entries,
                                   G_N_ELEMENTS (win_entries), win);

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, menu_ui, -1, NULL);

  doc_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "doc-menu"));
  win_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "win-menu"));

  button_menu = g_menu_new ();

  section = g_menu_item_new_section (NULL, doc_menu);
  g_menu_item_set_attribute (section, "action-namespace", "s", "doc");
  g_menu_append_item (button_menu, section);
  g_object_unref (section);

  section = g_menu_item_new_section (NULL, win_menu);
  g_menu_item_set_attribute (section, "action-namespace", "s", "win");
  g_menu_append_item (button_menu, section);
  g_object_unref (section);

  button = gtk_menu_button_new ();
  gtk_button_set_label (GTK_BUTTON (button), "Menu");
  gtk_widget_insert_action_group (button, "doc", G_ACTION_GROUP (doc_actions));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (button_menu));
  gtk_widget_set_halign (GTK_WIDGET (button), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (button), GTK_ALIGN_START);
  gtk_window_set_child (GTK_WINDOW (window), button);
  gtk_window_present (GTK_WINDOW (win));

  g_object_unref (button_menu);
  g_object_unref (doc_actions);
  g_object_unref (builder);
}

int
main(int    argc,
     char **argv)
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Example", 0);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
