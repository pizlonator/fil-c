/*
 * Copyright © 2012 Canonical Limited
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionable.h"

#include "gtkwidget.h"

/**
 * GtkActionable:
 *
 * The `GtkActionable` interface provides a convenient way of associating
 * widgets with actions.
 *
 * It primarily consists of two properties: [property@Gtk.Actionable:action-name]
 * and [property@Gtk.Actionable:action-target]. There are also some convenience
 * APIs for setting these properties.
 *
 * The action will be looked up in action groups that are found among
 * the widgets ancestors. Most commonly, these will be the actions with
 * the “win.” or “app.” prefix that are associated with the
 * `GtkApplicationWindow` or `GtkApplication`, but other action groups that
 * are added with [method@Gtk.Widget.insert_action_group] will be consulted
 * as well.
 **/

/**
 * GtkActionableInterface:
 * @get_action_name: virtual function for [method@Actionable.get_action_name]
 * @set_action_name: virtual function for [method@Actionable.set_action_name]
 * @get_action_target_value: virtual function for [method@Actionable.get_action_target_value]
 * @set_action_target_value: virtual function for [method@Actionable.set_action_target_value]
 *
 * The interface vtable for `GtkActionable`.
 **/

G_DEFINE_INTERFACE (GtkActionable, gtk_actionable, GTK_TYPE_WIDGET)

static void
gtk_actionable_default_init (GtkActionableInterface *iface)
{
  g_object_interface_install_property (iface,
    g_param_spec_string ("action-name", NULL, NULL,
                         NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
    g_param_spec_variant ("action-target", NULL, NULL,
                          G_VARIANT_TYPE_ANY, NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * gtk_actionable_get_action_name: (attributes org.gtk.Property.get=action-name)
 * @actionable: a `GtkActionable` widget
 *
 * Gets the action name for @actionable.
 *
 * Returns: (nullable): the action name
 */
const char *
gtk_actionable_get_action_name (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_name (actionable);
}

/**
 * gtk_actionable_set_action_name: (attributes org.gtk.Property.set=action-name)
 * @actionable: a `GtkActionable` widget
 * @action_name: (nullable): an action name
 *
 * Specifies the name of the action with which this widget should be
 * associated.
 *
 * If @action_name is %NULL then the widget will be unassociated from
 * any previous action.
 *
 * Usually this function is used when the widget is located (or will be
 * located) within the hierarchy of a `GtkApplicationWindow`.
 *
 * Names are of the form “win.save” or “app.quit” for actions on the
 * containing [class@ApplicationWindow] or its associated [class@Application],
 * respectively. This is the same form used for actions in the [class@Gio.Menu]
 * associated with the window.
 */
void
gtk_actionable_set_action_name (GtkActionable *actionable,
                                const char    *action_name)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_name (actionable, action_name);
}

/**
 * gtk_actionable_get_action_target_value: (attributes org.gtk.Method.get_property=action-target)
 * @actionable: a `GtkActionable` widget
 *
 * Gets the current target value of @actionable.
 *
 * Returns: (nullable) (transfer none): the current target value
 */
GVariant *
gtk_actionable_get_action_target_value (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_target_value (actionable);
}

/**
 * gtk_actionable_set_action_target_value: (attributes org.gtk.Method.set_property=action-target)
 * @actionable: a `GtkActionable` widget
 * @target_value: (nullable): a [struct@GLib.Variant] to set as the target value
 *
 * Sets the target value of an actionable widget.
 *
 * If @target_value is %NULL then the target value is unset.
 *
 * The target value has two purposes. First, it is used as the parameter
 * to activation of the action associated with the `GtkActionable` widget.
 * Second, it is used to determine if the widget should be rendered as
 * “active” — the widget is active if the state is equal to the given target.
 *
 * Consider the example of associating a set of buttons with a [iface@Gio.Action]
 * with string state in a typical “radio button” situation. Each button
 * will be associated with the same action, but with a different target
 * value for that action. Clicking on a particular button will activate
 * the action with the target of that button, which will typically cause
 * the action’s state to change to that value. Since the action’s state
 * is now equal to the target value of the button, the button will now
 * be rendered as active (and the other buttons, with different targets,
 * rendered inactive).
 */
void
gtk_actionable_set_action_target_value (GtkActionable *actionable,
                                        GVariant      *target_value)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_target_value (actionable, target_value);
}

/**
 * gtk_actionable_set_action_target:
 * @actionable: a `GtkActionable` widget
 * @format_string: a [struct@GLib.Variant] format string
 * @...: arguments appropriate for @format_string
 *
 * Sets the target of an actionable widget.
 *
 * This is a convenience function that calls [ctor@GLib.Variant.new] for
 * @format_string and uses the result to call
 * [method@Gtk.Actionable.set_action_target_value].
 *
 * If you are setting a string-valued target and want to set
 * the action name at the same time, you can use
 * [method@Gtk.Actionable.set_detailed_action_name].
 */
void
gtk_actionable_set_action_target (GtkActionable *actionable,
                                  const char    *format_string,
                                  ...)
{
  va_list ap;

  va_start (ap, format_string);
  gtk_actionable_set_action_target_value (actionable, g_variant_new_va (format_string, NULL, &ap));
  va_end (ap);
}

/**
 * gtk_actionable_set_detailed_action_name:
 * @actionable: a `GtkActionable` widget
 * @detailed_action_name: the detailed action name
 *
 * Sets the action-name and associated string target value of an
 * actionable widget.
 *
 * @detailed_action_name is a string in the format accepted by
 * [func@Gio.Action.parse_detailed_name].
 */
void
gtk_actionable_set_detailed_action_name (GtkActionable *actionable,
                                         const char    *detailed_action_name)
{
  GError *error = NULL;
  GVariant *target;
  char *name;

  if (detailed_action_name == NULL)
    {
      gtk_actionable_set_action_name (actionable, NULL);
      gtk_actionable_set_action_target_value (actionable, NULL);
      return;
    }

  if (!g_action_parse_detailed_name (detailed_action_name, &name, &target, &error))
    g_error ("gtk_actionable_set_detailed_action_name: %s", error->message);

  gtk_actionable_set_action_name (actionable, name);
  gtk_actionable_set_action_target_value (actionable, target);

  if (target)
    g_variant_unref (target);
  g_free (name);
}
