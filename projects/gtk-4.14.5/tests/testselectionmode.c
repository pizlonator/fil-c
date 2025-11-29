#include <gtk/gtk.h>
#include <glib/gstdio.h>

typedef struct {
  GtkListBoxRow parent;
  GtkWidget *box;
  GtkWidget *revealer;
  GtkWidget *check;
} SelectableRow;

typedef struct {
  GtkListBoxRowClass parent_class;
} SelectableRowClass;

static GType selectable_row_get_type (void);
G_DEFINE_TYPE (SelectableRow, selectable_row, GTK_TYPE_LIST_BOX_ROW)

static void
selectable_row_init (SelectableRow *row)
{
  row->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);  
  row->revealer = gtk_revealer_new ();
  gtk_revealer_set_transition_type (GTK_REVEALER (row->revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
  row->check = gtk_check_button_new ();
  gtk_widget_set_margin_start (row->check, 10);
  gtk_widget_set_margin_end (row->check, 10);
  gtk_widget_set_margin_top (row->check, 10);
  gtk_widget_set_margin_bottom (row->check, 10);

  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), row->box);
  gtk_box_append (GTK_BOX (row->box), row->revealer);
  gtk_revealer_set_child (GTK_REVEALER (row->revealer), row->check);
}

static void
selectable_row_add (SelectableRow *row, GtkWidget *child)
{
  gtk_box_append (GTK_BOX (row->box), child);
}

static void
update_selectable (GtkWidget *widget)
{
  SelectableRow *row = (SelectableRow *)widget;
  GtkListBox *list;

  list = GTK_LIST_BOX (gtk_widget_get_parent (widget));

  if (gtk_list_box_get_selection_mode (list) != GTK_SELECTION_NONE)
    gtk_revealer_set_reveal_child (GTK_REVEALER (row->revealer), TRUE);
  else
    gtk_revealer_set_reveal_child (GTK_REVEALER (row->revealer), FALSE);
}

static void
update_selected (GtkWidget *widget)
{
  SelectableRow *row = (SelectableRow *)widget;

  if (gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (row)))
    {
      gtk_check_button_set_active (GTK_CHECK_BUTTON (row->check), TRUE);
      gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_SELECTED);
    }
  else
    gtk_check_button_set_active (GTK_CHECK_BUTTON (row->check), FALSE);
}

static void
selectable_row_class_init (SelectableRowClass *class)
{
}

static GtkWidget *
selectable_row_new (void)
{
  return GTK_WIDGET (g_object_new (selectable_row_get_type (), NULL));
}

static void
add_row (GtkWidget *list, int i)
{
  GtkWidget *row;
  GtkWidget *label;
  char *text;

  row = selectable_row_new ();
  text = g_strdup_printf ("Docker %d", i);
  label = gtk_label_new (text);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  selectable_row_add ((SelectableRow*)row, label);
  g_free (text);

  gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);
}

static void
selection_mode_enter (GtkButton *button, GtkBuilder *builder)
{
  GtkWidget *header;
  GtkWidget *list;
  GtkWidget *headerbutton;
  GtkWidget *cancelbutton;
  GtkWidget *selectbutton;
  GtkWidget *titlestack;
  GtkWidget *child;

  header = GTK_WIDGET (gtk_builder_get_object (builder, "header"));
  list = GTK_WIDGET (gtk_builder_get_object (builder, "list"));
  headerbutton = GTK_WIDGET (gtk_builder_get_object (builder, "headerbutton"));
  cancelbutton = GTK_WIDGET (gtk_builder_get_object (builder, "cancel-button"));
  selectbutton = GTK_WIDGET (gtk_builder_get_object (builder, "select-button"));
  titlestack = GTK_WIDGET (gtk_builder_get_object (builder, "titlestack"));
 
  gtk_widget_add_css_class (header, "selection-mode");
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
  gtk_widget_set_visible (headerbutton, FALSE);
  gtk_widget_set_visible (selectbutton, FALSE);
  gtk_widget_set_visible (cancelbutton, TRUE);
  gtk_stack_set_visible_child_name (GTK_STACK (titlestack), "selection");

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list), FALSE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_MULTIPLE);
  for (child = gtk_widget_get_first_child (list);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    update_selectable (child);
}

static void
selection_mode_leave (GtkButton *button, GtkBuilder *builder)
{
  GtkWidget *header;
  GtkWidget *list;
  GtkWidget *headerbutton;
  GtkWidget *cancelbutton;
  GtkWidget *selectbutton;
  GtkWidget *titlestack;
  GtkWidget *child;

  header = GTK_WIDGET (gtk_builder_get_object (builder, "header"));
  list = GTK_WIDGET (gtk_builder_get_object (builder, "list"));
  headerbutton = GTK_WIDGET (gtk_builder_get_object (builder, "headerbutton"));
  cancelbutton = GTK_WIDGET (gtk_builder_get_object (builder, "cancel-button"));
  selectbutton = GTK_WIDGET (gtk_builder_get_object (builder, "select-button"));
  titlestack = GTK_WIDGET (gtk_builder_get_object (builder, "titlestack"));

  gtk_widget_remove_css_class (header, "selection-mode");
  gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
  gtk_widget_set_visible (headerbutton, TRUE);
  gtk_widget_set_visible (selectbutton, TRUE);
  gtk_widget_set_visible (cancelbutton, FALSE);
  gtk_stack_set_visible_child_name (GTK_STACK (titlestack), "title");

  gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list), TRUE);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
  for (child = gtk_widget_get_first_child (list);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    update_selectable (child);
}

static void
select_all (GAction *action, GVariant *param, GtkWidget *list)
{
  gtk_list_box_select_all (GTK_LIST_BOX (list));
}

static void
select_none (GAction *action, GVariant *param, GtkWidget *list)
{
  gtk_list_box_unselect_all (GTK_LIST_BOX (list));
}

static void
selected_rows_changed (GtkListBox *list)
{
  GtkWidget *child;

  for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    update_selected (child);
}

int
main (int argc, char *argv[])
{
  GtkBuilder *builder;
  GtkWidget *window;
  GtkWidget *list;
  GtkWidget *button;
  int i;
  GSimpleActionGroup *group;
  GSimpleAction *action;

#ifdef GTK_SRCDIR
  g_chdir (GTK_SRCDIR);
#endif

  gtk_init ();

  builder = gtk_builder_new_from_file ("selectionmode.ui");
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  list = GTK_WIDGET (gtk_builder_get_object (builder, "list"));

  group = g_simple_action_group_new ();
  action = g_simple_action_new ("select-all", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (select_all), list);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));

  action = g_simple_action_new ("select-none", NULL);
  g_signal_connect (action, "activate", G_CALLBACK (select_none), list);
  g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));

  gtk_widget_insert_action_group (window, "win", G_ACTION_GROUP (group));

  for (i = 0; i < 10; i++)
    add_row (list, i);

  button = GTK_WIDGET (gtk_builder_get_object (builder, "select-button"));
  g_signal_connect (button, "clicked", G_CALLBACK (selection_mode_enter), builder);
  button = GTK_WIDGET (gtk_builder_get_object (builder, "cancel-button"));
  g_signal_connect (button, "clicked", G_CALLBACK (selection_mode_leave), builder);

  g_signal_connect (list, "selected-rows-changed", G_CALLBACK (selected_rows_changed), NULL);

  gtk_window_present (GTK_WINDOW (window));
 
  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
