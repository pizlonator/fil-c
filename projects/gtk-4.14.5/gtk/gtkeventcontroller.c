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
 * GtkEventController:
 *
 * `GtkEventController` is the base class for event controllers.
 *
 * These are ancillary objects associated to widgets, which react
 * to `GdkEvents`, and possibly trigger actions as a consequence.
 *
 * Event controllers are added to a widget with
 * [method@Gtk.Widget.add_controller]. It is rarely necessary to
 * explicitly remove a controller with [method@Gtk.Widget.remove_controller].
 *
 * See the chapter on [input handling](input-handling.html) for
 * an overview of the basic concepts, such as the capture and bubble
 * phases of event propagation.
 */

#include "config.h"
#include "gtkeventcontroller.h"
#include "gtkeventcontrollerprivate.h"

#include "gtkwidgetprivate.h"
#include "gtktypebuiltins.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"
#include "gtknative.h"

typedef struct _GtkEventControllerPrivate GtkEventControllerPrivate;

enum {
  PROP_WIDGET = 1,
  PROP_PROPAGATION_PHASE,
  PROP_PROPAGATION_LIMIT,
  PROP_NAME,
  LAST_PROP
};

static GParamSpec *properties[LAST_PROP] = { NULL, };

struct _GtkEventControllerPrivate
{
  GtkWidget *widget;
  GtkPropagationPhase phase;
  GtkPropagationLimit limit;
  char *name;
  GtkWidget *target;
  GdkEvent *event;
  unsigned int name_is_static : 1;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GtkEventController, gtk_event_controller, G_TYPE_OBJECT)

static void
gtk_event_controller_set_widget (GtkEventController *self,
                                 GtkWidget          *widget)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  priv->widget = widget;
}

static void
gtk_event_controller_unset_widget (GtkEventController *self)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  priv->widget = NULL;
}

static gboolean
gtk_event_controller_filter_event_default (GtkEventController *self,
                                           GdkEvent           *event)
{
  return FALSE;
}

static gboolean
gtk_event_controller_handle_event_default (GtkEventController *self,
                                           GdkEvent           *event,
                                           double              x,
                                           double              y)
{
  return FALSE;
}

static void
gtk_event_controller_handle_crossing_default (GtkEventController    *self,
                                              const GtkCrossingData *crossing,
                                              double                 x,
                                              double                 y)
{
}

static void
gtk_event_controller_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PROPAGATION_PHASE:
      gtk_event_controller_set_propagation_phase (self,
                                                  g_value_get_enum (value));
      break;
    case PROP_PROPAGATION_LIMIT:
      gtk_event_controller_set_propagation_limit (self,
                                                  g_value_get_enum (value));
      break;
    case PROP_NAME:
      g_free (priv->name);
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_WIDGET:
      g_value_set_object (value, priv->widget);
      break;
    case PROP_PROPAGATION_PHASE:
      g_value_set_enum (value, priv->phase);
      break;
    case PROP_PROPAGATION_LIMIT:
      g_value_set_enum (value, priv->limit);
      break;
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_event_controller_finalize (GObject *object)
{
  GtkEventController *self = GTK_EVENT_CONTROLLER (object);
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (self);

  if (!priv->name_is_static)
    g_free (priv->name);

  G_OBJECT_CLASS (gtk_event_controller_parent_class)->finalize (object);
}

static void
gtk_event_controller_class_init (GtkEventControllerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->set_widget = gtk_event_controller_set_widget;
  klass->unset_widget = gtk_event_controller_unset_widget;
  klass->filter_event = gtk_event_controller_filter_event_default;
  klass->handle_event = gtk_event_controller_handle_event_default;
  klass->handle_crossing = gtk_event_controller_handle_crossing_default;

  object_class->finalize = gtk_event_controller_finalize;
  object_class->set_property = gtk_event_controller_set_property;
  object_class->get_property = gtk_event_controller_get_property;

  /**
   * GtkEventController:widget: (attributes org.gtk.Property.get=gtk_event_controller_get_widget)
   *
   * The widget receiving the `GdkEvents` that the controller will handle.
   */
  properties[PROP_WIDGET] =
      g_param_spec_object ("widget", NULL, NULL,
                           GTK_TYPE_WIDGET,
                           GTK_PARAM_READABLE);

  /**
   * GtkEventController:propagation-phase: (attributes org.gtk.Property.get=gtk_event_controller_get_propagation_phase org.gtk.Property.set=gtk_event_controller_set_propagation_phase)
   *
   * The propagation phase at which this controller will handle events.
   */
  properties[PROP_PROPAGATION_PHASE] =
      g_param_spec_enum ("propagation-phase", NULL, NULL,
                         GTK_TYPE_PROPAGATION_PHASE,
                         GTK_PHASE_BUBBLE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEventController:propagation-limit: (attributes org.gtk.Property.get=gtk_event_controller_get_propagation_limit org.gtk.Property.set=gtk_event_controller_set_propagation_limit)
   *
   * The limit for which events this controller will handle.
   */
  properties[PROP_PROPAGATION_LIMIT] =
      g_param_spec_enum ("propagation-limit", NULL, NULL,
                         GTK_TYPE_PROPAGATION_LIMIT,
                         GTK_LIMIT_SAME_NATIVE,
                         GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkEventController:name: (attributes org.gtk.Property.get=gtk_event_controller_get_name org.gtk.Property.set=gtk_event_controller_set_name)
   *
   * The name for this controller, typically used for debugging purposes.
   */
  properties[PROP_NAME] =
      g_param_spec_string ("name", NULL, NULL,
                           NULL,
                           GTK_PARAM_READWRITE);

  g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
gtk_event_controller_init (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  priv = gtk_event_controller_get_instance_private (controller);
  priv->phase = GTK_PHASE_BUBBLE;
  priv->limit = GTK_LIMIT_SAME_NATIVE;
}

static gboolean
same_native (GtkWidget *widget,
             GtkWidget *target)
{
  GtkNative *native;
  GtkNative *native2;

  if (!widget || !target)
    return TRUE;

  native = gtk_widget_get_native (widget);
  native2 = gtk_widget_get_native (target);

  return native == native2;
}

static gboolean
gtk_event_controller_filter_event (GtkEventController *controller,
                                   GdkEvent           *event,
                                   GtkWidget          *target)
{
  GtkEventControllerPrivate *priv;
  GtkEventControllerClass *controller_class;

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->widget && !gtk_widget_is_sensitive (priv->widget))
    return TRUE;

  if (priv->limit == GTK_LIMIT_SAME_NATIVE &&
      !same_native (priv->widget, target))
    return TRUE;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  return controller_class->filter_event (controller, event);
}

static gboolean
gtk_event_controller_filter_crossing (GtkEventController    *controller,
                                      const GtkCrossingData *data)
{
  GtkEventControllerPrivate *priv;
  GtkWidget *old_target, *new_target;

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->widget && !gtk_widget_is_sensitive (priv->widget))
    return TRUE;

  old_target = data->old_target;
  new_target = data->new_target;

  if (priv->limit == GTK_LIMIT_SAME_NATIVE)
    {
      /* treat out-of-scope targets like NULL */

      if (!same_native (priv->widget, old_target))
        old_target = NULL;

      if (!same_native (priv->widget, new_target))
        new_target = NULL;
    }

  if (old_target == NULL && new_target == NULL)
    return TRUE;

  return FALSE;
}

/*< private >
 * gtk_event_controller_handle_event:
 * @controller: a `GtkEventController`
 * @event: a `GdkEvent`
 * @target: the target widget
 * @x: event position in widget coordinates, or 0 if not a pointer event
 * @y: event position in widget coordinates, or 0 if not a pointer event
 *
 * Feeds an event into @controller, so it can be interpreted
 * and the controller actions triggered.
 *
 * Returns: %TRUE if the event was potentially useful to trigger the
 *   controller action
 */
gboolean
gtk_event_controller_handle_event (GtkEventController *controller,
                                   GdkEvent           *event,
                                   GtkWidget          *target,
                                   double              x,
                                   double              y)
{
  GtkEventControllerClass *controller_class;
  GtkEventControllerPrivate *priv;
  gboolean retval = FALSE;

  priv = gtk_event_controller_get_instance_private (controller);

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gtk_event_controller_filter_event (controller, event, target))
    return retval;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  priv->target = g_object_ref (target);
  priv->event = gdk_event_ref (event);

  g_object_ref (controller);
  retval = controller_class->handle_event (controller, event, x, y);

  g_clear_object (&priv->target);
  g_clear_pointer (&priv->event, gdk_event_unref);
  g_object_unref (controller);

  return retval;
}

/*< private >
 * gtk_event_controller_handle_crossing:
 * @controller: a `GtkEventController`
 * @crossing: a `GtkCrossingData`
 * @x: translated event coordinates
 * @y: translated event coordinates
 *
 * Feeds a crossing event into @controller, so it can be interpreted
 * and the controller actions triggered.
 */
void
gtk_event_controller_handle_crossing (GtkEventController    *controller,
                                      const GtkCrossingData *crossing,
                                      double                 x,
                                      double                 y)
{
  GtkEventControllerClass *controller_class;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (crossing != NULL);

  if (gtk_event_controller_filter_crossing (controller, crossing))
    return;

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  g_object_ref (controller);
  controller_class->handle_crossing (controller, crossing, x, y);
  g_object_unref (controller);
}

/**
 * gtk_event_controller_get_widget: (attributes org.gtk.Method.get_property=widget)
 * @controller: a `GtkEventController`
 *
 * Returns the `GtkWidget` this controller relates to.
 *
 * Returns: (transfer none): a `GtkWidget`
 **/
GtkWidget *
gtk_event_controller_get_widget (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), 0);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->widget;
}

/**
 * gtk_event_controller_reset:
 * @controller: a `GtkEventController`
 *
 * Resets the @controller to a clean state.
 */
void
gtk_event_controller_reset (GtkEventController *controller)
{
  GtkEventControllerClass *controller_class;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  controller_class = GTK_EVENT_CONTROLLER_GET_CLASS (controller);

  if (controller_class->reset)
    controller_class->reset (controller);
}

/**
 * gtk_event_controller_get_propagation_phase: (attributes org.gtk.Method.get_property=propagation-phase)
 * @controller: a `GtkEventController`
 *
 * Gets the propagation phase at which @controller handles events.
 *
 * Returns: the propagation phase
 */
GtkPropagationPhase
gtk_event_controller_get_propagation_phase (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), GTK_PHASE_NONE);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->phase;
}

/**
 * gtk_event_controller_set_propagation_phase: (attributes org.gtk.Method.set_property=propagation-phase)
 * @controller: a `GtkEventController`
 * @phase: a propagation phase
 *
 * Sets the propagation phase at which a controller handles events.
 *
 * If @phase is %GTK_PHASE_NONE, no automatic event handling will be
 * performed, but other additional gesture maintenance will.
 */
void
gtk_event_controller_set_propagation_phase (GtkEventController  *controller,
                                            GtkPropagationPhase  phase)
{
  GtkEventControllerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));
  g_return_if_fail (phase >= GTK_PHASE_NONE && phase <= GTK_PHASE_TARGET);

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->phase == phase)
    return;

  priv->phase = phase;

  if (phase == GTK_PHASE_NONE)
    gtk_event_controller_reset (controller);

  g_object_notify_by_pspec (G_OBJECT (controller), properties[PROP_PROPAGATION_PHASE]);
}

/**
 * gtk_event_controller_get_propagation_limit: (attributes org.gtk.Method.get_property=propagation-limit)
 * @controller: a `GtkEventController`
 *
 * Gets the propagation limit of the event controller.
 *
 * Returns: the propagation limit
 */
GtkPropagationLimit
gtk_event_controller_get_propagation_limit (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv;

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), GTK_LIMIT_SAME_NATIVE);

  priv = gtk_event_controller_get_instance_private (controller);

  return priv->limit;
}

/**
 * gtk_event_controller_set_propagation_limit: (attributes org.gtk.Method.set_property=propagation-limit)
 * @controller: a `GtkEventController`
 * @limit: the propagation limit
 *
 * Sets the event propagation limit on the event controller.
 *
 * If the limit is set to %GTK_LIMIT_SAME_NATIVE, the controller
 * won't handle events that are targeted at widgets on a different
 * surface, such as popovers.
 */
void
gtk_event_controller_set_propagation_limit (GtkEventController  *controller,
                                            GtkPropagationLimit  limit)
{
  GtkEventControllerPrivate *priv;

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  priv = gtk_event_controller_get_instance_private (controller);

  if (priv->limit == limit)
    return;

  priv->limit = limit;

  g_object_notify_by_pspec (G_OBJECT (controller), properties[PROP_PROPAGATION_LIMIT]);
}

/**
 * gtk_event_controller_get_name: (attributes org.gtk.Method.get_property=name)
 * @controller: a `GtkEventController`
 *
 * Gets the name of @controller.
 * 
 * Returns: (nullable): The controller name
 */
const char *
gtk_event_controller_get_name (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  g_return_val_if_fail (GTK_IS_EVENT_CONTROLLER (controller), NULL);

  return priv->name;
}

/**
 * gtk_event_controller_set_name: (attributes org.gtk.Method.set_property=name)
 * @controller: a `GtkEventController`
 * @name: (nullable): a name for @controller
 *
 * Sets a name on the controller that can be used for debugging.
 */
void
gtk_event_controller_set_name (GtkEventController *controller,
                               const char         *name)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  if (!priv->name_is_static)
    g_free (priv->name);
  priv->name = g_strdup (name);
  priv->name_is_static = FALSE;
}

/**
 * gtk_event_controller_set_static_name:
 * @controller: a `GtkEventController`
 * @name: (nullable): a name for @controller, must be a static string
 *
 * Sets a name on the controller that can be used for debugging.
 *
 * Since: 4.8
 */
void
gtk_event_controller_set_static_name (GtkEventController *controller,
                                      const char         *name)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  g_return_if_fail (GTK_IS_EVENT_CONTROLLER (controller));

  if (!priv->name_is_static)
    g_free (priv->name);
  priv->name = (char *)name;
  priv->name_is_static = TRUE;
}

GtkWidget *
gtk_event_controller_get_target (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  return priv->target;
}

/**
 * gtk_event_controller_get_current_event:
 * @controller: a `GtkEventController`
 *
 * Returns the event that is currently being handled by the controller.
 *
 * At other times, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): the event that is currently
 *   handled by @controller
 */
GdkEvent *
gtk_event_controller_get_current_event (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  return priv->event;
}

/**
 * gtk_event_controller_get_current_event_time:
 * @controller: a `GtkEventController`
 *
 * Returns the timestamp of the event that is currently being
 * handled by the controller.
 *
 * At other times, 0 is returned.
 *
 * Returns: timestamp of the event is currently handled by @controller
 */
guint32
gtk_event_controller_get_current_event_time (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  if (priv->event)
    return gdk_event_get_time (priv->event);

  return 0;
}

/**
 * gtk_event_controller_get_current_event_device:
 * @controller: a `GtkEventController`
 *
 * Returns the device of the event that is currently being
 * handled by the controller.
 *
 * At other times, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): device of the event is
 *   currently handled by @controller
 */
GdkDevice *
gtk_event_controller_get_current_event_device (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  if (priv->event)
    return gdk_event_get_device (priv->event);

  return NULL;
}

/**
 * gtk_event_controller_get_current_event_state:
 * @controller: a `GtkEventController`
 *
 * Returns the modifier state of the event that is currently being
 * handled by the controller.
 *
 * At other times, 0 is returned.
 *
 * Returns: modifier state of the event is currently handled by @controller
 */
GdkModifierType
gtk_event_controller_get_current_event_state (GtkEventController *controller)
{
  GtkEventControllerPrivate *priv = gtk_event_controller_get_instance_private (controller);

  if (priv->event)
    return gdk_event_get_modifier_state (priv->event);

  return 0;
}
