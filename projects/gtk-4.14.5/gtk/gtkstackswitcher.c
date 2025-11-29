/*
 * Copyright (c) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include "gtkstackswitcher.h"

#include "gtkboxlayout.h"
#include "gtkdropcontrollermotion.h"
#include "gtkimage.h"
#include "gtklabel.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtkselectionmodel.h"
#include "gtktogglebutton.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/**
 * GtkStackSwitcher:
 *
 * The `GtkStackSwitcher` shows a row of buttons to switch between `GtkStack`
 * pages.
 *
 * ![An example GtkStackSwitcher](stackswitcher.png)
 *
 * It acts as a controller for the associated `GtkStack`.
 *
 * All the content for the buttons comes from the properties of the stacks
 * [class@Gtk.StackPage] objects; the button visibility in a `GtkStackSwitcher`
 * widget is controlled by the visibility of the child in the `GtkStack`.
 *
 * It is possible to associate multiple `GtkStackSwitcher` widgets
 * with the same `GtkStack` widget.
 *
 * # CSS nodes
 *
 * `GtkStackSwitcher` has a single CSS node named stackswitcher and
 * style class .stack-switcher.
 *
 * When circumstances require it, `GtkStackSwitcher` adds the
 * .needs-attention style class to the widgets representing the
 * stack pages.
 *
 * # Accessibility
 *
 * `GtkStackSwitcher` uses the %GTK_ACCESSIBLE_ROLE_TAB_LIST role
 * and uses the %GTK_ACCESSIBLE_ROLE_TAB for its buttons.
 *
 * # Orientable
 *
 * Since GTK 4.4, `GtkStackSwitcher` implements `GtkOrientable` allowing
 * the stack switcher to be made vertical with
 * `gtk_orientable_set_orientation()`.
 */

#define TIMEOUT_EXPAND 500

typedef struct _GtkStackSwitcherClass   GtkStackSwitcherClass;

struct _GtkStackSwitcher
{
  GtkWidget parent_instance;

  GtkStack *stack;
  GtkSelectionModel *pages;
  GHashTable *buttons;
};

struct _GtkStackSwitcherClass
{
  GtkWidgetClass parent_class;
};

enum {
  PROP_0,
  PROP_STACK,
  PROP_ORIENTATION
};

G_DEFINE_TYPE_WITH_CODE (GtkStackSwitcher, gtk_stack_switcher, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

static void
gtk_stack_switcher_init (GtkStackSwitcher *switcher)
{
  switcher->buttons = g_hash_table_new_full (g_direct_hash, g_direct_equal, g_object_unref, NULL);

  gtk_widget_add_css_class (GTK_WIDGET (switcher), "linked");
}

static void
on_button_toggled (GtkWidget        *button,
                   GParamSpec       *pspec,
                   GtkStackSwitcher *self)
{
  gboolean active;
  guint index;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  index = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "child-index"));

  if (active)
    {
      gtk_selection_model_select_item (self->pages, index, TRUE);
    }
  else
    {
      gboolean selected = gtk_selection_model_is_selected (self->pages, index);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);
    }
}

static void
rebuild_child (GtkWidget   *self,
               const char *icon_name,
               const char *title,
               gboolean     use_underline)
{
  GtkWidget *button_child;

  button_child = NULL;

  if (icon_name != NULL)
    {
      button_child = gtk_image_new_from_icon_name (icon_name);
      if (title != NULL)
        gtk_widget_set_tooltip_text (GTK_WIDGET (self), title);

      gtk_widget_remove_css_class (self, "text-button");
      gtk_widget_add_css_class (self, "image-button");
    }
  else if (title != NULL)
    {
      button_child = gtk_label_new (title);
      gtk_label_set_use_underline (GTK_LABEL (button_child), use_underline);

      gtk_widget_set_tooltip_text (GTK_WIDGET (self), NULL);

      gtk_widget_remove_css_class (self, "image-button");
      gtk_widget_add_css_class (self, "text-button");
    }

  if (button_child)
    {
      gtk_widget_set_halign (GTK_WIDGET (button_child), GTK_ALIGN_CENTER);
      gtk_button_set_child (GTK_BUTTON (self), button_child);
    }

  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, title,
                                  -1);
}

static void
update_button (GtkStackSwitcher *self,
               GtkStackPage     *page,
               GtkWidget        *button)
{
  char *title;
  char *icon_name;
  gboolean needs_attention;
  gboolean visible;
  gboolean use_underline;

  g_object_get (page,
                "title", &title,
                "icon-name", &icon_name,
                "needs-attention", &needs_attention,
                "visible", &visible,
                "use-underline", &use_underline,
                NULL);

  rebuild_child (button, icon_name, title, use_underline);

  gtk_widget_set_visible (button, visible && (title != NULL || icon_name != NULL));

  if (needs_attention)
    gtk_widget_add_css_class (button, "needs-attention");
  else
    gtk_widget_remove_css_class (button, "needs-attention");

  g_free (title);
  g_free (icon_name);
}

static void
on_page_updated (GtkStackPage     *page,
                 GParamSpec       *pspec,
                 GtkStackSwitcher *self)
{
  GtkWidget *button;

  button = g_hash_table_lookup (self->buttons, page);
  update_button (self, page, button);
}

static gboolean
gtk_stack_switcher_switch_timeout (gpointer data)
{
  GtkWidget *button = data;

  g_object_steal_data (G_OBJECT (button), "-gtk-switch-timer");

  if (button)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

  return G_SOURCE_REMOVE;
}

static void
clear_timer (gpointer data)
{
  if (data)
    g_source_remove (GPOINTER_TO_UINT (data));
}

static void
gtk_stack_switcher_drag_enter (GtkDropControllerMotion *motion,
                               double                   x,
                               double                   y,
                               gpointer                 unused)
{
  GtkWidget *button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      guint switch_timer = g_timeout_add (TIMEOUT_EXPAND,
                                          gtk_stack_switcher_switch_timeout,
                                          button);
      gdk_source_set_static_name_by_id (switch_timer, "[gtk] gtk_stack_switcher_switch_timeout");
      g_object_set_data_full (G_OBJECT (button), "-gtk-switch-timer", GUINT_TO_POINTER (switch_timer), clear_timer);
    }
}

static void
gtk_stack_switcher_drag_leave (GtkDropControllerMotion *motion,
                               gpointer                 unused)
{
  GtkWidget *button = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (motion));
  guint switch_timer;

  switch_timer = GPOINTER_TO_UINT (g_object_steal_data (G_OBJECT (button), "-gtk-switch-timer"));
  if (switch_timer)
    g_source_remove (switch_timer);
}

static void
add_child (guint             position,
           GtkStackSwitcher *self)
{
  GtkWidget *button;
  gboolean selected;
  GtkStackPage *page;
  GtkEventController *controller;

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "accessible-role", GTK_ACCESSIBLE_ROLE_TAB,
                         "hexpand", TRUE,
                         "vexpand", TRUE,
                         NULL);
  gtk_widget_set_focus_on_click (button, FALSE);

  controller = gtk_drop_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_stack_switcher_drag_enter), NULL);
  g_signal_connect (controller, "leave", G_CALLBACK (gtk_stack_switcher_drag_leave), NULL);
  gtk_widget_add_controller (button, controller);

  page = g_list_model_get_item (G_LIST_MODEL (self->pages), position);
  update_button (self, page, button);

  gtk_widget_set_parent (button, GTK_WIDGET (self));

  g_object_set_data (G_OBJECT (button), "child-index", GUINT_TO_POINTER (position));
  selected = gtk_selection_model_is_selected (self->pages, position);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);

  gtk_accessible_update_state (GTK_ACCESSIBLE (button),
                               GTK_ACCESSIBLE_STATE_SELECTED, selected,
                               -1);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (button),
                                  GTK_ACCESSIBLE_RELATION_CONTROLS, page, NULL,
                                  -1);

  g_signal_connect (button, "notify::active", G_CALLBACK (on_button_toggled), self);
  g_signal_connect (page, "notify", G_CALLBACK (on_page_updated), self);

  g_hash_table_insert (self->buttons, g_object_ref (page), button);

  g_object_unref (page);
}

static void
populate_switcher (GtkStackSwitcher *self)
{
  guint i;

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->pages)); i++)
    add_child (i, self);
}

static void
clear_switcher (GtkStackSwitcher *self)
{
  GHashTableIter iter;
  GtkWidget *page;
  GtkWidget *button;

  g_hash_table_iter_init (&iter, self->buttons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&page, (gpointer *)&button))
    {
      gtk_widget_unparent (button);
      g_signal_handlers_disconnect_by_func (page, on_page_updated, self);
      g_hash_table_iter_remove (&iter);
    }
}

static void
items_changed_cb (GListModel       *model,
                  guint             position,
                  guint             removed,
                  guint             added,
                  GtkStackSwitcher *switcher)
{
  clear_switcher (switcher);
  populate_switcher (switcher);
}

static void
selection_changed_cb (GtkSelectionModel *model,
                      guint              position,
                      guint              n_items,
                      GtkStackSwitcher  *switcher)
{
  guint i;

  for (i = position; i < position + n_items; i++)
    {
      GtkStackPage *page;
      GtkWidget *button;
      gboolean selected;

      page = g_list_model_get_item (G_LIST_MODEL (switcher->pages), i);
      button = g_hash_table_lookup (switcher->buttons, page);
      if (button)
        {
          selected = gtk_selection_model_is_selected (switcher->pages, i);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), selected);

          gtk_accessible_update_state (GTK_ACCESSIBLE (button),
                                       GTK_ACCESSIBLE_STATE_SELECTED, selected,
                                       -1);
        }
      g_object_unref (page);
    }
}

static void
disconnect_stack_signals (GtkStackSwitcher *switcher)
{
  g_signal_handlers_disconnect_by_func (switcher->pages, items_changed_cb, switcher);
  g_signal_handlers_disconnect_by_func (switcher->pages, selection_changed_cb, switcher);
}

static void
connect_stack_signals (GtkStackSwitcher *switcher)
{
  g_signal_connect (switcher->pages, "items-changed", G_CALLBACK (items_changed_cb), switcher);
  g_signal_connect (switcher->pages, "selection-changed", G_CALLBACK (selection_changed_cb), switcher);
}

static void
set_stack (GtkStackSwitcher *switcher,
           GtkStack         *stack)
{
  if (stack)
    {
      switcher->stack = g_object_ref (stack);
      switcher->pages = gtk_stack_get_pages (stack);
      populate_switcher (switcher);
      connect_stack_signals (switcher);
    }
}

static void
unset_stack (GtkStackSwitcher *switcher)
{
  if (switcher->stack)
    {
      disconnect_stack_signals (switcher);
      clear_switcher (switcher);
      g_clear_object (&switcher->stack);
      g_clear_object (&switcher->pages);
    }
}

/**
 * gtk_stack_switcher_set_stack: (attributes org.gtk.Method.set_property=stack)
 * @switcher: a `GtkStackSwitcher`
 * @stack: (nullable): a `GtkStack`
 *
 * Sets the stack to control.
 */
void
gtk_stack_switcher_set_stack (GtkStackSwitcher *switcher,
                              GtkStack         *stack)
{
  g_return_if_fail (GTK_IS_STACK_SWITCHER (switcher));
  g_return_if_fail (GTK_IS_STACK (stack) || stack == NULL);

  if (switcher->stack == stack)
    return;

  unset_stack (switcher);
  set_stack (switcher, stack);

  gtk_widget_queue_resize (GTK_WIDGET (switcher));

  g_object_notify (G_OBJECT (switcher), "stack");
}

/**
 * gtk_stack_switcher_get_stack: (attributes org.gtk.Method.get_property=stack)
 * @switcher: a `GtkStackSwitcher`
 *
 * Retrieves the stack.
 *
 * Returns: (nullable) (transfer none): the stack
 */
GtkStack *
gtk_stack_switcher_get_stack (GtkStackSwitcher *switcher)
{
  g_return_val_if_fail (GTK_IS_STACK_SWITCHER (switcher), NULL);

  return switcher->stack;
}

static void
gtk_stack_switcher_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);
  GtkLayoutManager *box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (switcher));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      g_value_set_enum (value, gtk_orientable_get_orientation (GTK_ORIENTABLE (box_layout)));
      break;

    case PROP_STACK:
      g_value_set_object (value, switcher->stack);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_switcher_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);
  GtkLayoutManager *box_layout = gtk_widget_get_layout_manager (GTK_WIDGET (switcher));

  switch (prop_id)
    {
    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box_layout)) != orientation)
          {
            gtk_orientable_set_orientation (GTK_ORIENTABLE (box_layout), orientation);
            gtk_widget_update_orientation (GTK_WIDGET (switcher), orientation);
            g_object_notify_by_pspec (object, pspec);
          }
      }
      break;

    case PROP_STACK:
      gtk_stack_switcher_set_stack (switcher, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stack_switcher_dispose (GObject *object)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);

  unset_stack (switcher);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->dispose (object);
}

static void
gtk_stack_switcher_finalize (GObject *object)
{
  GtkStackSwitcher *switcher = GTK_STACK_SWITCHER (object);

  g_hash_table_destroy (switcher->buttons);

  G_OBJECT_CLASS (gtk_stack_switcher_parent_class)->finalize (object);
}

static void
gtk_stack_switcher_class_init (GtkStackSwitcherClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->get_property = gtk_stack_switcher_get_property;
  object_class->set_property = gtk_stack_switcher_set_property;
  object_class->dispose = gtk_stack_switcher_dispose;
  object_class->finalize = gtk_stack_switcher_finalize;

  /**
   * GtkStackSwitcher:stack: (attributes org.gtk.Property.get=gtk_stack_switcher_get_stack org.gtk.Property.set=gtk_stack_switcher_set_stack)
   *
   * The stack.
   */
  g_object_class_install_property (object_class,
                                   PROP_STACK,
                                   g_param_spec_object ("stack", NULL, NULL,
                                                        GTK_TYPE_STACK,
                                                        GTK_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT));

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("stackswitcher"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_TAB_LIST);
}

/**
 * gtk_stack_switcher_new:
 *
 * Create a new `GtkStackSwitcher`.
 *
 * Returns: a new `GtkStackSwitcher`.
 */
GtkWidget *
gtk_stack_switcher_new (void)
{
  return g_object_new (GTK_TYPE_STACK_SWITCHER, NULL);
}
