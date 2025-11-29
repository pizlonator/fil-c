/*
 * Copyright (c) 2016 Red Hat, Inc.
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

#include <glib-object.h>

#define GTK_TYPE_INSPECTOR_RECORDING            (gtk_inspector_recording_get_type())
#define GTK_INSPECTOR_RECORDING(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_RECORDING, GtkInspectorRecording))
#define GTK_INSPECTOR_RECORDING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_RECORDING, GtkInspectorRecordingClass))
#define GTK_INSPECTOR_IS_RECORDING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_RECORDING))
#define GTK_INSPECTOR_IS_RECORDING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_RECORDING))
#define GTK_INSPECTOR_RECORDING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_RECORDING, GtkInspectorRecordingClass))


typedef struct _GtkInspectorRecordingPrivate GtkInspectorRecordingPrivate;

typedef struct _GtkInspectorRecording
{
  GObject parent;

  gint64 timestamp;
} GtkInspectorRecording;

typedef struct _GtkInspectorRecordingClass
{
  GObjectClass parent;
} GtkInspectorRecordingClass;

G_BEGIN_DECLS

GType           gtk_inspector_recording_get_type                (void);

gint64          gtk_inspector_recording_get_timestamp           (GtkInspectorRecording  *recording);

G_END_DECLS


// vim: set et sw=2 ts=2:
