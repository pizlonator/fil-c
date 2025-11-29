/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "gdkdisplayprivate.h"
#include "gdkdragprivate.h"
#include "gdkdropprivate.h"
#include "gdkeventsprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"
#include "gdkprivate.h"

#include <gobject/gvaluecollector.h>

#include <string.h>
#include <math.h>

/**
 * GdkEvent: (ref-func gdk_event_ref) (unref-func gdk_event_unref)
 *
 * `GdkEvent`s are immutable data structures, created by GDK to
 * represent windowing system events.
 *
 * In GTK applications the events are handled automatically by toplevel
 * widgets and passed on to the event controllers of appropriate widgets,
 * so using `GdkEvent` and its related API is rarely needed.
 */

/**
 * GdkEventSequence:
 *
 * `GdkEventSequence` is an opaque type representing a sequence
 * of related touch events.
 */

static void
value_event_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_event_free_value (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    gdk_event_unref (value->data[0].v_pointer);
}

static void
value_event_copy_value (const GValue *src,
                        GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = gdk_event_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_event_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static char *
value_event_collect_value (GValue      *value,
                           guint        n_collect_values,
                           GTypeCValue *collect_values,
                           guint        collect_flags)
{
  GdkEvent *event = collect_values[0].v_pointer;

  if (event == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (event->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed GdkEvent pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = gdk_event_ref (event);

  return NULL;
}

static char *
value_event_lcopy_value (const GValue *value,
                         guint         n_collect_values,
                         GTypeCValue  *collect_values,
                         guint         collect_flags)
{
  GdkEvent **event_p = collect_values[0].v_pointer;

  if (event_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *event_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *event_p = value->data[0].v_pointer;
  else
    *event_p = gdk_event_ref (value->data[0].v_pointer);

  return NULL;
}

static void
gdk_event_finalize (GdkEvent *self)
{
  g_clear_object (&self->surface);
  g_clear_object (&self->device);

  g_type_free_instance ((GTypeInstance *) self);
}

static GdkModifierType
gdk_event_real_get_state (GdkEvent *self)
{
  return 0;
}

static gboolean
gdk_event_real_get_position (GdkEvent *self,
                             double   *x,
                             double   *y)
{
  *x = NAN;
  *y = NAN;

  return FALSE;
}

static GdkEventSequence *
gdk_event_real_get_sequence (GdkEvent *self)
{
  return NULL;
}

static GdkDeviceTool *
gdk_event_real_get_tool (GdkEvent *self)
{
  return NULL;
}

static gboolean
gdk_event_real_get_axes (GdkEvent  *self,
                         double   **axes,
                         guint     *n_axes)
{
  return FALSE;
}

static void
gdk_event_class_init (GdkEventClass *klass)
{
  klass->finalize = gdk_event_finalize;
  klass->get_state = gdk_event_real_get_state;
  klass->get_position = gdk_event_real_get_position;
  klass->get_sequence = gdk_event_real_get_sequence;
  klass->get_tool = gdk_event_real_get_tool;
  klass->get_axes = gdk_event_real_get_axes;
}

static void
gdk_event_init (GdkEvent *self)
{
  g_ref_count_init (&self->ref_count);
}

GType
gdk_event_get_type (void)
{
  static gsize event_type__volatile;

  if (g_once_init_enter (&event_type__volatile))
    {
      static const GTypeFundamentalInfo finfo = {
        (G_TYPE_FLAG_CLASSED |
         G_TYPE_FLAG_INSTANTIATABLE |
         G_TYPE_FLAG_DERIVABLE |
         G_TYPE_FLAG_DEEP_DERIVABLE),
      };

      static const GTypeValueTable value_table = {
        value_event_init,
        value_event_free_value,
        value_event_copy_value,
        value_event_peek_pointer,
        "p",
        value_event_collect_value,
        "p",
        value_event_lcopy_value,
      };

      const GTypeInfo event_info = {
        /* Class */
        sizeof (GdkEventClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_event_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        /* Instance */
        sizeof (GdkEvent),
        0,
        (GInstanceInitFunc) gdk_event_init,

        /* GValue */
        &value_table,
      };

      GType event_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("GdkEvent"),
                                     &event_info, &finfo,
                                     G_TYPE_FLAG_ABSTRACT);

      g_once_init_leave (&event_type__volatile, event_type);
    }

  return event_type__volatile;
}

/*< private >
 * GdkEventTypeInfo:
 * @instance_size: the size of the instance of a GdkEvent subclass
 * @instance_init: (nullable): the function to initialize the instance data
 * @finalize: (nullable): the function to free the instance data
 * @get_state: (nullable): the function to retrieve the `GdkModifierType`:w
 *   associated to the event
 * @get_position: (nullable): the function to retrieve the event coordinates
 * @get_sequence: (nullable): the function to retrieve the event sequence
 * @get_tool: (nullable): the function to retrieve the event's device tool
 * @get_axes: (nullable): the function to retrieve the event's axes
 *
 * A structure used when registering a new GdkEvent type.
 */
typedef struct {
  gsize instance_size;

  void (* instance_init) (GdkEvent *event);
  void (* finalize) (GdkEvent *event);

  GdkModifierType (* get_state) (GdkEvent *event);

  gboolean (* get_position) (GdkEvent *event,
                             double *x,
                             double *y);

  GdkEventSequence *(* get_sequence) (GdkEvent *event);

  GdkDeviceTool *(* get_tool) (GdkEvent *event);

  gboolean (* get_axes) (GdkEvent  *event,
                         double   **axes,
                         guint     *n_axes);
} GdkEventTypeInfo;

static void
gdk_event_generic_class_init (gpointer g_class,
                              gpointer class_data)
{
  GdkEventTypeInfo *info = class_data;
  GdkEventClass *event_class = g_class;

  /* Optional */
  if (info->finalize != NULL)
    event_class->finalize = info->finalize;
  if (info->get_state != NULL)
    event_class->get_state = info->get_state;
  if (info->get_position != NULL)
    event_class->get_position = info->get_position;
  if (info->get_sequence != NULL)
    event_class->get_sequence = info->get_sequence;
  if (info->get_tool != NULL)
    event_class->get_tool = info->get_tool;
  if (info->get_axes != NULL)
    event_class->get_axes = info->get_axes;

  g_free (info);
}

static GType
gdk_event_type_register_static (const char             *type_name,
                                const GdkEventTypeInfo *type_info)
{
  GTypeInfo info;

  info.class_size = sizeof (GdkEventClass);
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = gdk_event_generic_class_init;
  info.class_finalize = NULL;
  info.class_data = g_memdup2 (type_info, sizeof (GdkEventTypeInfo));

  info.instance_size = type_info->instance_size;
  info.n_preallocs = 0;
  info.instance_init = (GInstanceInitFunc) type_info->instance_init;
  info.value_table = NULL;

  return g_type_register_static (GDK_TYPE_EVENT, type_name, &info, 0);
}

/* Map GdkEventType to the appropriate GType */
static GType gdk_event_types[GDK_EVENT_LAST];

/*< private >
 * GDK_EVENT_TYPE_SLOT:
 * @ETYPE: a `GdkEvent`Type
 *
 * Associates a `GdkEvent` type with the given `GdkEvent`Type enumeration.
 *
 * This macro can only be used with %GDK_DEFINE_EVENT_TYPE.
 */
#define GDK_EVENT_TYPE_SLOT(ETYPE) { gdk_event_types[ETYPE] = gdk_define_event_type_id; }

/*< private >
 * GDK_DEFINE_EVENT_TYPE:
 * @TypeName: the type name, in camel case
 * @type_name: the type name, in snake case
 * @type_info: the address of the `GdkEvent`TypeInfo for the event type
 * @_C_: custom code to call after registering the event type
 *
 * Registers a new `GdkEvent` subclass with the given @TypeName and @type_info.
 *
 * Similarly to %G_DEFINE_TYPE_WITH_CODE, this macro will generate a `get_type()`
 * function that registers the event type.
 *
 * You can specify code to be run after the type registration; the `GType` of
 * the event is available in the `gdk_define_event_type_id` variable.
 */
#define GDK_DEFINE_EVENT_TYPE(TypeName, type_name, type_info, _C_) \
GType \
type_name ## _get_type (void) \
{ \
  static gsize gdk_define_event_type_id__volatile; \
  if (g_once_init_enter (&gdk_define_event_type_id__volatile)) \
    { \
      GType gdk_define_event_type_id = \
        gdk_event_type_register_static (g_intern_static_string (#TypeName), type_info); \
      { _C_ } \
      g_once_init_leave (&gdk_define_event_type_id__volatile, gdk_define_event_type_id); \
    } \
  return gdk_define_event_type_id__volatile; \
}

#define GDK_EVENT_SUPER(event) \
  ((GdkEventClass *) g_type_class_peek (g_type_parent (G_TYPE_FROM_INSTANCE (event))))

/*< private >
 * gdk_event_alloc:
 * @event_type: the `GdkEvent`Type to allocate
 * @surface: (nullable): the `GdkSurface` of the event
 * @device: (nullable): the `GdkDevice` of the event
 * @time_: the event serial
 *
 * Allocates a `GdkEvent` for the given @event_type, and sets its
 * common fields with the given parameters.
 *
 * Returns: (transfer full): the newly allocated `GdkEvent` instance
 */
static gpointer
gdk_event_alloc (GdkEventType event_type,
                 GdkSurface   *surface,
                 GdkDevice    *device,
                 guint32       time_)
{
  g_assert (event_type >= GDK_DELETE && event_type < GDK_EVENT_LAST);
  g_assert (gdk_event_types[event_type] != G_TYPE_INVALID);

  GdkEvent *event = (GdkEvent *) g_type_create_instance (gdk_event_types[event_type]);

  if (GDK_DEBUG_CHECK (EVENTS))
    {
      char *str = g_enum_to_string (GDK_TYPE_EVENT_TYPE, event_type);
      gdk_debug_message ("Allocating a new %s for event type %s",
                         g_type_name (gdk_event_types[event_type]), str);
      g_free (str);
    }

  event->event_type = event_type;
  event->surface = surface != NULL ? g_object_ref (surface) : NULL;
  event->device = device != NULL ? g_object_ref (device) : NULL;
  event->time = time_;

  if (device != NULL && time_ != GDK_CURRENT_TIME)
    gdk_device_set_timestamp (device, time_);

  return event;
}

static void
gdk_event_init_types_once (void)
{
  g_type_ensure (GDK_TYPE_BUTTON_EVENT);
  g_type_ensure (GDK_TYPE_CROSSING_EVENT);
  g_type_ensure (GDK_TYPE_DELETE_EVENT);
  g_type_ensure (GDK_TYPE_DND_EVENT);
  g_type_ensure (GDK_TYPE_FOCUS_EVENT);
  g_type_ensure (GDK_TYPE_GRAB_BROKEN_EVENT);
  g_type_ensure (GDK_TYPE_KEY_EVENT);
  g_type_ensure (GDK_TYPE_MOTION_EVENT);
  g_type_ensure (GDK_TYPE_PAD_EVENT);
  g_type_ensure (GDK_TYPE_PROXIMITY_EVENT);
  g_type_ensure (GDK_TYPE_SCROLL_EVENT);
  g_type_ensure (GDK_TYPE_TOUCH_EVENT);
  g_type_ensure (GDK_TYPE_TOUCHPAD_EVENT);
}

/*< private >
 * gdk_event_init_types:
 *
 * Initializes all GdkEvent types.
 */
void
gdk_event_init_types (void)
{
  static gsize event_types__volatile;

  if (g_once_init_enter (&event_types__volatile))
    {
      gboolean initialized = FALSE;

      gdk_event_init_types_once ();
      initialized = TRUE;

      g_once_init_leave (&event_types__volatile, initialized);
    }
}

#ifdef G_ENABLE_DEBUG
static gboolean
check_event_sanity (GdkEvent *event)
{
  if (event->device != NULL &&
      gdk_surface_get_display (event->surface) != gdk_device_get_display (event->device))
    {
      char *type = g_enum_to_string (GDK_TYPE_EVENT_TYPE, event->event_type);
      g_warning ("Event of type %s with mismatched device display", type);
      g_free (type);
      return FALSE;
    }

  return TRUE;
}
#endif

gboolean
_gdk_event_emit (GdkEvent *event)
{
#ifdef G_ENABLE_DEBUG
  if (!check_event_sanity (event))
    return FALSE;
#endif

  if (gdk_drag_handle_source_event (event))
    return TRUE;

  return gdk_surface_handle_event (event);
}

/*********************************************
 * Functions for maintaining the event queue *
 *********************************************/

/**
 * _gdk_event_queue_find_first:
 * @display: a `GdkDisplay`
 *
 * Find the first event on the queue that is not still
 * being filled in.
 *
 * Returns: (nullable): Pointer to the list node for that event
 */
GList*
_gdk_event_queue_find_first (GdkDisplay *display)
{
  GList *tmp_list;
  GList *pending_motion = NULL;

  gboolean paused = display->event_pause_count > 0;

  tmp_list = g_queue_peek_head_link (&display->queued_events);
  while (tmp_list)
    {
      GdkEvent *event = tmp_list->data;

      if ((event->flags & GDK_EVENT_PENDING) == 0 &&
	  (!paused || (event->flags & GDK_EVENT_FLUSHED) != 0))
        {
          if (pending_motion)
            return pending_motion;

          if ((event->event_type == GDK_MOTION_NOTIFY ||
               (event->event_type == GDK_SCROLL && gdk_scroll_event_get_direction (event) == GDK_SCROLL_SMOOTH)) &&
              (event->flags & GDK_EVENT_FLUSHED) == 0)
            pending_motion = tmp_list;
          else
            return tmp_list;
        }

      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * _gdk_event_queue_append:
 * @display: a `GdkDisplay`
 * @event: Event to append
 *
 * Appends an event onto the tail of the event queue.
 *
 * Returns: the newly appended list node.
 */
GList *
_gdk_event_queue_append (GdkDisplay *display,
			 GdkEvent   *event)
{
  g_queue_push_tail (&display->queued_events, event);

  return g_queue_peek_tail_link (&display->queued_events);
}

/*
 * _gdk_event_queue_remove_link:
 * @display: a `GdkDisplay`
 * @node: node to remove
 *
 * Removes a specified list node from the event queue.
 */
void
_gdk_event_queue_remove_link (GdkDisplay *display,
			      GList      *node)
{
  g_queue_unlink (&display->queued_events, node);
}

/*
 * _gdk_event_unqueue:
 * @display: a `GdkDisplay`
 *
 * Removes and returns the first event from the event
 * queue that is not still being filled in.
 *
 * Returns: (nullable): the event
 */
GdkEvent*
_gdk_event_unqueue (GdkDisplay *display)
{
  GdkEvent *event = NULL;
  GList *tmp_list;

  tmp_list = _gdk_event_queue_find_first (display);

  if (tmp_list)
    {
      event = tmp_list->data;
      _gdk_event_queue_remove_link (display, tmp_list);
      g_list_free_1 (tmp_list);
    }

  return event;
}

/*
 * If the last N events in the event queue are smooth scroll events
 * for the same surface, the same device and the same scroll unit,
 * combine them into one.
 *
 * We give the remaining event a history with N items, and deltas
 * that are the sum over the history entries.
 */
void
gdk_event_queue_handle_scroll_compression (GdkDisplay *display)
{
  GList *l;
  GdkSurface *surface = NULL;
  GdkDevice *device = NULL;
  GList *scrolls = NULL;
  GArray *history = NULL;
  GdkScrollUnit scroll_unit = GDK_SCROLL_UNIT_WHEEL;
  gboolean scroll_unit_defined = FALSE;
  GdkTimeCoord hist;

  l = g_queue_peek_tail_link (&display->queued_events);

  while (l)
    {
      GdkEvent *event = l->data;
      GdkScrollEvent *scroll_event = (GdkScrollEvent *) event;

      if (event->flags & GDK_EVENT_PENDING)
        break;

      if (event->event_type != GDK_SCROLL ||
          gdk_scroll_event_get_direction (event) != GDK_SCROLL_SMOOTH)
        break;

      if (surface != NULL &&
          surface != event->surface)
        break;

      if (device != NULL &&
          device != event->device)
        break;

      if (scroll_unit_defined &&
          scroll_unit != scroll_event->unit)
        break;

      surface = event->surface;
      device = event->device;
      scroll_unit = scroll_event->unit;
      scroll_unit_defined = TRUE;
      scrolls = l;

      l = l->prev;
    }

  while (scrolls && scrolls->next != NULL)
    {
      GdkEvent *event = scrolls->data;
      GList *next = scrolls->next;
      double dx, dy;
      gboolean inherited = FALSE;

      if (!history && ((GdkScrollEvent *)event)->history)
        {
          history = ((GdkScrollEvent *)event)->history;
          ((GdkScrollEvent *)event)->history = NULL;
          inherited = TRUE;
        }

      if (!history)
        history = g_array_new (FALSE, TRUE, sizeof (GdkTimeCoord));

      if (!inherited)
        {
          gdk_scroll_event_get_deltas (event, &dx, &dy);

          memset (&hist, 0, sizeof (GdkTimeCoord));
          hist.time = gdk_event_get_time (event);
          hist.flags = GDK_AXIS_FLAG_DELTA_X | GDK_AXIS_FLAG_DELTA_Y;
          hist.axes[GDK_AXIS_DELTA_X] = dx;
          hist.axes[GDK_AXIS_DELTA_Y] = dy;

          g_array_append_val (history, hist);
       }

      gdk_event_unref (event);
      g_queue_delete_link (&display->queued_events, scrolls);
      scrolls = next;
    }

  if (scrolls && history)
    {
      GdkEvent *old_event, *event;
      double dx, dy;

      old_event = scrolls->data;

      gdk_scroll_event_get_deltas (old_event, &dx, &dy);

      memset (&hist, 0, sizeof (GdkTimeCoord));
      hist.time = gdk_event_get_time (old_event);
      hist.flags = GDK_AXIS_FLAG_DELTA_X | GDK_AXIS_FLAG_DELTA_Y;
      hist.axes[GDK_AXIS_DELTA_X] = dx;
      hist.axes[GDK_AXIS_DELTA_Y] = dy;
      g_array_append_val (history, hist);

      dx = dy = 0;
      for (int i = 0; i < history->len; i++)
        {
          GdkTimeCoord *val = &g_array_index (history, GdkTimeCoord, i);
          dx += val->axes[GDK_AXIS_DELTA_X];
          dy += val->axes[GDK_AXIS_DELTA_Y];
        }

      event = gdk_scroll_event_new (surface,
                                    device,
                                    gdk_event_get_device_tool (old_event),
                                    gdk_event_get_time (old_event),
                                    gdk_event_get_modifier_state (old_event),
                                    dx,
                                    dy,
                                    gdk_scroll_event_is_stop (old_event),
                                    scroll_unit);

      ((GdkScrollEvent *)event)->history = history;

      g_queue_delete_link (&display->queued_events, scrolls);
      g_queue_push_tail (&display->queued_events, event);

      gdk_event_unref (old_event);
    }
}

static void
gdk_motion_event_push_history (GdkEvent *event,
                               GdkEvent *history_event)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;
  GdkDeviceTool *tool;
  GdkTimeCoord hist;
  int i;

  g_assert (GDK_IS_EVENT_TYPE (event, GDK_MOTION_NOTIFY));
  g_assert (GDK_IS_EVENT_TYPE (history_event, GDK_MOTION_NOTIFY));

  if (G_UNLIKELY (!self->history))
    self->history = g_array_new (FALSE, TRUE, sizeof (GdkTimeCoord));

  if (((GdkMotionEvent *)history_event)->history)
    {
      GArray *history = ((GdkMotionEvent *)history_event)->history;
      g_array_append_vals (self->history, history->data, history->len);
    }

  tool = gdk_event_get_device_tool (history_event);

  memset (&hist, 0, sizeof (GdkTimeCoord));
  hist.time = gdk_event_get_time (history_event);

  if (tool)
    {
      hist.flags = gdk_device_tool_get_axes (tool);
      for (i = GDK_AXIS_X; i < GDK_AXIS_LAST; i++)
        gdk_event_get_axis (history_event, i, &hist.axes[i]);
    }

  /* GdkTimeCoord has no dedicated fields to record event position. For plain
   * pointer events, and for tools which don't report GDK_AXIS_X/GDK_AXIS_Y
   * on their own, we surface the position using the X and Y input axes.
   */
  if (!(hist.flags & GDK_AXIS_FLAG_X) || !(hist.flags & GDK_AXIS_FLAG_Y))
    {
      hist.flags |= GDK_AXIS_FLAG_X | GDK_AXIS_FLAG_Y;
      gdk_event_get_position (history_event, &hist.axes[GDK_AXIS_X], &hist.axes[GDK_AXIS_Y]);
    }

  g_array_append_val (self->history, hist);
}

/* If the last N events in the event queue are motion notify
 * events for the same surface, drop all but the last.
 *
 * If a button is held down or the device has a tool, then
 * we give the remaining events a history containing the N-1
 * dropped events.
 */
void
_gdk_event_queue_handle_motion_compression (GdkDisplay *display)
{
  GList *tmp_list;
  GList *pending_motions = NULL;
  GdkSurface *pending_motion_surface = NULL;
  GdkDevice *pending_motion_device = NULL;
  GdkEvent *last_motion = NULL;

  tmp_list = g_queue_peek_tail_link (&display->queued_events);

  while (tmp_list)
    {
      GdkEvent *event = tmp_list->data;

      if (event->flags & GDK_EVENT_PENDING)
        break;

      if (event->event_type != GDK_MOTION_NOTIFY)
        break;

      if (pending_motion_surface != NULL &&
          pending_motion_surface != event->surface)
        break;

      if (pending_motion_device != NULL &&
          pending_motion_device != event->device)
        break;

      if (!last_motion)
        last_motion = event;

      pending_motion_surface = event->surface;
      pending_motion_device = event->device;
      pending_motions = tmp_list;

      tmp_list = tmp_list->prev;
    }

  while (pending_motions && pending_motions->next != NULL)
    {
      GList *next = pending_motions->next;

      if (last_motion != NULL)
        {
          if ((gdk_event_get_modifier_state (last_motion) &
               (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK |
                GDK_BUTTON4_MASK | GDK_BUTTON5_MASK)) ||
               gdk_event_get_device_tool (last_motion) != NULL)
            gdk_motion_event_push_history (last_motion, pending_motions->data);
        }

      gdk_event_unref (pending_motions->data);
      g_queue_delete_link (&display->queued_events, pending_motions);
      pending_motions = next;
    }
}

void
_gdk_event_queue_flush (GdkDisplay *display)
{
  while (TRUE)
    {
      GdkEvent *event;

      event = (GdkEvent *)g_queue_pop_head (&display->queued_events);
      if (!event)
        return;

      event->flags |= GDK_EVENT_FLUSHED;
      _gdk_event_emit (event);
      gdk_event_unref (event);
    }
}

/**
 * gdk_event_ref:
 * @event: a `GdkEvent`
 *
 * Increase the ref count of @event.
 *
 * Returns: (transfer full): @event
 */
GdkEvent *
gdk_event_ref (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  g_ref_count_inc (&event->ref_count);

  return event;
}

/**
 * gdk_event_unref:
 * @event: (transfer full): a `GdkEvent`
 *
 * Decrease the ref count of @event.
 *
 * If the last reference is dropped, the structure is freed.
 */
void
gdk_event_unref (GdkEvent *event)
{
  g_return_if_fail (GDK_IS_EVENT (event));

  if (g_ref_count_dec (&event->ref_count))
    GDK_EVENT_GET_CLASS (event)->finalize (event);
}

/**
 * gdk_event_get_pointer_emulated:
 * @event: a `GdkEvent`
 *
 * Returns whether this event is an 'emulated' pointer event.
 *
 * Emulated pointer events typically originate from a touch events.
 *
 * Returns: %TRUE if this event is emulated
 */
gboolean
gdk_event_get_pointer_emulated (GdkEvent *event)
{
  switch ((int) event->event_type)
    {
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_END:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_CANCEL:
      {
        GdkTouchEvent *tevent = (GdkTouchEvent *) event;

        return tevent->pointer_emulated;
      }

    default:
      break;
    }

  return FALSE;
}

/**
 * gdk_event_get_axis:
 * @event: a `GdkEvent`
 * @axis_use: the axis use to look for
 * @value: (out): location to store the value found
 *
 * Extract the axis value for a particular axis use from
 * an event structure.
 *
 * To find out which axes are used, use [method@Gdk.DeviceTool.get_axes]
 * on the device tool returned by [method@Gdk.Event.get_device_tool].
 *
 * Returns: %TRUE if the specified axis was found, otherwise %FALSE
 */
gboolean
gdk_event_get_axis (GdkEvent   *event,
		    GdkAxisUse  axis_use,
		    double     *value)
{
  double *axes;
  guint n_axes;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);

  if (axis_use == GDK_AXIS_X || axis_use == GDK_AXIS_Y)
    {
      double x, y;

      if (!gdk_event_get_position (event, &x, &y))
        return FALSE;

      if (axis_use == GDK_AXIS_X && value != NULL)
	*value = x;
      if (axis_use == GDK_AXIS_Y && value != NULL)
	*value = y;

      return TRUE;
    }

  if (!gdk_event_get_axes (event, &axes, &n_axes))
    return FALSE;

  *value = axes[axis_use];
  return TRUE;
}

/**
 * gdk_event_triggers_context_menu:
 * @event: a `GdkEvent`, currently only button events are meaningful values
 *
 * Returns whether a `GdkEvent` should trigger a context menu,
 * according to platform conventions.
 *
 * The right mouse button typically triggers context menus.
 *
 * This function should always be used instead of simply checking for
 * event->button == %GDK_BUTTON_SECONDARY.
 *
 * Returns: %TRUE if the event should trigger a context menu.
 */
gboolean
gdk_event_triggers_context_menu (GdkEvent *event)
{
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->event_type == GDK_BUTTON_PRESS)
    {
      GdkButtonEvent *bevent = (GdkButtonEvent *) event;

      g_return_val_if_fail (GDK_IS_SURFACE (event->surface), FALSE);

      if (bevent->button == GDK_BUTTON_SECONDARY &&
          ! (bevent->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK)))
        return TRUE;
    }

  return FALSE;
}

static gboolean
gdk_events_get_axis_distances (GdkEvent *event1,
                               GdkEvent *event2,
                               double   *x_distance,
                               double   *y_distance,
                               double   *distance)
{
  double x1, x2, y1, y2;
  double xd, yd;

  if (!gdk_event_get_position (event1, &x1, &y1) ||
      !gdk_event_get_position (event2, &x2, &y2))
    return FALSE;

  xd = x2 - x1;
  yd = y2 - y1;

  if (x_distance)
    *x_distance = xd;

  if (y_distance)
    *y_distance = yd;

  if (distance)
    *distance = sqrt ((xd * xd) + (yd * yd));

  return TRUE;
}

/**
 * gdk_events_get_distance:
 * @event1: first `GdkEvent`
 * @event2: second `GdkEvent`
 * @distance: (out): return location for the distance
 *
 * Returns the distance between the event locations.
 *
 * This assumes that both events have X/Y information.
 * If not, this function returns %FALSE.
 *
 * Returns: %TRUE if the distance could be calculated.
 **/
gboolean
gdk_events_get_distance (GdkEvent *event1,
                         GdkEvent *event2,
                         double   *distance)
{
  return gdk_events_get_axis_distances (event1, event2,
                                        NULL, NULL,
                                        distance);
}

/**
 * gdk_events_get_angle:
 * @event1: first `GdkEvent`
 * @event2: second `GdkEvent`
 * @angle: (out): return location for the relative angle between both events
 *
 * Returns the relative angle from @event1 to @event2.
 *
 * The relative angle is the angle between the X axis and the line
 * through both events' positions. The rotation direction for positive
 * angles is from the positive X axis towards the positive Y axis.
 *
 * This assumes that both events have X/Y information.
 * If not, this function returns %FALSE.
 *
 * Returns: %TRUE if the angle could be calculated.
 */
gboolean
gdk_events_get_angle (GdkEvent *event1,
                      GdkEvent *event2,
                      double   *angle)
{
  double x_distance, y_distance, distance;

  if (!gdk_events_get_axis_distances (event1, event2,
                                      &x_distance, &y_distance,
                                      &distance))
    return FALSE;

  if (angle)
    {
      *angle = atan2 (x_distance, y_distance);

      /* Invert angle */
      *angle = (2 * G_PI) - *angle;

      /* Shift it 90° */
      *angle += G_PI / 2;

      /* And constraint it to 0°-360° */
      *angle = fmod (*angle, 2 * G_PI);
    }

  return TRUE;
}

/**
 * gdk_events_get_center:
 * @event1: first `GdkEvent`
 * @event2: second `GdkEvent`
 * @x: (out): return location for the X coordinate of the center
 * @y: (out): return location for the Y coordinate of the center
 *
 * Returns the point halfway between the events' positions.
 *
 * This assumes that both events have X/Y information.
 * If not, this function returns %FALSE.
 *
 * Returns: %TRUE if the center could be calculated.
 */
gboolean
gdk_events_get_center (GdkEvent *event1,
                       GdkEvent *event2,
                       double   *x,
                       double   *y)
{
  double x1, x2, y1, y2;

  if (!gdk_event_get_position (event1, &x1, &y1) ||
      !gdk_event_get_position (event2, &x2, &y2))
    return FALSE;

  if (x)
    *x = (x2 + x1) / 2;

  if (y)
    *y = (y2 + y1) / 2;

  return TRUE;
}

static GdkEventSequence *
gdk_event_sequence_copy (GdkEventSequence *sequence)
{
  return sequence;
}

static void
gdk_event_sequence_free (GdkEventSequence *sequence)
{
  /* Nothing to free here */
}

G_DEFINE_BOXED_TYPE (GdkEventSequence, gdk_event_sequence,
                     gdk_event_sequence_copy,
                     gdk_event_sequence_free)



/**
 * gdk_event_get_axes:
 * @event: a `GdkEvent`
 * @axes: (transfer none) (out) (array length=n_axes): the array of values for all axes
 * @n_axes: (out): the length of array
 *
 * Extracts all axis values from an event.
 *
 * To find out which axes are used, use [method@Gdk.DeviceTool.get_axes]
 * on the device tool returned by [method@Gdk.Event.get_device_tool].
 *
 * Returns: %TRUE on success, otherwise %FALSE
 */
gboolean
gdk_event_get_axes (GdkEvent  *event,
                    double   **axes,
                    guint     *n_axes)
{
  gboolean ret;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);

  ret = GDK_EVENT_GET_CLASS (event)->get_axes (event, axes, n_axes);

  if (*axes == NULL)
    return FALSE;

  return ret;
}

double *
gdk_event_dup_axes (GdkEvent *event)
{
  double *axes;
  guint n_axes;

  if (gdk_event_get_axes (event, &axes, &n_axes))
    {
      double *axes_copy = g_memdup2 (axes, n_axes * sizeof (double));

      return axes_copy;
    }

  return NULL;
}

/**
 * gdk_event_get_event_type:
 * @event: a `GdkEvent`
 *
 * Retrieves the type of the event.
 *
 * Returns: a `GdkEvent`Type
 */
GdkEventType
gdk_event_get_event_type (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), 0);

  return event->event_type;
}

/**
 * gdk_event_get_surface:
 * @event: a `GdkEvent`
 *
 * Extracts the surface associated with an event.
 *
 * Returns: (transfer none) (nullable): The `GdkSurface` associated with the event
 */
GdkSurface *
gdk_event_get_surface (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  return event->surface;
}

/**
 * gdk_event_get_seat:
 * @event: a `GdkEvent`
 *
 * Returns the seat that originated the event.
 *
 * Returns: (nullable) (transfer none): a `GdkSeat`.
 */
GdkSeat *
gdk_event_get_seat (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  return event->device ? gdk_device_get_seat (event->device) : NULL;
}

/**
 * gdk_event_get_device:
 * @event: a `GdkEvent`.
 *
 * Returns the device of an event.
 *
 * Returns: (nullable) (transfer none): a `GdkDevice`
 */
GdkDevice *
gdk_event_get_device (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  return event->device;
}

/**
 * gdk_event_get_device_tool:
 * @event: a `GdkEvent`
 *
 * Returns a `GdkDeviceTool` representing the tool that
 * caused the event.
 *
 * If the was not generated by a device that supports
 * different tools (such as a tablet), this function will
 * return %NULL.
 *
 * Note: the `GdkDeviceTool` will be constant during
 * the application lifetime, if settings must be stored
 * persistently across runs, see [method@Gdk.DeviceTool.get_serial].
 *
 * Returns: (transfer none) (nullable): The current device tool
 */
GdkDeviceTool *
gdk_event_get_device_tool (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  return GDK_EVENT_GET_CLASS (event)->get_tool (event);
}

/**
 * gdk_event_get_time:
 * @event: a `GdkEvent`
 *
 * Returns the timestamp of @event.
 *
 * Not all events have timestamps. In that case, this function
 * returns %GDK_CURRENT_TIME.
 *
 * Returns: timestamp field from @event
 */
guint32
gdk_event_get_time (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), GDK_CURRENT_TIME);

  return event->time;
}

/**
 * gdk_event_get_display:
 * @event: a `GdkEvent`
 *
 * Retrieves the display associated to the @event.
 *
 * Returns: (transfer none) (nullable): a `GdkDisplay`
 */
GdkDisplay *
gdk_event_get_display (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  if (event->surface)
    return gdk_surface_get_display (event->surface);

  return NULL;
}

/**
 * gdk_event_get_event_sequence:
 * @event: a `GdkEvent`
 *
 * Returns the event sequence to which the event belongs.
 *
 * Related touch events are connected in a sequence. Other
 * events typically don't have event sequence information.
 *
 * Returns: (transfer none): the event sequence that the event belongs to
 */
GdkEventSequence *
gdk_event_get_event_sequence (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);

  return GDK_EVENT_GET_CLASS (event)->get_sequence (event);
}

/**
 * gdk_event_get_modifier_state:
 * @event: a `GdkEvent`
 *
 * Returns the modifier state field of an event.
 *
 * Returns: the modifier state of @event
 */
GdkModifierType
gdk_event_get_modifier_state (GdkEvent *event)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), 0);

  return GDK_EVENT_GET_CLASS (event)->get_state (event);
}

/**
 * gdk_event_get_position:
 * @event: a `GdkEvent`
 * @x: (out): location to put event surface x coordinate
 * @y: (out): location to put event surface y coordinate
 *
 * Extract the event surface relative x/y coordinates from an event.
 *
 * This position is in [surface coordinates](coordinates.html).
 */
gboolean
gdk_event_get_position (GdkEvent *event,
                        double   *x,
                        double   *y)
{
  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);

  return GDK_EVENT_GET_CLASS (event)->get_position (event, x, y);
}

/* {{{ GdkButtonEvent */

/**
 * GdkButtonEvent:
 *
 * An event related to a button on a pointer device.
 */

static void
gdk_button_event_finalize (GdkEvent *event)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;

  g_clear_object (&self->tool);
  g_clear_pointer (&self->axes, g_free);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static GdkModifierType
gdk_button_event_get_state (GdkEvent *event)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;

  return self->state;
}

static gboolean
gdk_button_event_get_position (GdkEvent *event,
                               double   *x,
                               double   *y)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static GdkDeviceTool *
gdk_button_event_get_tool (GdkEvent *event)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;

  return self->tool;
}

static gboolean
gdk_button_event_get_axes (GdkEvent  *event,
                           double   **axes,
                           guint     *n_axes)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;
  GdkDevice *source_device = gdk_event_get_device (event);

  if (source_device == NULL)
    return FALSE;

  *axes = self->axes;
  *n_axes = GDK_AXIS_LAST;

  return TRUE;
}

static const GdkEventTypeInfo gdk_button_event_info = {
  sizeof (GdkButtonEvent),
  NULL,
  gdk_button_event_finalize,
  gdk_button_event_get_state,
  gdk_button_event_get_position,
  NULL,
  gdk_button_event_get_tool,
  gdk_button_event_get_axes,
};

GDK_DEFINE_EVENT_TYPE (GdkButtonEvent, gdk_button_event,
                       &gdk_button_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_BUTTON_PRESS)
                       GDK_EVENT_TYPE_SLOT (GDK_BUTTON_RELEASE))

GdkEvent *
gdk_button_event_new (GdkEventType     type,
                      GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      guint            button,
                      double           x,
                      double           y,
                      double          *axes)
{
  g_return_val_if_fail (type == GDK_BUTTON_PRESS ||
                        type == GDK_BUTTON_RELEASE, NULL);

  GdkButtonEvent *self = gdk_event_alloc (type, surface, device, time);

  self->tool = tool != NULL ? g_object_ref (tool) : NULL;
  self->axes = axes;
  self->state = state;
  self->button = button;
  self->x = x;
  self->y = y;

  return (GdkEvent *) self;
}

/**
 * gdk_button_event_get_button:
 * @event: (type GdkButtonEvent): a button event
 *
 * Extract the button number from a button event.
 *
 * Returns: the button of @event
 **/
guint
gdk_button_event_get_button (GdkEvent *event)
{
  GdkButtonEvent *self = (GdkButtonEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_BUTTON_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_BUTTON_RELEASE), 0);

  return self->button;
}

/* }}} */
/* {{{ GdkKeyEvent */

/**
 * GdkKeyEvent:
 *
 * An event related to a key-based device.
 */

static void
gdk_key_event_finalize (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_free (self->compose_sequence);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static GdkModifierType
gdk_key_event_get_state (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  return self->state;
}

static const GdkEventTypeInfo gdk_key_event_info = {
  sizeof (GdkKeyEvent),
  NULL,
  gdk_key_event_finalize,
  gdk_key_event_get_state,
  NULL,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkKeyEvent, gdk_key_event,
                       &gdk_key_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_KEY_PRESS)
                       GDK_EVENT_TYPE_SLOT (GDK_KEY_RELEASE))

/*< private >
 * gdk_key_event_new:
 * @type: the event type, either %GDK_KEY_PRESS or %GDK_KEY_RELEASE
 * @surface: the surface of the event
 * @device: the device related to the event
 * @time: the event's timestamp
 * @keycode: the keycode of the event
 * @state: the modifiers state
 * @is_modifier: whether the event is a modifiers only event
 * @translated: the translated key data for the given @state
 * @no_lock: the translated key data without the given @state
 * @compose_sequence: (transfer none) (nullable):
 *   The compose sequence string, either partial or only the
 *   final composed string, if that can be determined at event
 *   creation time. Used by selected IM modules.
 *
 * Creates a new `GdkKeyEvent`.
 *
 * Returns: (transfer full) (type GdkKeyEvent): the newly created `GdkEvent`
 */
GdkEvent *
gdk_key_event_new (GdkEventType      type,
                   GdkSurface       *surface,
                   GdkDevice        *device,
                   guint32           time,
                   guint             keycode,
                   GdkModifierType   state,
                   gboolean          is_modifier,
                   GdkTranslatedKey *translated,
                   GdkTranslatedKey *no_lock,
                   char             *compose_sequence)
{
  g_return_val_if_fail (type == GDK_KEY_PRESS ||
                        type == GDK_KEY_RELEASE, NULL);

  GdkKeyEvent *self = gdk_event_alloc (type, surface, device, time);
  GdkEvent *event = (GdkEvent *) self;

  self->keycode = keycode;
  self->state = state;
  self->key_is_modifier = is_modifier;
  self->translated[0] = *translated;
  self->translated[1] = *no_lock;
  self->compose_sequence = g_strdup (compose_sequence);

  return event;
}

/*< private >
 * gdk_key_event_get_translated_key:
 * @event: (type GdkKeyEvent): a key event
 * @no_lock: whether the translated key should take the event
 *   state into account
 *
 * Extracts the translated key from a key event.
 *
 * Returns: (transfer none): the translated key
 */
GdkTranslatedKey *
gdk_key_event_get_translated_key (GdkEvent *event,
                                  gboolean  no_lock)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), NULL);

  if (no_lock)
    return &(self->translated[1]);

  return &(self->translated[0]);
}

/*< private >
 * gdk_key_event_get_compose_sequence:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the compose sequence string from a key event.
 *
 * Returns: (transfer none): the compose sequence string
 */
char *
gdk_key_event_get_compose_sequence (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), FALSE);

  return self->compose_sequence;
}

/**
 * gdk_key_event_get_keyval:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the keyval from a key event.
 *
 * Returns: the keyval of @event
 */
guint
gdk_key_event_get_keyval (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), 0);

  return self->translated[0].keyval;
}

/**
 * gdk_key_event_get_keycode:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the keycode from a key event.
 *
 * Returns: the keycode of @event
 */
guint
gdk_key_event_get_keycode (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), 0);

  return self->keycode;
}

/**
 * gdk_key_event_get_level:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the shift level from a key event.
 *
 * Returns: the shift level of @event
 */
guint
gdk_key_event_get_level (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), 0);

  return self->translated[0].level;
}

/**
 * gdk_key_event_get_layout:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the layout from a key event.
 *
 * Returns: the layout of @event
 */
guint
gdk_key_event_get_layout (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), 0);

  return self->translated[0].layout;
}

/**
 * gdk_key_event_get_consumed_modifiers:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts the consumed modifiers from a key event.
 *
 * Returns: the consumed modifiers or @event
 */
GdkModifierType
gdk_key_event_get_consumed_modifiers (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), 0);

  return self->translated[0].consumed;
}

/**
 * gdk_key_event_is_modifier:
 * @event: (type GdkKeyEvent): a key event
 *
 * Extracts whether the key event is for a modifier key.
 *
 * Returns: %TRUE if the @event is for a modifier key
 */
gboolean
gdk_key_event_is_modifier (GdkEvent *event)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), FALSE);

  return self->key_is_modifier;
}

static gboolean
keyval_in_group (GdkKeymap *keymap,
                 guint      keyval,
                 int        group)
{
  GdkKeymapKey *keys;
  guint n_keys;

  gdk_keymap_get_cached_entries_for_keyval (keymap, keyval, &keys, &n_keys);
  for (int i = 0; i < n_keys; i++)
    {
      if (keys[i].group == group)
        return TRUE;
    }

  return FALSE;
}

/**
 * gdk_key_event_matches:
 * @event: (type GdkKeyEvent): a key `GdkEvent`
 * @keyval: the keyval to match
 * @modifiers: the modifiers to match
 *
 * Matches a key event against a keyval and modifiers.
 *
 * This is typically used to trigger keyboard shortcuts such as Ctrl-C.
 *
 * Partial matches are possible where the combination matches
 * if the currently active group is ignored.
 *
 * Note that we ignore Caps Lock for matching.
 *
 * Returns: a `GdkKeyMatch` value describing whether @event matches
 */
GdkKeyMatch
gdk_key_event_matches (GdkEvent        *event,
                       guint            keyval,
                       GdkModifierType  modifiers)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;
  GdkKeymap *keymap;
  guint keycode;
  GdkModifierType state;
  guint ev_keyval;
  int layout;
  int level;
  GdkModifierType ignored_modifiers;
  GdkModifierType shift_group_mask;
  gboolean group_mod_is_accel_mod = FALSE;
  const GdkModifierType mask = GDK_CONTROL_MASK |
                               GDK_SHIFT_MASK |
                               GDK_ALT_MASK |
                               GDK_SUPER_MASK |
                               GDK_HYPER_MASK |
                               GDK_META_MASK;

  g_return_val_if_fail (GDK_IS_EVENT (event), GDK_KEY_MATCH_NONE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), GDK_KEY_MATCH_NONE);

  keycode = self->keycode;
  state = self->state & ~GDK_LOCK_MASK;
  ev_keyval = self->translated[1].keyval;
  layout = self->translated[1].layout;
  level = self->translated[1].level;

  /*
   * If a modifier is currently active (e.g. Shift is pressed) and was marked
   * as consumed, we ignore it for the purposes of matching shortcuts.
   * For example, when Ctrl+Shift+[plus/equals key] is translated into
   * Ctrl+plus on a keyboard where Shift+equals is the plus sign, we want
   * shortcuts for either <Control><Shift>plus or <Control>plus to match.
   * (See https://bugzilla.gnome.org/show_bug.cgi?id=100439)
   *
   * If a modifier is *not* currently active, the X11 backend can sometimes
   * mark it as consumed where the Wayland and Windows backends do not.
   * In this case, we still want to pay attention to its state.
   * For example, when Ctrl+x is translated into Ctrl+x, we only want to
   * trigger shortcuts for <Control>x, not for <Control><Shift>x.
   * (See https://gitlab.gnome.org/GNOME/gtk/-/issues/5095)
   */
  ignored_modifiers = (self->translated[1].consumed & state);

  /* if the group-toggling modifier is part of the default accel mod
   * mask, and it is active, disable it for matching
   *
   * FIXME: get shift group mask from backends
   */
  shift_group_mask = 0;

  if (mask & shift_group_mask)
    group_mod_is_accel_mod = TRUE;

  if ((modifiers & ~ignored_modifiers & mask) == (state & ~ignored_modifiers & mask))
    {
      /* modifier match */
      GdkKeymapKey *keys;
      guint n_keys;
      int i;
      guint key;

      /* Shift gets consumed and applied for the event,
       * so apply it to our keyval to match
       */
      key = keyval;
      if (modifiers & GDK_SHIFT_MASK)
        {
          if (key == GDK_KEY_Tab)
            key = GDK_KEY_ISO_Left_Tab;
          else
            key = gdk_keyval_to_upper (key);
        }

      if (ev_keyval == key && /* exact match */
          (!group_mod_is_accel_mod ||
           (state & shift_group_mask) == (modifiers & shift_group_mask)))
        {
          return GDK_KEY_MATCH_EXACT;
        }

      keymap = gdk_display_get_keymap (gdk_event_get_display (event));
      gdk_keymap_get_cached_entries_for_keyval (keymap, keyval, &keys, &n_keys);

      for (i = 0; i < n_keys; i++)
        {
          if (keys[i].keycode == keycode &&
              keys[i].level == level &&
              /* Only match for group if it's an accel mod */
              (keys[i].group == layout ||
               (!group_mod_is_accel_mod && !keyval_in_group (keymap, keyval, layout))))
            return GDK_KEY_MATCH_PARTIAL;
        }
    }

  return GDK_KEY_MATCH_NONE;
}

/**
 * gdk_key_event_get_match:
 * @event: (type GdkKeyEvent): a key `GdkEvent`
 * @keyval: (out): return location for a keyval
 * @modifiers: (out): return location for modifiers
 *
 * Gets a keyval and modifier combination that will match
 * the event.
 *
 * See [method@Gdk.KeyEvent.matches].
 *
 * Returns: %TRUE on success
 */
gboolean
gdk_key_event_get_match (GdkEvent        *event,
                         guint           *keyval,
                         GdkModifierType *modifiers)
{
  GdkKeyEvent *self = (GdkKeyEvent *) event;
  guint key;
  guint accel_key;
  GdkModifierType accel_mods;
  GdkModifierType consumed_modifiers;
  const GdkModifierType mask = GDK_CONTROL_MASK |
                               GDK_SHIFT_MASK |
                               GDK_ALT_MASK |
                               GDK_SUPER_MASK |
                               GDK_HYPER_MASK |
                               GDK_META_MASK;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_KEY_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_KEY_RELEASE), FALSE);

  accel_key = self->translated[1].keyval;
  accel_mods = self->state;
  consumed_modifiers = self->translated[1].consumed;

  if (accel_key == GDK_KEY_Sys_Req &&
      (accel_mods & GDK_ALT_MASK) != 0)
    {
      /* HACK: we don't want to use SysRq as a keybinding (but we do
       * want Alt+Print), so we avoid translation from Alt+Print to SysRq
       */
      *keyval = GDK_KEY_Print;
      *modifiers = accel_mods & mask;
      return TRUE;
    }

  key = gdk_keyval_to_lower (accel_key);

  if (key == GDK_KEY_ISO_Left_Tab)
    key = GDK_KEY_Tab;

  accel_mods &= mask & ~consumed_modifiers;

  if (accel_key != key)
    accel_mods |= GDK_SHIFT_MASK;

  *keyval = key;
  *modifiers = accel_mods;

  return TRUE;
}

/* }}} */
/* {{{ GdkTouchEvent */

/**
 * GdkTouchEvent:
 *
 * An event related to a touch-based device.
 */

static void
gdk_touch_event_finalize (GdkEvent *event)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;

  g_clear_pointer (&self->axes, g_free);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static GdkModifierType
gdk_touch_event_get_state (GdkEvent *event)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;

  return self->state;
}

static gboolean
gdk_touch_event_get_position (GdkEvent *event,
                              double   *x,
                              double   *y)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static GdkEventSequence *
gdk_touch_event_get_sequence (GdkEvent *event)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;

  return self->sequence;
}

static gboolean
gdk_touch_event_get_axes (GdkEvent  *event,
                          double   **axes,
                          guint     *n_axes)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;
  GdkDevice *source_device = gdk_event_get_device (event);

  if (source_device == NULL)
    return FALSE;

  *axes = self->axes;
  *n_axes = GDK_AXIS_LAST;

  return TRUE;
}

static const GdkEventTypeInfo gdk_touch_event_info = {
  sizeof (GdkTouchEvent),
  NULL,
  gdk_touch_event_finalize,
  gdk_touch_event_get_state,
  gdk_touch_event_get_position,
  gdk_touch_event_get_sequence,
  NULL,
  gdk_touch_event_get_axes,
};

GDK_DEFINE_EVENT_TYPE (GdkTouchEvent, gdk_touch_event,
                       &gdk_touch_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCH_BEGIN)
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCH_END)
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCH_UPDATE)
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCH_CANCEL))

GdkEvent *
gdk_touch_event_new (GdkEventType      type,
                     GdkEventSequence *sequence,
                     GdkSurface       *surface,
                     GdkDevice        *device,
                     guint32           time,
                     GdkModifierType   state,
                     double            x,
                     double            y,
                     double           *axes,
                     gboolean          emulating)
{
  GdkTouchEvent *self;

  g_return_val_if_fail (type == GDK_TOUCH_BEGIN ||
                        type == GDK_TOUCH_END ||
                        type == GDK_TOUCH_UPDATE ||
                        type == GDK_TOUCH_CANCEL, NULL);

  self = gdk_event_alloc (type, surface, device, time);
  self->sequence = sequence;
  self->state = state;
  self->x = x;
  self->y = y;
  self->axes = axes;
  self->touch_emulating = emulating;
  self->pointer_emulated = emulating;

  return (GdkEvent *) self;
}

/**
 * gdk_touch_event_get_emulating_pointer:
 * @event: (type GdkTouchEvent): a touch event
 *
 * Extracts whether a touch event is emulating a pointer event.
 *
 * Returns: %TRUE if @event is emulating
 **/
gboolean
gdk_touch_event_get_emulating_pointer (GdkEvent *event)
{
  GdkTouchEvent *self = (GdkTouchEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCH_BEGIN) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCH_UPDATE) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCH_END) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCH_CANCEL), FALSE);

  return self->touch_emulating;
}

/* }}} */
/* {{{ GdkCrossingEvent */

/**
 * GdkCrossingEvent:
 *
 * An event caused by a pointing device moving between surfaces.
 */

static void
gdk_crossing_event_finalize (GdkEvent *event)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  g_clear_object (&self->child_surface);

  GDK_EVENT_SUPER (self)->finalize (event);
}

static GdkModifierType
gdk_crossing_event_get_state (GdkEvent *event)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  return self->state;
}

static gboolean
gdk_crossing_event_get_position (GdkEvent *event,
                                 double   *x,
                                 double   *y)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static const GdkEventTypeInfo gdk_crossing_event_info = {
  sizeof (GdkCrossingEvent),
  NULL,
  gdk_crossing_event_finalize,
  gdk_crossing_event_get_state,
  gdk_crossing_event_get_position,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkCrossingEvent, gdk_crossing_event,
                       &gdk_crossing_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_ENTER_NOTIFY)
                       GDK_EVENT_TYPE_SLOT (GDK_LEAVE_NOTIFY))

GdkEvent *
gdk_crossing_event_new (GdkEventType     type,
                        GdkSurface      *surface,
                        GdkDevice       *device,
                        guint32          time,
                        GdkModifierType  state,
                        double           x,
                        double           y,
                        GdkCrossingMode  mode,
                        GdkNotifyType    detail)
{
  GdkCrossingEvent *self;

  g_return_val_if_fail (type == GDK_ENTER_NOTIFY ||
                        type == GDK_LEAVE_NOTIFY, NULL);

  self = gdk_event_alloc (type, surface, device, time);

  self->state = state;
  self->x = x;
  self->y = y;
  self->mode = mode;
  self->detail = detail;

  return (GdkEvent *) self;
}

/**
 * gdk_crossing_event_get_mode:
 * @event: (type GdkCrossingEvent): a crossing event
 *
 * Extracts the crossing mode from a crossing event.
 *
 * Returns: the mode of @event
 */
GdkCrossingMode
gdk_crossing_event_get_mode (GdkEvent *event)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_ENTER_NOTIFY) ||
                        GDK_IS_EVENT_TYPE (event, GDK_LEAVE_NOTIFY), 0);

  return self->mode;
}

/**
 * gdk_crossing_event_get_focus:
 * @event: (type GdkCrossingEvent): a crossing event
 *
 * Checks if the @event surface is the focus surface.
 *
 * Returns: %TRUE if the surface is the focus surface
 */
gboolean
gdk_crossing_event_get_focus (GdkEvent *event)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_ENTER_NOTIFY) ||
                        GDK_IS_EVENT_TYPE (event, GDK_LEAVE_NOTIFY), FALSE);

  return self->focus;
}

/**
 * gdk_crossing_event_get_detail:
 * @event: (type GdkCrossingEvent): a crossing event
 *
 * Extracts the notify detail from a crossing event.
 *
 * Returns: the notify detail of @event
 */
GdkNotifyType
gdk_crossing_event_get_detail (GdkEvent *event)
{
  GdkCrossingEvent *self = (GdkCrossingEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_ENTER_NOTIFY) ||
                        GDK_IS_EVENT_TYPE (event, GDK_LEAVE_NOTIFY), 0);

  return self->detail;
}

/* }}} */
/* {{{ GdkDeleteEvent */

/**
 * GdkDeleteEvent:
 *
 * An event related to closing a top-level surface.
 */

static const GdkEventTypeInfo gdk_delete_event_info = {
  sizeof (GdkDeleteEvent),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkDeleteEvent, gdk_delete_event,
                       &gdk_delete_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_DELETE))

GdkEvent *
gdk_delete_event_new (GdkSurface *surface)
{
  return gdk_event_alloc (GDK_DELETE, surface, NULL, GDK_CURRENT_TIME);
}

/* }}} */
/* {{{ GdkFocusEvent */

/**
 * GdkFocusEvent:
 *
 * An event related to a keyboard focus change.
 */

static const GdkEventTypeInfo gdk_focus_event_info = {
  sizeof (GdkFocusEvent),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkFocusEvent, gdk_focus_event,
                       &gdk_focus_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_FOCUS_CHANGE))

GdkEvent *
gdk_focus_event_new (GdkSurface *surface,
                     GdkDevice  *device,
                     gboolean    focus_in)
{
  GdkFocusEvent *self = gdk_event_alloc (GDK_FOCUS_CHANGE, surface, device, GDK_CURRENT_TIME);

  self->focus_in = focus_in;

  return (GdkEvent *) self;
}

/**
 * gdk_focus_event_get_in:
 * @event: (type GdkFocusEvent): a focus change event
 *
 * Extracts whether this event is about focus entering or
 * leaving the surface.
 *
 * Returns: %TRUE of the focus is entering
 */
gboolean
gdk_focus_event_get_in (GdkEvent *event)
{
  GdkFocusEvent *self = (GdkFocusEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_FOCUS_CHANGE), FALSE);

  return self->focus_in;
}

/* }}} */
/* {{{ GdkScrollEvent */

/**
 * GdkScrollEvent:
 *
 * An event related to a scrolling motion.
 */

static void
gdk_scroll_event_finalize (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  g_clear_object (&self->tool);
  if (self->history)
    g_array_free (self->history, TRUE);

  GDK_EVENT_SUPER (self)->finalize (event);
}

static GdkModifierType
gdk_scroll_event_get_state (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  return self->state;
}

static GdkDeviceTool *
gdk_scroll_event_get_tool (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  return self->tool;
}

static const GdkEventTypeInfo gdk_scroll_event_info = {
  sizeof (GdkScrollEvent),
  NULL,
  gdk_scroll_event_finalize,
  gdk_scroll_event_get_state,
  NULL,
  NULL,
  gdk_scroll_event_get_tool,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkScrollEvent, gdk_scroll_event,
                       &gdk_scroll_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_SCROLL))

GdkEvent *
gdk_scroll_event_new (GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      double           delta_x,
                      double           delta_y,
                      gboolean         is_stop,
                      GdkScrollUnit    unit)
{
  GdkScrollEvent *self = gdk_event_alloc (GDK_SCROLL, surface, device, time);

  self->tool = tool != NULL ? g_object_ref (tool) : NULL;
  self->state = state;
  self->direction = GDK_SCROLL_SMOOTH;
  self->delta_x = delta_x;
  self->delta_y = delta_y;
  self->is_stop = is_stop;
  self->unit = unit;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_scroll_event_new_discrete (GdkSurface         *surface,
                               GdkDevice          *device,
                               GdkDeviceTool      *tool,
                               guint32             time,
                               GdkModifierType     state,
                               GdkScrollDirection  direction)
{
  GdkScrollEvent *self = gdk_event_alloc (GDK_SCROLL, surface, device, time);
  double delta_x = 0, delta_y = 0;

  switch (direction)
    {
    case GDK_SCROLL_UP:
      delta_y = -1;
      break;
    case GDK_SCROLL_DOWN:
      delta_y = 1;
      break;
    case GDK_SCROLL_LEFT:
      delta_x = -1;
      break;
    case GDK_SCROLL_RIGHT:
      delta_x = 1;
      break;
    case GDK_SCROLL_SMOOTH:
    default:
      g_assert_not_reached ();
      break;
    }

  self->tool = tool != NULL ? g_object_ref (tool) : NULL;
  self->state = state;
  self->direction = direction;
  self->delta_x = delta_x;
  self->delta_y = delta_y;
  self->unit = GDK_SCROLL_UNIT_WHEEL;

  return (GdkEvent *) self;
}

/*< private >
 * gtk_scroll_event_new_value120:
 * @surface: the `GdkSurface` of the event
 * @device: the `GdkDevice` of the event
 * @tool: (nullable): the tool that generated to event
 * @time: the event serial
 * @state: Flags to indicate the state of modifier keys and mouse buttons
 *   in events.
 * @direction: scroll direction.
 * @delta_x: delta on the X axis in the 120.0 scale
 * @delta_x: delta on the Y axis in the 120.0 scale
 *
 * Creates a new discrete GdkScrollEvent for high resolution mouse wheels.
 *
 * Both axes send data in fractions of 120 where each multiple of 120
 * amounts to one logical scroll event. Fractions of 120 indicate a wheel
 * movement less than one detent.
 *
 * Returns: the newly created scroll event
 */
GdkEvent *
gdk_scroll_event_new_value120 (GdkSurface         *surface,
                               GdkDevice          *device,
                               GdkDeviceTool      *tool,
                               guint32             time,
                               GdkModifierType     state,
                               GdkScrollDirection  direction,
                               double              delta_x,
                               double              delta_y)
{
  GdkScrollEvent *self = gdk_event_alloc (GDK_SCROLL, surface, device, time);

  self->tool = tool != NULL ? g_object_ref (tool) : NULL;
  self->state = state;
  self->direction = direction;
  self->delta_x = delta_x / 120.0;
  self->delta_y = delta_y / 120.0;
  self->unit = GDK_SCROLL_UNIT_WHEEL;

  return (GdkEvent *) self;
}

/**
 * gdk_scroll_event_get_direction:
 * @event: (type GdkScrollEvent): a scroll event
 *
 * Extracts the direction of a scroll event.
 *
 * Returns: the scroll direction of @event
 */
GdkScrollDirection
gdk_scroll_event_get_direction (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_SCROLL), 0);

  return self->direction;
}

/**
 * gdk_scroll_event_get_deltas:
 * @event: (type GdkScrollEvent): a scroll event
 * @delta_x: (out): return location for x scroll delta
 * @delta_y: (out): return location for y scroll delta
 *
 * Extracts the scroll deltas of a scroll event.
 *
 * The deltas will be zero unless the scroll direction
 * is %GDK_SCROLL_SMOOTH.
 *
 * For the representation unit of these deltas, see
 * [method@Gdk.ScrollEvent.get_unit].
 */
void
gdk_scroll_event_get_deltas (GdkEvent *event,
                             double   *delta_x,
                             double   *delta_y)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  g_return_if_fail (GDK_IS_EVENT (event));
  g_return_if_fail (GDK_IS_EVENT_TYPE (event, GDK_SCROLL));

  *delta_x = self->delta_x;
  *delta_y = self->delta_y;
}

/**
 * gdk_scroll_event_is_stop:
 * @event: (type GdkScrollEvent): a scroll event
 *
 * Check whether a scroll event is a stop scroll event.
 *
 * Scroll sequences with smooth scroll information may provide
 * a stop scroll event once the interaction with the device finishes,
 * e.g. by lifting a finger. This stop scroll event is the signal
 * that a widget may trigger kinetic scrolling based on the current
 * velocity.
 *
 * Stop scroll events always have a delta of 0/0.
 *
 * Returns: %TRUE if the event is a scroll stop event
 */
gboolean
gdk_scroll_event_is_stop (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_SCROLL), FALSE);

  return self->is_stop;
}

/**
 * gdk_scroll_event_get_unit:
 * @event: (type GdkScrollEvent): a scroll event.
 *
 * Extracts the scroll delta unit of a scroll event.
 *
 * The unit will always be %GDK_SCROLL_UNIT_WHEEL if the scroll direction is not
 * %GDK_SCROLL_SMOOTH.
 *
 * Returns: the scroll unit.
 *
 * Since: 4.8
 */
GdkScrollUnit
gdk_scroll_event_get_unit (GdkEvent *event)
{
  GdkScrollEvent *self = (GdkScrollEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), GDK_SCROLL_UNIT_WHEEL);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_SCROLL),
                        GDK_SCROLL_UNIT_WHEEL);

  return self->unit;
}

/* }}} */
/* {{{ GdkTouchpadEvent */

/**
 * GdkTouchpadEvent:
 *
 * An event related to a gesture on a touchpad device.
 *
 * Unlike touchscreens, where the windowing system sends basic
 * sequences of begin, update, end events, and leaves gesture
 * recognition to the clients, touchpad gestures are typically
 * processed by the system, resulting in these events.
 */

static GdkModifierType
gdk_touchpad_event_get_state (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  return self->state;
}

static GdkEventSequence *
gdk_touchpad_event_get_sequence (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  return self->sequence;
}

static gboolean
gdk_touchpad_event_get_position (GdkEvent *event,
                                 double   *x,
                                 double   *y)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static const GdkEventTypeInfo gdk_touchpad_event_info = {
  sizeof (GdkTouchpadEvent),
  NULL,
  NULL,
  gdk_touchpad_event_get_state,
  gdk_touchpad_event_get_position,
  gdk_touchpad_event_get_sequence,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkTouchpadEvent, gdk_touchpad_event,
                       &gdk_touchpad_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCHPAD_SWIPE)
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCHPAD_PINCH)
                       GDK_EVENT_TYPE_SLOT (GDK_TOUCHPAD_HOLD))

GdkEvent *
gdk_touchpad_event_new_swipe (GdkSurface              *surface,
                              GdkEventSequence        *sequence,
                              GdkDevice               *device,
                              guint32                  time,
                              GdkModifierType          state,
                              GdkTouchpadGesturePhase  phase,
                              double                   x,
                              double                   y,
                              int                      n_fingers,
                              double                   dx,
                              double                   dy)
{
  GdkTouchpadEvent *self;

  g_return_val_if_fail (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_END ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL, NULL);

  self = gdk_event_alloc (GDK_TOUCHPAD_SWIPE, surface, device, time);

  self->sequence = sequence;
  self->state = state;
  self->phase = phase;
  self->x = x;
  self->y = y;
  self->dx = dx;
  self->dy = dy;
  self->n_fingers = n_fingers;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_touchpad_event_new_pinch (GdkSurface              *surface,
                              GdkEventSequence        *sequence,
                              GdkDevice               *device,
                              guint32                  time,
                              GdkModifierType          state,
                              GdkTouchpadGesturePhase  phase,
                              double                   x,
                              double                   y,
                              int                      n_fingers,
                              double                   dx,
                              double                   dy,
                              double                   scale,
                              double                   angle_delta)
{
  GdkTouchpadEvent *self;

  g_return_val_if_fail (phase == GDK_TOUCHPAD_GESTURE_PHASE_BEGIN ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_END ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_UPDATE ||
                        phase == GDK_TOUCHPAD_GESTURE_PHASE_CANCEL, NULL);

  self = gdk_event_alloc (GDK_TOUCHPAD_PINCH, surface, device, time);

  self->sequence = sequence;
  self->state = state;
  self->phase = phase;
  self->x = x;
  self->y = y;
  self->dx = dx;
  self->dy = dy;
  self->n_fingers = n_fingers;
  self->scale = scale;
  self->angle_delta = angle_delta;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_touchpad_event_new_hold (GdkSurface              *surface,
                             GdkEventSequence        *sequence,
                             GdkDevice               *device,
                             guint32                  time,
                             GdkModifierType          state,
                             GdkTouchpadGesturePhase  phase,
                             double                   x,
                             double                   y,
                             int                      n_fingers)
{
  GdkTouchpadEvent *self = gdk_event_alloc (GDK_TOUCHPAD_HOLD, surface, device, time);

  self->state = state;
  self->phase = phase;
  self->sequence = sequence;
  self->x = x;
  self->y = y;
  self->n_fingers = n_fingers;

  return (GdkEvent *) self;
}

/**
 * gdk_touchpad_event_get_gesture_phase:
 * @event: (type GdkTouchpadEvent): a touchpad event
 *
 * Extracts the touchpad gesture phase from a touchpad event.
 *
 * Returns: the gesture phase of @event
 **/
GdkTouchpadGesturePhase
gdk_touchpad_event_get_gesture_phase (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_PINCH) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_SWIPE) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_HOLD), 0);

  return self->phase;
}

/**
 * gdk_touchpad_event_get_n_fingers:
 * @event: (type GdkTouchpadEvent): a touchpad event
 *
 * Extracts the number of fingers from a touchpad event.
 *
 * Returns: the number of fingers for @event
 **/
guint
gdk_touchpad_event_get_n_fingers (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_PINCH) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_SWIPE) ||
                        GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_HOLD), 0);

  return self->n_fingers;
}

/**
 * gdk_touchpad_event_get_deltas:
 * @event: (type GdkTouchpadEvent): a touchpad event
 * @dx: (out): return location for x
 * @dy: (out): return location for y
 *
 * Extracts delta information from a touchpad event.
 */
void
gdk_touchpad_event_get_deltas (GdkEvent *event,
                               double   *dx,
                               double   *dy)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  g_return_if_fail (GDK_IS_EVENT (event));
  g_return_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_PINCH) ||
                    GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_SWIPE));

  *dx = self->dx;
  *dy = self->dy;
}

/**
 * gdk_touchpad_event_get_pinch_angle_delta:
 * @event: (type GdkTouchpadEvent): a touchpad pinch event
 *
 * Extracts the angle delta from a touchpad pinch event.
 *
 * Returns: the angle delta of @event
 */
double
gdk_touchpad_event_get_pinch_angle_delta (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0.0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_PINCH), 0.0);

  return self->angle_delta;
}

/**
 * gdk_touchpad_event_get_pinch_scale:
 * @event: (type GdkTouchpadEvent): a touchpad pinch event
 *
 * Extracts the scale from a touchpad pinch event.
 *
 * Returns: the scale of @event
 **/
double
gdk_touchpad_event_get_pinch_scale (GdkEvent *event)
{
  GdkTouchpadEvent *self = (GdkTouchpadEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0.0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_TOUCHPAD_PINCH), 0.0);

  return self->scale;
}

/* }}} */
/* {{{ GdkPadEvent */

/**
 * GdkPadEvent:
 *
 * An event related to a pad-based device.
 */

static const GdkEventTypeInfo gdk_pad_event_info = {
  sizeof (GdkPadEvent),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkPadEvent, gdk_pad_event,
                       &gdk_pad_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_PAD_BUTTON_PRESS)
                       GDK_EVENT_TYPE_SLOT (GDK_PAD_BUTTON_RELEASE)
                       GDK_EVENT_TYPE_SLOT (GDK_PAD_RING)
                       GDK_EVENT_TYPE_SLOT (GDK_PAD_STRIP)
                       GDK_EVENT_TYPE_SLOT (GDK_PAD_GROUP_MODE))

GdkEvent *
gdk_pad_event_new_ring (GdkSurface *surface,
                        GdkDevice  *device,
                        guint32     time,
                        guint       group,
                        guint       index,
                        guint       mode,
                        double      value)
{
  GdkPadEvent *self = gdk_event_alloc (GDK_PAD_RING, surface, device, time);

  self->group = group;
  self->index = index;
  self->mode = mode;
  self->value = value;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_pad_event_new_strip (GdkSurface *surface,
                         GdkDevice  *device,
                         guint32     time,
                         guint       group,
                         guint       index,
                         guint       mode,
                         double      value)
{
  GdkPadEvent *self = gdk_event_alloc (GDK_PAD_STRIP, surface, device, time);

  self->group = group;
  self->index = index;
  self->mode = mode;
  self->value = value;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_pad_event_new_button (GdkEventType  type,
                          GdkSurface   *surface,
                          GdkDevice    *device,
                          guint32       time,
                          guint         group,
                          guint         button,
                          guint         mode)
{
  GdkPadEvent *self;

  g_return_val_if_fail (type == GDK_PAD_BUTTON_PRESS ||
                        type == GDK_PAD_BUTTON_RELEASE, NULL);

  self = gdk_event_alloc (type, surface, device, time);

  self->group = group;
  self->button = button;
  self->mode = mode;

  return (GdkEvent *) self;
}

GdkEvent *
gdk_pad_event_new_group_mode (GdkSurface *surface,
                              GdkDevice  *device,
                              guint32     time,
                              guint       group,
                              guint       mode)
{
  GdkPadEvent *self = gdk_event_alloc (GDK_PAD_GROUP_MODE, surface, device, time);

  self->group = group;
  self->mode = mode;

  return (GdkEvent *) self;
}
/**
 * gdk_pad_event_get_button:
 * @event: (type GdkPadEvent): a pad button event
 *
 * Extracts information about the pressed button from
 * a pad event.
 *
 * Returns: the button of @event
 */
guint
gdk_pad_event_get_button (GdkEvent *event)
{
  GdkPadEvent *self = (GdkPadEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), 0);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_PAD_BUTTON_PRESS) ||
                        GDK_IS_EVENT_TYPE (event, GDK_PAD_BUTTON_RELEASE), 0);

  return self->button;
}

/**
 * gdk_pad_event_get_axis_value:
 * @event: (type GdkPadEvent): a pad strip or ring event
 * @index: (out): Return location for the axis index
 * @value: (out): Return location for the axis value
 *
 * Extracts the information from a pad strip or ring event.
 */
void
gdk_pad_event_get_axis_value (GdkEvent *event,
                              guint    *index,
                              double   *value)
{
  GdkPadEvent *self = (GdkPadEvent *) event;

  g_return_if_fail (GDK_IS_EVENT (event));
  g_return_if_fail (GDK_IS_EVENT_TYPE (event, GDK_PAD_RING) ||
                    GDK_IS_EVENT_TYPE (event, GDK_PAD_STRIP));

  *index = self->index;
  *value = self->value;
}

/**
 * gdk_pad_event_get_group_mode:
 * @event: (type GdkPadEvent): a pad event
 * @group: (out): return location for the group
 * @mode: (out): return location for the mode
 *
 * Extracts group and mode information from a pad event.
 */
void
gdk_pad_event_get_group_mode (GdkEvent *event,
                              guint    *group,
                              guint    *mode)
{
  GdkPadEvent *self = (GdkPadEvent *) event;

  g_return_if_fail (GDK_IS_EVENT (event));
  g_return_if_fail (GDK_IS_EVENT_TYPE (event, GDK_PAD_GROUP_MODE) ||
                    GDK_IS_EVENT_TYPE (event, GDK_PAD_BUTTON_PRESS) ||
                    GDK_IS_EVENT_TYPE (event, GDK_PAD_BUTTON_RELEASE) ||
                    GDK_IS_EVENT_TYPE (event, GDK_PAD_RING) ||
                    GDK_IS_EVENT_TYPE (event, GDK_PAD_STRIP));

  *group = self->group;
  *mode = self->mode;
}

/* }}} */
/* {{{ GdkMotionEvent */

/**
 * GdkMotionEvent:
 *
 * An event related to a pointer or touch device motion.
 */

static void
gdk_motion_event_finalize (GdkEvent *event)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;

  g_clear_object (&self->tool);
  g_clear_pointer (&self->axes, g_free);
  if (self->history)
    g_array_free (self->history, TRUE);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static GdkModifierType
gdk_motion_event_get_state (GdkEvent *event)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;

  return self->state;
}

static gboolean
gdk_motion_event_get_position (GdkEvent *event,
                               double   *x,
                               double   *y)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static GdkDeviceTool *
gdk_motion_event_get_tool (GdkEvent *event)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;

  return self->tool;
}

static gboolean
gdk_motion_event_get_axes (GdkEvent  *event,
                           double   **axes,
                           guint     *n_axes)
{
  GdkMotionEvent *self = (GdkMotionEvent *) event;
  GdkDevice *source_device = gdk_event_get_device (event);

  if (source_device == NULL)
    return FALSE;

  *axes = self->axes;
  *n_axes = GDK_AXIS_LAST;

  return TRUE;
}

static const GdkEventTypeInfo gdk_motion_event_info = {
  sizeof (GdkMotionEvent),
  NULL,
  gdk_motion_event_finalize,
  gdk_motion_event_get_state,
  gdk_motion_event_get_position,
  NULL,
  gdk_motion_event_get_tool,
  gdk_motion_event_get_axes,
};

GDK_DEFINE_EVENT_TYPE (GdkMotionEvent, gdk_motion_event,
                       &gdk_motion_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_MOTION_NOTIFY))

GdkEvent *
gdk_motion_event_new (GdkSurface      *surface,
                      GdkDevice       *device,
                      GdkDeviceTool   *tool,
                      guint32          time,
                      GdkModifierType  state,
                      double           x,
                      double           y,
                      double          *axes)
{
  GdkMotionEvent *self = gdk_event_alloc (GDK_MOTION_NOTIFY, surface, device, time);

  self->tool = tool ? g_object_ref (tool) : NULL;
  self->state = state;
  self->x = x;
  self->y = y;
  self->axes = axes;
  self->state = state;

  return (GdkEvent *) self;
}

/**
 * gdk_event_get_history:
 * @event: a motion or scroll event
 * @out_n_coords: (out): Return location for the length of the returned array
 *
 * Retrieves the history of the device that @event is for, as a list of
 * time and coordinates.
 *
 * The history includes positions that are not delivered as separate events
 * to the application because they occurred in the same frame as @event.
 *
 * Note that only motion and scroll events record history, and motion
 * events do it only if one of the mouse buttons is down, or the device
 * has a tool.
 *
 * Returns: (transfer container) (array length=out_n_coords) (nullable): an
 *   array of time and coordinates
 */
GdkTimeCoord *
gdk_event_get_history (GdkEvent *event,
                       guint    *out_n_coords)
{
  GArray *history;

  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_MOTION_NOTIFY) ||
                        GDK_IS_EVENT_TYPE (event, GDK_SCROLL), NULL);
  g_return_val_if_fail (out_n_coords != NULL, NULL);

  if (GDK_IS_EVENT_TYPE (event, GDK_MOTION_NOTIFY))
    {
      GdkMotionEvent *self = (GdkMotionEvent *) event;
      history = self->history;
    }
  else
    {
      GdkScrollEvent *self = (GdkScrollEvent *) event;
      history = self->history;
    }

  if (history && history->len > 0)
    {
      GdkTimeCoord *result;

      *out_n_coords = history->len;

      result = g_malloc (sizeof (GdkTimeCoord) * history->len);
      memcpy (result, history->data, sizeof (GdkTimeCoord) * history->len);

      return result;
    }

  *out_n_coords = 0;
  return NULL;
}

/* }}} */
/* {{{ GdkProximityEvent */

/**
 * GdkProximityEvent:
 *
 * An event related to the proximity of a tool to a device.
 */

static void
gdk_proximity_event_finalize (GdkEvent *event)
{
  GdkProximityEvent *self = (GdkProximityEvent *) event;

  g_clear_object (&self->tool);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static GdkDeviceTool *
gdk_proximity_event_get_tool (GdkEvent *event)
{
  GdkProximityEvent *self = (GdkProximityEvent *) event;

  return self->tool;
}

static const GdkEventTypeInfo gdk_proximity_event_info = {
  sizeof (GdkProximityEvent),
  NULL,
  gdk_proximity_event_finalize,
  NULL,
  NULL,
  NULL,
  gdk_proximity_event_get_tool,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkProximityEvent, gdk_proximity_event,
                       &gdk_proximity_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_PROXIMITY_IN)
                       GDK_EVENT_TYPE_SLOT (GDK_PROXIMITY_OUT))

GdkEvent *
gdk_proximity_event_new (GdkEventType   type,
                         GdkSurface    *surface,
                         GdkDevice     *device,
                         GdkDeviceTool *tool,
                         guint32        time)
{
  GdkProximityEvent *self;

  g_return_val_if_fail (type == GDK_PROXIMITY_IN ||
                        type == GDK_PROXIMITY_OUT, NULL);

  self = gdk_event_alloc (type, surface, device, time);

  self->tool = tool ? g_object_ref (tool) : NULL;

  return (GdkEvent *) self;
}

/* }}} */
/* {{{ GdkDNDEvent */

/**
 * GdkDNDEvent:
 *
 * An event related to drag and drop operations.
 */

static void
gdk_dnd_event_finalize (GdkEvent *event)
{
  GdkDNDEvent *self = (GdkDNDEvent *) event;

  g_clear_object (&self->drop);

  GDK_EVENT_SUPER (event)->finalize (event);
}

static gboolean
gdk_dnd_event_get_position (GdkEvent *event,
                            double   *x,
                            double   *y)
{
  GdkDNDEvent *self = (GdkDNDEvent *) event;

  *x = self->x;
  *y = self->y;

  return TRUE;
}

static GdkEventSequence *
gdk_dnd_event_get_sequence (GdkEvent *event)
{
  GdkDNDEvent *self = (GdkDNDEvent *) event;

  return (GdkEventSequence *) self->drop;
}

static const GdkEventTypeInfo gdk_dnd_event_info = {
  sizeof (GdkDNDEvent),
  NULL,
  gdk_dnd_event_finalize,
  NULL,
  gdk_dnd_event_get_position,
  gdk_dnd_event_get_sequence,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkDNDEvent, gdk_dnd_event,
                       &gdk_dnd_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_DRAG_ENTER)
                       GDK_EVENT_TYPE_SLOT (GDK_DRAG_MOTION)
                       GDK_EVENT_TYPE_SLOT (GDK_DRAG_LEAVE)
                       GDK_EVENT_TYPE_SLOT (GDK_DROP_START))

GdkEvent *
gdk_dnd_event_new (GdkEventType  type,
                   GdkSurface   *surface,
                   GdkDevice    *device,
                   GdkDrop      *drop,
                   guint32       time,
                   double        x,
                   double        y)
{
  GdkDNDEvent *self;

  g_return_val_if_fail (type == GDK_DRAG_ENTER ||
                        type == GDK_DRAG_MOTION ||
                        type == GDK_DRAG_LEAVE ||
                        type == GDK_DROP_START, NULL);

  self = gdk_event_alloc (type, surface, device, time);

  self->drop = drop != NULL ? g_object_ref (drop) : NULL;
  self->x = x;
  self->y = y;

  return (GdkEvent *) self;
}

/**
 * gdk_dnd_event_get_drop:
 * @event: (type GdkDNDEvent): a DND event
 *
 * Gets the `GdkDrop` object from a DND event.
 *
 * Returns: (transfer none) (nullable): the drop
 **/
GdkDrop *
gdk_dnd_event_get_drop (GdkEvent *event)
{
  GdkDNDEvent *self = (GdkDNDEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_DRAG_ENTER) ||
                        GDK_IS_EVENT_TYPE (event, GDK_DRAG_MOTION) ||
                        GDK_IS_EVENT_TYPE (event, GDK_DRAG_LEAVE) ||
                        GDK_IS_EVENT_TYPE (event, GDK_DROP_START), NULL);

  return self->drop;
}

/* }}} */
/* {{{ GdkGrabBrokenEvent */

/**
 * GdkGrabBrokenEvent:
 *
 * An event related to a broken windowing system grab.
 */

static const GdkEventTypeInfo gdk_grab_broken_event_info = {
  sizeof (GdkGrabBrokenEvent),
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

GDK_DEFINE_EVENT_TYPE (GdkGrabBrokenEvent, gdk_grab_broken_event,
                       &gdk_grab_broken_event_info,
                       GDK_EVENT_TYPE_SLOT (GDK_GRAB_BROKEN))

GdkEvent *
gdk_grab_broken_event_new (GdkSurface *surface,
                           GdkDevice  *device,
                           GdkSurface *grab_surface,
                           gboolean    implicit)
{
  GdkGrabBrokenEvent *self = gdk_event_alloc (GDK_GRAB_BROKEN, surface, device, GDK_CURRENT_TIME);

  self->grab_surface = grab_surface;
  self->implicit = implicit;
  self->keyboard = gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD;

  return (GdkEvent *) self;
}

/**
 * gdk_grab_broken_event_get_grab_surface:
 * @event: (type GdkGrabBrokenEvent): a grab broken event
 *
 * Extracts the grab surface from a grab broken event.
 *
 * Returns: (transfer none): the grab surface of @event
 **/
GdkSurface *
gdk_grab_broken_event_get_grab_surface (GdkEvent *event)
{
  GdkGrabBrokenEvent *self = (GdkGrabBrokenEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), NULL);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_GRAB_BROKEN), NULL);

  return self->grab_surface;
}

/**
 * gdk_grab_broken_event_get_implicit:
 * @event: (type GdkGrabBrokenEvent): a grab broken event
 *
 * Checks whether the grab broken event is for an implicit grab.
 *
 * Returns: %TRUE if the an implicit grab was broken
 */
gboolean
gdk_grab_broken_event_get_implicit (GdkEvent *event)
{
  GdkGrabBrokenEvent *self = (GdkGrabBrokenEvent *) event;

  g_return_val_if_fail (GDK_IS_EVENT (event), FALSE);
  g_return_val_if_fail (GDK_IS_EVENT_TYPE (event, GDK_GRAB_BROKEN), FALSE);

  return self->implicit;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
