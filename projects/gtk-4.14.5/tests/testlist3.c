#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GdkContentProvider *
prepare (GtkDragSource *source,
         double         x,
         double         y,
         GtkWidget     *row)
{
  return gdk_content_provider_new_typed (GTK_TYPE_LIST_BOX_ROW, row);
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag,
            GtkWidget      *widget)
{
  GtkWidget *row;
  GtkAllocation alloc;
  GdkPaintable *paintable;
  double x, y;

  row = gtk_widget_get_ancestor (widget, GTK_TYPE_LIST_BOX_ROW);
  gtk_widget_get_allocation (row, &alloc);

  paintable = gtk_widget_paintable_new (row);
  gtk_widget_translate_coordinates (widget, row, 0, 0, &x, &y);
  gtk_drag_source_set_icon (source, paintable, -x, -y);

  g_object_unref (paintable);
}

static gboolean
drag_drop (GtkDropTarget *dest,
           const GValue  *value,
           double         x,
           double         y,
           gpointer       data)
{
  GtkWidget *target = data;
  GtkWidget *source;
  int pos;

  source = g_value_get_object (value);
  if (source == NULL)
    return FALSE;

  pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (target));
  if (source == target)
    return FALSE;

  g_object_ref (source);
  gtk_box_remove (GTK_BOX (gtk_widget_get_parent (source)), source);
  gtk_list_box_insert (GTK_LIST_BOX (gtk_widget_get_parent (target)), source, pos);
  g_object_unref (source);

  return TRUE;
}

static GtkWidget *
create_row (const char *text)
{
  GtkWidget *row, *box, *label, *image;
  GtkDragSource *source;
  GtkDropTarget *dest;

  row = gtk_list_box_row_new (); 
  image = gtk_image_new_from_icon_name ("open-menu-symbolic");
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  g_object_set (box, "margin-start", 10, "margin-end", 10, NULL);
  label = gtk_label_new (text);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_box_append (GTK_BOX (box), label);
  gtk_box_append (GTK_BOX (box), image);

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), image);
  g_signal_connect (source, "prepare", G_CALLBACK (prepare), row);
  gtk_widget_add_controller (image, GTK_EVENT_CONTROLLER (source));

  dest = gtk_drop_target_new (GTK_TYPE_LIST_BOX_ROW, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drop", G_CALLBACK (drag_drop), row);
  gtk_widget_add_controller (GTK_WIDGET (row), GTK_EVENT_CONTROLLER (dest));

  return row;
}

static void
on_row_activated (GtkListBox *self,
                  GtkWidget  *child)
{
  const char *id;
  id = g_object_get_data (G_OBJECT (gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (child))), "id");
  g_message ("Row activated %p: %s", child, id);
}

static void
on_selected_children_changed (GtkListBox *self)
{
  g_message ("Selection changed");
}

static void
selection_mode_changed (GtkComboBox *combo, gpointer data)
{
  GtkListBox *list = data;

  gtk_list_box_set_selection_mode (list, gtk_combo_box_get_active (combo));
}

static const char *css =
  ".during-dnd { "
  "  background: white; "
  "  border: 1px solid black; "
  "}";

int
main (int argc, char *argv[])
{
  GtkWidget *window, *list, *sw, *row;
  GtkWidget *hbox, *vbox, *combo, *button;
  int i;
  char *text;
  GtkCssProvider *provider;

  gtk_init ();

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (provider), 800);
  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), -1, 300);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_window_set_child (GTK_WINDOW (window), hbox);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_set_margin_start (vbox, 12);
  gtk_widget_set_margin_end (vbox, 12);
  gtk_widget_set_margin_top (vbox, 12);
  gtk_widget_set_margin_bottom (vbox, 12);
  gtk_box_append (GTK_BOX (hbox), vbox);

  list = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);

  g_signal_connect (list, "row-activated", G_CALLBACK (on_row_activated), NULL);
  g_signal_connect (list, "selected-rows-changed", G_CALLBACK (on_selected_children_changed), NULL);

  sw = gtk_scrolled_window_new ();
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_box_append (GTK_BOX (hbox), sw);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), list);

  button = gtk_check_button_new_with_label ("Activate on single click");
  g_object_bind_property (list, "activate-on-single-click",
                          button, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_box_append (GTK_BOX (vbox), button);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "None");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Single");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Browse");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Multiple");
  g_signal_connect (combo, "changed", G_CALLBACK (selection_mode_changed), list);
  gtk_box_append (GTK_BOX (vbox), combo);
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), gtk_list_box_get_selection_mode (GTK_LIST_BOX (list)));

  for (i = 0; i < 20; i++)
    {
      text = g_strdup_printf ("Row %d", i);
      row = create_row (text);
      gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
    }

  gtk_window_present (GTK_WINDOW (window));

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
