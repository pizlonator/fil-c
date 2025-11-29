/* Lists/Selections
 * #Keywords: suggestion, completion
 *
 * The GtkDropDown widget is a modern alternative to GtkComboBox.
 * It uses list models instead of tree models, and the content is
 * displayed using widgets instead of cell renderers.
 *
 * This example also shows a custom widget that can replace
 * GtkEntryCompletion or GtkComboBoxText. It is not currently
 * part of GTK.
 */

#include <gtk/gtk.h>
#include "suggestionentry.h"

#define STRING_TYPE_HOLDER (string_holder_get_type ())
G_DECLARE_FINAL_TYPE (StringHolder, string_holder, STRING, HOLDER, GObject)

struct _StringHolder {
  GObject parent_instance;
  char *title;
  char *icon;
  char *description;
};

G_DEFINE_TYPE (StringHolder, string_holder, G_TYPE_OBJECT);

static void
string_holder_init (StringHolder *holder)
{
}

static void
string_holder_finalize (GObject *object)
{
  StringHolder *holder = STRING_HOLDER (object);

  g_free (holder->title);
  g_free (holder->icon);
  g_free (holder->description);

  G_OBJECT_CLASS (string_holder_parent_class)->finalize (object);
}

static void
string_holder_class_init (StringHolderClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = string_holder_finalize;
}

static StringHolder *
string_holder_new (const char *title, const char *icon, const char *description)
{
  StringHolder *holder = g_object_new (STRING_TYPE_HOLDER, NULL);
  holder->title = g_strdup (title);
  holder->icon = g_strdup (icon);
  holder->description = g_strdup (description);
  return holder;
}

static void
strings_setup_item_single_line (GtkSignalListItemFactory *factory,
                                GtkListItem              *item)
{
  GtkWidget *box, *image, *title;
  GtkWidget *checkmark;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), title);
  gtk_box_append (GTK_BOX (box), checkmark);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);
  g_object_set_data (G_OBJECT (item), "checkmark", checkmark);

  gtk_list_item_set_child (item, box);
}

static void
strings_setup_item_full (GtkSignalListItemFactory *factory,
                         GtkListItem              *item)
{
  GtkWidget *box, *box2, *image, *title, *description;
  GtkWidget *checkmark;

  image = gtk_image_new ();
  title = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (title), 0.0);
  description = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (description), 0.0);
  gtk_widget_add_css_class (description, "dim-label");
  checkmark = gtk_image_new_from_icon_name ("object-select-symbolic");

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

  gtk_box_append (GTK_BOX (box), image);
  gtk_box_append (GTK_BOX (box), box2);
  gtk_box_append (GTK_BOX (box2), title);
  gtk_box_append (GTK_BOX (box2), description);
  gtk_box_append (GTK_BOX (box), checkmark);

  g_object_set_data (G_OBJECT (item), "title", title);
  g_object_set_data (G_OBJECT (item), "image", image);
  g_object_set_data (G_OBJECT (item), "description", description);
  g_object_set_data (G_OBJECT (item), "checkmark", checkmark);

  gtk_list_item_set_child (item, box);
}

static void
selected_item_changed (GtkDropDown *dropdown,
                       GParamSpec  *pspec,
                       GtkListItem *item)
{
  GtkWidget *checkmark;

  checkmark = g_object_get_data (G_OBJECT (item), "checkmark");

  if (gtk_drop_down_get_selected_item (dropdown) == gtk_list_item_get_item (item))
    gtk_widget_set_opacity (checkmark, 1.0);
  else
    gtk_widget_set_opacity (checkmark, 0.0);
}

static void
strings_bind_item (GtkSignalListItemFactory *factory,
                   GtkListItem              *item,
                   gpointer                  data)
{
  GtkDropDown *dropdown = data;
  GtkWidget *image, *title, *description;
  GtkWidget *checkmark;
  StringHolder *holder;
  GtkWidget *popup;

  holder = gtk_list_item_get_item (item);

  title = g_object_get_data (G_OBJECT (item), "title");
  image = g_object_get_data (G_OBJECT (item), "image");
  description = g_object_get_data (G_OBJECT (item), "description");
  checkmark = g_object_get_data (G_OBJECT (item), "checkmark");

  gtk_label_set_label (GTK_LABEL (title), holder->title);
  if (image)
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), holder->icon);
      gtk_widget_set_visible (image, holder->icon != NULL);
    }
  if (description)
    {
      gtk_label_set_label (GTK_LABEL (description), holder->description);
      gtk_widget_set_visible (description , holder->description != NULL);
    }

  popup = gtk_widget_get_ancestor (title, GTK_TYPE_POPOVER);
  if (popup && gtk_widget_is_ancestor (popup, GTK_WIDGET (dropdown)))
    {
      gtk_widget_set_visible (checkmark, TRUE);
      g_signal_connect (dropdown, "notify::selected-item",
                        G_CALLBACK (selected_item_changed), item);
      selected_item_changed (dropdown, NULL, item);
    }
  else
    {
      gtk_widget_set_visible (checkmark, FALSE);
    }
}

static void
strings_unbind_item (GtkSignalListItemFactory *factory,
                     GtkListItem              *list_item,
                     gpointer                  data)
{
  GtkDropDown *dropdown = data;

  g_signal_handlers_disconnect_by_func (dropdown, selected_item_changed, list_item);
}

static GtkListItemFactory *
strings_factory_new (gpointer data, gboolean full)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();
  if (full)
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_full), data);
  else
    g_signal_connect (factory, "setup", G_CALLBACK (strings_setup_item_single_line), data);
  g_signal_connect (factory, "bind", G_CALLBACK (strings_bind_item), data);
  g_signal_connect (factory, "unbind", G_CALLBACK (strings_unbind_item), data);

  return factory;
}

static GListModel *
strings_model_new (const char *const *titles,
                   const char *const *icons,
                   const char *const *descriptions)
{
  GListStore *store;
  int i;

  store = g_list_store_new (STRING_TYPE_HOLDER);
  for (i = 0; titles[i]; i++)
    {
      StringHolder *holder = string_holder_new (titles[i],
                                                icons ? icons[i] : NULL,
                                                descriptions ? descriptions[i] : NULL);
      g_list_store_append (store, holder);
      g_object_unref (holder);
    }

  return G_LIST_MODEL (store);
}

static GtkWidget *
drop_down_new_from_strings (const char *const *titles,
                            const char *const *icons,
                            const char *const *descriptions)
{
  GtkWidget *widget;
  GListModel *model;
  GtkListItemFactory *factory;
  GtkListItemFactory *list_factory;

  g_return_val_if_fail (titles != NULL, NULL);
  g_return_val_if_fail (icons == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)titles), NULL);
  g_return_val_if_fail (descriptions == NULL || g_strv_length ((char **)icons) == g_strv_length ((char **)descriptions), NULL);

  model = strings_model_new (titles, icons, descriptions);
  widget = g_object_new (GTK_TYPE_DROP_DOWN,
                         "model", model,
                         NULL);
  g_object_unref (model);

  factory = strings_factory_new (widget, FALSE);
  if (icons != NULL || descriptions != NULL)
    list_factory = strings_factory_new (widget, TRUE);
  else
    list_factory = NULL;

  g_object_set (widget,
                "factory", factory,
                "list-factory", list_factory,
                NULL);

  g_object_unref (factory);
  if (list_factory)
    g_object_unref (list_factory);

  return widget;
}

static char *
get_family_name (gpointer item)
{
  return g_strdup (pango_font_family_get_name (PANGO_FONT_FAMILY (item)));
}

static char *
get_title (gpointer item)
{
  return g_strdup (STRING_HOLDER (item)->title);
}

static char *
get_file_name (gpointer item)
{
  return g_strdup (g_file_info_get_display_name (G_FILE_INFO (item)));
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *item)
{
  GtkWidget *box;
  GtkWidget *icon;
  GtkWidget *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  icon = gtk_image_new ();
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_box_append (GTK_BOX (box), icon);
  gtk_box_append (GTK_BOX (box), label);
  gtk_list_item_set_child (item, box);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *item)
{
  MatchObject *match = MATCH_OBJECT (gtk_list_item_get_item (item));
  GFileInfo *info = G_FILE_INFO (match_object_get_item (match));
  GtkWidget *box = gtk_list_item_get_child (item);
  GtkWidget *icon = gtk_widget_get_first_child (box);
  GtkWidget *label = gtk_widget_get_last_child (box);

  gtk_image_set_from_gicon (GTK_IMAGE (icon), g_file_info_get_icon (info));
  gtk_label_set_label (GTK_LABEL (label), g_file_info_get_display_name (info));
}

static void
setup_highlight_item (GtkSignalListItemFactory *factory,
                      GtkListItem              *item)
{
  GtkWidget *label;

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_list_item_set_child (item, label);
}

static void
bind_highlight_item (GtkSignalListItemFactory *factory,
                     GtkListItem              *item)
{
  MatchObject *obj;
  GtkWidget *label;
  PangoAttrList *attrs;
  PangoAttribute *attr;
  const char *str;

  obj = MATCH_OBJECT (gtk_list_item_get_item (item));
  label = gtk_list_item_get_child (item);

  str = match_object_get_string (obj);

  gtk_label_set_label (GTK_LABEL (label), str);
  attrs = pango_attr_list_new ();
  attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attr->start_index = match_object_get_match_start (obj);
  attr->end_index = match_object_get_match_end (obj);
  pango_attr_list_insert (attrs, attr);
  gtk_label_set_attributes (GTK_LABEL (label), attrs);
  pango_attr_list_unref (attrs);
}

static void
match_func (MatchObject *obj,
            const char  *search,
            gpointer     user_data)
{
  char *tmp1, *tmp2;
  char *p;

  tmp1 = g_utf8_normalize (match_object_get_string (obj), -1, G_NORMALIZE_ALL);
  tmp2 = g_utf8_normalize (search, -1, G_NORMALIZE_ALL);

  if ((p = strstr (tmp1, tmp2)) != NULL)
    match_object_set_match (obj,
                            p - tmp1,
                            (p - tmp1) + g_utf8_strlen (search, -1),
                            1);
  else
    match_object_set_match (obj, 0, 0, 0);

  g_free (tmp1);
  g_free (tmp2);
}

static void
setup_header (GtkSignalListItemFactory *factory,
              GObject                  *list_item,
              gpointer                  data)
{
  GtkListHeader *self = GTK_LIST_HEADER (list_item);
  GtkWidget *child;

  child = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (child), 0);
  gtk_label_set_use_markup (GTK_LABEL (child), TRUE);
  gtk_widget_set_margin_top (child, 10);
  gtk_widget_set_margin_bottom (child, 10);

  gtk_list_header_set_child (self, child);
}

static void
bind_header (GtkSignalListItemFactory *factory,
             GObject                  *list_item,
             gpointer                  data)
{
  GtkListHeader *self = GTK_LIST_HEADER (list_item);
  GtkWidget *child = gtk_list_header_get_child (self);
  GObject *item = gtk_list_header_get_item (self);

  if (strstr (gtk_string_object_get_string (GTK_STRING_OBJECT (item)), "hour"))
    gtk_label_set_label (GTK_LABEL (child), "<big><b>Hours</b></big>");
  else
    gtk_label_set_label (GTK_LABEL (child), "<big><b>Minutes</b></big>");
}

GtkWidget *
do_listview_selections (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *button, *box, *spin, *check, *hbox, *label, *entry;
  GListModel *model;
  GtkExpression *expression;
  GtkListItemFactory *factory;
  const char * const times[] = { "1 minute", "2 minutes", "5 minutes", "20 minutes", NULL };
  const char * const minutes[] = {
    "1 minute", "2 minutes", "5 minutes", "10 minutes", "15 minutes", "20 minutes",
    "25 minutes", "30 minutes", "35 minutes", "40 minutes", "45 minutes", "50 minutes",
    "55 minutes", NULL
  };
  const char * const hours[] = { "1 hour", "2 hours", "3 hours", "5 hours", "6 hours", "7 hours",
    "8 hours", "9 hours", "10 hours", "11 hours", "12 hours", NULL
  };
  const char * const device_titles[] = { "Digital Output", "Headphones", "Digital Output", "Analog Output", NULL };
  const char * const device_icons[] = {  "audio-card-symbolic", "audio-headphones-symbolic", "audio-card-symbolic", "audio-card-symbolic", NULL };
  const char * const device_descriptions[] = {
    "Built-in Audio", "Built-in audio", "Thinkpad Tunderbolt 3 Dock USB Audio", "Thinkpad Tunderbolt 3 Dock USB Audio", NULL
  };
  char *cwd;
  GFile *file;
  GListModel *dir;
  GtkStringList *strings;

  if (!window)
    {
      GtkStringList *minutes_model, *hours_model;
      GListStore *store;
      GtkFlattenListModel *flat;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Selections");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);

      gtk_widget_set_margin_start (hbox, 20);
      gtk_widget_set_margin_end (hbox, 20);
      gtk_widget_set_margin_top (hbox, 20);
      gtk_widget_set_margin_bottom (hbox, 20);
      gtk_window_set_child (GTK_WINDOW (window), hbox);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (hbox), box);

      label = gtk_label_new ("Dropdowns");
      gtk_widget_add_css_class (label, "title-4");
      gtk_box_append (GTK_BOX (box), label);

      /* A basic dropdown */
      button = drop_down_new_from_strings (times, NULL, NULL);
      gtk_box_append (GTK_BOX (box), button);

      /* A dropdown using an expression to obtain strings */
      minutes_model = gtk_string_list_new (minutes);
      hours_model = gtk_string_list_new (hours);
      store = g_list_store_new (G_TYPE_LIST_MODEL);
      g_list_store_append (store, minutes_model);
      g_list_store_append (store, hours_model);
      g_object_unref (minutes_model);
      g_object_unref (hours_model);
      flat = gtk_flatten_list_model_new (G_LIST_MODEL (store));
      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_title,
                                                NULL, NULL);
      button = gtk_drop_down_new (G_LIST_MODEL (flat), expression);
      gtk_drop_down_set_enable_search (GTK_DROP_DOWN (button), TRUE);
      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_header), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_header), NULL);
      gtk_drop_down_set_header_factory (GTK_DROP_DOWN (button), factory);
      g_object_unref (factory);
      gtk_box_append (GTK_BOX (box), button);

      button = gtk_drop_down_new (NULL, NULL);

      model = G_LIST_MODEL (pango_cairo_font_map_get_default ());
      gtk_drop_down_set_model (GTK_DROP_DOWN (button), model);
      gtk_drop_down_set_selected (GTK_DROP_DOWN (button), 0);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_family_name,
                                                NULL, NULL);
      gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
      gtk_expression_unref (expression);
      gtk_box_append (GTK_BOX (box), button);

      spin = gtk_spin_button_new_with_range (-1, g_list_model_get_n_items (G_LIST_MODEL (model)), 1);
      gtk_widget_set_halign (spin, GTK_ALIGN_START);
      gtk_widget_set_margin_start (spin, 20);
      g_object_bind_property  (button, "selected", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), spin);

      check = gtk_check_button_new_with_label ("Enable search");
      gtk_widget_set_margin_start (check, 20);
      g_object_bind_property  (button, "enable-search", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
      gtk_box_append (GTK_BOX (box), check);

      g_object_unref (model);

      /* A dropdown with a separate list factory */
      button = drop_down_new_from_strings (device_titles, device_icons, device_descriptions);
      gtk_box_append (GTK_BOX (box), button);

      gtk_box_append (GTK_BOX (hbox), gtk_separator_new (GTK_ORIENTATION_VERTICAL));

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
      gtk_box_append (GTK_BOX (hbox), box);

      label = gtk_label_new ("Suggestions");
      gtk_widget_add_css_class (label, "title-4");
      gtk_box_append (GTK_BOX (box), label);

      /* A basic suggestion entry */
      entry = suggestion_entry_new ();
      g_object_set (entry, "placeholder-text", "Words with T or G…", NULL);
      strings = gtk_string_list_new ((const char *[]){
                                     "GNOME",
                                     "gnominious",
                                     "Gnomonic projection",
                                     "total",
                                     "totally",
                                     "toto",
                                     "tottery",
                                     "totterer",
                                     "Totten trust",
                                     "totipotent",
                                     "totipotency",
                                     "totemism",
                                     "totem pole",
                                     "Totara",
                                     "totalizer",
                                     "totalizator",
                                     "totalitarianism",
                                     "total parenteral nutrition",
                                     "total hysterectomy",
                                     "total eclipse",
                                     "Totipresence",
                                     "Totipalmi",
                                     "Tomboy",
                                     "zombie",
                                     NULL});
      suggestion_entry_set_model (SUGGESTION_ENTRY (entry), G_LIST_MODEL (strings));
      g_object_unref (strings);

      gtk_box_append (GTK_BOX (box), entry);

      /* A suggestion entry using a custom model, and no filtering */
      entry = suggestion_entry_new ();

      cwd = g_get_current_dir ();
      file = g_file_new_for_path (cwd);
      dir = G_LIST_MODEL (gtk_directory_list_new ("standard::display-name,standard::content-type,standard::icon,standard::size", file));
      suggestion_entry_set_model (SUGGESTION_ENTRY (entry), dir);
      g_object_unref (dir);
      g_object_unref (file);
      g_free (cwd);

      expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                (GCallback)get_file_name,
                                                NULL, NULL);
      suggestion_entry_set_expression (SUGGESTION_ENTRY (entry), expression);
      gtk_expression_unref (expression);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);

      suggestion_entry_set_factory (SUGGESTION_ENTRY (entry), factory);
      g_object_unref (factory);

      suggestion_entry_set_use_filter (SUGGESTION_ENTRY (entry), FALSE);
      suggestion_entry_set_show_arrow (SUGGESTION_ENTRY (entry), TRUE);

      gtk_box_append (GTK_BOX (box), entry);

      /* A suggestion entry with match highlighting */
      entry = suggestion_entry_new ();
      g_object_set (entry, "placeholder-text", "Destination", NULL);

      strings = gtk_string_list_new ((const char *[]){
                                     "app-mockups",
                                     "settings-mockups",
                                     "os-mockups",
                                     "software-mockups",
                                     "mocktails",
                                     NULL});
      suggestion_entry_set_model (SUGGESTION_ENTRY (entry), G_LIST_MODEL (strings));
      g_object_unref (strings);

      gtk_box_append (GTK_BOX (box), entry);

      suggestion_entry_set_match_func (SUGGESTION_ENTRY (entry), match_func, NULL, NULL);

      factory = gtk_signal_list_item_factory_new ();
      g_signal_connect (factory, "setup", G_CALLBACK (setup_highlight_item), NULL);
      g_signal_connect (factory, "bind", G_CALLBACK (bind_highlight_item), NULL);
      suggestion_entry_set_factory (SUGGESTION_ENTRY (entry), factory);
      g_object_unref (factory);

    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
