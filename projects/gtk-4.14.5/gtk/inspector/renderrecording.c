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

#include "renderrecording.h"

G_DEFINE_TYPE (GtkInspectorRenderRecording, gtk_inspector_render_recording, GTK_TYPE_INSPECTOR_RECORDING)

static void
gtk_inspector_render_recording_finalize (GObject *object)
{
  GtkInspectorRenderRecording *recording = GTK_INSPECTOR_RENDER_RECORDING (object);

  g_clear_pointer (&recording->clip_region, cairo_region_destroy);
  g_clear_pointer (&recording->node, gsk_render_node_unref);
  g_clear_pointer (&recording->profiler_info, g_free);

  G_OBJECT_CLASS (gtk_inspector_render_recording_parent_class)->finalize (object);
}

static void
gtk_inspector_render_recording_class_init (GtkInspectorRenderRecordingClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_inspector_render_recording_finalize;
}

static void
gtk_inspector_render_recording_init (GtkInspectorRenderRecording *vis)
{
}

static void
collect_profiler_info (GtkInspectorRenderRecording *recording,
                       GskProfiler                 *profiler)
{
  GString *string;

  string = g_string_new (NULL);
  gsk_profiler_append_timers (profiler, string);
  gsk_profiler_append_counters (profiler, string);
  recording->profiler_info = g_string_free (string, FALSE);
}

GtkInspectorRecording *
gtk_inspector_render_recording_new (gint64                timestamp,
                                    GskProfiler          *profiler,
                                    const GdkRectangle   *area,
                                    const cairo_region_t *clip_region,
                                    GskRenderNode        *node)
{
  GtkInspectorRenderRecording *recording;

  recording = g_object_new (GTK_TYPE_INSPECTOR_RENDER_RECORDING,
                            "timestamp", timestamp,
                            NULL);

  collect_profiler_info (recording, profiler);
  recording->area = *area;
  recording->clip_region = cairo_region_copy (clip_region);
  recording->node = gsk_render_node_ref (node);

  return GTK_INSPECTOR_RECORDING (recording);
}

GskRenderNode *
gtk_inspector_render_recording_get_node (GtkInspectorRenderRecording *recording)
{
  return recording->node;
}

const cairo_region_t *
gtk_inspector_render_recording_get_clip_region (GtkInspectorRenderRecording *recording)
{
  return recording->clip_region;
}

const cairo_rectangle_int_t *
gtk_inspector_render_recording_get_area (GtkInspectorRenderRecording *recording)
{
  return &recording->area;
}

const char *
gtk_inspector_render_recording_get_profiler_info (GtkInspectorRenderRecording *recording)
{
  return recording->profiler_info;
}

// vim: set et sw=2 ts=2:
