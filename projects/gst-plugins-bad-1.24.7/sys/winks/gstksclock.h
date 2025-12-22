/*
 * Copyright (C) 2008 Ole André Vadla Ravnås <ole.andre.ravnas@tandberg.com>
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

#ifndef __GST_KS_CLOCK_H__
#define __GST_KS_CLOCK_H__

#include <gst/gst.h>
#include <windows.h>

#include "ksvideohelpers.h"

G_BEGIN_DECLS

#define GST_TYPE_KS_CLOCK \
  (gst_ks_clock_get_type ())
#define GST_KS_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_KS_CLOCK, GstKsClock))
#define GST_KS_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_KS_CLOCK, GstKsClockClass))
#define GST_IS_KS_CLOCK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_KS_CLOCK))
#define GST_IS_KS_CLOCK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_KS_CLOCK))

typedef struct _GstKsClock        GstKsClock;
typedef struct _GstKsClockClass   GstKsClockClass;
typedef struct _GstKsClockPrivate GstKsClockPrivate;

struct _GstKsClock
{
  GObject parent;

  GstKsClockPrivate *priv;
};

struct _GstKsClockClass
{
  GObjectClass parent_class;
};

GType gst_ks_clock_get_type (void);

gboolean gst_ks_clock_open (GstKsClock * self);
void gst_ks_clock_close (GstKsClock * self);

HANDLE gst_ks_clock_get_handle (GstKsClock * self);

void gst_ks_clock_prepare (GstKsClock * self);
void gst_ks_clock_start (GstKsClock * self);

void gst_ks_clock_provide_master_clock (GstKsClock * self,
    GstClock * master_clock);

G_END_DECLS

#endif /* __GST_KS_CLOCK_H__ */
