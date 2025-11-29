/*
 * gdkmonitorprivate.h
 *
 * Copyright 2016 Red Hat, Inc.
 *
 * Matthias Clasen <mclasen@redhat.com>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gdkmonitor.h"

G_BEGIN_DECLS

#define GDK_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MONITOR, GdkMonitorClass))
#define GDK_IS_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MONITOR))
#define GDK_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MONITOR, GdkMonitorClass))

struct _GdkMonitor {
  GObject parent;

  GdkDisplay *display;
  char *manufacturer;
  char *model;
  char *connector;
  char *description;
  GdkRectangle geometry;
  int width_mm;
  int height_mm;
  int scale_factor;
  int refresh_rate;
  GdkSubpixelLayout subpixel_layout;
  gboolean valid;
  double scale;
  gboolean scale_set;
};

struct _GdkMonitorClass {
  GObjectClass parent_class;
};

GdkMonitor *    gdk_monitor_new                 (GdkDisplay *display);

void            gdk_monitor_set_manufacturer    (GdkMonitor *monitor,
                                                 const char *manufacturer);
void            gdk_monitor_set_model           (GdkMonitor *monitor,
                                                 const char *model);
void            gdk_monitor_set_connector       (GdkMonitor *monitor,
                                                 const char *connector);
void            gdk_monitor_set_geometry        (GdkMonitor *monitor,
                                                 const GdkRectangle *geometry);
void            gdk_monitor_set_physical_size   (GdkMonitor *monitor,
                                                 int         width_mm,
                                                 int         height_mm);
void            gdk_monitor_set_scale_factor    (GdkMonitor *monitor,
                                                 int         scale_factor);
void            gdk_monitor_set_scale           (GdkMonitor *monitor,
                                                 double      scale);
void            gdk_monitor_set_refresh_rate    (GdkMonitor *monitor,
                                                 int         refresh_rate);
void            gdk_monitor_set_subpixel_layout (GdkMonitor        *monitor,
                                                 GdkSubpixelLayout  subpixel);
void            gdk_monitor_invalidate          (GdkMonitor *monitor);
void            gdk_monitor_set_description     (GdkMonitor *monitor,
                                                 const char *description);

G_END_DECLS

