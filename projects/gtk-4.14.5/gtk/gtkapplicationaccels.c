/*
 * Copyright © 2013 Canonical Limited
 * Copyright © 2016 Sébastien Wilmet
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Sébastien Wilmet <swilmet@gnome.org>
 */

#include "config.h"

#include "gtkapplicationaccelsprivate.h"

#include "gtkactionmuxerprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcutaction.h"
#include "gtkshortcuttrigger.h"

struct _GtkApplicationAccels
{
  GObject parent;

  GListModel *shortcuts;
};

G_DEFINE_TYPE (GtkApplicationAccels, gtk_application_accels, G_TYPE_OBJECT)

static void
gtk_application_accels_finalize (GObject *object)
{
  GtkApplicationAccels *accels = GTK_APPLICATION_ACCELS (object);

  g_list_store_remove_all (G_LIST_STORE (accels->shortcuts));
  g_object_unref (accels->shortcuts);

  G_OBJECT_CLASS (gtk_application_accels_parent_class)->finalize (object);
}

static void
gtk_application_accels_class_init (GtkApplicationAccelsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_application_accels_finalize;
}

static void
gtk_application_accels_init (GtkApplicationAccels *accels)
{
  accels->shortcuts = G_LIST_MODEL (g_list_store_new (GTK_TYPE_SHORTCUT));
}

GtkApplicationAccels *
gtk_application_accels_new (void)
{
  return g_object_new (GTK_TYPE_APPLICATION_ACCELS, NULL);
}

void
gtk_application_accels_set_accels_for_action (GtkApplicationAccels *accels,
                                              const char           *detailed_action_name,
                                              const char * const  *accelerators)
{
  char *action_name;
  GVariant *target;
  GtkShortcut *shortcut;
  GtkShortcutTrigger *trigger = NULL;
  GError *error = NULL;
  guint i;

  if (!g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error))
    {
      g_critical ("Error parsing action name: %s", error->message);
      g_error_free (error);
      return;
    }

  /* remove the accelerator if it already exists */
  for (i = 0; i < g_list_model_get_n_items (accels->shortcuts); i++)
    {
      GtkShortcut *shortcut_i = g_list_model_get_item (accels->shortcuts, i);
      GtkShortcutAction *action = gtk_shortcut_get_action (shortcut_i);
      GVariant *args = gtk_shortcut_get_arguments (shortcut_i);

      if (!GTK_IS_NAMED_ACTION (action) ||
          !g_str_equal (gtk_named_action_get_action_name (GTK_NAMED_ACTION (action)), action_name))
        {
          g_object_unref (shortcut_i);
          continue;
        }

      if ((target == NULL && args != NULL) ||
          (target != NULL && (args == NULL || !g_variant_equal (target, args))))
        {
          g_object_unref (shortcut_i);
          continue;
        }

      g_list_store_remove (G_LIST_STORE (accels->shortcuts), i);
      g_object_unref (shortcut_i);
      break;
    }

  if (accelerators == NULL)
    goto out;

  for (i = 0; accelerators[i]; i++)
    {
      GtkShortcutTrigger *new_trigger;
      guint key, modifier;

      if (!gtk_accelerator_parse (accelerators[i], &key, &modifier))
        {
          g_critical ("Unable to parse accelerator '%s': ignored request to install accelerators",
                      accelerators[i]);
          g_clear_object (&trigger);
          goto out;
        }
      new_trigger = gtk_keyval_trigger_new (key, modifier);
      if (trigger)
        trigger = gtk_alternative_trigger_new (trigger, new_trigger);
      else
        trigger = new_trigger;
    }
  if (trigger == NULL)
    goto out;

  shortcut = gtk_shortcut_new (trigger, gtk_named_action_new (action_name));
  gtk_shortcut_set_arguments (shortcut, target);
  g_list_store_append (G_LIST_STORE (accels->shortcuts), shortcut);
  g_object_unref (shortcut);

out:
  g_free (action_name);
  if (target)
    g_variant_unref (target);
}

static void
append_accelerators (GPtrArray          *accels,
                     GtkShortcutTrigger *trigger)
{
  if (GTK_IS_KEYVAL_TRIGGER (trigger))
    {
      GtkKeyvalTrigger *kt = GTK_KEYVAL_TRIGGER (trigger);
      guint keyval = gtk_keyval_trigger_get_keyval (kt);
      GdkModifierType mods = gtk_keyval_trigger_get_modifiers (kt);

      g_ptr_array_add (accels, gtk_accelerator_name (keyval, mods));
      return;
    }
  else if (GTK_IS_ALTERNATIVE_TRIGGER (trigger))
    {
      GtkAlternativeTrigger *at = GTK_ALTERNATIVE_TRIGGER (trigger);
      GtkShortcutTrigger *first = gtk_alternative_trigger_get_first (at);
      GtkShortcutTrigger *second = gtk_alternative_trigger_get_second (at);

      append_accelerators (accels, first);
      append_accelerators (accels, second); 
      return;
    }
}

char **
gtk_application_accels_get_accels_for_action (GtkApplicationAccels *accels,
                                              const char           *detailed_action_name)
{
  GPtrArray *result;
  char *action_name;
  GVariant *target;
  GError *error = NULL;
  guint i;

  result = g_ptr_array_new ();

  if (!g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error))
    {
      g_critical ("Error parsing action name: %s", error->message);
      g_error_free (error);
      g_ptr_array_add (result, NULL);
      return (char **) g_ptr_array_free (result, FALSE);
    }

  for (i = 0; i < g_list_model_get_n_items (accels->shortcuts); i++)
    {
      GtkShortcut *shortcut = g_list_model_get_item (accels->shortcuts, i);
      GtkShortcutAction *action = gtk_shortcut_get_action (shortcut);
      GVariant *args = gtk_shortcut_get_arguments (shortcut);

      if (!GTK_IS_NAMED_ACTION (action) ||
          !g_str_equal (gtk_named_action_get_action_name (GTK_NAMED_ACTION (action)), action_name))
        {
          g_object_unref (shortcut);
          continue;
        }

      if ((target == NULL && args != NULL) ||
          (target != NULL && (args == NULL || !g_variant_equal (target, args))))
        {
          g_object_unref (shortcut);
          continue;
        }

      append_accelerators (result, gtk_shortcut_get_trigger (shortcut));
      g_object_unref (shortcut);
      break;
    }

  g_free (action_name);
  if (target)
    g_variant_unref (target);
  g_ptr_array_add (result, NULL);
  return (char **) g_ptr_array_free (result, FALSE);
}

static gboolean
trigger_matches_accel (GtkShortcutTrigger *trigger,
                       guint               keyval,
                       GdkModifierType     modifiers)
{
  if (GTK_IS_KEYVAL_TRIGGER (trigger))
    {
      GtkKeyvalTrigger *kt = GTK_KEYVAL_TRIGGER (trigger);

      return gtk_keyval_trigger_get_keyval (kt) == keyval
          && gtk_keyval_trigger_get_modifiers (kt) == modifiers;
    }
  else if (GTK_IS_ALTERNATIVE_TRIGGER (trigger))
    {
      GtkAlternativeTrigger *at = GTK_ALTERNATIVE_TRIGGER (trigger);

      return trigger_matches_accel (gtk_alternative_trigger_get_first (at), keyval, modifiers)
          || trigger_matches_accel (gtk_alternative_trigger_get_second (at), keyval, modifiers);
    }
  else
    {
      return FALSE;
    }
}

static char *
get_detailed_name_for_shortcut (GtkShortcut *shortcut)
{
  GtkShortcutAction *action = gtk_shortcut_get_action (shortcut);

  if (!GTK_IS_NAMED_ACTION (action))
    return NULL;

  return g_action_print_detailed_name (gtk_named_action_get_action_name (GTK_NAMED_ACTION (action)),
                                       gtk_shortcut_get_arguments (shortcut));
}

char **
gtk_application_accels_get_actions_for_accel (GtkApplicationAccels *accels,
                                              const char           *accel)
{
  GPtrArray *result;
  guint key, modifiers;
  guint i;

  if (!gtk_accelerator_parse (accel, &key, &modifiers))
    {
      g_critical ("invalid accelerator string '%s'", accel);
      return NULL;
    }

  result = g_ptr_array_new ();

  for (i = 0; i < g_list_model_get_n_items (accels->shortcuts); i++)
    {
      GtkShortcut *shortcut = g_list_model_get_item (accels->shortcuts, i);
      char *detailed_name;

      if (!trigger_matches_accel (gtk_shortcut_get_trigger (shortcut), key, modifiers))
        {
          g_object_unref (shortcut);
          continue;
        }

      detailed_name = get_detailed_name_for_shortcut (shortcut);
      if (detailed_name)
        g_ptr_array_add (result, detailed_name);

      g_object_unref (shortcut);
    }

  g_ptr_array_add (result, NULL);
  return (char **) g_ptr_array_free (result, FALSE);
}

char **
gtk_application_accels_list_action_descriptions (GtkApplicationAccels *accels)
{
  GPtrArray *result;
  guint i;

  result = g_ptr_array_new ();

  for (i = 0; i < g_list_model_get_n_items (accels->shortcuts); i++)
    {
      GtkShortcut *shortcut = g_list_model_get_item (accels->shortcuts, i);
      char *detailed_name;

      detailed_name = get_detailed_name_for_shortcut (shortcut);
      if (detailed_name)
        g_ptr_array_add (result, detailed_name);

      g_object_unref (shortcut);
    }

  g_ptr_array_add (result, NULL);
  return (char **) g_ptr_array_free (result, FALSE);
}

GListModel *
gtk_application_accels_get_shortcuts (GtkApplicationAccels *accels)
{
  return accels->shortcuts;
}
