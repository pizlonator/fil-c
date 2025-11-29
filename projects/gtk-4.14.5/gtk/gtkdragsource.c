/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include "gtkdragsourceprivate.h"

#include "gtkgesturedrag.h"
#include "gtkgesturesingleprivate.h"
#include "gtkimagedefinitionprivate.h"
#include "gtknative.h"
#include "gtkwidgetprivate.h"
#include "gtkimageprivate.h"
#include "gtkdragicon.h"
#include "gtkprivate.h"
#include "gtkmarshalers.h"
#include "gtkpicture.h"
#include "gtksettingsprivate.h"
#include "gtkgesturesingle.h"

#define MIN_TIME_TO_DND 100

/**
 * GtkDragSource:
 *
 * `GtkDragSource` is an event controller to initiate Drag-And-Drop operations.
 *
 * `GtkDragSource` can be set up with the necessary
 * ingredients for a DND operation ahead of time. This includes
 * the source for the data that is being transferred, in the form
 * of a [class@Gdk.ContentProvider], the desired action, and the icon to
 * use during the drag operation. After setting it up, the drag
 * source must be added to a widget as an event controller, using
 * [method@Gtk.Widget.add_controller].
 *
 * ```c
 * static void
 * my_widget_init (MyWidget *self)
 * {
 *   GtkDragSource *drag_source = gtk_drag_source_new ();
 *
 *   g_signal_connect (drag_source, "prepare", G_CALLBACK (on_drag_prepare), self);
 *   g_signal_connect (drag_source, "drag-begin", G_CALLBACK (on_drag_begin), self);
 *
 *   gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (drag_source));
 * }
 * ```
 *
 * Setting up the content provider and icon ahead of time only makes
 * sense when the data does not change. More commonly, you will want
 * to set them up just in time. To do so, `GtkDragSource` has
 * [signal@Gtk.DragSource::prepare] and [signal@Gtk.DragSource::drag-begin]
 * signals.
 *
 * The ::prepare signal is emitted before a drag is started, and
 * can be used to set the content provider and actions that the
 * drag should be started with.
 *
 * ```c
 * static GdkContentProvider *
 * on_drag_prepare (GtkDragSource *source,
 *                  double         x,
 *                  double         y,
 *                  MyWidget      *self)
 * {
 *   // This widget supports two types of content: GFile objects
 *   // and GdkPixbuf objects; GTK will handle the serialization
 *   // of these types automatically
 *   GFile *file = my_widget_get_file (self);
 *   GdkPixbuf *pixbuf = my_widget_get_pixbuf (self);
 *
 *   return gdk_content_provider_new_union ((GdkContentProvider *[2]) {
 *       gdk_content_provider_new_typed (G_TYPE_FILE, file),
 *       gdk_content_provider_new_typed (GDK_TYPE_PIXBUF, pixbuf),
 *     }, 2);
 * }
 * ```
 *
 * The ::drag-begin signal is emitted after the `GdkDrag` object has
 * been created, and can be used to set up the drag icon.
 *
 * ```c
 * static void
 * on_drag_begin (GtkDragSource *source,
 *                GdkDrag       *drag,
 *                MyWidget      *self)
 * {
 *   // Set the widget as the drag icon
 *   GdkPaintable *paintable = gtk_widget_paintable_new (GTK_WIDGET (self));
 *   gtk_drag_source_set_icon (source, paintable, 0, 0);
 *   g_object_unref (paintable);
 * }
 * ```
 *
 * During the DND operation, `GtkDragSource` emits signals that
 * can be used to obtain updates about the status of the operation,
 * but it is not normally necessary to connect to any signals,
 * except for one case: when the supported actions include
 * %GDK_ACTION_MOVE, you need to listen for the
 * [signal@Gtk.DragSource::drag-end] signal and delete the
 * data after it has been transferred.
 */

struct _GtkDragSource
{
  GtkGestureSingle parent_instance;

  GdkContentProvider *content;
  GdkDragAction actions;

  GdkPaintable *paintable;
  int hot_x;
  int hot_y;

  double start_x;
  double start_y;

  guint timeout_id;

  GdkDrag *drag;
};

struct _GtkDragSourceClass
{
  GtkGestureSingleClass parent_class;

  GdkContentProvider *(* prepare) (GtkDragSource *source,
                                  double         x,
                                  double         y);
};

enum {
  PROP_CONTENT = 1,
  PROP_ACTIONS,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

enum {
  PREPARE,
  DRAG_BEGIN,
  DRAG_END,
  DRAG_CANCEL,
  NUM_SIGNALS
};

static guint signals[NUM_SIGNALS];

static void gtk_drag_source_dnd_finished_cb   (GdkDrag             *drag,
                                               GtkDragSource       *source);
static void gtk_drag_source_cancel_cb         (GdkDrag             *drag,
                                               GdkDragCancelReason  reason,
                                               GtkDragSource       *source);

static GdkContentProvider *gtk_drag_source_prepare (GtkDragSource *source,
                                                    double         x,
                                                    double         y);

static void gtk_drag_source_drag_begin (GtkDragSource *source);

G_DEFINE_TYPE (GtkDragSource, gtk_drag_source, GTK_TYPE_GESTURE_SINGLE);

static void
gtk_drag_source_init (GtkDragSource *source)
{
  source->actions = GDK_ACTION_COPY;
}

static void
gtk_drag_source_finalize (GObject *object)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  g_clear_object (&source->content);
  g_clear_object (&source->paintable);
  g_clear_handle_id (&source->timeout_id, g_source_remove);

  G_OBJECT_CLASS (gtk_drag_source_parent_class)->finalize (object);
}

static void
gtk_drag_source_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      gtk_drag_source_set_content (source, g_value_get_object (value));
      break;

    case PROP_ACTIONS:
      gtk_drag_source_set_actions (source, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_drag_source_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (object);

  switch (prop_id)
    {
    case PROP_CONTENT:
      g_value_set_object (value, gtk_drag_source_get_content (source));
      break;

    case PROP_ACTIONS:
      g_value_set_flags (value, gtk_drag_source_get_actions (source));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gboolean
gtk_drag_source_filter_event (GtkEventController *controller,
                              GdkEvent           *event)
{
  /* Let touchpad swipe events go through, only if they match n-points  */
  if (gdk_event_get_event_type (event) == GDK_TOUCHPAD_SWIPE)
    {
      guint n_points;
      guint n_fingers;

      g_object_get (G_OBJECT (controller), "n-points", &n_points, NULL);
      n_fingers = gdk_touchpad_event_get_n_fingers (event);

      if (n_fingers == n_points)
        return FALSE;
      else
        return TRUE;
    }

  return GTK_EVENT_CONTROLLER_CLASS (gtk_drag_source_parent_class)->filter_event (controller, event);
}

static gboolean
drag_timeout (gpointer user_data)
{
  GtkDragSource *source = user_data;

  source->timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
gtk_drag_source_begin (GtkGesture       *gesture,
                       GdkEventSequence *sequence)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (gesture);
  GdkEventSequence *current;

  current = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  source->timeout_id = g_timeout_add (MIN_TIME_TO_DND, drag_timeout, source);

  gtk_gesture_get_point (gesture, current, &source->start_x, &source->start_y);
}

static void
gtk_drag_source_update (GtkGesture       *gesture,
                        GdkEventSequence *sequence)
{
  GtkDragSource *source = GTK_DRAG_SOURCE (gesture);
  GtkWidget *widget;
  double x, y;

  if (!gtk_gesture_is_recognized (gesture))
    return;

  gtk_gesture_get_point (gesture, sequence, &x, &y);

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));

  if (gtk_drag_check_threshold_double (widget, source->start_x, source->start_y, x, y) &&
      !source->timeout_id)
    {
      gtk_drag_source_drag_begin (source);
    }
}

static void
gtk_drag_source_class_init (GtkDragSourceClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkEventControllerClass *controller_class = GTK_EVENT_CONTROLLER_CLASS (class);
  GtkGestureClass *gesture_class = GTK_GESTURE_CLASS (class);

  object_class->finalize = gtk_drag_source_finalize;
  object_class->set_property = gtk_drag_source_set_property;
  object_class->get_property = gtk_drag_source_get_property;

  controller_class->filter_event = gtk_drag_source_filter_event;

  gesture_class->begin = gtk_drag_source_begin;
  gesture_class->update = gtk_drag_source_update;
  gesture_class->end = NULL;

  class->prepare = gtk_drag_source_prepare;

  /**
   * GtkDragSource:content: (attributes org.gtk.Property.get=gtk_drag_source_get_content org.gtk.Property.set=gtk_drag_source_set_content)
   *
   * The data that is offered by drag operations from this source.
   */
  properties[PROP_CONTENT] =
       g_param_spec_object ("content", NULL, NULL,
                           GDK_TYPE_CONTENT_PROVIDER,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkDragSource:actions: (attributes org.gtk.Property.get=gtk_drag_source_get_actions org.gtk.Property.set=gtk_drag_source_set_actions)
   *
   * The actions that are supported by drag operations from the source.
   *
   * Note that you must handle the [signal@Gtk.DragSource::drag-end] signal
   * if the actions include %GDK_ACTION_MOVE.
   */
  properties[PROP_ACTIONS] =
       g_param_spec_flags ("actions", NULL, NULL,
                           GDK_TYPE_DRAG_ACTION, GDK_ACTION_COPY,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * GtkDragSource::prepare:
   * @source: the `GtkDragSource`
   * @x: the X coordinate of the drag starting point
   * @y: the Y coordinate of the drag starting point
   *
   * Emitted when a drag is about to be initiated.
   *
   * It returns the `GdkContentProvider` to use for the drag that is about
   * to start. The default handler for this signal returns the value of
   * the [property@Gtk.DragSource:content] property, so if you set up that
   * property ahead of time, you don't need to connect to this signal.
   *
   * Returns: (transfer full) (nullable): a `GdkContentProvider`
   */
  signals[PREPARE] =
      g_signal_new (I_("prepare"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkDragSourceClass, prepare),
                    g_signal_accumulator_first_wins, NULL,
                    _gtk_marshal_OBJECT__DOUBLE_DOUBLE,
                    GDK_TYPE_CONTENT_PROVIDER, 2,
                    G_TYPE_DOUBLE, G_TYPE_DOUBLE);
  g_signal_set_va_marshaller (signals[PREPARE],
                              GTK_TYPE_DRAG_SOURCE,
                              _gtk_marshal_OBJECT__DOUBLE_DOUBLEv);

  /**
   * GtkDragSource::drag-begin:
   * @source: the `GtkDragSource`
   * @drag: the `GdkDrag` object
   *
   * Emitted on the drag source when a drag is started.
   *
   * It can be used to e.g. set a custom drag icon with
   * [method@Gtk.DragSource.set_icon].
   */
  signals[DRAG_BEGIN] =
      g_signal_new (I_("drag-begin"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    NULL,
                    G_TYPE_NONE, 1,
                    GDK_TYPE_DRAG);

  /**
   * GtkDragSource::drag-end:
   * @source: the `GtkDragSource`
   * @drag: the `GdkDrag` object
   * @delete_data: %TRUE if the drag was performing %GDK_ACTION_MOVE,
   *    and the data should be deleted
   *
   * Emitted on the drag source when a drag is finished.
   *
   * A typical reason to connect to this signal is to undo
   * things done in [signal@Gtk.DragSource::prepare] or
   * [signal@Gtk.DragSource::drag-begin] handlers.
   */
  signals[DRAG_END] =
      g_signal_new (I_("drag-end"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    _gtk_marshal_VOID__OBJECT_BOOLEAN,
                    G_TYPE_NONE, 2,
                    GDK_TYPE_DRAG,
                    G_TYPE_BOOLEAN);
  g_signal_set_va_marshaller (signals[DRAG_END],
                              GTK_TYPE_DRAG_SOURCE,
                              _gtk_marshal_VOID__OBJECT_BOOLEANv);

  /**
   * GtkDragSource::drag-cancel:
   * @source: the `GtkDragSource`
   * @drag: the `GdkDrag` object
   * @reason: information on why the drag failed
   *
   * Emitted on the drag source when a drag has failed.
   *
   * The signal handler may handle a failed drag operation based on
   * the type of error. It should return %TRUE if the failure has been handled
   * and the default "drag operation failed" animation should not be shown.
   *
   * Returns: %TRUE if the failed drag operation has been already handled
   */
  signals[DRAG_CANCEL] =
      g_signal_new (I_("drag-cancel"),
                    G_TYPE_FROM_CLASS (class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    _gtk_boolean_handled_accumulator, NULL,
                    _gtk_marshal_BOOLEAN__OBJECT_ENUM,
                    G_TYPE_BOOLEAN, 2,
                    GDK_TYPE_DRAG,
                    GDK_TYPE_DRAG_CANCEL_REASON);
  g_signal_set_va_marshaller (signals[DRAG_CANCEL],
                              G_TYPE_FROM_CLASS (class),
                              _gtk_marshal_BOOLEAN__OBJECT_ENUMv);
}

static GdkContentProvider *
gtk_drag_source_prepare (GtkDragSource *source,
                         double         x,
                         double         y)
{
  if (source->actions == 0)
    return NULL;

  if (source->content == NULL)
    return NULL;

  return g_object_ref (source->content);
}

static void
drag_end (GtkDragSource *source,
          gboolean       success)
{
  gboolean delete_data;

  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_dnd_finished_cb, source);
  g_signal_handlers_disconnect_by_func (source->drag, gtk_drag_source_cancel_cb, source);

  delete_data = success && gdk_drag_get_selected_action (source->drag) == GDK_ACTION_MOVE;

  g_signal_emit (source, signals[DRAG_END], 0, source->drag, delete_data);

  gdk_drag_drop_done (source->drag, success);
  g_clear_object (&source->drag);
  g_clear_handle_id (&source->timeout_id, g_source_remove);
  g_object_unref (source);
}

static void
gtk_drag_source_dnd_finished_cb (GdkDrag       *drag,
                                 GtkDragSource *source)
{
  drag_end (source, TRUE);
}

static void
gtk_drag_source_cancel_cb (GdkDrag             *drag,
                           GdkDragCancelReason  reason,
                           GtkDragSource       *source)
{
  gboolean success = FALSE;

  g_signal_emit (source, signals[DRAG_CANCEL], 0, source->drag, reason, &success);
  drag_end (source, success);
}

static void
gtk_drag_source_ensure_icon (GtkDragSource *self,
                             GdkDrag       *drag)
{
  GdkContentProvider *provider;
  GtkWidget *icon, *child;
  GdkContentFormats *formats;
  const GType *types;
  gsize i, n_types;

  icon = gtk_drag_icon_get_for_drag (drag);
  /* If an icon has been set already, we don't need to set one. */
  if (gtk_drag_icon_get_child (GTK_DRAG_ICON (icon)))
    return;

  if (self->paintable)
    {
      gtk_drag_icon_set_from_paintable (drag,
                                        self->paintable,
                                        self->hot_x,
                                        self->hot_y);
      return;
    }

  gdk_drag_set_hotspot (drag, -2, -2);

  provider = gdk_drag_get_content (drag);
  formats = gdk_content_provider_ref_formats (provider);
  types = gdk_content_formats_get_gtypes (formats, &n_types);
  for (i = 0; i < n_types; i++)
    {
      GValue value = G_VALUE_INIT;

      g_value_init (&value, types[i]);
      if (gdk_content_provider_get_value (provider, &value, NULL))
        {
          child = gtk_drag_icon_create_widget_for_value (&value);

          if (child)
            {
              gtk_drag_icon_set_child (GTK_DRAG_ICON (icon), child);
              g_value_unset (&value);
              gdk_content_formats_unref (formats);
              return;
            }
        }
      g_value_unset (&value);
    }

  gdk_content_formats_unref (formats);
  child = gtk_image_new_from_icon_name ("text-x-generic");
  gtk_image_set_icon_size (GTK_IMAGE (child), GTK_ICON_SIZE_LARGE);
  gtk_drag_icon_set_child (GTK_DRAG_ICON (icon), child);
}

static void
gtk_drag_source_drag_begin (GtkDragSource *source)
{
  GtkWidget *widget;
  GdkDevice *device, *pointer;
  GdkSeat *seat;
  graphene_point_t p;
  GtkNative *native;
  GdkSurface *surface;
  double px, py;
  double dx, dy;
  GdkContentProvider *content = NULL;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  device = gtk_gesture_get_device (GTK_GESTURE (source));
  seat = gdk_device_get_seat (device);

  if (device == gdk_seat_get_keyboard (seat))
    pointer = gdk_seat_get_pointer (seat);
  else
    pointer = device;

  native = gtk_widget_get_native (widget);
  surface = gtk_native_get_surface (native);

  if (!gtk_widget_compute_point (widget, GTK_WIDGET (native),
                                 &GRAPHENE_POINT_INIT (source->start_x, source->start_y),
                                 &p))
    return;

  gdk_surface_get_device_position (surface, pointer, &px, &py, NULL);

  dx = round (px - p.x);
  dy = round (py - p.y);

  g_signal_emit (source, signals[PREPARE], 0, source->start_x, source->start_y, &content);
  if (!content)
    return;

  source->drag = gdk_drag_begin (surface, pointer, content, source->actions, dx, dy);

  g_object_unref (content);

  if (source->drag == NULL)
    return;

  gtk_widget_reset_controllers (widget);

  g_signal_emit (source, signals[DRAG_BEGIN], 0, source->drag);

  gtk_drag_source_ensure_icon (source, source->drag);

  /* Keep the source alive until the drag is done */
  g_object_ref (source);

  g_signal_connect (source->drag, "dnd-finished",
                    G_CALLBACK (gtk_drag_source_dnd_finished_cb), source);
  g_signal_connect (source->drag, "cancel",
                    G_CALLBACK (gtk_drag_source_cancel_cb), source);
}

/**
 * gtk_drag_source_new:
 *
 * Creates a new `GtkDragSource` object.
 *
 * Returns: the new `GtkDragSource`
 */
GtkDragSource *
gtk_drag_source_new (void)
{
  return g_object_new (GTK_TYPE_DRAG_SOURCE, NULL);
}

/**
 * gtk_drag_source_get_content: (attributes org.gtk.Method.get_property=content)
 * @source: a `GtkDragSource`
 *
 * Gets the current content provider of a `GtkDragSource`.
 *
 * Returns: (nullable) (transfer none): the `GdkContentProvider` of @source
 */
GdkContentProvider *
gtk_drag_source_get_content (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->content;
}

/**
 * gtk_drag_source_set_content: (attributes org.gtk.Method.set_property=content)
 * @source: a `GtkDragSource`
 * @content: (nullable): a `GdkContentProvider`
 *
 * Sets a content provider on a `GtkDragSource`.
 *
 * When the data is requested in the cause of a DND operation,
 * it will be obtained from the content provider.
 *
 * This function can be called before a drag is started,
 * or in a handler for the [signal@Gtk.DragSource::prepare] signal.
 *
 * You may consider setting the content provider back to
 * %NULL in a [signal@Gtk.DragSource::drag-end] signal handler.
 */
void
gtk_drag_source_set_content (GtkDragSource      *source,
                             GdkContentProvider *content)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (!g_set_object (&source->content, content))
    return;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_CONTENT]);
}

/**
 * gtk_drag_source_get_actions: (attributes org.gtk.Method.get_property=actions)
 * @source: a `GtkDragSource`
 *
 * Gets the actions that are currently set on the `GtkDragSource`.
 *
 * Returns: the actions set on @source
 */
GdkDragAction
gtk_drag_source_get_actions (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), 0);

  return source->actions;
}

/**
 * gtk_drag_source_set_actions: (attributes org.gtk.Method.set_property=actions)
 * @source: a `GtkDragSource`
 * @actions: the actions to offer
 *
 * Sets the actions on the `GtkDragSource`.
 *
 * During a DND operation, the actions are offered to potential
 * drop targets. If @actions include %GDK_ACTION_MOVE, you need
 * to listen to the [signal@Gtk.DragSource::drag-end] signal and
 * handle @delete_data being %TRUE.
 *
 * This function can be called before a drag is started,
 * or in a handler for the [signal@Gtk.DragSource::prepare] signal.
 */
void
gtk_drag_source_set_actions (GtkDragSource *source,
                             GdkDragAction  actions)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->actions == actions)
    return;

  source->actions = actions;

  g_object_notify_by_pspec (G_OBJECT (source), properties[PROP_ACTIONS]);
}

/**
 * gtk_drag_source_set_icon:
 * @source: a `GtkDragSource`
 * @paintable: (nullable): the `GdkPaintable` to use as icon
 * @hot_x: the hotspot X coordinate on the icon
 * @hot_y: the hotspot Y coordinate on the icon
 *
 * Sets a paintable to use as icon during DND operations.
 *
 * The hotspot coordinates determine the point on the icon
 * that gets aligned with the hotspot of the cursor.
 *
 * If @paintable is %NULL, a default icon is used.
 *
 * This function can be called before a drag is started, or in
 * a [signal@Gtk.DragSource::prepare] or
 * [signal@Gtk.DragSource::drag-begin] signal handler.
 */
void
gtk_drag_source_set_icon (GtkDragSource *source,
                          GdkPaintable  *paintable,
                          int            hot_x,
                          int            hot_y)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  g_set_object (&source->paintable, paintable);

  source->hot_x = hot_x;
  source->hot_y = hot_y;
}

/**
 * gtk_drag_source_get_drag:
 * @source: a `GtkDragSource`
 *
 * Returns the underlying `GdkDrag` object for an ongoing drag.
 *
 * Returns: (nullable) (transfer none): the `GdkDrag` of the current
 *   drag operation
 */
GdkDrag *
gtk_drag_source_get_drag (GtkDragSource *source)
{
  g_return_val_if_fail (GTK_IS_DRAG_SOURCE (source), NULL);

  return source->drag;
}

/**
 * gtk_drag_source_drag_cancel:
 * @source: a `GtkDragSource`
 *
 * Cancels a currently ongoing drag operation.
 */
void
gtk_drag_source_drag_cancel (GtkDragSource *source)
{
  g_return_if_fail (GTK_IS_DRAG_SOURCE (source));

  if (source->drag)
    {
      gboolean success = FALSE;

      g_signal_emit (source, signals[DRAG_CANCEL], 0, source->drag, GDK_DRAG_CANCEL_ERROR, &success);
      drag_end (source, FALSE);
    }
}

/**
 * gtk_drag_check_threshold: (method)
 * @widget: a `GtkWidget`
 * @start_x: X coordinate of start of drag
 * @start_y: Y coordinate of start of drag
 * @current_x: current X coordinate
 * @current_y: current Y coordinate
 *
 * Checks to see if a drag movement has passed the GTK drag threshold.
 *
 * Returns: %TRUE if the drag threshold has been passed.
 */
gboolean
gtk_drag_check_threshold (GtkWidget *widget,
                          int        start_x,
                          int        start_y,
                          int        current_x,
                          int        current_y)
{
  int drag_threshold;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  drag_threshold = gtk_settings_get_dnd_drag_threshold (gtk_widget_get_settings (widget));

  return (ABS (current_x - start_x) > drag_threshold ||
          ABS (current_y - start_y) > drag_threshold);
}

gboolean
gtk_drag_check_threshold_double (GtkWidget *widget,
                                 double     start_x,
                                 double     start_y,
                                 double     current_x,
                                 double     current_y)
{
  int drag_threshold;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  drag_threshold = gtk_settings_get_dnd_drag_threshold (gtk_widget_get_settings (widget));

  return (ABS (current_x - start_x) > drag_threshold ||
          ABS (current_y - start_y) > drag_threshold);
}
