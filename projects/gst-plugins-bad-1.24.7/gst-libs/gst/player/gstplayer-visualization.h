/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
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

#ifndef __GST_PLAYER_VISUALIZATION_H__
#define __GST_PLAYER_VISUALIZATION_H__

#include <gst/gst.h>
#include <gst/player/player-prelude.h>

G_BEGIN_DECLS

typedef struct _GstPlayerVisualization GstPlayerVisualization;
/**
 * GstPlayerVisualization:
 * @name: name of the visualization.
 * @description: description of the visualization.
 *
 * A #GstPlayerVisualization descriptor.
 */
struct _GstPlayerVisualization {
  gchar *name;
  gchar *description;
};

GST_PLAYER_API
GType                     gst_player_visualization_get_type (void);

GST_PLAYER_API
GstPlayerVisualization *  gst_player_visualization_copy  (const GstPlayerVisualization *vis);

GST_PLAYER_API
void                      gst_player_visualization_free  (GstPlayerVisualization *vis);

GST_PLAYER_API
GstPlayerVisualization ** gst_player_visualizations_get  (void);

GST_PLAYER_API
void                      gst_player_visualizations_free (GstPlayerVisualization **viss);

G_END_DECLS

#endif /* __GST_PLAYER_VISUALIZATION_H__ */
