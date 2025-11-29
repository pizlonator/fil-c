/* GTK - The GIMP Toolkit
 * Copyright (C) 2017, Red Hat, Inc.
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
 * GtkEventControllerKey:
 *
 * `GtkEventControllerKey` is an event controller that provides access
 * to key events.
 */

#include "config.h"

#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollerkey.h"
#include "gtkenums.h"
#include "gtkmain.h"
#include "gtktypebuiltins.h"

#include <gdk/gdk.h>

struct _GtkEventControllerKey
{
  GtkEventController parent_instance;
  GtkIMContext *im_context;
  GHashTable *pressed_keys;

  GdkModifierType state;

  gboolean is_focus;

  GdkEvent *current_event;
};

struct _GtkEventControllerKeyClass
{
  GtkEventControllerClass parent_class;
};

enum {
  KEY_PRESSED,
  KEY_RELEASED,
  MODIFIERS,
  IM_UPDATE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkEventControllerKey, gtk_event_controller_key,
               GTK_TYPE_EVENT_CONTROLLER)

static void
gtk_event_controller_key_finalize (GObject *object)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (object);

  g_hash_table_destroy (key->pressed_keys);
  g_clear_object (&key->im_context);

  G_OBJECT_CLASS (gtk_event_controller_key_parent_class)->finalize (object);
}

static gboolean
gtk_event_controller_key_handle_event (GtkEventController *controller,
                                       GdkEvent           *event,
                                       double              x,
                                       double              y)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (controller);
  GdkEventType event_type = gdk_event_get_event_type (event);
  GdkModifierType state;
  guint16 keycode;
  guint keyval;
  gboolean handled = FALSE;

  if (event_type != GDK_KEY_PRESS && event_type != GDK_KEY_RELEASE)
    return FALSE;

  if (key->im_context &&
      gtk_im_context_filter_keypress (key->im_context, event))
    {
      g_signal_emit (controller, signals[IM_UPDATE], 0);
      return TRUE;
    }

  key->current_event = event;

  state = gdk_event_get_modifier_state (event);
  if (key->state != state)
    {
      gboolean unused;

      key->state = state;
      g_signal_emit (controller, signals[MODIFIERS], 0, state, &unused);
    }

  keycode = gdk_key_event_get_keycode (event);
  keyval = gdk_key_event_get_keyval (event);

  if (event_type == GDK_KEY_PRESS)
    {
      g_signal_emit (controller, signals[KEY_PRESSED], 0,
                     keyval, keycode, state, &handled);
      if (handled)
        g_hash_table_add (key->pressed_keys, GUINT_TO_POINTER (keyval));
    }
  else if (event_type == GDK_KEY_RELEASE)
    {
      g_signal_emit (controller, signals[KEY_RELEASED], 0,
                     keyval, keycode, state);

      handled = g_hash_table_lookup (key->pressed_keys, GUINT_TO_POINTER (keyval)) != NULL;
      g_hash_table_remove (key->pressed_keys, GUINT_TO_POINTER (keyval));
    }
  else
    handled = FALSE;

  key->current_event = NULL;

  return handled;
}

static void
gtk_event_controller_key_handle_crossing (GtkEventController    *controller,
                                          const GtkCrossingData *crossing,
                                          double                 x,
                                          double                 y)
{
  GtkEventControllerKey *key = GTK_EVENT_CONTROLLER_KEY (controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gboolean start_crossing, end_crossing;
  gboolean is_focus;

  if (crossing->type != GTK_CROSSING_FOCUS &&
      crossing->type != GTK_CROSSING_ACTIVE)
    return;

  start_crossing = crossing->direction == GTK_CROSSING_OUT &&
                   widget == crossing->old_target;
  end_crossing = crossing->direction == GTK_CROSSING_IN &&
                 widget == crossing->new_target;

  if (!start_crossing && !end_crossing)
    return;

  is_focus = end_crossing;

  if (key->is_focus != is_focus)
    {
      key->is_focus = is_focus;

      if (key->im_context)
        {
          if (is_focus)
            gtk_im_context_focus_in (key->im_context);
          else
            gtk_im_context_focus_out (key->im_context);
        }
    }
}

static void
gtk_event_controller_key_class_init (GtkEventControllerKeyClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_event_controller_key_finalize;
  controller_class->handle_event = gtk_event_controller_key_handle_event;
  controller_class->handle_crossing = gtk_event_controller_key_handle_crossing;

  /**
   * GtkEventControllerKey::key-pressed:
   * @controller: the object which received the signal.
   * @keyval: the pressed key.
   * @keycode: the raw code of the pressed key.
   * @state: the bitmask, representing the state of modifier keys and pointer buttons.
   *
   * Emitted whenever a key is pressed.
   *
   * Returns: %TRUE if the key press was handled, %FALSE otherwise.
   */
  signals[KEY_PRESSED] =
    g_signal_new (I_("key-pressed"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, _gtk_boolean_handled_accumulator, NULL,
                  _gtk_marshal_BOOLEAN__UINT_UINT_FLAGS,
                  G_TYPE_BOOLEAN, 3, G_TYPE_UINT, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[KEY_PRESSED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__UINT_UINT_FLAGSv);

  /**
   * GtkEventControllerKey::key-released:
   * @controller: the object which received the signal.
   * @keyval: the released key.
   * @keycode: the raw code of the released key.
   * @state: the bitmask, representing the state of modifier keys and pointer buttons.
   *
   * Emitted whenever a key is released.
   */
  signals[KEY_RELEASED] =
    g_signal_new (I_("key-released"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__UINT_UINT_FLAGS,
                  G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_UINT, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[KEY_RELEASED],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__UINT_UINT_FLAGSv);

  /**
   * GtkEventControllerKey::modifiers:
   * @controller: the object which received the signal.
   * @state: the bitmask, representing the new state of modifier keys and
   *   pointer buttons.
   *
   * Emitted whenever the state of modifier keys and pointer buttons change.
   */
  signals[MODIFIERS] =
    g_signal_new (I_("modifiers"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL,
                  NULL,
                  _gtk_marshal_BOOLEAN__FLAGS,
                  G_TYPE_BOOLEAN, 1, GDK_TYPE_MODIFIER_TYPE);
  g_signal_set_va_marshaller (signals[MODIFIERS],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_BOOLEAN__FLAGSv);

  /**
   * GtkEventControllerKey::im-update:
   * @controller: the object which received the signal
   *
   * Emitted whenever the input method context filters away
   * a keypress and prevents the @controller receiving it.
   *
   * See [method@Gtk.EventControllerKey.set_im_context] and
   * [method@Gtk.IMContext.filter_keypress].
   */
  signals[IM_UPDATE] =
    g_signal_new (I_("im-update"),
                  GTK_TYPE_EVENT_CONTROLLER_KEY,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
gtk_event_controller_key_init (GtkEventControllerKey *controller)
{
  controller->pressed_keys = g_hash_table_new (NULL, NULL);
}

/**
 * gtk_event_controller_key_new:
 *
 * Creates a new event controller that will handle key events.
 *
 * Returns: a new `GtkEventControllerKey`
 **/
GtkEventController *
gtk_event_controller_key_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_KEY, NULL);
}

/**
 * gtk_event_controller_key_set_im_context:
 * @controller: a `GtkEventControllerKey`
 * @im_context: (nullable): a `GtkIMContext`
 *
 * Sets the input method context of the key @controller.
 */
void
gtk_event_controller_key_set_im_context (GtkEventControllerKey *controller,
                                         GtkIMContext          *im_context)
{
  g_return_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller));
  g_return_if_fail (!im_context || GTK_IS_IM_CONTEXT (im_context));

  if (controller->im_context)
    gtk_im_context_reset (controller->im_context);

  g_set_object (&controller->im_context, im_context);
}

/**
 * gtk_event_controller_key_get_im_context:
 * @controller: a `GtkEventControllerKey`
 *
 * Gets the input method context of the key @controller.
 *
 * Returns: (transfer none) (nullable): the `GtkIMContext`
 **/
GtkIMContext *
gtk_event_controller_key_get_im_context (GtkEventControllerKey *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), NULL);

  return controller->im_context;
}

/**
 * gtk_event_controller_key_forward:
 * @controller: a `GtkEventControllerKey`
 * @widget: a `GtkWidget`
 *
 * Forwards the current event of this @controller to a @widget.
 *
 * This function can only be used in handlers for the
 * [signal@Gtk.EventControllerKey::key-pressed],
 * [signal@Gtk.EventControllerKey::key-released]
 * or [signal@Gtk.EventControllerKey::modifiers] signals.
 *
 * Returns: whether the @widget handled the event
 */
gboolean
gtk_event_controller_key_forward (GtkEventControllerKey *controller,
                                  GtkWidget             *widget)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);
  g_return_val_if_fail (gdk_event_get_event_type (controller->current_event) == GDK_KEY_PRESS ||
                        gdk_event_get_event_type (controller->current_event) == GDK_KEY_RELEASE, FALSE);

  if (!gtk_widget_get_realized (widget))
    gtk_widget_realize (widget);

  if (gtk_widget_run_controllers (widget, controller->current_event, widget, 0, 0,
                                  GTK_PHASE_CAPTURE))
    return TRUE;
  if (gtk_widget_run_controllers (widget, controller->current_event, widget, 0, 0,
                                  GTK_PHASE_TARGET))
    return TRUE;
  if (gtk_widget_run_controllers (widget, controller->current_event, widget, 0, 0,
                                  GTK_PHASE_BUBBLE))
    return TRUE;

  return FALSE;
}

/**
 * gtk_event_controller_key_get_group:
 * @controller: a `GtkEventControllerKey`
 *
 * Gets the key group of the current event of this @controller.
 *
 * See [method@Gdk.KeyEvent.get_layout].
 *
 * Returns: the key group
 */
guint
gtk_event_controller_key_get_group (GtkEventControllerKey *controller)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_KEY (controller), FALSE);
  g_return_val_if_fail (controller->current_event != NULL, FALSE);

  return gdk_key_event_get_layout (controller->current_event);
}
