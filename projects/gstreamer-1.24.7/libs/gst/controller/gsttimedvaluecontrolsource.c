/* GStreamer
 *
 * Copyright (C) 2007,2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *               2011 Stefan Sauer <ensonic@users.sf.net>
 *
 * gsttimedvaluecontrolsource.c: Base class for timeed value based control
 *                               sources
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
 * SECTION:gsttimedvaluecontrolsource
 * @title: GstTimedValueControlSource
 * @short_description: timed value control source base class
 *
 * Base class for #GstControlSource that use time-stamped values.
 *
 * When overriding bind, chain up first to give this bind implementation a
 * chance to setup things.
 *
 * All functions are MT-safe.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib-object.h>
#include <gst/gst.h>

#include "gstinterpolationcontrolsource.h"
#include "gst/glib-compat-private.h"

#define GST_CAT_DEFAULT controller_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "timed value control source", 0, \
    "timed value control source base class")

#define gst_timed_value_control_source_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstTimedValueControlSource,
    gst_timed_value_control_source, GST_TYPE_CONTROL_SOURCE, _do_init);


enum
{
  VALUE_CHANGED_SIGNAL,
  VALUE_ADDED_SIGNAL,
  VALUE_REMOVED_SIGNAL,
  LAST_SIGNAL
};

static guint gst_timed_value_control_source_signals[LAST_SIGNAL] = { 0 };

/**
 * gst_control_point_free:
 * @cp: the object to free
 *
 * Frees all data allocated by a #GstControlPoint instance.
 */
void
gst_control_point_free (GstControlPoint * cp)
{
  g_return_if_fail (cp);

  g_free (cp);
}

/**
 * gst_control_point_copy:
 * @cp: The control point to copy
 *
 * Copies a #GstControlPoint
 *
 * Returns: (transfer full): A copy of @cp
 */
GstControlPoint *
gst_control_point_copy (GstControlPoint * cp)
{
  return g_memdup2 (cp, sizeof (GstControlPoint));
}

GType
gst_control_point_get_type (void)
{
  static gpointer type_id = 0;

  if (g_once_init_enter_pointer (&type_id)) {
    GType tmp =
        g_boxed_type_register_static (g_intern_static_string
        ("GstControlPoint"),
        (GBoxedCopyFunc) gst_control_point_copy,
        (GBoxedFreeFunc) gst_control_point_free);
    g_once_init_leave_pointer (&type_id, tmp);
  }

  return type_id;
}

static void
gst_timed_value_control_source_reset (GstTimedValueControlSource * self)
{
  GstControlSource *csource = (GstControlSource *) self;

  csource->get_value = NULL;
  csource->get_value_array = NULL;

  if (self->values) {
    g_sequence_free (self->values);
    self->values = NULL;
  }

  self->nvalues = 0;
  self->valid_cache = FALSE;
}

/*
 * gst_control_point_compare:
 * @p1: a pointer to a #GstControlPoint
 * @p2: a pointer to a #GstControlPoint
 *
 * Compare function for g_list operations that operates on two #GstControlPoint
 * parameters.
 */
static gint
gst_control_point_compare (gconstpointer p1, gconstpointer p2)
{
  GstClockTime ct1 = ((GstControlPoint *) p1)->timestamp;
  GstClockTime ct2 = ((GstControlPoint *) p2)->timestamp;

  return ((ct1 < ct2) ? -1 : ((ct1 == ct2) ? 0 : 1));
}

/*
 * gst_control_point_find:
 * @p1: a pointer to a #GstControlPoint
 * @p2: a pointer to a #GstClockTime
 * @user_data: supplied user data
 *
 * Compare function for g_sequence operations that operates on a #GstControlPoint and
 * a #GstClockTime.
 */
static gint
gst_control_point_find (gconstpointer p1, gconstpointer p2, gpointer user_data)
{
  GstClockTime ct1 = ((GstControlPoint *) p1)->timestamp;
  GstClockTime ct2 = *(GstClockTime *) p2;

  return ((ct1 < ct2) ? -1 : ((ct1 == ct2) ? 0 : 1));
}

static GstControlPoint *
_make_new_cp (GstTimedValueControlSource * self, GstClockTime timestamp,
    const gdouble value)
{
  GstControlPoint *cp;

  /* create a new GstControlPoint */
  cp = g_new0 (GstControlPoint, 1);
  cp->timestamp = timestamp;
  cp->value = value;

  return cp;
}

static void
gst_timed_value_control_source_set_internal (GstTimedValueControlSource *
    self, GstClockTime timestamp, const gdouble value)
{
  GstControlPoint *cp;

  g_mutex_lock (&self->lock);

  /* check if a control point for the timestamp already exists */
  if (G_LIKELY (self->values)) {
    GSequenceIter *iter = g_sequence_lookup (self->values, &timestamp,
        (GCompareDataFunc) gst_control_point_find, NULL);

    if (iter) {
      GstControlPoint *cp = g_sequence_get (iter);

      /* update control point */
      cp->value = value;
      g_mutex_unlock (&self->lock);

      g_signal_emit (self,
          gst_timed_value_control_source_signals[VALUE_CHANGED_SIGNAL], 0, cp);
      goto done;
    }
  } else {
    self->values = g_sequence_new ((GDestroyNotify) gst_control_point_free);
    GST_INFO ("create new timed value sequence");
  }

  /* sort new cp into the prop->values list */
  cp = _make_new_cp (self, timestamp, value);
  g_sequence_insert_sorted (self->values, cp,
      (GCompareDataFunc) gst_control_point_compare, NULL);
  self->nvalues++;
  g_mutex_unlock (&self->lock);

  g_signal_emit (self,
      gst_timed_value_control_source_signals[VALUE_ADDED_SIGNAL], 0, cp);

done:
  self->valid_cache = FALSE;
}

/**
 * gst_timed_value_control_source_find_control_point_iter:
 * @self: the control source to search in
 * @timestamp: the search key
 *
 * Find last value before given timestamp in control point list.
 * If all values in the control point list come after the given
 * timestamp or no values exist, %NULL is returned.
 *
 * For use in control source implementations.
 *
 * Returns: (transfer none) (nullable): the found #GSequenceIter or %NULL
 */
GSequenceIter *gst_timed_value_control_source_find_control_point_iter
    (GstTimedValueControlSource * self, GstClockTime timestamp)
{
  GSequenceIter *iter;

  if (!self->values)
    return NULL;

  iter =
      g_sequence_search (self->values, &timestamp,
      (GCompareDataFunc) gst_control_point_find, NULL);

  /* g_sequence_search() returns the iter where timestamp
   * would be inserted, i.e. the iter > timestamp, so
   * we need to get the previous one. And of course, if
   * there is no previous one, we return NULL. */
  if (g_sequence_iter_is_begin (iter))
    return NULL;

  return g_sequence_iter_prev (iter);
}


/**
 * gst_timed_value_control_source_set:
 * @self: the #GstTimedValueControlSource object
 * @timestamp: the time the control-change is scheduled for
 * @value: the control-value
 *
 * Set the value of given controller-handled property at a certain time.
 *
 * Returns: FALSE if the values couldn't be set, TRUE otherwise.
 */
gboolean
gst_timed_value_control_source_set (GstTimedValueControlSource * self,
    GstClockTime timestamp, const gdouble value)
{
  g_return_val_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);

  gst_timed_value_control_source_set_internal (self, timestamp, value);

  return TRUE;
}

/**
 * gst_timed_value_control_source_set_from_list:
 * @self: the #GstTimedValueControlSource object
 * @timedvalues: (transfer none) (element-type GstTimedValue): a list
 * with #GstTimedValue items
 *
 * Sets multiple timed values at once.
 *
 * Returns: FALSE if the values couldn't be set, TRUE otherwise.
 */
gboolean
gst_timed_value_control_source_set_from_list (GstTimedValueControlSource *
    self, const GSList * timedvalues)
{
  const GSList *node;
  GstTimedValue *tv;
  gboolean res = FALSE;

  g_return_val_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self), FALSE);

  for (node = timedvalues; node; node = g_slist_next (node)) {
    tv = node->data;
    if (!GST_CLOCK_TIME_IS_VALID (tv->timestamp)) {
      GST_WARNING ("GstTimedValued with invalid timestamp passed to %s",
          GST_FUNCTION);
    } else {
      gst_timed_value_control_source_set_internal (self, tv->timestamp,
          tv->value);
      res = TRUE;
    }
  }
  return res;
}

/**
 * gst_timed_value_control_source_unset:
 * @self: the #GstTimedValueControlSource object
 * @timestamp: the time the control-change should be removed from
 *
 * Used to remove the value of given controller-handled property at a certain
 * time.
 *
 * Returns: FALSE if the value couldn't be unset (i.e. not found, TRUE otherwise.
 */
gboolean
gst_timed_value_control_source_unset (GstTimedValueControlSource * self,
    GstClockTime timestamp)
{
  GSequenceIter *iter;
  gboolean res = FALSE;
  GstControlPoint *cp = NULL;

  g_return_val_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (timestamp), FALSE);

  g_mutex_lock (&self->lock);
  /* check if a control point for the timestamp exists */
  if (G_LIKELY (self->values) && (iter =
          g_sequence_lookup (self->values, &timestamp,
              (GCompareDataFunc) gst_control_point_find, NULL))) {

    /* Iter contains the iter right after timestamp, i.e.
     * we need to get the previous one and check the timestamp
     */
    cp = g_memdup2 (g_sequence_get (iter), sizeof (GstControlPoint));
    g_sequence_remove (iter);
    self->nvalues--;
    self->valid_cache = FALSE;
    res = TRUE;
  }
  g_mutex_unlock (&self->lock);

  if (cp) {
    g_signal_emit (self,
        gst_timed_value_control_source_signals[VALUE_REMOVED_SIGNAL], 0, cp);
    g_free (cp);
  }

  return res;
}

/**
 * gst_timed_value_control_source_unset_all:
 * @self: the #GstTimedValueControlSource object
 *
 * Used to remove all time-stamped values of given controller-handled property
 *
 */
void
gst_timed_value_control_source_unset_all (GstTimedValueControlSource * self)
{
  g_return_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self));

  g_mutex_lock (&self->lock);
  /* free GstControlPoint structures */
  if (self->values) {
    g_sequence_free (self->values);
    self->values = NULL;
  }
  self->nvalues = 0;
  self->valid_cache = FALSE;

  g_mutex_unlock (&self->lock);
}

static void
_append_control_point (GstControlPoint * cp, GQueue * res)
{
  g_queue_push_tail (res, cp);
}

/**
 * gst_timed_value_control_source_get_all:
 * @self: the #GstTimedValueControlSource to get the list from
 *
 * Returns a read-only copy of the list of #GstTimedValue for the given property.
 * Free the list after done with it.
 *
 * Returns: (transfer container) (element-type GstTimedValue): a copy
 * of the list, or %NULL if the property isn't handled by the controller
 */
GList *
gst_timed_value_control_source_get_all (GstTimedValueControlSource * self)
{
  GQueue res = G_QUEUE_INIT;

  g_return_val_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self), NULL);

  g_mutex_lock (&self->lock);
  if (G_LIKELY (self->values))
    g_sequence_foreach (self->values, (GFunc) _append_control_point, &res);
  g_mutex_unlock (&self->lock);

  return res.head;
}

/**
 * gst_timed_value_control_source_get_count:
 * @self: the #GstTimedValueControlSource to get the number of values from
 *
 * Get the number of control points that are set.
 *
 * Returns: the number of control points that are set.
 */
gint
gst_timed_value_control_source_get_count (GstTimedValueControlSource * self)
{
  g_return_val_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self), 0);
  return self->nvalues;
}

/**
 * gst_timed_value_control_invalidate_cache:
 * @self: the #GstTimedValueControlSource
 *
 * Reset the controlled value cache.
 */
void
gst_timed_value_control_invalidate_cache (GstTimedValueControlSource * self)
{
  g_return_if_fail (GST_IS_TIMED_VALUE_CONTROL_SOURCE (self));
  self->valid_cache = FALSE;
}

static void
gst_timed_value_control_source_init (GstTimedValueControlSource * self)
{
  g_mutex_init (&self->lock);
}

static void
gst_timed_value_control_source_finalize (GObject * obj)
{
  GstTimedValueControlSource *self = GST_TIMED_VALUE_CONTROL_SOURCE (obj);

  g_mutex_lock (&self->lock);
  gst_timed_value_control_source_reset (self);
  g_mutex_unlock (&self->lock);
  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
gst_timed_value_control_source_class_init (GstTimedValueControlSourceClass
    * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  //GstControlSourceClass *csource_class = GST_CONTROL_SOURCE_CLASS (klass);

  /**
   * GstTimedValueControlSource::value-changed
   * @self: The #GstTimedValueControlSource on which a #GstTimedValue has changed
   * @timed_value: The #GstTimedValue where the value changed
   *
   * Emitted right after the new value has been set on @timed_signals
   *
   * Since: 1.6
   */
  gst_timed_value_control_source_signals[VALUE_CHANGED_SIGNAL] =
      g_signal_new ("value-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, gst_control_point_get_type ());

  /**
   * GstTimedValueControlSource::value-added
   * @self: The #GstTimedValueControlSource into which a #GstTimedValue has been
   *        added
   * @timed_value: The newly added #GstTimedValue
   *
   * Emitted right after the new value has been added to @self
   *
   * Since: 1.6
   */
  gst_timed_value_control_source_signals[VALUE_ADDED_SIGNAL] =
      g_signal_new ("value-added", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, gst_control_point_get_type ());

  /**
   * GstTimedValueControlSource::value-removed
   * @self: The #GstTimedValueControlSource from which a #GstTimedValue has been
   *        removed
   * @timed_value: The removed #GstTimedValue
   *
   * Emitted when @timed_value is removed from @self
   *
   * Since: 1.6
   */
  gst_timed_value_control_source_signals[VALUE_REMOVED_SIGNAL] =
      g_signal_new ("value-removed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, gst_control_point_get_type ());


  gobject_class->finalize = gst_timed_value_control_source_finalize;
}
