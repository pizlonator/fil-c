/*
 * Copyright © 2020 Benjamin Otte
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
 * GtkDropControllerMotion:
 *
 * `GtkDropControllerMotion` is an event controller tracking
 * the pointer during Drag-and-Drop operations.
 *
 * It is modeled after [class@Gtk.EventControllerMotion] so if you
 * have used that, this should feel really familiar.
 *
 * This controller is not able to accept drops, use [class@Gtk.DropTarget]
 * for that purpose.
 */

#include "config.h"

#include "gtkdropcontrollermotion.h"

#include "gtkprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkmarshalers.h"
#include "gtkeventcontrollerprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"

struct _GtkDropControllerMotion
{
  GtkEventController parent_instance;

  GdkDrop *drop;
  guint is_pointer             : 1;
  guint contains_pointer       : 1;
};

struct _GtkDropControllerMotionClass
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
  PROP_0,
  PROP_CONTAINS_POINTER,
  PROP_DROP,
  PROP_IS_POINTER,
  NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE (GtkDropControllerMotion, gtk_drop_controller_motion, GTK_TYPE_EVENT_CONTROLLER)

static gboolean
gtk_drop_controller_motion_handle_event (GtkEventController *controller,
                                         GdkEvent           *event,
                                         double              x,
                                         double              y)
{
  GtkEventControllerClass *parent_class;
  GdkEventType type;

  type = gdk_event_get_event_type (event);
  if (type == GDK_DRAG_MOTION)
    g_signal_emit (controller, signals[MOTION], 0, x, y);

  parent_class = GTK_EVENT_CONTROLLER_CLASS (gtk_drop_controller_motion_parent_class);

  return parent_class->handle_event (controller, event, x, y);
}

static void
update_pointer_focus (GtkEventController    *controller,
                      const GtkCrossingData *crossing,
                      double                 x,
                      double                 y)
{
  GtkDropControllerMotion *self = GTK_DROP_CONTROLLER_MOTION (controller);
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

  if (self->contains_pointer != contains_pointer)
    {
      enter = contains_pointer;
      leave = !contains_pointer;
    }

  if (leave)
    g_signal_emit (controller, signals[LEAVE], 0);

  g_object_freeze_notify (G_OBJECT (self));
  if (self->is_pointer != is_pointer)
    {
      self->is_pointer = is_pointer;
      g_object_notify (G_OBJECT (self), "is-pointer");
    }
  if (self->contains_pointer != contains_pointer)
    {
      self->contains_pointer = contains_pointer;
      if (contains_pointer)
        self->drop = g_object_ref (crossing->drop);
      else
        g_clear_object (&self->drop);
      g_object_notify (G_OBJECT (self), "contains-pointer");
      g_object_notify (G_OBJECT (self), "drop");
    }
  g_object_thaw_notify (G_OBJECT (self));

  if (enter)
    g_signal_emit (controller, signals[ENTER], 0, x, y);
}

static void
gtk_drop_controller_motion_handle_crossing (GtkEventController    *controller,
                                            const GtkCrossingData *crossing,
                                            double                 x,
                                            double                 y)
{
  if (crossing->type == GTK_CROSSING_DROP)
    update_pointer_focus (controller, crossing, x, y);
}

static void
gtk_drop_controller_motion_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GtkDropControllerMotion *self = GTK_DROP_CONTROLLER_MOTION (object);

  switch (prop_id)
    {
    case PROP_CONTAINS_POINTER:
      g_value_set_boolean (value, self->contains_pointer);
      break;

    case PROP_DROP:
      g_value_set_object (value, self->drop);
      break;

    case PROP_IS_POINTER:
      g_value_set_boolean (value, self->is_pointer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drop_controller_motion_class_init (GtkDropControllerMotionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (klass);

  object_class->get_property = gtk_drop_controller_motion_get_property;

  controller_class->handle_event = gtk_drop_controller_motion_handle_event;
  controller_class->handle_crossing = gtk_drop_controller_motion_handle_crossing;

  /**
   * GtkDropControllerMotion:contains-pointer: (attributes org.gtk.Property.get=gtk_drop_controller_motion_contains_pointer)
   *
   * Whether the pointer of a Drag-and-Drop operation is in
   * the controller's widget or a descendant.
   *
   * See also [property@Gtk.DropControllerMotion:is-pointer].
   *
   * When handling crossing events, this property is updated
   * before [signal@Gtk.DropControllerMotion::enter], but after
   * [signal@Gtk.DropControllerMotion::leave] is emitted.
   */
  props[PROP_CONTAINS_POINTER] =
      g_param_spec_boolean ("contains-pointer", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE);

  /**
   * GtkDropControllerMotion:drop: (attributes org.gtk.Property.get=gtk_drop_controller_motion_get_drop)
   *
   * The ongoing drop operation over the controller's widget or
   * its descendant.
   *
   * If no drop operation is going on, this property returns %NULL.
   *
   * The event controller should not modify the @drop, but it might
   * want to query its properties.
   *
   * When handling crossing events, this property is updated
   * before [signal@Gtk.DropControllerMotion::enter], but after
   * [signal@Gtk.DropControllerMotion::leave] is emitted.
   */
  props[PROP_DROP] =
      g_param_spec_object ("drop", NULL, NULL,
                           GDK_TYPE_DROP,
                           G_PARAM_READABLE);

  /**
   * GtkDropControllerMotion:is-pointer: (attributes org.gtk.Property.get=gtk_drop_controller_motion_is_pointer)
   *
   * Whether the pointer is in the controllers widget itself,
   * as opposed to in a descendent widget.
   *
   * See also [property@Gtk.DropControllerMotion:contains-pointer].
   *
   * When handling crossing events, this property is updated
   * before [signal@Gtk.DropControllerMotion::enter], but after
   * [signal@Gtk.DropControllerMotion::leave] is emitted.
   */
  props[PROP_IS_POINTER] =
      g_param_spec_boolean ("is-pointer", NULL, NULL,
                            FALSE,
                            G_PARAM_READABLE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  /**
   * GtkDropControllerMotion::enter:
   * @self: the object which received the signal
   * @x: coordinates of pointer location
   * @y: coordinates of pointer location
   *
   * Signals that the pointer has entered the widget.
   */
  signals[ENTER] =
    g_signal_new (I_("enter"),
                  GTK_TYPE_DROP_CONTROLLER_MOTION,
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
   * GtkDropControllerMotion::leave:
   * @self: the object which received the signal
   *
   * Signals that the pointer has left the widget.
   */
  signals[LEAVE] =
    g_signal_new (I_("leave"),
                  GTK_TYPE_DROP_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * GtkDropControllerMotion::motion:
   * @self: The object that received the signal
   * @x: the x coordinate
   * @y: the y coordinate
   *
   * Emitted when the pointer moves inside the widget.
   */
  signals[MOTION] =
    g_signal_new (I_("motion"),
                  GTK_TYPE_DROP_CONTROLLER_MOTION,
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  _gtk_marshal_VOID__DOUBLE_DOUBLE,
                  G_TYPE_NONE, 2,
                  G_TYPE_DOUBLE,
                  G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[MOTION],
                              G_TYPE_FROM_CLASS (klass),
                              _gtk_marshal_VOID__DOUBLE_DOUBLEv);
}

static void
gtk_drop_controller_motion_init (GtkDropControllerMotion *self)
{
}

/**
 * gtk_drop_controller_motion_new:
 *
 * Creates a new event controller that will handle pointer motion
 * events during drag and drop.
 *
 * Returns: a new `GtkDropControllerMotion`
 **/
GtkEventController *
gtk_drop_controller_motion_new (void)
{
  return g_object_new (GTK_TYPE_DROP_CONTROLLER_MOTION,
                       NULL);
}

/**
 * gtk_drop_controller_motion_contains_pointer: (attributes org.gtk.Method.get_property=contains-pointer)
 * @self: a `GtkDropControllerMotion`
 *
 * Returns if a Drag-and-Drop operation is within the widget
 * @self or one of its children.
 *
 * Returns: %TRUE if a dragging pointer is within @self or one of its children.
 */
gboolean
gtk_drop_controller_motion_contains_pointer (GtkDropControllerMotion *self)
{
  g_return_val_if_fail (GTK_IS_DROP_CONTROLLER_MOTION (self), FALSE);

  return self->contains_pointer;
}

/**
 * gtk_drop_controller_motion_get_drop: (attributes org.gtk.Method.get_property=drop)
 * @self: a `GtkDropControllerMotion`
 *
 * Returns the `GdkDrop` of a current Drag-and-Drop operation
 * over the widget of @self.
 *
 * Returns: (transfer none) (nullable): The `GdkDrop` currently
 *   happening within @self
 */
GdkDrop *
gtk_drop_controller_motion_get_drop (GtkDropControllerMotion *self)
{
  g_return_val_if_fail (GTK_IS_DROP_CONTROLLER_MOTION (self), FALSE);

  return self->drop;
}

/**
 * gtk_drop_controller_motion_is_pointer: (attributes org.gtk.Method.get_property=is-pointer)
 * @self: a `GtkDropControllerMotion`
 *
 * Returns if a Drag-and-Drop operation is within the widget
 * @self, not one of its children.
 *
 * Returns: %TRUE if a dragging pointer is within @self but
 *   not one of its children
 */
gboolean
gtk_drop_controller_motion_is_pointer (GtkDropControllerMotion *self)
{
  g_return_val_if_fail (GTK_IS_DROP_CONTROLLER_MOTION (self), FALSE);

  return self->is_pointer;
}
