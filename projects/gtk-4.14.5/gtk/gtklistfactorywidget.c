/*
 * Copyright © 2023 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtklistfactorywidgetprivate.h"

#include "gtkbinlayout.h"
#include "gtkeventcontrollermotion.h"
#include "gtkgestureclick.h"
#include "gtklistitemfactoryprivate.h"
#include "gtklistbaseprivate.h"
#include "gtkwidget.h"

typedef struct _GtkListFactoryWidgetPrivate GtkListFactoryWidgetPrivate;
struct _GtkListFactoryWidgetPrivate
{
  GtkListItemFactory *factory;

  gpointer object;
  gboolean single_click_activate;
  gboolean selectable;
  gboolean activatable;
};

enum {
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_FACTORY,
  PROP_SELECTABLE,
  PROP_SINGLE_CLICK_ACTIVATE,

  N_PROPS
};

enum
{
  ACTIVATE_SIGNAL,
  LAST_SIGNAL
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkListFactoryWidget, gtk_list_factory_widget, GTK_TYPE_LIST_ITEM_BASE)

static GParamSpec *properties[N_PROPS] = { NULL, };
static guint signals[LAST_SIGNAL] = { 0 };

static void
gtk_list_factory_widget_activate_signal (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (!priv->activatable)
    return;

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.activate-item",
                              "u",
                              gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)));
}

static gpointer
gtk_list_factory_widget_default_create_object (GtkListFactoryWidget *self)
{
  g_assert_not_reached ();
  return NULL;
}

static void
gtk_list_factory_widget_default_setup_object (GtkListFactoryWidget *self,
                                              gpointer              object)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  priv->object = object;
}

static void
gtk_list_factory_widget_setup_func (gpointer object,
                                    gpointer data)
{
  GTK_LIST_FACTORY_WIDGET_GET_CLASS (data)->setup_object (data, object);
}

static void
gtk_list_factory_widget_setup_factory (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);
  gpointer object;

  object = GTK_LIST_FACTORY_WIDGET_GET_CLASS (self)->create_object (self);

  gtk_list_item_factory_setup (priv->factory,
                               object,
                               gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                               gtk_list_factory_widget_setup_func,
                               self);

  g_assert (priv->object == object);
}

static void
gtk_list_factory_widget_default_teardown_object (GtkListFactoryWidget *self,
                                                 gpointer              object)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  priv->object = NULL;
}

static void
gtk_list_factory_widget_teardown_func (gpointer object,
                                       gpointer data)
{
  GTK_LIST_FACTORY_WIDGET_GET_CLASS (data)->teardown_object (data, object);
}

static void
gtk_list_factory_widget_teardown_factory (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);
  gpointer item = priv->object;

  gtk_list_item_factory_teardown (priv->factory,
                                  item,
                                  gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                  gtk_list_factory_widget_teardown_func,
                                  self);

  g_assert (priv->object == NULL);
  g_object_unref (item);
}

static void
gtk_list_factory_widget_default_update_object (GtkListFactoryWidget *self,
                                               gpointer              object,
                                               guint                 position,
                                               gpointer              item,
                                               gboolean              selected)
{
  GTK_LIST_ITEM_BASE_CLASS (gtk_list_factory_widget_parent_class)->update (GTK_LIST_ITEM_BASE (self),
                                                                           position,
                                                                           item,
                                                                           selected);
}

typedef struct {
  GtkListFactoryWidget *widget;
  guint position;
  gpointer item;
  gboolean selected;
} GtkListFactoryWidgetUpdate;

static void
gtk_list_factory_widget_update_func (gpointer object,
                                     gpointer data)
{
  GtkListFactoryWidgetUpdate *update = data;

  GTK_LIST_FACTORY_WIDGET_GET_CLASS (update->widget)->update_object (update->widget,
                                                                     object,
                                                                     update->position,
                                                                     update->item,
                                                                     update->selected);
}

static void
gtk_list_factory_widget_update (GtkListItemBase *base,
                                guint            position,
                                gpointer         item,
                                gboolean         selected)
{
  GtkListFactoryWidget *self = GTK_LIST_FACTORY_WIDGET (base);
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);
  GtkListFactoryWidgetUpdate update = { self, position, item, selected };

  if (priv->object)
    {
      gtk_list_item_factory_update (priv->factory,
                                    priv->object,
                                    gtk_list_item_base_get_item (GTK_LIST_ITEM_BASE (self)) != NULL,
                                    item != NULL,
                                    gtk_list_factory_widget_update_func,
                                    &update);
    }
  else
    {
      gtk_list_factory_widget_update_func (NULL, &update);
    }
}

static void
gtk_list_factory_widget_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkListFactoryWidget *self = GTK_LIST_FACTORY_WIDGET (object);

  switch (property_id)
    {
    case PROP_ACTIVATABLE:
      gtk_list_factory_widget_set_activatable (self, g_value_get_boolean (value));
      break;

    case PROP_FACTORY:
      gtk_list_factory_widget_set_factory (self, g_value_get_object (value));
      break;

    case PROP_SELECTABLE:
      gtk_list_factory_widget_set_selectable (self, g_value_get_boolean (value));
      break;

    case PROP_SINGLE_CLICK_ACTIVATE:
      gtk_list_factory_widget_set_single_click_activate (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_list_factory_widget_clear_factory (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->factory == NULL)
    return;

  if (priv->object)
    gtk_list_factory_widget_teardown_factory (self);

  g_clear_object (&priv->factory);
}
static void
gtk_list_factory_widget_dispose (GObject *object)
{
  GtkListFactoryWidget *self = GTK_LIST_FACTORY_WIDGET (object);

  gtk_list_factory_widget_clear_factory (self);

  G_OBJECT_CLASS (gtk_list_factory_widget_parent_class)->dispose (object);
}

static void
gtk_list_factory_widget_select_action (GtkWidget  *widget,
                                       const char *action_name,
                                       GVariant   *parameter)
{
  GtkListFactoryWidget *self = GTK_LIST_FACTORY_WIDGET (widget);
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);
  gboolean modify, extend;

  if (!priv->selectable)
    return;

  g_variant_get (parameter, "(bb)", &modify, &extend);

  gtk_widget_activate_action (GTK_WIDGET (self),
                              "list.select-item",
                              "(ubb)",
                              gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)), modify, extend);
}

static void
gtk_list_factory_widget_scroll_to_action (GtkWidget  *widget,
                                          const char *action_name,
                                          GVariant   *parameter)
{
  gtk_widget_activate_action (widget,
                              "list.scroll-to-item",
                              "u",
                              gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (widget)));
}

static void
gtk_list_factory_widget_class_init (GtkListFactoryWidgetClass *klass)
{
  GtkListItemBaseClass *base_class = GTK_LIST_ITEM_BASE_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->activate_signal = gtk_list_factory_widget_activate_signal;
  klass->create_object = gtk_list_factory_widget_default_create_object;
  klass->setup_object = gtk_list_factory_widget_default_setup_object;
  klass->update_object = gtk_list_factory_widget_default_update_object;
  klass->teardown_object = gtk_list_factory_widget_default_teardown_object;

  base_class->update = gtk_list_factory_widget_update;

  gobject_class->set_property = gtk_list_factory_widget_set_property;
  gobject_class->dispose = gtk_list_factory_widget_dispose;

  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable", NULL, NULL,
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_FACTORY] =
    g_param_spec_object ("factory", NULL, NULL,
                         GTK_TYPE_LIST_ITEM_FACTORY,
                         G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SELECTABLE] =
    g_param_spec_boolean ("selectable", NULL, NULL,
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  properties[PROP_SINGLE_CLICK_ACTIVATE] =
    g_param_spec_boolean ("single-click-activate", NULL, NULL,
                          FALSE,
                          G_PARAM_WRITABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);

  signals[ACTIVATE_SIGNAL] =
    g_signal_new (I_("activate-keybinding"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET (GtkListFactoryWidgetClass, activate_signal),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE_SIGNAL]);

  /**
   * GtkListItem|listitem.select:
   * @modify: %TRUE to toggle the existing selection, %FALSE to select
   * @extend: %TRUE to extend the selection
   *
   * Changes selection if the item is selectable.
   * If the item is not selectable, nothing happens.
   *
   * This function will emit the list.select-item action and the resulting
   * behavior, in particular the interpretation of @modify and @extend
   * depends on the view containing this listitem. See for example
   * GtkListView|list.select-item or GtkGridView|list.select-item.
   */
  gtk_widget_class_install_action (widget_class,
                                   "listitem.select",
                                   "(bb)",
                                   gtk_list_factory_widget_select_action);

  /**
   * GtkListItem|listitem.scroll-to:
   *
   * Moves the visible area of the list to this item with the minimum amount
   * of scrolling required. If the item is already visible, nothing happens.
   */
  gtk_widget_class_install_action (widget_class,
                                   "listitem.scroll-to",
                                   NULL,
                                   gtk_list_factory_widget_scroll_to_action);

  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_Return, 0,
                                       "activate-keybinding", 0);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_ISO_Enter, 0,
                                       "activate-keybinding", 0);
  gtk_widget_class_add_binding_signal (widget_class, GDK_KEY_KP_Enter, 0,
                                       "activate-keybinding", 0);

  /* note that some of these may get overwritten by child widgets,
   * such as GtkTreeExpander */
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, 0,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_CONTROL_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, 0,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_CONTROL_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);
  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_KP_Space, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
                                       "listitem.select", "(bb)", TRUE, FALSE);

  /* This gets overwritten by gtk_list_factory_widget_new() but better safe than sorry */
  gtk_widget_class_set_css_name (widget_class, I_("row"));
}

static void
gtk_list_factory_widget_click_gesture_pressed (GtkGestureClick      *gesture,
                                               int                   n_press,
                                               double                x,
                                               double                y,
                                               GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);
  GtkWidget *widget = GTK_WIDGET (self);

  if (!priv->selectable && !priv->activatable)
    {
      gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_DENIED);
      return;
    }

  if (priv->activatable)
    {
      if (n_press == 2 && !priv->single_click_activate)
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
          gtk_widget_activate_action (GTK_WIDGET (self),
                                      "list.activate-item",
                                      "u",
                                      gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)));
        }
    }

  if (gtk_widget_get_focus_on_click (widget))
    gtk_widget_grab_focus (widget);
}

static void
gtk_list_factory_widget_click_gesture_released (GtkGestureClick      *gesture,
                                                int                   n_press,
                                                double                x,
                                                double                y,
                                                GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->activatable)
    {
      if (n_press == 1 && priv->single_click_activate)
        {
          gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
          gtk_widget_activate_action (GTK_WIDGET (self),
                                      "list.activate-item",
                                      "u",
                                      gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)));
          return;
        }
    }

  if (priv->selectable)
    {
      GdkModifierType state;
      GdkEvent *event;
      gboolean extend, modify;

      event = gtk_gesture_get_last_event (GTK_GESTURE (gesture),
                                          gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture)));
      state = gdk_event_get_modifier_state (event);
      extend = (state & GDK_SHIFT_MASK) != 0;
      modify = (state & GDK_CONTROL_MASK) != 0;

      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "list.select-item",
                                  "(ubb)",
                                  gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)), modify, extend);
    }
}

static void
gtk_list_factory_widget_hover_cb (GtkEventControllerMotion *controller,
                                  double                    x,
                                  double                    y,
                                  GtkListFactoryWidget     *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (!priv->single_click_activate)
    return;

  if (priv->selectable)
    {
      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "list.select-item",
                                  "(ubb)",
                                  gtk_list_item_base_get_position (GTK_LIST_ITEM_BASE (self)), FALSE, FALSE);
    }
}

static void
gtk_list_factory_widget_init (GtkListFactoryWidget *self)
{
  GtkEventController *controller;
  GtkGesture *gesture;

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);

  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture),
                                              GTK_PHASE_BUBBLE);
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture),
                                     FALSE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture),
                                 GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed",
                    G_CALLBACK (gtk_list_factory_widget_click_gesture_pressed), self);
  g_signal_connect (gesture, "released",
                    G_CALLBACK (gtk_list_factory_widget_click_gesture_released), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "enter", G_CALLBACK (gtk_list_factory_widget_hover_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

gpointer
gtk_list_factory_widget_get_object (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  return priv->object;
}

void
gtk_list_factory_widget_set_factory (GtkListFactoryWidget *self,
                                     GtkListItemFactory   *factory)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->factory == factory)
    return;

  gtk_list_factory_widget_clear_factory (self);

  if (factory)
    {
      priv->factory = g_object_ref (factory);

      gtk_list_factory_widget_setup_factory (self);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FACTORY]);
}

void
gtk_list_factory_widget_set_single_click_activate (GtkListFactoryWidget *self,
                                                   gboolean              single_click_activate)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->single_click_activate == single_click_activate)
    return;

  priv->single_click_activate = single_click_activate;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SINGLE_CLICK_ACTIVATE]);
}

gboolean
gtk_list_factory_widget_get_single_click_activate (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  return priv->single_click_activate;
}

void
gtk_list_factory_widget_set_activatable (GtkListFactoryWidget *self,
                                         gboolean              activatable)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->activatable == activatable)
    return;

  priv->activatable = activatable;

  if (activatable)
    gtk_widget_add_css_class (GTK_WIDGET (self), "activatable");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "activatable");

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVATABLE]);
}

gboolean
gtk_list_factory_widget_get_activatable (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  return priv->activatable;
}

void
gtk_list_factory_widget_set_selectable (GtkListFactoryWidget *self,
                                        gboolean              selectable)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  if (priv->selectable == selectable)
    return;

  priv->selectable = selectable;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SELECTABLE]);
}

gboolean
gtk_list_factory_widget_get_selectable (GtkListFactoryWidget *self)
{
  GtkListFactoryWidgetPrivate *priv = gtk_list_factory_widget_get_instance_private (self);

  return priv->selectable;
}
