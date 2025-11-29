/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtkshortcut.h"

#include "gtkshortcutaction.h"
#include "gtkshortcuttrigger.h"
#include "gtkwidget.h"

/**
 * GtkShortcut:
 *
 * A `GtkShortcut` describes a keyboard shortcut.
 *
 * It contains a description of how to trigger the shortcut via a
 * [class@Gtk.ShortcutTrigger] and a way to activate the shortcut
 * on a widget via a [class@Gtk.ShortcutAction].
 *
 * The actual work is usually done via [class@Gtk.ShortcutController],
 * which decides if and when to activate a shortcut. Using that controller
 * directly however is rarely necessary as various higher level
 * convenience APIs exist on `GtkWidget`s that make it easier to use
 * shortcuts in GTK.
 *
 * `GtkShortcut` does provide functionality to make it easy for users
 * to work with shortcuts, either by providing informational strings
 * for display purposes or by allowing shortcuts to be configured.
 */

struct _GtkShortcut
{
  GObject parent_instance;

  GtkShortcutAction *action;
  GtkShortcutTrigger *trigger;
  GVariant *args;
};

enum
{
  PROP_0,
  PROP_ACTION,
  PROP_ARGUMENTS,
  PROP_TRIGGER,

  N_PROPS
};

G_DEFINE_TYPE (GtkShortcut, gtk_shortcut, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_shortcut_dispose (GObject *object)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  g_clear_object (&self->action);
  g_clear_object (&self->trigger);
  g_clear_pointer (&self->args, g_variant_unref);

  G_OBJECT_CLASS (gtk_shortcut_parent_class)->dispose (object);
}

static void
gtk_shortcut_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  switch (property_id)
    {
    case PROP_ACTION:
      g_value_set_object (value, self->action);
      break;

    case PROP_ARGUMENTS:
      g_value_set_variant (value, self->args);
      break;

    case PROP_TRIGGER:
      g_value_set_object (value, self->trigger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_shortcut_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  switch (property_id)
    {
    case PROP_ACTION:
      gtk_shortcut_set_action (self, g_value_dup_object (value));
      break;

    case PROP_ARGUMENTS:
      gtk_shortcut_set_arguments (self, g_value_get_variant (value));
      break;

    case PROP_TRIGGER:
      gtk_shortcut_set_trigger (self, g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_shortcut_class_init (GtkShortcutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_shortcut_dispose;
  gobject_class->get_property = gtk_shortcut_get_property;
  gobject_class->set_property = gtk_shortcut_set_property;

  /**
   * GtkShortcut:action: (attributes org.gtk.Property.get=gtk_shortcut_get_action org.gtk.Property.set=gtk_shortcut_set_action)
   *
   * The action that gets activated by this shortcut.
   */
  properties[PROP_ACTION] =
    g_param_spec_object ("action", NULL, NULL,
                         GTK_TYPE_SHORTCUT_ACTION,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcut:arguments: (attributes org.gtk.Property.get=gtk_shortcut_get_arguments org.gtk.Property.set=gtk_shortcut_set_arguments)
   *
   * Arguments passed to activation.
   */
  properties[PROP_ARGUMENTS] =
    g_param_spec_variant ("arguments", NULL, NULL,
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcut:trigger: (attributes org.gtk.Property.get=gtk_shortcut_get_trigger org.gtk.Property.set=gtk_shortcut_set_trigger)
   *
   * The trigger that triggers this shortcut.
   */
  properties[PROP_TRIGGER] =
    g_param_spec_object ("trigger", NULL, NULL,
                         GTK_TYPE_SHORTCUT_TRIGGER,
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_shortcut_init (GtkShortcut *self)
{
  self->action = g_object_ref (gtk_nothing_action_get ());
  self->trigger = g_object_ref (gtk_never_trigger_get ());
}

/**
 * gtk_shortcut_new:
 * @trigger: (transfer full) (nullable): The trigger that will trigger the shortcut
 * @action: (transfer full) (nullable): The action that will be activated upon
 *    triggering
 *
 * Creates a new `GtkShortcut` that is triggered by
 * @trigger and then activates @action.
 *
 * Returns: a new `GtkShortcut`
 */
GtkShortcut *
gtk_shortcut_new (GtkShortcutTrigger *trigger,
                  GtkShortcutAction  *action)
{
  GtkShortcut *shortcut;

  shortcut = g_object_new (GTK_TYPE_SHORTCUT,
                           "action", action,
                           "trigger", trigger,
                           NULL);

  if (trigger)
    g_object_unref (trigger);
  if (action)
    g_object_unref (action);

  return shortcut;
}

/**
 * gtk_shortcut_new_with_arguments: (skip)
 * @trigger: (transfer full) (nullable): The trigger that will trigger the shortcut
 * @action: (transfer full) (nullable): The action that will be activated upon
 *   triggering
 * @format_string: (nullable): GVariant format string for arguments or %NULL for
 *   no arguments
 * @...: arguments, as given by format string.
 *
 * Creates a new `GtkShortcut` that is triggered by @trigger and then activates
 * @action with arguments given by @format_string.
 *
 * Returns: a new `GtkShortcut`
 */
GtkShortcut *
gtk_shortcut_new_with_arguments (GtkShortcutTrigger *trigger,
                                 GtkShortcutAction  *action,
                                 const char         *format_string,
                                 ...)
{
  GtkShortcut *shortcut;
  GVariant *args;

  if (format_string)
    {
      va_list valist;
      va_start (valist, format_string);
      args = g_variant_new_va (format_string, NULL, &valist);
      va_end (valist);
    }
  else
    {
      args = NULL;
    }

  shortcut = g_object_new (GTK_TYPE_SHORTCUT,
                           "action", action,
                           "arguments", args,
                           "trigger", trigger,
                           NULL);

  if (trigger)
    g_object_unref (trigger);
  if (action)
    g_object_unref (action);

  return shortcut;
}

/**
 * gtk_shortcut_get_action: (attributes org.gtk.Method.get_property=action)
 * @self: a `GtkShortcut`
 *
 * Gets the action that is activated by this shortcut.
 *
 * Returns: (transfer none) (nullable): the action
 */
GtkShortcutAction *
gtk_shortcut_get_action (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->action;
}

/**
 * gtk_shortcut_set_action: (attributes org.gtk.Method.set_property=action)
 * @self: a `GtkShortcut`
 * @action: (transfer full) (nullable): The new action.
 *   If the @action is %NULL, the nothing action will be used.
 *
 * Sets the new action for @self to be @action.
 */
void
gtk_shortcut_set_action (GtkShortcut *self,
                         GtkShortcutAction *action)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (action == NULL)
    action = g_object_ref (gtk_nothing_action_get ());

  if (g_set_object (&self->action, action))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTION]);
      g_object_unref (action);
    }
}

/**
 * gtk_shortcut_get_trigger: (attributes org.gtk.Method.get_property=trigger)
 * @self: a `GtkShortcut`
 *
 * Gets the trigger used to trigger @self.
 *
 * Returns: (transfer none) (nullable): the trigger used
 */
GtkShortcutTrigger *
gtk_shortcut_get_trigger (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->trigger;
}

/**
 * gtk_shortcut_set_trigger: (attributes org.gtk.Method.set_property=trigger)
 * @self: a `GtkShortcut`
 * @trigger: (transfer full) (nullable): The new trigger.
 *   If the @trigger is %NULL, the never trigger will be used.
 *
 * Sets the new trigger for @self to be @trigger.
 */
void
gtk_shortcut_set_trigger (GtkShortcut *self,
                          GtkShortcutTrigger *trigger)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (trigger == NULL)
    trigger = g_object_ref (gtk_never_trigger_get ());

  if (g_set_object (&self->trigger, trigger))
    {
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRIGGER]);
      g_object_unref (trigger);
    }
}

/**
 * gtk_shortcut_get_arguments: (attributes org.gtk.Method.get_property=arguments)
 * @self: a `GtkShortcut`
 *
 * Gets the arguments that are passed when activating the shortcut.
 *
 * Returns: (transfer none) (nullable): the arguments
 */
GVariant *
gtk_shortcut_get_arguments (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->args;
}

/**
 * gtk_shortcut_set_arguments: (attributes org.gtk.Method.set_property=arguments)
 * @self: a `GtkShortcut`
 * @args: (nullable): arguments to pass when activating @self
 *
 * Sets the arguments to pass when activating the shortcut.
 */
void
gtk_shortcut_set_arguments (GtkShortcut *self,
                            GVariant    *args)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (self->args == args)
    return;
  
  g_clear_pointer (&self->args, g_variant_unref);
  if (args)
    self->args = g_variant_ref_sink (args);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARGUMENTS]);
}
