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
 * Author(s): Matthias Clasen <mclasen@redhat.com>
 */

/**
 * GtkEventControllerMotion:
 *
 * `GtkEventControllerMotion` is an event controller tracking the pointer
 * position.
 *
 * The event controller offers [signal@Gtk.EventControllerMotion::enter]
 * and [signal@Gtk.EventControllerMotion::leave] signals, as well as
 * [property@Gtk.EventControllerMotion:is-pointer] and
 * [property@Gtk.EventControllerMotion:contains-pointer] properties
 * which are updated to reflect changes in the pointer position as it
 * moves over the widget.
 */
#include "config.h"

#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerprivate.h"
#include "gtkeventcontrollermotion.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"

struct _GtkEventControllerMotion
{
  GtkEventController parent_instance;

  guint is_pointer             : 1;
  guint contains_pointer       : 1;
};

struct _GtkEventControllerMotionClass
{
  GtkEventControllerClass parent_class;
};

enum {
  ENTER,
  LEAVE,
  MOTION,
  N_SIGNALS
};

enum {
  PROP_IS_POINTER = 1,
  PROP_CONTAINS_POINTER,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkEventControllerMotion, gtk_event_controller_motion, GTK_TYPE_EVENT_CONTROLLER)

static gboolean
gtk_event_controller_motion_handle_event (GtkEventController *controller,
                                          GdkEvent           *event,
                                          double              x,
                                          double              y)
{
  GtkEventControllerClass *parent_class;
  GdkEventType type;

  type = gdk_event_get_event_type (event);
  if (type == GDK_MOTION_NOTIFY)
    g_signal_emit (controller, signals[MOTION], 0, x, y);

  parent_class = GTK_EVENT_CONTROLLER_CLASS (gtk_event_controller_motion_parent_class);

  return parent_class->handle_event (controller, event, x, y);
}

static void
update_pointer_focus (GtkEventController    *controller,
                      const GtkCrossingData *crossing,
                      double                 x,
                      double                 y)
{
  GtkEventControllerMotion *motion = GTK_EVENT_CONTROLLER_MOTION (controller);
  GtkWidget *widget = gtk_event_controller_get_widget (controller);
  gboolean is_pointer = FALSE;
  gboolean contains_pointer = FALSE;
  gboolean enter = FALSE;
  gboolean leave = FALSE;

  if (crossing->direction == GTK_CROSSING_IN)
    {
     if (crossing->new_descendent != NULL)
        {
          contains_pointer = TRUE;
        }
      if (crossing->new_target == widget)
        {
          contains_pointer = TRUE;
          is_pointer = TRUE;
        }
    }
  else
    {
      if (crossing->new_descendent != NULL ||
          crossing->new_target == widget)
        contains_pointer = TRUE;
      is_pointer = FALSE;
    }

  if (motion->contains_pointer != contains_pointer)
    {
      enter = contains_pointer;
      leave = !contains_pointer;
    }

  if (leave)
    g_signal_emit (controller, signals[LEAVE], 0);

  g_object_freeze_notify (G_OBJECT (motion));
  if (motion->is_pointer != is_pointer)
    {
      motion->is_pointer = is_pointer;
      g_object_notify_by_pspec (G_OBJECT (motion), props[PROP_IS_POINTER]);
    }
  if (motion->contains_pointer != contains_pointer)
    {
      motion->contains_pointer = contains_pointer;
      g_object_notify_by_pspec (G_OBJECT (motion), props[PROP_CONTAINS_POINTER]);
    }
  g_object_thaw_notify (G_OBJECT (motion));

  if (enter)
    g_signal_emit (controller, signals[ENTER], 0, x, y);
}

static void
gtk_event_controller_motion_handle_crossing (GtkEventController    *controller,
                                             const GtkCrossingData *crossing,
                                             double                 x,
                                             double                 y)
{
  if (crossing->type == GTK_CROSSING_POINTER)
    update_pointer_focus (controller, crossing, x, y);
}

static void
gtk_event_controller_motion_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GtkEventControllerMotion *controller = GTK_EVENT_CONTROLLER_MOTION (object);

  switch (prop_id)
    {
    case PROP_IS_POINTER:
      g_value_set_boolean (value, controller->is_pointer);
      break;

    case PROP_CONTAINS_POINTER:
      g_value_set_boolean (value, controller->contains_pointer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_motion_class_init (GtkEventControllerMotionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_event_controller_motion_get_property;

  controller_class->handle_event = gtk_event_controller_motion_handle_event;
  controller_class->handle_crossing = gtk_event_controller_motion_handle_crossing;

  /**
   * GtkEventControllerMotion:is-pointer: (attributes org.gtk.Property.get=gtk_event_controller_motion_is_pointer)
   *
   * Whether the pointer is in the controllers widget itself,
   * as opposed to in a descendent widget.
   *
   * See also [property@Gtk.EventControllerMotion:contains-pointer].
   *
   * When handling crossing events, this property is updated
   * before [signal@Gtk.EventControllerMotion::enter], but after
   * [signal@Gtk.EventControllerMotion::leave] is emitted.
   */
  props[PROP_IS_POINTER] =
      g_param_spec_boolean ("is-pointer", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE);

  /**
   * GtkEventControllerMotion:contains-pointer: (attributes org.gtk.Property.get=gtk_event_controller_motion_contains_pointer)
   *
   * Whether the pointer is in the controllers widget or a descendant.
   *
   * See also [property@Gtk.EventControllerMotion:is-pointer].
   *
   * When handling crossing events, this property is updated
   * before [signal@Gtk.EventControllerMotion::enter], but after
   * [signal@Gtk.EventControllerMotion::leave] is emitted.
   */
  props[PROP_CONTAINS_POINTER] =
      g_param_spec_boolean ("contains-pointer", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  /**
   * GtkEventControllerMotion::enter:
   * @controller: the object which received the signal
   * @x: coordinates of pointer location
   * @y: coordinates of pointer location
   *
   * Signals that the pointer has entered the widget.
   */
  signals[ENTER] =
    g_signal_new (I_("enter"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[ENTER],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);

  /**
   * GtkEventControllerMotion::leave:
   * @controller: the object which received the signal
   *
   * Signals that the pointer has left the widget.
   */
  signals[LEAVE] =
    g_signal_new (I_("leave"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkEventControllerMotion::motion:
   * @controller: The object that received the signal
   * @x: the x coordinate
   * @y: the y coordinate
   *
   * Emitted when the pointer moves inside the widget.
   */
  signals[MOTION] =
    g_signal_new (I_("motion"),
                  GTK_TYPE_EVENT_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[MOTION],
                              GTK_TYPE_EVENT_CONTROLLER_MOTION,
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_event_controller_motion_init (GtkEventControllerMotion *motion)
{
}

/**
 * gtk_event_controller_motion_new:
 *
 * Creates a new event controller that will handle motion events.
 *
 * Returns: a new `GtkEventControllerMotion`
 **/
GtkEventController *
gtk_event_controller_motion_new (void)
{
  return g_object_new (GTK_TYPE_EVENT_CONTROLLER_MOTION,
                       NULL);
}

/**
 * gtk_event_controller_motion_contains_pointer: (attributes org.gtk.Method.get_property=contains-pointer)
 * @self: a `GtkEventControllerMotion`
 *
 * Returns if a pointer is within @self or one of its children.
 *
 * Returns: %TRUE if a pointer is within @self or one of its children
 */
gboolean
gtk_event_controller_motion_contains_pointer (GtkEventControllerMotion *self)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_MOTION (self), FALSE);

  return self->contains_pointer;
}

/**
 * gtk_event_controller_motion_is_pointer: (attributes org.gtk.Method.get_property=is-pointer)
 * @self: a `GtkEventControllerMotion`
 *
 * Returns if a pointer is within @self, but not one of its children.
 *
 * Returns: %TRUE if a pointer is within @self but not one of its children
 */
gboolean
gtk_event_controller_motion_is_pointer (GtkEventControllerMotion *self)
{
  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER_MOTION (self), FALSE);

  return self->is_pointer;
}
