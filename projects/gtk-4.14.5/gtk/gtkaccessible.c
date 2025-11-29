/* gtkaccessible.c: Accessible interface
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkAccessible:
 *
 * `GtkAccessible` is an interface for describing UI elements for
 * Assistive Technologies.
 *
 * Every accessible implementation has:
 *
 *  - a “role”, represented by a value of the [enum@Gtk.AccessibleRole] enumeration
 *  - an “attribute”, represented by a set of [enum@Gtk.AccessibleState],
 *    [enum@Gtk.AccessibleProperty] and [enum@Gtk.AccessibleRelation] values
 *
 * The role cannot be changed after instantiating a `GtkAccessible`
 * implementation.
 *
 * The attributes are updated every time a UI element's state changes in
 * a way that should be reflected by assistive technologies. For instance,
 * if a `GtkWidget` visibility changes, the %GTK_ACCESSIBLE_STATE_HIDDEN
 * state will also change to reflect the [property@Gtk.Widget:visible] property.
 *
 * Every accessible implementation is part of a tree of accessible objects.
 * Normally, this tree corresponds to the widget tree, but can be customized
 * by reimplementing the [vfunc@Gtk.Accessible.get_accessible_parent],
 * [vfunc@Gtk.Accessible.get_first_accessible_child] and
 * [vfunc@Gtk.Accessible.get_next_accessible_sibling] virtual functions.
 * Note that you can not create a top-level accessible object as of now,
 * which means that you must always have a parent accessible object.
 * Also note that when an accessible object does not correspond to a widget,
 * and it has children, whose implementation you don't control,
 * it is necessary to ensure the correct shape of the a11y tree
 * by calling [method@Gtk.Accessible.set_accessible_parent] and
 * updating the sibling by [method@Gtk.Accessible.update_next_accessible_sibling].
 */

#include "config.h"

#include "gtkaccessibleprivate.h"

#include "gtkatcontextprivate.h"
#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"

#include <glib/gi18n-lib.h>

#include <stdarg.h>

G_DEFINE_INTERFACE (GtkAccessible, gtk_accessible, G_TYPE_OBJECT)

static void
gtk_accessible_default_init (GtkAccessibleInterface *iface)
{
  /**
   * GtkAccessible:accessible-role: (attributes org.gtk.Property.get=gtk_accessible_get_accessible_role)
   *
   * The accessible role of the given `GtkAccessible` implementation.
   *
   * The accessible role cannot be changed once set.
   */
  GParamSpec *pspec =
    g_param_spec_enum ("accessible-role", NULL, NULL,
                       GTK_TYPE_ACCESSIBLE_ROLE,
                       GTK_ACCESSIBLE_ROLE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS);

  g_object_interface_install_property (iface, pspec);
}

/**
 * gtk_accessible_get_at_context:
 * @self: a `GtkAccessible`
 *
 * Retrieves the accessible implementation for the given `GtkAccessible`.
 *
 * Returns: (transfer full): the accessible implementation object
 *
 * Since: 4.10
 */
GtkATContext *
gtk_accessible_get_at_context (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_at_context (self);
}

/**
 * gtk_accessible_get_accessible_parent:
 * @self: a `GtkAccessible`
 *
 * Retrieves the accessible parent for an accessible object.
 *
 * This function returns `NULL` for top level widgets.
 *
 * Returns: (transfer full) (nullable): the accessible parent
 *
 * Since: 4.10
 */
GtkAccessible *
gtk_accessible_get_accessible_parent (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  GtkATContext *context;
  GtkAccessible *parent = NULL;

  context = gtk_accessible_get_at_context (self);
  if (context != NULL)
    {
      parent = gtk_at_context_get_accessible_parent (context);
      g_object_unref (context);
    }

  if (parent != NULL)
    return g_object_ref (parent);
  else
    return GTK_ACCESSIBLE_GET_IFACE (self)->get_accessible_parent (self);
}

/**
 * gtk_accessible_set_accessible_parent:
 * @self: an accessible object
 * @parent: (nullable): the parent accessible object
 * @next_sibling: (nullable): the sibling accessible object
 *
 * Sets the parent and sibling of an accessible object.
 *
 * This function is meant to be used by accessible implementations that are
 * not part of the widget hierarchy, and but act as a logical bridge between
 * widgets. For instance, if a widget creates an object that holds metadata
 * for each child, and you want that object to implement the `GtkAccessible`
 * interface, you will use this function to ensure that the parent of each
 * child widget is the metadata object, and the parent of each metadata
 * object is the container widget.
 *
 * Since: 4.10
 */
void
gtk_accessible_set_accessible_parent (GtkAccessible *self,
                                      GtkAccessible *parent,
                                      GtkAccessible *next_sibling)
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (parent == NULL || GTK_IS_ACCESSIBLE (parent));
  g_return_if_fail (next_sibling == NULL || GTK_IS_ACCESSIBLE (parent));

  GtkATContext *context;

  context = gtk_accessible_get_at_context (self);
  if (context != NULL)
    {
      gtk_at_context_set_accessible_parent (context, parent);
      gtk_at_context_set_next_accessible_sibling (context, next_sibling);
      g_object_unref (context);
    }
}

/**
 * gtk_accessible_update_next_accessible_sibling:
 * @self: a `GtkAccessible`
 * @new_sibling: (nullable): the new next accessible sibling to set
 *
 * Updates the next accessible sibling of @self.
 *
 * That might be useful when a new child of a custom `GtkAccessible`
 * is created, and it needs to be linked to a previous child.
 *
 * Since: 4.10
 */
void
gtk_accessible_update_next_accessible_sibling (GtkAccessible *self,
                                               GtkAccessible *new_sibling)
{
  GtkATContext *context;
  GtkAccessible *parent;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  parent = gtk_at_context_get_accessible_parent (context);
  if (parent == NULL)
    {
      g_object_unref (context);
      g_critical ("Failed to update next accessible sibling: no parent accessible set for this accessible");
      return;
    }

  gtk_at_context_set_next_accessible_sibling (context, new_sibling);

  g_object_unref (parent);
  g_object_unref (context);
}

/**
 * gtk_accessible_get_first_accessible_child:
 * @self: an accessible object
 *
 * Retrieves the first accessible child of an accessible object.
 *
 * Returns: (transfer full) (nullable): the first accessible child
 *
 * since: 4.10
 */
GtkAccessible *
gtk_accessible_get_first_accessible_child (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_first_accessible_child (self);
}

/**
 * gtk_accessible_get_next_accessible_sibling:
 * @self: an accessible object
 *
 * Retrieves the next accessible sibling of an accessible object
 *
 * Returns: (transfer full) (nullable): the next accessible sibling
 *
 * since: 4.10
 */
GtkAccessible *
gtk_accessible_get_next_accessible_sibling (GtkAccessible *self)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), NULL);

  GtkATContext *context;
  GtkAccessible *sibling = NULL;

  context = gtk_accessible_get_at_context (self);
  if (context != NULL && gtk_at_context_get_accessible_parent (context) != NULL)
    {
      sibling = gtk_at_context_get_next_accessible_sibling (context);
      if (sibling != NULL)
        sibling = g_object_ref (sibling);
    }
  else
    sibling = GTK_ACCESSIBLE_GET_IFACE (self)->get_next_accessible_sibling (self);

  g_clear_object (&context);

  return sibling;
}

/**
 * gtk_accessible_get_accessible_role:
 * @self: an accessible object
 *
 * Retrieves the accessible role of an accessible object.
 *
 * Returns: the accessible role
 */
GtkAccessibleRole
gtk_accessible_get_accessible_role (GtkAccessible *self)
{
  GtkAccessibleRole role = GTK_ACCESSIBLE_ROLE_NONE;

  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), GTK_ACCESSIBLE_ROLE_NONE);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context != NULL)
    {
      if (gtk_at_context_is_realized (context))
        role = gtk_at_context_get_accessible_role (context);

      g_object_unref (context);

      if (role != GTK_ACCESSIBLE_ROLE_NONE)
        return role;
    }

  g_object_get (G_OBJECT (self), "accessible-role", &role, NULL);

  return role;
}

/**
 * gtk_accessible_update_state:
 * @self: a `GtkAccessible`
 * @first_state: the first `GtkAccessibleState`
 * @...: a list of state and value pairs, terminated by -1
 *
 * Updates a list of accessible states. See the [enum@Gtk.AccessibleState]
 * documentation for the value types of accessible states.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * Example:
 *
 * ```c
 * value = GTK_ACCESSIBLE_TRISTATE_MIXED;
 * gtk_accessible_update_state (GTK_ACCESSIBLE (check_button),
 *                              GTK_ACCESSIBLE_STATE_CHECKED, value,
 *                              -1);
 * ```
 */
void
gtk_accessible_update_state (GtkAccessible      *self,
                             GtkAccessibleState  first_state,
                             ...)
{
  int state;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_state);

  state = first_state;

  while (state != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_state ((GtkAccessibleState) state, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect value for state “%s”: %s",
                      gtk_accessible_state_get_attribute_name (state),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_state (context, (GtkAccessibleState) state, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      state = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);

  g_object_unref (context);
}

/**
 * gtk_accessible_update_state_value: (rename-to gtk_accessible_update_state)
 * @self: a `GtkAccessible`
 * @n_states: the number of accessible states to set
 * @states: (array length=n_states): an array of `GtkAccessibleState`
 * @values: (array length=n_states): an array of `GValues`, one for each state
 *
 * Updates an array of accessible states.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * state change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_state_value (GtkAccessible      *self,
                                   int                 n_states,
                                   GtkAccessibleState  states[],
                                   const GValue        values[])
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_states > 0);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  for (int i = 0; i < n_states; i++)
    {
      GtkAccessibleState state = states[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_state_value (state, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for state “%s”: %s",
                      gtk_accessible_state_get_attribute_name (state),
                      error->message);
          g_error_free (error);
          break;
        }

      gtk_at_context_set_accessible_state (context, state, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_reset_state:
 * @self: a `GtkAccessible`
 * @state: a `GtkAccessibleState`
 *
 * Resets the accessible @state to its default value.
 */
void
gtk_accessible_reset_state (GtkAccessible      *self,
                            GtkAccessibleState  state)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_state (context, state, NULL);
  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_update_property:
 * @self: a `GtkAccessible`
 * @first_property: the first `GtkAccessibleProperty`
 * @...: a list of property and value pairs, terminated by -1
 *
 * Updates a list of accessible properties.
 *
 * See the [enum@Gtk.AccessibleProperty] documentation for the
 * value types of accessible properties.
 *
 * This function should be called by `GtkWidget` types whenever
 * an accessible property change must be communicated to assistive
 * technologies.
 *
 * Example:
 * ```c
 * value = gtk_adjustment_get_value (adjustment);
 * gtk_accessible_update_property (GTK_ACCESSIBLE (spin_button),
                                   GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, value,
                                   -1);
 * ```
 */
void
gtk_accessible_update_property (GtkAccessible         *self,
                                GtkAccessibleProperty  first_property,
                                ...)
{
  int property;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_property);

  property = first_property;

  while (property != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_property ((GtkAccessibleProperty) property, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for property “%s”: %s",
                      gtk_accessible_property_get_attribute_name (property),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_property (context, (GtkAccessibleProperty) property, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      property = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);

  g_object_unref (context);
}

/**
 * gtk_accessible_update_property_value: (rename-to gtk_accessible_update_property)
 * @self: a `GtkAccessible`
 * @n_properties: the number of accessible properties to set
 * @properties: (array length=n_properties): an array of `GtkAccessibleProperty`
 * @values: (array length=n_properties): an array of `GValues`, one for each property
 *
 * Updates an array of accessible properties.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * property change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_property_value (GtkAccessible         *self,
                                      int                    n_properties,
                                      GtkAccessibleProperty  properties[],
                                      const GValue           values[])
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_properties > 0);

  GtkATContext *context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  for (int i = 0; i < n_properties; i++)
    {
      GtkAccessibleProperty property = properties[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_property_value (property, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for property “%s”: %s",
                      gtk_accessible_property_get_attribute_name (property),
                      error->message);
          g_error_free (error);
          break;
        }

      gtk_at_context_set_accessible_property (context, property, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_reset_property:
 * @self: a `GtkAccessible`
 * @property: a `GtkAccessibleProperty`
 *
 * Resets the accessible @property to its default value.
 */
void
gtk_accessible_reset_property (GtkAccessible         *self,
                               GtkAccessibleProperty  property)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_property (context, property, NULL);
  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_update_relation:
 * @self: a `GtkAccessible`
 * @first_relation: the first `GtkAccessibleRelation`
 * @...: a list of relation and value pairs, terminated by -1
 *
 * Updates a list of accessible relations.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * relation change must be communicated to assistive technologies.
 *
 * If the [enum@Gtk.AccessibleRelation] requires a list of references,
 * you should pass each reference individually, followed by %NULL, e.g.
 *
 * ```c
 * gtk_accessible_update_relation (accessible,
 *                                 GTK_ACCESSIBLE_RELATION_CONTROLS,
 *                                   ref1, NULL,
 *                                 GTK_ACCESSIBLE_RELATION_LABELLED_BY,
 *                                   ref1, ref2, ref3, NULL,
 *                                 -1);
 * ```
 */
void
gtk_accessible_update_relation (GtkAccessible         *self,
                                GtkAccessibleRelation  first_relation,
                                ...)
{
  int relation;
  GtkATContext *context;
  va_list args;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  va_start (args, first_relation);

  relation = first_relation;

  while (relation != -1)
    {
      GError *error = NULL;
      GtkAccessibleValue *value =
        gtk_accessible_value_collect_for_relation ((GtkAccessibleRelation) relation, &error, &args);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for relation “%s”: %s",
                      gtk_accessible_relation_get_attribute_name (relation),
                      error->message);
          g_error_free (error);
          goto out;
        }

      gtk_at_context_set_accessible_relation (context, (GtkAccessibleRelation) relation, value);

      if (value != NULL)
        gtk_accessible_value_unref (value);

      relation = va_arg (args, int);
    }

  gtk_at_context_update (context);

out:
  va_end (args);

  g_object_unref (context);
}

/**
 * gtk_accessible_update_relation_value: (rename-to gtk_accessible_update_relation)
 * @self: a `GtkAccessible`
 * @n_relations: the number of accessible relations to set
 * @relations: (array length=n_relations): an array of `GtkAccessibleRelation`
 * @values: (array length=n_relations): an array of `GValues`, one for each relation
 *
 * Updates an array of accessible relations.
 *
 * This function should be called by `GtkWidget` types whenever an accessible
 * relation change must be communicated to assistive technologies.
 *
 * This function is meant to be used by language bindings.
 */
void
gtk_accessible_update_relation_value (GtkAccessible         *self,
                                      int                    n_relations,
                                      GtkAccessibleRelation  relations[],
                                      const GValue           values[])
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));
  g_return_if_fail (n_relations > 0);

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  for (int i = 0; i < n_relations; i++)
    {
      GtkAccessibleRelation relation = relations[i];
      const GValue *value = &(values[i]);
      GError *error = NULL;
      GtkAccessibleValue *real_value =
        gtk_accessible_value_collect_for_relation_value (relation, value, &error);

      if (error != NULL)
        {
          g_critical ("Unable to collect the value for relation “%s”: %s",
                      gtk_accessible_relation_get_attribute_name (relation),
                      error->message);
          g_error_free (error);
          break;
        }

      gtk_at_context_set_accessible_relation (context, relation, real_value);

      if (real_value != NULL)
        gtk_accessible_value_unref (real_value);
    }

  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_reset_relation:
 * @self: a `GtkAccessible`
 * @relation: a `GtkAccessibleRelation`
 *
 * Resets the accessible @relation to its default value.
 */
void
gtk_accessible_reset_relation (GtkAccessible         *self,
                               GtkAccessibleRelation  relation)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_set_accessible_relation (context, relation, NULL);
  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_announce:
 * @self: a `GtkAccessible`
 * @message: the string to announce
 * @priority: the priority of the announcement
 *
 * Requests the user's screen reader to announce the given message.
 *
 * This kind of notification is useful for messages that
 * either have only a visual representation or that are not
 * exposed visually at all, e.g. a notification about a
 * successful operation.
 *
 * Also, by using this API, you can ensure that the message
 * does not interrupts the user's current screen reader output.
 *
 * Since: 4.14
 */
void
gtk_accessible_announce (GtkAccessible                     *self,
                         const char                        *message,
                         GtkAccessibleAnnouncementPriority  priority)
{
  GtkATContext *context;

  g_return_if_fail (GTK_IS_ACCESSIBLE (self));

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_announce (context, message, priority);
  g_object_unref (context);
}

static const char *role_names[] = {
  [GTK_ACCESSIBLE_ROLE_ALERT] = NC_("accessibility", "alert"),
  [GTK_ACCESSIBLE_ROLE_ALERT_DIALOG] = NC_("accessibility", "alert dialog"),
  [GTK_ACCESSIBLE_ROLE_BANNER] = NC_("accessibility", "banner"),
  [GTK_ACCESSIBLE_ROLE_BUTTON] = NC_("accessibility", "button"),
  [GTK_ACCESSIBLE_ROLE_CAPTION] = NC_("accessibility", "caption"),
  [GTK_ACCESSIBLE_ROLE_CELL] = NC_("accessibility", "cell"),
  [GTK_ACCESSIBLE_ROLE_CHECKBOX] = NC_("accessibility", "checkbox"),
  [GTK_ACCESSIBLE_ROLE_COLUMN_HEADER] = NC_("accessibility", "column header"),
  [GTK_ACCESSIBLE_ROLE_COMBO_BOX] = NC_("accessibility", "combo box"),
  [GTK_ACCESSIBLE_ROLE_COMMAND] = NC_("accessibility", "command"),
  [GTK_ACCESSIBLE_ROLE_COMPOSITE] = NC_("accessibility", "composite"),
  [GTK_ACCESSIBLE_ROLE_DIALOG] = NC_("accessibility", "dialog"),
  [GTK_ACCESSIBLE_ROLE_DOCUMENT] = NC_("accessibility", "document"),
  [GTK_ACCESSIBLE_ROLE_FEED] = NC_("accessibility", "feed"),
  [GTK_ACCESSIBLE_ROLE_FORM] = NC_("accessibility", "form"),
  [GTK_ACCESSIBLE_ROLE_GENERIC] = NC_("accessibility", "generic"),
  [GTK_ACCESSIBLE_ROLE_GRID] = NC_("accessibility", "grid"),
  [GTK_ACCESSIBLE_ROLE_GRID_CELL] = NC_("accessibility", "grid cell"),
  [GTK_ACCESSIBLE_ROLE_GROUP] = NC_("accessibility", "group"),
  [GTK_ACCESSIBLE_ROLE_HEADING] = NC_("accessibility", "heading"),
  [GTK_ACCESSIBLE_ROLE_IMG] = NC_("accessibility", "image"),
  [GTK_ACCESSIBLE_ROLE_INPUT] = NC_("accessibility", "input"),
  [GTK_ACCESSIBLE_ROLE_LABEL] = NC_("accessibility", "label"),
  [GTK_ACCESSIBLE_ROLE_LANDMARK] = NC_("accessibility", "landmark"),
  [GTK_ACCESSIBLE_ROLE_LEGEND] = NC_("accessibility", "legend"),
  [GTK_ACCESSIBLE_ROLE_LINK] = NC_("accessibility", "link"),
  [GTK_ACCESSIBLE_ROLE_LIST] = NC_("accessibility", "list"),
  [GTK_ACCESSIBLE_ROLE_LIST_BOX] = NC_("accessibility", "list box"),
  [GTK_ACCESSIBLE_ROLE_LIST_ITEM] = NC_("accessibility", "list item"),
  [GTK_ACCESSIBLE_ROLE_LOG] = NC_("accessibility", "log"),
  [GTK_ACCESSIBLE_ROLE_MAIN] = NC_("accessibility", "main"),
  [GTK_ACCESSIBLE_ROLE_MARQUEE] = NC_("accessibility", "marquee"),
  [GTK_ACCESSIBLE_ROLE_MATH] = NC_("accessibility", "math"),
  [GTK_ACCESSIBLE_ROLE_METER] = NC_("accessibility", "meter"),
  [GTK_ACCESSIBLE_ROLE_MENU] = NC_("accessibility", "menu"),
  [GTK_ACCESSIBLE_ROLE_MENU_BAR] = NC_("accessibility", "menu bar"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM] = NC_("accessibility", "menu item"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX] = NC_("accessibility", "menu item checkbox"),
  [GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO] = NC_("accessibility", "menu item radio"),
  [GTK_ACCESSIBLE_ROLE_NAVIGATION] = NC_("accessibility", "navigation"),
  [GTK_ACCESSIBLE_ROLE_NONE] = NC_("accessibility", "none"),
  [GTK_ACCESSIBLE_ROLE_NOTE] = NC_("accessibility", "note"),
  [GTK_ACCESSIBLE_ROLE_OPTION] = NC_("accessibility", "option"),
  [GTK_ACCESSIBLE_ROLE_PRESENTATION] = NC_("accessibility", "presentation"),
  [GTK_ACCESSIBLE_ROLE_PROGRESS_BAR] = NC_("accessibility", "progress bar"),
  [GTK_ACCESSIBLE_ROLE_RADIO] = NC_("accessibility", "radio"),
  [GTK_ACCESSIBLE_ROLE_RADIO_GROUP] = NC_("accessibility", "radio group"),
  [GTK_ACCESSIBLE_ROLE_RANGE] = NC_("accessibility", "range"),
  [GTK_ACCESSIBLE_ROLE_REGION] = NC_("accessibility", "region"),
  [GTK_ACCESSIBLE_ROLE_ROW] = NC_("accessibility", "row"),
  [GTK_ACCESSIBLE_ROLE_ROW_GROUP] = NC_("accessibility", "row group"),
  [GTK_ACCESSIBLE_ROLE_ROW_HEADER] = NC_("accessibility", "row header"),
  [GTK_ACCESSIBLE_ROLE_SCROLLBAR] = NC_("accessibility", "scroll bar"),
  [GTK_ACCESSIBLE_ROLE_SEARCH] = NC_("accessibility", "search"),
  [GTK_ACCESSIBLE_ROLE_SEARCH_BOX] = NC_("accessibility", "search box"),
  [GTK_ACCESSIBLE_ROLE_SECTION] = NC_("accessibility", "section"),
  [GTK_ACCESSIBLE_ROLE_SECTION_HEAD] = NC_("accessibility", "section head"),
  [GTK_ACCESSIBLE_ROLE_SELECT] = NC_("accessibility", "select"),
  [GTK_ACCESSIBLE_ROLE_SEPARATOR] = NC_("accessibility", "separator"),
  [GTK_ACCESSIBLE_ROLE_SLIDER] = NC_("accessibility", "slider"),
  [GTK_ACCESSIBLE_ROLE_SPIN_BUTTON] = NC_("accessibility", "spin button"),
  [GTK_ACCESSIBLE_ROLE_STATUS] = NC_("accessibility", "status"),
  [GTK_ACCESSIBLE_ROLE_STRUCTURE] = NC_("accessibility", "structure"),
  [GTK_ACCESSIBLE_ROLE_SWITCH] = NC_("accessibility", "switch"),
  [GTK_ACCESSIBLE_ROLE_TAB] = NC_("accessibility", "tab"),
  [GTK_ACCESSIBLE_ROLE_TABLE] = NC_("accessibility", "table"),
  [GTK_ACCESSIBLE_ROLE_TAB_LIST] = NC_("accessibility", "tab list"),
  [GTK_ACCESSIBLE_ROLE_TAB_PANEL] = NC_("accessibility", "tab panel"),
  [GTK_ACCESSIBLE_ROLE_TEXT_BOX] = NC_("accessibility", "text box"),
  [GTK_ACCESSIBLE_ROLE_TIME] = NC_("accessibility", "time"),
  [GTK_ACCESSIBLE_ROLE_TIMER] = NC_("accessibility", "timer"),
  [GTK_ACCESSIBLE_ROLE_TOOLBAR] = NC_("accessibility", "tool bar"),
  [GTK_ACCESSIBLE_ROLE_TOOLTIP] = NC_("accessibility", "tool tip"),
  [GTK_ACCESSIBLE_ROLE_TREE] = NC_("accessibility", "tree"),
  [GTK_ACCESSIBLE_ROLE_TREE_GRID] = NC_("accessibility", "tree grid"),
  [GTK_ACCESSIBLE_ROLE_TREE_ITEM] = NC_("accessibility", "tree item"),
  [GTK_ACCESSIBLE_ROLE_WIDGET] = NC_("accessibility", "widget"),
  [GTK_ACCESSIBLE_ROLE_WINDOW] = NC_("accessibility", "window"),
  [GTK_ACCESSIBLE_ROLE_TOGGLE_BUTTON] = NC_("accessibility", "toggle button"),
  [GTK_ACCESSIBLE_ROLE_APPLICATION] = NC_("accessibility", "application"),
  [GTK_ACCESSIBLE_ROLE_PARAGRAPH] = NC_("accessibility", "paragraph"),
  [GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE] = NC_("accessibility", "block quote"),
  [GTK_ACCESSIBLE_ROLE_ARTICLE] = NC_("accessibility", "article"),
  [GTK_ACCESSIBLE_ROLE_COMMENT] = NC_("accessibility", "comment"),
  [GTK_ACCESSIBLE_ROLE_TERMINAL] = NC_("accessibility", "terminal"),
};

/*< private >
 * gtk_accessible_role_to_name:
 * @role: a `GtkAccessibleRole`
 * @domain: (nullable): the translation domain
 *
 * Converts a `GtkAccessibleRole` value to the equivalent role name.
 *
 * If @domain is not %NULL, the returned string will be localized.
 *
 * Returns: (transfer none): the name of the role
 */
const char *
gtk_accessible_role_to_name (GtkAccessibleRole  role,
                             const char        *domain)
{
  if (domain != NULL)
    return g_dpgettext2 (domain, "accessibility", role_names[role]);

  return role_names[role];
}

static struct {
  GtkAccessibleRole superclass;
  GtkAccessibleRole role;
} superclasses[] = {
  { GTK_ACCESSIBLE_ROLE_COMMAND, GTK_ACCESSIBLE_ROLE_BUTTON },
  { GTK_ACCESSIBLE_ROLE_COMMAND, GTK_ACCESSIBLE_ROLE_LINK },
  { GTK_ACCESSIBLE_ROLE_COMMAND, GTK_ACCESSIBLE_ROLE_MENU_ITEM },
  { GTK_ACCESSIBLE_ROLE_COMPOSITE, GTK_ACCESSIBLE_ROLE_GRID },
  { GTK_ACCESSIBLE_ROLE_COMPOSITE, GTK_ACCESSIBLE_ROLE_SELECT },
  { GTK_ACCESSIBLE_ROLE_COMPOSITE, GTK_ACCESSIBLE_ROLE_SPIN_BUTTON },
  { GTK_ACCESSIBLE_ROLE_COMPOSITE, GTK_ACCESSIBLE_ROLE_TAB_LIST },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_CHECKBOX },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_COMBO_BOX },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_OPTION },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_RADIO },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_SLIDER },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_SPIN_BUTTON },
  { GTK_ACCESSIBLE_ROLE_INPUT, GTK_ACCESSIBLE_ROLE_TEXT_BOX },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_BANNER },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_FORM },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_MAIN },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_NAVIGATION },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_REGION },
  { GTK_ACCESSIBLE_ROLE_LANDMARK, GTK_ACCESSIBLE_ROLE_SEARCH },
  { GTK_ACCESSIBLE_ROLE_RANGE, GTK_ACCESSIBLE_ROLE_METER },
  { GTK_ACCESSIBLE_ROLE_RANGE, GTK_ACCESSIBLE_ROLE_PROGRESS_BAR },
  { GTK_ACCESSIBLE_ROLE_RANGE, GTK_ACCESSIBLE_ROLE_SCROLLBAR },
  { GTK_ACCESSIBLE_ROLE_RANGE, GTK_ACCESSIBLE_ROLE_SLIDER },
  { GTK_ACCESSIBLE_ROLE_RANGE, GTK_ACCESSIBLE_ROLE_SPIN_BUTTON },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_ALERT },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_BLOCK_QUOTE },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_CAPTION },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_CELL },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_GROUP },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_IMG },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_LANDMARK },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_LIST },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_LIST_ITEM },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_LOG },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_MARQUEE },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_MATH },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_NOTE },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_PARAGRAPH },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_STATUS },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_TABLE },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_TAB_PANEL },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_TIME },
  { GTK_ACCESSIBLE_ROLE_SECTION, GTK_ACCESSIBLE_ROLE_TOOLTIP },
  { GTK_ACCESSIBLE_ROLE_SECTION_HEAD, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER },
  { GTK_ACCESSIBLE_ROLE_SECTION_HEAD, GTK_ACCESSIBLE_ROLE_HEADING },
  { GTK_ACCESSIBLE_ROLE_SECTION_HEAD, GTK_ACCESSIBLE_ROLE_ROW_HEADER },
  { GTK_ACCESSIBLE_ROLE_SECTION_HEAD, GTK_ACCESSIBLE_ROLE_TAB },
  { GTK_ACCESSIBLE_ROLE_SELECT, GTK_ACCESSIBLE_ROLE_LIST_BOX },
  { GTK_ACCESSIBLE_ROLE_SELECT, GTK_ACCESSIBLE_ROLE_MENU },
  { GTK_ACCESSIBLE_ROLE_SELECT, GTK_ACCESSIBLE_ROLE_RADIO_GROUP },
  { GTK_ACCESSIBLE_ROLE_SELECT, GTK_ACCESSIBLE_ROLE_TREE },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_APPLICATION },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_DOCUMENT },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_GENERIC },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_PRESENTATION },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_RANGE },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_ROW_GROUP },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_SECTION },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_SECTION_HEAD },
  { GTK_ACCESSIBLE_ROLE_STRUCTURE, GTK_ACCESSIBLE_ROLE_SEPARATOR },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_COMMAND },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_COMPOSITE },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_GRID_CELL },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_INPUT },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_PROGRESS_BAR },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_SCROLLBAR },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_SEPARATOR },
  { GTK_ACCESSIBLE_ROLE_WIDGET, GTK_ACCESSIBLE_ROLE_TAB },
  { GTK_ACCESSIBLE_ROLE_WINDOW, GTK_ACCESSIBLE_ROLE_DIALOG },
  { GTK_ACCESSIBLE_ROLE_CHECKBOX, GTK_ACCESSIBLE_ROLE_SWITCH },
  { GTK_ACCESSIBLE_ROLE_GRID_CELL, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER },
  { GTK_ACCESSIBLE_ROLE_GRID_CELL, GTK_ACCESSIBLE_ROLE_ROW_HEADER },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM, GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX },
  { GTK_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX, GTK_ACCESSIBLE_ROLE_MENU_ITEM_RADIO },
  { GTK_ACCESSIBLE_ROLE_TREE, GTK_ACCESSIBLE_ROLE_TREE_GRID },
  { GTK_ACCESSIBLE_ROLE_CELL, GTK_ACCESSIBLE_ROLE_COLUMN_HEADER },
  { GTK_ACCESSIBLE_ROLE_CELL, GTK_ACCESSIBLE_ROLE_GRID_CELL },
  { GTK_ACCESSIBLE_ROLE_CELL, GTK_ACCESSIBLE_ROLE_ROW_HEADER },
  { GTK_ACCESSIBLE_ROLE_GROUP, GTK_ACCESSIBLE_ROLE_ROW },
  { GTK_ACCESSIBLE_ROLE_GROUP, GTK_ACCESSIBLE_ROLE_SELECT },
  { GTK_ACCESSIBLE_ROLE_GROUP, GTK_ACCESSIBLE_ROLE_TOOLBAR },
  { GTK_ACCESSIBLE_ROLE_LIST, GTK_ACCESSIBLE_ROLE_FEED },
  { GTK_ACCESSIBLE_ROLE_LIST_ITEM, GTK_ACCESSIBLE_ROLE_TREE_ITEM },
  { GTK_ACCESSIBLE_ROLE_TABLE, GTK_ACCESSIBLE_ROLE_GRID },
  { GTK_ACCESSIBLE_ROLE_ALERT, GTK_ACCESSIBLE_ROLE_ALERT_DIALOG },
  { GTK_ACCESSIBLE_ROLE_STATUS, GTK_ACCESSIBLE_ROLE_TIMER },
  { GTK_ACCESSIBLE_ROLE_DIALOG, GTK_ACCESSIBLE_ROLE_ALERT_DIALOG },
  { GTK_ACCESSIBLE_ROLE_DOCUMENT, GTK_ACCESSIBLE_ROLE_ARTICLE },
  { GTK_ACCESSIBLE_ROLE_ARTICLE, GTK_ACCESSIBLE_ROLE_COMMENT },
  { GTK_ACCESSIBLE_ROLE_TERMINAL, GTK_ACCESSIBLE_ROLE_WIDGET },
};

gboolean
gtk_accessible_role_is_subclass (GtkAccessibleRole role,
                                 GtkAccessibleRole superclass)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (superclasses); i++)
    {
      if (superclasses[i].role == role &&
          superclasses[i].superclass == superclass)
        return TRUE;
    }

  return FALSE;
}

/*< private >
 * gtk_accessible_role_is_range_subclass:
 * @role: a `GtkAccessibleRole`
 *
 * Checks if @role is considered to be a subclass of %GTK_ACCESSIBLE_ROLE_RANGE
 * according to the WAI-ARIA specification.
 *
 * Returns: whether the @role is range-like
 */
gboolean
gtk_accessible_role_is_range_subclass (GtkAccessibleRole role)
{
  return gtk_accessible_role_is_subclass (role, GTK_ACCESSIBLE_ROLE_RANGE);
}

/* < private >
 * gtk_accessible_role_is_abstract:
 * @role: a `GtkAccessibleRole`
 *
 * Checks if @role is considered abstract and should not be used
 * for concrete widgets.
 *
 * Returns: whether the role is abstract
 */
gboolean
gtk_accessible_role_is_abstract (GtkAccessibleRole role)
{
  switch ((int) role)
    {
    case GTK_ACCESSIBLE_ROLE_COMMAND:
    case GTK_ACCESSIBLE_ROLE_COMPOSITE:
    case GTK_ACCESSIBLE_ROLE_INPUT:
    case GTK_ACCESSIBLE_ROLE_LANDMARK:
    case GTK_ACCESSIBLE_ROLE_RANGE:
    case GTK_ACCESSIBLE_ROLE_SECTION:
    case GTK_ACCESSIBLE_ROLE_SECTION_HEAD:
    case GTK_ACCESSIBLE_ROLE_SELECT:
    case GTK_ACCESSIBLE_ROLE_STRUCTURE:
    case GTK_ACCESSIBLE_ROLE_WIDGET:
    case GTK_ACCESSIBLE_ROLE_WINDOW:
      return TRUE;
    default:
      return FALSE;
    }
}

/*< private >
 * gtk_accessible_platform_changed:
 * @self: a `GtkAccessible`
 * @change: the platform state change to report
 *
 * Notify accessible technologies that a platform value has changed.
 *
 * ARIA discriminates between author-controlled states and 'platform'
 * states, which are not. This function can be used by widgets to
 * inform ATs that a platform state, such as focus, has changed.
 *
 * Note that the state itself is not included in this API.
 * AT backends should use [method@Gtk.Accessible.get_platform_state]
 * to obtain the actual state.
 */
void
gtk_accessible_platform_changed (GtkAccessible               *self,
                                 GtkAccessiblePlatformChange  change)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  context = gtk_accessible_get_at_context (self);

  /* propagate changes up from ignored widgets */
  if (gtk_accessible_get_accessible_role (self) == GTK_ACCESSIBLE_ROLE_NONE)
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (self);

      if (parent != NULL)
        {
          g_clear_object (&context);
          context = gtk_accessible_get_at_context (parent);
          g_object_unref (parent);
        }
    }

  if (context == NULL)
    return;

  gtk_at_context_platform_changed (context, change);
  gtk_at_context_update (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_get_platform_state:
 * @self: a `GtkAccessible`
 * @state: platform state to query
 *
 * Query a platform state, such as focus.
 *
 * See gtk_accessible_platform_changed().
 *
 * This functionality can be overridden by `GtkAccessible`
 * implementations, e.g. to get platform state from an ignored
 * child widget, as is the case for `GtkText` wrappers.
 *
 * Returns: the value of @state for the accessible
 *
 * Since: 4.10
 */
gboolean
gtk_accessible_get_platform_state (GtkAccessible              *self,
                                   GtkAccessiblePlatformState  state)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), FALSE);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_platform_state (self, state);
}

/*< private >
 * gtk_accessible_bounds_changed:
 * @self: a `GtkAccessible`
 *
 * This function can be used to inform ATs that an
 * accessibles bounds (ie its screen extents) have
 * changed.
 *
 * Note that the bounds are not included in this API.
 * AT backends should use [method@Gtk.Accessible.get_bounds] to get them.
 */
void
gtk_accessible_bounds_changed (GtkAccessible *self)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return;

  gtk_at_context_bounds_changed (context);
  g_object_unref (context);
}

/**
 * gtk_accessible_get_bounds:
 * @self: a `GtkAccessible`
 * @x: (out): the x coordinate of the top left corner of the accessible
 * @y: (out): the y coordinate of the top left corner of the widget
 * @width: (out): the width of the accessible object
 * @height: (out): the height of the accessible object
 *
 * Queries the coordinates and dimensions of this accessible
 *
 * This functionality can be overridden by `GtkAccessible`
 * implementations, e.g. to get the bounds from an ignored
 * child widget.
 *
 * Returns: true if the bounds are valid, and false otherwise
 *
 * Since: 4.10
 */
gboolean
gtk_accessible_get_bounds (GtkAccessible *self,
                           int           *x,
                           int           *y,
                           int           *width,
                           int           *height)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (self), FALSE);
  g_return_val_if_fail (x != NULL && y != NULL, FALSE);
  g_return_val_if_fail (width != NULL && height != NULL, FALSE);

  return GTK_ACCESSIBLE_GET_IFACE (self)->get_bounds (self, x, y, width, height);
}

struct _GtkAccessibleList
{
  GList *objects;
};

/**
 * gtk_accessible_list_new_from_list:
 * @list: (element-type GtkAccessible): a reference to a `GList` containing a list of accessible values
 *
 * Allocates a new `GtkAccessibleList`, doing a shallow copy of the
 * passed list of `GtkAccessible` instances.
 *
 * Returns: (transfer full): the list of accessible instances
 *
 * Since: 4.14
 */
GtkAccessibleList *
gtk_accessible_list_new_from_list (GList *list)
{
  GtkAccessibleList *accessible_list = g_new (GtkAccessibleList, 1);

  accessible_list->objects = g_list_copy (list);

  return accessible_list;
}

/**
 * gtk_accessible_list_new_from_array:
 * @accessibles: (array length=n_accessibles): array of GtkAccessible
 * @n_accessibles: length of @accessibles array
 *
 * Allocates a new list of accessible instances.
 *
 * Returns: (transfer full): the newly created list of accessible instances
 *
 * Since: 4.14
 */
GtkAccessibleList *
gtk_accessible_list_new_from_array (GtkAccessible **accessibles,
                                    gsize           n_accessibles)
{
  GtkAccessibleList *accessible_list;
  GList *list = NULL;

  g_return_val_if_fail (accessibles == NULL || n_accessibles == 0, NULL);

  accessible_list = g_new (GtkAccessibleList, 1);

  for (gsize i = 0; i < n_accessibles; i++)
    {
      list = g_list_prepend (list, accessibles[i]);
    }

  accessible_list->objects = g_list_reverse (list);

  return accessible_list;
}

static void
gtk_accessible_list_free (GtkAccessibleList *accessible_list)
{
  g_free (accessible_list->objects);
  g_free (accessible_list);
}

static GtkAccessibleList *
gtk_accessible_list_copy (GtkAccessibleList *accessible_list)
{
  return gtk_accessible_list_new_from_list (accessible_list->objects);
}

/**
 * gtk_accessible_list_get_objects:
 *
 * Gets the list of objects this boxed type holds
 *
 * Returns: (transfer container) (element-type GtkAccessible): a shallow copy of the objects
 *
 * Since: 4.14
 */
GList *
gtk_accessible_list_get_objects (GtkAccessibleList *accessible_list)
{
  return g_list_copy (accessible_list->objects);
}

G_DEFINE_BOXED_TYPE (GtkAccessibleList, gtk_accessible_list,
                     gtk_accessible_list_copy,
                     gtk_accessible_list_free)

/*< private >
 * gtk_accessible_should_present:
 * @self: a `GtkAccessible`
 *
 * Returns whether this accessible should be represented to ATs.
 *
 * By default, hidden widgets are are among these, but there can
 * be other reasons to return %FALSE, e.g. for widgets that are
 * purely presentations, or for widgets whose functionality is
 * represented elsewhere, as is the case for `GtkText` widgets.
 *
 * Returns: %TRUE if the widget should be represented
 */
gboolean
gtk_accessible_should_present (GtkAccessible *self)
{
  GtkAccessibleRole role;
  GtkATContext *context;
  gboolean res = TRUE;

  if (GTK_IS_WIDGET (self) &&
      !gtk_widget_get_visible (GTK_WIDGET (self)))
    return FALSE;

  role = gtk_accessible_get_accessible_role (self);
  if (role == GTK_ACCESSIBLE_ROLE_NONE ||
      role == GTK_ACCESSIBLE_ROLE_PRESENTATION)
    return FALSE;

  context = gtk_accessible_get_at_context (self);
  if (context == NULL)
    return FALSE;

  if (gtk_at_context_has_accessible_state (context, GTK_ACCESSIBLE_STATE_HIDDEN))
    {
      GtkAccessibleValue *value;

      value = gtk_at_context_get_accessible_state (context, GTK_ACCESSIBLE_STATE_HIDDEN);
      if (gtk_boolean_accessible_value_get (value))
        res = FALSE;
    }

  g_object_unref (context);

  return res;
}

void
gtk_accessible_update_children (GtkAccessible           *self,
                                GtkAccessible           *child,
                                GtkAccessibleChildState  state)
{
  GtkATContext *context;

  if (GTK_IS_WIDGET (self) &&
      gtk_widget_get_root (GTK_WIDGET (self)) == NULL)
    return;

  /* propagate changes up from ignored widgets */
  if (gtk_accessible_get_accessible_role (self) == GTK_ACCESSIBLE_ROLE_NONE)
    {
      GtkAccessible *parent = gtk_accessible_get_accessible_parent (self);

      context = gtk_accessible_get_at_context (parent);

      g_object_unref (parent);
    }
  else
    {
      context = gtk_accessible_get_at_context (self);
    }

  if (context == NULL)
    return;

  gtk_at_context_child_changed (context, 1 << state, child);
  gtk_at_context_update (context);
  g_object_unref (context);
}
