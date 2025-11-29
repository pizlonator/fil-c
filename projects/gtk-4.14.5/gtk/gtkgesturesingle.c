/* GTK - The GIMP Toolkit
 * Copyright (C) 2012, One Laptop Per Child.
 * Copyright (C) 2014, Red Hat, Inc.
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
 * GtkGestureSingle:
 *
 * `GtkGestureSingle` is a `GtkGestures` subclass optimized for singe-touch
 * and mouse gestures.
 *
 * Under interaction, these gestures stick to the first interacting sequence,
 * which is accessible through [method@Gtk.GestureSingle.get_current_sequence]
 * while the gesture is being interacted with.
 *
 * By default gestures react to both %GDK_BUTTON_PRIMARY and touch events.
 * [method@Gtk.GestureSingle.set_touch_only] can be used to change the
 * touch behavior. Callers may also specify a different mouse button number
 * to interact with through [method@Gtk.GestureSingle.set_button], or react
 * to any mouse button by setting it to 0. While the gesture is active, the
 * button being currently pressed can be known through
 * [method@Gtk.GestureSingle.get_current_button].
 */

#include "config.h"
#include "gtkgesturesingle.h"
#include "gtkgesturesingleprivate.h"
#include "gtkprivate.h"
#include "gtkdebug.h"

typedef struct _GtkGestureSinglePrivate GtkGestureSinglePrivate;

struct _GtkGestureSinglePrivate
{
  GdkEventSequence *current_sequence;
  guint button;
  guint current_button;
  guint touch_only : 1;
  guint exclusive  : 1;
};

enum {
  PROP_TOUCH_ONLY = 1,
  PROP_EXCLUSIVE,
  PROP_BUTTON,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkGestureSingle, gtk_gesture_single, GTK_TYPE_GESTURE)

static void
gtk_gesture_single_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (object));

  switch (prop_id)
    {
    case PROP_TOUCH_ONLY:
      g_value_set_boolean (value, priv->touch_only);
      break;
    case PROP_EXCLUSIVE:
      g_value_set_boolean (value, priv->exclusive);
      break;
    case PROP_BUTTON:
      g_value_set_uint (value, priv->button);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_single_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_TOUCH_ONLY:
      gtk_gesture_single_set_touch_only (GTK_GESTURE_SINGLE (object),
                                         g_value_get_boolean (value));
      break;
    case PROP_EXCLUSIVE:
      gtk_gesture_single_set_exclusive (GTK_GESTURE_SINGLE (object),
                                        g_value_get_boolean (value));
      break;
    case PROP_BUTTON:
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (object),
                                     g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_gesture_single_cancel (GtkGesture       *gesture,
                           GdkEventSequence *sequence)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (gesture));

  if (sequence == priv->current_sequence)
    priv->current_button = 0;
}

static gboolean
gtk_gesture_single_handle_event (GtkEventController *controller,
                                 GdkEvent           *event,
                                 double              x,
                                 double              y)
{
  GdkEventSequence *sequence = NULL;
  GtkGestureSinglePrivate *priv;
  GdkDevice *source_device;
  GdkInputSource source;
  guint button = 0, state, i;
  gboolean retval;
  GdkEventType event_type;

  source_device = gdk_event_get_device (event);

  if (!source_device)
    return FALSE;

  priv = gtk_gesture_single_get_instance_private (GTK_GESTURE_SINGLE (controller));
  source = gdk_device_get_source (source_device);

  event_type = gdk_event_get_event_type (event);

  switch ((guint) event_type)
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
    case GDK_TOUCH_UPDATE:
      if (priv->exclusive && !gdk_touch_event_get_emulating_pointer (event))
        return FALSE;

      sequence = gdk_event_get_event_sequence (event);
      button = 1;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      if (priv->touch_only && source != GDK_SOURCE_TOUCHSCREEN)
        return FALSE;

      button = gdk_button_event_get_button (event);
      break;
    case GDK_MOTION_NOTIFY:
      if (!gtk_gesture_handles_sequence (GTK_GESTURE (controller), sequence))
        return FALSE;
      if (priv->touch_only && source != GDK_SOURCE_TOUCHSCREEN)
        return FALSE;
      state = gdk_event_get_modifier_state (event);

      if (priv->current_button > 0 && priv->current_button <= 5 &&
          (state & (GDK_BUTTON1_MASK << (priv->current_button - 1))))
        button = priv->current_button;
      else if (priv->current_button == 0)
        {
          /* No current button, find out from the mask */
          for (i = 0; i < 3; i++)
            {
              if ((state & (GDK_BUTTON1_MASK << i)) == 0)
                continue;
              button = i + 1;
              break;
            }
        }

      break;
    case GDK_TOUCHPAD_HOLD:
      if (gdk_touchpad_event_get_n_fingers (event) == 1)
        return FALSE;
      G_GNUC_FALLTHROUGH;
    case GDK_TOUCH_CANCEL:
    case GDK_GRAB_BROKEN:
    case GDK_TOUCHPAD_SWIPE:
      return GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_single_parent_class)->handle_event (controller, event, x, y);
      break;
    default:
      return FALSE;
    }

  if (button == 0 ||
      (priv->button != 0 && priv->button != button) ||
      (priv->current_button != 0 && priv->current_button != button))
    {
      if (gtk_gesture_is_active (GTK_GESTURE (controller)))
        gtk_event_controller_reset (controller);
      return FALSE;
    }

  if (event_type == GDK_BUTTON_PRESS || event_type == GDK_TOUCH_BEGIN ||
      event_type == GDK_MOTION_NOTIFY || event_type == GDK_TOUCH_UPDATE)
    {
      if (!gtk_gesture_is_active (GTK_GESTURE (controller)))
        priv->current_sequence = sequence;

      priv->current_button = button;
    }

  retval = GTK_EVENT_CONTROLLER_CLASS (gtk_gesture_single_parent_class)->handle_event (controller, event, x, y);

  if (sequence == priv->current_sequence &&
      (event_type == GDK_BUTTON_RELEASE || event_type == GDK_TOUCH_END))
    priv->current_button = 0;
  else if (priv->current_sequence == sequence &&
           !gtk_gesture_handles_sequence (GTK_GESTURE (controller), sequence))
    {
      if (button == priv->current_button && event_type == GDK_BUTTON_PRESS)
        priv->current_button = 0;
      else if (sequence == priv->current_sequence && event_type == GDK_TOUCH_BEGIN)
        priv->current_sequence = NULL;
    }

  return retval;
}

static void
gtk_gesture_single_class_init (GtkGestureSingleClass *klass)
{
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_gesture_single_get_property;
  object_class->set_property = gtk_gesture_single_set_property;

  controller_class->handle_event = gtk_gesture_single_handle_event;

  gesture_class->cancel = gtk_gesture_single_cancel;

  /**
   * GtkGestureSingle:touch-only: (attributes org.gtk.Property.get=gtk_gesture_single_get_touch_only org.gtk.Property.set=gtk_gesture_single_set_touch_only)
   *
   * Whether the gesture handles only touch events.
   */
  properties[PROP_TOUCH_ONLY] =
      g_param_spec_boolean ("touch-only", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGestureSingle:exclusive: (attributes org.gtk.Property.get=gtk_gesture_single_get_exclusive org.gtk.Property.set=gtk_gesture_single_set_exclusive)
   *
   * Whether the gesture is exclusive.
   *
   * Exclusive gestures only listen to pointer and pointer emulated events.
   */
  properties[PROP_EXCLUSIVE] =
      g_param_spec_boolean ("exclusive", NULL, NULL,
                            FALSE,
                            GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkGestureSingle:button: (attributes org.gtk.Property.get=gtk_gesture_single_get_button org.gtk.Property.set=gtk_gesture_single_set_button)
   *
   * Mouse button number to listen to, or 0 to listen for any button.
   */
  properties[PROP_BUTTON] =
      g_param_spec_uint ("button", NULL, NULL,
                         0, G_MAXUINT,
                         GDK_BUTTON_PRIMARY,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_gesture_single_init (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  priv = gtk_gesture_single_get_instance_private (gesture);
  priv->touch_only = FALSE;
  priv->button = GDK_BUTTON_PRIMARY;
}

/**
 * gtk_gesture_single_get_touch_only: (attributes org.gtk.Method.get_property=touch-only)
 * @gesture: a `GtkGestureSingle`
 *
 * Returns %TRUE if the gesture is only triggered by touch events.
 *
 * Returns: %TRUE if the gesture only handles touch events
 */
gboolean
gtk_gesture_single_get_touch_only (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), FALSE);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->touch_only;
}

/**
 * gtk_gesture_single_set_touch_only: (attributes org.gtk.Method.set_property=touch-only)
 * @gesture: a `GtkGestureSingle`
 * @touch_only: whether @gesture handles only touch events
 *
 * Sets whether to handle only touch events.
 *
 * If @touch_only is %TRUE, @gesture will only handle events of type
 * %GDK_TOUCH_BEGIN, %GDK_TOUCH_UPDATE or %GDK_TOUCH_END. If %FALSE,
 * mouse events will be handled too.
 */
void
gtk_gesture_single_set_touch_only (GtkGestureSingle *gesture,
                                   gboolean          touch_only)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  touch_only = touch_only != FALSE;
  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->touch_only == touch_only)
    return;

  priv->touch_only = touch_only;
  g_object_notify_by_pspec (G_OBJECT (gesture), properties[PROP_TOUCH_ONLY]);
}

/**
 * gtk_gesture_single_get_exclusive: (attributes org.gtk.Method.get_property=exclusive)
 * @gesture: a `GtkGestureSingle`
 *
 * Gets whether a gesture is exclusive.
 *
 * For more information, see [method@Gtk.GestureSingle.set_exclusive].
 *
 * Returns: Whether the gesture is exclusive
 */
gboolean
gtk_gesture_single_get_exclusive (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), FALSE);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->exclusive;
}

/**
 * gtk_gesture_single_set_exclusive: (attributes org.gtk.Method.set_property=exclusive)
 * @gesture: a `GtkGestureSingle`
 * @exclusive: %TRUE to make @gesture exclusive
 *
 * Sets whether @gesture is exclusive.
 *
 * An exclusive gesture will only handle pointer and "pointer emulated"
 * touch events, so at any given time, there is only one sequence able
 * to interact with those.
 */
void
gtk_gesture_single_set_exclusive (GtkGestureSingle *gesture,
                                  gboolean          exclusive)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  exclusive = exclusive != FALSE;
  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->exclusive == exclusive)
    return;

  priv->exclusive = exclusive;
  g_object_notify_by_pspec (G_OBJECT (gesture), properties[PROP_EXCLUSIVE]);
}

/**
 * gtk_gesture_single_get_button: (attributes org.gtk.Method.get_property=button)
 * @gesture: a `GtkGestureSingle`
 *
 * Returns the button number @gesture listens for.
 *
 * If this is 0, the gesture reacts to any button press.
 *
 * Returns: The button number, or 0 for any button
 */
guint
gtk_gesture_single_get_button (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), 0);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->button;
}

/**
 * gtk_gesture_single_set_button: (attributes org.gtk.Method.set_property=button)
 * @gesture: a `GtkGestureSingle`
 * @button: button number to listen to, or 0 for any button
 *
 * Sets the button number @gesture listens to.
 *
 * If non-0, every button press from a different button
 * number will be ignored. Touch events implicitly match
 * with button 1.
 */
void
gtk_gesture_single_set_button (GtkGestureSingle *gesture,
                               guint             button)
{
  GtkGestureSinglePrivate *priv;

  g_return_if_fail (GTK_IS_GESTURE_SINGLE (gesture));

  priv = gtk_gesture_single_get_instance_private (gesture);

  if (priv->button == button)
    return;

  priv->button = button;
  g_object_notify_by_pspec (G_OBJECT (gesture), properties[PROP_BUTTON]);
}

/**
 * gtk_gesture_single_get_current_button:
 * @gesture: a `GtkGestureSingle`
 *
 * Returns the button number currently interacting
 * with @gesture, or 0 if there is none.
 *
 * Returns: The current button number
 */
guint
gtk_gesture_single_get_current_button (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), 0);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->current_button;
}

/**
 * gtk_gesture_single_get_current_sequence:
 * @gesture: a `GtkGestureSingle`
 *
 * Returns the event sequence currently interacting with @gesture.
 *
 * This is only meaningful if [method@Gtk.Gesture.is_active]
 * returns %TRUE.
 *
 * Returns: (nullable): the current sequence
 */
GdkEventSequence *
gtk_gesture_single_get_current_sequence (GtkGestureSingle *gesture)
{
  GtkGestureSinglePrivate *priv;

  g_return_val_if_fail (GTK_IS_GESTURE_SINGLE (gesture), NULL);

  priv = gtk_gesture_single_get_instance_private (gesture);

  return priv->current_sequence;
}
