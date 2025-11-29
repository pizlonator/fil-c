/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkdropdown.h"

#include "gtkbuiltiniconprivate.h"
#include "gtklistview.h"
#include "gtklistitemfactory.h"
#include "gtksignallistitemfactory.h"
#include "gtklistitemwidgetprivate.h"
#include "gtkpopover.h"
#include "gtkprivate.h"
#include "gtksingleselection.h"
#include "gtkfilterlistmodel.h"
#include "gtkstringfilter.h"
#include "gtkmultifilter.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtktogglebutton.h"
#include "gtkexpression.h"
#include "gtkstack.h"
#include "gtksearchentry.h"
#include "gtklabel.h"
#include "gtklistitem.h"
#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkstringlist.h"
#include "gtkbox.h"
#include "gtktypebuiltins.h"

/**
 * GtkDropDown:
 *
 * `GtkDropDown` is a widget that allows the user to choose an item
 * from a list of options.
 *
 * ![An example GtkDropDown](drop-down.png)
 *
 * The `GtkDropDown` displays the [selected][property@Gtk.DropDown:selected]
 * choice.
 *
 * The options are given to `GtkDropDown` in the form of `GListModel`
 * and how the individual options are represented is determined by
 * a [class@Gtk.ListItemFactory]. The default factory displays simple strings,
 * and adds a checkmark to the selected item in the popup.
 *
 * To set your own factory, use [method@Gtk.DropDown.set_factory]. It is
 * possible to use a separate factory for the items in the popup, with
 * [method@Gtk.DropDown.set_list_factory].
 *
 * `GtkDropDown` knows how to obtain strings from the items in a
 * [class@Gtk.StringList]; for other models, you have to provide an expression
 * to find the strings via [method@Gtk.DropDown.set_expression].
 *
 * `GtkDropDown` can optionally allow search in the popup, which is
 * useful if the list of options is long. To enable the search entry,
 * use [method@Gtk.DropDown.set_enable_search].
 *
 * Here is a UI definition example for `GtkDropDown` with a simple model:
 *
 * ```xml
 * <object class="GtkDropDown">
 *   <property name="model">
 *     <object class="GtkStringList">
 *       <items>
 *         <item translatable="yes">Factory</item>
 *         <item translatable="yes">Home</item>
 *         <item translatable="yes">Subway</item>
 *       </items>
 *     </object>
 *   </property>
 * </object>
 * ```
 *
 * If a `GtkDropDown` is created in this manner, or with
 * [ctor@Gtk.DropDown.new_from_strings], for instance, the object returned from
 * [method@Gtk.DropDown.get_selected_item] will be a [class@Gtk.StringObject].
 *
 * To learn more about the list widget framework, see the
 * [overview](section-list-widget.html).
 *
 * ## CSS nodes
 *
 * `GtkDropDown` has a single CSS node with name dropdown,
 * with the button and popover nodes as children.
 *
 * ## Accessibility
 *
 * `GtkDropDown` uses the %GTK_ACCESSIBLE_ROLE_COMBO_BOX role.
 */

struct _GtkDropDown
{
  GtkWidget parent_instance;

  gboolean uses_default_factory;
  gboolean uses_default_list_factory;
  GtkListItemFactory *factory;
  GtkListItemFactory *list_factory;
  GtkListItemFactory *header_factory;
  GListModel *model;
  GtkSelectionModel *selection;
  GListModel *filter_model;
  GtkSelectionModel *popup_selection;

  GtkWidget *popup;
  GtkWidget *button;
  GtkWidget *arrow;

  GtkWidget *popup_list;
  GtkWidget *button_stack;
  GtkWidget *button_item;
  GtkWidget *button_placeholder;
  GtkWidget *search_box;
  GtkWidget *search_entry;

  GtkExpression *expression;

  GtkStringFilterMatchMode search_match_mode;

  guint enable_search : 1;
  guint show_arrow : 1;
};

struct _GtkDropDownClass
{
  GtkWidgetClass parent_class;
};

enum
{
  PROP_0,
  PROP_FACTORY,
  PROP_HEADER_FACTORY,
  PROP_LIST_FACTORY,
  PROP_MODEL,
  PROP_SELECTED,
  PROP_SELECTED_ITEM,
  PROP_ENABLE_SEARCH,
  PROP_EXPRESSION,
  PROP_SHOW_ARROW,
  PROP_SEARCH_MATCH_MODE,

  N_PROPS
};

enum
{
  ACTIVATE,
  LAST_SIGNAL
};

G_DEFINE_TYPE (GtkDropDown, gtk_drop_down, GTK_TYPE_WIDGET)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
button_toggled (GtkWidget *widget,
                gpointer   data)
{
  GtkDropDown *self = data;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    gtk_popover_popup (GTK_POPOVER (self->popup));
  else
    gtk_popover_popdown (GTK_POPOVER (self->popup));

  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_EXPANDED, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
                               -1);
}

static void
popover_closed (GtkPopover *popover,
                gpointer    data)
{
  GtkDropDown *self = data;

  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
}

static void
row_activated (GtkListView *listview,
               guint        position,
               gpointer     data)
{
  GtkDropDown *self = data;
  GtkFilter *filter;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), FALSE);
  gtk_popover_popdown (GTK_POPOVER (self->popup));

  /* reset the filter so positions are 1-1 */
  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_drop_down_set_selected (self, gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->popup_selection)));
}

static void
selection_changed (GtkSingleSelection *selection,
                   GParamSpec         *pspec,
                   gpointer            data)
{
  GtkDropDown *self = data;
  guint selected;
  GtkFilter *filter;

  selected = gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection));

  if (selected == GTK_INVALID_LIST_POSITION)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->button_stack), "empty");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->button_stack), "item");
    }

  if (selected != gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self->button_item)))
    {
      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (self->button_item),
                                 selected,
                                 gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->selection)),
                                 FALSE);
    }

  /* reset the filter so positions are 1-1 */
  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), "");
  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (self->popup_selection), selected);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED]);
}

static void
selection_item_changed (GtkSingleSelection *selection,
                        GParamSpec         *pspec,
                        gpointer            data)
{
  GtkDropDown *self = data;
  gpointer item;

  item = gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->selection));

  if (item != gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self->button_item)))
    {
      gtk_list_item_base_update (GTK_LIST_ITEM_BASE (self->button_item),
                                 gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection)),
                                 item,
                                 FALSE);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTED_ITEM]);
}

static void
gtk_drop_down_activate (GtkDropDown *self)
{
  gtk_widget_activate (self->button);
}

static void
update_filter (GtkDropDown *self)
{
  if (self->filter_model)
    {
      GtkFilter *filter;

      if (self->expression)
        {
          filter = GTK_FILTER (gtk_string_filter_new (gtk_expression_ref (self->expression)));
          gtk_string_filter_set_match_mode (GTK_STRING_FILTER (filter), self->search_match_mode);
        }
      else
        filter = GTK_FILTER (gtk_every_filter_new ());
      gtk_filter_list_model_set_filter (GTK_FILTER_LIST_MODEL (self->filter_model), filter);
      g_object_unref (filter);
    }
}

static void
search_changed (GtkSearchEntry *entry, gpointer data)
{
  GtkDropDown *self = data;
  const char *text;
  GtkFilter *filter;

  text  = gtk_editable_get_text (GTK_EDITABLE (entry));

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    gtk_string_filter_set_search (GTK_STRING_FILTER (filter), text);
}

static void
search_stop (GtkSearchEntry *entry, gpointer data)
{
  GtkDropDown *self = data;
  GtkFilter *filter;

  filter = gtk_filter_list_model_get_filter (GTK_FILTER_LIST_MODEL (self->filter_model));
  if (GTK_IS_STRING_FILTER (filter))
    {
      if (gtk_string_filter_get_search (GTK_STRING_FILTER (filter)))
        gtk_string_filter_set_search (GTK_STRING_FILTER (filter), NULL);
      else
        gtk_popover_popdown (GTK_POPOVER (self->popup));
    }
}

static void
gtk_drop_down_dispose (GObject *object)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  g_clear_pointer (&self->popup, gtk_widget_unparent);
  g_clear_pointer (&self->button, gtk_widget_unparent);

  g_clear_object (&self->model);
  if (self->selection)
    {
      g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);
      g_signal_handlers_disconnect_by_func (self->selection, selection_item_changed, self);
    }
  g_clear_object (&self->filter_model);
  g_clear_pointer (&self->expression, gtk_expression_unref);
  g_clear_object (&self->selection);
  g_clear_object (&self->popup_selection);
  g_clear_object (&self->factory);
  g_clear_object (&self->list_factory);
  g_clear_object (&self->header_factory);

  G_OBJECT_CLASS (gtk_drop_down_parent_class)->dispose (object);
}

static void
gtk_drop_down_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      g_value_set_object (value, self->factory);
      break;

    case PROP_HEADER_FACTORY:
      g_value_set_object (value, self->header_factory);
      break;

    case PROP_LIST_FACTORY:
      g_value_set_object (value, self->list_factory);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_SELECTED:
      g_value_set_uint (value, gtk_drop_down_get_selected (self));
      break;

    case PROP_SELECTED_ITEM:
      g_value_set_object (value, gtk_drop_down_get_selected_item (self));
      break;

    case PROP_ENABLE_SEARCH:
      g_value_set_boolean (value, self->enable_search);
      break;

    case PROP_EXPRESSION:
      gtk_value_set_expression (value, self->expression);
      break;

    case PROP_SHOW_ARROW:
      g_value_set_boolean (value, gtk_drop_down_get_show_arrow (self));
      break;

    case PROP_SEARCH_MATCH_MODE:
      g_value_set_enum (value, gtk_drop_down_get_search_match_mode (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkDropDown *self = GTK_DROP_DOWN (object);

  switch (property_id)
    {
    case PROP_FACTORY:
      gtk_drop_down_set_factory (self, g_value_get_object (value));
      break;

    case PROP_HEADER_FACTORY:
      gtk_drop_down_set_header_factory (self, g_value_get_object (value));
      break;

    case PROP_LIST_FACTORY:
      gtk_drop_down_set_list_factory (self, g_value_get_object (value));
      break;

    case PROP_MODEL:
      gtk_drop_down_set_model (self, g_value_get_object (value));
      break;

    case PROP_SELECTED:
      gtk_drop_down_set_selected (self, g_value_get_uint (value));
      break;

    case PROP_ENABLE_SEARCH:
      gtk_drop_down_set_enable_search (self, g_value_get_boolean (value));
      break;

    case PROP_EXPRESSION:
      gtk_drop_down_set_expression (self, gtk_value_get_expression (value));
      break;

    case PROP_SHOW_ARROW:
      gtk_drop_down_set_show_arrow (self, g_value_get_boolean (value));
      break;

    case PROP_SEARCH_MATCH_MODE:
      gtk_drop_down_set_search_match_mode (self, g_value_get_enum (value));
      break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_drop_down_measure (GtkWidget      *widget,
                       GtkOrientation  orientation,
                       int             size,
                       int            *minimum,
                       int            *natural,
                       int            *minimum_baseline,
                       int            *natural_baseline)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  gtk_widget_measure (self->button,
                      orientation,
                      size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_drop_down_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  gtk_widget_size_allocate (self->button, &(GtkAllocation) { 0, 0, width, height }, baseline);

  gtk_widget_set_size_request (self->popup, width, -1);
  gtk_widget_queue_resize (self->popup);

  gtk_popover_present (GTK_POPOVER (self->popup));
}

static gboolean
gtk_drop_down_focus (GtkWidget        *widget,
                     GtkDirectionType  direction)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  if (self->popup && gtk_widget_get_visible (self->popup))
    return gtk_widget_child_focus (self->popup, direction);
  else
    return gtk_widget_child_focus (self->button, direction);
}

static gboolean
gtk_drop_down_grab_focus (GtkWidget *widget)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  return gtk_widget_grab_focus (self->button);
}

static void
gtk_drop_down_root (GtkWidget *widget)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  GTK_WIDGET_CLASS (gtk_drop_down_parent_class)->root (widget);

  if (self->factory)
    gtk_list_factory_widget_set_factory (GTK_LIST_FACTORY_WIDGET (self->button_item), self->factory);
}

static void
gtk_drop_down_unroot (GtkWidget *widget)
{
  GtkDropDown *self = GTK_DROP_DOWN (widget);

  if (self->factory)
    gtk_list_factory_widget_set_factory (GTK_LIST_FACTORY_WIDGET (self->button_item), NULL);

  GTK_WIDGET_CLASS (gtk_drop_down_parent_class)->unroot (widget);
}

static void
gtk_drop_down_class_init (GtkDropDownClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_drop_down_dispose;
  gobject_class->get_property = gtk_drop_down_get_property;
  gobject_class->set_property = gtk_drop_down_set_property;

  widget_class->measure = gtk_drop_down_measure;
  widget_class->size_allocate = gtk_drop_down_size_allocate;
  widget_class->focus = gtk_drop_down_focus;
  widget_class->grab_focus = gtk_drop_down_grab_focus;
  widget_class->root = gtk_drop_down_root;
  widget_class->unroot = gtk_drop_down_unroot;

  /**
   * GtkDropDown:factory: (attributes org.gtk.Property.get=gtk_drop_down_get_factory org.gtk.Property.set=gtk_drop_down_set_factory)
   *
   * Factory for populating list items.
   */
  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:header-factory: (attributes org.gtk.Property.get=gtk_drop_down_get_header_factory org.gtk.Property.set=gtk_drop_down_set_header_factory)
   *
   * The factory for creating header widgets for the popup.
   *
   * Since: 4.12
   */
  properties[PROP_HEADER_FACTORY] =
    g_param_spec_object ("header-factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:list-factory: (attributes org.gtk.Property.get=gtk_drop_down_get_list_factory org.gtk.Property.set=gtk_drop_down_set_list_factory)
   *
   * The factory for populating list items in the popup.
   *
   * If this is not set, [property@Gtk.DropDown:factory] is used.
   */
  properties[PROP_LIST_FACTORY] =
    g_param_spec_object ("list-factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:model: (attributes org.gtk.Property.get=gtk_drop_down_get_model org.gtk.Property.set=gtk_drop_down_set_model)
   *
   * Model for the displayed items.
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:selected: (attributes org.gtk.Property.get=gtk_drop_down_get_selected org.gtk.Property.set=gtk_drop_down_set_selected)
   *
   * The position of the selected item.
   *
   * If no item is selected, the property has the value
   * %GTK_INVALID_LIST_POSITION.
   */
  properties[PROP_SELECTED] =
    g_param_spec_uint ("selected", NULL, NULL,
                       0, G_MAXUINT, GTK_INVALID_LIST_POSITION,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:selected-item: (attributes org.gtk.Property.get=gtk_drop_down_get_selected_item)
   *
   * The selected item.
   */
  properties[PROP_SELECTED_ITEM] =
    g_param_spec_object ("selected-item", NULL, NULL,
                         G_TYPE_OBJECT,
                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:enable-search: (attributes org.gtk.Property.get=gtk_drop_down_get_enable_search org.gtk.Property.set=gtk_drop_down_set_enable_search)
   *
   * Whether to show a search entry in the popup.
   *
   * Note that search requires [property@Gtk.DropDown:expression]
   * to be set.
   */
  properties[PROP_ENABLE_SEARCH] =
    g_param_spec_boolean  ("enable-search", NULL, NULL,
                         FALSE,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:expression: (type GtkExpression) (attributes org.gtk.Property.get=gtk_drop_down_get_expression org.gtk.Property.set=gtk_drop_down_set_expression)
   *
   * An expression to evaluate to obtain strings to match against the search
   * term.
   *
   * See [property@Gtk.DropDown:enable-search] for how to enable search.
   * If [property@Gtk.DropDown:factory] is not set, the expression is also
   * used to bind strings to labels produced by a default factory.
   */
  properties[PROP_EXPRESSION] =
    gtk_param_spec_expression ("expression", NULL, NULL,
                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:show-arrow: (attributes org.gtk.Property.get=gtk_drop_down_get_show_arrow org.gtk.Property.set=gtk_drop_down_set_show_arrow)
   *
   * Whether to show an arrow within the GtkDropDown widget.
   *
   * Since: 4.6
   */
  properties[PROP_SHOW_ARROW] =
    g_param_spec_boolean  ("show-arrow", NULL, NULL,
                           TRUE,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkDropDown:search-match-mode: (attributes org.gtk.Property.get=gtk_drop_down_get_search_match_mode org.gtk.Property.set=gtk_drop_down_set_search_match_mode)
   *
   * The match mode for the search filter.
   *
   * Since: 4.12
   */
  properties[PROP_SEARCH_MATCH_MODE] =
    g_param_spec_enum  ("search-match-mode", NULL, NULL,
                           GTK_TYPE_STRING_FILTER_MATCH_MODE,
                           GTK_STRING_FILTER_MATCH_MODE_PREFIX,
                           G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  /**
   * GtkDropDown::activate:
   * @widget: the object which received the signal.
   *
   * Emitted to when the drop down is activated.
   *
   * The `::activate` signal on `GtkDropDown` is an action signal and
   * emitting it causes the drop down to pop up its dropdown.
   *
   * Since: 4.6
   */
  signals[ACTIVATE] =
      g_signal_new_class_handler (I_ ("activate"),
                                  G_OBJECT_CLASS_TYPE (gobject_class),
                                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                  G_CALLBACK (gtk_drop_down_activate),
                                  NULL, NULL,
                                  NULL,
                                  G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE]);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/ui/gtkdropdown.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, arrow);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, button_item);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, popup);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, popup_list);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, search_box);
  gtk_widget_class_bind_template_child (widget_class, GtkDropDown, search_entry);

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, button_toggled);
  gtk_widget_class_bind_template_callback (widget_class, popover_closed);
  gtk_widget_class_bind_template_callback (widget_class, search_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_stop);

  gtk_widget_class_set_css_name (widget_class, I_("dropdown"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_COMBO_BOX);
}

static void
setup_item (GtkSignalListItemFactory *factory,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *icon;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_append (GTK_BOX (box), label);
  icon = g_object_new (GTK_TYPE_IMAGE,
                       "icon-name", "object-select-symbolic",
                       "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                       NULL);
  gtk_box_append (GTK_BOX (box), icon);
  gtk_list_item_set_child (list_item, box);
}

static void
selected_item_changed (GtkDropDown *self,
                       GParamSpec  *pspec,
                       GtkListItem *list_item)
{
  GtkWidget *box;
  GtkWidget *icon;

  box = gtk_list_item_get_child (list_item);
  icon = gtk_widget_get_last_child (box);

  if (gtk_drop_down_get_selected_item (self) == gtk_list_item_get_item (list_item))
    gtk_widget_set_opacity (icon, 1.0);
  else
    gtk_widget_set_opacity (icon, 0.0);
}

static void
root_changed (GtkWidget   *box,
              GParamSpec  *pspec,
              GtkDropDown *self)
{
  GtkWidget *icon;

  icon = gtk_widget_get_last_child (box);

  if (gtk_widget_get_ancestor (box, GTK_TYPE_POPOVER) == self->popup)
    gtk_widget_set_visible (icon, TRUE);
  else
    gtk_widget_set_visible (icon, FALSE);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item,
           gpointer                  data)
{
  GtkDropDown *self = data;
  gpointer item;
  GtkWidget *box;
  GtkWidget *label;
  GValue value = G_VALUE_INIT;

  item = gtk_list_item_get_item (list_item);
  box = gtk_list_item_get_child (list_item);
  label = gtk_widget_get_first_child (box);

  if (self->expression &&
      gtk_expression_evaluate (self->expression, item, &value))
    {
      gtk_label_set_label (GTK_LABEL (label), g_value_get_string (&value));
      g_value_unset (&value);
    }
  else if (GTK_IS_STRING_OBJECT (item))
    {
      const char *string;

      string = gtk_string_object_get_string (GTK_STRING_OBJECT (item));
      gtk_label_set_label (GTK_LABEL (label), string);
    }
  else
    {
      g_critical ("Either GtkDropDown:factory or GtkDropDown:expression must be set");
    }

  g_signal_connect (self, "notify::selected-item",
                    G_CALLBACK (selected_item_changed), list_item);
  selected_item_changed (self, NULL, list_item);

  g_signal_connect (box, "notify::root",
                    G_CALLBACK (root_changed), self);
  root_changed (box, NULL, self);
}

static void
unbind_item (GtkSignalListItemFactory *factory,
             GtkListItem              *list_item,
             gpointer                  data)
{
  GtkDropDown *self = data;
  GtkWidget *box;

  box = gtk_list_item_get_child (list_item);

  g_signal_handlers_disconnect_by_func (self, selected_item_changed, list_item);
  g_signal_handlers_disconnect_by_func (box, root_changed, self);
}

static void
set_default_factory (GtkDropDown *self)
{
  GtkListItemFactory *factory;

  factory = gtk_signal_list_item_factory_new ();

  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), self);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_item), self);

  gtk_drop_down_set_factory (self, factory);
  self->uses_default_factory = TRUE;

  if (self->uses_default_list_factory)
    gtk_drop_down_set_list_factory (self, NULL);

  g_object_unref (factory);
}

static void
gtk_drop_down_init (GtkDropDown *self)
{
  g_type_ensure (GTK_TYPE_BUILTIN_ICON);
  g_type_ensure (GTK_TYPE_LIST_ITEM_WIDGET);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->show_arrow = gtk_widget_get_visible (self->arrow);

  set_default_factory (self);

  self->search_match_mode = GTK_STRING_FILTER_MATCH_MODE_PREFIX;
}

/**
 * gtk_drop_down_new:
 * @model: (transfer full) (nullable): the model to use
 * @expression: (transfer full) (nullable): the expression to use
 *
 * Creates a new `GtkDropDown`.
 *
 * You may want to call [method@Gtk.DropDown.set_factory]
 * to set up a way to map its items to widgets.
 *
 * Returns: a new `GtkDropDown`
 */
GtkWidget *
gtk_drop_down_new (GListModel    *model,
                   GtkExpression *expression)
{
  GtkWidget *self;

  self = g_object_new (GTK_TYPE_DROP_DOWN,
                       "model", model,
                       "expression", expression,
                       NULL);

  /* we're consuming the references */
  g_clear_object (&model);
  g_clear_pointer (&expression, gtk_expression_unref);

  return self;
}

/**
 * gtk_drop_down_new_from_strings:
 * @strings: (array zero-terminated=1): The strings to put in the dropdown
 *
 * Creates a new `GtkDropDown` that is populated with
 * the strings.
 *
 * Returns: a new `GtkDropDown`
 */
GtkWidget *
gtk_drop_down_new_from_strings (const char * const *strings)
{
  return gtk_drop_down_new (G_LIST_MODEL (gtk_string_list_new (strings)), NULL);
}

/**
 * gtk_drop_down_get_model: (attributes org.gtk.Method.get_property=model)
 * @self: a `GtkDropDown`
 *
 * Gets the model that provides the displayed items.
 *
 * Returns: (nullable) (transfer none): The model in use
 */
GListModel *
gtk_drop_down_get_model (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->model;
}

/**
 * gtk_drop_down_set_model: (attributes org.gtk.Method.set_property=model)
 * @self: a `GtkDropDown`
 * @model: (nullable) (transfer none): the model to use
 *
 * Sets the `GListModel` to use.
 */
void
gtk_drop_down_set_model (GtkDropDown *self,
                         GListModel  *model)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));

  if (!g_set_object (&self->model, model))
    return;

  if (model == NULL)
    {
      gtk_list_view_set_model (GTK_LIST_VIEW (self->popup_list), NULL);

      if (self->selection)
        {
          g_signal_handlers_disconnect_by_func (self->selection, selection_changed, self);
          g_signal_handlers_disconnect_by_func (self->selection, selection_item_changed, self);
        }

      g_clear_object (&self->selection);
      g_clear_object (&self->filter_model);
      g_clear_object (&self->popup_selection);
    }
  else
    {
      GListModel *filter_model;
      GtkSelectionModel *selection;

      filter_model = G_LIST_MODEL (gtk_filter_list_model_new (g_object_ref (model), NULL));
      g_set_object (&self->filter_model, filter_model);

      update_filter (self);

      selection = GTK_SELECTION_MODEL (gtk_single_selection_new (filter_model));
      g_set_object (&self->popup_selection, selection);
      gtk_list_view_set_model (GTK_LIST_VIEW (self->popup_list), selection);
      g_object_unref (selection);

      selection = GTK_SELECTION_MODEL (gtk_single_selection_new (g_object_ref (model)));
      g_set_object (&self->selection, selection);
      g_object_unref (selection);

      g_signal_connect (self->selection, "notify::selected", G_CALLBACK (selection_changed), self);
      g_signal_connect (self->selection, "notify::selected-item", G_CALLBACK (selection_item_changed), self);
      selection_changed (GTK_SINGLE_SELECTION (self->selection), NULL, self);
      selection_item_changed (GTK_SINGLE_SELECTION (self->selection), NULL, self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);
}

/**
 * gtk_drop_down_get_factory: (attributes org.gtk.Method.get_property=factory)
 * @self: a `GtkDropDown`
 *
 * Gets the factory that's currently used to populate list items.
 *
 * The factory returned by this function is always used for the
 * item in the button. It is also used for items in the popup
 * if [property@Gtk.DropDown:list-factory] is not set.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_drop_down_get_factory (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->factory;
}

/**
 * gtk_drop_down_set_factory: (attributes org.gtk.Method.set_property=factory)
 * @self: a `GtkDropDown`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items.
 */
void
gtk_drop_down_set_factory (GtkDropDown        *self,
                           GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->factory, factory))
    return;

  if (gtk_widget_get_root (GTK_WIDGET (self)))
    gtk_list_factory_widget_set_factory (GTK_LIST_FACTORY_WIDGET (self->button_item), factory);

  if (self->list_factory == NULL)
    {
      gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), factory);
      self->uses_default_list_factory = TRUE;
    }

  self->uses_default_factory = factory != NULL;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

/**
 * gtk_drop_down_get_header_factory: (attributes org.gtk.Method.get_property=header-factory)
 * @self: a `GtkDropDown`
 *
 * Gets the factory that's currently used to create header widgets for the popup.
 *
 * Returns: (nullable) (transfer none): The factory in use
 *
 * Since: 4.12
 */
GtkListItemFactory *
gtk_drop_down_get_header_factory (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->header_factory;
}

/**
 * gtk_drop_down_set_header_factory: (attributes org.gtk.Method.set_property=header-factory)
 * @self: a `GtkDropDown`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for creating header widgets for the popup.
 *
 * Since: 4.12
 */
void
gtk_drop_down_set_header_factory (GtkDropDown        *self,
                                  GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->header_factory, factory))
    return;

  gtk_list_view_set_header_factory (GTK_LIST_VIEW (self->popup_list), self->header_factory);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_HEADER_FACTORY]);
}

/**
 * gtk_drop_down_get_list_factory: (attributes org.gtk.Method.get_property=list-factory)
 * @self: a `GtkDropDown`
 *
 * Gets the factory that's currently used to populate list items in the popup.
 *
 * Returns: (nullable) (transfer none): The factory in use
 */
GtkListItemFactory *
gtk_drop_down_get_list_factory (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->list_factory;
}

/**
 * gtk_drop_down_set_list_factory: (attributes org.gtk.Method.set_property=list-factory)
 * @self: a `GtkDropDown`
 * @factory: (nullable) (transfer none): the factory to use
 *
 * Sets the `GtkListItemFactory` to use for populating list items in the popup.
 */
void
gtk_drop_down_set_list_factory (GtkDropDown        *self,
                                GtkListItemFactory *factory)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (factory == NULL || GTK_LIST_ITEM_FACTORY (factory));

  if (!g_set_object (&self->list_factory, factory))
    return;

  if (self->list_factory != NULL)
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), self->list_factory);
  else
    gtk_list_view_set_factory (GTK_LIST_VIEW (self->popup_list), self->factory);

  self->uses_default_list_factory = factory != NULL;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_LIST_FACTORY]);
}

/**
 * gtk_drop_down_set_selected: (attributes org.gtk.Method.set_property=selected)
 * @self: a `GtkDropDown`
 * @position: the position of the item to select, or %GTK_INVALID_LIST_POSITION
 *
 * Selects the item at the given position.
 */
void
gtk_drop_down_set_selected (GtkDropDown *self,
                            guint        position)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  if (self->selection == NULL)
    return;

  if (gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection)) == position)
    return;

  gtk_single_selection_set_selected (GTK_SINGLE_SELECTION (self->selection), position);
}

/**
 * gtk_drop_down_get_selected: (attributes org.gtk.Method.get_property=selected)
 * @self: a `GtkDropDown`
 *
 * Gets the position of the selected item.
 *
 * Returns: the position of the selected item, or %GTK_INVALID_LIST_POSITION
 *   if not item is selected
 */
guint
gtk_drop_down_get_selected (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), GTK_INVALID_LIST_POSITION);

  if (self->selection == NULL)
    return GTK_INVALID_LIST_POSITION;

  return gtk_single_selection_get_selected (GTK_SINGLE_SELECTION (self->selection));
}

/**
 * gtk_drop_down_get_selected_item: (attributes org.gtk.Method.get_property=selected-item)
 * @self: a `GtkDropDown`
 *
 * Gets the selected item. If no item is selected, %NULL is returned.
 *
 * Returns: (transfer none) (type GObject) (nullable): The selected item
 */
gpointer
gtk_drop_down_get_selected_item (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  if (self->selection == NULL)
    return NULL;

  return gtk_single_selection_get_selected_item (GTK_SINGLE_SELECTION (self->selection));
}

/**
 * gtk_drop_down_set_enable_search: (attributes org.gtk.Method.set_property=enable-search)
 * @self: a `GtkDropDown`
 * @enable_search: whether to enable search
 *
 * Sets whether a search entry will be shown in the popup that
 * allows to search for items in the list.
 *
 * Note that [property@Gtk.DropDown:expression] must be set for
 * search to work.
 */
void
gtk_drop_down_set_enable_search (GtkDropDown *self,
                                 gboolean     enable_search)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  enable_search = !!enable_search;

  if (self->enable_search == enable_search)
    return;

  self->enable_search = enable_search;

  gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
  gtk_widget_set_visible (self->search_box, enable_search);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ENABLE_SEARCH]);
}

/**
 * gtk_drop_down_get_enable_search: (attributes org.gtk.Method.set_property=enable-search)
 * @self: a `GtkDropDown`
 *
 * Returns whether search is enabled.
 *
 * Returns: %TRUE if the popup includes a search entry
 */
gboolean
gtk_drop_down_get_enable_search (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), FALSE);

  return self->enable_search;
}

/**
 * gtk_drop_down_set_expression: (attributes org.gtk.Method.set_property=expression)
 * @self: a `GtkDropDown`
 * @expression: (nullable): a `GtkExpression`
 *
 * Sets the expression that gets evaluated to obtain strings from items.
 *
 * This is used for search in the popup. The expression must have
 * a value type of %G_TYPE_STRING.
 */
void
gtk_drop_down_set_expression (GtkDropDown   *self,
                              GtkExpression *expression)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));
  g_return_if_fail (expression == NULL ||
                    gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  if (self->expression)
    gtk_expression_unref (self->expression);
  self->expression = expression;
  if (self->expression)
    gtk_expression_ref (self->expression);

  if (self->uses_default_factory)
    set_default_factory (self);

  update_filter (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

/**
 * gtk_drop_down_get_expression: (attributes org.gtk.Method.get_property=expression)
 * @self: a `GtkDropDown`
 *
 * Gets the expression set that is used to obtain strings from items.
 *
 * See [method@Gtk.DropDown.set_expression].
 *
 * Returns: (nullable) (transfer none): a `GtkExpression`
 */
GtkExpression *
gtk_drop_down_get_expression (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), NULL);

  return self->expression;
}

/**
 * gtk_drop_down_set_show_arrow: (attributes org.gtk.Method.set_property=show-arrow)
 * @self: a `GtkDropDown`
 * @show_arrow: whether to show an arrow within the widget
 *
 * Sets whether an arrow will be displayed within the widget.
 *
 * Since: 4.6
 */
void
gtk_drop_down_set_show_arrow (GtkDropDown *self,
                              gboolean     show_arrow)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  show_arrow = !!show_arrow;

  if (self->show_arrow == show_arrow)
    return;

  self->show_arrow = show_arrow;

  gtk_widget_set_visible (self->arrow, show_arrow);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_ARROW]);
}

/**
 * gtk_drop_down_get_show_arrow: (attributes org.gtk.Method.set_property=show-arrow)
 * @self: a `GtkDropDown`
 *
 * Returns whether to show an arrow within the widget.
 *
 * Returns: %TRUE if an arrow will be shown.
 *
 * Since: 4.6
 */
gboolean
gtk_drop_down_get_show_arrow (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), FALSE);

  return self->show_arrow;
}

/**
 * gtk_drop_down_set_search_match_mode: (attributes org.gtk.Method.set_property=search-match-mode)
 * @self: a `GtkDropDown`
 * @search_match_mode: the new match mode
 *
 * Sets the match mode for the search filter.
 *
 * Since: 4.12
 */
void
gtk_drop_down_set_search_match_mode (GtkDropDown *self,
                                     GtkStringFilterMatchMode search_match_mode)
{
  g_return_if_fail (GTK_IS_DROP_DOWN (self));

  if (self->search_match_mode == search_match_mode)
    return;

  self->search_match_mode = search_match_mode;

  update_filter (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH_MATCH_MODE]);
}

/**
 * gtk_drop_down_get_search_match_mode: (attributes org.gtk.Method.get_property=search-match-mode)
 * @self: a `GtkDropDown`
 *
 * Returns the match mode that the search filter is using.
 *
 * Returns: the match mode of the search filter
 *
 * Since: 4.12
 */
GtkStringFilterMatchMode
gtk_drop_down_get_search_match_mode (GtkDropDown *self)
{
  g_return_val_if_fail (GTK_IS_DROP_DOWN (self), GTK_STRING_FILTER_MATCH_MODE_PREFIX);

  return self->search_match_mode;
}
