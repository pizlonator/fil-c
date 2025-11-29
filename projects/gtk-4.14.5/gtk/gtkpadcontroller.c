/* GTK - The GIMP Toolkit
 * Copyright (C) 2016, Red Hat, Inc.
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
 *
 * Author(s): Carlos Garnacho <carlosg@gnome.org>
 */

/**
 * GtkPadController:
 *
 * `GtkPadController` is an event controller for the pads found in drawing
 * tablets.
 *
 * Pads are the collection of buttons and tactile sensors often found around
 * the stylus-sensitive area.
 *
 * These buttons and sensors have no implicit meaning, and by default they
 * perform no action. `GtkPadController` is provided to map those to
 * [iface@Gio.Action] objects, thus letting the application give them a more
 * semantic meaning.
 *
 * Buttons and sensors are not constrained to triggering a single action,
 * some %GDK_SOURCE_TABLET_PAD devices feature multiple "modes". All these
 * input elements have one current mode, which may determine the final action
 * being triggered.
 *
 * Pad devices often divide buttons and sensors into groups. All elements
 * in a group share the same current mode, but different groups may have
 * different modes. See [method@Gdk.DevicePad.get_n_groups] and
 * [method@Gdk.DevicePad.get_group_n_modes].
 *
 * Each of the actions that a given button/strip/ring performs for a given mode
 * is defined by a [struct@Gtk.PadActionEntry]. It contains an action name that
 * will be looked up in the given [iface@Gio.ActionGroup] and activated whenever
 * the specified input element and mode are triggered.
 *
 * A simple example of `GtkPadController` usage: Assigning button 1 in all
 * modes and pad devices to an "invert-selection" action:
 *
 * ```c
 * GtkPadActionEntry *pad_actions[] = {
 *   { GTK_PAD_ACTION_BUTTON, 1, -1, "Invert selection", "pad-actions.invert-selection" },
 *   …
 * };
 *
 * …
 * action_group = g_simple_action_group_new ();
 * action = g_simple_action_new ("pad-actions.invert-selection", NULL);
 * g_signal_connect (action, "activate", on_invert_selection_activated, NULL);
 * g_action_map_add_action (G_ACTION_MAP (action_group), action);
 * …
 * pad_controller = gtk_pad_controller_new (action_group, NULL);
 * ```
 *
 * The actions belonging to rings/strips will be activated with a parameter
 * of type %G_VARIANT_TYPE_DOUBLE bearing the value of the given axis, it
 * is required that those are made stateful and accepting this `GVariantType`.
 */

#include "config.h"

#include "gtkeventcontrollerprivate.h"
#include "gtkpadcontroller.h"
#include "gtkwindow.h"
#include "gtkprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#include "gdk/wayland/gdkdevice-wayland-private.h"
#endif

struct _GtkPadController {
  GtkEventController parent_instance;
  GActionGroup *action_group;
  GdkDevice *pad;

  GArray *action_entries;
};

struct _GtkPadControllerClass {
  GtkEventControllerClass parent_class;
};

enum {
  PROP_0,
  PROP_ACTION_GROUP,
  PROP_PAD,
  N_PROPS
};

typedef struct
{
  GtkPadActionType type;
  int index;
  int mode;
  char *label;
  char *action_name;
} ActionEntryData;

static GParamSpec *pspecs[N_PROPS] = { NULL };

G_DEFINE_TYPE (GtkPadController, gtk_pad_controller, GTK_TYPE_EVENT_CONTROLLER)

static const ActionEntryData *
gtk_pad_action_find_match (GtkPadController *controller,
                           GtkPadActionType  type,
                           int               index,
                           int               mode)
{
  guint i;

  for (i = 0; i < controller->action_entries->len; i++)
    {
      const ActionEntryData *entry = &g_array_index (controller->action_entries,
                                                     ActionEntryData, i);
      gboolean match_index = FALSE, match_mode = FALSE;

      if (entry->type != type)
        continue;

      match_index = entry->index < 0 || entry->index == index;
      match_mode = entry->mode < 0 || entry->mode == mode;

      if (match_index && match_mode)
        return entry;
    }

  return NULL;
}

static void
gtk_pad_controller_activate_action (GtkPadController        *controller,
                                    const ActionEntryData   *entry)
{
  g_action_group_activate_action (controller->action_group,
                                  entry->action_name,
                                  NULL);
}

static void
gtk_pad_controller_activate_action_with_axis (GtkPadController        *controller,
                                              const ActionEntryData   *entry,
                                              double                   value)
{
  g_action_group_activate_action (controller->action_group,
                                  entry->action_name,
                                  g_variant_new_double (value));
}

static void
gtk_pad_controller_handle_mode_switch (GtkPadController *controller,
                                       GdkDevice        *pad,
                                       guint             group,
                                       guint             mode)
{
#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_device_get_display (pad)))
    {
      const ActionEntryData *entry;
      int elem, idx, n_features;

      for (elem = GTK_PAD_ACTION_BUTTON; elem <= GTK_PAD_ACTION_STRIP; elem++)
        {
          n_features = gdk_device_pad_get_n_features (GDK_DEVICE_PAD (pad),
                                                      elem);

          for (idx = 0; idx < n_features; idx++)
            {
              if (gdk_device_pad_get_feature_group (GDK_DEVICE_PAD (pad),
                                                    elem, idx) != group)
                continue;

              entry = gtk_pad_action_find_match (controller, elem, idx, mode);
              if (!entry)
                continue;
              if (!g_action_group_has_action (controller->action_group,
                                              entry->action_name))
                continue;

              gdk_wayland_device_pad_set_feedback (pad, elem, idx,
                                                   g_dgettext (NULL, entry->label));
            }
        }
    }
#endif
}

static gboolean
gtk_pad_controller_filter_event (GtkEventController *controller,
                                 GdkEvent           *event)
{
  GtkPadController *pad_controller = GTK_PAD_CONTROLLER (controller);
  GdkEventType event_type = gdk_event_get_event_type (event);

  if (event_type != GDK_PAD_BUTTON_PRESS &&
      event_type != GDK_PAD_BUTTON_RELEASE &&
      event_type != GDK_PAD_RING &&
      event_type != GDK_PAD_STRIP &&
      event_type != GDK_PAD_GROUP_MODE)
    return TRUE;

  if (pad_controller->pad &&
      gdk_event_get_device (event) != pad_controller->pad)
    return TRUE;

  return FALSE;
}

static gboolean
gtk_pad_controller_handle_event (GtkEventController *controller,
                                 GdkEvent           *event,
                                 double              x,
                                 double              y)
{
  GtkPadController *pad_controller = GTK_PAD_CONTROLLER (controller);
  GdkEventType event_type = gdk_event_get_event_type (event);
  const ActionEntryData *entry;
  GtkPadActionType type;
  guint index, mode, group;
  double value = 0;

  gdk_pad_event_get_group_mode (event, &group, &mode);
  if (event_type == GDK_PAD_GROUP_MODE)
    {
      gtk_pad_controller_handle_mode_switch (pad_controller,
                                             gdk_event_get_device (event),
                                             group,
                                             mode);
      return GDK_EVENT_PROPAGATE;
    }

  switch ((guint) event_type)
    {
    case GDK_PAD_BUTTON_PRESS:
      type = GTK_PAD_ACTION_BUTTON;
      index = gdk_pad_event_get_button (event);
      break;
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
      type = event_type == GDK_PAD_RING ?
        GTK_PAD_ACTION_RING : GTK_PAD_ACTION_STRIP;
      gdk_pad_event_get_axis_value (event, &index, &value);
      break;
    default:
      return GDK_EVENT_PROPAGATE;
    }

  entry = gtk_pad_action_find_match (pad_controller,
                                     type, index, mode);
  if (!entry)
    return GDK_EVENT_PROPAGATE;

  if (event_type == GDK_PAD_RING || event_type == GDK_PAD_STRIP)
    gtk_pad_controller_activate_action_with_axis (pad_controller, entry, value);
  else
    gtk_pad_controller_activate_action (pad_controller, entry);

  return GDK_EVENT_STOP;
}

static void
gtk_pad_controller_set_pad (GtkPadController *controller,
                            GdkDevice        *pad)
{
  g_return_if_fail (!pad || GDK_IS_DEVICE (pad));
  g_return_if_fail (!pad || gdk_device_get_source (pad) == GDK_SOURCE_TABLET_PAD);

  g_set_object (&controller->pad, pad);
}

static void
gtk_pad_controller_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkPadController *controller = GTK_PAD_CONTROLLER (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      controller->action_group = g_value_dup_object (value);
      break;
    case PROP_PAD:
      gtk_pad_controller_set_pad (controller, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_pad_controller_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkPadController *controller = GTK_PAD_CONTROLLER (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      g_value_set_object (value, controller->action_group);
      break;
    case PROP_PAD:
      g_value_set_object (value, controller->pad);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_pad_controller_dispose (GObject *object)
{
  GtkPadController *controller = GTK_PAD_CONTROLLER (object);

  g_clear_object (&controller->action_group);
  g_clear_object (&controller->pad);

  G_OBJECT_CLASS (gtk_pad_controller_parent_class)->dispose (object);
}

static void
gtk_pad_controller_finalize (GObject *object)
{
  GtkPadController *controller = GTK_PAD_CONTROLLER (object);
  guint i;

  for (i = 0; i < controller->action_entries->len; i++)
    {
      const ActionEntryData *entry = &g_array_index (controller->action_entries,
                                                     ActionEntryData, i);

      g_free (entry->label);
      g_free (entry->action_name);
    }
  g_array_free (controller->action_entries, TRUE);

  G_OBJECT_CLASS (gtk_pad_controller_parent_class)->finalize (object);
}

static void
gtk_pad_controller_class_init (GtkPadControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  controller_class->filter_event = gtk_pad_controller_filter_event;
  controller_class->handle_event = gtk_pad_controller_handle_event;

  object_class->set_property = gtk_pad_controller_set_property;
  object_class->get_property = gtk_pad_controller_get_property;
  object_class->dispose = gtk_pad_controller_dispose;
  object_class->finalize = gtk_pad_controller_finalize;

  pspecs[PROP_ACTION_GROUP] =
    g_param_spec_object ("action-group", NULL, NULL,
                         G_TYPE_ACTION_GROUP,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  pspecs[PROP_PAD] =
    g_param_spec_object ("pad", NULL, NULL,
                         GDK_TYPE_DEVICE,
                         GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, pspecs);
}

static void
gtk_pad_controller_init (GtkPadController *controller)
{
  controller->action_entries = g_array_new (FALSE, TRUE, sizeof (ActionEntryData));
}

/**
 * gtk_pad_controller_new:
 * @group: `GActionGroup` to trigger actions from
 * @pad: (nullable): A %GDK_SOURCE_TABLET_PAD device, or %NULL to handle all pads
 *
 * Creates a new `GtkPadController` that will associate events from @pad to
 * actions.
 *
 * A %NULL pad may be provided so the controller manages all pad devices
 * generically, it is discouraged to mix `GtkPadController` objects with
 * %NULL and non-%NULL @pad argument on the same toplevel window, as execution
 * order is not guaranteed.
 *
 * The `GtkPadController` is created with no mapped actions. In order to
 * map pad events to actions, use [method@Gtk.PadController.set_action_entries]
 * or [method@Gtk.PadController.set_action].
 *
 * Be aware that pad events will only be delivered to `GtkWindow`s, so adding
 * a pad controller to any other type of widget will not have an effect.
 *
 * Returns: A newly created `GtkPadController`
 */
GtkPadController *
gtk_pad_controller_new (GActionGroup *group,
                        GdkDevice    *pad)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (group), NULL);
  g_return_val_if_fail (!pad || GDK_IS_DEVICE (pad), NULL);
  g_return_val_if_fail (!pad || gdk_device_get_source (pad) == GDK_SOURCE_TABLET_PAD, NULL);

  return g_object_new (GTK_TYPE_PAD_CONTROLLER,
                       "propagation-phase", GTK_PHASE_CAPTURE,
                       "action-group", group,
                       "pad", pad,
                       NULL);
}

static int
entry_compare_func (gconstpointer a,
                    gconstpointer b)
{
  const GtkPadActionEntry *entry1 = a, *entry2 = b;

  if (entry1->mode > entry2->mode)
    return -1;
  else if (entry1->mode < entry2->mode)
    return 1;
  else if (entry1->index > entry2->index)
    return -1;
  else if (entry1->index < entry2->index)
    return 1;

  return 0;
}

static void
gtk_pad_controller_add_entry (GtkPadController        *controller,
                              const GtkPadActionEntry *entry)
{
  guint i;
  const ActionEntryData new_entry = {
   .type = entry->type,
   .index = entry->index,
   .mode = entry->mode,
   .label= g_strdup (entry->label),
   .action_name = g_strdup (entry->action_name)
  };

  for (i = 0; i < controller->action_entries->len; i++)
    {
      if (entry_compare_func (&new_entry,
                              &g_array_index (controller->action_entries, ActionEntryData, i)) == 0)
        break;
    }

  g_array_insert_val (controller->action_entries, i, new_entry);
}

/**
 * gtk_pad_controller_set_action_entries:
 * @controller: a `GtkPadController`
 * @entries: (array length=n_entries): the action entries to set on @controller
 * @n_entries: the number of elements in @entries
 *
 * A convenience function to add a group of action entries on
 * @controller.
 *
 * See [struct@Gtk.PadActionEntry] and [method@Gtk.PadController.set_action].
 */
void
gtk_pad_controller_set_action_entries (GtkPadController        *controller,
                                       const GtkPadActionEntry *entries,
                                       int                      n_entries)
{
  int i;

  g_return_if_fail (GTK_IS_PAD_CONTROLLER (controller));
  g_return_if_fail (entries != NULL);

  for (i = 0; i < n_entries; i++)
    gtk_pad_controller_add_entry (controller, &entries[i]);
}

/**
 * gtk_pad_controller_set_action:
 * @controller: a `GtkPadController`
 * @type: the type of pad feature that will trigger this action
 * @index: the 0-indexed button/ring/strip number that will trigger this action
 * @mode: the mode that will trigger this action, or -1 for all modes.
 * @label: Human readable description of this action, this string should
 *   be deemed user-visible.
 * @action_name: action name that will be activated in the `GActionGroup`
 *
 * Adds an individual action to @controller.
 *
 * This action will only be activated if the given button/ring/strip number
 * in @index is interacted while the current mode is @mode. -1 may be used
 * for simple cases, so the action is triggered on all modes.
 *
 * The given @label should be considered user-visible, so internationalization
 * rules apply. Some windowing systems may be able to use those for user
 * feedback.
 */
void
gtk_pad_controller_set_action (GtkPadController *controller,
                               GtkPadActionType  type,
                               int               index,
                               int               mode,
                               const char       *label,
                               const char       *action_name)
{
  const GtkPadActionEntry entry = { type, index, mode, label, action_name };

  g_return_if_fail (GTK_IS_PAD_CONTROLLER (controller));
  g_return_if_fail (type <= GTK_PAD_ACTION_STRIP);

  gtk_pad_controller_add_entry (controller, &entry);
}
