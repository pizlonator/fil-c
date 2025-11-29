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

#include "config.h"
#include <glib/gi18n-lib.h>

#include "recording.h"

enum
{
  PROP_0,
  PROP_TIMESTAMP,
  LAST_PROP
};

static GParamSpec *props[LAST_PROP] = { NULL, };


G_DEFINE_TYPE (GtkInspectorRecording, gtk_inspector_recording, G_TYPE_OBJECT)

static void
gtk_inspector_recording_get_property (GObject    *object,
                                      guint       param_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (object);

  switch (param_id)
    {
    case PROP_TIMESTAMP:
      g_value_set_int64 (value, recording->timestamp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recording_set_property (GObject      *object,
                                      guint         param_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkInspectorRecording *recording = GTK_INSPECTOR_RECORDING (object);

  switch (param_id)
    {
    case PROP_TIMESTAMP:
      recording->timestamp = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_recording_class_init (GtkInspectorRecordingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gtk_inspector_recording_get_property;
  object_class->set_property = gtk_inspector_recording_set_property;

  props[PROP_TIMESTAMP] =
    g_param_spec_int64 ("timestamp", NULL, NULL,
                        G_MININT64, G_MAXINT64, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
gtk_inspector_recording_init (GtkInspectorRecording *vis)
{
}

gint64
gtk_inspector_recording_get_timestamp (GtkInspectorRecording *recording)
{
  return recording->timestamp;
}

// vim: set et sw=2 ts=2:
