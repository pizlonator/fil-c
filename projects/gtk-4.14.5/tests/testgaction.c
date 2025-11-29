#include <gtk/gtk.h>

GtkWidget *label;


static void
change_label_button (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  gtk_label_set_label (GTK_LABEL (label), "Text set from button");
}

static void
normal_menu_item (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  gtk_label_set_label (GTK_LABEL (label), "Text set from normal menu item");
}

static void
toggle_menu_item (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  GVariant *state = g_action_get_state (G_ACTION (action));

  gtk_label_set_label (GTK_LABEL (label), "Text set from toggle menu item");

  g_simple_action_set_state (action, g_variant_new_boolean (!g_variant_get_boolean (state)));

  g_variant_unref (state);
}

static void
submenu_item (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  gtk_label_set_label (GTK_LABEL (label), "Text set from submenu item");
}

static void
radio (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  char *str;

  str = g_strdup_printf ("From Radio menu item %s",
                         g_variant_get_string (parameter, NULL));

  gtk_label_set_label (GTK_LABEL (label), str);

  g_free (str);

  g_simple_action_set_state (action, parameter);
}



static const GActionEntry win_actions[] = {
  { "change-label-button", change_label_button, NULL, NULL, NULL },
  { "normal-menu-item",    normal_menu_item,    NULL, NULL, NULL },
  { "toggle-menu-item",    toggle_menu_item,    NULL, "true", NULL },
  { "submenu-item",        submenu_item,        NULL, NULL, NULL },
  { "radio",               radio,               "s", "'1'", NULL },
};


static const char *menu_data =
  "<interface>"
  "  <menu id=\"menu_model\">"
  "    <section>"
  "      <item>"
  "        <attribute name=\"label\">Normal Menu Item</attribute>"
  "        <attribute name=\"action\">win.normal-menu-item</attribute>"
  "      </item>"
  "      <submenu>"
  "        <attribute name=\"label\">Submenu</attribute>"
  "        <item>"
  "          <attribute name=\"label\">Submenu Item</attribute>"
  "          <attribute name=\"action\">win.submenu-item</attribute>"
  "        </item>"
  "      </submenu>"
  "      <item>"
  "        <attribute name=\"label\">Toggle Menu Item</attribute>"
  "        <attribute name=\"action\">win.toggle-menu-item</attribute>"
  "      </item>"
  "    </section>"
  "    <section>"
  "      <item>"
  "        <attribute name=\"label\">Radio 1</attribute>"
  "        <attribute name=\"action\">win.radio</attribute>"
  "        <attribute name=\"target\">1</attribute>"
  "      </item>"
  "      <item>"
  "        <attribute name=\"label\">Radio 2</attribute>"
  "        <attribute name=\"action\">win.radio</attribute>"
  "        <attribute name=\"target\">2</attribute>"
  "      </item>"
  "      <item>"
  "        <attribute name=\"label\">Radio 3</attribute>"
  "        <attribute name=\"action\">win.radio</attribute>"
  "        <attribute name=\"target\">3</attribute>"
  "      </item>"
  "    </section>"
  "  </menu>"
  "</interface>"
;


int main (int argc, char **argv)
{
  gtk_init ();
  GtkWidget *window = gtk_window_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  GtkWidget *menubutton = gtk_menu_button_new ();
  GtkWidget *button1 = gtk_button_new_with_label ("Change Label Text");
  GtkWidget *menu;
  GSimpleActionGroup *action_group;
  GtkWidget *box1;


  action_group = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (action_group),
                                   win_actions,
                                   G_N_ELEMENTS (win_actions),
                                   NULL);

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (action_group));


  label = gtk_label_new ("Initial Text");
  gtk_widget_set_margin_top (label, 12);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_append (GTK_BOX (box), label);
  gtk_widget_set_halign (menubutton, GTK_ALIGN_CENTER);
  {
    GMenuModel *menu_model;
    GtkBuilder *builder = gtk_builder_new_from_string (menu_data, -1);
    menu_model = G_MENU_MODEL (gtk_builder_get_object (builder, "menu_model"));

    menu = gtk_popover_menu_new_from_model (menu_model);

  }
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (menubutton), menu);
  gtk_box_append (GTK_BOX (box), menubutton);
  gtk_widget_set_halign (button1, GTK_ALIGN_CENTER);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button1), "win.change-label-button");
  gtk_box_append (GTK_BOX (box), button1);

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  button1 = gtk_toggle_button_new_with_label ("Toggle");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button1), "win.toggle-menu-item");
  gtk_box_append (GTK_BOX (box1), button1);

  button1 = gtk_check_button_new_with_label ("Check");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button1), "win.toggle-menu-item");
  gtk_box_append (GTK_BOX (box1), button1);

  gtk_box_append (GTK_BOX (box), box1);

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  button1 = gtk_toggle_button_new_with_label ("Radio 1");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::1");
  gtk_box_append (GTK_BOX (box1), button1);

  button1 = gtk_toggle_button_new_with_label ("Radio 2");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::2");
  gtk_box_append (GTK_BOX (box1), button1);

  button1 = gtk_toggle_button_new_with_label ("Radio 3");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::3");
  gtk_box_append (GTK_BOX (box1), button1);

  gtk_box_append (GTK_BOX (box), box1);

  box1 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  button1 = gtk_check_button_new_with_label ("Radio 1");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::1");
  gtk_box_append (GTK_BOX (box1), button1);

  button1 = gtk_check_button_new_with_label ("Radio 2");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::2");
  gtk_box_append (GTK_BOX (box1), button1);

  button1 = gtk_check_button_new_with_label ("Radio 3");
  gtk_actionable_set_detailed_action_name (GTK_ACTIONABLE (button1), "win.radio::3");
  gtk_box_append (GTK_BOX (box1), button1);

  gtk_box_append (GTK_BOX (box), box1);

  gtk_window_set_child (GTK_WINDOW (window), box);

  gtk_window_present (GTK_WINDOW (window));
  while (TRUE)
    g_main_context_iteration (NULL, TRUE);
  return 0;
}
