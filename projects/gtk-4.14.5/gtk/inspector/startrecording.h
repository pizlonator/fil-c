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

#include <gdk/gdk.h>
#include <gsk/gsk.h>

#include "inspector/recording.h"

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_START_RECORDING            (gtk_inspector_start_recording_get_type())
#define GTK_INSPECTOR_START_RECORDING(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_START_RECORDING, GtkInspectorStartRecording))
#define GTK_INSPECTOR_START_RECORDING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_START_RECORDING, GtkInspectorStartRecordingClass))
#define GTK_INSPECTOR_IS_START_RECORDING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_START_RECORDING))
#define GTK_INSPECTOR_IS_START_RECORDING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_START_RECORDING))
#define GTK_INSPECTOR_START_RECORDING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_START_RECORDING, GtkInspectorStartRecordingClass))


typedef struct _GtkInspectorStartRecordingPrivate GtkInspectorStartRecordingPrivate;

typedef struct _GtkInspectorStartRecording
{
  GtkInspectorRecording parent;

} GtkInspectorStartRecording;

typedef struct _GtkInspectorStartRecordingClass
{
  GtkInspectorRecordingClass parent;
} GtkInspectorStartRecordingClass;

GType           gtk_inspector_start_recording_get_type          (void);

GtkInspectorRecording *
                gtk_inspector_start_recording_new               (void);


G_END_DECLS


// vim: set et sw=2 ts=2:
