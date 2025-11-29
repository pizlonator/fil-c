/* Entry/Type to Search
 *
 * GtkSearchEntry provides an entry that is ready for search.
 *
 * Search entries have their "search-changed" signal delayed and
 * should be used when the search operation is slow, such as big
 * datasets to search, or online searches.
 *
 * GtkSearchBar allows have a hidden search entry that 'springs
 * into action' upon keyboard input.
 */

#include <gtk/gtk.h>

static void
search_changed_cb (GtkSearchEntry *entry,
                   GtkLabel       *result_label)
{
  const char *text;
  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  gtk_label_set_text (result_label, text ? text : "");
}

GtkWidget *
do_search_entry2 (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *searchbar;
  GtkWidget *button;
  GtkWidget *header;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Type to Search");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_widget_set_size_request (window, 200, -1);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), vbox);

      entry = gtk_search_entry_new ();
      gtk_widget_set_halign (entry, GTK_ALIGN_CENTER);
      searchbar = gtk_search_bar_new ();
      gtk_search_bar_connect_entry (GTK_SEARCH_BAR (searchbar), GTK_EDITABLE (entry));
      gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (searchbar), FALSE);
      gtk_search_bar_set_child (GTK_SEARCH_BAR (searchbar), entry);
      gtk_box_append (GTK_BOX (vbox), searchbar);

      /* Hook the search bar to key presses */
      gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (searchbar), window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 18);
      gtk_widget_set_margin_start (box, 18);
      gtk_widget_set_margin_end (box, 18);
      gtk_widget_set_margin_top (box, 18);
      gtk_widget_set_margin_bottom (box, 18);
      gtk_box_append (GTK_BOX (vbox), box);

      /* Toggle button */
      button = gtk_toggle_button_new ();
      gtk_button_set_icon_name (GTK_BUTTON (button), "system-search-symbolic");
      g_object_bind_property (button, "active",
                              searchbar, "search-mode-enabled",
                              G_BINDING_BIDIRECTIONAL);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      /* Result */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_append (GTK_BOX (box), hbox);

      label = gtk_label_new ("Searching for:");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_box_append (GTK_BOX (hbox), label);

      label = gtk_label_new ("");
      gtk_box_append (GTK_BOX (hbox), label);

      g_signal_connect (entry, "search-changed",
                        G_CALLBACK (search_changed_cb), label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
