/*
 * Copyright (c) 2014 Red Hat, Inc.
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

#pragma once

#include <gtk/gtkbox.h>

#define GTK_TYPE_INSPECTOR_STATISTICS            (gtk_inspector_statistics_get_type())
#define GTK_INSPECTOR_STATISTICS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_STATISTICS, GtkInspectorStatistics))
#define GTK_INSPECTOR_STATISTICS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_STATISTICS, GtkInspectorStatisticsClass))
#define GTK_INSPECTOR_IS_STATISTICS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_STATISTICS))
#define GTK_INSPECTOR_IS_STATISTICS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_STATISTICS))
#define GTK_INSPECTOR_STATISTICS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_STATISTICS, GtkInspectorStatisticsClass))


typedef struct _GtkInspectorStatisticsPrivate GtkInspectorStatisticsPrivate;

typedef struct _GtkInspectorStatistics
{
  GtkBox parent;
  GtkInspectorStatisticsPrivate *priv;
} GtkInspectorStatistics;

typedef struct _GtkInspectorStatisticsClass
{
  GtkBoxClass parent;
} GtkInspectorStatisticsClass;

G_BEGIN_DECLS

GType      gtk_inspector_statistics_get_type   (void);

G_END_DECLS


// vim: set et sw=2 ts=2:
