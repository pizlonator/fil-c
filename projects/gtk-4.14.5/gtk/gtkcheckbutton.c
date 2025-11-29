/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkcheckbutton.h"

#include "gtkactionhelperprivate.h"
#include "gtkboxlayout.h"
#include "gtkbuiltiniconprivate.h"
#include "gtkgestureclick.h"
#include <glib/gi18n-lib.h>
#include "gtklabel.h"
#include "gtkprivate.h"
#include "gtkshortcuttrigger.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"

/**
 * GtkCheckButton:
 *
 * A `GtkCheckButton` places a label next to an indicator.
 *
 * ![Example GtkCheckButtons](check-button.png)
 *
 * A `GtkCheckButton` is created by calling either [ctor@Gtk.CheckButton.new]
 * or [ctor@Gtk.CheckButton.new_with_label].
 *
 * The state of a `GtkCheckButton` can be set specifically using
 * [method@Gtk.CheckButton.set_active], and retrieved using
 * [method@Gtk.CheckButton.get_active].
 *
 * # Inconsistent state
 *
 * In addition to "on" and "off", check buttons can be an
 * "in between" state that is neither on nor off. This can be used
 * e.g. when the user has selected a range of elements (such as some
 * text or spreadsheet cells) that are affected by a check button,
 * and the current values in that range are inconsistent.
 *
 * To set a `GtkCheckButton` to inconsistent state, use
 * [method@Gtk.CheckButton.set_inconsistent].
 *
 * # Grouping
 *
 * Check buttons can be grouped together, to form mutually exclusive
 * groups - only one of the buttons can be toggled at a time, and toggling
 * another one will switch the currently toggled one off.
 *
 * Grouped check buttons use a different indicator, and are commonly referred
 * to as *radio buttons*.
 *
 * ![Example GtkCheckButtons](radio-button.png)
 *
 * To add a `GtkCheckButton` to a group, use [method@Gtk.CheckButton.set_group].
 *
 * When the code must keep track of the state of a group of radio buttons, it
 * is recommended to keep track of such state through a stateful
 * `GAction` with a target for each button. Using the `toggled` signals to keep
 * track of the group changes and state is discouraged.
 *
 * # CSS nodes
 *
 * ```
 * checkbutton[.text-button]
 * ├── check
 * ╰── [label]
 * ```
 *
 * A `GtkCheckButton` has a main node with name checkbutton. If the
 * [property@Gtk.CheckButton:label] or [property@Gtk.CheckButton:child]
 * properties are set, it contains a child widget. The indicator node
 * is named check when no group is set, and radio if the checkbutton
 * is grouped together with other checkbuttons.
 *
 * # Accessibility
 *
 * `GtkCheckButton` uses the %GTK_ACCESSIBLE_ROLE_CHECKBOX role.
 */

typedef struct {
  GtkWidget *indicator_widget;
  GtkWidget *child;

  guint inconsistent:  1;
  guint active:        1;
  guint use_underline: 1;
  guint child_type:    1;

  GtkCheckButton *group_next;
  GtkCheckButton *group_prev;

  GtkActionHelper *action_helper;
} GtkCheckButtonPrivate;

enum {
  PROP_0,
  PROP_ACTIVE,
  PROP_GROUP,
  PROP_LABEL,
  PROP_INCONSISTENT,
  PROP_USE_UNDERLINE,
  PROP_CHILD,

  /* actionable properties */
  PROP_ACTION_NAME,
  PROP_ACTION_TARGET,
  LAST_PROP = PROP_ACTION_NAME
};

enum {
  TOGGLED,
  ACTIVATE,
  LAST_SIGNAL
};

enum {
  LABEL_CHILD,
  WIDGET_CHILD
};

static void gtk_check_button_actionable_iface_init (GtkActionableInterface *iface);

static guint signals[LAST_SIGNAL] = { 0 };
static GParamSpec *props[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_CODE (GtkCheckButton, gtk_check_button, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GtkCheckButton)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTIONABLE, gtk_check_button_actionable_iface_init))

static void
gtk_check_button_dispose (GObject *object)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (object));

  g_clear_object (&priv->action_helper);

  g_clear_pointer (&priv->indicator_widget, gtk_widget_unparent);
  g_clear_pointer (&priv->child, gtk_widget_unparent);

  gtk_check_button_set_group (GTK_CHECK_BUTTON (object), NULL);

  G_OBJECT_CLASS (gtk_check_button_parent_class)->dispose (object);
}

static void
button_role_changed (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  if (gtk_action_helper_get_role (priv->action_helper) == GTK_BUTTON_ROLE_RADIO)
    gtk_css_node_set_name (gtk_widget_get_css_node (priv->indicator_widget),
                           g_quark_from_static_string("radio"));
  else
    gtk_css_node_set_name (gtk_widget_get_css_node (priv->indicator_widget),
                           g_quark_from_static_string("check"));
}

static void
ensure_action_helper (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  if (priv->action_helper)
    return;

  priv->action_helper = gtk_action_helper_new (GTK_ACTIONABLE (self));
  g_signal_connect_swapped (priv->action_helper, "notify::role",
                            G_CALLBACK (button_role_changed), self);
}

static void
gtk_check_button_set_action_name (GtkActionable *actionable,
                                  const char    *action_name)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (actionable);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  ensure_action_helper (self);

  gtk_action_helper_set_action_name (priv->action_helper, action_name);
}

static void
gtk_check_button_set_action_target_value (GtkActionable *actionable,
                                          GVariant      *action_target)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (actionable);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  ensure_action_helper (self);

  gtk_action_helper_set_action_target_value (priv->action_helper, action_target);
}

static void
gtk_check_button_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_ACTIVE:
      gtk_check_button_set_active (GTK_CHECK_BUTTON (object), g_value_get_boolean (value));
      break;
    case PROP_GROUP:
      gtk_check_button_set_group (GTK_CHECK_BUTTON (object), g_value_get_object (value));
      break;
    case PROP_LABEL:
      gtk_check_button_set_label (GTK_CHECK_BUTTON (object), g_value_get_string (value));
      break;
    case PROP_INCONSISTENT:
      gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (object), g_value_get_boolean (value));
      break;
    case PROP_USE_UNDERLINE:
      gtk_check_button_set_use_underline (GTK_CHECK_BUTTON (object), g_value_get_boolean (value));
      break;
    case PROP_CHILD:
      gtk_check_button_set_child (GTK_CHECK_BUTTON (object), g_value_get_object (value));
      break;
    case PROP_ACTION_NAME:
      gtk_check_button_set_action_name (GTK_ACTIONABLE (object), g_value_get_string (value));
      break;
    case PROP_ACTION_TARGET:
      gtk_check_button_set_action_target_value (GTK_ACTIONABLE (object), g_value_get_variant (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_check_button_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (GTK_CHECK_BUTTON (object));

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, gtk_check_button_get_active (GTK_CHECK_BUTTON (object)));
      break;
    case PROP_LABEL:
      g_value_set_string (value, gtk_check_button_get_label (GTK_CHECK_BUTTON (object)));
      break;
    case PROP_INCONSISTENT:
      g_value_set_boolean (value, gtk_check_button_get_inconsistent (GTK_CHECK_BUTTON (object)));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, gtk_check_button_get_use_underline (GTK_CHECK_BUTTON (object)));
      break;
    case PROP_CHILD:
      g_value_set_object (value, gtk_check_button_get_child (GTK_CHECK_BUTTON (object)));
      break;
    case PROP_ACTION_NAME:
      g_value_set_string (value, gtk_action_helper_get_action_name (priv->action_helper));
      break;
    case PROP_ACTION_TARGET:
      g_value_set_variant (value, gtk_action_helper_get_action_target_value (priv->action_helper));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static const char *
gtk_check_button_get_action_name (GtkActionable *actionable)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (actionable);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  return gtk_action_helper_get_action_name (priv->action_helper);
}

static GVariant *
gtk_check_button_get_action_target_value (GtkActionable *actionable)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (actionable);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  return gtk_action_helper_get_action_target_value (priv->action_helper);
}

static void
gtk_check_button_actionable_iface_init (GtkActionableInterface *iface)
{
  iface->get_action_name = gtk_check_button_get_action_name;
  iface->set_action_name = gtk_check_button_set_action_name;
  iface->get_action_target_value = gtk_check_button_get_action_target_value;
  iface->set_action_target_value = gtk_check_button_set_action_target_value;
}

static void
click_pressed_cb (GtkGestureClick *gesture,
                  guint            n_press,
                  double           x,
                  double           y,
                  GtkWidget       *widget)
{
  if (gtk_widget_get_focus_on_click (widget) && !gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);
}

static void
click_released_cb (GtkGestureClick *gesture,
                   guint            n_press,
                   double           x,
                   double           y,
                   GtkWidget       *widget)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (widget);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  if (priv->active && (priv->group_prev || priv->group_next))
    return;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
  if  (gtk_widget_is_sensitive (widget) &&
       gtk_widget_contains (widget, x, y))
    {
      if (priv->action_helper)
        gtk_action_helper_activate (priv->action_helper);
      else
        gtk_check_button_set_active (self, !priv->active);
    }
}

static void
update_accessible_state (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  GtkAccessibleTristate checked_state;

  if (priv->inconsistent)
    checked_state = GTK_ACCESSIBLE_TRISTATE_MIXED;
  else if (priv->active)
    checked_state = GTK_ACCESSIBLE_TRISTATE_TRUE;
  else
    checked_state = GTK_ACCESSIBLE_TRISTATE_FALSE;

  gtk_accessible_update_state (GTK_ACCESSIBLE (check_button),
                               GTK_ACCESSIBLE_STATE_CHECKED, checked_state,
                               -1);
}


static GtkCheckButton *
get_group_next (GtkCheckButton *self)
{
  return ((GtkCheckButtonPrivate *)gtk_check_button_get_instance_private (self))->group_next;
}

static GtkCheckButton *
get_group_prev (GtkCheckButton *self)
{
  return ((GtkCheckButtonPrivate *)gtk_check_button_get_instance_private (self))->group_prev;
}

static GtkCheckButton *
get_group_first (GtkCheckButton *self)
{
  GtkCheckButton *group_first = NULL;
  GtkCheckButton *iter;

  /* Find first in group */
  iter = self;
  while (iter)
    {
      group_first = iter;

      iter = get_group_prev (iter);
      if (!iter)
        break;
    }

  g_assert (group_first);

  return group_first;
}

static GtkCheckButton *
get_group_active_button (GtkCheckButton *self)
{
  GtkCheckButton *iter;

  for (iter = get_group_first (self); iter; iter = get_group_next (iter))
    {
      if (gtk_check_button_get_active (iter))
        return iter;
    }

  return NULL;
}

static void
gtk_check_button_state_flags_changed (GtkWidget     *widget,
                                      GtkStateFlags  previous_flags)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (widget);
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);
  GtkStateFlags state = gtk_widget_get_state_flags (GTK_WIDGET (self));

  gtk_widget_set_state_flags (priv->indicator_widget, state, TRUE);

  GTK_WIDGET_CLASS (gtk_check_button_parent_class)->state_flags_changed (widget, previous_flags);
}

static gboolean
gtk_check_button_focus (GtkWidget         *widget,
                        GtkDirectionType   direction)
{
  GtkCheckButton *self = GTK_CHECK_BUTTON (widget);

  if (gtk_widget_is_focus (widget))
    {
      GtkCheckButton *iter;
      GPtrArray *child_array;
      GtkWidget *new_focus = NULL;
      guint index;
      gboolean found;
      guint i;

      if (direction == GTK_DIR_TAB_FORWARD ||
          direction == GTK_DIR_TAB_BACKWARD)
        return FALSE;

      child_array = g_ptr_array_new ();
      for (iter = get_group_first (self); iter; iter = get_group_next (iter))
        g_ptr_array_add (child_array, iter);

      gtk_widget_focus_sort (widget, direction, child_array);
      found = g_ptr_array_find (child_array, widget, &index);

      if (found)
        {
          /* Start at the *next* widget in the list */
          if (index < child_array->len - 1)
            index ++;
        }
      else
        {
          /* Search from the start of the list */
          index = 0;
        }

      for (i = index; i < child_array->len; i ++)
        {
          GtkWidget *child = g_ptr_array_index (child_array, i);

          if (gtk_widget_get_mapped (child) && gtk_widget_is_sensitive (child))
            {
              new_focus = child;
              break;
            }
        }

      g_ptr_array_free (child_array, TRUE);

      if (new_focus && new_focus != widget)
        {
          gtk_widget_grab_focus (new_focus);
          gtk_widget_activate (new_focus);
          return TRUE;
        }
      return FALSE;
    }
  else
    {
      GtkCheckButton *active_button;

      active_button = get_group_active_button (self);
      if (active_button && active_button != self)
        return FALSE;

      gtk_widget_grab_focus (widget);
      return TRUE;
    }
}

static void
gtk_check_button_real_set_child (GtkCheckButton *self,
                                 GtkWidget      *child,
                                 guint           child_type)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (self));

  g_clear_pointer (&priv->child, gtk_widget_unparent);

  priv->child = child;

  if (priv->child)
    {
      gtk_widget_set_parent (priv->child, GTK_WIDGET (self));
      gtk_widget_insert_after (priv->child, GTK_WIDGET (self), priv->indicator_widget);
    }

  if (child_type == priv->child_type)
    return;

  priv->child_type = child_type;
  if (child_type != LABEL_CHILD)
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LABEL]);
  else
    g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CHILD]);

}

static void
gtk_check_button_real_activate (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  if (priv->active && (priv->group_prev || priv->group_next))
    return;

  if (priv->action_helper)
    gtk_action_helper_activate (priv->action_helper);
  else
    gtk_check_button_set_active (self, !gtk_check_button_get_active (self));
}

static void
gtk_check_button_class_init (GtkCheckButtonClass *class)
{
  const guint activate_keyvals[] = {
    GDK_KEY_space,
    GDK_KEY_KP_Space,
    GDK_KEY_Return,
    GDK_KEY_ISO_Enter,
    GDK_KEY_KP_Enter
  };
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkShortcutAction *activate_action;

  object_class->dispose = gtk_check_button_dispose;
  object_class->set_property = gtk_check_button_set_property;
  object_class->get_property = gtk_check_button_get_property;

  widget_class->state_flags_changed = gtk_check_button_state_flags_changed;
  widget_class->focus = gtk_check_button_focus;

  class->activate = gtk_check_button_real_activate;

  /**
   * GtkCheckButton:active: (attributes org.gtk.Property.get=gtk_check_button_get_active org.gtk.Property.set=gtk_check_button_set_active)
   *
   * If the check button is active.
   *
   * Setting `active` to %TRUE will add the `:checked:` state to both
   * the check button and the indicator CSS node.
   */
  props[PROP_ACTIVE] =
      g_param_spec_boolean ("active", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCheckButton:group: (attributes org.gtk.Property.set=gtk_check_button_set_group)
   *
   * The check button whose group this widget belongs to.
   */
  props[PROP_GROUP] =
      g_param_spec_object ("group", NULL, NULL,
                           GTK_TYPE_CHECK_BUTTON,
                           GTK_PARAM_WRITABLE);

  /**
   * GtkCheckButton:label: (attributes org.gtk.Property.get=gtk_check_button_get_label org.gtk.Property.set=gtk_check_button_set_label)
   *
   * Text of the label inside the check button, if it contains a label widget.
   */
  props[PROP_LABEL] =
    g_param_spec_string ("label", NULL, NULL,
                         NULL,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCheckButton:inconsistent: (attributes org.gtk.Property.get=gtk_check_button_get_inconsistent org.gtk.Property.set=gtk_check_button_set_inconsistent)
   *
   * If the check button is in an “in between” state.
   *
   * The inconsistent state only affects visual appearance,
   * not the semantics of the button.
   */
  props[PROP_INCONSISTENT] =
      g_param_spec_boolean ("inconsistent", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCheckButton:use-underline: (attributes org.gtk.Property.get=gtk_check_button_get_use_underline org.gtk.Property.set=gtk_check_button_set_use_underline)
   *
   * If set, an underline in the text indicates that the following
   * character is to be used as mnemonic.
   */
  props[PROP_USE_UNDERLINE] =
      g_param_spec_boolean ("use-underline", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkCheckButton:child: (attributes org.gtk.Property.get=gtk_check_button_get_child org.gtk.Property.set=gtk_check_button_set_child)
   *
   * The child widget.
   *
   * Since: 4.8
   */
  props[PROP_CHILD] =
      g_param_spec_object ("child", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);

  g_object_class_override_property (object_class, PROP_ACTION_NAME, "action-name");
  g_object_class_override_property (object_class, PROP_ACTION_TARGET, "action-target");

  /**
   * GtkCheckButton::toggled:
   *
   * Emitted when the buttons's [property@Gtk.CheckButton:active]
   * property changes.
   */
  signals[TOGGLED] =
    g_signal_new (I_("toggled"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GtkCheckButtonClass, toggled),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkCheckButton::activate:
   * @widget: the object which received the signal.
   *
   * Emitted to when the check button is activated.
   *
   * The `::activate` signal on `GtkCheckButton` is an action signal and
   * emitting it causes the button to animate press then release.
   *
   * Applications should never connect to this signal, but use the
   * [signal@Gtk.CheckButton::toggled] signal.
   *
   * The default bindings for this signal are all forms of the
   * <kbd>␣</kbd> and <kbd>Enter</kbd> keys.
   *
   * Since: 4.2
   */
  signals[ACTIVATE] =
      g_signal_new (I_ ("activate"),
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                    G_STRUCT_OFFSET (GtkCheckButtonClass, activate),
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 0);

  gtk_widget_class_set_activate_signal (widget_class, signals[ACTIVATE]);

  activate_action = gtk_signal_action_new ("activate");
  for (guint i = 0; i < G_N_ELEMENTS (activate_keyvals); i++)
    {
      GtkShortcut *activate_shortcut = gtk_shortcut_new (gtk_keyval_trigger_new (activate_keyvals[i], 0),
                                                         g_object_ref (activate_action));

      gtk_widget_class_add_shortcut (widget_class, activate_shortcut);
      g_object_unref (activate_shortcut);
    }
  g_object_unref (activate_action);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
  gtk_widget_class_set_css_name (widget_class, I_("checkbutton"));
  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_CHECKBOX);
}

static void
gtk_check_button_init (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);
  GtkGesture *gesture;

  gtk_widget_set_receives_default (GTK_WIDGET (self), FALSE);
  priv->indicator_widget = gtk_builtin_icon_new ("check");
  gtk_widget_set_halign (priv->indicator_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (priv->indicator_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_parent (priv->indicator_widget, GTK_WIDGET (self));

  update_accessible_state (self);

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (gesture), FALSE);
  gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (gesture), TRUE);
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_PRIMARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (click_pressed_cb), self);
  g_signal_connect (gesture, "released", G_CALLBACK (click_released_cb), self);
  gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (gesture), GTK_PHASE_CAPTURE);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (gesture));

  gtk_widget_set_focusable (GTK_WIDGET (self), TRUE);
}

/**
 * gtk_check_button_new:
 *
 * Creates a new `GtkCheckButton`.
 *
 * Returns: a new `GtkCheckButton`
 */
GtkWidget *
gtk_check_button_new (void)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, NULL);
}

/**
 * gtk_check_button_new_with_label:
 * @label: (nullable): the text for the check button.
 *
 * Creates a new `GtkCheckButton` with the given text.
 *
 * Returns: a new `GtkCheckButton`
 */
GtkWidget*
gtk_check_button_new_with_label (const char *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON, "label", label, NULL);
}

/**
 * gtk_check_button_new_with_mnemonic:
 * @label: (nullable): The text of the button, with an underscore
 *   in front of the mnemonic character
 *
 * Creates a new `GtkCheckButton` with the given text and a mnemonic.
 *
 * Returns: a new `GtkCheckButton`
 */
GtkWidget*
gtk_check_button_new_with_mnemonic (const char *label)
{
  return g_object_new (GTK_TYPE_CHECK_BUTTON,
                       "label", label,
                       "use-underline", TRUE,
                       NULL);
}

/**
 * gtk_check_button_set_inconsistent: (attributes org.gtk.Method.set_property=inconsistent)
 * @check_button: a `GtkCheckButton`
 * @inconsistent: %TRUE if state is inconsistent
 *
 * Sets the `GtkCheckButton` to inconsistent state.
 *
 * You should turn off the inconsistent state again if the user checks
 * the check button. This has to be done manually.
 */
void
gtk_check_button_set_inconsistent (GtkCheckButton *check_button,
                                   gboolean        inconsistent)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (check_button));

  inconsistent = !!inconsistent;
  if (priv->inconsistent != inconsistent)
    {
      priv->inconsistent = inconsistent;

      if (inconsistent)
        {
          gtk_widget_set_state_flags (GTK_WIDGET (check_button), GTK_STATE_FLAG_INCONSISTENT, FALSE);
          gtk_widget_set_state_flags (priv->indicator_widget, GTK_STATE_FLAG_INCONSISTENT, FALSE);
        }
      else
        {
          gtk_widget_unset_state_flags (GTK_WIDGET (check_button), GTK_STATE_FLAG_INCONSISTENT);
          gtk_widget_unset_state_flags (priv->indicator_widget, GTK_STATE_FLAG_INCONSISTENT);
        }

      update_accessible_state (check_button);

      g_object_notify_by_pspec (G_OBJECT (check_button), props[PROP_INCONSISTENT]);
    }
}

/**
 * gtk_check_button_get_inconsistent: (attributes org.gtk.Method.get_property=inconsistent)
 * @check_button: a `GtkCheckButton`
 *
 * Returns whether the check button is in an inconsistent state.
 *
 * Returns: %TRUE if @check_button is currently in an inconsistent state
 */
gboolean
gtk_check_button_get_inconsistent (GtkCheckButton *check_button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (check_button);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (check_button), FALSE);

  return priv->inconsistent;
}

/**
 * gtk_check_button_get_active: (attributes org.gtk.Method.get_property=active)
 * @self: a `GtkCheckButton`
 *
 * Returns whether the check button is active.
 *
 * Returns: whether the check button is active
 */
gboolean
gtk_check_button_get_active (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (self), FALSE);

  return priv->active;
}

/**
 * gtk_check_button_set_active: (attributes org.gtk.Method.set_property=active)
 * @self: a `GtkCheckButton`
 * @setting: the new value to set
 *
 * Changes the check buttons active state.
 */
void
gtk_check_button_set_active (GtkCheckButton *self,
                             gboolean       setting)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (self));

  setting = !!setting;

  if (setting == priv->active)
    return;

  if (setting)
    {
      gtk_widget_set_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_CHECKED, FALSE);
      gtk_widget_set_state_flags (priv->indicator_widget, GTK_STATE_FLAG_CHECKED, FALSE);
    }
  else
    {
      gtk_widget_unset_state_flags (GTK_WIDGET (self), GTK_STATE_FLAG_CHECKED);
      gtk_widget_unset_state_flags (priv->indicator_widget, GTK_STATE_FLAG_CHECKED);
    }

  if (setting && (priv->group_prev || priv->group_next))
    {
      GtkCheckButton *group_first = NULL;
      GtkCheckButton *iter;

      group_first = get_group_first (self);
      g_assert (group_first);

      /* Set all buttons in group to !active */
      for (iter = group_first; iter; iter = get_group_next (iter))
        gtk_check_button_set_active (iter, FALSE);

      /* ... and the next code block will set this one to active */
    }

  priv->active = setting;
  update_accessible_state (self);
  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_ACTIVE]);
  g_signal_emit (self, signals[TOGGLED], 0);
}

/**
 * gtk_check_button_get_label: (attributes org.gtk.Method.get_property=label)
 * @self: a `GtkCheckButton`
 *
 * Returns the label of the check button or `NULL` if [property@CheckButton:child] is set.
 *
 * Returns: (nullable) (transfer none): The label @self shows next
 *   to the indicator. If no label is shown, %NULL will be returned.
 */
const char *
gtk_check_button_get_label (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (self), "");

  if (priv->child_type == LABEL_CHILD && priv->child != NULL)
    return gtk_label_get_label (GTK_LABEL (priv->child));

  return NULL;
}

/**
 * gtk_check_button_set_label: (attributes org.gtk.Method.set_property=label)
 * @self: a `GtkCheckButton`
 * @label: (nullable): The text shown next to the indicator, or %NULL
 *   to show no text
 *
 * Sets the text of @self.
 *
 * If [property@Gtk.CheckButton:use-underline] is %TRUE, an underscore
 * in @label is interpreted as mnemonic indicator, see
 * [method@Gtk.CheckButton.set_use_underline] for details on this behavior.
 */
void
gtk_check_button_set_label (GtkCheckButton *self,
                            const char     *label)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);
  GtkWidget *child;

  g_return_if_fail (GTK_IS_CHECK_BUTTON (self));

  g_object_freeze_notify (G_OBJECT (self));

  if (label == NULL || label[0] == '\0')
    {
      gtk_check_button_real_set_child (self, NULL, LABEL_CHILD);
      gtk_widget_remove_css_class (GTK_WIDGET (self), "text-button");
    }
  else
    {
      if (priv->child_type != LABEL_CHILD || priv->child == NULL)
        {
          child = gtk_label_new (NULL);
          gtk_widget_set_hexpand (child, TRUE);
          gtk_label_set_xalign (GTK_LABEL (child), 0.0f);
          if (priv->use_underline)
            gtk_label_set_use_underline (GTK_LABEL (child), priv->use_underline);
          gtk_check_button_real_set_child (self, GTK_WIDGET (child), LABEL_CHILD);
        }

      gtk_widget_add_css_class (GTK_WIDGET (self), "text-button");
      gtk_label_set_label (GTK_LABEL (priv->child), label);
    }


  gtk_accessible_update_property (GTK_ACCESSIBLE (self),
                                  GTK_ACCESSIBLE_PROPERTY_LABEL, label,
                                  -1);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_LABEL]);

  g_object_thaw_notify (G_OBJECT (self));
}

/**
 * gtk_check_button_set_group: (attributes org.gtk.Method.set_property=group)
 * @self: a `GtkCheckButton`
 * @group: (nullable) (transfer none): another `GtkCheckButton` to
 *   form a group with
 *
 * Adds @self to the group of @group.
 *
 * In a group of multiple check buttons, only one button can be active
 * at a time. The behavior of a checkbutton in a group is also commonly
 * known as a *radio button*.
 *
 * Setting the group of a check button also changes the css name of the
 * indicator widget's CSS node to 'radio'.
 *
 * Setting up groups in a cycle leads to undefined behavior.
 *
 * Note that the same effect can be achieved via the [iface@Gtk.Actionable]
 * API, by using the same action with parameter type and state type 's'
 * for all buttons in the group, and giving each button its own target
 * value.
 */
void
gtk_check_button_set_group (GtkCheckButton *self,
                            GtkCheckButton *group)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);
  GtkCheckButtonPrivate *group_priv;

  g_return_if_fail (GTK_IS_CHECK_BUTTON (self));
  g_return_if_fail (self != group);

  if (!group)
    {
      if (priv->group_prev)
        {
          GtkCheckButtonPrivate *p = gtk_check_button_get_instance_private (priv->group_prev);
          p->group_next = priv->group_next;
        }
      if (priv->group_next)
        {
          GtkCheckButtonPrivate *p = gtk_check_button_get_instance_private (priv->group_next);
          p->group_prev = priv->group_prev;
        }

      priv->group_next = NULL;
      priv->group_prev = NULL;

      if (priv->indicator_widget)
        gtk_css_node_set_name (gtk_widget_get_css_node (priv->indicator_widget),
                               g_quark_from_static_string("check"));

      return;
    }

  if (priv->group_next == group)
    return;

  group_priv = gtk_check_button_get_instance_private (group);

  priv->group_prev = NULL;
  if (group_priv->group_prev)
    {
      GtkCheckButtonPrivate *prev = gtk_check_button_get_instance_private (group_priv->group_prev);

      prev->group_next = self;
      priv->group_prev = group_priv->group_prev;
    }

  group_priv->group_prev = self;
  priv->group_next = group;

  if (priv->indicator_widget)
    gtk_css_node_set_name (gtk_widget_get_css_node (priv->indicator_widget),
                           g_quark_from_static_string("radio"));

  gtk_css_node_set_name (gtk_widget_get_css_node (group_priv->indicator_widget),
                         g_quark_from_static_string("radio"));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_GROUP]);
}

/**
 * gtk_check_button_get_use_underline: (attributes org.gtk.Method.get_property=use-underline)
 * @self: a `GtkCheckButton`
 *
 * Returns whether underlines in the label indicate mnemonics.
 *
 * Returns: The value of the [property@Gtk.CheckButton:use-underline] property.
 *   See [method@Gtk.CheckButton.set_use_underline] for details on how to set
 *   a new value.
 */
gboolean
gtk_check_button_get_use_underline (GtkCheckButton *self)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (self), FALSE);

  return priv->use_underline;
}

/**
 * gtk_check_button_set_use_underline: (attributes org.gtk.Method.set_property=use-underline)
 * @self: a `GtkCheckButton`
 * @setting: the new value to set
 *
 * Sets whether underlines in the label indicate mnemonics.
 *
 * If @setting is %TRUE, an underscore character in @self's label
 * indicates a mnemonic accelerator key. This behavior is similar
 * to [property@Gtk.Label:use-underline].
 */
void
gtk_check_button_set_use_underline (GtkCheckButton *self,
                                    gboolean       setting)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (self);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (self));

  setting = !!setting;

  if (setting == priv->use_underline)
    return;

  priv->use_underline = setting;
  if (priv->child_type == LABEL_CHILD && priv->child != NULL)
    gtk_label_set_use_underline (GTK_LABEL (priv->child), priv->use_underline);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_USE_UNDERLINE]);
}

/**
 * gtk_check_button_set_child: (attributes org.gtk.Method.set_property=child)
 * @button: a `GtkCheckButton`
 * @child: (nullable): the child widget
 *
 * Sets the child widget of @button.
 *
 * Note that by using this API, you take full responsibility for setting
 * up the proper accessibility label and description information for @button.
 * Most likely, you'll either set the accessibility label or description
 * for @button explicitly, or you'll set a labelled-by or described-by
 * relations from @child to @button.
 *
 * Since: 4.8
 */
void
gtk_check_button_set_child (GtkCheckButton *button,
                            GtkWidget      *child)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (button);

  g_return_if_fail (GTK_IS_CHECK_BUTTON (button));
  g_return_if_fail (child == NULL || priv->child == child || gtk_widget_get_parent (child) == NULL);

  if (priv->child == child)
    return;

  g_object_freeze_notify (G_OBJECT (button));

  gtk_widget_remove_css_class (GTK_WIDGET (button), "text-button");

  gtk_check_button_real_set_child (button, child, WIDGET_CHILD);

  g_object_notify_by_pspec (G_OBJECT (button), props[PROP_CHILD]);

  g_object_thaw_notify (G_OBJECT (button));
}

/**
 * gtk_check_button_get_child: (attributes org.gtk.Method.get_property=child)
 * @button: a `GtkCheckButton`
 *
 * Gets the child widget of @button or `NULL` if [property@CheckButton:label] is set.
 *
 * Returns: (nullable) (transfer none): the child widget of @button
 *
 * Since: 4.8
 */
GtkWidget *
gtk_check_button_get_child (GtkCheckButton *button)
{
  GtkCheckButtonPrivate *priv = gtk_check_button_get_instance_private (button);

  g_return_val_if_fail (GTK_IS_CHECK_BUTTON (button), NULL);

  if (priv->child_type == WIDGET_CHILD)
    return priv->child;

  return NULL;
}
