/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * Authors:
 * - Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkPopoverMenuBar:
 *
 * `GtkPopoverMenuBar` presents a horizontal bar of items that pop
 * up popover menus when clicked.
 *
 * ![An example GtkPopoverMenuBar](menubar.png)
 *
 * The only way to create instances of `GtkPopoverMenuBar` is
 * from a `GMenuModel`.
 *
 * # CSS nodes
 *
 * ```
 * menubar
 * ├── item[.active]
 * ┊   ╰── popover
 * ╰── item
 *     ╰── popover
 * ```
 *
 * `GtkPopoverMenuBar` has a single CSS node with name menubar, below which
 * each item has its CSS node, and below that the corresponding popover.
 *
 * The item whose popover is currently open gets the .active
 * style class.
 *
 * # Accessibility
 *
 * `GtkPopoverMenuBar` uses the %GTK_ACCESSIBLE_ROLE_MENU_BAR role,
 * the menu items use the %GTK_ACCESSIBLE_ROLE_MENU_ITEM role and
 * the menus use the %GTK_ACCESSIBLE_ROLE_MENU role.
 */


#include "config.h"

#include "gtkpopovermenubar.h"
#include "gtkpopovermenubarprivate.h"
#include "gtkpopovermenu.h"

#include "gtkbinlayout.h"
#include "gtkboxlayout.h"
#include "gtklabel.h"
#include "gtkmenubutton.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkgestureclick.h"
#include "gtkeventcontrollermotion.h"
#include "gtkactionmuxerprivate.h"
#include "gtkmenutrackerprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmain.h"
#include "gtknative.h"
#include "gtkbuildable.h"

#define GTK_TYPE_POPOVER_MENU_BAR_ITEM    (gtk_popover_menu_bar_item_get_type ())
#define GTK_POPOVER_MENU_BAR_ITEM(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_POPOVER_MENU_BAR_ITEM, GtkPopoverMenuBarItem))
#define GTK_IS_POPOVER_MENU_BAR_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_POPOVER_MENU_BAR_ITEM))

GType gtk_popover_menu_bar_item_get_type (void) G_GNUC_CONST;

typedef struct _GtkPopoverMenuBarItem GtkPopoverMenuBarItem;

struct _GtkPopoverMenuBar
{
  GtkWidget parent;

  GMenuModel *model;
  GtkMenuTracker *tracker;

  GtkPopoverMenuBarItem *active_item;
};

typedef struct _GtkPopoverMenuBarClass GtkPopoverMenuBarClass;
struct _GtkPopoverMenuBarClass
{
  GtkWidgetClass parent_class;
};

struct _GtkPopoverMenuBarItem
{
  GtkWidget parent;

  GtkWidget *label;
  GtkPopover *popover;
  GtkMenuTrackerItem *tracker;
};

typedef struct _GtkPopoverMenuBarItemClass GtkPopoverMenuBarItemClass;
struct _GtkPopoverMenuBarItemClass
{
  GtkWidgetClass parent_class;

  void (* activate) (GtkPopoverMenuBarItem *item);
};

G_DEFINE_TYPE (GtkPopoverMenuBarItem, gtk_popover_menu_bar_item, GTK_TYPE_WIDGET)

static void
open_submenu (GtkPopoverMenuBarItem *item)
{
  gtk_popover_popup (item->popover);
}

static void
close_submenu (GtkPopoverMenuBarItem *item)
{
  gtk_popover_popdown (item->popover);
}

static void
set_active_item (GtkPopoverMenuBar     *bar,
                 GtkPopoverMenuBarItem *item,
                 gboolean               popup)
{
  gboolean changed;
  gboolean was_popup;

  changed = item != bar->active_item;

  if (bar->active_item)
    was_popup = gtk_widget_get_mapped (GTK_WIDGET (bar->active_item->popover));
  else
    was_popup = FALSE;

  if (was_popup && changed)
    close_submenu (bar->active_item);

  if (changed)
    {
      if (bar->active_item)
        gtk_widget_unset_state_flags (GTK_WIDGET (bar->active_item), GTK_STATE_FLAG_SELECTED);

      bar->active_item = item;

      if (bar->active_item)
        gtk_widget_set_state_flags (GTK_WIDGET (bar->active_item), GTK_STATE_FLAG_SELECTED, FALSE);
    }

  if (bar->active_item)
    {
      if (popup || (was_popup && changed))
        open_submenu (bar->active_item);
      else if (changed)
        gtk_widget_grab_focus (GTK_WIDGET (bar->active_item));
    }
}

static void
clicked_cb (GtkGesture *gesture,
            int         n,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *target;
  GtkPopoverMenuBar *bar;

  target = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU_BAR));

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (target), TRUE);
}

static void
item_enter_cb (GtkEventController   *controller,
               double                x,
               double                y,
               gpointer              data)
{
  GtkWidget *target;
  GtkPopoverMenuBar *bar;

  target = gtk_event_controller_get_widget (controller);
  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU_BAR));

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (target), FALSE);
}

static void
bar_leave_cb (GtkEventController   *controller,
              gpointer              data)
{
  GtkWidget *target;
  GtkPopoverMenuBar *bar;

  target = gtk_event_controller_get_widget (controller);
  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (target, GTK_TYPE_POPOVER_MENU_BAR));

  if (bar->active_item &&
      !gtk_widget_get_mapped (GTK_WIDGET (bar->active_item->popover)))
    set_active_item (bar, NULL, FALSE);
}

static gboolean
gtk_popover_menu_bar_focus (GtkWidget        *widget,
                            GtkDirectionType  direction)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (widget);
  GtkWidget *next;

  if (bar->active_item &&
      gtk_widget_get_mapped (GTK_WIDGET (bar->active_item->popover)))
    {
      if (gtk_widget_child_focus (GTK_WIDGET (bar->active_item->popover), direction))
        return TRUE;
    }

  if (direction == GTK_DIR_LEFT)
    {
      if (bar->active_item)
        next = gtk_widget_get_prev_sibling (GTK_WIDGET (bar->active_item));
      else
        next = NULL;

      if (next == NULL)
        next = gtk_widget_get_last_child (GTK_WIDGET (bar));
    }
  else if (direction == GTK_DIR_RIGHT)
    {
      if (bar->active_item)
        next = gtk_widget_get_next_sibling (GTK_WIDGET (bar->active_item));
      else
        next = NULL;

      if (next == NULL)
        next = gtk_widget_get_first_child (GTK_WIDGET (bar));
    }
  else
    return FALSE;

  set_active_item (bar, GTK_POPOVER_MENU_BAR_ITEM (next), FALSE);

  return TRUE;
}

static void
gtk_popover_menu_bar_item_init (GtkPopoverMenuBarItem *item)
{
  GtkEventController *controller;

  gtk_widget_set_focusable (GTK_WIDGET (item), TRUE);

  item->label = g_object_new (GTK_TYPE_LABEL,
                              "use-underline", TRUE,
                              NULL);
  gtk_widget_set_parent (item->label, GTK_WIDGET (item));

  controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
  g_signal_connect (controller, "pressed", G_CALLBACK (clicked_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), controller);

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  g_signal_connect (controller, "enter", G_CALLBACK (item_enter_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), controller);
}

static void
gtk_popover_menu_bar_item_dispose (GObject *object)
{
  GtkPopoverMenuBarItem *item = GTK_POPOVER_MENU_BAR_ITEM (object);

  g_clear_object (&item->tracker);
  g_clear_pointer (&item->label, gtk_widget_unparent);
  g_clear_pointer ((GtkWidget **)&item->popover, gtk_widget_unparent);

  G_OBJECT_CLASS (gtk_popover_menu_bar_item_parent_class)->dispose (object);
}

static void
gtk_popover_menu_bar_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (gtk_popover_menu_bar_item_parent_class)->finalize (object);
}

static void
gtk_popover_menu_bar_item_activate (GtkPopoverMenuBarItem *item)
{
  GtkPopoverMenuBar *bar;

  bar = GTK_POPOVER_MENU_BAR (gtk_widget_get_ancestor (GTK_WIDGET (item), GTK_TYPE_POPOVER_MENU_BAR));

  set_active_item (bar, item, TRUE);
}

static void
gtk_popover_menu_bar_item_root (GtkWidget *widget)
{
  GtkPopoverMenuBarItem *item = GTK_POPOVER_MENU_BAR_ITEM (widget);

  GTK_WIDGET_CLASS (gtk_popover_menu_bar_item_parent_class)->root (widget);

  gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_RELATION_LABELLED_BY, item->label, NULL,
                                  GTK_ACCESSIBLE_RELATION_CONTROLS, item->popover, NULL,
                                  -1);
  gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                  GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE,
                                  -1);
}

static void
gtk_popover_menu_bar_item_class_init (GtkPopoverMenuBarItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  guint activate_signal;

  object_class->dispose = gtk_popover_menu_bar_item_dispose;
  object_class->finalize = gtk_popover_menu_bar_item_finalize;

  widget_class->root = gtk_popover_menu_bar_item_root;

  klass->activate = gtk_popover_menu_bar_item_activate;

  activate_signal =
    g_signal_new (I_("activate"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkPopoverMenuBarItemClass, activate),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (widget_class, I_("item"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_MENU_ITEM);
  gtk_widget_class_set_activate_signal (widget_class, activate_signal);
  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}
enum
{
  PROP_0,
  PROP_MENU_MODEL,
  LAST_PROP
};

static GParamSpec * bar_props[LAST_PROP];

static void gtk_popover_menu_bar_buildable_iface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPopoverMenuBar, gtk_popover_menu_bar, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                gtk_popover_menu_bar_buildable_iface_init))

static void
tracker_remove (int      position,
                gpointer user_data)
{
  GtkWidget *bar = user_data;
  GtkWidget *child;
  int i;

  for (child = gtk_widget_get_first_child (bar), i = 0;
       child;
       child = gtk_widget_get_next_sibling (child), i++)
    {
      if (i == position)
        {
          gtk_widget_unparent (child);
          break;
        }
    }
}

static void
popover_unmap (GtkPopover        *popover,
               GtkPopoverMenuBar *bar)
{
  if (bar->active_item && bar->active_item->popover == popover)
    set_active_item (bar, NULL, FALSE);
}

static void
popover_shown (GtkPopover            *popover,
               GtkPopoverMenuBarItem *item)
{
  gtk_accessible_update_state (GTK_ACCESSIBLE (item),
                               GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                               -1);

  if (gtk_menu_tracker_item_get_should_request_show (item->tracker))
    gtk_menu_tracker_item_request_submenu_shown (item->tracker, TRUE);
}

static void
popover_hidden (GtkPopover            *popover,
                GtkPopoverMenuBarItem *item)
{
  gtk_accessible_update_state (GTK_ACCESSIBLE (item),
                               GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                               -1);

  if (gtk_menu_tracker_item_get_should_request_show (item->tracker))
    gtk_menu_tracker_item_request_submenu_shown (item->tracker, FALSE);
}

static void
tracker_insert (GtkMenuTrackerItem *item,
                int                 position,
                gpointer            user_data)
{
  GtkPopoverMenuBar *bar = user_data;

  if (gtk_menu_tracker_item_get_has_link (item, G_MENU_LINK_SUBMENU))
    {
      GtkPopoverMenuBarItem *widget;
      GMenuModel *model;
      GtkWidget *sibling;
      GtkWidget *child;
      GtkPopover *popover;
      int i;

      widget = g_object_new (GTK_TYPE_POPOVER_MENU_BAR_ITEM, NULL);
      g_object_bind_property (item, "label",
                              widget->label, "label",
                              G_BINDING_SYNC_CREATE);

      model = _gtk_menu_tracker_item_get_link (item, G_MENU_LINK_SUBMENU);
      popover = GTK_POPOVER (gtk_popover_menu_new_from_model_full (model, GTK_POPOVER_MENU_NESTED));
      gtk_widget_set_parent (GTK_WIDGET (popover), GTK_WIDGET (widget));
      gtk_popover_set_position (popover, GTK_POS_BOTTOM);
      gtk_popover_set_has_arrow (popover, FALSE);
      gtk_widget_set_halign (GTK_WIDGET (popover), GTK_ALIGN_START);

      g_signal_connect (popover, "unmap", G_CALLBACK (popover_unmap), bar);
      g_signal_connect (popover, "show", G_CALLBACK (popover_shown), widget);
      g_signal_connect (popover, "hide", G_CALLBACK (popover_hidden), widget);

      widget->popover = popover;
      widget->tracker = g_object_ref (item);

      sibling = NULL;
      for (child = gtk_widget_get_first_child (GTK_WIDGET (bar)), i = 1;
           child;
           child = gtk_widget_get_next_sibling (child), i++)
        {
          if (i == position)
            {
              sibling = child;
              break;
            }
        }
      gtk_widget_insert_after (GTK_WIDGET (widget), GTK_WIDGET (bar), sibling);
    }
  else
    g_warning ("Don't know how to handle this item");
}

static void
gtk_popover_menu_bar_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        gtk_popover_menu_bar_set_menu_model (bar, g_value_get_object (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_popover_menu_bar_get_property (GObject    *object,
                                   guint       property_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);

  switch (property_id)
    {
      case PROP_MENU_MODEL:
        g_value_set_object (value, bar->model);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_popover_menu_bar_dispose (GObject *object)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (object);
  GtkWidget *child;

  g_clear_pointer (&bar->tracker, gtk_menu_tracker_free);
  g_clear_object (&bar->model);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (bar))))
    gtk_widget_unparent (child);

  G_OBJECT_CLASS (gtk_popover_menu_bar_parent_class)->dispose (object);
}

static GList *
get_menu_bars (GtkWindow *window)
{
  return g_object_get_data (G_OBJECT (window), "gtk-menu-bar-list");
}

GList *
gtk_popover_menu_bar_get_viewable_menu_bars (GtkWindow *window)
{
  GList *menu_bars;
  GList *viewable_menu_bars = NULL;

  for (menu_bars = get_menu_bars (window);
       menu_bars;
       menu_bars = menu_bars->next)
    {
      GtkWidget *widget = menu_bars->data;
      gboolean viewable = TRUE;

      while (widget)
        {
          if (!gtk_widget_get_mapped (widget))
            viewable = FALSE;

          widget = gtk_widget_get_parent (widget);
        }

      if (viewable)
        viewable_menu_bars = g_list_prepend (viewable_menu_bars, menu_bars->data);
    }

  return g_list_reverse (viewable_menu_bars);
}

static void
set_menu_bars (GtkWindow *window,
               GList     *menubars)
{
  g_object_set_data (G_OBJECT (window), I_("gtk-menu-bar-list"), menubars);
}

static void
add_to_window (GtkWindow         *window,
               GtkPopoverMenuBar *bar)
{
  GList *menubars = get_menu_bars (window);

  set_menu_bars (window, g_list_prepend (menubars, bar));
}

static void
remove_from_window (GtkWindow         *window,
                    GtkPopoverMenuBar *bar)
{
  GList *menubars = get_menu_bars (window);

  menubars = g_list_remove (menubars, bar);
  set_menu_bars (window, menubars);
}

static void
gtk_popover_menu_bar_root (GtkWidget *widget)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (widget);
  GtkWidget *toplevel;

  GTK_WIDGET_CLASS (gtk_popover_menu_bar_parent_class)->root (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  add_to_window (GTK_WINDOW (toplevel), bar);

  gtk_accessible_update_property (GTK_ACCESSIBLE (bar),
                                  GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL,
                                  -1);
}

static void
gtk_popover_menu_bar_unroot (GtkWidget *widget)
{
  GtkPopoverMenuBar *bar = GTK_POPOVER_MENU_BAR (widget);
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  remove_from_window (GTK_WINDOW (toplevel), bar);

  GTK_WIDGET_CLASS (gtk_popover_menu_bar_parent_class)->unroot (widget);
}

static void
gtk_popover_menu_bar_class_init (GtkPopoverMenuBarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = gtk_popover_menu_bar_dispose;
  object_class->set_property = gtk_popover_menu_bar_set_property;
  object_class->get_property = gtk_popover_menu_bar_get_property;

  widget_class->root = gtk_popover_menu_bar_root;
  widget_class->unroot = gtk_popover_menu_bar_unroot;
  widget_class->focus = gtk_popover_menu_bar_focus;

  /**
   * GtkPopoverMenuBar:menu-model: (attributes org.gtk.Property.get=gtk_popover_menu_bar_get_menu_model org.gtk.Property.set=gtk_popover_menu_bar_set_menu_model)
   *
   * The `GMenuModel` from which the menu bar is created.
   *
   * The model should only contain submenus as toplevel elements.
   */
  bar_props[PROP_MENU_MODEL] =
      g_param_spec_object ("menu-model", NULL, NULL,
                           G_TYPE_MENU_MODEL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, bar_props);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("menubar"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_MENU_BAR);
}

static void
gtk_popover_menu_bar_init (GtkPopoverMenuBar *bar)
{
  GtkEventController *controller;

  controller = gtk_event_controller_motion_new ();
  gtk_event_controller_set_propagation_limit (controller, GTK_LIMIT_NONE);
  g_signal_connect (controller, "leave", G_CALLBACK (bar_leave_cb), NULL);
  gtk_widget_add_controller (GTK_WIDGET (bar), controller);
}

static GtkBuildableIface *parent_buildable_iface;

static void
gtk_popover_menu_bar_buildable_add_child (GtkBuildable *buildable,
                                          GtkBuilder   *builder,
                                          GObject      *child,
                                          const char   *type)
{
  if (GTK_IS_WIDGET (child))
    {
      if (!gtk_popover_menu_bar_add_child (GTK_POPOVER_MENU_BAR (buildable), GTK_WIDGET (child), type))
        g_warning ("No such custom attribute: %s", type);
    }
  else
    parent_buildable_iface->add_child (buildable, builder, child, type);
}

static void
gtk_popover_menu_bar_buildable_iface_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);

  iface->add_child = gtk_popover_menu_bar_buildable_add_child;
}

/**
 * gtk_popover_menu_bar_new_from_model:
 * @model: (nullable): a `GMenuModel`
 *
 * Creates a `GtkPopoverMenuBar` from a `GMenuModel`.
 *
 * Returns: a new `GtkPopoverMenuBar`
 */
GtkWidget *
gtk_popover_menu_bar_new_from_model (GMenuModel *model)
{
  return g_object_new (GTK_TYPE_POPOVER_MENU_BAR,
                       "menu-model", model,
                       NULL);
}

/**
 * gtk_popover_menu_bar_set_menu_model: (attributes org.gtk.Method.set_property=menu-model)
 * @bar: a `GtkPopoverMenuBar`
 * @model: (nullable): a `GMenuModel`
 *
 * Sets a menu model from which @bar should take
 * its contents.
 */
void
gtk_popover_menu_bar_set_menu_model (GtkPopoverMenuBar *bar,
                                     GMenuModel        *model)
{
  g_return_if_fail (GTK_IS_POPOVER_MENU_BAR (bar));
  g_return_if_fail (model == NULL || G_IS_MENU_MODEL (model));

  if (g_set_object (&bar->model, model))
    {
      GtkWidget *child;
      GtkActionMuxer *muxer;

      while ((child = gtk_widget_get_first_child (GTK_WIDGET (bar))))
        gtk_widget_unparent (child);

      g_clear_pointer (&bar->tracker, gtk_menu_tracker_free);

      if (model)
        {
          muxer = _gtk_widget_get_action_muxer (GTK_WIDGET (bar), TRUE);
          bar->tracker = gtk_menu_tracker_new (GTK_ACTION_OBSERVABLE (muxer),
                                               model,
                                               FALSE,
                                               TRUE,
                                               FALSE,
                                               NULL,
                                               tracker_insert,
                                               tracker_remove,
                                               bar);
        }

      g_object_notify_by_pspec (G_OBJECT (bar), bar_props[PROP_MENU_MODEL]);
    }
}

/**
 * gtk_popover_menu_bar_get_menu_model: (attributes org.gtk.Method.get_property=menu-model)
 * @bar: a `GtkPopoverMenuBar`
 *
 * Returns the model from which the contents of @bar are taken.
 *
 * Returns: (transfer none) (nullable): a `GMenuModel`
 */
GMenuModel *
gtk_popover_menu_bar_get_menu_model (GtkPopoverMenuBar *bar)
{
  g_return_val_if_fail (GTK_IS_POPOVER_MENU_BAR (bar), NULL);

  return bar->model;
}

void
gtk_popover_menu_bar_select_first (GtkPopoverMenuBar *bar)
{
  GtkPopoverMenuBarItem *item;

  item = GTK_POPOVER_MENU_BAR_ITEM (gtk_widget_get_first_child (GTK_WIDGET (bar)));
  set_active_item (bar, item, TRUE);
}

/**
 * gtk_popover_menu_bar_add_child:
 * @bar: a `GtkPopoverMenuBar`
 * @child: the `GtkWidget` to add
 * @id: the ID to insert @child at
 *
 * Adds a custom widget to a generated menubar.
 *
 * For this to work, the menu model of @bar must have an
 * item with a `custom` attribute that matches @id.
 *
 * Returns: %TRUE if @id was found and the widget added
 */
gboolean
gtk_popover_menu_bar_add_child (GtkPopoverMenuBar *bar,
                                GtkWidget         *child,
                                const char        *id)
{
  GtkWidget *item;

  g_return_val_if_fail (GTK_IS_POPOVER_MENU_BAR (bar), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);
  g_return_val_if_fail (id != NULL, FALSE);

  for (item = gtk_widget_get_first_child (GTK_WIDGET (bar));
       item;
       item = gtk_widget_get_next_sibling (item))
    {
      GtkPopover *popover = GTK_POPOVER_MENU_BAR_ITEM (item)->popover;

      if (gtk_popover_menu_add_child (GTK_POPOVER_MENU (popover), child, id))
        return TRUE;
    }

  return FALSE;
}

/**
 * gtk_popover_menu_bar_remove_child:
 * @bar: a `GtkPopoverMenuBar`
 * @child: the `GtkWidget` to remove
 *
 * Removes a widget that has previously been added with
 * gtk_popover_menu_bar_add_child().
 *
 * Returns: %TRUE if the widget was removed
 */
gboolean
gtk_popover_menu_bar_remove_child (GtkPopoverMenuBar *bar,
                                   GtkWidget         *child)
{
  GtkWidget *item;

  g_return_val_if_fail (GTK_IS_POPOVER_MENU_BAR (bar), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (child), FALSE);

  for (item = gtk_widget_get_first_child (GTK_WIDGET (bar));
       item;
       item = gtk_widget_get_next_sibling (item))
    {
      GtkPopover *popover = GTK_POPOVER_MENU_BAR_ITEM (item)->popover;

      if (gtk_popover_menu_remove_child (GTK_POPOVER_MENU (popover), child))
        return TRUE;
    }

  return FALSE;
}
