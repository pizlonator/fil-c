/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2022, 2023 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstsourcebufferlist
 * @title: GstSourceBufferList
 * @short_description: Source Buffer List
 * @include: mse/mse.h
 * @symbols:
 * - GstSourceBufferList
 *
 * The Source Buffer List is a list of #GstSourceBuffer<!-- -->s that can be
 * indexed numerically and monitored for changes. The list itself cannot be
 * modified through this interface, though the Source Buffers it holds can be
 * modified after retrieval.
 *
 * It is used by #GstMediaSource to provide direct access to its child
 * #GstSourceBuffer<!-- -->s through #GstMediaSource:source-buffers as well as
 * informing clients which of the Source Buffers are active through
 * #GstMediaSource:active-source-buffers.
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsourcebufferlist.h"
#include "gstsourcebufferlist-private.h"

#include "gstmseeventqueue-private.h"

typedef struct
{
  gboolean frozen;
  gboolean added;
  gboolean removed;
} PendingNotifications;

/**
 * GstSourceBufferList:
 *
 * Since: 1.24
 */
struct _GstSourceBufferList
{
  GstObject parent_instance;

  GPtrArray *buffers;

  GstMseEventQueue *event_queue;

  PendingNotifications pending_notifications;
};

G_DEFINE_TYPE (GstSourceBufferList, gst_source_buffer_list, GST_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_LENGTH,
  N_PROPS,
};

typedef enum
{
  ON_SOURCEBUFFER_ADDED,
  ON_SOURCEBUFFER_REMOVED,
  N_SIGNALS,
} SourceBufferListEvent;

typedef struct
{
  GstDataQueueItem item;
  SourceBufferListEvent event;
} SourceBufferListEventItem;

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS] = { 0 };

static gboolean
is_frozen (GstSourceBufferList * self)
{
  return g_atomic_int_get (&self->pending_notifications.frozen);
}

static void
set_frozen (GstSourceBufferList * self)
{
  g_atomic_int_set (&self->pending_notifications.frozen, TRUE);
}

static void
clear_frozen (GstSourceBufferList * self)
{
  g_atomic_int_set (&self->pending_notifications.frozen, FALSE);
}

static void
set_pending_added (GstSourceBufferList * self)
{
  g_atomic_int_set (&self->pending_notifications.added, TRUE);
}

static void
set_pending_removed (GstSourceBufferList * self)
{
  g_atomic_int_set (&self->pending_notifications.removed, TRUE);
}

static gboolean
clear_pending_added (GstSourceBufferList * self)
{
  return g_atomic_int_and (&self->pending_notifications.added, FALSE);
}

static gboolean
clear_pending_removed (GstSourceBufferList * self)
{
  return g_atomic_int_and (&self->pending_notifications.removed, FALSE);
}

static void
schedule_event (GstSourceBufferList * self, SourceBufferListEvent event)
{
  SourceBufferListEventItem item = {
    .item = {.destroy = g_free,.visible = TRUE,.size = 1,.object = NULL},
    .event = event,
  };

  gst_mse_event_queue_push (self->event_queue, g_memdup2 (&item,
          sizeof (SourceBufferListEventItem)));
}

static void
dispatch_event (SourceBufferListEventItem * item, GstSourceBufferList * self)
{
  g_signal_emit (self, signals[item->event], 0);
}

static void
call_source_buffer_added (GstSourceBufferList * self)
{
  if (is_frozen (self)) {
    set_pending_added (self);
  } else {
    clear_pending_added (self);
    schedule_event (self, ON_SOURCEBUFFER_ADDED);
  }
}

static void
call_source_buffer_removed (GstSourceBufferList * self)
{
  if (is_frozen (self)) {
    set_pending_removed (self);
  } else {
    clear_pending_removed (self);
    schedule_event (self, ON_SOURCEBUFFER_REMOVED);
  }
}

GstSourceBufferList *
gst_source_buffer_list_new (void)
{
  return gst_object_ref_sink (g_object_new (GST_TYPE_SOURCE_BUFFER_LIST, NULL));
}

static void
gst_source_buffer_list_dispose (GObject * object)
{
  GstSourceBufferList *self = (GstSourceBufferList *) object;
  gst_clear_object (&self->event_queue);

  G_OBJECT_CLASS (gst_source_buffer_list_parent_class)->dispose (object);
}

static void
gst_source_buffer_list_finalize (GObject * object)
{
  GstSourceBufferList *self = (GstSourceBufferList *) object;

  g_clear_pointer (&self->buffers, g_ptr_array_unref);

  G_OBJECT_CLASS (gst_source_buffer_list_parent_class)->finalize (object);
}

static void
gst_source_buffer_list_get_property (GObject * object, guint prop_id, GValue
    * value, GParamSpec * pspec)
{
  GstSourceBufferList *self = GST_SOURCE_BUFFER_LIST (object);

  switch (prop_id) {
    case PROP_LENGTH:
      g_value_set_uint (value, gst_source_buffer_list_get_length (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_source_buffer_list_class_init (GstSourceBufferListClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = GST_DEBUG_FUNCPTR (gst_source_buffer_list_dispose);
  oclass->finalize = GST_DEBUG_FUNCPTR (gst_source_buffer_list_finalize);
  oclass->get_property =
      GST_DEBUG_FUNCPTR (gst_source_buffer_list_get_property);

  /**
   * GstSourceBufferList:length:
   *
   * The number of #GstSourceBuffer<!-- -->s contained by this structure
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-sourcebufferlist-length)
   *
   * Since: 1.24
   */
  properties[PROP_LENGTH] = g_param_spec_ulong ("length",
      "Length",
      "The number of SourceBuffers contained by this structure",
      0, G_MAXULONG, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, N_PROPS, properties);

  /**
   * GstSourceBufferList::on-sourcebuffer-added:
   * @self: The #GstSourceBufferList that has just added a
   * #GstSourceBuffer
   *
   * Emitted when a #GstSourceBuffer has been added to this list.
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-sourcebufferlist-onaddsourcebuffer)
   *
   * Since: 1.24
   */
  signals[ON_SOURCEBUFFER_ADDED] = g_signal_new ("on-sourcebuffer-added",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstSourceBufferList::on-sourcebuffer-removed:
   * @self: The #GstSourceBufferList that has just removed a
   * #GstSourceBuffer
   *
   * Emitted when a #GstSourceBuffer has been removed from this list.
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-sourcebufferlist-onremovesourcebuffer)
   *
   * Since: 1.24
   */
  signals[ON_SOURCEBUFFER_REMOVED] = g_signal_new ("on-sourcebuffer-removed",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
gst_source_buffer_list_init (GstSourceBufferList * self)
{
  set_frozen (self);
  self->buffers = g_ptr_array_new_with_free_func (gst_object_unref);
  self->event_queue =
      gst_mse_event_queue_new ((GstMseEventQueueCallback) dispatch_event, self);
  clear_pending_added (self);
  clear_pending_removed (self);
  clear_frozen (self);
}

/**
 * gst_source_buffer_list_index:
 * @self: #GstSourceBufferList instance
 * @index: index of requested Source Buffer
 *
 * Retrieves the #GstSourceBuffer at @index from @self. If @index is greater than
 * the highest index in the list, it will return `NULL`.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dfn-sourcebufferlist-getter)
 *
 * Returns: (transfer full) (nullable): The requested #GstSourceBuffer or `NULL`
 * Since: 1.24
 */
GstSourceBuffer *
gst_source_buffer_list_index (GstSourceBufferList * self, guint index)
{
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER_LIST (self), NULL);
  if (index >= gst_source_buffer_list_get_length (self))
    return NULL;
  return gst_object_ref (g_ptr_array_index (self->buffers, index));
}

/**
 * gst_source_buffer_list_get_length:
 * @self: #GstSourceBufferList instance
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-sourcebufferlist-length)
 *
 * Returns: The number of #GstSourceBuffer objects in the list
 * Since: 1.24
 */
guint
gst_source_buffer_list_get_length (GstSourceBufferList * self)
{
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER_LIST (self), 0);
  return self->buffers->len;
}

gboolean
gst_source_buffer_list_contains (GstSourceBufferList * self,
    GstSourceBuffer * buf)
{
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER_LIST (self), FALSE);
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER (buf), FALSE);
  return g_ptr_array_find (self->buffers, buf, NULL);
}

void
gst_source_buffer_list_append (GstSourceBufferList * self,
    GstSourceBuffer * buf)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  g_ptr_array_add (self->buffers, gst_object_ref (buf));
  call_source_buffer_added (self);
}

gboolean
gst_source_buffer_list_remove (GstSourceBufferList * self,
    GstSourceBuffer * buf)
{
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER_LIST (self), FALSE);
  gboolean removed = g_ptr_array_remove (self->buffers, buf);
  if (removed) {
    call_source_buffer_removed (self);
    return TRUE;
  } else {
    return FALSE;
  }
}

void
gst_source_buffer_list_remove_all (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  if (self->buffers->len < 1) {
    return;
  }
  g_ptr_array_set_size (self->buffers, 0);
  call_source_buffer_removed (self);
}

/**
 * gst_source_buffer_list_notify_freeze:
 * @self: #GstSourceBufferList instance
 *
 * Prevents any notifications from being emitted by @self until the next call to
 * gst_source_buffer_list_notify_thaw().
 *
 */
void
gst_source_buffer_list_notify_freeze (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  clear_pending_added (self);
  clear_pending_removed (self);
  set_frozen (self);
}

/**
 * gst_source_buffer_list_notify_cancel:
 * @self: #GstSourceBufferList instance
 *
 * Cancels any pending notifications that are waiting between calls to
 * gst_source_buffer_list_notify_freeze() and
 * gst_source_buffer_list_notify_thaw().
 *
 */
void
gst_source_buffer_list_notify_cancel (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  clear_pending_added (self);
  clear_pending_removed (self);
}

/**
 * gst_source_buffer_list_notify_added:
 * @self: #GstSourceBufferList instance
 *
 * Explicitly notifies subscribers to the ::on-sourcebuffer-added signal that an
 * item has been added to @self.
 *
 */
void
gst_source_buffer_list_notify_added (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  g_return_if_fail (!is_frozen (self));
  call_source_buffer_added (self);
}

/**
 * gst_source_buffer_list_notify_removed:
 * @self: #GstSourceBufferList instance
 *
 * Explicitly notifies subscribers to the ::on-sourcebuffer-removed signal that
 * an item has been removed from @self.
 *
 */
void
gst_source_buffer_list_notify_removed (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  g_return_if_fail (!is_frozen (self));
  call_source_buffer_removed (self);
}

/**
 * gst_source_buffer_list_notify_thaw:
 * @self: #GstSourceBufferList instance
 *
 * Resumes notifications emitted from @self after a call to
 * gst_source_buffer_list_notify_freeze(). If any notifications are pending,
 * they will be emitted as a result of this call. To prevent pending
 * notifications from being published, use
 * gst_source_buffer_list_notify_cancel() before calling this method.
 *
 */
void
gst_source_buffer_list_notify_thaw (GstSourceBufferList * self)
{
  g_return_if_fail (GST_IS_SOURCE_BUFFER_LIST (self));
  clear_frozen (self);
  if (clear_pending_added (self)) {
    g_signal_emit (self, signals[ON_SOURCEBUFFER_ADDED], 0);
  }
  if (clear_pending_removed (self)) {
    g_signal_emit (self, signals[ON_SOURCEBUFFER_REMOVED], 0);
  }
}
