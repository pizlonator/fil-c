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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "eventrecording.h"

G_DEFINE_TYPE (GtkInspectorEventRecording, gtk_inspector_event_recording, GTK_TYPE_INSPECTOR_RECORDING)

static void
gtk_inspector_event_recording_finalize (GObject *object)
{
  GtkInspectorEventRecording *recording = GTK_INSPECTOR_EVENT_RECORDING (object);

  g_clear_pointer (&recording->event, gdk_event_unref);

  G_OBJECT_CLASS (gtk_inspector_event_recording_parent_class)->finalize (object);
}

static void
gtk_inspector_event_recording_class_init (GtkInspectorEventRecordingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_inspector_event_recording_finalize;
}

static void
gtk_inspector_event_recording_init (GtkInspectorEventRecording *vis)
{
}

GtkInspectorRecording *
gtk_inspector_event_recording_new (gint64    timestamp,
                                   GdkEvent *event)
{
  GtkInspectorEventRecording *recording;

  recording = g_object_new (GTK_TYPE_INSPECTOR_EVENT_RECORDING,
                            "timestamp", timestamp,
                            NULL);

  recording->event = gdk_event_ref (event);

  return GTK_INSPECTOR_RECORDING (recording);
}

GdkEvent *
gtk_inspector_event_recording_get_event (GtkInspectorEventRecording *recording)
{
  return recording->event;
}

// vim: set et sw=2 ts=2:
