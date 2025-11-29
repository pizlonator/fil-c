/*
 * Copyright © 2011 Canonical Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionmuxerprivate.h"

#include "gtkactionobservableprivate.h"
#include "gtkactionobserverprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkmarshalers.h"
#include "gtkwidgetprivate.h"
#include "gsettings-mapping.h"
#include "gtkdebug.h"
#include "gtkprivate.h"

#include <string.h>

typedef struct
{
  char *action_and_target;
  char *accel;
} GtkAccel;

static void
gtk_accel_clear (GtkAccel *accel)
{
  g_free (accel->action_and_target);
  g_free (accel->accel);
}

#define GDK_ARRAY_NAME gtk_accels
#define GDK_ARRAY_TYPE_NAME GtkAccels
#define GDK_ARRAY_ELEMENT_TYPE GtkAccel
#define GDK_ARRAY_FREE_FUNC gtk_accel_clear
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 2
#include "gdk/gdkarrayimpl.c"

static guint
gtk_accels_find (GtkAccels  *accels,
                 const char *action_and_target)
{
  guint i;

  for (i = 0; i < gtk_accels_get_size (accels); i++)
    {
      GtkAccel *accel = gtk_accels_index (accels, i);
      if (strcmp (accel->action_and_target, action_and_target) == 0)
        return i;
    }

  return G_MAXUINT;
}

static void
gtk_accels_replace (GtkAccels  *accels,
                    const char *action_and_target,
                    const char *primary_accel)
{
  guint position;

  position = gtk_accels_find (accels, action_and_target);
  if (position < gtk_accels_get_size (accels))
    {
      GtkAccel *accel = gtk_accels_index (accels, position);
      g_free (accel->accel);
      accel->accel = g_strdup (primary_accel);
    }
  else
    {
      GtkAccel accel;

      accel.action_and_target = g_strdup (action_and_target);
      accel.accel = g_strdup (primary_accel);
      gtk_accels_append (accels, &accel);
    }
}

static void
gtk_accels_remove (GtkAccels  *accels,
                   const char *action_and_target)
{
  guint position;

  position = gtk_accels_find (accels, action_and_target);
  if (position < gtk_accels_get_size (accels))
    gtk_accels_splice (accels, position, 1, FALSE, NULL, 0);
}

/*< private >
 * GtkActionMuxer:
 *
 * GtkActionMuxer aggregates and monitors actions from multiple sources.
 *
 * `GtkActionMuxer` is `GtkActionObservable` and `GtkActionObserver` that
 * offers a `GActionGroup`-like api and is capable of containing other
 * `GActionGroup` instances. `GtkActionMuxer` does not implement the
 * `GActionGroup` interface because it requires excessive signal emissions
 * and has poor scalability. We use the `GtkActionObserver` machinery
 * instead to propagate changes between action muxer instances and
 * to other users.
 *
 * Beyond action groups, `GtkActionMuxer` can incorporate actions that
 * are associated with widget classes (*class actions*) and actions
 * that are associated with the parent widget, allowing for recursive
 * lookup.
 *
 * In addition to the action attributes provided by `GActionGroup`,
 * `GtkActionMuxer` maintains a *primary accelerator* string for
 * actions that can be shown in menuitems.
 *
 * The typical use is aggregating all of the actions applicable to a
 * particular context into a single action group, with namespacing.
 *
 * Consider the case of two action groups -- one containing actions
 * applicable to an entire application (such as “quit”) and one
 * containing actions applicable to a particular window in the
 * application (such as “fullscreen”).
 *
 * In this case, each of these action groups could be added to a
 * `GtkActionMuxer` with the prefixes “app” and “win”, respectively.
 * This would expose the actions as “app.quit” and “win.fullscreen”
 * on the `GActionGroup`-like interface presented by the `GtkActionMuxer`.
 *
 * Activations and state change requests on the `GtkActionMuxer` are
 * wired through to the underlying actions in the expected way.
 *
 * This class is typically only used at the site of “consumption” of
 * actions (eg: when displaying a menu that contains many actions on
 * different objects).
 */

static void     gtk_action_muxer_observable_iface_init    (GtkActionObservableInterface *iface);
static void     gtk_action_muxer_observer_iface_init      (GtkActionObserverInterface *iface);

typedef GObjectClass GtkActionMuxerClass;

struct _GtkActionMuxer
{
  GObject parent_instance;
  GtkActionMuxer *parent;
  GtkWidget *widget;

  GHashTable *observed_actions;
  GHashTable *groups;
  GtkAccels primary_accels;

  GtkBitmask *widget_actions_disabled;
};

G_DEFINE_TYPE_WITH_CODE (GtkActionMuxer, gtk_action_muxer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVER, gtk_action_muxer_observer_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ACTION_OBSERVABLE, gtk_action_muxer_observable_iface_init))

enum
{
  PROP_0,
  PROP_PARENT,
  PROP_WIDGET,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

typedef struct
{
  GtkActionMuxer *muxer;
  GSList       *watchers;
  char         *fullname;
} Action;

typedef struct
{
  GtkActionMuxer *muxer;
  GActionGroup *group;
  char         *prefix;
  gulong        handler_ids[4];
} Group;

static inline guint
get_action_position (GtkWidgetAction *action)
{
  guint slot;
  /* We use the length of @action to the end of the chain as the slot so that
   * we have stable positions for any class or it's subclasses. Doing so helps
   * us avoid having mutable arrays in the class data as we will not have
   * access to the ClassPrivate data during instance _init() functions.
   */
  for (slot = 0; action->next != NULL; slot++, action = action->next) {}
  return slot;
}

static void
gtk_action_muxer_append_group_actions (const char *prefix,
                                       Group      *group,
                                       GHashTable *actions)
{
  char **group_actions;
  char **action;

  group_actions = g_action_group_list_actions (group->group);
  for (action = group_actions; *action; action++)
    {
      char *name = g_strconcat (prefix, ".", *action, NULL);
      g_hash_table_add (actions, name);
    }

  g_strfreev (group_actions);
}

char **
gtk_action_muxer_list_actions (GtkActionMuxer *muxer,
                               gboolean        local_only)
{
  GHashTable *actions;
  char **keys;

  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  actions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  for ( ; muxer != NULL; muxer = muxer->parent)
    {
      GHashTableIter iter;
      const char *prefix;
      Group *group;

      if (muxer->widget)
        {
          GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
          GtkWidgetClassPrivate *priv = klass->priv;
          GtkWidgetAction *action;

          for (action = priv->actions; action; action = action->next)
            g_hash_table_add (actions, g_strdup (action->name));
        }

      if (muxer->groups)
        {
          g_hash_table_iter_init (&iter, muxer->groups);
          while (g_hash_table_iter_next (&iter, (gpointer *)&prefix, (gpointer *)&group))
            gtk_action_muxer_append_group_actions (prefix, group, actions);
        }

      if (local_only)
        break;
    }

  keys = (char **)g_hash_table_get_keys_as_array (actions, NULL);

  g_hash_table_steal_all (actions);
  g_hash_table_unref (actions);

  return (char **)keys;
}

static Group *
gtk_action_muxer_find_group (GtkActionMuxer  *muxer,
                             const char      *full_name,
                             const char     **action_name)
{
  const char *dot;
  char *prefix;
  const char *name;
  Group *group;

  if (!muxer->groups)
    return NULL;

  dot = strchr (full_name, '.');

  if (!dot)
    return NULL;

  name = dot + 1;

  prefix = g_strndup (full_name, dot - full_name);
  group = g_hash_table_lookup (muxer->groups, prefix);
  g_free (prefix);

  if (action_name)
    *action_name = name;

  if (group &&
      g_action_group_has_action (group->group, name))
    return group;

  return NULL;
}

GActionGroup *
gtk_action_muxer_find (GtkActionMuxer  *muxer,
                       const char      *action_name,
                       const char     **unprefixed_name)
{
  Group *group;

  group = gtk_action_muxer_find_group (muxer, action_name, unprefixed_name);
  if (group)
    return group->group;

  return NULL;
}

GActionGroup *
gtk_action_muxer_get_group (GtkActionMuxer *muxer,
                            const char     *group_name)
{
  Group *group;

  if (!muxer->groups)
    return NULL;

  group = g_hash_table_lookup (muxer->groups, group_name);
  if (group)
    return group->group;

  return NULL;
}

static inline Action *
find_observers (GtkActionMuxer *muxer,
                const char     *action_name)
{
  if (muxer->observed_actions)
    return g_hash_table_lookup (muxer->observed_actions, action_name);

  return NULL;
}

void
gtk_action_muxer_action_enabled_changed (GtkActionMuxer *muxer,
                                         const char     *action_name,
                                         gboolean        enabled)
{
  GtkWidgetAction *iter;
  Action *action;
  GSList *node;

  if (muxer->widget)
    {
      GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
      GtkWidgetClassPrivate *priv = klass->priv;

      for (iter = priv->actions; iter; iter = iter->next)
        {
          if (strcmp (action_name, iter->name) == 0)
            {
              guint position = get_action_position (iter);
              muxer->widget_actions_disabled =
                _gtk_bitmask_set (muxer->widget_actions_disabled, position, !enabled);
              break;
            }
        }
    }

  action = find_observers (muxer, action_name);

  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_enabled_changed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name, enabled);
}

static void
gtk_action_muxer_group_action_enabled_changed (GActionGroup *action_group,
                                               const char   *action_name,
                                               gboolean      enabled,
                                               gpointer      user_data)
{
  Group *group = user_data;
  char *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_enabled_changed (group->muxer, fullname, enabled);
  g_free (fullname);
}

void
gtk_action_muxer_action_state_changed (GtkActionMuxer *muxer,
                                       const char     *action_name,
                                       GVariant       *state)
{
  Action *action;
  GSList *node;

  action = find_observers (muxer, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_state_changed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name, state);
}

static void
gtk_action_muxer_group_action_state_changed (GActionGroup *action_group,
                                             const char   *action_name,
                                             GVariant     *state,
                                             gpointer      user_data)
{
  Group *group = user_data;
  char *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_state_changed (group->muxer, fullname, state);
  g_free (fullname);
}

static gboolean action_muxer_query_action (GtkActionMuxer      *muxer,
                                           const char          *action_name,
                                           gboolean            *enabled,
                                           const GVariantType **parameter_type,
                                           const GVariantType **state_type,
                                           GVariant           **state_hint,
                                           GVariant           **state,
                                           gboolean             recurse);

static void
notify_observers_added (GtkActionMuxer *muxer,
                        GtkActionMuxer *parent)
{
  GHashTableIter iter;
  const char *action_name;
  Action *action;

  if (!muxer->observed_actions)
    return;

  g_hash_table_iter_init (&iter, muxer->observed_actions);
  while (g_hash_table_iter_next (&iter, (gpointer *)&action_name, (gpointer *)&action))
    {
      const GVariantType *parameter_type;
      gboolean enabled;
      GVariant *state;
      GSList *node;
      GSList *action_watchers;

      action_watchers = action ? action->watchers : NULL;

      if (!action_watchers)
        continue;

      for (node = action_watchers; node; node = node->next)
        gtk_action_observer_primary_accel_changed (node->data, GTK_ACTION_OBSERVABLE (muxer),
                                                   action_name, NULL);

      gtk_action_observable_register_observer (GTK_ACTION_OBSERVABLE (parent), action_name, GTK_ACTION_OBSERVER (muxer));

      if (!action_muxer_query_action (muxer, action_name,
                                      &enabled, &parameter_type,
                                      NULL, NULL, &state,
                                      TRUE))
        continue;

      for (node = action_watchers; node; node = node->next)
        {
          gtk_action_observer_action_added (node->data,
                                            GTK_ACTION_OBSERVABLE (muxer),
                                            action_name, parameter_type, enabled, state);
        }

      if (state)
        g_variant_unref (state);
    }
}

static void
notify_observers_removed (GtkActionMuxer *muxer,
                          GtkActionMuxer *parent)
{
  GHashTableIter iter;
  const char *action_name;
  Action *action;

  if (!muxer->observed_actions)
    return;

  g_hash_table_iter_init (&iter, muxer->observed_actions);
  while (g_hash_table_iter_next (&iter, (gpointer *)&action_name, (gpointer *)&action))
    {
      GSList *node;

      gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (parent), action_name, GTK_ACTION_OBSERVER (muxer));

      for (node = action->watchers; node; node = node->next)
        {
          gtk_action_observer_action_removed (node->data,
                                              GTK_ACTION_OBSERVABLE (muxer),
                                              action_name);
        }
    }
}

static void
gtk_action_muxer_action_added (GtkActionMuxer     *muxer,
                               const char         *action_name,
                               const GVariantType *parameter_type,
                               gboolean            enabled,
                               GVariant           *state)
{
  Action *action;
  GSList *node;

  action = find_observers (muxer, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_added (node->data,
                                      GTK_ACTION_OBSERVABLE (muxer),
                                      action_name, parameter_type, enabled, state);
}

static void
gtk_action_muxer_action_added_to_group (GActionGroup *action_group,
                                        const char   *action_name,
                                        gpointer      user_data)
{
  Group *group = user_data;
  GtkActionMuxer *muxer = group->muxer;
  Action *action;
  const GVariantType *parameter_type;
  gboolean enabled;
  GVariant *state;
  char *fullname;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);

   if (muxer->parent)
     gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                fullname,
                                                GTK_ACTION_OBSERVER (muxer));

  action = find_observers (muxer, fullname);

  if (action && action->watchers &&
      g_action_group_query_action (action_group, action_name,
                                   &enabled, &parameter_type, NULL, NULL, &state))
    {
      gtk_action_muxer_action_added (muxer, fullname, parameter_type, enabled, state);

      if (state)
        g_variant_unref (state);
    }

  g_free (fullname);
}

static void
gtk_action_muxer_action_removed (GtkActionMuxer *muxer,
                                 const char     *action_name)
{
  Action *action;
  GSList *node;

  action = find_observers (muxer, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_action_removed (node->data, GTK_ACTION_OBSERVABLE (muxer), action_name);
}

static void
gtk_action_muxer_action_removed_from_group (GActionGroup *action_group,
                                            const char   *action_name,
                                            gpointer      user_data)
{
  Group *group = user_data;
  GtkActionMuxer *muxer = group->muxer;
  char *fullname;
  Action *action;

  fullname = g_strconcat (group->prefix, ".", action_name, NULL);
  gtk_action_muxer_action_removed (muxer, fullname);
  g_free (fullname);

  action = find_observers (muxer, action_name);

  if (action && action->watchers &&
      !action_muxer_query_action (muxer, action_name,
                                  NULL, NULL, NULL, NULL, NULL, FALSE))
    {
      if (muxer->parent)
         gtk_action_observable_register_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                  action_name,
                                                  GTK_ACTION_OBSERVER (muxer));
    }
}

static void
gtk_action_muxer_primary_accel_changed (GtkActionMuxer *muxer,
                                        const char     *action_name,
                                        const char     *action_and_target)
{
  Action *action;
  GSList *node;

  if (!action_name)
    action_name = strrchr (action_and_target, '|') + 1;

  action = find_observers (muxer, action_name);
  for (node = action ? action->watchers : NULL; node; node = node->next)
    gtk_action_observer_primary_accel_changed (node->data, GTK_ACTION_OBSERVABLE (muxer),
                                               action_name, action_and_target);
}

static GVariant *
prop_action_get_state (GtkWidget       *widget,
                       GtkWidgetAction *action)
{
  GValue value = G_VALUE_INIT;
  GVariant *result;

  g_value_init (&value, action->pspec->value_type);
  g_object_get_property (G_OBJECT (widget), action->pspec->name, &value);

  result = g_settings_set_mapping (&value, action->state_type, NULL);
  g_value_unset (&value);

  return g_variant_ref_sink (result);
}

static GVariant *
prop_action_get_state_hint (GtkWidget       *widget,
                            GtkWidgetAction *action)
{
  if (action->pspec->value_type == G_TYPE_INT)
    {
      GParamSpecInt *pspec = (GParamSpecInt *)action->pspec;
      return g_variant_new ("(ii)", pspec->minimum, pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_UINT)
    {
      GParamSpecUInt *pspec = (GParamSpecUInt *)action->pspec;
      return g_variant_new ("(uu)", pspec->minimum, pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_FLOAT)
    {
      GParamSpecFloat *pspec = (GParamSpecFloat *)action->pspec;
      return g_variant_new ("(dd)", (double)pspec->minimum, (double)pspec->maximum);
    }
  else if (action->pspec->value_type == G_TYPE_DOUBLE)
    {
      GParamSpecDouble *pspec = (GParamSpecDouble *)action->pspec;
      return g_variant_new ("(dd)", pspec->minimum, pspec->maximum);
    }

  return NULL;
}

static void
prop_action_set_state (GtkWidget       *widget,
                       GtkWidgetAction *action,
                       GVariant        *state)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, action->pspec->value_type);
  g_settings_get_mapping (&value, state, NULL);

  g_object_set_property (G_OBJECT (widget), action->pspec->name, &value);
  g_value_unset (&value);
}

static void
prop_action_activate (GtkWidget       *widget,
                      GtkWidgetAction *action,
                      GVariant        *parameter)
{
  if (action->pspec->value_type == G_TYPE_BOOLEAN)
    {
      gboolean value;

      g_return_if_fail (parameter == NULL);

      g_object_get (G_OBJECT (widget), action->pspec->name, &value, NULL);
      value = !value;
      g_object_set (G_OBJECT (widget), action->pspec->name, value, NULL);
    }
  else
    {
      g_return_if_fail (parameter != NULL && g_variant_is_of_type (parameter, action->state_type));

      prop_action_set_state (widget, action, parameter);
    }
}

static void
prop_action_notify (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  GtkWidget *widget = GTK_WIDGET (object);
  GtkWidgetAction *action = user_data;
  GtkActionMuxer *muxer = _gtk_widget_get_action_muxer (widget, TRUE);
  GVariant *state;

  g_assert (muxer->widget == widget);
  g_assert (action->pspec == pspec);

  state = prop_action_get_state (widget, action);
  gtk_action_muxer_action_state_changed (muxer, action->name, state);
  g_variant_unref (state);
}

static void
prop_actions_connect (GtkActionMuxer *muxer)
{
  GtkWidgetClassPrivate *priv;
  GtkWidgetAction *action;
  GtkWidgetClass *klass;
  guint signal_id;

  if (!muxer->widget)
    return;

  klass = GTK_WIDGET_GET_CLASS (muxer->widget);
  priv = klass->priv;
  if (!priv->actions)
    return;

  signal_id = g_signal_lookup ("notify", G_TYPE_OBJECT);

  for (action = priv->actions; action; action = action->next)
    {
      if (!action->pspec)
        continue;

      g_signal_connect_closure_by_id (muxer->widget,
                                      signal_id,
                                      g_param_spec_get_name_quark (action->pspec),
                                      g_cclosure_new (G_CALLBACK (prop_action_notify),
                                                                  action,
                                                                  NULL),
                                      FALSE);
    }
}

static gboolean
action_muxer_query_action (GtkActionMuxer      *muxer,
                           const char          *action_name,
                           gboolean            *enabled,
                           const GVariantType **parameter_type,
                           const GVariantType **state_type,
                           GVariant           **state_hint,
                           GVariant           **state,
                           gboolean             recurse)
{
  GtkWidgetAction *action;
  Group *group;
  const char *unprefixed_name;

  if (muxer->widget)
    {
      GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
      GtkWidgetClassPrivate *priv = klass->priv;

      for (action = priv->actions; action; action = action->next)
        {
          guint position;

          if (strcmp (action->name, action_name) != 0)
            continue;

          position = get_action_position (action);

          if (enabled)
            *enabled = !_gtk_bitmask_get (muxer->widget_actions_disabled, position);
          if (parameter_type)
            *parameter_type = action->parameter_type;
          if (state_type)
            *state_type = action->state_type;

          if (state_hint)
            *state_hint = NULL;
          if (state)
            *state = NULL;

          if (action->pspec)
            {
              if (state)
                *state = prop_action_get_state (muxer->widget, action);
              if (state_hint)
                *state_hint = prop_action_get_state_hint (muxer->widget, action);
            }

          return TRUE;
        }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    return g_action_group_query_action (group->group, unprefixed_name, enabled,
                                        parameter_type, state_type, state_hint, state);

  if (muxer->parent && recurse)
    return gtk_action_muxer_query_action (muxer->parent, action_name,
                                          enabled, parameter_type,
                                          state_type, state_hint, state);

  return FALSE;
}

gboolean
gtk_action_muxer_query_action (GtkActionMuxer      *muxer,
                               const char          *action_name,
                               gboolean            *enabled,
                               const GVariantType **parameter_type,
                               const GVariantType **state_type,
                               GVariant           **state_hint,
                               GVariant           **state)
{
  return action_muxer_query_action (muxer, action_name,
                                    enabled, parameter_type,
                                    state_type, state_hint, state,
                                    TRUE);
}

gboolean
gtk_action_muxer_has_action (GtkActionMuxer *muxer,
                             const char     *action_name)
{
  return action_muxer_query_action (muxer, action_name,
                                    NULL, NULL, NULL, NULL, NULL,
                                    TRUE);
}

void
gtk_action_muxer_activate_action (GtkActionMuxer *muxer,
                                  const char     *action_name,
                                  GVariant       *parameter)
{
  const char *unprefixed_name;
  Group *group;

  if (muxer->widget)
    {
      GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
      GtkWidgetClassPrivate *priv = klass->priv;
      GtkWidgetAction *action;

      for (action = priv->actions; action; action = action->next)
        {
          if (strcmp (action->name, action_name) == 0)
            {
              guint position = get_action_position (action);

              if (!_gtk_bitmask_get (muxer->widget_actions_disabled, position))
                {
                  if (action->activate)
                    {
                      GTK_DEBUG (ACTIONS, "%s: activate action", action->name);
                      action->activate (muxer->widget, action->name, parameter);
                    }
                  else if (action->pspec)
                    {
                      GTK_DEBUG (ACTIONS, "%s: activate prop action", action->pspec->name);
                      prop_action_activate (muxer->widget, action, parameter);
                    }
                }

              return;
            }
        }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_activate_action (group->group, unprefixed_name, parameter);
  else if (muxer->parent)
    gtk_action_muxer_activate_action (muxer->parent, action_name, parameter);
}

void
gtk_action_muxer_change_action_state (GtkActionMuxer *muxer,
                                      const char     *action_name,
                                      GVariant       *state)
{
  GtkWidgetAction *action;
  const char *unprefixed_name;
  Group *group;

  if (muxer->widget)
    {
      GtkWidgetClass *klass = GTK_WIDGET_GET_CLASS (muxer->widget);
      GtkWidgetClassPrivate *priv = klass->priv;

      for (action = priv->actions; action; action = action->next)
        {
          if (strcmp (action->name, action_name) == 0)
            {
              if (action->pspec)
                prop_action_set_state (muxer->widget, action, state);

              return;
            }
        }
    }

  group = gtk_action_muxer_find_group (muxer, action_name, &unprefixed_name);

  if (group)
    g_action_group_change_action_state (group->group, unprefixed_name, state);
  else if (muxer->parent)
    gtk_action_muxer_change_action_state (muxer->parent, action_name, state);
}

static void
gtk_action_muxer_unregister_internal (Action   *action,
                                      gpointer  observer)
{
  GtkActionMuxer *muxer = action->muxer;
  GSList **ptr;

  for (ptr = &action->watchers; *ptr; ptr = &(*ptr)->next)
    if ((*ptr)->data == observer)
      {
        *ptr = g_slist_remove (*ptr, observer);

        if (action->watchers == NULL)
          {
            if (muxer->parent)
              gtk_action_observable_unregister_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                         action->fullname,
                                                         GTK_ACTION_OBSERVER (muxer));
            g_hash_table_remove (muxer->observed_actions, action->fullname);
          }

        break;
      }
}

static void
gtk_action_muxer_weak_notify (gpointer  data,
                              GObject  *where_the_object_was)
{
  Action *action = data;

  gtk_action_muxer_unregister_internal (action, where_the_object_was);
}

static void gtk_action_muxer_free_action (gpointer data);

static void
gtk_action_muxer_register_observer (GtkActionObservable *observable,
                                    const char          *name,
                                    GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action;
  gboolean enabled;
  const GVariantType *parameter_type;
  GVariant *state;
  gboolean is_duplicate;

  if (!muxer->observed_actions)
    muxer->observed_actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_action);

  action = g_hash_table_lookup (muxer->observed_actions, name);

  if (action == NULL)
    {
      action = g_new (Action, 1);
      action->muxer = muxer;
      action->fullname = g_strdup (name);
      action->watchers = NULL;

      g_hash_table_insert (muxer->observed_actions, action->fullname, action);
    }

  is_duplicate = g_slist_find (action->watchers, observer) != NULL;
  action->watchers = g_slist_prepend (action->watchers, observer);
  g_object_weak_ref (G_OBJECT (observer), gtk_action_muxer_weak_notify, action);

  if (!is_duplicate)
    {
      if (action_muxer_query_action (muxer, name,
                                     &enabled, &parameter_type,
                                     NULL, NULL, &state, TRUE))
        {
          gtk_action_muxer_action_added (muxer, name, parameter_type, enabled, state);
          g_clear_pointer (&state, g_variant_unref);
        }

      if (muxer->parent)
        {
          gtk_action_observable_register_observer (GTK_ACTION_OBSERVABLE (muxer->parent),
                                                   name,
                                                   GTK_ACTION_OBSERVER (muxer));
        }
    }
}

static void
gtk_action_muxer_unregister_observer (GtkActionObservable *observable,
                                      const char          *name,
                                      GtkActionObserver   *observer)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (observable);
  Action *action = find_observers (muxer, name);

  if (action)
    {
      if (g_slist_find (action->watchers, observer) != NULL)
        {
          g_object_weak_unref (G_OBJECT (observer), gtk_action_muxer_weak_notify, action);
          gtk_action_muxer_unregister_internal (action, observer);
        }
    }
}

static void
gtk_action_muxer_free_group (gpointer data)
{
  Group *group = data;
  int i;

  /* 'for loop' or 'four loop'? */
  for (i = 0; i < 4; i++)
    g_signal_handler_disconnect (group->group, group->handler_ids[i]);

  g_object_unref (group->group);
  g_free (group->prefix);

  g_free (group);
}

static void
gtk_action_muxer_free_action (gpointer data)
{
  Action *action = data;
  GSList *it;

  for (it = action->watchers; it; it = it->next)
    g_object_weak_unref (G_OBJECT (it->data), gtk_action_muxer_weak_notify, action);

  g_slist_free (action->watchers);
  g_free (action->fullname);

  g_free (action);
}

static void
gtk_action_muxer_finalize (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  if (muxer->observed_actions)
    {
      g_assert_cmpint (g_hash_table_size (muxer->observed_actions), ==, 0);
      g_hash_table_unref (muxer->observed_actions);
    }
  if (muxer->groups)
    g_hash_table_unref (muxer->groups);

  gtk_accels_clear (&muxer->primary_accels);

  _gtk_bitmask_free (muxer->widget_actions_disabled);

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)->finalize (object);
}

static void
gtk_action_muxer_dispose (GObject *object)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  g_clear_object (&muxer->parent);
  if (muxer->observed_actions)
    g_hash_table_remove_all (muxer->observed_actions);

  muxer->widget = NULL;

  G_OBJECT_CLASS (gtk_action_muxer_parent_class)->dispose (object);
}

void
gtk_action_muxer_connect_class_actions (GtkActionMuxer *muxer)
{
  prop_actions_connect (muxer);
}

static void
gtk_action_muxer_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      g_value_set_object (value, gtk_action_muxer_get_parent (muxer));
      break;

    case PROP_WIDGET:
      g_value_set_object (value, muxer->widget);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_action_muxer_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkActionMuxer *muxer = GTK_ACTION_MUXER (object);

  switch (property_id)
    {
    case PROP_PARENT:
      gtk_action_muxer_set_parent (muxer, g_value_get_object (value));
      break;

    case PROP_WIDGET:
      muxer->widget = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
gtk_action_muxer_init (GtkActionMuxer *muxer)
{
  muxer->widget_actions_disabled = _gtk_bitmask_new ();
}

static void
gtk_action_muxer_observable_iface_init (GtkActionObservableInterface *iface)
{
  iface->register_observer = gtk_action_muxer_register_observer;
  iface->unregister_observer = gtk_action_muxer_unregister_observer;
}


static void
gtk_action_muxer_observer_action_added (GtkActionObserver    *observer,
                                        GtkActionObservable  *observable,
                                        const char           *action_name,
                                        const GVariantType   *parameter_type,
                                        gboolean              enabled,
                                        GVariant             *state)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_added (GTK_ACTION_MUXER (observer),
                                 action_name,
                                 parameter_type,
                                 enabled,
                                 state);
}

static void
gtk_action_muxer_observer_action_removed (GtkActionObserver   *observer,
                                          GtkActionObservable *observable,
                                          const char          *action_name)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_removed (GTK_ACTION_MUXER (observer), action_name);
}

static void
gtk_action_muxer_observer_action_enabled_changed (GtkActionObserver   *observer,
                                                  GtkActionObservable *observable,
                                                  const char          *action_name,
                                                  gboolean             enabled)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_enabled_changed (GTK_ACTION_MUXER (observer), action_name, enabled);
}

static void
gtk_action_muxer_observer_action_state_changed (GtkActionObserver   *observer,
                                                GtkActionObservable *observable,
                                                const char          *action_name,
                                                GVariant            *state)
{
  if (action_muxer_query_action (GTK_ACTION_MUXER (observer), action_name,
                                 NULL, NULL, NULL, NULL, NULL, FALSE))
    return;

  gtk_action_muxer_action_state_changed (GTK_ACTION_MUXER (observer), action_name, state);
}

static void
gtk_action_muxer_observer_primary_accel_changed (GtkActionObserver   *observer,
                                                 GtkActionObservable *observable,
                                                 const char          *action_name,
                                                 const char          *action_and_target)
{
  gtk_action_muxer_primary_accel_changed (GTK_ACTION_MUXER (observer),
                                          action_name,
                                          action_and_target);
}

static void
gtk_action_muxer_observer_iface_init (GtkActionObserverInterface *iface)
{
  iface->action_added = gtk_action_muxer_observer_action_added;
  iface->action_removed = gtk_action_muxer_observer_action_removed;
  iface->action_enabled_changed = gtk_action_muxer_observer_action_enabled_changed;
  iface->action_state_changed = gtk_action_muxer_observer_action_state_changed;
  iface->primary_accel_changed = gtk_action_muxer_observer_primary_accel_changed;
}

static void
gtk_action_muxer_class_init (GObjectClass *class)
{
  class->get_property = gtk_action_muxer_get_property;
  class->set_property = gtk_action_muxer_set_property;
  class->finalize = gtk_action_muxer_finalize;
  class->dispose = gtk_action_muxer_dispose;

  properties[PROP_PARENT] = g_param_spec_object ("parent", NULL, NULL,
                                                 GTK_TYPE_ACTION_MUXER,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_WIDGET] = g_param_spec_object ("widget", NULL, NULL,
                                                 GTK_TYPE_WIDGET,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT_ONLY |
                                                 G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (class, NUM_PROPERTIES, properties);
}

/*< private >
 * gtk_action_muxer_insert:
 * @muxer: a `GtkActionMuxer`
 * @prefix: the prefix string for the action group
 * @action_group: a `GActionGroup`
 *
 * Adds the actions in @action_group to the list of actions provided by
 * @muxer.  @prefix is prefixed to each action name, such that for each
 * action `x` in @action_group, there is an equivalent
 * action @prefix`.x` in @muxer.
 *
 * For example, if @prefix is “`app`” and @action_group
 * contains an action called “`quit`”, then @muxer will
 * now contain an action called “`app.quit`”.
 *
 * If any `GtkActionObserver`s are registered for actions in the group,
 * “action_added” notifications will be emitted, as appropriate.
 *
 * @prefix must not contain a dot ('.').
 */
void
gtk_action_muxer_insert (GtkActionMuxer *muxer,
                         const char     *prefix,
                         GActionGroup   *action_group)
{
  char **actions;
  Group *group;
  int i;

  /* TODO: diff instead of ripout and replace */
  gtk_action_muxer_remove (muxer, prefix);

  if (!muxer->groups)
    muxer->groups = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, gtk_action_muxer_free_group);

  group = g_new0 (Group, 1);
  group->muxer = muxer;
  group->group = g_object_ref (action_group);
  group->prefix = g_strdup (prefix);

  g_hash_table_insert (muxer->groups, group->prefix, group);

  actions = g_action_group_list_actions (group->group);
  for (i = 0; actions[i]; i++)
    gtk_action_muxer_action_added_to_group (group->group, actions[i], group);
  g_strfreev (actions);

  group->handler_ids[0] = g_signal_connect (group->group, "action-added",
                                            G_CALLBACK (gtk_action_muxer_action_added_to_group), group);
  group->handler_ids[1] = g_signal_connect (group->group, "action-removed",
                                            G_CALLBACK (gtk_action_muxer_action_removed_from_group), group);
  group->handler_ids[2] = g_signal_connect (group->group, "action-enabled-changed",
                                            G_CALLBACK (gtk_action_muxer_group_action_enabled_changed), group);
  group->handler_ids[3] = g_signal_connect (group->group, "action-state-changed",
                                            G_CALLBACK (gtk_action_muxer_group_action_state_changed), group);
}

/*< private >
 * gtk_action_muxer_remove:
 * @muxer: a `GtkActionMuxer`
 * @prefix: the prefix of the action group to remove
 *
 * Removes a `GActionGroup` from the `GtkActionMuxer`.
 *
 * If any `GtkActionObserver`s are registered for actions in the group,
 * “action_removed” notifications will be emitted, as appropriate.
 */
void
gtk_action_muxer_remove (GtkActionMuxer *muxer,
                         const char     *prefix)
{
  Group *group;

  if (!muxer->groups)
    return;

  group = g_hash_table_lookup (muxer->groups, prefix);

  if (group != NULL)
    {
      char **actions;
      int i;

      g_hash_table_steal (muxer->groups, prefix);

      actions = g_action_group_list_actions (group->group);
      for (i = 0; actions[i]; i++)
        gtk_action_muxer_action_removed_from_group (group->group, actions[i], group);
      g_strfreev (actions);

      gtk_action_muxer_free_group (group);
    }
}

/*< private >
 * gtk_action_muxer_new:
 * @widget: the widget to which the muxer belongs
 *
 * Creates a new `GtkActionMuxer`.
 */
GtkActionMuxer *
gtk_action_muxer_new (GtkWidget *widget)
{
  return g_object_new (GTK_TYPE_ACTION_MUXER,
                       "widget", widget,
                       NULL);
}

/*< private >
 * gtk_action_muxer_get_parent:
 * @muxer: a `GtkActionMuxer`
 *
 * Returns: (transfer none): the parent of @muxer, or NULL.
 */
GtkActionMuxer *
gtk_action_muxer_get_parent (GtkActionMuxer *muxer)
{
  g_return_val_if_fail (GTK_IS_ACTION_MUXER (muxer), NULL);

  return muxer->parent;
}

/*< private >
 * gtk_action_muxer_set_parent:
 * @muxer: a `GtkActionMuxer`
 * @parent: (nullable): the new parent `GtkActionMuxer`
 *
 * Sets the parent of @muxer to @parent.
 */
void
gtk_action_muxer_set_parent (GtkActionMuxer *muxer,
                             GtkActionMuxer *parent)
{
  g_return_if_fail (GTK_IS_ACTION_MUXER (muxer));
  g_return_if_fail (parent == NULL || GTK_IS_ACTION_MUXER (parent));

  if (muxer->parent == parent)
    return;

  if (muxer->parent != NULL)
    {
      notify_observers_removed (muxer, muxer->parent);
      g_object_unref (muxer->parent);
    }

  muxer->parent = parent;

  if (muxer->parent != NULL)
    {
      g_object_ref (muxer->parent);
      notify_observers_added (muxer, muxer->parent);
    }

  g_object_notify_by_pspec (G_OBJECT (muxer), properties[PROP_PARENT]);
}

void
gtk_action_muxer_set_primary_accel (GtkActionMuxer *muxer,
                                    const char     *action_and_target,
                                    const char     *primary_accel)
{
  if (primary_accel)
    gtk_accels_replace (&muxer->primary_accels, action_and_target, primary_accel);
  else
    gtk_accels_remove (&muxer->primary_accels, action_and_target);

  gtk_action_muxer_primary_accel_changed (muxer, NULL, action_and_target);
}

const char *
gtk_action_muxer_get_primary_accel (GtkActionMuxer *muxer,
                                    const char     *action_and_target)
{
   guint position;

   position = gtk_accels_find (&muxer->primary_accels, action_and_target);
   if (position < G_MAXUINT)
     return gtk_accels_index (&muxer->primary_accels, position)->accel;

  if (!muxer->parent)
    return NULL;

  return gtk_action_muxer_get_primary_accel (muxer->parent, action_and_target);
}

char *
gtk_print_action_and_target (const char *action_namespace,
                             const char *action_name,
                             GVariant    *target)
{
  GString *result;

  g_return_val_if_fail (strchr (action_name, '|') == NULL, NULL);
  g_return_val_if_fail (action_namespace == NULL || strchr (action_namespace, '|') == NULL, NULL);

  result = g_string_new (NULL);

  if (target)
    g_variant_print_string (target, result, TRUE);
  g_string_append_c (result, '|');

  if (action_namespace)
    {
      g_string_append (result, action_namespace);
      g_string_append_c (result, '.');
    }

  g_string_append (result, action_name);

  return g_string_free (result, FALSE);
}

char *
gtk_normalise_detailed_action_name (const char *detailed_action_name)
{
  GError *error = NULL;
  char *action_and_target;
  char *action_name;
  GVariant *target;

  g_action_parse_detailed_name (detailed_action_name, &action_name, &target, &error);
  g_assert_no_error (error);

  action_and_target = gtk_print_action_and_target (NULL, action_name, target);

  if (target)
    g_variant_unref (target);

  g_free (action_name);

  return action_and_target;
}

