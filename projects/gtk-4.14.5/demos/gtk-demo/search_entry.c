/* Entry/Search Entry
 *
 * GtkEntry allows to display icons and progress information.
 * This demo shows how to use these features in a search entry.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;
static GtkWidget *notebook = NULL;
static GSimpleActionGroup *actions = NULL;

static guint search_progress_id = 0;
static guint finish_search_id = 0;

static void
show_find_button (void)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
}

static void
show_cancel_button (void)
{
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
}

static gboolean
search_progress (gpointer data)
{
  gtk_entry_progress_pulse (GTK_ENTRY (data));
  return G_SOURCE_CONTINUE;
}

static void
search_progress_done (GtkEntry *entry)
{
  gtk_entry_set_progress_fraction (entry, 0.0);
  g_object_unref (entry);
}

static gboolean
finish_search (GtkButton *button)
{
  show_find_button ();
  if (search_progress_id)
    {
      g_source_remove (search_progress_id);
      search_progress_id = 0;
    }
  return G_SOURCE_REMOVE;
}

static gboolean
start_search_feedback (gpointer data)
{
  gtk_entry_set_progress_fraction (GTK_ENTRY (data), 0.1);
  search_progress_id = g_timeout_add_full (G_PRIORITY_DEFAULT, 100,
                                           (GSourceFunc)search_progress, g_object_ref (data),
                                           (GDestroyNotify)search_progress_done);

  return G_SOURCE_REMOVE;
}

static void
start_search (GtkButton *button,
              GtkEntry  *entry)
{
  show_cancel_button ();
  search_progress_id = g_timeout_add_seconds (1, (GSourceFunc)start_search_feedback, entry);
  finish_search_id = g_timeout_add_seconds (15, (GSourceFunc)finish_search, button);
}


static void
stop_search (GtkButton *button,
             gpointer   data)
{
  if (finish_search_id)
    {
      g_source_remove (finish_search_id);
      finish_search_id = 0;
    }
  finish_search (button);
}

static void
clear_entry (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GtkEditable *editable = user_data;

  gtk_editable_set_text (editable, "");
}

static void
set_search_by (GSimpleAction *action,
               GVariant      *value,
               gpointer       user_data)
{
  GtkEntry *entry = user_data;
  const char *term;

  term = g_variant_get_string (value, NULL);

  g_simple_action_set_state (action, value);

  if (g_str_equal (term, "name"))
    {
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY, "Search by name");
      gtk_entry_set_placeholder_text (entry, "Name…");
    }
  else if (g_str_equal (term, "description"))
    {
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY, "Search by description");
      gtk_entry_set_placeholder_text (entry, "Description…");
    }
  else if (g_str_equal (term, "filename"))
    {
      gtk_entry_set_icon_tooltip_text (entry, GTK_ENTRY_ICON_PRIMARY, "Search by file name");
      gtk_entry_set_placeholder_text (entry, "File name…");
    }
}

static void
icon_press_cb (GtkEntry       *entry,
               int             position,
               gpointer        data)
{
  if (position == GTK_ENTRY_ICON_PRIMARY)
    {
      GAction *action;
      GVariant *state;
      GVariant *new_state;
      const char *s;

      action = g_action_map_lookup_action (G_ACTION_MAP (actions), "search-by");
      state = g_action_get_state (action);
      s = g_variant_get_string (state, NULL);

      if (g_str_equal (s, "name"))
        new_state = g_variant_new_string ("description");
      else if (g_str_equal (s, "description"))
        new_state = g_variant_new_string ("filename");
      else if (g_str_equal (s, "filename"))
        new_state = g_variant_new_string ("name");
      else
        g_assert_not_reached ();

      g_action_change_state (action, new_state);
      g_variant_unref (state);
    }
}

static void
activate_cb (GtkEntry  *entry,
             GtkButton *button)
{
  if (search_progress_id != 0)
    return;

  start_search (button, entry);
}

static void
search_entry_destroyed (gpointer  data,
                        GObject  *widget)
{
  if (finish_search_id != 0)
    {
      g_source_remove (finish_search_id);
      finish_search_id = 0;
    }

  if (search_progress_id != 0)
    {
      g_source_remove (search_progress_id);
      search_progress_id = 0;
    }

  window = NULL;
}

static void
text_changed (GObject *object,
              GParamSpec *pspec,
              gpointer data)
{
  GtkEntry *entry = GTK_ENTRY (object);
  GActionMap *action_map = data;
  GAction *action;
  gboolean has_text;

  has_text = gtk_entry_get_text_length (entry) > 0;

  action = g_action_map_lookup_action (action_map, "clear");
  g_simple_action_set_enabled (G_SIMPLE_ACTION (action), has_text);
}

static GMenuModel *
create_search_menu_model (void)
{
  GMenu *menu = g_menu_new ();
  g_menu_append (menu, _("Name"), "search.search-by::name");
  g_menu_append (menu, _("Description"), "search.search-by::description");
  g_menu_append (menu, _("File Name"), "search.search-by::filename");

  return G_MENU_MODEL (menu);
}

static void
entry_add_to_context_menu (GtkEntry *entry)
{
  GMenu *menu;
  GActionEntry entries[] = {
    { "clear", clear_entry, NULL, NULL, NULL },
    { "search-by", NULL, "s", "'name'", set_search_by }
  };
  GMenuModel *submenu;
  GMenuItem *item;
  GAction *action;
  GVariant *value;

  actions = g_simple_action_group_new ();
  g_action_map_add_action_entries (G_ACTION_MAP (actions), entries, G_N_ELEMENTS(entries), entry);
  gtk_widget_insert_action_group (GTK_WIDGET (entry), "search", G_ACTION_GROUP (actions));

  action = g_action_map_lookup_action (G_ACTION_MAP (actions), "search-by");
  value = g_variant_ref_sink (g_variant_new_string ("name"));
  set_search_by (G_SIMPLE_ACTION (action), value, entry);
  g_variant_unref (value);

  menu = g_menu_new ();
  item = g_menu_item_new (_("C_lear"), "search.clear");
  g_menu_item_set_attribute (item, "touch-icon", "s", "edit-clear-symbolic");
  g_menu_append_item (G_MENU (menu), item);
  g_object_unref (item);

  submenu = create_search_menu_model ();
  g_menu_append_submenu (menu, _("Search By"), submenu);
  g_object_unref (submenu);

  gtk_entry_set_extra_menu (entry, G_MENU_MODEL (menu));

  g_object_unref (menu);

  g_signal_connect (entry, "notify::text", G_CALLBACK (text_changed), actions);
}

GtkWidget *
do_search_entry (GtkWidget *do_widget)
{
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *entry;
  GtkWidget *find_button;
  GtkWidget *cancel_button;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Search Entry");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      g_object_weak_ref (G_OBJECT (window), search_entry_destroyed, &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      gtk_widget_set_margin_start (vbox, 18);
      gtk_widget_set_margin_end (vbox, 18);
      gtk_widget_set_margin_top (vbox, 18);
      gtk_widget_set_margin_bottom (vbox, 18);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
      gtk_box_append (GTK_BOX (vbox), hbox);

      /* Create our entry */
      entry = gtk_entry_new ();
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (entry),
                                         GTK_ENTRY_ICON_PRIMARY,
                                         "edit-find-symbolic");
      gtk_box_append (GTK_BOX (hbox), entry);

      /* Create the find and cancel buttons */
      notebook = gtk_notebook_new ();
      gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
      gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
      gtk_box_append (GTK_BOX (hbox), notebook);

      find_button = gtk_button_new_with_label ("Find");
      g_signal_connect (find_button, "clicked",
                        G_CALLBACK (start_search), entry);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), find_button, NULL);

      cancel_button = gtk_button_new_with_label ("Cancel");
      g_signal_connect (cancel_button, "clicked",
                        G_CALLBACK (stop_search), NULL);
      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), cancel_button, NULL);

      /* Set up the search icon */
      gtk_entry_set_icon_activatable (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, TRUE);
      gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_PRIMARY, TRUE);

      g_signal_connect (entry, "icon-press", G_CALLBACK (icon_press_cb), NULL);
      g_signal_connect (entry, "activate", G_CALLBACK (activate_cb), NULL);

      /* add accessible alternatives for icon functionality */
      entry_add_to_context_menu (GTK_ENTRY (entry));
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    {
      g_clear_object (&actions);
      gtk_window_destroy (GTK_WINDOW (window));
    }

  return window;
}
