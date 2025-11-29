/*
 * Copyright (c) 2021 Red Hat, Inc.
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
#include "gsk/gskprofilerprivate.h"

#include "inspector/recording.h"

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_EVENT_RECORDING            (gtk_inspector_event_recording_get_type())
#define GTK_INSPECTOR_EVENT_RECORDING(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_EVENT_RECORDING, GtkInspectorEventRecording))
#define GTK_INSPECTOR_EVENT_RECORDING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_EVENT_RECORDING, GtkInspectorEventRecordingClass))
#define GTK_INSPECTOR_IS_EVENT_RECORDING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_EVENT_RECORDING))
#define GTK_INSPECTOR_IS_EVENT_RECORDING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_EVENT_RECORDING))
#define GTK_INSPECTOR_EVENT_RECORDING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_EVENT_RECORDING, GtkInspectorEventRecordingClass))


typedef struct _GtkInspectorEventRecordingPrivate GtkInspectorEventRecordingPrivate;

typedef struct _GtkInspectorEventRecording
{
  GtkInspectorRecording parent;

  GdkEvent *event;
} GtkInspectorEventRecording;

typedef struct _GtkInspectorEventRecordingClass
{
  GtkInspectorRecordingClass parent;
} GtkInspectorEventRecordingClass;

GType           gtk_inspector_event_recording_get_type      (void);

GtkInspectorRecording *
                gtk_inspector_event_recording_new            (gint64    timestamp,
                                                              GdkEvent *event);

GdkEvent *      gtk_inspector_event_recording_get_event      (GtkInspectorEventRecording       *recording);


G_END_DECLS


// vim: set et sw=2 ts=2:
