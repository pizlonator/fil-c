/* testgmenu.c
 * Copyright (C) 2011  Red Hat, Inc.
 * Written by Matthias Clasen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* TODO
 *
 * - Labeled sections
 *
 * - Focus changes. Verify that stopping subscriptions works.
 *
 * - Other attributes. What about icons ?
 */

/* The example menu {{{1 */

static const char menu_markup[] =
  "<interface>\n"
  "<menu id='edit-menu'>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='action'>actions.undo</attribute>\n"
  "      <attribute name='label' translatable='yes' context='Stock label'>_Undo</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Redo</attribute>\n"
  "      <attribute name='action'>actions.redo</attribute>\n"
  "    </item>\n"
  "  </section>\n"
  "  <section/>\n"
  "  <section>\n"
  "    <attribute name='label' translatable='yes'>Copy &amp; Paste</attribute>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Cut</attribute>\n"
  "      <attribute name='action'>actions.cut</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Copy</attribute>\n"
  "      <attribute name='action'>actions.copy</attribute>\n"
  "    </item>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Paste</attribute>\n"
  "      <attribute name='action'>actions.paste</attribute>\n"
  "    </item>\n"
  "  </section>\n"
  "  <section>\n"
  "    <item>\n"
  "      <attribute name='label' translatable='yes'>Bold</attribute>\n"
  "      <attribute name='action'>actions.bold</attribute>\n"
  "    </item>\n"
  "    <section id=\"size-placeholder\">\n"
  "      <attribute name=\"label\">Size</attribute>"
  "    </section>\n"
  "    <submenu>\n"
  "      <attribute name='label' translatable='yes'>Language</attribute>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Latin</attribute>\n"
  "        <attribute name='action'>actions.lang</attribute>\n"
  "        <attribute name='target'>latin</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Greek</attribute>\n"
  "        <attribute name='action'>actions.lang</attribute>\n"
  "        <attribute name='target'>greek</attribute>\n"
  "      </item>\n"
  "      <item>\n"
  "        <attribute name='label' translatable='yes'>Urdu</attribute>\n"
  "        <attribute name='action'>actions.lang</attribute>\n"
  "        <attribute name='target'>urdu</attribute>\n"
  "      </item>\n"
  "    </submenu>\n"
  "  </section>\n"
  "</menu>\n"
  "</interface>\n";

static GMenuModel *
get_model (void)
{
  GError *error = NULL;
  GtkBuilder *builder;
  GMenuModel *menu, *section;
  float i;

  builder = gtk_builder_new ();
  gtk_builder_add_from_string (builder, menu_markup, -1, &error);
  g_assert_no_error (error);

  menu = G_MENU_MODEL (g_object_ref (gtk_builder_get_object (builder, "edit-menu")));

  section = G_MENU_MODEL (g_object_ref (gtk_builder_get_object (builder, "size-placeholder")));
  g_object_unref (builder);

  for (i = 0.5; i <= 2.0; i += 0.5)
    {
      GMenuItem *item;
      char *target;
      char *label;

      target = g_strdup_printf ("actions.size::%.1f", i);
      label = g_strdup_printf ("x %.1f", i);
      item = g_menu_item_new (label, target);
      g_menu_append_item (G_MENU (section), item);
      g_free (label);
      g_free (target);
    }

  return menu;
}

/* The example actions {{{1 */

static void
activate_action (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  g_print ("Action %s activated\n", g_action_get_name (G_ACTION (action)));
}

static void
activate_toggle (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_boolean (!g_variant_get_boolean (old_state));

  g_print ("Toggle action %s activated, state changes from %d to %d\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_boolean (old_state),
           g_variant_get_boolean (new_state));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static void
activate_radio (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  GVariant *old_state, *new_state;

  old_state = g_action_get_state (G_ACTION (action));
  new_state = g_variant_new_string (g_variant_get_string (parameter, NULL));

  g_print ("Radio action %s activated, state changes from %s to %s\n",
           g_action_get_name (G_ACTION (action)),
           g_variant_get_string (old_state, NULL),
           g_variant_get_string (new_state, NULL));

  g_simple_action_set_state (action, new_state);
  g_variant_unref (old_state);
}

static GActionEntry actions[] = {
  { "undo",  activate_action, NULL, NULL,      NULL },
  { "redo",  activate_action, NULL, NULL,      NULL },
  { "cut",   activate_action, NULL, NULL,      NULL },
  { "copy",  activate_action, NULL, NULL,      NULL },
  { "paste", activate_action, NULL, NULL,      NULL },
  { "bold",  activate_toggle, NULL, "true",    NULL },
  { "lang",  activate_radio,  "s",  "'latin'", NULL },
};

static GActionGroup *
get_group (void)
{
  GSimpleActionGroup *group;

  group = g_simple_action_group_new ();

  g_action_map_add_action_entries (G_ACTION_MAP (group), actions, G_N_ELEMENTS (actions), NULL);

  return G_ACTION_GROUP (group);
}
 
/* The action treeview {{{1 */

static void
enabled_cell_func (GtkTreeViewColumn *column,
                   GtkCellRenderer   *cell,
                   GtkTreeModel      *model,
                   GtkTreeIter       *iter,
                   gpointer           data)
{
  GActionGroup *group = data;
  char *name;
  gboolean enabled;

  gtk_tree_model_get (model, iter, 0, &name, -1);
  enabled = g_action_group_get_action_enabled (group, name);
  g_free (name);

  gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell), enabled);
}

static void
state_cell_func (GtkTreeViewColumn *column,
                 GtkCellRenderer   *cell,
                 GtkTreeModel      *model,
                 GtkTreeIter       *iter,
                 gpointer           data)
{
  GActionGroup *group = data;
  char *name;
  GVariant *state;

  gtk_tree_model_get (model, iter, 0, &name, -1);
  state = g_action_group_get_action_state (group, name);
  g_free (name);

  gtk_cell_renderer_set_visible (cell, FALSE);
  g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);

  if (state == NULL)
    return;

  if (g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN) &&
      GTK_IS_CELL_RENDERER_TOGGLE (cell))
    {
      gtk_cell_renderer_set_visible (cell, TRUE);
      g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);
      gtk_cell_renderer_toggle_set_active (GTK_CELL_RENDERER_TOGGLE (cell),
                                           g_variant_get_boolean (state));
    }
  else if (g_variant_is_of_type (state, G_VARIANT_TYPE_STRING) &&
           GTK_IS_CELL_RENDERER_COMBO (cell))
    {
      gtk_cell_renderer_set_visible (cell, TRUE);
      g_object_set (cell, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
      g_object_set (cell, "text", g_variant_get_string (state, NULL), NULL);
    }

  g_variant_unref (state);
}

static void
enabled_cell_toggled (GtkCellRendererToggle *cell,
                      const char            *path_str,
                      GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  char *name;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean enabled;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);

  enabled = g_action_group_get_action_enabled (group, name);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), !enabled);

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
}

static void
state_cell_toggled (GtkCellRendererToggle *cell,
                    const char            *path_str,
                    GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  char *name;
  GtkTreePath *path;
  GtkTreeIter iter;
  GVariant *state;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);

  state = g_action_group_get_action_state (group, name);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  if (state && g_variant_is_of_type (state, G_VARIANT_TYPE_BOOLEAN))
    {
      gboolean b;

      b = g_variant_get_boolean (state);
      g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_boolean (!b));
    }
  else
    {
      /* nothing to do */
    }

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
  if (state)
    g_variant_unref (state);
}

static void
state_cell_edited (GtkCellRendererCombo  *cell,
                   const char            *path_str,
                   const char            *new_text,
                   GtkTreeModel          *model)
{
  GActionGroup *group;
  GAction *action;
  char *name;
  GtkTreePath *path;
  GtkTreeIter iter;

  group = g_object_get_data (G_OBJECT (model), "group");
  path = gtk_tree_path_new_from_string (path_str);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, 0, &name, -1);
  action = g_action_map_lookup_action (G_ACTION_MAP (group), name);
  g_simple_action_set_state (G_SIMPLE_ACTION (action), g_variant_new_string (new_text));

  gtk_tree_model_row_changed (model, path, &iter);

  g_free (name);
  gtk_tree_path_free (path);
}

static GtkWidget *
create_action_treeview (GActionGroup *group)
{
  GtkWidget *tv;
  GtkListStore *store;
  GtkListStore *values;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell;
  char **group_actions;
  int i;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  group_actions = g_action_group_list_actions (group);
  for (i = 0; group_actions[i]; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, group_actions[i], -1);
    }
  g_strfreev (group_actions);
  g_object_set_data (G_OBJECT (store), "group", group);

  tv = gtk_tree_view_new ();

  g_signal_connect_swapped (group, "action-enabled-changed",
                            G_CALLBACK (gtk_widget_queue_draw), tv);
  g_signal_connect_swapped (group, "action-state-changed",
                            G_CALLBACK (gtk_widget_queue_draw), tv);

  gtk_tree_view_set_model (GTK_TREE_VIEW (tv), GTK_TREE_MODEL (store));

  cell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Action", cell,
                                                     "text", 0,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Enabled");
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, enabled_cell_func, group, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (enabled_cell_toggled), store);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "State");
  cell = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, state_cell_func, group, NULL);
  g_signal_connect (cell, "toggled", G_CALLBACK (state_cell_toggled), store);
  cell = gtk_cell_renderer_combo_new ();
  values = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "latin", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "greek", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "urdu", -1);
  gtk_list_store_append (values, &iter);
  gtk_list_store_set (values, &iter, 0, "sumerian", -1);
  g_object_set (cell,
                "has-entry", FALSE,
                "model", values,
                "text-column", 0,
                "editable", TRUE,
                NULL);
  gtk_tree_view_column_pack_start (column, cell, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, cell, state_cell_func, group, NULL);
  g_signal_connect (cell, "edited", G_CALLBACK (state_cell_edited), store);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tv), column);

  return tv;
}

/* Dynamic menu changes {{{1 */

static void
toggle_sumerian (GtkCheckButton *button, gpointer data)
{
  GMenuModel *model;
  gboolean adding;
  GMenuModel *m;

  model = g_object_get_data (G_OBJECT (button), "model");

  adding = gtk_check_button_get_active (button);

  m = g_menu_model_get_item_link (model, g_menu_model_get_n_items (model) - 1, G_MENU_LINK_SECTION);
  m = g_menu_model_get_item_link (m, g_menu_model_get_n_items (m) - 1, G_MENU_LINK_SUBMENU);
  if (adding)
    g_menu_append (G_MENU (m), "Sumerian", "lang::sumerian");
  else
    g_menu_remove (G_MENU (m), g_menu_model_get_n_items (m) - 1);
}

static void
action_list_add (GtkTreeModel *store,
                 const char   *action)
{
  GtkTreeIter iter;

  gtk_list_store_append (GTK_LIST_STORE (store), &iter);
  gtk_list_store_set (GTK_LIST_STORE (store), &iter, 0, action, -1);
}

static void
action_list_remove (GtkTreeModel *store,
                    const char   *action)
{
  GtkTreeIter iter;
  char *text;

  gtk_tree_model_get_iter_first (store, &iter);
  do {
    gtk_tree_model_get (store, &iter, 0, &text, -1);
    if (g_strcmp0 (action, text) == 0)
      {
        g_free (text);
        gtk_list_store_remove (GTK_LIST_STORE (store), &iter);
        break;
      }
    g_free (text);
  } while (gtk_tree_model_iter_next (store, &iter));
}

static void
toggle_italic (GtkCheckButton *button, gpointer data)
{
  GMenuModel *model;
  GActionGroup *group;
  GSimpleAction *action;
  gboolean adding;
  GMenuModel *m;
  GtkTreeView *tv = data;
  GtkTreeModel *store;

  model = g_object_get_data (G_OBJECT (button), "model");
  group = g_object_get_data (G_OBJECT (button), "group");

  store = gtk_tree_view_get_model (tv);

  adding = gtk_check_button_get_active (button);

  m = g_menu_model_get_item_link (model, g_menu_model_get_n_items (model) - 1, G_MENU_LINK_SECTION);
  if (adding)
    {
      action = g_simple_action_new_stateful ("italic", NULL, g_variant_new_boolean (FALSE));
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_toggle), NULL);
      g_object_unref (action);
      action_list_add (store, "italic");
      g_menu_insert (G_MENU (m), 1, "Italic", "italic");
    }
  else
    {
      g_action_map_remove_action (G_ACTION_MAP (group), "italic");
      action_list_remove (store, "italic");
      g_menu_remove (G_MENU (m), 1);
    }
}

static void
toggle_speed (GtkCheckButton *button, gpointer data)
{
  GMenuModel *model;
  GActionGroup *group;
  GSimpleAction *action;
  gboolean adding;
  GMenuModel *m;
  GMenu *submenu;
  GtkTreeView *tv = data;
  GtkTreeModel *store;

  model = g_object_get_data (G_OBJECT (button), "model");
  group = g_object_get_data (G_OBJECT (button), "group");

  store = gtk_tree_view_get_model (tv);

  adding = gtk_check_button_get_active (button);

  m = g_menu_model_get_item_link (model, 1, G_MENU_LINK_SECTION);
  if (adding)
    {
      action = g_simple_action_new ("faster", NULL);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_action), NULL);
      g_object_unref (action);

      action = g_simple_action_new ("slower", NULL);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_signal_connect (action, "activate", G_CALLBACK (activate_action), NULL);
      g_object_unref (action);

      action_list_add (store, "faster");
      action_list_add (store, "slower");

      submenu = g_menu_new ();
      g_menu_append (submenu, "Faster", "faster");
      g_menu_append (submenu, "Slower", "slower");
      g_menu_append_submenu (G_MENU (m), "Speed", G_MENU_MODEL (submenu));
    }
  else
    {
      g_action_map_remove_action (G_ACTION_MAP (group), "faster");
      g_action_map_remove_action (G_ACTION_MAP (group), "slower");

      action_list_remove (store, "faster");
      action_list_remove (store, "slower");

      g_menu_remove (G_MENU (m), g_menu_model_get_n_items (m) - 1);
    }
}
static GtkWidget *
create_add_remove_buttons (GActionGroup *group,
                           GMenuModel   *model,
                           GtkWidget    *treeview)
{
  GtkWidget *box;
  GtkWidget *button;

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  button = gtk_check_button_new_with_label ("Add Italic");
  gtk_box_append (GTK_BOX (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_italic), treeview);

  button = gtk_check_button_new_with_label ("Add Sumerian");
  gtk_box_append (GTK_BOX (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_sumerian), NULL);

  button = gtk_check_button_new_with_label ("Add Speed");
  gtk_box_append (GTK_BOX (box), button);

  g_object_set_data  (G_OBJECT (button), "group", group);
  g_object_set_data  (G_OBJECT (button), "model", model);

  g_signal_connect (button, "toggled",
                    G_CALLBACK (toggle_speed), treeview);
  return box;
}

/* main {{{1 */

#define BUS_NAME "org.gtk.TestMenus"
#define OBJ_PATH "/org/gtk/TestMenus"

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *button;
  GtkWidget *tv;
  GtkWidget *buttons;
  GMenuModel *model;
  GActionGroup *group;
  GDBusConnection *bus;
  GError *error = NULL;
  gboolean do_export = FALSE;
  gboolean do_import = FALSE;
  GOptionEntry entries[] = {
    { "export", 0, 0, G_OPTION_ARG_NONE, &do_export, "Export actions and menus over D-Bus", NULL },
    { "import", 0, 0, G_OPTION_ARG_NONE, &do_import, "Use exported actions and menus", NULL },
    { NULL, }
  };
  GOptionContext *context;
  gboolean done = FALSE;

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_parse (context, &argc, &argv, NULL);
  gtk_init ();

  if (do_export && do_import)
    {
       g_error ("can't have it both ways\n");
       exit (1);
    }

  window = gtk_window_new ();
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_window_set_child (GTK_WINDOW (window), box);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (do_import)
    {
      g_print ("Getting menus from the bus...\n");
      model = (GMenuModel*)g_dbus_menu_model_get (bus, BUS_NAME, OBJ_PATH);
      g_print ("Getting actions from the bus...\n");
      group = (GActionGroup*)g_dbus_action_group_get (bus, BUS_NAME, OBJ_PATH);
    }
  else
    {
      group = get_group ();
      model = get_model ();

      tv = create_action_treeview (group);
      gtk_box_append (GTK_BOX (box), tv);
      buttons = create_add_remove_buttons (group, model, tv);
      gtk_box_append (GTK_BOX (box), buttons);
    }

  if (do_export)
    {
      g_print ("Exporting menus on the bus...\n");
      if (!g_dbus_connection_export_menu_model (bus, OBJ_PATH, model, &error))
        {
          g_warning ("Menu export failed: %s", error->message);
          exit (1);
        }
      g_print ("Exporting actions on the bus...\n");
      if (!g_dbus_connection_export_action_group (bus, OBJ_PATH, group, &error))
        {
          g_warning ("Action export failed: %s", error->message);
          exit (1);
        }
      g_bus_own_name_on_connection (bus, BUS_NAME, 0, NULL, NULL, NULL, NULL);
    }
  else
    {
      button = gtk_menu_button_new ();
      gtk_menu_button_set_label (GTK_MENU_BUTTON (button), "Click here");
      gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), model);
      gtk_widget_insert_action_group (button, "actions", group);
      gtk_box_append (GTK_BOX (box), button);
    }

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
