/* Lists/Settings
 * #Keywords: GtkListItemFactory, GListModel
 *
 * This demo shows a settings viewer for GSettings.
 *
 * It demonstrates how to implement support for trees with GtkListView.
 * It also shows how to set up sorting and filtering for columns in a
 * GtkColumnView.
 *
 * It also demonstrates different styles of list. The tree on the left
 * uses the ­.navigation-sidebar style class, the list on the right uses
 * the ­.data-table style class.
 */

#include <gtk/gtk.h>

#include "settings-key.h"

static void
item_value_changed (GtkEditableLabel  *label,
                    GParamSpec        *pspec,
                    GtkColumnViewCell *cell)
{
  SettingsKey *self;
  GSettingsSchemaKey *key;
  const char *text;
  const GVariantType *type;
  GVariant *variant;
  GError *error = NULL;
  const char *name;
  char *value;

  text = gtk_editable_get_text (GTK_EDITABLE (label));

  self = gtk_column_view_cell_get_item (cell);
  key = settings_key_get_key (self);

  type = g_settings_schema_key_get_value_type (key);
  name = g_settings_schema_key_get_name (key);

  variant = g_variant_parse (type, text, NULL, NULL, &error);
  if (!variant)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      goto revert;
    }

  if (!g_settings_schema_key_range_check (key, variant))
    {
      g_warning ("Not a valid value for %s", name);
      goto revert;
    }

  g_settings_set_value (settings_key_get_settings (self), name, variant);
  g_variant_unref (variant);
  return;

revert:
  gtk_widget_error_bell (GTK_WIDGET (label));

  g_object_get (self, "value", &value, NULL);
  gtk_editable_set_text (GTK_EDITABLE (label), value);
  g_free (value);
}

static int
strvcmp (gconstpointer p1,
         gconstpointer p2)
{
  const char * const *s1 = p1;
  const char * const *s2 = p2;

  return strcmp (*s1, *s2);
}

static GtkFilter *current_filter;

static gboolean
transform_settings_to_keys (GBinding     *binding,
                            const GValue *from_value,
                            GValue       *to_value,
                            gpointer      data)
{
  GtkTreeListRow *treelistrow;
  GSettings *settings;
  GSettingsSchema *schema;
  GListStore *store;
  GtkSortListModel *sort_model;
  GtkFilterListModel *filter_model;
  GtkFilter *filter;
  char **keys;
  guint i;

  treelistrow = g_value_get_object (from_value);
  if (treelistrow == NULL)
    return TRUE;
  settings = gtk_tree_list_row_get_item (treelistrow);
  g_object_get (settings, "settings-schema", &schema, NULL);

  store = g_list_store_new (SETTINGS_TYPE_KEY);

  keys = g_settings_schema_list_keys (schema);

  for (i = 0; keys[i] != NULL; i++)
    {
      GSettingsSchemaKey *almost_there = g_settings_schema_get_key (schema, keys[i]);
      SettingsKey *finally = settings_key_new (settings, almost_there);
      g_list_store_append (store, finally);
      g_object_unref (finally);
      g_settings_schema_key_unref (almost_there);
    }

  g_strfreev (keys);
  g_settings_schema_unref (schema);
  g_object_unref (settings);

  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (store),
                                        g_object_ref (gtk_column_view_get_sorter (GTK_COLUMN_VIEW (data))));

  filter = GTK_FILTER (gtk_string_filter_new (gtk_property_expression_new (SETTINGS_TYPE_KEY, NULL, "name")));
  g_set_object (&current_filter, filter);
  filter_model = gtk_filter_list_model_new (G_LIST_MODEL (sort_model), filter);

  g_value_take_object (to_value, gtk_no_selection_new (G_LIST_MODEL (filter_model)));

  return TRUE;
}

static GListModel *
create_settings_model (gpointer item,
                       gpointer unused)
{
  GSettings *settings = item;
  char **schemas;
  GListStore *result;
  guint i;

  if (settings == NULL)
    {
      g_settings_schema_source_list_schemas (g_settings_schema_source_get_default (),
                                             TRUE,
                                             &schemas,
                                             NULL);
    }
  else
    {
      schemas = g_settings_list_children (settings);
    }

  if (schemas == NULL || schemas[0] == NULL)
    {
      g_free (schemas);
      return NULL;
    }

  qsort (schemas, g_strv_length (schemas), sizeof (char *), strvcmp);

  result = g_list_store_new (G_TYPE_SETTINGS);
  for (i = 0;  schemas[i] != NULL; i++)
    {
      GSettings *child;

      if (settings == NULL)
        child = g_settings_new (schemas[i]);
      else
        child = g_settings_get_child (settings, schemas[i]);

      g_list_store_append (result, child);
      g_object_unref (child);
    }

  g_strfreev (schemas);

  return G_LIST_MODEL (result);
}

static void
search_enabled (GtkSearchEntry *entry)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");
}

static void
search_changed (GtkSearchEntry *entry,
                gpointer data)
{
  const char *text = gtk_editable_get_text (GTK_EDITABLE (entry));

  if (current_filter)
    gtk_string_filter_set_search (GTK_STRING_FILTER (current_filter), text);
}

static void
stop_search (GtkSearchEntry *entry,
             gpointer data)
{
  gtk_editable_set_text (GTK_EDITABLE (entry), "");

  if (current_filter)
    gtk_string_filter_set_search (GTK_STRING_FILTER (current_filter), "");
}

static GtkWidget *window = NULL;

GtkWidget *
do_listview_settings (GtkWidget *do_widget)
{
  if (window == NULL)
    {
      GtkWidget *listview, *columnview;
      GListModel *model;
      GtkTreeListModel *treemodel;
      GtkSingleSelection *selection;
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GtkColumnViewColumn *name_column;
      GtkColumnViewColumn *type_column;
      GtkColumnViewColumn *default_column;
      GtkColumnViewColumn *summary_column;
      GtkColumnViewColumn *description_column;
      GtkSorter *sorter;
      GActionGroup *actions;
      GAction *action;

      g_type_ensure (SETTINGS_TYPE_KEY);

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, search_enabled);
      gtk_builder_cscope_add_callback (scope, search_changed);
      gtk_builder_cscope_add_callback (scope, stop_search);
      gtk_builder_cscope_add_callback (scope, item_value_changed);

      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      g_object_unref (scope);

      gtk_builder_add_from_resource (builder, "/listview_settings/listview_settings.ui", NULL);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *) &window);

      listview = GTK_WIDGET (gtk_builder_get_object (builder, "listview"));
      columnview = GTK_WIDGET (gtk_builder_get_object (builder, "columnview"));
      type_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "type_column"));
      default_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "default_column"));
      summary_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "summary_column"));
      description_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "description_column"));

      actions = G_ACTION_GROUP (g_simple_action_group_new ());

      action = G_ACTION (g_property_action_new ("show-type", type_column, "visible"));
      g_action_map_add_action (G_ACTION_MAP (actions), action);
      g_object_unref (action);

      action = G_ACTION (g_property_action_new ("show-default", default_column, "visible"));
      g_action_map_add_action (G_ACTION_MAP (actions), action);
      g_object_unref (action);

      action = G_ACTION (g_property_action_new ("show-summary", summary_column, "visible"));
      g_action_map_add_action (G_ACTION_MAP (actions), action);
      g_object_unref (action);

      action = G_ACTION (g_property_action_new ("show-description", description_column, "visible"));
      g_action_map_add_action (G_ACTION_MAP (actions), action);
      g_object_unref (action);

      gtk_widget_insert_action_group (columnview, "columnview", actions);
      g_object_unref (actions);

      model = create_settings_model (NULL, NULL);
      treemodel = gtk_tree_list_model_new (model,
                                           FALSE,
                                           TRUE,
                                           create_settings_model,
                                           NULL,
                                           NULL);
      selection = gtk_single_selection_new (G_LIST_MODEL (treemodel));
      g_object_bind_property_full (selection, "selected-item",
                                   columnview, "model",
                                   G_BINDING_SYNC_CREATE,
                                   transform_settings_to_keys,
                                   NULL,
                                   columnview, NULL);
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), GTK_SELECTION_MODEL (selection));
      g_object_unref (selection);

      name_column = GTK_COLUMN_VIEW_COLUMN (gtk_builder_get_object (builder, "name_column"));
      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (SETTINGS_TYPE_KEY, NULL, "name")));
      gtk_column_view_column_set_sorter (name_column, sorter);
      g_object_unref (sorter);

      sorter = GTK_SORTER (gtk_string_sorter_new (gtk_property_expression_new (SETTINGS_TYPE_KEY, NULL, "type")));
      gtk_column_view_column_set_sorter (type_column, sorter);
      g_object_unref (sorter);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
