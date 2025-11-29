#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/*******************************************************
 *                      Simple Test                    *
 *******************************************************/
enum {
  SIMPLE_COLUMN_NAME,
  SIMPLE_COLUMN_ICON,
  SIMPLE_COLUMN_DESCRIPTION,
  N_SIMPLE_COLUMNS
};

static GtkCellRenderer *cell_1 = NULL, *cell_2 = NULL, *cell_3 = NULL;

static GtkTreeModel *
simple_list_model (void)
{
  GtkTreeIter   iter;
  GtkListStore *store = 
    gtk_list_store_new (N_SIMPLE_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_STRING,  /* icon name */
			G_TYPE_STRING); /* description text */

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Alice in wonderland",
		      SIMPLE_COLUMN_ICON, "system-run",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Twas brillig, and the slithy toves "
		      "did gyre and gimble in the wabe; "
		      "all mimsy were the borogoves, "
		      "and the mome raths outgrabe",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Marry Poppins",
		      SIMPLE_COLUMN_ICON, "dialog-information",
		      SIMPLE_COLUMN_DESCRIPTION, "Supercalifragilisticexpialidocious",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "George Bush",
		      SIMPLE_COLUMN_ICON, "dialog-warning",
		      SIMPLE_COLUMN_DESCRIPTION, "It's a very good question, very direct, "
		      "and I'm not going to answer it",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Whinnie the pooh",
		      SIMPLE_COLUMN_ICON, "process-stop",
		      SIMPLE_COLUMN_DESCRIPTION, "The most wonderful thing about tiggers, "
		      "is tiggers are wonderful things",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Aleister Crowley",
		      SIMPLE_COLUMN_ICON, "help-about",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Thou shalt do what thou wilt shall be the whole of the law",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      SIMPLE_COLUMN_NAME, "Mark Twain",
		      SIMPLE_COLUMN_ICON, "application-exit",
		      SIMPLE_COLUMN_DESCRIPTION, 
		      "Giving up smoking is the easiest thing in the world. "
		      "I know because I've done it thousands of times.",
		      -1);


  return (GtkTreeModel *)store;
}

static GtkWidget *
simple_iconview (void)
{
  GtkTreeModel *model;
  GtkWidget *iconview;
  GtkCellArea *area;
  GtkCellRenderer *renderer;

  iconview = gtk_icon_view_new ();

  model = simple_list_model ();

  gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), model);
  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (iconview), GTK_ORIENTATION_HORIZONTAL);

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));

  cell_1 = renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_NAME);

  cell_2 = renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "icon-name", SIMPLE_COLUMN_ICON);

  cell_3 = renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 215,
		NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", SIMPLE_COLUMN_DESCRIPTION);

  return iconview;
}

static void
orientation_changed (GtkComboBox      *combo,
		     GtkIconView *iconview)
{
  GtkOrientation orientation = gtk_combo_box_get_active (combo);

  gtk_icon_view_set_item_orientation (iconview, orientation);
}

static void
align_cell_2_toggled (GtkCheckButton  *toggle,
		      GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     align = gtk_check_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "align", align, NULL);
}

static void
align_cell_3_toggled (GtkCheckButton  *toggle,
		      GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     align = gtk_check_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "align", align, NULL);
}

static void
expand_cell_1_toggled (GtkCheckButton  *toggle,
		       GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     expand = gtk_check_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_1, "expand", expand, NULL);
}

static void
expand_cell_2_toggled (GtkCheckButton  *toggle,
		       GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     expand = gtk_check_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_2, "expand", expand, NULL);
}

static void
expand_cell_3_toggled (GtkCheckButton  *toggle,
		       GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     expand = gtk_check_button_get_active (toggle);

  gtk_cell_area_cell_set (area, cell_3, "expand", expand, NULL);
}

static void
simple_cell_area (void)
{
  GtkWidget *window, *widget;
  GtkWidget *iconview, *frame, *vbox, *hbox;

  window = gtk_window_new ();

  gtk_window_set_title (GTK_WINDOW (window), "CellArea expand and alignments");

  iconview = simple_iconview ();

  hbox  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  frame = gtk_frame_new (NULL);
  gtk_widget_set_hexpand (frame, TRUE);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_frame_set_child (GTK_FRAME (frame), iconview);

  /* Now add some controls */
  vbox  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append (GTK_BOX (hbox), vbox);

  gtk_box_append (GTK_BOX (hbox), frame);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), iconview);

  widget = gtk_check_button_new_with_label ("Align 2nd Cell");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), FALSE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_2_toggled), iconview);

  widget = gtk_check_button_new_with_label ("Align 3rd Cell");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), TRUE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (align_cell_3_toggled), iconview);


  widget = gtk_check_button_new_with_label ("Expand 1st Cell");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), FALSE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_1_toggled), iconview);

  widget = gtk_check_button_new_with_label ("Expand 2nd Cell");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), TRUE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_2_toggled), iconview);

  widget = gtk_check_button_new_with_label ("Expand 3rd Cell");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), FALSE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (expand_cell_3_toggled), iconview);

  gtk_window_set_child (GTK_WINDOW (window), hbox);

  gtk_window_present (GTK_WINDOW (window));
}

/*******************************************************
 *                      Focus Test                     *
 *******************************************************/
static GtkCellRenderer *focus_renderer, *sibling_renderer;

enum {
  FOCUS_COLUMN_NAME,
  FOCUS_COLUMN_CHECK,
  FOCUS_COLUMN_STATIC_TEXT,
  N_FOCUS_COLUMNS
};

static GtkTreeModel *
focus_list_model (void)
{
  GtkTreeIter   iter;
  GtkListStore *store = 
    gtk_list_store_new (N_FOCUS_COLUMNS,
			G_TYPE_STRING,  /* name text */
			G_TYPE_BOOLEAN, /* check */
			G_TYPE_STRING); /* static text */

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Enter a string",
		      FOCUS_COLUMN_CHECK, TRUE,
		      FOCUS_COLUMN_STATIC_TEXT, "Does it fly ?",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Enter a string",
		      FOCUS_COLUMN_CHECK, FALSE,
		      FOCUS_COLUMN_STATIC_TEXT, "Would you put it in a toaster ?",
		      -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 
		      FOCUS_COLUMN_NAME, "Type something",
		      FOCUS_COLUMN_CHECK, FALSE,
		      FOCUS_COLUMN_STATIC_TEXT, "Does it feed on cute kittens ?",
		      -1);

  return (GtkTreeModel *)store;
}

static void
cell_toggled (GtkCellRendererToggle *cell_renderer,
	      const char            *path,
	      GtkIconView      *iconview)
{
  GtkTreeModel *model = gtk_icon_view_get_model (iconview);
  GtkTreeIter   iter;
  gboolean      active;

  g_print ("Cell toggled !\n");

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter, FOCUS_COLUMN_CHECK, &active, -1);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter, FOCUS_COLUMN_CHECK, !active, -1);
}

static void
cell_edited (GtkCellRendererToggle *cell_renderer,
	     const char            *path,
	     const char            *new_text,
	     GtkIconView      *iconview)
{
  GtkTreeModel *model = gtk_icon_view_get_model (iconview);
  GtkTreeIter   iter;

  g_print ("Cell edited with new text '%s' !\n", new_text);

  if (!gtk_tree_model_get_iter_from_string (model, &iter, path))
    return;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter, FOCUS_COLUMN_NAME, new_text, -1);
}

static GtkWidget *
focus_iconview (gboolean color_bg, GtkCellRenderer **focus, GtkCellRenderer **sibling)
{
  GtkTreeModel *model;
  GtkWidget *iconview;
  GtkCellArea *area;
  GtkCellRenderer *renderer, *toggle;

  iconview = gtk_icon_view_new ();

  model = focus_list_model ();

  gtk_icon_view_set_model (GTK_ICON_VIEW (iconview), model);
  gtk_icon_view_set_item_orientation (GTK_ICON_VIEW (iconview), GTK_ORIENTATION_HORIZONTAL);

  area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, TRUE, FALSE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", FOCUS_COLUMN_NAME);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "red", NULL);

  g_signal_connect (G_OBJECT (renderer), "edited",
		    G_CALLBACK (cell_edited), iconview);

  toggle = renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "xalign", 0.0F, NULL);
  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "active", FOCUS_COLUMN_CHECK);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "green", NULL);

  if (focus)
    *focus = renderer;

  g_signal_connect (G_OBJECT (renderer), "toggled",
		    G_CALLBACK (cell_toggled), iconview);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
		"wrap-mode", PANGO_WRAP_WORD,
		"wrap-width", 150,
		NULL);

  if (color_bg)
    g_object_set (G_OBJECT (renderer), "cell-background", "blue", NULL);

  if (sibling)
    *sibling = renderer;

  gtk_cell_area_box_pack_start (GTK_CELL_AREA_BOX (area), renderer, FALSE, TRUE, FALSE);
  gtk_cell_area_attribute_connect (area, renderer, "text", FOCUS_COLUMN_STATIC_TEXT);

  gtk_cell_area_add_focus_sibling (area, toggle, renderer);

  return iconview;
}

static void
focus_sibling_toggled (GtkCheckButton  *toggle,
		       GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  gboolean     active = gtk_check_button_get_active (toggle);

  if (active)
    gtk_cell_area_add_focus_sibling (area, focus_renderer, sibling_renderer);
  else
    gtk_cell_area_remove_focus_sibling (area, focus_renderer, sibling_renderer);

  gtk_widget_queue_draw (GTK_WIDGET (iconview));
}


static void
focus_cell_area (void)
{
  GtkWidget *window, *widget;
  GtkWidget *iconview, *frame, *vbox, *hbox;

  window = gtk_window_new ();
  hbox  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  gtk_window_set_title (GTK_WINDOW (window), "Focus and editable cells");

  iconview = focus_iconview (FALSE, &focus_renderer, &sibling_renderer);

  frame = gtk_frame_new (NULL);
  gtk_widget_set_hexpand (frame, TRUE);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_frame_set_child (GTK_FRAME (frame), iconview);

  /* Now add some controls */
  vbox  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append (GTK_BOX (hbox), vbox);
  gtk_box_append (GTK_BOX (hbox), frame);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), iconview);

  widget = gtk_check_button_new_with_label ("Focus Sibling");
  gtk_check_button_set_active (GTK_CHECK_BUTTON (widget), TRUE);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "toggled",
                    G_CALLBACK (focus_sibling_toggled), iconview);

  gtk_window_set_child (GTK_WINDOW (window), hbox);

  gtk_window_present (GTK_WINDOW (window));
}



/*******************************************************
 *                  Background Area                    *
 *******************************************************/
static void
cell_spacing_changed (GtkSpinButton    *spin_button,
		      GtkIconView *iconview)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (iconview));
  int         value;

  value = (int)gtk_spin_button_get_value (spin_button);

  gtk_cell_area_box_set_spacing (GTK_CELL_AREA_BOX (area), value);
}

static void
row_spacing_changed (GtkSpinButton    *spin_button,
		     GtkIconView *iconview)
{
  int value;

  value = (int)gtk_spin_button_get_value (spin_button);

  gtk_icon_view_set_row_spacing (iconview, value);
}

static void
item_padding_changed (GtkSpinButton    *spin_button,
		     GtkIconView *iconview)
{
  int value;

  value = (int)gtk_spin_button_get_value (spin_button);

  gtk_icon_view_set_item_padding (iconview, value);
}

static void
background_area (void)
{
  GtkWidget *window, *widget, *label, *main_vbox;
  GtkWidget *iconview, *frame, *vbox, *hbox;

  window = gtk_window_new ();
  hbox  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  main_vbox  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_window_set_child (GTK_WINDOW (window), main_vbox);

  gtk_window_set_title (GTK_WINDOW (window), "Background Area");

  label = gtk_label_new ("In this example, row spacing gets divided into the background area, "
			 "column spacing is added between each background area, item_padding is "
			 "prepended space distributed to the background area.");
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_width_chars (GTK_LABEL (label), 40);
  gtk_box_append (GTK_BOX (main_vbox), label);

  iconview = focus_iconview (TRUE, NULL, NULL);

  frame = gtk_frame_new (NULL);
  gtk_widget_set_hexpand (frame, TRUE);

  gtk_widget_set_valign (frame, GTK_ALIGN_CENTER);
  gtk_widget_set_halign (frame, GTK_ALIGN_FILL);

  gtk_frame_set_child (GTK_FRAME (frame), iconview);

  /* Now add some controls */
  vbox  = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append (GTK_BOX (hbox), vbox);
  gtk_box_append (GTK_BOX (hbox), frame);

  gtk_box_append (GTK_BOX (main_vbox), hbox);

  widget = gtk_combo_box_text_new ();
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Horizontal");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), "Vertical");
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
  gtk_box_append (GTK_BOX (vbox), widget);

  g_signal_connect (G_OBJECT (widget), "changed",
                    G_CALLBACK (orientation_changed), iconview);

  widget = gtk_spin_button_new_with_range (0, 10, 1);
  label = gtk_label_new ("Cell spacing");
  gtk_widget_set_hexpand (label, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (cell_spacing_changed), iconview);


  widget = gtk_spin_button_new_with_range (0, 10, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), gtk_icon_view_get_row_spacing (GTK_ICON_VIEW (iconview)));
  label = gtk_label_new ("Row spacing");
  gtk_widget_set_hexpand (label, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (row_spacing_changed), iconview);

  widget = gtk_spin_button_new_with_range (0, 30, 1);
  label = gtk_label_new ("Item padding");
  gtk_widget_set_hexpand (label, TRUE);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), gtk_icon_view_get_item_padding (GTK_ICON_VIEW (iconview)));
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_append (GTK_BOX (hbox), label);
  gtk_box_append (GTK_BOX (hbox), widget);
  gtk_box_append (GTK_BOX (vbox), hbox);

  g_signal_connect (G_OBJECT (widget), "value-changed",
                    G_CALLBACK (item_padding_changed), iconview);

  gtk_window_present (GTK_WINDOW (window));
}






int
main (int argc, char *argv[])
{
  gtk_init ();

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  simple_cell_area ();
  focus_cell_area ();
  background_area ();

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
