#include "suggestionentry.h"

struct _MatchObject
{
  GObject parent_instance;

  GObject *item;
  char *string;
  guint match_start;
  guint match_end;
  guint score;
};

typedef struct
{
  GObjectClass parent_class;
} MatchObjectClass;

enum
{
  PROP_ITEM = 1,
  PROP_STRING,
  PROP_MATCH_START,
  PROP_MATCH_END,
  PROP_SCORE,
  N_MATCH_PROPERTIES
};

static GParamSpec *match_properties[N_MATCH_PROPERTIES];

G_DEFINE_TYPE (MatchObject, match_object, G_TYPE_OBJECT)

static void
match_object_init (MatchObject *object)
{
}

static void
match_object_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  MatchObject *self = MATCH_OBJECT (object);

  switch (property_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_STRING:
      g_value_set_string (value, self->string);
      break;

    case PROP_MATCH_START:
      g_value_set_uint (value, self->match_start);
      break;

    case PROP_MATCH_END:
      g_value_set_uint (value, self->match_end);
      break;

    case PROP_SCORE:
      g_value_set_uint (value, self->score);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
match_object_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  MatchObject *self = MATCH_OBJECT (object);

  switch (property_id)
    {
    case PROP_ITEM:
      self->item = g_value_get_object (value);
      break;

    case PROP_STRING:
      self->string = g_value_dup_string (value);
      break;

    case PROP_MATCH_START:
      if (self->match_start != g_value_get_uint (value))
        {
          self->match_start = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_MATCH_END:
      if (self->match_end != g_value_get_uint (value))
        {
          self->match_end = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    case PROP_SCORE:
      if (self->score != g_value_get_uint (value))
        {
          self->score = g_value_get_uint (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
match_object_dispose (GObject *object)
{
  MatchObject *self = MATCH_OBJECT (object);

  g_clear_object (&self->item);
  g_clear_pointer (&self->string, g_free);

  G_OBJECT_CLASS (match_object_parent_class)->dispose (object);
}

static void
match_object_class_init (MatchObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = match_object_dispose;
  object_class->get_property = match_object_get_property;
  object_class->set_property = match_object_set_property;

  match_properties[PROP_ITEM]
      = g_param_spec_object ("item", "Item", "Item",
                             G_TYPE_OBJECT,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);
  match_properties[PROP_STRING]
      = g_param_spec_string ("string", "String", "String",
                             NULL,
                             G_PARAM_READWRITE |
                             G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS);
  match_properties[PROP_MATCH_START]
      = g_param_spec_uint ("match-start", "Match Start", "Match Start",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);
  match_properties[PROP_MATCH_END]
      = g_param_spec_uint ("match-end", "Match End", "Match End",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);
  match_properties[PROP_SCORE]
      = g_param_spec_uint ("score", "Score", "Score",
                           0, G_MAXUINT, 0,
                           G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_MATCH_PROPERTIES, match_properties);
}

static MatchObject *
match_object_new (gpointer    item,
                  const char *string)
{
  return g_object_new (MATCH_TYPE_OBJECT,
                       "item", item,
                       "string", string,
                       NULL);
}

gpointer
match_object_get_item (MatchObject *object)
{
  return object->item;
}

const char *
match_object_get_string (MatchObject *object)
{
  return object->string;
}

guint
match_object_get_match_start (MatchObject *object)
{
  return object->match_start;
}

guint
match_object_get_match_end (MatchObject *object)
{
  return object->match_end;
}

guint
match_object_get_score (MatchObject *object)
{
  return object->score;
}

void
match_object_set_match (MatchObject *object,
                        guint           start,
                        guint           end,
                        guint           score)
{
  g_object_freeze_notify (G_OBJECT (object));

  g_object_set (object,
                "match-start", start,
                "match-end", end,
                "score", score,
                NULL);

  g_object_thaw_notify (G_OBJECT (object));
}

/* ---- */

struct _SuggestionEntry
{
  GtkWidget parent_instance;

  GListModel *model;
  GtkListItemFactory *factory;
  GtkExpression *expression;

  GtkFilter *filter;
  GtkMapListModel *map_model;
  GtkSingleSelection *selection;

  GtkWidget *entry;
  GtkWidget *arrow;
  GtkWidget *popup;
  GtkWidget *list;

  char *search;

  SuggestionEntryMatchFunc match_func;
  gpointer match_data;
  GDestroyNotify destroy;

  gulong changed_id;

  guint use_filter : 1;
  guint show_arrow : 1;
};

typedef struct _SuggestionEntryClass SuggestionEntryClass;

struct _SuggestionEntryClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_MODEL,
  PROP_FACTORY,
  PROP_EXPRESSION,
  PROP_PLACEHOLDER_TEXT,
  PROP_POPUP_VISIBLE,
  PROP_USE_FILTER,
  PROP_SHOW_ARROW,

  N_PROPERTIES,
};

static void suggestion_entry_set_popup_visible (SuggestionEntry *self,
                                                gboolean         visible);

static GtkEditable *
suggestion_entry_get_delegate (GtkEditable *editable)
{
  return GTK_EDITABLE (SUGGESTION_ENTRY (editable)->entry);
}

static void
suggestion_entry_editable_init (GtkEditableInterface *iface)
{
  iface->get_delegate = suggestion_entry_get_delegate;
}

G_DEFINE_TYPE_WITH_CODE (SuggestionEntry, suggestion_entry, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE,
                                                suggestion_entry_editable_init))

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void
suggestion_entry_dispose (GObject *object)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (object);

  if (self->changed_id)
    {
      g_signal_handler_disconnect (self->entry, self->changed_id);
      self->changed_id = 0;
    }
  g_clear_pointer (&self->entry, gtk_widget_unparent);
  g_clear_pointer (&self->arrow, gtk_widget_unparent);
  g_clear_pointer (&self->popup, gtk_widget_unparent);

  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->factory);

  g_clear_object (&self->model);
  g_clear_object (&self->map_model);
  g_clear_object (&self->selection);

  g_clear_pointer (&self->search, g_free);

  if (self->destroy)
    self->destroy (self->match_data);

  G_OBJECT_CLASS (suggestion_entry_parent_class)->dispose (object);
}

static void
suggestion_entry_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (object);

  if (gtk_editable_delegate_get_property (object, property_id, value, pspec))
    return;

  switch (property_id)
    {
    case PROP_MODEL:
      g_value_set_object (value, suggestion_entry_get_model (self));
      break;

    case PROP_FACTORY:
      g_value_set_object (value, suggestion_entry_get_factory (self));
      break;

    case PROP_EXPRESSION:
      gtk_value_set_expression (value, suggestion_entry_get_expression (self));
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, gtk_text_get_placeholder_text (GTK_TEXT (self->entry)));
      break;

    case PROP_POPUP_VISIBLE:
      g_value_set_boolean (value, self->popup && gtk_widget_get_visible (self->popup));
      break;

    case PROP_USE_FILTER:
      g_value_set_boolean (value, suggestion_entry_get_use_filter (self));
      break;

    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, suggestion_entry_get_show_arrow (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
suggestion_entry_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (object);

  if (gtk_editable_delegate_set_property (object, property_id, value, pspec))
    return;

  switch (property_id)
    {
    case PROP_MODEL:
      suggestion_entry_set_model (self, g_value_get_object (value));
      break;

    case PROP_FACTORY:
      suggestion_entry_set_factory (self, g_value_get_object (value));
      break;

    case PROP_EXPRESSION:
      suggestion_entry_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_PLACEHOLDER_TEXT:
      gtk_text_set_placeholder_text (GTK_TEXT (self->entry), g_value_get_string (value));
      break;

    case PROP_POPUP_VISIBLE:
      suggestion_entry_set_popup_visible (self, g_value_get_boolean (value));
      break;

    case PROP_USE_FILTER:
      suggestion_entry_set_use_filter (self, g_value_get_boolean (value));
      break;

    case PROP_SHOW_ARROW:
      suggestion_entry_set_show_arrow (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
suggestion_entry_measure (GtkWidget      *widget,
                          GtkOrientation  orientation,
                          int             for_size,
                          int            *minimum,
                          int            *natural,
                          int            *minimum_baseline,
                          int            *natural_baseline)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (widget);
  int arrow_min = 0, arrow_nat = 0;

  gtk_widget_measure (self->entry, orientation, for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_measure (self->arrow, orientation, for_size,
                        &arrow_min, &arrow_nat,
                        NULL, NULL);
}

static void
suggestion_entry_size_allocate (GtkWidget *widget,
                                int        width,
                                int        height,
                                int        baseline)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (widget);
  int arrow_min = 0, arrow_nat = 0;

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_measure (self->arrow, GTK_ORIENTATION_HORIZONTAL, -1,
                        &arrow_min, &arrow_nat,
                        NULL, NULL);

  gtk_widget_size_allocate (self->entry,
                            &(GtkAllocation) { 0, 0, width - arrow_nat, height },
                            baseline);

  if (self->arrow && gtk_widget_get_visible (self->arrow))
    gtk_widget_size_allocate (self->arrow,
                              &(GtkAllocation) { width - arrow_nat, 0, arrow_nat, height },
                              baseline);

  gtk_widget_set_size_request (self->popup, gtk_widget_get_width (GTK_WIDGET (self)), -1);
  gtk_widget_queue_resize (self->popup);

  gtk_popover_present (GTK_POPOVER (self->popup));
}

static gboolean
suggestion_entry_grab_focus (GtkWidget *widget)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (widget);

  return gtk_widget_grab_focus (self->entry);
}

static void
suggestion_entry_class_init (SuggestionEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = suggestion_entry_dispose;
  object_class->get_property = suggestion_entry_get_property;
  object_class->set_property = suggestion_entry_set_property;

  widget_class->measure = suggestion_entry_measure;
  widget_class->size_allocate = suggestion_entry_size_allocate;
  widget_class->grab_focus = suggestion_entry_grab_focus;

  properties[PROP_MODEL] =
    g_param_spec_object ("model",
                         "Model",
                         "Model for the displayed items",
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FACTORY] =
    g_param_spec_object ("factory",
                         "Factory",
                         "Factory for populating list items",
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression",
                               "Expression",
                               "Expression to determine strings to search for",
                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_PLACEHOLDER_TEXT] =
      g_param_spec_string ("placeholder-text",
                           "Placeholder text",
                           "Show text in the entry when it’s empty and unfocused",
                           NULL,
                           G_PARAM_READWRITE);

  properties[PROP_POPUP_VISIBLE] =
      g_param_spec_boolean ("popup-visible",
                            "Popup visible",
                            "Whether the popup with suggestions is currently visible",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_USE_FILTER] =
      g_param_spec_boolean ("use-filter",
                            "Use filter",
                            "Whether to filter the list for matches",
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SHOW_ARROW] =
      g_param_spec_boolean ("show-arrow",
                            "Show arrow",
                            "Whether to show a clickable arrow for presenting the popup",
                            FALSE,
                            G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
  gtk_editable_install_properties (object_class, N_PROPERTIES);

  gtk_widget_class_install_property_action (widget_class, "popup.show", "popup-visible");

  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Down, GDK_ALT_MASK,
                                       "popup.show", NULL);

  gtk_widget_class_set_css_name (widget_class, "entry");
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  gpointer item;
  GtkWidget *label;
  GValue value = G_VALUE_INIT;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (label), match_object_get_string (MATCH_OBJECT (item)));
  g_value_unset (&value);
}

static void
suggestion_entry_set_popup_visible (SuggestionEntry *self,
                                    gboolean         visible)
{
  if (gtk_widget_get_visible (self->popup) == visible)
    return;

  if (g_list_model_get_n_items (G_LIST_MODEL (self->selection)) == 0)
    return;

  if (visible)
    {
      if (!gtk_widget_has_focus (self->entry))
        gtk_text_grab_focus_without_selecting (GTK_TEXT (self->entry));

      gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);
      gtk_popover_popup (GTK_POPOVER (self->popup));
    }
  else
    {
      gtk_popover_popdown (GTK_POPOVER (self->popup));
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_POPUP_VISIBLE]);
}

static void update_map (SuggestionEntry *self);

static gboolean
text_changed_idle (gpointer data)
{
  SuggestionEntry *self = data;
  const char *text;
  guint matches;

  if (!self->map_model)
    return G_SOURCE_REMOVE;

  text = gtk_editable_get_text (GTK_EDITABLE (self->entry));

  g_free (self->search);
  self->search = g_strdup (text);

  update_map (self);

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->selection));

  suggestion_entry_set_popup_visible (self, matches > 0);

  return G_SOURCE_REMOVE;
}

static void
text_changed (GtkEditable        *editable,
              GParamSpec         *pspec,
              SuggestionEntry *self)
{
  /* We need to defer to an idle since GtkText sets selection bounds
   * after notify::text
   */
  g_idle_add (text_changed_idle, self);
}

static void
accept_current_selection (SuggestionEntry *self)
{
  gpointer item;

  item = gtk_single_selection_get_selected_item (self->selection);
  if (!item)
    return;

  g_signal_handler_block (self->entry, self->changed_id);

  gtk_editable_set_text (GTK_EDITABLE (self->entry),
                         match_object_get_string (MATCH_OBJECT (item)));

  gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

  g_signal_handler_unblock (self->entry, self->changed_id);
}

static void
suggestion_entry_row_activated (GtkListView     *listview,
                                guint            position,
                                SuggestionEntry *self)
{
  suggestion_entry_set_popup_visible (self, FALSE);
  accept_current_selection (self);
}

static inline gboolean
keyval_is_cursor_move (guint keyval)
{
  if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
    return TRUE;

  if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
    return TRUE;

  if (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_Page_Down)
    return TRUE;

  return FALSE;
}

#define PAGE_STEP 10

static gboolean
suggestion_entry_key_pressed (GtkEventControllerKey *controller,
                              guint                  keyval,
                              guint                  keycode,
                              GdkModifierType        state,
                              SuggestionEntry       *self)
{
  guint matches;
  guint selected;

  if (state & (GDK_SHIFT_MASK | GDK_ALT_MASK | GDK_CONTROL_MASK))
    return FALSE;

  if (keyval == GDK_KEY_Return ||
      keyval == GDK_KEY_KP_Enter ||
      keyval == GDK_KEY_ISO_Enter)
    {
      suggestion_entry_set_popup_visible (self, FALSE);
      accept_current_selection (self);
      g_free (self->search);
      self->search = g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->entry)));
      update_map (self);

      return TRUE;
    }
  else if (keyval == GDK_KEY_Escape)
    {
      if (gtk_widget_get_mapped (self->popup))
        {
          suggestion_entry_set_popup_visible (self, FALSE);

          g_signal_handler_block (self->entry, self->changed_id);

          gtk_editable_set_text (GTK_EDITABLE (self->entry), self->search ? self->search : "");

          gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);

          g_signal_handler_unblock (self->entry, self->changed_id);
          return TRUE;
       }
    }
  else if (keyval == GDK_KEY_Right ||
           keyval == GDK_KEY_KP_Right)
    {
      gtk_editable_set_position (GTK_EDITABLE (self->entry), -1);
      return TRUE;
    }
  else if (keyval == GDK_KEY_Left ||
           keyval == GDK_KEY_KP_Left)
    {
      return FALSE;
    }
  else if (keyval == GDK_KEY_Tab ||
           keyval == GDK_KEY_KP_Tab ||
           keyval == GDK_KEY_ISO_Left_Tab)
    {
      suggestion_entry_set_popup_visible (self, FALSE);
      return FALSE; /* don't disrupt normal focus handling */
    }

  matches = g_list_model_get_n_items (G_LIST_MODEL (self->selection));
  selected = gtk_single_selection_get_selected (self->selection);

  if (keyval_is_cursor_move (keyval))
    {
      if (keyval == GDK_KEY_Up || keyval == GDK_KEY_KP_Up)
        {
          if (selected == 0)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else
            selected--;
        }
      else if (keyval == GDK_KEY_Down || keyval == GDK_KEY_KP_Down)
        {
          if (selected == matches - 1)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else
            selected++;
        }
      else if (keyval == GDK_KEY_Page_Up)
        {
          if (selected == 0)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = matches - 1;
          else if (selected >= PAGE_STEP)
            selected -= PAGE_STEP;
          else
            selected = 0;
        }
      else if (keyval == GDK_KEY_Page_Down)
        {
          if (selected == matches - 1)
            selected = GTK_INVALID_LIST_POSITION;
          else if (selected == GTK_INVALID_LIST_POSITION)
            selected = 0;
          else if (selected + PAGE_STEP < matches)
            selected += PAGE_STEP;
          else
            selected = matches - 1;
        }

      gtk_list_view_scroll_to (GTK_LIST_VIEW (self->list), selected, GTK_LIST_SCROLL_SELECT, NULL);
      return TRUE;
    }

  return FALSE;
}

static void
suggestion_entry_focus_out (GtkEventController *controller,
                            SuggestionEntry    *self)
{
  if (!gtk_widget_get_mapped (self->popup))
    return;

  suggestion_entry_set_popup_visible (self, FALSE);
  accept_current_selection (self);
}

static void
set_default_factory (SuggestionEntry *self)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();

  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), self);

  suggestion_entry_set_factory (self, factory);

  g_object_unref (factory);
}

static void default_match_func (MatchObject *object,
                                const char  *search,
                                gpointer     data);

static void
suggestion_entry_init (SuggestionEntry *self)
{
  GtkWidget *sw;
  GtkEventController *controller;

  if (!g_object_get_data (G_OBJECT (gdk_display_get_default ()), "suggestion-style"))
    {
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_resource (provider, "/listview_selections/suggestionentry.css");
      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  800);
      g_object_set_data (G_OBJECT (gdk_display_get_default ()), "suggestion-style", provider);
      g_object_unref (provider);
    }

  self->use_filter = TRUE;
  self->show_arrow = FALSE;

  self->match_func = default_match_func;
  self->match_data = NULL;
  self->destroy = NULL;

  gtk_widget_add_css_class (GTK_WIDGET (self), "suggestion");

  self->entry = gtk_text_new ();
  gtk_widget_set_parent (self->entry, GTK_WIDGET (self));
  gtk_widget_set_hexpand (self->entry, TRUE);
  gtk_editable_init_delegate (GTK_EDITABLE (self));
  self->changed_id = g_signal_connect (self->entry, "notify::text", G_CALLBACK (text_changed), self);

  self->popup = gtk_popover_new ();
  gtk_popover_set_position (GTK_POPOVER (self->popup), GTK_POS_BOTTOM);
  gtk_popover_set_autohide (GTK_POPOVER (self->popup), FALSE);
  gtk_popover_set_has_arrow (GTK_POPOVER (self->popup), FALSE);
  gtk_widget_set_halign (self->popup, GTK_ALIGN_START);
  gtk_widget_add_css_class (self->popup, "menu");
  gtk_widget_set_parent (self->popup, GTK_WIDGET (self));
  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (sw), 400);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

  gtk_popover_set_child (GTK_POPOVER (self->popup), sw);
  self->list = gtk_list_view_new (NULL, NULL);
  gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (self->list), TRUE);
  g_signal_connect (self->list, "activate",
                    G_CALLBACK (suggestion_entry_row_activated), self);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), self->list);

  set_default_factory (self);

  controller = gtk_event_controller_key_new ();
  gtk_event_controller_set_name (controller, "gtk-suggestion-entry");
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (suggestion_entry_key_pressed), self);
  gtk_widget_add_controller (self->entry, controller);

  controller = gtk_event_controller_focus_new ();
  gtk_event_controller_set_name (controller, "gtk-suggestion-entry");
  g_signal_connect (controller, "leave",
                    G_CALLBACK (suggestion_entry_focus_out), self);
  gtk_widget_add_controller (self->entry, controller);
}

GtkWidget *
suggestion_entry_new (void)
{
  return g_object_new (SUGGESTION_TYPE_ENTRY, NULL);
}

GListModel *
suggestion_entry_get_model (SuggestionEntry *self)
{
  g_return_val_if_fail (SUGGESTION_IS_ENTRY (self), NULL);

  return self->model;
}

static void
selection_changed (GtkSingleSelection *selection,
                   GParamSpec         *pspec,
                   SuggestionEntry    *self)
{
  accept_current_selection (self);
}

static gboolean
filter_func (gpointer item, gpointer user_data)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (user_data);
  guint min_score;

  if (self->use_filter)
    min_score = 1;
  else
    min_score = 0;

  return match_object_get_score (MATCH_OBJECT (item)) >= min_score;
}

static void
default_match_func (MatchObject *object,
                    const char  *search,
                    gpointer     data)
{
  char *tmp1, *tmp2, *tmp3, *tmp4;

  tmp1 = g_utf8_normalize (match_object_get_string (object), -1, G_NORMALIZE_ALL);
  tmp2 = g_utf8_casefold (tmp1, -1);

  tmp3 = g_utf8_normalize (search, -1, G_NORMALIZE_ALL);
  tmp4 = g_utf8_casefold (tmp3, -1);

  if (g_str_has_prefix (tmp2, tmp4))
    match_object_set_match (object, 0, g_utf8_strlen (search, -1), 1);
  else
    match_object_set_match (object, 0, 0, 0);

  g_free (tmp1);
  g_free (tmp2);
  g_free (tmp3);
  g_free (tmp4);
}

static gpointer
map_func (gpointer item, gpointer user_data)
{
  SuggestionEntry *self = SUGGESTION_ENTRY (user_data);
  GValue value = G_VALUE_INIT;
  gpointer obj;

  if (self->expression)
    {
      gtk_expression_evaluate (self->expression, item, &value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      g_object_get_property (G_OBJECT (item), "string", &value);
    }
  else
    {
      g_critical ("Either SuggestionEntry:expression must be set "
                  "or SuggestionEntry:model must be a GtkStringList");
      g_value_set_string (&value, "No value");
    }

  obj = match_object_new (item, g_value_get_string (&value));

  g_value_unset (&value);

  if (self->search && self->search[0])
    self->match_func (obj, self->search, self->match_data);
  else
    match_object_set_match (obj, 0, 0, 1);

  return obj;
}

static void
update_map (SuggestionEntry *self)
{
  gtk_map_list_model_set_map_func (self->map_model, map_func, self, NULL);
}

void
suggestion_entry_set_model (SuggestionEntry *self,
                            GListModel      *model)
{
  g_return_if_fail (SUGGESTION_IS_ENTRY (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (!g_set_object (&self->model, model))
    return;

  if (self->selection)
    g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);

  if (model == NULL)
    {
      gtk_list_view_set_model (GTK_LIST_VIEW (self->list), NULL);
      g_clear_object (&self->selection);
      g_clear_object (&self->map_model);
      g_clear_object (&self->filter);
    }
  else
    {
      GtkMapListModel *map_model;
      GtkFilterListModel *filter_model;
      GtkFilter *filter;
      GtkSortListModel *sort_model;
      GtkSingleSelection *selection;
      GtkSorter *sorter;

      map_model = gtk_map_list_model_new (g_object_ref (model), NULL, NULL, NULL);
      g_set_object (&self->map_model, map_model);

      update_map (self);

      filter = GTK_FILTER (gtk_custom_filter_new (filter_func, self, NULL));
      filter_model = gtk_filter_list_model_new (G_LIST_MODEL (self->map_model), filter);
      g_set_object (&self->filter, filter);

      sorter = GTK_SORTER (gtk_numeric_sorter_new (gtk_property_expression_new (MATCH_TYPE_OBJECT, NULL, "score")));
      gtk_numeric_sorter_set_sort_order (GTK_NUMERIC_SORTER (sorter), GTK_SORT_DESCENDING);
      sort_model = gtk_sort_list_model_new (G_LIST_MODEL (filter_model), sorter);

      update_map (self);

      selection = gtk_single_selection_new (G_LIST_MODEL (sort_model));
      gtk_single_selection_set_autoselect (selection, FALSE);
      gtk_single_selection_set_can_unselect (selection, TRUE);
      gtk_single_selection_set_selected (selection, GTK_INVALID_LIST_POSITION);
      g_set_object (&self->selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->list), GTK_SELECTION_MODEL (selection));
      g_object_unref (selection);
    }

  if (self->selection)
    {
      g_signal_connect (self->selection, "notify::selected",
                        G_CALLBACK (selection_changed), self);
      selection_changed (self->selection, NULL, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

GtkListItemFactory *
suggestion_entry_get_factory (SuggestionEntry *self)
{
  g_return_val_if_fail (SUGGESTION_IS_ENTRY (self), NULL);

  return self->factory;
}

void
suggestion_entry_set_factory (SuggestionEntry    *self,
                              GtkListItemFactory *factory)
{
  g_return_if_fail (SUGGESTION_IS_ENTRY (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  if (self->list)
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->list), factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

void
suggestion_entry_set_expression (SuggestionEntry *self,
                                 GtkExpression   *expression)
{
  g_return_if_fail (SUGGESTION_IS_ENTRY (self));
  g_return_if_fail (expression == NULL ||
                    gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  if (self->expression)
    gtk_expression_unref (self->expression);

  self->expression = expression;

  if (self->expression)
    gtk_expression_ref (self->expression);

  update_map (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

GtkExpression *
suggestion_entry_get_expression (SuggestionEntry *self)
{
  g_return_val_if_fail (SUGGESTION_IS_ENTRY (self), NULL);

  return self->expression;
}

void
suggestion_entry_set_use_filter (SuggestionEntry *self,
                                 gboolean         use_filter)
{
  g_return_if_fail (SUGGESTION_IS_ENTRY (self));

  if (self->use_filter == use_filter)
    return;

  self->use_filter = use_filter;

  gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_USE_FILTER]);
}

gboolean
suggestion_entry_get_use_filter (SuggestionEntry *self)
{
  g_return_val_if_fail (SUGGESTION_IS_ENTRY (self), TRUE);

  return self->use_filter;
}

static void
suggestion_entry_arrow_clicked (SuggestionEntry *self)
{
  gboolean visible;

  visible = gtk_widget_get_visible (self->popup);
  suggestion_entry_set_popup_visible (self, !visible);
}

void
suggestion_entry_set_show_arrow (SuggestionEntry *self,
                                 gboolean         show_arrow)
{
  g_return_if_fail (SUGGESTION_IS_ENTRY (self));

  if (self->show_arrow == show_arrow)
    return;

  self->show_arrow = show_arrow;

  if (show_arrow)
    {
      GtkGesture *press;

      self->arrow = gtk_image_new_from_icon_name ("pan-down-symbolic");
      gtk_widget_set_tooltip_text (self->arrow, "Show suggestions");
      gtk_widget_set_parent (self->arrow, GTK_WIDGET (self));

      press = gtk_gesture_click_new ();
      g_signal_connect_swapped (press, "released",
                                G_CALLBACK (suggestion_entry_arrow_clicked), self);
      gtk_widget_add_controller (self->arrow, GTK_EVENT_CONTROLLER (press));

    }
  else
    {
      g_clear_pointer (&self->arrow, gtk_widget_unparent);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ARROW]);
}

gboolean
suggestion_entry_get_show_arrow (SuggestionEntry *self)
{
  g_return_val_if_fail (SUGGESTION_IS_ENTRY (self), FALSE);

  return self->show_arrow;
}

void
suggestion_entry_set_match_func (SuggestionEntry          *self,
                                 SuggestionEntryMatchFunc  match_func,
                                 gpointer                  user_data,
                                 GDestroyNotify            destroy)
{
  if (self->destroy)
    self->destroy (self->match_data);
  self->match_func = match_func;
  self->match_data = user_data;
  self->destroy = destroy;
}
