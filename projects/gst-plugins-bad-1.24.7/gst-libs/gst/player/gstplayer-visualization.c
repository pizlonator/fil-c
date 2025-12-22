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

/**
 * SECTION:gstplayer-visualization
 * @title: GstPlayerVisualization
 * @short_description: Player Visualization
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplayer-visualization.h"

#include <string.h>

static GMutex vis_lock;
static GQueue vis_list = G_QUEUE_INIT;
static guint32 vis_cookie;

G_DEFINE_BOXED_TYPE (GstPlayerVisualization, gst_player_visualization,
    (GBoxedCopyFunc) gst_player_visualization_copy,
    (GBoxedFreeFunc) gst_player_visualization_free);

/**
 * gst_player_visualization_free:
 * @vis: #GstPlayerVisualization instance
 *
 * Frees a #GstPlayerVisualization.
 */
void
gst_player_visualization_free (GstPlayerVisualization * vis)
{
  g_return_if_fail (vis != NULL);

  g_free (vis->name);
  g_free (vis->description);
  g_free (vis);
}

/**
 * gst_player_visualization_copy:
 * @vis: #GstPlayerVisualization instance
 *
 * Makes a copy of the #GstPlayerVisualization. The result must be
 * freed using gst_player_visualization_free().
 *
 * Returns: (transfer full): an allocated copy of @vis.
 */
GstPlayerVisualization *
gst_player_visualization_copy (const GstPlayerVisualization * vis)
{
  GstPlayerVisualization *ret;

  g_return_val_if_fail (vis != NULL, NULL);

  ret = g_new0 (GstPlayerVisualization, 1);
  ret->name = vis->name ? g_strdup (vis->name) : NULL;
  ret->description = vis->description ? g_strdup (vis->description) : NULL;

  return ret;
}

/**
 * gst_player_visualizations_free:
 * @viss: a %NULL terminated array of #GstPlayerVisualization to free
 *
 * Frees a %NULL terminated array of #GstPlayerVisualization.
 */
void
gst_player_visualizations_free (GstPlayerVisualization ** viss)
{
  GstPlayerVisualization **p;

  g_return_if_fail (viss != NULL);

  p = viss;
  while (*p) {
    g_free ((*p)->name);
    g_free ((*p)->description);
    g_free (*p);
    p++;
  }
  g_free (viss);
}

static void
gst_player_update_visualization_list (void)
{
  GList *features;
  GList *l;
  guint32 cookie;
  GstPlayerVisualization *vis;

  g_mutex_lock (&vis_lock);

  /* check if we need to update the list */
  cookie = gst_registry_get_feature_list_cookie (gst_registry_get ());
  if (vis_cookie == cookie) {
    g_mutex_unlock (&vis_lock);
    return;
  }

  /* if update is needed then first free the existing list */
  while ((vis = g_queue_pop_head (&vis_list)))
    gst_player_visualization_free (vis);

  features = gst_registry_get_feature_list (gst_registry_get (),
      GST_TYPE_ELEMENT_FACTORY);

  for (l = features; l; l = l->next) {
    GstPluginFeature *feature = l->data;
    const gchar *klass;

    klass = gst_element_factory_get_metadata (GST_ELEMENT_FACTORY (feature),
        GST_ELEMENT_METADATA_KLASS);

    if (strstr (klass, "Visualization")) {
      vis = g_new0 (GstPlayerVisualization, 1);

      vis->name = g_strdup (gst_plugin_feature_get_name (feature));
      vis->description =
          g_strdup (gst_element_factory_get_metadata (GST_ELEMENT_FACTORY
              (feature), GST_ELEMENT_METADATA_DESCRIPTION));
      g_queue_push_tail (&vis_list, vis);
    }
  }
  gst_plugin_feature_list_free (features);

  vis_cookie = cookie;

  g_mutex_unlock (&vis_lock);
}

/**
 * gst_player_visualizations_get:
 *
 * Returns: (transfer full) (array zero-terminated=1) (element-type GstPlayerVisualization):
 *  a %NULL terminated array containing all available
 *  visualizations. Use gst_player_visualizations_free() after
 *  usage.
 */
GstPlayerVisualization **
gst_player_visualizations_get (void)
{
  gint i = 0;
  GList *l;
  GstPlayerVisualization **ret;

  gst_player_update_visualization_list ();

  g_mutex_lock (&vis_lock);
  ret = g_new0 (GstPlayerVisualization *, g_queue_get_length (&vis_list) + 1);
  for (l = vis_list.head; l; l = l->next)
    ret[i++] = gst_player_visualization_copy (l->data);
  g_mutex_unlock (&vis_lock);

  return ret;
}
