/*
 * gst-cpu-throttling-clock.h
 *
 * Copyright (C) 2015 Thibault Saunier <tsaunier@gnome.org>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */



#ifndef __GST_CPU_THROTTLING_CLOCK_H__
#define __GST_CPU_THROTTLING_CLOCK_H__

#include <glib-object.h>
#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstCpuThrottlingClock GstCpuThrottlingClock;
typedef struct _GstCpuThrottlingClockClass GstCpuThrottlingClockClass;
typedef struct _GstCpuThrottlingClockPrivate GstCpuThrottlingClockPrivate;

GType gst_cpu_throttling_clock_get_type (void) G_GNUC_CONST;

#define GST_TYPE_CPU_THROTTLING_CLOCK (gst_cpu_throttling_clock_get_type ())
#define GST_CPU_THROTTLING_CLOCK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_CPU_THROTTLING_CLOCK, GstCpuThrottlingClock))
#define GST_CPU_THROTTLING_CLOCK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_CPU_THROTTLING_CLOCK, GstCpuThrottlingClockClass))
#define GST_IS_CPU_THROTTLING_CLOCK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_CPU_THROTTLING_CLOCK))
#define GST_IS_CPU_THROTTLING_CLOCK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_CPU_THROTTLING_CLOCK))
#define GST_CPU_THROTTLING_CLOCK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_CPU_THROTTLING_CLOCK, GstCpuThrottlingClockClass))

struct _GstCpuThrottlingClockClass
{
  /*<private>*/
  GstClockClass parent_class;
};

struct _GstCpuThrottlingClock
{
  /*<private>*/
  GstClock parent;
  GstCpuThrottlingClockPrivate *priv;
};

GstCpuThrottlingClock * gst_cpu_throttling_clock_new (guint cpu_usage);

G_END_DECLS

#endif /* #ifndef __GST_CPU_THROTTLING_CLOCK_H__*/
