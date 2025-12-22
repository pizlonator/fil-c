/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#pragma once

#include <gst/gst.h>
#include <gst/video/video.h>
#include "gstd3d12.h"
#include "gstd3d12pluginutils.h"

G_BEGIN_DECLS

#define GST_D3D12_WINDOW_FLOW_CLOSED GST_FLOW_CUSTOM_ERROR

#define GST_TYPE_D3D12_WINDOW (gst_d3d12_window_get_type())
G_DECLARE_FINAL_TYPE (GstD3D12Window, gst_d3d12_window,
    GST, D3D12_WINDOW, GstObject)

enum GstD3D12WindowState
{
  GST_D3D12_WINDOW_STATE_INIT,
  GST_D3D12_WINDOW_STATE_OPENED,
  GST_D3D12_WINDOW_STATE_CLOSED,
};

GstD3D12Window * gst_d3d12_window_new (void);

guintptr         gst_d3d12_window_get_window_handle (GstD3D12Window * window);

GstD3D12WindowState gst_d3d12_window_get_state (GstD3D12Window * window);

GstFlowReturn    gst_d3d12_window_prepare (GstD3D12Window * window,
                                           GstD3D12Device * device,
                                           guintptr window_handle,
                                           guint display_width,
                                           guint display_height,
                                           GstCaps * caps,
                                           GstStructure * config);

void             gst_d3d12_window_unprepare (GstD3D12Window * window);

void             gst_d3d12_window_unlock    (GstD3D12Window * window);

void             gst_d3d12_window_unlock_stop (GstD3D12Window * window);

GstFlowReturn    gst_d3d12_window_set_buffer (GstD3D12Window * window,
                                              GstBuffer * buffer);

GstFlowReturn    gst_d3d12_window_present (GstD3D12Window * window);

void             gst_d3d12_window_set_render_rect (GstD3D12Window * window,
                                                   const GstVideoRectangle * rect);

void             gst_d3d12_window_set_force_aspect_ratio (GstD3D12Window * window,
                                                          gboolean force_aspect_ratio);

void             gst_d3d12_window_set_enable_navigation_events (GstD3D12Window * window,
                                                                gboolean enable);

void             gst_d3d12_window_set_orientation (GstD3D12Window * window,
                                                   gboolean immediate,
                                                   GstVideoOrientationMethod orientation,
                                                   gfloat fov,
                                                   gboolean ortho,
                                                   gfloat rotation_x,
                                                   gfloat rotation_y,
                                                   gfloat rotation_z,
                                                   gfloat scale_x,
                                                   gfloat scale_y);

void             gst_d3d12_window_set_title (GstD3D12Window * window,
                                             const gchar * title);

void             gst_d3d12_window_enable_fullscreen_on_alt_enter (GstD3D12Window * window,
                                                                  gboolean enable);

void             gst_d3d12_window_set_fullscreen (GstD3D12Window * window,
                                                  gboolean enable);

void             gst_d3d12_window_set_msaa (GstD3D12Window * window,
                                            GstD3D12MSAAMode msaa);

G_END_DECLS

