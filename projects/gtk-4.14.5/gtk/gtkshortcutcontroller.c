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


/**
 * GtkShortcutController:
 *
 * `GtkShortcutController` is an event controller that manages shortcuts.
 *
 * Most common shortcuts are using this controller implicitly, e.g. by
 * adding a mnemonic underline to a [class@Gtk.Label], or by installing a key
 * binding using [method@Gtk.WidgetClass.add_binding], or by adding accelerators
 * to global actions using [method@Gtk.Application.set_accels_for_action].
 *
 * But it is possible to create your own shortcut controller, and add
 * shortcuts to it.
 *
 * `GtkShortcutController` implements [iface@Gio.ListModel] for querying the
 * shortcuts that have been added to it.
 *
 * # GtkShortcutController as GtkBuildable
 *
 * `GtkShortcutController`s can be created in [class@Gtk.Builder] ui files, to set up
 * shortcuts in the same place as the widgets.
 *
 * An example of a UI definition fragment with `GtkShortcutController`:
 * ```xml
 *   <object class='GtkButton'>
 *     <child>
 *       <object class='GtkShortcutController'>
 *         <property name='scope'>managed</property>
 *         <child>
 *           <object class='GtkShortcut'>
 *             <property name='trigger'>&lt;Control&gt;k</property>
 *             <property name='action'>activate</property>
 *           </object>
 *         </child>
 *       </object>
 *     </child>
 *   </object>
 * ```
 *
 * This example creates a [class@Gtk.ActivateAction] for triggering the
 * `activate` signal of the [class@Gtk.Button]. See [ctor@Gtk.ShortcutAction.parse_string]
 * for the syntax for other kinds of [class@Gtk.ShortcutAction]. See
 * [ctor@Gtk.ShortcutTrigger.parse_string] to learn more about the syntax
 * for triggers.
 */

#include "config.h"

#include "gtkshortcutcontrollerprivate.h"

#include "gtkflattenlistmodel.h"
#include "gtkbuildable.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkshortcut.h"
#include "gtkshortcutmanager.h"
#include "gtkshortcuttrigger.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"
#include "gtknative.h"
#include "gtkdebug.h"
#include "gtkmodelbuttonprivate.h"

#include <gdk/gdk.h>

struct _GtkShortcutController
{
  GtkEventController parent_instance;

  GListModel *shortcuts;
  GtkShortcutScope scope;
  GdkModifierType mnemonics_modifiers;

  gulong shortcuts_changed_id;
  guint custom_shortcuts : 1;

  guint last_activated;
};

struct _GtkShortcutControllerClass
{
  GtkEventControllerClass parent_class;
};

enum {
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MNEMONICS_MODIFIERS,
  PROP_MODEL,
  PROP_N_ITEMS,
  PROP_SCOPE,

  N_PROPS
};

static GParamSpec *properties[N_PROPS] = { NULL, };

static GType
gtk_shortcut_controller_list_model_get_item_type (GListModel *list)
{
  return G_TYPE_OBJECT;
}

static guint
gtk_shortcut_controller_list_model_get_n_items (GListModel *list)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (list);

  return g_list_model_get_n_items (self->shortcuts);
}

static gpointer
gtk_shortcut_controller_list_model_get_item (GListModel *list,
                                             guint       position)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (list);

  return g_list_model_get_item (self->shortcuts, position);
}

static void
gtk_shortcut_controller_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_shortcut_controller_list_model_get_item_type;
  iface->get_n_items = gtk_shortcut_controller_list_model_get_n_items;
  iface->get_item = gtk_shortcut_controller_list_model_get_item;
}

static void
gtk_shortcut_controller_buildable_add_child (GtkBuildable  *buildable,
                                             GtkBuilder    *builder,
                                             GObject       *child,
                                             const char    *type)
{
  if (type != NULL)
    {
      GTK_BUILDER_WARN_INVALID_CHILD_TYPE (buildable, type);
    }
  if (GTK_IS_SHORTCUT (child))
    {
      gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (buildable), g_object_ref (GTK_SHORTCUT (child)));
    }
  else
    {
      g_warning ("Cannot add an object of type %s to a controller of type %s",
                 g_type_name (G_OBJECT_TYPE (child)), g_type_name (G_OBJECT_TYPE (buildable)));
    }
}

static void
gtk_shortcut_controller_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_shortcut_controller_buildable_add_child;
}

G_DEFINE_TYPE_WITH_CODE (GtkShortcutController, gtk_shortcut_controller,
                         GTK_TYPE_EVENT_CONTROLLER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_shortcut_controller_list_model_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_shortcut_controller_buildable_init))

static gboolean
gtk_shortcut_controller_is_rooted (GtkShortcutController *self)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));

  if (widget == NULL)
    return FALSE;

  return gtk_widget_get_root (widget) != NULL;
}

static void
gtk_shortcut_controller_items_changed_cb (GListModel            *model,
                                          guint                  position,
                                          guint                  removed,
                                          guint                  added,
                                          GtkShortcutController *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static void
gtk_shortcut_controller_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  switch (prop_id)
    {
    case PROP_MNEMONICS_MODIFIERS:
      gtk_shortcut_controller_set_mnemonics_modifiers (self, g_value_get_flags (value));
      break;

    case PROP_MODEL:
      {
        GListModel *model = g_value_get_object (value);
        if (model == NULL)
          {
            self->shortcuts = G_LIST_MODEL (g_list_store_new (GTK_TYPE_SHORTCUT));
            self->custom_shortcuts = TRUE;
          }
        else
          {
            self->shortcuts = g_object_ref (model);
            self->custom_shortcuts = FALSE;
          }

        self->shortcuts_changed_id =  g_signal_connect (self->shortcuts,
                                                        "items-changed",
                                                        G_CALLBACK (gtk_shortcut_controller_items_changed_cb),
                                                        self);
      }
      break;

    case PROP_SCOPE:
      gtk_shortcut_controller_set_scope (self, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcut_controller_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  switch (prop_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, G_TYPE_OBJECT);
      break;

    case PROP_MNEMONICS_MODIFIERS:
      g_value_set_flags (value, self->mnemonics_modifiers);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, g_list_model_get_n_items (self->shortcuts));
      break;

    case PROP_SCOPE:
      g_value_set_enum (value, self->scope);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_shortcut_controller_dispose (GObject *object)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  if (self->custom_shortcuts)
    g_list_store_remove_all (G_LIST_STORE (self->shortcuts));

  G_OBJECT_CLASS (gtk_shortcut_controller_parent_class)->dispose (object);
}

static void
gtk_shortcut_controller_finalize (GObject *object)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (object);

  g_clear_signal_handler (&self->shortcuts_changed_id, self->shortcuts);
  g_clear_object (&self->shortcuts);

  G_OBJECT_CLASS (gtk_shortcut_controller_parent_class)->finalize (object);
}

typedef struct {
  GtkShortcut *shortcut;
  GtkWidget *widget;
  guint index;
} ShortcutData;

static void
shortcut_data_free (gpointer data)
{
  ShortcutData *sdata = data;

  g_object_unref (sdata->shortcut);
}

static gboolean
gtk_shortcut_controller_run_controllers (GtkEventController *controller,
                                         GdkEvent           *event,
                                         double              x,
                                         double              y,
                                         gboolean            enable_mnemonics)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (controller);
  int i, p;
  GArray *shortcuts = NULL;
  gboolean has_exact = FALSE;
  gboolean retval = FALSE;

  for (i = 0, p = g_list_model_get_n_items (self->shortcuts); i < p; i++)
    {
      GtkShortcut *shortcut;
      ShortcutData *data;
      guint index;
      GtkWidget *widget;
      GtkNative *native;

      /* This is not entirely right, but we only want to do round-robin cycling
       * for mnemonics.
       */
      if (enable_mnemonics)
        index = (self->last_activated + 1 + i) % g_list_model_get_n_items (self->shortcuts);
      else
        index = i;

      shortcut = g_list_model_get_item (self->shortcuts, index);
      if (!GTK_IS_SHORTCUT (shortcut))
        {
          g_object_unref (shortcut);
          continue;
        }

      switch (gtk_shortcut_trigger_trigger (gtk_shortcut_get_trigger (shortcut), event, enable_mnemonics))
        {
        case GDK_KEY_MATCH_PARTIAL:
          if (!has_exact)
            break;
          G_GNUC_FALLTHROUGH;

        case GDK_KEY_MATCH_NONE:
          g_object_unref (shortcut);
          continue;

        case GDK_KEY_MATCH_EXACT:
          if (!has_exact)
            {
              if (shortcuts)
                g_array_set_size (shortcuts, 0);
            }
          has_exact = TRUE;
          break;

        default:
          g_assert_not_reached ();
        }

      widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
      if (!self->custom_shortcuts &&
          GTK_IS_FLATTEN_LIST_MODEL (self->shortcuts))
        {
          GListModel *model = gtk_flatten_list_model_get_model_for_item (GTK_FLATTEN_LIST_MODEL (self->shortcuts), index);
          if (GTK_IS_SHORTCUT_CONTROLLER (model))
            widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (model));
        }

      if (!_gtk_widget_is_sensitive (widget) ||
          !_gtk_widget_get_mapped (widget))
        {
          g_object_unref (shortcut);
          continue;
        }

      native = gtk_widget_get_native (widget);
      if (!native ||
          !gdk_surface_get_mapped (gtk_native_get_surface (native)))
        {
          g_object_unref (shortcut);
          continue;
        }

      if (G_UNLIKELY (!shortcuts))
        {
          shortcuts = g_array_sized_new (FALSE, TRUE, sizeof (ShortcutData), 8);
          g_array_set_clear_func (shortcuts, shortcut_data_free);
        }

      g_array_set_size (shortcuts, shortcuts->len + 1);
      data = &g_array_index (shortcuts, ShortcutData, shortcuts->len - 1);
      data->shortcut = shortcut;
      data->index = index;
      data->widget = widget;
    }

  if (GTK_DEBUG_CHECK (KEYBINDINGS))
    {
      g_message ("Found %u shortcuts triggered %s by %s %u %u",
                 shortcuts ? shortcuts->len : 0,
                 has_exact ? "exactly" : "approximately",
                 gdk_event_get_event_type (event) == GDK_KEY_PRESS ? "key press" : "key release",
                 gdk_key_event_get_keyval (event),
                 gdk_event_get_modifier_state (event));
    }

  if (!shortcuts)
    return retval;

  p = shortcuts->len;
  for (i = 0; i < shortcuts->len; i++)
    {
      const ShortcutData *data = &g_array_index (shortcuts, ShortcutData, i);

      if (gtk_shortcut_action_activate (gtk_shortcut_get_action (data->shortcut),
                                        i == p - 1 ? GTK_SHORTCUT_ACTION_EXCLUSIVE : 0,
                                        data->widget,
                                        gtk_shortcut_get_arguments (data->shortcut)))
        {
          self->last_activated = data->index;
          retval = TRUE;
          break;
        }
    }

  g_array_free (shortcuts, TRUE);

  return retval;
}

static gboolean
gtk_shortcut_controller_handle_event (GtkEventController *controller,
                                      GdkEvent           *event,
                                      double              x,
                                      double              y)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (controller);
  GdkEventType event_type = gdk_event_get_event_type (event);
  gboolean enable_mnemonics;

  if (self->scope != GTK_SHORTCUT_SCOPE_LOCAL)
    return FALSE;

  if (event_type != GDK_KEY_PRESS && event_type != GDK_KEY_RELEASE)
    return FALSE;

  if (event_type == GDK_KEY_PRESS)
    {
      GdkModifierType modifiers, consumed_modifiers;

      modifiers = gdk_event_get_modifier_state (event);
      consumed_modifiers = gdk_key_event_get_consumed_modifiers (event);
      enable_mnemonics = (modifiers & ~consumed_modifiers & gtk_accelerator_get_default_mod_mask ()) == self->mnemonics_modifiers;
    }
  else
    {
      enable_mnemonics = FALSE;
    }

  return gtk_shortcut_controller_run_controllers (controller, event, x, y, enable_mnemonics);
}

static void
update_accel (GtkShortcut    *shortcut,
              GtkActionMuxer *muxer,
              gboolean        set)
{
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GVariant *target;
  const char *action_name;
  char *action_and_target;
  char *accel = NULL;

  if (!muxer)
    return;

  action = gtk_shortcut_get_action (shortcut);
  if (!GTK_IS_NAMED_ACTION (action))
    return;

  trigger = gtk_shortcut_get_trigger (shortcut);
  if (!GTK_IS_KEYVAL_TRIGGER (trigger))
    return;

  target = gtk_shortcut_get_arguments (shortcut);
  action_name = gtk_named_action_get_action_name (GTK_NAMED_ACTION (action));
  action_and_target = gtk_print_action_and_target (NULL, action_name, target);
  if (set)
    accel = gtk_shortcut_trigger_to_string (trigger);
  gtk_action_muxer_set_primary_accel (muxer, action_and_target, accel);

  g_free (action_and_target);
  g_free (accel);
}

void
gtk_shortcut_controller_update_accels (GtkShortcutController *self)
{
  GListModel *shortcuts = self->shortcuts;
  GtkWidget *widget;
  GtkActionMuxer *muxer;
  guint i, p;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  if (!widget || GTK_IS_MODEL_BUTTON (widget))
    return;

  muxer = _gtk_widget_get_action_muxer (widget, TRUE);
  for (i = 0, p = g_list_model_get_n_items (shortcuts); i < p; i++)
    {
      GtkShortcut *shortcut = g_list_model_get_item (shortcuts, i);
      if (GTK_IS_SHORTCUT (shortcut))
        update_accel (shortcut, muxer, TRUE);
      g_object_unref (shortcut);
    }
}

static void
gtk_shortcut_controller_set_widget (GtkEventController *controller,
                                    GtkWidget          *widget)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (controller);

  GTK_EVENT_CONTROLLER_CLASS (gtk_shortcut_controller_parent_class)->set_widget (controller, widget);

  gtk_shortcut_controller_update_accels (self);

  if (_gtk_widget_get_root (widget))
    gtk_shortcut_controller_root (self);
}

static void
gtk_shortcut_controller_unset_widget (GtkEventController *controller)
{
  GtkShortcutController *self = GTK_SHORTCUT_CONTROLLER (controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);

  if (_gtk_widget_get_root (widget))
    gtk_shortcut_controller_unroot (self);

#if 0
  int i;
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (controller)); i++)
    {
      GtkShortcut *shortcut = g_list_model_get_item (G_LIST_MODEL (controller), i);
      if (GTK_IS_SHORTCUT (shortcut))
        update_accel (shortcut, widget, FALSE);
      g_object_unref (shortcut);
    }
#endif

  GTK_EVENT_CONTROLLER_CLASS (gtk_shortcut_controller_parent_class)->unset_widget (controller);
}

static void
gtk_shortcut_controller_class_init (GtkShortcutControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->dispose = gtk_shortcut_controller_dispose;
  object_class->finalize = gtk_shortcut_controller_finalize;
  object_class->set_property = gtk_shortcut_controller_set_property;
  object_class->get_property = gtk_shortcut_controller_get_property;

  controller_class->handle_event = gtk_shortcut_controller_handle_event;
  controller_class->set_widget = gtk_shortcut_controller_set_widget;
  controller_class->unset_widget = gtk_shortcut_controller_unset_widget;

  /**
   * GtkShortcutController:item-type:
   *
   * The type of items. See [method@Gio.ListModel.get_item_type].
   *
   * Since: 4.8
   **/
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL,
                        G_TYPE_OBJECT,
                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcutController:mnemonic-modifiers: (attributes org.gtk.Property.get=gtk_shortcut_controller_get_mnemonics_modifiers org.gtk.Property.set=gtk_shortcut_controller_set_mnemonics_modifiers)
   *
   * The modifiers that need to be pressed to allow mnemonics activation.
   */
  properties[PROP_MNEMONICS_MODIFIERS] =
      g_param_spec_flags ("mnemonic-modifiers", NULL, NULL,
                          GDK_TYPE_MODIFIER_TYPE,
                          GDK_ALT_MASK,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcutController:model:
   *
   * A list model to take shortcuts from.
   */
  properties[PROP_MODEL] =
      g_param_spec_object ("model", NULL, NULL,
                           G_TYPE_LIST_MODEL,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcutController:n-items:
   *
   * The number of items. See [method@Gio.ListModel.get_n_items].
   *
   * Since: 4.8
   **/
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcutController:scope: (attributes org.gtk.Property.get=gtk_shortcut_controller_get_scope org.gtk.Property.set=gtk_shortcut_controller_set_scope)
   *
   * What scope the shortcuts will be handled in.
   */
  properties[PROP_SCOPE] =
      g_param_spec_enum ("scope", NULL, NULL,
                         GTK_TYPE_SHORTCUT_SCOPE,
                         GTK_SHORTCUT_SCOPE_LOCAL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_shortcut_controller_init (GtkShortcutController *self)
{
  self->mnemonics_modifiers = GDK_ALT_MASK;
}

void
gtk_shortcut_controller_root (GtkShortcutController *self)
{
  GtkShortcutManager *manager;

  switch (self->scope)
    {
    case GTK_SHORTCUT_SCOPE_LOCAL:
      return;

    case GTK_SHORTCUT_SCOPE_MANAGED:
      {
        GtkWidget *widget;

        for (widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
             !GTK_IS_SHORTCUT_MANAGER (widget);
             widget = _gtk_widget_get_parent (widget))
          ;

        if (!GTK_IS_SHORTCUT_MANAGER (widget))
          return;

        manager = GTK_SHORTCUT_MANAGER (widget);
      }
      break;

    case GTK_SHORTCUT_SCOPE_GLOBAL:
      {
        GtkRoot *root = gtk_widget_get_root (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self)));

        if (!GTK_IS_SHORTCUT_MANAGER (root))
          return;

        manager = GTK_SHORTCUT_MANAGER (root);
      }
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  GTK_SHORTCUT_MANAGER_GET_IFACE (manager)->add_controller (manager, self);
}

void
gtk_shortcut_controller_unroot (GtkShortcutController *self)
{
  GtkShortcutManager *manager;

  switch (self->scope)
    {
    case GTK_SHORTCUT_SCOPE_LOCAL:
      return;

    case GTK_SHORTCUT_SCOPE_MANAGED:
      {
        GtkWidget *widget;

        for (widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
             !GTK_IS_SHORTCUT_MANAGER (widget);
             widget = _gtk_widget_get_parent (widget))
          ;

        if (!GTK_IS_SHORTCUT_MANAGER (widget))
          return;

        manager = GTK_SHORTCUT_MANAGER (widget);
      }
      break;

    case GTK_SHORTCUT_SCOPE_GLOBAL:
      {
        GtkRoot *root = gtk_widget_get_root (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self)));

        if (!GTK_IS_SHORTCUT_MANAGER (root))
          return;

        manager = GTK_SHORTCUT_MANAGER (root);
      }
      break;

    default:
      g_assert_not_reached ();
      return;
    }

  GTK_SHORTCUT_MANAGER_GET_IFACE (manager)->remove_controller (manager, self);
}

/**
 * gtk_shortcut_controller_new:
 *
 * Creates a new shortcut controller.
 *
 * Returns: a newly created shortcut controller
 */
GtkEventController *
gtk_shortcut_controller_new (void)
{
  return g_object_new (GTK_TYPE_SHORTCUT_CONTROLLER,
                       NULL);
}

/**
 * gtk_shortcut_controller_new_for_model:
 * @model: a `GListModel` containing shortcuts
 *
 * Creates a new shortcut controller that takes its shortcuts from
 * the given list model.
 *
 * A controller created by this function does not let you add or
 * remove individual shortcuts using the shortcut controller api,
 * but you can change the contents of the model.
 *
 * Returns: a newly created shortcut controller
 */
GtkEventController *
gtk_shortcut_controller_new_for_model (GListModel *model)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);

  return g_object_new (GTK_TYPE_SHORTCUT_CONTROLLER,
                       "model", model,
                       NULL);
}

/**
 * gtk_shortcut_controller_add_shortcut:
 * @self: the controller
 * @shortcut: (transfer full): a `GtkShortcut`
 *
 * Adds @shortcut to the list of shortcuts handled by @self.
 *
 * If this controller uses an external shortcut list, this
 * function does nothing.
 */
void
gtk_shortcut_controller_add_shortcut (GtkShortcutController *self,
                                      GtkShortcut           *shortcut)
{
  GtkWidget *widget;

  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  if (!self->custom_shortcuts)
    {
      g_object_unref (shortcut);
      return;
    }

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  if (widget)
    {
      GtkActionMuxer *muxer = _gtk_widget_get_action_muxer (widget, TRUE);

      update_accel (shortcut, muxer, TRUE);
    }

  g_list_store_append (G_LIST_STORE (self->shortcuts), shortcut);
  g_object_unref (shortcut);
}

/**
 * gtk_shortcut_controller_remove_shortcut:
 * @self: the controller
 * @shortcut: a `GtkShortcut`
 *
 * Removes @shortcut from the list of shortcuts handled by @self.
 *
 * If @shortcut had not been added to @controller or this controller
 * uses an external shortcut list, this function does nothing.
 **/
void
gtk_shortcut_controller_remove_shortcut (GtkShortcutController  *self,
                                         GtkShortcut            *shortcut)
{
  GtkWidget *widget;
  guint i;

  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));
  g_return_if_fail (GTK_IS_SHORTCUT (shortcut));

  if (!self->custom_shortcuts)
    return;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (self));
  if (widget)
    {
      GtkActionMuxer *muxer = _gtk_widget_get_action_muxer (widget, FALSE);

      update_accel (shortcut, muxer, FALSE);
    }

  for (i = 0; i < g_list_model_get_n_items (self->shortcuts); i++)
    {
      GtkShortcut *item = g_list_model_get_item (self->shortcuts, i);

      if (item == shortcut)
        {
          g_object_unref (item);
          g_list_store_remove (G_LIST_STORE (self->shortcuts), i);
          return;
        }

      g_object_unref (item);
    }
}

/**
 * gtk_shortcut_controller_set_scope: (attributes org.gtk.Method.set_property=scope)
 * @self: a `GtkShortcutController`
 * @scope: the new scope to use
 *
 * Sets the controller to have the given @scope.
 *
 * The scope allows shortcuts to be activated outside of the normal
 * event propagation. In particular, it allows installing global
 * keyboard shortcuts that can be activated even when a widget does
 * not have focus.
 *
 * With %GTK_SHORTCUT_SCOPE_LOCAL, shortcuts will only be activated
 * when the widget has focus.
 */
void
gtk_shortcut_controller_set_scope (GtkShortcutController *self,
                                   GtkShortcutScope       scope)
{
  gboolean rooted;

  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));

  if (self->scope == scope)
    return;

  rooted = gtk_shortcut_controller_is_rooted (self);

  if (rooted)
    gtk_shortcut_controller_unroot (self);

  self->scope = scope;

  if (rooted)
    gtk_shortcut_controller_root (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SCOPE]);
}

/**
 * gtk_shortcut_controller_get_scope: (attributes org.gtk.Method.get_property=scope)
 * @self: a `GtkShortcutController`
 *
 * Gets the scope for when this controller activates its shortcuts.
 *
 * See [method@Gtk.ShortcutController.set_scope] for details.
 *
 * Returns: the controller's scope
 */
GtkShortcutScope
gtk_shortcut_controller_get_scope (GtkShortcutController *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self), GTK_SHORTCUT_SCOPE_LOCAL);

  return self->scope;
}

/**
 * gtk_shortcut_controller_set_mnemonics_modifiers: (attributes org.gtk.Method.set_property=mnemonic-modifiers)
 * @self: a `GtkShortcutController`
 * @modifiers: the new mnemonics_modifiers to use
 *
 * Sets the controller to use the given modifier for mnemonics.
 *
 * The mnemonics modifiers determines which modifiers need to be pressed to allow
 * activation of shortcuts with mnemonics triggers.
 *
 * GTK normally uses the Alt modifier for mnemonics, except in `GtkPopoverMenu`s,
 * where mnemonics can be triggered without any modifiers. It should be very
 * rarely necessary to change this, and doing so is likely to interfere with
 * other shortcuts.
 *
 * This value is only relevant for local shortcut controllers. Global and managed
 * shortcut controllers will have their shortcuts activated from other places which
 * have their own modifiers for activating mnemonics.
 */
void
gtk_shortcut_controller_set_mnemonics_modifiers (GtkShortcutController *self,
                                                 GdkModifierType        modifiers)
{
  g_return_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self));

  if (self->mnemonics_modifiers == modifiers)
    return;

  self->mnemonics_modifiers = modifiers;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MNEMONICS_MODIFIERS]);
}

/**
 * gtk_shortcut_controller_get_mnemonics_modifiers: (attributes org.gtk.Method.get_property=mnemonic-modifiers)
 * @self: a `GtkShortcutController`
 *
 * Gets the mnemonics modifiers for when this controller activates its shortcuts.
 *
 * Returns: the controller's mnemonics modifiers
 */
GdkModifierType
gtk_shortcut_controller_get_mnemonics_modifiers (GtkShortcutController *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_CONTROLLER (self), 0);

  return self->mnemonics_modifiers;
}
