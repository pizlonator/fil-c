/* cc-panel-list.c
 *
 * Copyright (C) 2016 Endless, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Georges Basile Stavracas Neto <gbsneto@gnome.org>
 */

#define G_LOG_DOMAIN "cc-panel-list"

#include <string.h>

#include "cc-log.h"
#include "cc-panel-list.h"
#include "cc-util.h"

typedef struct
{
  GtkWidget          *row;
  GtkWidget          *description_label;
  CcPanelCategory     category;
  gchar              *id;
  gchar              *name;
  gchar              *description;
  gchar             **keywords;
  CcPanelVisibility   visibility;
} RowData;

struct _CcPanelList
{
  AdwBin              parent;

  GtkWidget          *main_listbox;
  GtkWidget          *search_listbox;
  GtkStack           *stack;

  /* When clicking on Details or Devices row, show it
   * automatically select the first panel of the list.
   */
  gboolean            autoselect_panel : 1;

  gchar              *current_panel_id;
  gchar              *search_query;
  gchar             **search_words;

  CcPanelListView     previous_view;
  CcPanelListView     view;
  GHashTable         *id_to_data;
  GHashTable         *id_to_search_data;

  /* When true, the next row being activated will be vertically centered on
   * the visible part of panel list. Currently we do that for panels activated
   * from Search or from the set_active_panel_from_id() CcShell iface */
  gboolean            center_activated_row;
};

G_DEFINE_TYPE (CcPanelList, cc_panel_list, ADW_TYPE_BIN)

enum
{
  PROP_0,
  PROP_SEARCH_MODE,
  PROP_SEARCH_QUERY,
  PROP_VIEW,
  N_PROPS
};

enum
{
  SHOW_PANEL,
  LAST_SIGNAL
};

static GParamSpec *properties [N_PROPS] = { NULL, };
static gint signals [LAST_SIGNAL] = { 0, };

/*
 * Auxiliary methods
 */
static GtkWidget*
get_widget_from_view (CcPanelList     *self,
                      CcPanelListView  view)
{
  switch (view)
    {
    case CC_PANEL_LIST_MAIN:
      return self->main_listbox;

    case CC_PANEL_LIST_SEARCH:
      return self->search_listbox;

    default:
      return NULL;
    }
}

static void
activate_row_below (CcPanelList *self,
                    RowData     *data)
{
  GtkListBoxRow *next_row;
  guint row_index;

  row_index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (data->row));
  next_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->main_listbox),
                                            row_index + 1);

  /* Try the previous one if the current is invalid */
  if (!next_row)
    next_row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (self->main_listbox),
                                              row_index - 1);

  if (next_row)
    g_signal_emit_by_name (next_row, "activate");
}

static CcPanelListView
get_view_from_listbox (CcPanelList *self,
                       GtkWidget   *listbox)
{
  if (listbox == self->main_listbox)
    return CC_PANEL_LIST_MAIN;

  return CC_PANEL_LIST_SEARCH;
}

static void
switch_to_view (CcPanelList     *self,
                CcPanelListView  view)
{
  GtkWidget *visible_child;
  gboolean should_crossfade;

  CC_ENTRY;

  if (self->view == view)
    CC_RETURN ();

  CC_TRACE_MSG ("Switching to view: %d", view);

  self->previous_view = self->view;
  self->view = view;

  /*
   * When changing to or from the search view, the animation should
   * be crossfade. Otherwise, it's the previous-forward movement.
   */
  should_crossfade = view == CC_PANEL_LIST_SEARCH ||
                     self->previous_view == CC_PANEL_LIST_SEARCH;

  gtk_stack_set_transition_type (self->stack,
                                 should_crossfade ? GTK_STACK_TRANSITION_TYPE_CROSSFADE :
                                                    GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

  visible_child = get_widget_from_view (self, view);

  gtk_stack_set_visible_child (self->stack, visible_child);

  /* For non-search views, make sure the displayed panel matches the
   * newly selected row
   */
  if (self->autoselect_panel &&
      view != CC_PANEL_LIST_SEARCH)
    {
      cc_panel_list_activate (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_VIEW]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_MODE]);

  CC_EXIT;
}

static void
update_search (CcPanelList *self)
{
  /*
   * Only change to the search view is there's a
   * search query available.
   */
  if (self->search_query &&
      g_utf8_strlen (self->search_query, -1) > 0)
    {
      if (self->view == CC_PANEL_LIST_MAIN)
        switch_to_view (self, CC_PANEL_LIST_SEARCH);
    }
  else if (self->view == CC_PANEL_LIST_SEARCH)
    {
      /* Don't autoselect first panel when going back from search view */
      self->autoselect_panel = FALSE;

      switch_to_view (self, self->previous_view);
    }

  gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->search_listbox));
  gtk_list_box_unselect_all (GTK_LIST_BOX (self->search_listbox));
}

static const gchar*
get_panel_id_from_row (CcPanelList   *self,
                       GtkListBoxRow *row)
{

  RowData *row_data = g_object_get_data (G_OBJECT (row), "data");

  g_assert (row_data != NULL);
  return row_data->id;
}

/*
 * RowData functions
 */
static void
row_data_free (RowData *data)
{
  g_strfreev (data->keywords);
  g_free (data->description);
  g_free (data->name);
  g_free (data->id);
  g_free (data);
}

static RowData*
row_data_new (CcPanelCategory     category,
              const gchar        *id,
              const gchar        *name,
              const gchar        *description,
              const GStrv         keywords,
              const gchar        *icon,
              CcPanelVisibility   visibility)
{
  GtkWidget *label, *grid, *image;
  RowData *data;

  data = g_new0 (RowData, 1);
  data->category = category;
  data->row = gtk_list_box_row_new ();
  data->id = g_strdup (id);
  data->name = g_strdup (name);
  data->description = g_strdup (description);
  data->keywords = g_strdupv (keywords);

  /* Setup the row */
  grid = gtk_grid_new ();
  gtk_widget_set_hexpand (grid, TRUE);
  gtk_widget_set_margin_top (grid, 12);
  gtk_widget_set_margin_bottom (grid, 12);
  gtk_widget_set_margin_start (grid, 6);
  gtk_widget_set_margin_end (grid, 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);

  /* Icon */
  image = gtk_image_new_from_icon_name (icon);

  gtk_grid_attach (GTK_GRID (grid), image, 0, 0, 1, 1);

  /* Name label */
  label = gtk_label_new (name);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), label, 1, 0, 1, 1);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (data->row),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY,
                                  label,
                                  NULL,
                                  -1);

  /* Description label */
  label = gtk_label_new (description);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_hexpand (label, TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 25);
  gtk_label_set_wrap (GTK_LABEL (label), TRUE);
  gtk_widget_set_visible (label, FALSE);
  gtk_accessible_update_relation (GTK_ACCESSIBLE (data->row),
                                  GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                                  label,
                                  NULL,
                                  -1);

  gtk_widget_add_css_class (label, "dim-label");
  gtk_grid_attach (GTK_GRID (grid), label, 1, 1, 1, 1);

  data->description_label = label;

  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (data->row), grid);

  g_object_set_data_full (G_OBJECT (data->row), "data", data, (GDestroyNotify) row_data_free);

  data->visibility = visibility;

  return data;
}

/*
 * GtkListBox functions
 */
static gboolean
filter_func (GtkListBoxRow *row,
             gpointer       user_data)
{
  CcPanelList *self;
  RowData *data;
  g_autofree gchar *panel_text = NULL;
  g_autofree gchar *panel_description = NULL;
  gboolean retval = TRUE;
  gint i, j;

  self = CC_PANEL_LIST (user_data);
  data = g_object_get_data (G_OBJECT (row), "data");

  if (!self->search_words)
    return TRUE;

  panel_text = cc_util_normalize_casefold_and_unaccent (data->name);
  panel_description = cc_util_normalize_casefold_and_unaccent (data->description);

  g_strstrip (panel_text);
  g_strstrip (panel_description);

  /*
   * The description label is only visible when the search is
   * happening.
   */
  gtk_widget_set_visible (data->description_label, self->view == CC_PANEL_LIST_SEARCH);

  for (j = 0; retval && self->search_words[j] != NULL; j++) {
    const gchar *search_word = self->search_words[j];
    gboolean match = FALSE;

    if (search_word[0] == '\0')
      continue;

    // Compare keywords
    for (i = 0; !match && data->keywords[i] != NULL; i++)
      match = (strstr (data->keywords[i], search_word) == data->keywords[i]);

    // Compare panel title and description
    match = match || (g_strstr_len (panel_text, -1, search_word) != NULL ||
                      g_strstr_len (panel_description, -1, search_word) != NULL);

    // All search words must match
    retval = retval && match;
  }

  return retval;
}

static const gchar * const panel_order[] = {
  /* Main page */
  "wifi",
  "network",
  "wwan",
  "mobile-broadband",
  "bluetooth",

  "separator",

  "display",
  "sound",
  "power",
  "multitasking",
  "background",

  "separator",

  "applications",
  "notifications",
  "search",
  "online-accounts",
  "sharing",

  "separator",

  "mouse",
  "keyboard",
  "color",
  "printers",
  "wacom",

  "separator",

  "universal-access",
  "privacy",
  "system",
  "reset-settings",
};

static guint
get_panel_id_index (const gchar *panel_id)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (panel_order); i++)
    {
      if (g_str_equal (panel_order[i], panel_id))
        return i;
    }

  return 0;
}

static gint
sort_function (GtkListBoxRow *a,
               GtkListBoxRow *b,
               gpointer       user_data)
{
  CcPanelList *self = CC_PANEL_LIST (user_data);
  const gchar *a_id, *b_id;

  a_id = get_panel_id_from_row (self, a);
  b_id = get_panel_id_from_row (self, b);

  return get_panel_id_index (a_id) - get_panel_id_index (b_id);
}


/* FIXME: This is now different from the "match all words" search.
          Maybe add a search score based on number of matches in filter_func()? */
static gint
search_sort_function (GtkListBoxRow *a,
                      GtkListBoxRow *b,
                      gpointer       user_data)
{
  CcPanelList *self;
  RowData *a_data, *b_data;
  g_autofree gchar *a_name = NULL;
  g_autofree gchar *b_name = NULL;
  g_autofree gchar *search = NULL;
  gchar *a_strstr, *b_strstr;
  gint a_distance, b_distance;

  self = CC_PANEL_LIST (user_data);
  search = NULL;
  a_data = g_object_get_data (G_OBJECT (a), "data");
  b_data = g_object_get_data (G_OBJECT (b), "data");

  a_distance = b_distance = G_MAXINT;

  a_name = cc_util_normalize_casefold_and_unaccent (a_data->name);
  b_name = cc_util_normalize_casefold_and_unaccent (b_data->name);
  g_strstrip (a_name);
  g_strstrip (b_name);

  if (self->search_query)
    {
      search = cc_util_normalize_casefold_and_unaccent (self->search_query);
      g_strstrip (search);
    }

  /* Default result for empty search */
  if (!search || g_utf8_strlen (search, -1) == 0)
    return g_strcmp0 (a_name, b_name);

  a_strstr = g_strstr_len (a_name, -1, search);
  b_strstr = g_strstr_len (b_name, -1, search);

  if (a_strstr)
    a_distance = g_strstr_len (a_name, -1, search) - a_name;

  if (b_strstr)
    b_distance = g_strstr_len (b_name, -1, search) - b_name;

  return a_distance - b_distance;
}

static void
header_func (GtkListBoxRow *row,
             GtkListBoxRow *before,
             gpointer       user_data)
{
  guint pid;

  if (!before)
    return;

  pid = get_panel_id_index (get_panel_id_from_row (user_data, row));
  if (pid > 0 && g_str_equal (panel_order[pid-1], "separator"))
    {
      GtkWidget *separator;

      separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_set_hexpand (separator, TRUE);

      gtk_list_box_row_set_header (row, separator);
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

/*
 * Callbacks
 */
static void
row_activated_cb (GtkWidget     *listbox,
                  GtkListBoxRow *row,
                  CcPanelList   *self)
{
  RowData *data;
  const gchar *parent_panel = 0;

  /*
   * Since we're not sure that the activated row is in the
   * current view, set the view here.
   */
  switch_to_view (self, get_view_from_listbox (self, listbox));

  data = g_object_get_data (G_OBJECT (row), "data");
  if (data->category == CC_CATEGORY_SYSTEM)
    parent_panel = "system";
  else if (data->category == CC_CATEGORY_PRIVACY)
    parent_panel = "privacy";

  g_signal_emit (self, signals[SHOW_PANEL], 0, data->id, parent_panel);

  /* After selecting the panel and eventually changing the view, reset the
   * autoselect flag. If necessary, cc_panel_list_set_active_panel() will
   * set it to FALSE again.
   */
  self->autoselect_panel = TRUE;
}

static void
search_row_activated_cb (GtkWidget     *listbox,
                         GtkListBoxRow *row,
                         CcPanelList   *self)
{
  GtkWidget *child;
  RowData *data;

  CC_ENTRY;

  data = g_object_get_data (G_OBJECT (row), "data");

  /* Select the correct row */
  for (child = gtk_widget_get_first_child (self->main_listbox);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      RowData *real_row_data;

      real_row_data = g_object_get_data (G_OBJECT (child), "data");

      /*
       * The main listbox has the Details & Devices rows, and neither
       * of them contains "data", so we have to ensure we have valid
       * data before going on.
       */
      if (!real_row_data)
        continue;

      if (g_strcmp0 (real_row_data->id, data->id) == 0)
        {
          GtkListBoxRow *real_row;

          real_row = GTK_LIST_BOX_ROW (real_row_data->row);

          gtk_list_box_select_row (GTK_LIST_BOX (self->main_listbox), real_row);
          gtk_widget_grab_focus (GTK_WIDGET (real_row));

          /* Don't autoselect first panel because we are already
           * activating a panel from search result */
          self->autoselect_panel = FALSE;

          /* center Search activated row on panel list */
          self->center_activated_row = TRUE;
          g_signal_emit_by_name (real_row, "activate");
          break;
        }
    }

  CC_EXIT;
}

static void
cc_panel_list_finalize (GObject *object)
{
  CcPanelList *self = (CcPanelList *)object;

  g_clear_pointer (&self->search_query, g_free);
  g_clear_pointer (&self->search_words, g_strfreev);
  g_clear_pointer (&self->current_panel_id, g_free);
  g_clear_pointer (&self->id_to_data, g_hash_table_destroy);
  g_clear_pointer (&self->id_to_search_data, g_hash_table_destroy);

  G_OBJECT_CLASS (cc_panel_list_parent_class)->finalize (object);
}

static void
cc_panel_list_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CcPanelList *self = CC_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE:
      g_value_set_boolean (value, self->view == CC_PANEL_LIST_SEARCH);
      break;

    case PROP_SEARCH_QUERY:
      g_value_set_string (value, self->search_query);
      break;

    case PROP_VIEW:
      g_value_set_int (value, self->view);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_panel_list_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcPanelList *self = CC_PANEL_LIST (object);

  switch (prop_id)
    {
    case PROP_SEARCH_MODE:
      update_search (self);
      break;

    case PROP_SEARCH_QUERY:
      cc_panel_list_set_search_query (self, g_value_get_string (value));
      break;

    case PROP_VIEW:
      switch_to_view (self, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
search_list_keynav_failed_cb (CcPanelList *self,
                              GtkDirectionType direction)
{
  GtkWidget *toplevel;

  /* We are in the first result of search list and pressing Arrow Up,
   * so then we move focus back to search text entry */
  if (direction == GTK_DIR_UP)
    {
      toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

      if (!toplevel)
        return FALSE;

      return gtk_widget_child_focus (toplevel, GTK_DIR_TAB_BACKWARD);
    }

  return FALSE;
}


static void
cc_panel_list_class_init (CcPanelListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_panel_list_finalize;
  object_class->get_property = cc_panel_list_get_property;
  object_class->set_property = cc_panel_list_set_property;

  /**
   * CcPanelList:show-panel:
   *
   * Emitted when a panel is selected.
   */
  signals[SHOW_PANEL] = g_signal_new ("show-panel",
                                      CC_TYPE_PANEL_LIST,
                                      G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL, NULL,
                                      G_TYPE_NONE,
                                      2,
                                      G_TYPE_STRING,
                                      G_TYPE_STRING);

  /**
   * CcPanelList:search-mode:
   *
   * Whether the search is visible or not.
   */
  properties[PROP_SEARCH_MODE] = g_param_spec_boolean ("search-mode",
                                                       "Search mode",
                                                       "Whether it's in search mode or not",
                                                       FALSE,
                                                       G_PARAM_READWRITE);

  /**
   * CcPanelList:search-query:
   *
   * The search that is being applied to sidelist.
   */
  properties[PROP_SEARCH_QUERY] = g_param_spec_string ("search-query",
                                                       "Search query",
                                                       "The current search query",
                                                       NULL,
                                                       G_PARAM_READWRITE);

  /**
   * CcPanelList:view:
   *
   * The current view of the sidelist.
   */
  properties[PROP_VIEW] = g_param_spec_int ("view",
                                            "View",
                                            "The current view of the sidelist",
                                            CC_PANEL_LIST_MAIN,
                                            CC_PANEL_LIST_SEARCH,
                                            CC_PANEL_LIST_MAIN,
                                            G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/Settings/gtk/cc-panel-list.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPanelList, main_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPanelList, search_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcPanelList, stack);

  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_list_keynav_failed_cb);
}

static void
cc_panel_list_init (CcPanelList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->id_to_data = g_hash_table_new (g_str_hash, g_str_equal);
  self->id_to_search_data = g_hash_table_new (g_str_hash, g_str_equal);
  self->view = CC_PANEL_LIST_MAIN;

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->main_listbox),
                              sort_function,
                              self,
                              NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->main_listbox),
                                header_func,
                                self,
                                NULL);

  /* Search listbox */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->search_listbox),
                              search_sort_function,
                              self,
                              NULL);

  gtk_list_box_set_filter_func (GTK_LIST_BOX (self->search_listbox),
                                filter_func,
                                self,
                                NULL);
}

GtkWidget*
cc_panel_list_new (void)
{
  return g_object_new (CC_TYPE_PANEL_LIST, NULL);
}

void
cc_panel_list_center_activated_row (CcPanelList *self,
                                    gboolean     val)
{
  g_return_if_fail (CC_IS_PANEL_LIST (self));

  if (self->center_activated_row != val)
    self->center_activated_row = val;
}

gboolean
cc_panel_list_activate (CcPanelList *self)
{
  GtkListBoxRow *row;
  GtkWidget *listbox;
  guint i = 0;

  CC_ENTRY;

  g_return_val_if_fail (CC_IS_PANEL_LIST (self), FALSE);

  listbox = get_widget_from_view (self, self->view);
  if (!GTK_IS_LIST_BOX (listbox))
    CC_RETURN (FALSE);

  /* Select the first visible row */
  do
    row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (listbox), i++);
  while (row && !(gtk_widget_get_visible (GTK_WIDGET (row)) &&
         gtk_widget_get_child_visible (GTK_WIDGET (row))));

  /* If the row is valid, activate it */
  if (row)
    {
      gtk_list_box_select_row (GTK_LIST_BOX (listbox), row);
      gtk_widget_grab_focus (GTK_WIDGET (row));

      g_signal_emit_by_name (row, "activate");
    }

  CC_RETURN (row != NULL);
}

const gchar*
cc_panel_list_get_search_query (CcPanelList *self)
{
  g_return_val_if_fail (CC_IS_PANEL_LIST (self), NULL);

  return self->search_query;
}

void
cc_panel_list_set_search_query (CcPanelList *self,
                                const gchar *search)
{
  g_return_if_fail (CC_IS_PANEL_LIST (self));

  if (g_strcmp0 (self->search_query, search) != 0)
    {
      g_autofree gchar *search_query_normalized;

      g_clear_pointer (&self->search_query, g_free);
      g_clear_pointer (&self->search_words, g_strfreev);

      self->search_query = g_strdup (search);

      /* Split on spaces */
      search_query_normalized = cc_util_normalize_casefold_and_unaccent (search);
      self->search_words = g_strsplit (g_strstrip (search_query_normalized), " ", 0);

      update_search (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_QUERY]);

      gtk_list_box_invalidate_filter (GTK_LIST_BOX (self->search_listbox));
      gtk_list_box_invalidate_sort (GTK_LIST_BOX (self->search_listbox));
    }
}

CcPanelListView
cc_panel_list_get_view (CcPanelList *self)
{
  g_return_val_if_fail (CC_IS_PANEL_LIST (self), -1);

  return self->view;
}

/**
 * cc_panel_list_get_current_panel:
 * @self: a #CcPanelList
 *
 * Returns: (allow-none): id string of current active panel on @self, or %NULL when there's none yet.
 */
const gchar*
cc_panel_list_get_current_panel (CcPanelList *self)
{
  g_return_val_if_fail (CC_IS_PANEL_LIST (self), NULL);

  return self->current_panel_id;
}

void
cc_panel_list_add_panel (CcPanelList        *self,
                         CcPanelCategory     category,
                         const gchar        *id,
                         const gchar        *title,
                         const gchar        *description,
                         const GStrv         keywords,
                         const gchar        *icon,
                         CcPanelVisibility   visibility)
{
  RowData *data, *search_data;

  g_return_if_fail (CC_IS_PANEL_LIST (self));

  /* Add the panel to the proper listbox */
  data = row_data_new (category, id, title, description, keywords, icon, visibility);
  gtk_widget_set_visible (data->row, visibility == CC_PANEL_VISIBLE);

  gtk_list_box_append (GTK_LIST_BOX (self->main_listbox), data->row);

  /* And add to the search listbox too */
  search_data = row_data_new (category, id, title, description, keywords, icon, visibility);
  gtk_widget_set_visible (search_data->row, visibility != CC_PANEL_HIDDEN);

  gtk_list_box_append (GTK_LIST_BOX (self->search_listbox), search_data->row);

  g_hash_table_insert (self->id_to_data, data->id, data);
  g_hash_table_insert (self->id_to_search_data, search_data->id, search_data);
}

/* Scrolls sibebar so that @row is at middle of the visible part of list */
static void
cc_panel_list_scroll_to_center_row (CcPanelList *self,
                                    GtkWidget *row)
{
  double target_value;
  graphene_point_t p;
  GtkAdjustment *adj;
  GtkWidget *scrolled_window;

  g_return_if_fail (GTK_IS_LIST_BOX_ROW (row));

  scrolled_window = gtk_widget_get_ancestor (row, GTK_TYPE_SCROLLED_WINDOW);
  adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window));
  if (!adj)
    return;

  if (!gtk_widget_compute_point (row, GTK_WIDGET (self), &GRAPHENE_POINT_INIT (0, 0), &p))
    return;

  target_value = p.y + gtk_widget_get_height (row) / 2;

  gtk_adjustment_set_value (adj, target_value - gtk_adjustment_get_page_size (adj) / 2);
}

typedef struct {
  CcPanelList *panel_list;
  GtkWidget *row;
} ScrollData;

static gboolean
scroll_to_idle_cb (ScrollData *data)
{
  cc_panel_list_scroll_to_center_row (data->panel_list, data->row);

  g_object_unref (data->panel_list);
  g_object_unref (data->row);
  g_free (data);

  return FALSE;
}

/**
 * cc_panel_list_set_active_panel:
 * @self: a #CcPanelList
 * @id: the id of the panel to be activated
 *
 * Sets the current active panel.
 */
void
cc_panel_list_set_active_panel (CcPanelList *self,
                                const gchar *id)
{
  GtkWidget *listbox;
  RowData *data;
  ScrollData *sdata;
  gboolean scroll_to_center = FALSE;

  g_return_if_fail (CC_IS_PANEL_LIST (self));

  data = g_hash_table_lookup (self->id_to_data, id);

  g_assert (data != NULL);

  if (self->center_activated_row)
    {
      scroll_to_center = TRUE;
      self->center_activated_row = FALSE;
    }

  /* Stop if row is supposed to be always hidden */
  if (data->visibility == CC_PANEL_HIDDEN)
    {
      g_debug ("Panel '%s' is always hidden, stopping.", id);
      cc_panel_list_activate (self);
      return;
    }

  /* If the currently selected panel is not always visible, for example when
   * the panel is only visible on search and we're temporarily seeing it, make
   * sure to hide it after the user moves out.
   */
  if (self->current_panel_id != NULL && g_strcmp0 (self->current_panel_id, id) != 0)
    {
      RowData *current_row_data;

      current_row_data = g_hash_table_lookup (self->id_to_data, self->current_panel_id);

      /* We cannot be showing a non-existent panel */
      g_assert (current_row_data != NULL);

      gtk_widget_set_visible (current_row_data->row, current_row_data->visibility == CC_PANEL_VISIBLE);
    }

  listbox = gtk_widget_get_parent (data->row);

  /* The row might be hidden now, so make sure it's visible */
  gtk_widget_set_visible (data->row, TRUE);

  gtk_list_box_select_row (GTK_LIST_BOX (listbox), GTK_LIST_BOX_ROW (data->row));
  gtk_widget_grab_focus (data->row);

  /* When setting the active panel programatically, prevent from
   * autoselecting the first panel of the new view.
   */
  self->autoselect_panel = FALSE;

  g_signal_emit_by_name (data->row, "activate");

  /* Store the current panel id */
  g_clear_pointer (&self->current_panel_id, g_free);
  self->current_panel_id = g_strdup (id);

  /* This centering is currently set for panels activated from Search
   * or from set_active_panel_from_id() CcShell iface */
  if (scroll_to_center)
    {
      /* Scroll the sidebar to the selected panel row, as that row may be
       * out of view when panel is launched from a search or from cli */
      sdata = g_new (ScrollData, 1);
      sdata->panel_list = g_object_ref (self);
      sdata->row = g_object_ref (data->row);

      g_idle_add (G_SOURCE_FUNC (scroll_to_idle_cb), sdata);
    }
}

/**
 * cc_panel_list_set_panel_visibility:
 * @self: a #CcPanelList
 * @id: the id of the panel
 * @visibility: visibility of panel with @id
 *
 * Sets the visibility of panel with @id. @id must be a valid
 * id with a corresponding panel.
 */
void
cc_panel_list_set_panel_visibility (CcPanelList       *self,
                                    const gchar       *id,
                                    CcPanelVisibility  visibility)
{
  RowData *data, *search_data;

  g_return_if_fail (CC_IS_PANEL_LIST (self));

  data = g_hash_table_lookup (self->id_to_data, id);
  search_data = g_hash_table_lookup (self->id_to_search_data, id);

  g_assert (data != NULL);
  g_assert (search_data != NULL);

  data->visibility = visibility;

  /* If this is the currently selected row, and the panel can't be displayed
   * (i.e. visibility != VISIBLE), then select the next possible row */
  if (gtk_list_box_row_is_selected (GTK_LIST_BOX_ROW (data->row)) &&
      visibility != CC_PANEL_VISIBLE)
    {
      activate_row_below (self, data);
    }

  gtk_widget_set_visible (data->row, visibility == CC_PANEL_VISIBLE);
  gtk_widget_set_visible (search_data->row, visibility =! CC_PANEL_HIDDEN);
}

void
cc_panel_list_set_selection_mode (CcPanelList      *self,
                                  GtkSelectionMode  selection_mode)
{
  g_return_if_fail (CC_IS_PANEL_LIST (self));

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->main_listbox), selection_mode);

  /* When selection mode changed, selection will be lost.  So reselect */
  if (selection_mode == GTK_SELECTION_SINGLE && self->current_panel_id)
    {
      GtkWidget *listbox;
      RowData *data;

      data = g_hash_table_lookup (self->id_to_data, self->current_panel_id);
      listbox = gtk_widget_get_parent (data->row);
      gtk_list_box_select_row (GTK_LIST_BOX (listbox), GTK_LIST_BOX_ROW (data->row));
    }
}

