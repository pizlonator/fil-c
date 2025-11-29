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
#include "gsk/gskprofilerprivate.h"

#include "inspector/recording.h"

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_RENDER_RECORDING            (gtk_inspector_render_recording_get_type())
#define GTK_INSPECTOR_RENDER_RECORDING(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INSPECTOR_RENDER_RECORDING, GtkInspectorRenderRecording))
#define GTK_INSPECTOR_RENDER_RECORDING_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INSPECTOR_RENDER_RECORDING, GtkInspectorRenderRecordingClass))
#define GTK_INSPECTOR_IS_RENDER_RECORDING(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INSPECTOR_RENDER_RECORDING))
#define GTK_INSPECTOR_IS_RENDER_RECORDING_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INSPECTOR_RENDER_RECORDING))
#define GTK_INSPECTOR_RENDER_RECORDING_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INSPECTOR_RENDER_RECORDING, GtkInspectorRenderRecordingClass))


typedef struct _GtkInspectorRenderRecordingPrivate GtkInspectorRenderRecordingPrivate;

typedef struct _GtkInspectorRenderRecording
{
  GtkInspectorRecording parent;

  GdkRectangle area;
  cairo_region_t *clip_region;
  GskRenderNode *node;
  char *profiler_info;
} GtkInspectorRenderRecording;

typedef struct _GtkInspectorRenderRecordingClass
{
  GtkInspectorRecordingClass parent;
} GtkInspectorRenderRecordingClass;

GType           gtk_inspector_render_recording_get_type      (void);

GtkInspectorRecording *
                gtk_inspector_render_recording_new           (gint64                             timestamp,
                                                              GskProfiler                       *profiler,
                                                              const GdkRectangle                *area,
                                                              const cairo_region_t              *clip_region,
                                                              GskRenderNode                     *node);

GskRenderNode * gtk_inspector_render_recording_get_node      (GtkInspectorRenderRecording       *recording);
const cairo_region_t *
                gtk_inspector_render_recording_get_clip_region (GtkInspectorRenderRecording     *recording);
const cairo_rectangle_int_t *
                gtk_inspector_render_recording_get_area      (GtkInspectorRenderRecording       *recording);
const char *    gtk_inspector_render_recording_get_profiler_info
                                                             (GtkInspectorRenderRecording       *recording);


G_END_DECLS


// vim: set et sw=2 ts=2:
