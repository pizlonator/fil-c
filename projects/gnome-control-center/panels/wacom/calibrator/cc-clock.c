/*
 * Copyright © 2018 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Joaquim Rocha <jrocha@redhat.com>
 *          Carlos Garnacho <carlosg@gnome.org>
 */
#include "config.h"
#include "cc-clock.h"

#include <math.h>

#define CLOCK_RADIUS       50
#define CLOCK_LINE_WIDTH   10
#define CLOCK_LINE_PADDING 10
#define EXTRA_SPACE        2

typedef struct _CcClock CcClock;

struct _CcClock
{
  GtkWidget parent_instance;
  guint duration;
  gint64 start_time;
  gboolean running;
};

enum
{
  PROP_DURATION = 1,
  N_PROPS
};

static GParamSpec *props[N_PROPS] = { 0, };

enum {
  FINISHED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (CcClock, cc_clock, GTK_TYPE_WIDGET)

static gint64
cc_clock_get_time_diff (CcClock *clock)
{
  GdkFrameClock *frame_clock;
  gint64 current_time;

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (clock));
  current_time = gdk_frame_clock_get_frame_time (frame_clock);

  return current_time - clock->start_time;
}

static gdouble
cc_clock_get_angle (CcClock *clock)
{
  gint64 time_diff;

  time_diff = cc_clock_get_time_diff (clock);

  if (time_diff > clock->duration * 1000)
    return 360;

  return ((gdouble) time_diff / (clock->duration * 1000)) * 360;
}

static void
cc_clock_snapshot (GtkWidget   *widget,
                   GtkSnapshot *snapshot)
{
  GtkAllocation allocation;
  cairo_t *cr;
  gdouble angle;

  gtk_widget_get_allocation (widget, &allocation);
  angle = cc_clock_get_angle (CC_CLOCK (widget));

  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (0, 0, allocation.width, allocation.height));

  /* Draw the clock background */
  cairo_arc (cr, allocation.width / 2, allocation.height / 2, CLOCK_RADIUS / 2, 0.0, 2.0 * M_PI);
  cairo_set_source_rgb (cr, 0.5, 0.5, 0.5);
  cairo_fill_preserve (cr);
  cairo_stroke (cr);

  cairo_set_line_width (cr, CLOCK_LINE_WIDTH);

  cairo_arc (cr,
             allocation.width / 2,
             allocation.height / 2,
             (CLOCK_RADIUS - CLOCK_LINE_WIDTH - CLOCK_LINE_PADDING) / 2,
             3 * M_PI_2,
             3 * M_PI_2 + angle * M_PI / 180.0);
  cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
  cairo_stroke (cr);
}

static void
cc_clock_stop (CcClock *clock)
{
  GdkFrameClock *frame_clock;

  if (!clock->running)
    return;

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (clock));

  gdk_frame_clock_end_updating (frame_clock);
  clock->running = FALSE;
}

static void
on_frame_clock_update (CcClock *clock)
{
  gint64 time_diff;

  if (!clock->running)
    return;

  time_diff = cc_clock_get_time_diff (clock);

  if (time_diff > clock->duration * 1000)
    {
      g_signal_emit (clock, signals[FINISHED], 0);
      cc_clock_stop (clock);
    }

  gtk_widget_queue_draw (GTK_WIDGET (clock));
}

static void
cc_clock_map (GtkWidget *widget)
{
  GdkFrameClock *frame_clock;

  GTK_WIDGET_CLASS (cc_clock_parent_class)->map (widget);

  frame_clock = gtk_widget_get_frame_clock (widget);
  g_signal_connect_object (frame_clock, "update",
                           G_CALLBACK (on_frame_clock_update),
                           widget, G_CONNECT_SWAPPED);
  cc_clock_reset (CC_CLOCK (widget));
}

static void
cc_clock_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  CcClock *clock = CC_CLOCK (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      clock->duration = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_clock_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  CcClock *clock = CC_CLOCK (object);

  switch (prop_id)
    {
    case PROP_DURATION:
      g_value_set_uint (value, clock->duration);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_clock_measure (GtkWidget      *widget,
                  GtkOrientation  orientation,
                  gint            for_size,
                  gint           *minimum,
                  gint           *natural,
                  gint           *minimum_baseline,
                  gint           *natural_baseline)
{
  if (minimum)
    *minimum = CLOCK_RADIUS + EXTRA_SPACE;
  if (natural)
    *natural = CLOCK_RADIUS + EXTRA_SPACE;
}

static void
cc_clock_class_init (CcClockClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = cc_clock_set_property;
  object_class->get_property = cc_clock_get_property;

  widget_class->map = cc_clock_map;
  widget_class->snapshot = cc_clock_snapshot;
  widget_class->measure = cc_clock_measure;

  signals[FINISHED] =
    g_signal_new ("finished",
                  CC_TYPE_CLOCK,
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  props[PROP_DURATION] =
    g_param_spec_uint ("duration",
                       "Duration",
                       "Duration",
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_clock_init (CcClock *clock)
{
}

GtkWidget *
cc_clock_new (guint duration)
{
  return g_object_new (CC_TYPE_CLOCK,
                       "duration", duration,
                       NULL);
}

void
cc_clock_reset (CcClock *clock)
{
  GdkFrameClock *frame_clock;

  if (!gtk_widget_get_mapped (GTK_WIDGET (clock)))
    return;

  frame_clock = gtk_widget_get_frame_clock (GTK_WIDGET (clock));

  cc_clock_stop (clock);

  clock->running = TRUE;
  clock->start_time = g_get_monotonic_time ();
  gdk_frame_clock_begin_updating (frame_clock);
}

void
cc_clock_set_duration (CcClock *clock,
                       guint    duration)
{
  clock->duration = duration;
  g_object_notify (G_OBJECT (clock), "duration");
  cc_clock_reset (clock);
}

guint
cc_clock_get_duration (CcClock *clock)
{
  return clock->duration;
}
