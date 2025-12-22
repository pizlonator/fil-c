/* GStreamer
 * Copyright (C) <2017> Philippe Renon <philippe_renon@yahoo.fr>
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

#include "cameraevent.hpp"

#include <opencv2/opencv.hpp>

/**
 * gst_camera_event_new_calibrated:
 * @settings: .
 *
 * Creates a new calibrated event.
 *
 * To parse an event created by gst_camera_event_new_calibrated() use
 * gst_camera_event_parse_calibrated().
 *
 * Returns: The new GstEvent
 */
GstEvent *
gst_camera_event_new_calibrated (gchar * settings)
{
  GstEvent *calibrated_event;
  GstStructure *s;

  s = gst_structure_new (GST_CAMERA_EVENT_CALIBRATED_NAME,
      "undistort-settings", G_TYPE_STRING, g_strdup (settings), NULL);

  calibrated_event = gst_event_new_custom (GST_EVENT_CUSTOM_BOTH, s);

  return calibrated_event;
}

/**
 * gst_camera_event_parse_calibrated:
 * @event: A #GstEvent to parse
 * @in_still: A boolean to receive the still-frame status from the event, or NULL
 *
 * Parse a #GstEvent, identify if it is a calibrated event, and
 * return the s.
 *
 * Create a calibrated event using gst_camera_event_new_calibrated()
 *
 * Returns: %TRUE if the event is a valid calibrated event. %FALSE if not
 */
gboolean
gst_camera_event_parse_calibrated (GstEvent * event, gchar ** settings)
{
  const GstStructure *s;

  g_return_val_if_fail (event != NULL, FALSE);

  if (GST_EVENT_TYPE (event) != GST_EVENT_CUSTOM_BOTH)
    return FALSE;               /* Not a calibrated event */

  s = gst_event_get_structure (event);
  if (s == NULL
      || !gst_structure_has_name (s, GST_CAMERA_EVENT_CALIBRATED_NAME))
    return FALSE;               /* Not a calibrated event */

  const gchar *str = gst_structure_get_string (s, "undistort-settings");
  if (!str)
    return FALSE;               /* Not calibrated frame event */

  *settings = g_strdup (str);

  return TRUE;
}
