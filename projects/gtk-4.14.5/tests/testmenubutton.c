#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#define INITIAL_HALIGN          GTK_ALIGN_START
#define INITIAL_VALIGN          GTK_ALIGN_START

static GList *menubuttons = NULL;

static void
horizontal_alignment_changed (GtkComboBox *box)
{
	GtkAlign alignment = gtk_combo_box_get_active (box);
	GList *l;

	for (l = menubuttons; l != NULL; l = l->next) {
		GtkPopover *popup = gtk_menu_button_get_popover (GTK_MENU_BUTTON (l->data));
		if (popup != NULL)
			gtk_widget_set_halign (GTK_WIDGET (popup), alignment);
	}
}

static void
vertical_alignment_changed (GtkComboBox *box)
{
	GtkAlign alignment = gtk_combo_box_get_active (box);
	GList *l;

	for (l = menubuttons; l != NULL; l = l->next) {
		GtkPopover *popup = gtk_menu_button_get_popover (GTK_MENU_BUTTON (l->data));
		if (popup != NULL)
			gtk_widget_set_valign (GTK_WIDGET (popup), alignment);
	}
}

int main (int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *grid;
	GtkWidget *entry;
	GtkWidget *label;
	GtkWidget *combo;
	guint i;
	guint row = 0;
	GMenu *menu;

	gtk_init ();

	window = gtk_window_new ();
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_window_set_child (GTK_WINDOW (window), grid);

	/* horizontal alignment */
	label = gtk_label_new ("Horizontal Alignment:");
	gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 1, 1);

	combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Fill");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Start");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "End");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Center");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Baseline");
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), INITIAL_HALIGN);
	gtk_grid_attach_next_to (GTK_GRID (grid), combo, label, GTK_POS_RIGHT, 1, 1);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (horizontal_alignment_changed), menubuttons);

	/* vertical alignment */
	label = gtk_label_new ("Vertical Alignment:");
	gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 1, 1);

	combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Fill");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Start");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "End");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Center");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Baseline");
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), INITIAL_HALIGN);
	gtk_grid_attach_next_to (GTK_GRID (grid), combo, label, GTK_POS_RIGHT, 1, 1);
	g_signal_connect (G_OBJECT (combo), "changed",
			  G_CALLBACK (vertical_alignment_changed), menubuttons);

	/* Button next to entry */
	entry = gtk_entry_new ();
	gtk_grid_attach (GTK_GRID (grid), entry, 0, row++, 1, 1);
	button = gtk_menu_button_new ();
	gtk_widget_set_halign (button, GTK_ALIGN_START);

	gtk_grid_attach_next_to (GTK_GRID (grid), button, entry, GTK_POS_RIGHT, 1, 1);
	menubuttons = g_list_prepend (menubuttons, button);

	/* Button with GMenuModel */
	menu = g_menu_new ();
	for (i = 5; i > 0; i--) {
		char *item_label;
                GMenuItem *item;
		item_label = g_strdup_printf ("Item _%d", i);
                item = g_menu_item_new (item_label, NULL);
                if (i == 3)
                  g_menu_item_set_attribute (item, "icon", "s", "preferences-desktop-locale-symbolic");
		g_menu_insert_item (menu, 0, item);
                g_object_unref (item);
		g_free (item_label);
	}

	button = gtk_menu_button_new ();

	gtk_widget_set_halign (button, GTK_ALIGN_START);
	menubuttons = g_list_prepend (menubuttons, button);
	gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (button), G_MENU_MODEL (menu));
	gtk_grid_attach (GTK_GRID (grid), button, 1, row++, 1, 1);

	gtk_window_present (GTK_WINDOW (window));

        while (TRUE)
                g_main_context_iteration (NULL, TRUE);

	return 0;
}
