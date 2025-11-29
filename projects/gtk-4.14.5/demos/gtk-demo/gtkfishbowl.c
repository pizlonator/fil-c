/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Benjamin Otte <otte@gnome.org>
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

#include "config.h"

#include "gtkfishbowl.h"

typedef struct _GtkFishbowlPrivate       GtkFishbowlPrivate;
typedef struct _GtkFishbowlChild         GtkFishbowlChild;

struct _GtkFishbowlPrivate
{
  GtkFishCreationFunc creation_func;
  GHashTable *children;
  guint count;

  gint64 last_frame_time;
  gint64 update_delay;
  guint tick_id;

  double framerate;
  int last_benchmark_change;

  guint benchmark : 1;
};

struct _GtkFishbowlChild
{
  GtkWidget *widget;
  double x;
  double y;
  double dx;
  double dy;
};

enum {
   PROP_0,
   PROP_ANIMATING,
   PROP_BENCHMARK,
   PROP_COUNT,
   PROP_FRAMERATE,
   PROP_UPDATE_DELAY,
   NUM_PROPERTIES
};

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GtkFishbowl, gtk_fishbowl, GTK_TYPE_WIDGET)

static void
gtk_fishbowl_init (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  priv->update_delay = G_USEC_PER_SEC;
  priv->children = g_hash_table_new_full (NULL, NULL,
                                          NULL, (GDestroyNotify) g_free);
}

/**
 * gtk_fishbowl_new:
 *
 * Creates a new `GtkFishbowl`.
 *
 * Returns: a new `GtkFishbowl`.
 */
GtkWidget*
gtk_fishbowl_new (void)
{
  return g_object_new (GTK_TYPE_FISHBOWL, NULL);
}

static void
gtk_fishbowl_measure (GtkWidget      *widget,
                      GtkOrientation  orientation,
                      int             for_size,
                      int            *minimum,
                      int            *natural,
                      int            *minimum_baseline,
                      int            *natural_baseline)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GHashTableIter iter;
  gpointer key, value;
  GtkFishbowlChild *child;
  int child_min, child_nat;

  *minimum = 0;
  *natural = 0;

  g_hash_table_iter_init (&iter, priv->children);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      child = value;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_measure (child->widget, orientation, -1, &child_min, &child_nat, NULL, NULL);
        }
      else
        {
          int min_width;

          gtk_widget_measure (child->widget, GTK_ORIENTATION_HORIZONTAL, -1, &min_width, NULL, NULL, NULL);
          gtk_widget_measure (child->widget, orientation, min_width, &child_min, &child_nat, NULL, NULL);
        }

      *minimum = MAX (*minimum, child_min);
      *natural = MAX (*natural, child_nat);
    }
}

static void
gtk_fishbowl_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  GtkAllocation child_allocation;
  GtkRequisition child_requisition;
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, priv->children);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      child = value;

      gtk_widget_get_preferred_size (child->widget, &child_requisition, NULL);
      child_allocation.x = round (child->x * (width - child_requisition.width));
      child_allocation.y = round (child->y * (height - child_requisition.height));
      child_allocation.width = child_requisition.width;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate (child->widget, &child_allocation, -1);
    }
}

static double
new_speed (void)
{
  /* 5s to 50s to cross screen seems fair */
  return g_random_double_range (0.02, 0.2);
}

static void
gtk_fishbowl_add (GtkFishbowl *fishbowl,
                  GtkWidget   *widget)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child_info;

  g_return_if_fail (GTK_IS_FISHBOWL (fishbowl));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  child_info = g_new0 (GtkFishbowlChild, 1);
  child_info->widget = widget;
  child_info->x = 0;
  child_info->y = 0;
  child_info->dx = new_speed ();
  child_info->dy = new_speed ();

  gtk_widget_set_parent (widget, GTK_WIDGET (fishbowl));
  gtk_accessible_update_state (GTK_ACCESSIBLE (widget),
                               GTK_ACCESSIBLE_STATE_HIDDEN, TRUE,
                               -1);

  g_hash_table_insert (priv->children, widget, child_info);
  priv->count++;
  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_COUNT]);
}

static void
gtk_fishbowl_remove (GtkFishbowl *fishbowl,
                     GtkWidget   *widget)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (g_hash_table_remove (priv->children, widget))
    {
      gtk_widget_unparent (widget);

      priv->count--;
      g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_COUNT]);
    }
}

static void
gtk_fishbowl_finalize (GObject *object)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  g_hash_table_destroy (priv->children);
  priv->children = NULL;
}

static void
gtk_fishbowl_dispose (GObject *object)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  gtk_fishbowl_set_animating (fishbowl, FALSE);
  gtk_fishbowl_set_count (fishbowl, 0);

  G_OBJECT_CLASS (gtk_fishbowl_parent_class)->dispose (object);
}

static void
gtk_fishbowl_set_property (GObject         *object,
                           guint            prop_id,
                           const GValue    *value,
                           GParamSpec      *pspec)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  switch (prop_id)
    {
    case PROP_ANIMATING:
      gtk_fishbowl_set_animating (fishbowl, g_value_get_boolean (value));
      break;

    case PROP_BENCHMARK:
      gtk_fishbowl_set_benchmark (fishbowl, g_value_get_boolean (value));
      break;

    case PROP_COUNT:
      gtk_fishbowl_set_count (fishbowl, g_value_get_uint (value));
      break;

    case PROP_UPDATE_DELAY:
      gtk_fishbowl_set_update_delay (fishbowl, g_value_get_int64 (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fishbowl_get_property (GObject         *object,
                           guint            prop_id,
                           GValue          *value,
                           GParamSpec      *pspec)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (object);

  switch (prop_id)
    {
    case PROP_ANIMATING:
      g_value_set_boolean (value, gtk_fishbowl_get_animating (fishbowl));
      break;

    case PROP_BENCHMARK:
      g_value_set_boolean (value, gtk_fishbowl_get_benchmark (fishbowl));
      break;

    case PROP_COUNT:
      g_value_set_uint (value, gtk_fishbowl_get_count (fishbowl));
      break;

    case PROP_FRAMERATE:
      g_value_set_double (value, gtk_fishbowl_get_framerate (fishbowl));
      break;

    case PROP_UPDATE_DELAY:
      g_value_set_int64 (value, gtk_fishbowl_get_update_delay (fishbowl));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_fishbowl_class_init (GtkFishbowlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_fishbowl_finalize;
  object_class->dispose = gtk_fishbowl_dispose;
  object_class->set_property = gtk_fishbowl_set_property;
  object_class->get_property = gtk_fishbowl_get_property;

  widget_class->measure = gtk_fishbowl_measure;
  widget_class->size_allocate = gtk_fishbowl_size_allocate;

  props[PROP_ANIMATING] =
      g_param_spec_boolean ("animating",
                            "animating",
                            "Whether children are moving around",
                            FALSE,
                            G_PARAM_READWRITE);

  props[PROP_BENCHMARK] =
      g_param_spec_boolean ("benchmark",
                            "Benchmark",
                            "Adapt the count property to hit the maximum framerate",
                            FALSE,
                            G_PARAM_READWRITE);

  props[PROP_COUNT] =
      g_param_spec_uint ("count",
                         "Count",
                         "Number of widgets",
                         0, G_MAXUINT,
                         0,
                         G_PARAM_READWRITE);

  props[PROP_FRAMERATE] =
      g_param_spec_double ("framerate",
                           "Framerate",
                           "Framerate of this widget in frames per second",
                           0, G_MAXDOUBLE,
                           0,
                           G_PARAM_READABLE);

  props[PROP_UPDATE_DELAY] =
      g_param_spec_int64 ("update-delay",
                          "Update delay",
                          "Number of usecs between updates",
                          0, G_MAXINT64,
                          G_USEC_PER_SEC,
                          G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, props);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_PRESENTATION);
}

guint
gtk_fishbowl_get_count (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->count;
}

void
gtk_fishbowl_set_count (GtkFishbowl *fishbowl,
                        guint        count)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (priv->count == count)
    return;

  g_object_freeze_notify (G_OBJECT (fishbowl));

  while (priv->count > count)
    {
      gtk_fishbowl_remove (fishbowl, gtk_widget_get_first_child (GTK_WIDGET (fishbowl)));
    }

  while (priv->count < count)
    {
      GtkWidget *new_widget;

      new_widget = priv->creation_func ();

      gtk_fishbowl_add (fishbowl, new_widget);
    }

  g_object_thaw_notify (G_OBJECT (fishbowl));
}

gboolean
gtk_fishbowl_get_benchmark (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->benchmark;
}

void
gtk_fishbowl_set_benchmark (GtkFishbowl *fishbowl,
                            gboolean     benchmark)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (priv->benchmark == benchmark)
    return;

  priv->benchmark = benchmark;
  if (!benchmark)
    priv->last_benchmark_change = 0;

  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_BENCHMARK]);
}

gboolean
gtk_fishbowl_get_animating (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->tick_id != 0;
}

static gint64
guess_refresh_interval (GdkFrameClock *frame_clock)
{
  gint64 interval;
  gint64 i;

  interval = G_MAXINT64;

  for (i = gdk_frame_clock_get_history_start (frame_clock);
       i < gdk_frame_clock_get_frame_counter (frame_clock);
       i++)
    {
      GdkFrameTimings *t, *before;
      gint64 ts, before_ts;

      t = gdk_frame_clock_get_timings (frame_clock, i);
      before = gdk_frame_clock_get_timings (frame_clock, i - 1);
      if (t == NULL || before == NULL)
        continue;

      ts = gdk_frame_timings_get_frame_time (t);
      before_ts = gdk_frame_timings_get_frame_time (before);
      if (ts == 0 || before_ts == 0)
        continue;

      interval = MIN (interval, ts - before_ts);
    }

  if (interval == G_MAXINT64)
    return 0;

  return interval;
}

static void
gtk_fishbowl_do_update (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GdkFrameClock *frame_clock;
  GdkFrameTimings *end;
  gint64 end_counter;
  double fps, expected_fps;
  gint64 interval;

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (fishbowl));
  if (frame_clock == NULL)
    return;

  fps = gdk_frame_clock_get_fps (frame_clock);
  if (fps <= 0.0)
    return;

  priv->framerate = fps;
  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_FRAMERATE]);

  if (!priv->benchmark)
    return;

  end_counter = gdk_frame_clock_get_frame_counter (frame_clock);
  for (end = gdk_frame_clock_get_timings (frame_clock, end_counter);
       end != NULL && !gdk_frame_timings_get_complete (end);
       end = gdk_frame_clock_get_timings (frame_clock, end_counter))
    end_counter--;
  if (end == NULL)
    return;

  interval = gdk_frame_timings_get_refresh_interval (end);
  if (interval == 0)
    {
      interval = guess_refresh_interval (frame_clock);
      if (interval == 0)
        return;
    }
  expected_fps = (double) G_USEC_PER_SEC / interval;

  if (fps > (expected_fps - 1))
    {
      if (priv->last_benchmark_change > 0)
        priv->last_benchmark_change *= 2;
      else
        priv->last_benchmark_change = 1;
    }
  else if (0.95 * fps < expected_fps)
    {
      if (priv->last_benchmark_change < 0)
        priv->last_benchmark_change--;
      else
        priv->last_benchmark_change = -1;
    }
  else
    {
      priv->last_benchmark_change = 0;
    }

  gtk_fishbowl_set_count (fishbowl, MAX (1, (int) priv->count + priv->last_benchmark_change));
}

static gboolean
gtk_fishbowl_tick (GtkWidget     *widget,
                   GdkFrameClock *frame_clock,
                   gpointer       unused)
{
  GtkFishbowl *fishbowl = GTK_FISHBOWL (widget);
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);
  GtkFishbowlChild *child;
  gint64 frame_time, elapsed;
  GHashTableIter iter;
  gpointer key, value;
  gboolean do_update;

  frame_time = gdk_frame_clock_get_frame_time (gtk_widget_get_frame_clock (widget));
  elapsed = frame_time - priv->last_frame_time;
  do_update = frame_time / priv->update_delay != priv->last_frame_time / priv->update_delay;
  priv->last_frame_time = frame_time;

  /* last frame was 0, so we're just starting to animate */
  if (elapsed == frame_time)
    return G_SOURCE_CONTINUE;

  g_hash_table_iter_init (&iter, priv->children);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      child = value;

      child->x += child->dx * ((double) elapsed / G_USEC_PER_SEC);
      child->y += child->dy * ((double) elapsed / G_USEC_PER_SEC);

      if (child->x <= 0)
        {
          child->x = 0;
          child->dx = new_speed ();
        }
      else if (child->x >= 1)
        {
          child->x = 1;
          child->dx =  - new_speed ();
        }

      if (child->y <= 0)
        {
          child->y = 0;
          child->dy = new_speed ();
        }
      else if (child->y >= 1)
        {
          child->y = 1;
          child->dy =  - new_speed ();
        }
    }

  gtk_widget_queue_allocate (widget);

  if (do_update)
    gtk_fishbowl_do_update (fishbowl);

  return G_SOURCE_CONTINUE;
}

void
gtk_fishbowl_set_animating (GtkFishbowl *fishbowl,
                            gboolean     animating)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (gtk_fishbowl_get_animating (fishbowl) == animating)
    return;

  if (animating)
    {
      priv->tick_id = gtk_widget_add_tick_callback (GTK_WIDGET (fishbowl),
                                                    gtk_fishbowl_tick,
                                                    NULL,
                                                    NULL);
    }
  else
    {
      priv->last_frame_time = 0;
      gtk_widget_remove_tick_callback (GTK_WIDGET (fishbowl), priv->tick_id);
      priv->tick_id = 0;
      priv->framerate = 0;
      g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_FRAMERATE]);
    }

  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_ANIMATING]);
}

double
gtk_fishbowl_get_framerate (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->framerate;
}

gint64
gtk_fishbowl_get_update_delay (GtkFishbowl *fishbowl)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  return priv->update_delay;
}

void
gtk_fishbowl_set_update_delay (GtkFishbowl *fishbowl,
                               gint64       update_delay)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  if (priv->update_delay == update_delay)
    return;

  priv->update_delay = update_delay;

  g_object_notify_by_pspec (G_OBJECT (fishbowl), props[PROP_UPDATE_DELAY]);
}

void
gtk_fishbowl_set_creation_func (GtkFishbowl         *fishbowl,
                                GtkFishCreationFunc  creation_func)
{
  GtkFishbowlPrivate *priv = gtk_fishbowl_get_instance_private (fishbowl);

  g_object_freeze_notify (G_OBJECT (fishbowl));

  gtk_fishbowl_set_count (fishbowl, 0);
  priv->last_benchmark_change = 0;

  priv->creation_func = creation_func;

  gtk_fishbowl_set_count (fishbowl, 1);

  g_object_thaw_notify (G_OBJECT (fishbowl));
}
