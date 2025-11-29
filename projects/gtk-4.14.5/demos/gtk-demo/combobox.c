/* Combo Boxes
 * #Keywords: GtkCellRenderer
 *
 * The GtkComboBox widget allows to select one option out of a list.
 * The GtkComboBoxEntry additionally allows the user to enter a value
 * that is not in the list of options.
 *
 * How the options are displayed is controlled by cell renderers.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

enum
{
  ICON_NAME_COL,
  TEXT_COL
};

static GtkTreeModel *
create_icon_store (void)
{
  const char *icon_names[6] = {
    "dialog-warning",
    "process-stop",
    "document-new",
    "edit-clear",
    NULL,
    "document-open"
  };
  const char *labels[6] = {
    N_("Warning"),
    N_("Stop"),
    N_("New"),
    N_("Clear"),
    NULL,
    N_("Open")
  };

  GtkTreeIter iter;
  GtkListStore *store;
  int i;

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (icon_names); i++)
    {
      if (icon_names[i])
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              ICON_NAME_COL, icon_names[i],
                              TEXT_COL, _(labels[i]),
                              -1);
        }
      else
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              ICON_NAME_COL, NULL,
                              TEXT_COL, "separator",
                              -1);
        }
    }

  return GTK_TREE_MODEL (store);
}

/* A GtkCellLayoutDataFunc that demonstrates how one can control
 * sensitivity of rows. This particular function does nothing
 * useful and just makes the second row insensitive.
 */
static void
set_sensitive (GtkCellLayout   *cell_layout,
               GtkCellRenderer *cell,
               GtkTreeModel    *tree_model,
               GtkTreeIter     *iter,
               gpointer         data)
{
  GtkTreePath *path;
  int *indices;
  gboolean sensitive;

  path = gtk_tree_model_get_path (tree_model, iter);
  indices = gtk_tree_path_get_indices (path);
  sensitive = indices[0] != 1;
  gtk_tree_path_free (path);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

/* A GtkTreeViewRowSeparatorFunc that demonstrates how rows can be
 * rendered as separators. This particular function does nothing
 * useful and just turns the fourth row into a separator.
 */
static gboolean
is_separator (GtkTreeModel *model,
              GtkTreeIter  *iter,
              gpointer      data)
{
  GtkTreePath *path;
  gboolean result;

  path = gtk_tree_model_get_path (model, iter);
  result = gtk_tree_path_get_indices (path)[0] == 4;
  gtk_tree_path_free (path);

  return result;
}

static GtkTreeModel *
create_capital_store (void)
{
  struct {
    const char *group;
    const char *capital;
  } capitals[] = {
    { "A - B", NULL },
    { NULL, "Albany" },
    { NULL, "Annapolis" },
    { NULL, "Atlanta" },
    { NULL, "Augusta" },
    { NULL, "Austin" },
    { NULL, "Baton Rouge" },
    { NULL, "Bismarck" },
    { NULL, "Boise" },
    { NULL, "Boston" },
    { "C - D", NULL },
    { NULL, "Carson City" },
    { NULL, "Charleston" },
    { NULL, "Cheyenne" },
    { NULL, "Columbia" },
    { NULL, "Columbus" },
    { NULL, "Concord" },
    { NULL, "Denver" },
    { NULL, "Des Moines" },
    { NULL, "Dover" },
    { "E - J", NULL },
    { NULL, "Frankfort" },
    { NULL, "Harrisburg" },
    { NULL, "Hartford" },
    { NULL, "Helena" },
    { NULL, "Honolulu" },
    { NULL, "Indianapolis" },
    { NULL, "Jackson" },
    { NULL, "Jefferson City" },
    { NULL, "Juneau" },
    { "K - O", NULL },
    { NULL, "Lansing" },
    { NULL, "Lincoln" },
    { NULL, "Little Rock" },
    { NULL, "Madison" },
    { NULL, "Montgomery" },
    { NULL, "Montpelier" },
    { NULL, "Nashville" },
    { NULL, "Oklahoma City" },
    { NULL, "Olympia" },
    { "P - S", NULL },
    { NULL, "Phoenix" },
    { NULL, "Pierre" },
    { NULL, "Providence" },
    { NULL, "Raleigh" },
    { NULL, "Richmond" },
    { NULL, "Sacramento" },
    { NULL, "Salem" },
    { NULL, "Salt Lake City" },
    { NULL, "Santa Fe" },
    { NULL, "Springfield" },
    { NULL, "St. Paul" },
    { "T - Z", NULL },
    { NULL, "Tallahassee" },
    { NULL, "Topeka" },
    { NULL, "Trenton" },
    { NULL, NULL }
  };

  GtkTreeIter iter, iter2;
  GtkTreeStore *store;
  int i;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  for (i = 0; capitals[i].group || capitals[i].capital; i++)
    {
      if (capitals[i].group)
        {
          gtk_tree_store_append (store, &iter, NULL);
          gtk_tree_store_set (store, &iter, 0, capitals[i].group, -1);
        }
      else if (capitals[i].capital)
        {
          gtk_tree_store_append (store, &iter2, &iter);
          gtk_tree_store_set (store, &iter2, 0, capitals[i].capital, -1);
        }
    }

  return GTK_TREE_MODEL (store);
}

static void
is_capital_sensitive (GtkCellLayout   *cell_layout,
                      GtkCellRenderer *cell,
                      GtkTreeModel    *tree_model,
                      GtkTreeIter     *iter,
                      gpointer         data)
{
  gboolean sensitive;

  sensitive = !gtk_tree_model_iter_has_child (tree_model, iter);

  g_object_set (cell, "sensitive", sensitive, NULL);
}

static void
fill_combo_entry (GtkWidget *combo)
{
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "One");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Two");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "2\302\275");
  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), "Three");
}


/* A simple validating entry */

#define TYPE_MASK_ENTRY             (mask_entry_get_type ())
#define MASK_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MASK_ENTRY, MaskEntry))
#define MASK_ENTRY_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), TYPE_MASK_ENTRY, MaskEntryClass))
#define IS_MASK_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MASK_ENTRY))
#define IS_MASK_ENTRY_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), TYPE_MASK_ENTRY))
#define MASK_ENTRY_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), TYPE_MASK_ENTRY, MaskEntryClass))


typedef struct _MaskEntry MaskEntry;
struct _MaskEntry
{
  GtkEntry entry;
  const char *mask;
};

typedef struct _MaskEntryClass MaskEntryClass;
struct _MaskEntryClass
{
  GtkEntryClass parent_class;
};


static void mask_entry_editable_init (GtkEditableInterface *iface);

static GType mask_entry_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MaskEntry, mask_entry, GTK_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                mask_entry_editable_init));


static void
mask_entry_set_background (MaskEntry *entry)
{
  if (entry->mask)
    {
      if (!g_regex_match_simple (entry->mask, gtk_editable_get_text (GTK_EDITABLE (entry)), 0, 0))
        {
          PangoAttrList *attrs;

          attrs = pango_attr_list_new ();
          pango_attr_list_insert (attrs, pango_attr_foreground_new (65535, 32767, 32767));
          gtk_entry_set_attributes (GTK_ENTRY (entry), attrs);
          pango_attr_list_unref (attrs);
          return;
        }
    }

  gtk_entry_set_attributes (GTK_ENTRY (entry), NULL);
}


static void
mask_entry_changed (GtkEditable *editable)
{
  mask_entry_set_background (MASK_ENTRY (editable));
}


static void
mask_entry_init (MaskEntry *entry)
{
  entry->mask = NULL;
}


static void
mask_entry_class_init (MaskEntryClass *klass)
{ }


static void
mask_entry_editable_init (GtkEditableInterface *iface)
{
  iface->changed = mask_entry_changed;
}


GtkWidget *
do_combobox (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox, *frame, *box, *combo, *entry;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;
  GtkTreePath *path;
  GtkTreeIter iter;

  if (!window)
  {
    window = gtk_window_new ();
    gtk_window_set_display (GTK_WINDOW (window),
                            gtk_widget_get_display (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Combo Boxes");
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start (vbox, 10);
    gtk_widget_set_margin_end (vbox, 10);
    gtk_widget_set_margin_top (vbox, 10);
    gtk_widget_set_margin_bottom (vbox, 10);
    gtk_window_set_child (GTK_WINDOW (window), vbox);

    /* A combobox demonstrating cell renderers, separators and
     *  insensitive rows
     */
    frame = gtk_frame_new ("Items with icons");
    gtk_box_append (GTK_BOX (vbox), frame);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (box, 5);
    gtk_widget_set_margin_end (box, 5);
    gtk_widget_set_margin_top (box, 5);
    gtk_widget_set_margin_bottom (box, 5);
    gtk_frame_set_child (GTK_FRAME (frame), box);

    model = create_icon_store ();
    combo = gtk_combo_box_new_with_model (model);
    g_object_unref (model);
    gtk_box_append (GTK_BOX (box), combo);

    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                    "icon-name", ICON_NAME_COL,
                                    NULL);

    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
                                        renderer,
                                        set_sensitive,
                                        NULL, NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                    "text", TEXT_COL,
                                    NULL);

    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
                                        renderer,
                                        set_sensitive,
                                        NULL, NULL);

    gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
                                          is_separator, NULL, NULL);

    gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);

    /* A combobox demonstrating trees.
     */
    frame = gtk_frame_new ("Where are we ?");
    gtk_box_append (GTK_BOX (vbox), frame);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (box, 5);
    gtk_widget_set_margin_end (box, 5);
    gtk_widget_set_margin_top (box, 5);
    gtk_widget_set_margin_bottom (box, 5);
    gtk_frame_set_child (GTK_FRAME (frame), box);

    model = create_capital_store ();
    combo = gtk_combo_box_new_with_model (model);
    g_object_unref (model);
    gtk_box_append (GTK_BOX (box), combo);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                    "text", 0,
                                    NULL);
    gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo),
                                        renderer,
                                        is_capital_sensitive,
                                        NULL, NULL);

    path = gtk_tree_path_new_from_indices (0, 8, -1);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_path_free (path);
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);

    /* A GtkComboBoxEntry with validation */
    frame = gtk_frame_new ("Editable");
    gtk_box_append (GTK_BOX (vbox), frame);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (box, 5);
    gtk_widget_set_margin_end (box, 5);
    gtk_widget_set_margin_top (box, 5);
    gtk_widget_set_margin_bottom (box, 5);
    gtk_frame_set_child (GTK_FRAME (frame), box);

    combo = gtk_combo_box_text_new_with_entry ();
    fill_combo_entry (combo);
    gtk_box_append (GTK_BOX (box), combo);

    entry = g_object_new (TYPE_MASK_ENTRY, NULL);
    MASK_ENTRY (entry)->mask = "^([0-9]*|One|Two|2\302\275|Three)$";

    gtk_combo_box_set_child (GTK_COMBO_BOX (combo), entry);

    /* A combobox with string IDs */
    frame = gtk_frame_new ("String IDs");
    gtk_box_append (GTK_BOX (vbox), frame);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_start (box, 5);
    gtk_widget_set_margin_end (box, 5);
    gtk_widget_set_margin_top (box, 5);
    gtk_widget_set_margin_bottom (box, 5);
    gtk_frame_set_child (GTK_FRAME (frame), box);

    combo = gtk_combo_box_text_new ();
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "never", "Not visible");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "when-active", "Visible when active");
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "always", "Always visible");
    gtk_box_append (GTK_BOX (box), combo);

    entry = gtk_entry_new ();
    g_object_bind_property (combo, "active-id",
                            entry, "text",
                            G_BINDING_BIDIRECTIONAL);
    gtk_box_append (GTK_BOX (box), entry);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
