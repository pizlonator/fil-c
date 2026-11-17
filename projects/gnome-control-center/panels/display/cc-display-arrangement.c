/* cc-display-arrangement.c
 *
 * Copyright (C) 2007, 2008, 2017  Red Hat, Inc.
 * Copyright (C) 2013 Intel, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include "cc-display-arrangement.h"
#include "cc-display-config.h"

struct _CcDisplayArrangement
{
  GtkDrawingArea    object;

  CcDisplayConfig  *config;

  cairo_matrix_t    to_widget;
  cairo_matrix_t    to_actual;

  gboolean          drag_active;
  CcDisplayMonitor *selected_output;
  CcDisplayMonitor *prelit_output;
  /* Starting position of cursor inside the monitor. */
  gdouble           drag_anchor_x;
  gdouble           drag_anchor_y;

  guint             major_snap_distance;
};

typedef struct _CcDisplayArrangement CcDisplayArrangement;

enum {
  PROP_0,
  PROP_CONFIG,
  PROP_SELECTED_OUTPUT,
  PROP_LAST
};

typedef enum {
  SNAP_DIR_NONE = 0,
  SNAP_DIR_X    = 1 << 0,
  SNAP_DIR_Y    = 1 << 1,
  SNAP_DIR_BOTH = (SNAP_DIR_X | SNAP_DIR_Y),
} SnapDirection;

typedef struct {
  cairo_matrix_t     to_widget;
  guint              major_snap_distance;
  gdouble            dist_x;
  gdouble            dist_y;
  gint               mon_x;
  gint               mon_y;
  SnapDirection      snapped;
} SnapData;

#define MARGIN_PX  0
#define MARGIN_MON  0.66
#define MAJOR_SNAP_DISTANCE 25
#define MINOR_SNAP_DISTANCE 5
#define MIN_OVERLAP 25

G_DEFINE_TYPE (CcDisplayArrangement, cc_display_arrangement, GTK_TYPE_DRAWING_AREA)

static GParamSpec *props[PROP_LAST];

static void
apply_rotation_to_geometry (CcDisplayMonitor *output,
                            int              *w,
                            int              *h)
{
  CcDisplayRotation rotation;

  rotation = cc_display_monitor_get_rotation (output);
  if ((rotation == CC_DISPLAY_ROTATION_90) || (rotation == CC_DISPLAY_ROTATION_270))
    {
      int tmp;
      tmp = *h;
      *h = *w;
      *w = tmp;
    }
}

/* get_geometry */
static void
get_scaled_geometry (CcDisplayConfig  *config,
                     CcDisplayMonitor *output,
                     int              *x,
                     int              *y,
                     int              *w,
                     int              *h)
{
  if (cc_display_monitor_is_active (output))
    {
      cc_display_monitor_get_geometry (output, x, y, w, h);
    }
  else
    {
      cc_display_monitor_get_geometry (output, x, y, NULL, NULL);
      cc_display_mode_get_resolution (cc_display_monitor_get_preferred_mode (output), w, h);
    }

  if (cc_display_config_is_layout_logical (config))
    {
      double scale = cc_display_monitor_get_scale (output);
      *w = round (*w / scale);
      *h = round (*h / scale);
    }

  apply_rotation_to_geometry (output, w, h);
}

static void
get_bounding_box (CcDisplayConfig *config,
                  gint            *x1,
                  gint            *y1,
                  gint            *x2,
                  gint            *y2,
                  gint            *max_w,
                  gint            *max_h)
{
  GList *outputs, *l;

  g_assert (x1 && y1 && x2 && y2);

  *x1 = *y1 = G_MAXINT;
  *x2 = *y2 = G_MININT;
  *max_w = 0;
  *max_h = 0;

  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      int x, y, w, h;

      if (!cc_display_monitor_is_useful (output))
        continue;

      get_scaled_geometry (config, output, &x, &y, &w, &h);

      *x1 = MIN (*x1, x);
      *y1 = MIN (*y1, y);
      *x2 = MAX (*x2, x + w);
      *y2 = MAX (*y2, y + h);
      *max_w = MAX (*max_w, w);
      *max_h = MAX (*max_h, h);
    }
}

static void
monitor_get_drawing_rect (CcDisplayArrangement *self,
                          CcDisplayMonitor     *output,
                          gint                 *x1,
                          gint                 *y1,
                          gint                 *x2,
                          gint                 *y2)
{
  gdouble x, y;

  get_scaled_geometry (self->config, output, x1, y1, x2, y2);

  /* get_scaled_geometry returns the width and height */
  *x2 = *x1 + *x2;
  *y2 = *y1 + *y2;

  x = *x1; y = *y1;
  cairo_matrix_transform_point (&self->to_widget, &x, &y);
  *x1 = round (x);
  *y1 = round (y);

  x = *x2; y = *y2;
  cairo_matrix_transform_point (&self->to_widget, &x, &y);
  *x2 = round (x);
  *y2 = round (y);
}


static void
get_snap_distance (SnapData  *snap_data,
                   gint       mon_x,
                   gint       mon_y,
                   gint       new_x,
                   gint       new_y,
                   gdouble   *dist_x,
                   gdouble   *dist_y)
{
  gdouble local_dist_x, local_dist_y;

  local_dist_x = ABS (mon_x - new_x);
  local_dist_y = ABS (mon_y - new_y);

  cairo_matrix_transform_distance (&snap_data->to_widget, &local_dist_x, &local_dist_y);

  if (dist_x)
    *dist_x = local_dist_x;
  if (dist_y)
    *dist_y = local_dist_y;
}

static void
maybe_update_snap (SnapData       *snap_data,
                   gint            mon_x,
                   gint            mon_y,
                   gint            new_x,
                   gint            new_y,
                   SnapDirection   snapped,
                   SnapDirection   major_axis,
                   gint            minor_unlimited)
{
  SnapDirection update_snap = SNAP_DIR_NONE;
  gdouble dist_x, dist_y;
  gdouble dist;

  get_snap_distance (snap_data, mon_x, mon_y, new_x, new_y, &dist_x, &dist_y);
  dist = MAX (dist_x, dist_y);

  /* Snap by the variable max snap distance on the major axis, ensure the
   * minor axis is below the minimum snapping distance (often just zero). */
  switch (major_axis)
    {
      case SNAP_DIR_X:
        if (dist_x > snap_data->major_snap_distance)
          return;
        if (dist_y > MINOR_SNAP_DISTANCE)
          {
            if (new_y > mon_y && minor_unlimited <= 0)
              return;
            if (new_y < mon_y && minor_unlimited >= 0)
              return;
          }
        break;

      case SNAP_DIR_Y:
        if (dist_y > snap_data->major_snap_distance)
          return;
        if (dist_x > MINOR_SNAP_DISTANCE)
          {
            if (new_x > mon_x && minor_unlimited <= 0)
              return;
            if (new_x < mon_x && minor_unlimited >= 0)
              return;
          }
        break;

      default:
        g_assert_not_reached();
    }

  if (snapped == SNAP_DIR_BOTH)
    {
      if (snap_data->snapped == SNAP_DIR_NONE)
        update_snap = SNAP_DIR_BOTH;

      /* Update, if this is closer on the main axis. */
      if (((major_axis == SNAP_DIR_X) && (dist_x < snap_data->dist_x)) ||
          ((major_axis == SNAP_DIR_Y) && (dist_y < snap_data->dist_y)))
        {
          update_snap = SNAP_DIR_BOTH;
        }

      /* Also update if we were only snapping in one direction earlier and it
       * is better or equally good. */
      if ((snap_data->snapped == SNAP_DIR_X && (dist <= snap_data->dist_x)) ||
          (snap_data->snapped == SNAP_DIR_Y && (dist <= snap_data->dist_y)))
        {
          update_snap = SNAP_DIR_BOTH;
        }

      /* Also allow a minor axis to be added if the first axis remains identical. */
      if (((snap_data->snapped == SNAP_DIR_X) && (major_axis == SNAP_DIR_X) && (new_x == snap_data->mon_x)) ||
          ((snap_data->snapped == SNAP_DIR_Y) && (major_axis == SNAP_DIR_Y) && (new_y == snap_data->mon_y)))
        {
          update_snap = SNAP_DIR_BOTH;
        }
    }
  else if (snapped == SNAP_DIR_X)
    {
      if (dist_x < snap_data->dist_x || (snap_data->snapped & SNAP_DIR_X) == SNAP_DIR_NONE)
        update_snap = SNAP_DIR_X;
    }
  else if (snapped == SNAP_DIR_Y)
    {
      if (dist_y < snap_data->dist_y || (snap_data->snapped & SNAP_DIR_Y) == SNAP_DIR_NONE)
        update_snap = SNAP_DIR_Y;
    }
  else
    {
      g_assert_not_reached ();
    }

  if (update_snap & SNAP_DIR_X)
    {
      snap_data->dist_x = dist_x;
      snap_data->mon_x = new_x;
      snap_data->snapped = snap_data->snapped | SNAP_DIR_X;
    }
  if (update_snap & SNAP_DIR_Y)
    {
      snap_data->dist_y = dist_y;
      snap_data->mon_y = new_y;
      snap_data->snapped = snap_data->snapped | SNAP_DIR_Y;
    }
}

static void
find_best_snapping (CcDisplayConfig   *config,
                    CcDisplayMonitor  *snap_output,
                    SnapData          *snap_data)
{
  GList *outputs, *l;
  gint x1, y1, x2, y2;
  gint w, h;

  g_assert (snap_data != NULL);

  get_scaled_geometry (config, snap_output, &x1, &y1, &w, &h);
  x2 = x1 + w;
  y2 = y1 + h;

#define OVERLAP(_s1, _s2, _t1, _t2) ((_s1) <= (_t2) && (_t1) <= (_s2))

  outputs = cc_display_config_get_monitors (config);
  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      gint _x1, _y1, _x2, _y2, _h, _w;
      gint bottom_snap_pos;
      gint top_snap_pos;
      gint left_snap_pos;
      gint right_snap_pos;
      gdouble dist_x, dist_y;
      gdouble tmp;

      if (output == snap_output)
        continue;

      if (!cc_display_monitor_is_useful (output))
        continue;

      get_scaled_geometry (config, output, &_x1, &_y1, &_w, &_h);
      _x2 = _x1 + _w;
      _y2 = _y1 + _h;

      top_snap_pos = _y1 - h;
      bottom_snap_pos = _y2;
      left_snap_pos = _x1 - w;
      right_snap_pos = _x2;

      dist_y = 9999;
      /* overlap on the X axis */
      if (OVERLAP (x1, x2, _x1, _x2))
        {
          get_snap_distance (snap_data, x1, y1, x1, top_snap_pos, NULL, &dist_y);
          get_snap_distance (snap_data, x1, y1, x1, bottom_snap_pos, NULL, &tmp);
          dist_y = MIN(dist_y, tmp);
        }

      dist_x = 9999;
      /* overlap on the Y axis */
      if (OVERLAP (y1, y2, _y1, _y2))
        {
          get_snap_distance (snap_data, x1, y1, left_snap_pos, y1, &dist_x, NULL);
          get_snap_distance (snap_data, x1, y1, right_snap_pos, y1, &tmp, NULL);
          dist_x = MIN(dist_x, tmp);
        }

      /* We only snap horizontally or vertically to an edge of the same monitor */
      if (dist_y < dist_x)
        {
          maybe_update_snap (snap_data, x1, y1, x1, top_snap_pos, SNAP_DIR_Y, SNAP_DIR_Y, 0);
          maybe_update_snap (snap_data, x1, y1, x1, bottom_snap_pos, SNAP_DIR_Y, SNAP_DIR_Y, 0);
        }
      else if (dist_x < 9999)
        {
          maybe_update_snap (snap_data, x1, y1, left_snap_pos, y1, SNAP_DIR_X, SNAP_DIR_X, 0);
          maybe_update_snap (snap_data, x1, y1, right_snap_pos, y1, SNAP_DIR_X, SNAP_DIR_X, 0);
        }

      /* Left/right edge identical on the top */
      maybe_update_snap (snap_data, x1, y1, _x1, top_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 0);
      maybe_update_snap (snap_data, x1, y1, _x2 - w, top_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 0);

      /* Left/right edge identical on the bottom */
      maybe_update_snap (snap_data, x1, y1, _x1, bottom_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 0);
      maybe_update_snap (snap_data, x1, y1, _x2 - w, bottom_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 0);

      /* Top/bottom edge identical on the left */
      maybe_update_snap (snap_data, x1, y1, left_snap_pos, _y1, SNAP_DIR_BOTH, SNAP_DIR_X, 0);
      maybe_update_snap (snap_data, x1, y1, left_snap_pos, _y2 - h, SNAP_DIR_BOTH, SNAP_DIR_X, 0);

      /* Top/bottom edge identical on the right */
      maybe_update_snap (snap_data, x1, y1, right_snap_pos, _y1, SNAP_DIR_BOTH, SNAP_DIR_X, 0);
      maybe_update_snap (snap_data, x1, y1, right_snap_pos, _y2 - h, SNAP_DIR_BOTH, SNAP_DIR_X, 0);

      /* If snapping is infinite, then add snapping points with minimal overlap
       * to prevent detachment.
       * This is similar to the above but simply re-defines the snapping pos
       * to have only minimal overlap */
      if (snap_data->major_snap_distance == G_MAXUINT)
        {
          /* Hanging over the left/right edge on the top */
          maybe_update_snap (snap_data, x1, y1, _x1 - w + MIN_OVERLAP, top_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 1);
          maybe_update_snap (snap_data, x1, y1, _x2 - MIN_OVERLAP, top_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, -1);

          /* Left/right edge identical on the bottom */
          maybe_update_snap (snap_data, x1, y1, _x1 - w + MIN_OVERLAP, bottom_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, 1);
          maybe_update_snap (snap_data, x1, y1, _x2 - MIN_OVERLAP, bottom_snap_pos, SNAP_DIR_BOTH, SNAP_DIR_Y, -1);

          /* Top/bottom edge identical on the left */
          maybe_update_snap (snap_data, x1, y1, left_snap_pos, _y1 - h + MIN_OVERLAP, SNAP_DIR_BOTH, SNAP_DIR_X, 1);
          maybe_update_snap (snap_data, x1, y1, left_snap_pos, _y2 - MIN_OVERLAP, SNAP_DIR_BOTH, SNAP_DIR_X, -1);

          /* Top/bottom edge identical on the right */
          maybe_update_snap (snap_data, x1, y1, right_snap_pos, _y1 - h + MIN_OVERLAP, SNAP_DIR_BOTH, SNAP_DIR_X, 1);
          maybe_update_snap (snap_data, x1, y1, right_snap_pos, _y2 - MIN_OVERLAP, SNAP_DIR_BOTH, SNAP_DIR_X, -1);
        }
    }

#undef OVERLAP
}

static void
cc_display_arrangement_update_matrices (CcDisplayArrangement *self)
{
  GtkAllocation allocation;
  gdouble scale, scale_h, scale_w;
  gint x1, y1, x2, y2, max_w, max_h;

  g_assert (self->config);

  /* Do not update the matrices while the user is dragging things around. */
  if (self->drag_active)
    return;

  get_bounding_box (self->config, &x1, &y1, &x2, &y2, &max_w, &max_h);
  gtk_widget_get_allocation (GTK_WIDGET (self), &allocation);

  scale_h = (gdouble) (allocation.width - 2 * MARGIN_PX) / (x2 - x1 + max_w * 2 * MARGIN_MON);
  scale_w = (gdouble) (allocation.height - 2 * MARGIN_PX) / (y2 - y1 + max_h * 2 * MARGIN_MON);

  scale = MIN (scale_h, scale_w);

  cairo_matrix_init_identity (&self->to_widget);
  cairo_matrix_translate (&self->to_widget, allocation.width / 2.0, allocation.height / 2.0);
  cairo_matrix_scale (&self->to_widget, scale, scale);
  cairo_matrix_translate (&self->to_widget, - (x1 + x2) / 2.0, - (y1 + y2) / 2.0);

  self->to_actual = self->to_widget;
  cairo_matrix_invert (&self->to_actual);
}

static CcDisplayMonitor*
cc_display_arrangement_find_monitor_at (CcDisplayArrangement *self,
                                        gint                  x,
                                        gint                  y)
{
  g_autoptr(GList) outputs = NULL;
  GList *l;

  outputs = g_list_copy (cc_display_config_get_monitors (self->config));

  if (self->selected_output)
    outputs = g_list_prepend (outputs, self->selected_output);

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      gint x1, y1, x2, y2;

      if (!cc_display_monitor_is_useful (output))
        continue;

      monitor_get_drawing_rect (self, output, &x1, &y1, &x2, &y2);

      if (x >= x1 && x <= x2 && y >= y1 && y <= y2)
        return output;
    }

  return NULL;
}

static void
on_output_changed_cb (CcDisplayArrangement *self,
                      CcDisplayMonitor     *output)
{
  if (cc_display_config_count_useful_monitors (self->config) > 2)
    self->major_snap_distance = MAJOR_SNAP_DISTANCE;
  else
    self->major_snap_distance = G_MAXUINT;

  gtk_widget_queue_draw (GTK_WIDGET (self));
}

static void
cc_display_arrangement_draw (GtkDrawingArea *drawing_area,
                             cairo_t        *cr,
                             gint            width,
                             gint            height,
                             gpointer        user_data)
{
  CcDisplayArrangement *self = CC_DISPLAY_ARRANGEMENT (drawing_area);
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));
  g_autoptr(GList) outputs = NULL;
  GList *l;

  if (!self->config)
    return;

  cc_display_arrangement_update_matrices (self);

  /* Draw in reverse order so that hit detection matches visual. Also pull
   * the selected output to the back. */
  outputs = g_list_copy (cc_display_config_get_monitors (self->config));
  outputs = g_list_remove (outputs, self->selected_output);
  if (self->selected_output != NULL)
    outputs = g_list_prepend (outputs, self->selected_output);
  outputs = g_list_reverse (outputs);

  for (l = outputs; l; l = l->next)
    {
      CcDisplayMonitor *output = l->data;
      GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
      GtkBorder border, padding, margin;
      gint x1, y1, x2, y2;
      gint w, h;
      gint num;

      if (!cc_display_monitor_is_useful (output))
        continue;

      gtk_style_context_save (context);
      cairo_save (cr);

      gtk_style_context_add_class (context, "monitor");

      if (output == self->selected_output)
        state |= GTK_STATE_FLAG_SELECTED;
      if (output == self->prelit_output)
        state |= GTK_STATE_FLAG_PRELIGHT;

      gtk_style_context_set_state (context, state);
      if (cc_display_monitor_is_primary (output) || cc_display_config_is_cloning (self->config))
        gtk_style_context_add_class (context, "primary");

      /* Set in cc-display-panel.c */
      num = cc_display_monitor_get_ui_number (output);

      monitor_get_drawing_rect (self, output, &x1, &y1, &x2, &y2);
      w = x2 - x1;
      h = y2 - y1;

      cairo_translate (cr, x1, y1);

      gtk_style_context_get_margin (context, &margin);

      cairo_translate (cr, margin.left, margin.top);

      w -= margin.left + margin.right;
      h -= margin.top + margin.bottom;

      gtk_render_background (context, cr, 0, 0, w, h);
      gtk_render_frame (context, cr, 0, 0, w, h);

      gtk_style_context_get_border (context, &border);
      gtk_style_context_get_padding (context, &padding);

      w -= border.left + border.right + padding.left + padding.right;
      h -= border.top + border.bottom + padding.top + padding.bottom;

      cairo_translate (cr, border.left + padding.left, border.top + padding.top);

      if (num > 0)
        {
          PangoLayout *layout;
          g_autofree gchar *number_str = NULL;
          PangoRectangle extents;
          GdkRGBA color;
          gdouble text_width, text_padding;

          gtk_style_context_add_class (context, "monitor-label");
          gtk_style_context_remove_class (context, "monitor");

          gtk_style_context_get_border (context, &border);
          gtk_style_context_get_padding (context, &padding);

          cairo_translate (cr, w / 2, h / 2);

          number_str = g_strdup_printf ("%d", num);
          layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), number_str);
          pango_layout_get_extents (layout, NULL, &extents);

          h = (extents.height - extents.y) / PANGO_SCALE;
          text_width = (extents.width - extents.x) / PANGO_SCALE;
          w = MAX (text_width, h - padding.left - padding.right);
          text_padding = w - text_width;

          w += border.left + border.right + padding.left + padding.right;
          h += border.top + border.bottom + padding.top + padding.bottom;

          /* Enforce evenness */
          if ((w % 2) != 0)
                  w++;
          if ((h % 2) != 0)
                  h++;

          cairo_translate (cr, - w / 2, - h / 2);

          gtk_render_background (context, cr, 0, 0, w, h);
          gtk_render_frame (context, cr, 0, 0, w, h);

          cairo_translate (cr, border.left + padding.left, border.top + padding.top);
          cairo_translate (cr, extents.x + text_padding / 2, 0);

          gtk_style_context_get_color (context, &color);
          gdk_cairo_set_source_rgba (cr, &color);

          gtk_render_layout (context, cr, 0, 0, layout);
          g_object_unref (layout);
        }

      gtk_style_context_restore (context);
      cairo_restore (cr);
    }
}

static gboolean
on_click_gesture_pressed_cb (CcDisplayArrangement *self,
                             gint                  n_press,
                             gdouble               x,
                             gdouble               y)
{
  CcDisplayMonitor *output;
  gdouble event_x, event_y;
  gint mon_x, mon_y;

  if (!self->config)
    return FALSE;

  g_return_val_if_fail (self->drag_active == FALSE, FALSE);

  output = cc_display_arrangement_find_monitor_at (self, x, y);
  if (!output)
    return FALSE;

  event_x = x;
  event_y = y;

  cairo_matrix_transform_point (&self->to_actual, &event_x, &event_y);
  cc_display_monitor_get_geometry (output, &mon_x, &mon_y, NULL, NULL);

  cc_display_arrangement_set_selected_output (self, output);

  if (cc_display_config_count_useful_monitors (self->config) > 1)
    {
      self->drag_active = TRUE;
      self->drag_anchor_x = event_x - mon_x;
      self->drag_anchor_y = event_y - mon_y;
    }

  return TRUE;
}

static gboolean
on_click_gesture_released_cb (CcDisplayArrangement *self,
                              gint                  n_press,
                              gdouble               x,
                              gdouble               y)
{
  CcDisplayMonitor *output;

  if (!self->config)
    return FALSE;

  if (!self->drag_active)
    return FALSE;

  self->drag_active = FALSE;

  output = cc_display_arrangement_find_monitor_at (self, x, y);
  gtk_widget_set_cursor_from_name (GTK_WIDGET (self),
                                   output != NULL ? "fleur" : NULL);

  /* And queue a redraw to recenter everything */
  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_signal_emit_by_name (G_OBJECT (self), "updated");

  return TRUE;
}

static gboolean
on_motion_controller_motion_cb (CcDisplayArrangement *self,
                                gdouble               x,
                                gdouble               y)
{
  gdouble event_x, event_y;
  gint mon_x, mon_y;
  SnapData snap_data;

  if (!self->config)
    return FALSE;

  if (cc_display_config_count_useful_monitors (self->config) <= 1)
    return FALSE;

  if (!self->drag_active)
    {
      CcDisplayMonitor *output;
      output = cc_display_arrangement_find_monitor_at (self, x, y);

      gtk_widget_set_cursor_from_name (GTK_WIDGET (self),
                                       output != NULL ? "fleur" : NULL);
      if (self->prelit_output != output)
        gtk_widget_queue_draw (GTK_WIDGET (self));

      self->prelit_output = output;

      return FALSE;
    }

  g_assert (self->selected_output);

  event_x = x;
  event_y = y;

  cairo_matrix_transform_point (&self->to_actual, &event_x, &event_y);

  mon_x = round (event_x - self->drag_anchor_x);
  mon_y = round (event_y - self->drag_anchor_y);

  /* The monitor is now at the location as if there was no snapping whatsoever. */
  snap_data.snapped = SNAP_DIR_NONE;
  snap_data.mon_x = mon_x;
  snap_data.mon_y = mon_y;
  snap_data.dist_x = 0;
  snap_data.dist_y = 0;
  snap_data.to_widget = self->to_widget;
  snap_data.major_snap_distance = self->major_snap_distance;

  cc_display_monitor_set_position (self->selected_output, mon_x, mon_y);

  find_best_snapping (self->config, self->selected_output, &snap_data);

  cc_display_monitor_set_position (self->selected_output, snap_data.mon_x, snap_data.mon_y);

  return TRUE;
}

static void
cc_display_arrangement_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  CcDisplayArrangement *self = CC_DISPLAY_ARRANGEMENT (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      g_value_set_object (value, self->config);
      break;

    case PROP_SELECTED_OUTPUT:
      g_value_set_object (value, self->selected_output);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_arrangement_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  CcDisplayArrangement *obj = CC_DISPLAY_ARRANGEMENT (object);

  switch (prop_id)
    {
    case PROP_CONFIG:
      cc_display_arrangement_set_config (obj, g_value_get_object (value));
      break;

    case PROP_SELECTED_OUTPUT:
      cc_display_arrangement_set_selected_output (obj, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_display_arrangement_finalize (GObject *object)
{
  CcDisplayArrangement *self = CC_DISPLAY_ARRANGEMENT (object);

  g_clear_object (&self->config);

  G_OBJECT_CLASS (cc_display_arrangement_parent_class)->finalize (object);
}

static void
cc_display_arrangement_class_init (CcDisplayArrangementClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = cc_display_arrangement_finalize;
  gobject_class->get_property = cc_display_arrangement_get_property;
  gobject_class->set_property = cc_display_arrangement_set_property;

  props[PROP_CONFIG] = g_param_spec_object ("config", "Display Config",
                                            "The display configuration to work with",
                                            CC_TYPE_DISPLAY_CONFIG,
                                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  props[PROP_SELECTED_OUTPUT] = g_param_spec_object ("selected-output", "Selected Output",
                                                     "The output that is currently selected on the configuration",
                                                     CC_TYPE_DISPLAY_MONITOR,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class,
                                     PROP_LAST,
                                     props);

  g_signal_new ("updated",
                CC_TYPE_DISPLAY_ARRANGEMENT,
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL,
                G_TYPE_NONE, 0);

  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "display-arrangement");
}

static void
cc_display_arrangement_init (CcDisplayArrangement *self)
{
  GtkEventController *motion_controller;
  GtkGesture *click_gesture;

  click_gesture = gtk_gesture_click_new ();
  g_signal_connect_swapped (click_gesture, "pressed", G_CALLBACK (on_click_gesture_pressed_cb), self);
  g_signal_connect_swapped (click_gesture, "released", G_CALLBACK (on_click_gesture_released_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), GTK_EVENT_CONTROLLER (click_gesture));

  motion_controller = gtk_event_controller_motion_new ();
  g_signal_connect_swapped (motion_controller, "motion", G_CALLBACK (on_motion_controller_motion_cb), self);
  gtk_widget_add_controller (GTK_WIDGET (self), motion_controller);

  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (self),
                                  cc_display_arrangement_draw,
                                  self,
                                  NULL);

  self->major_snap_distance = MAJOR_SNAP_DISTANCE;
}

CcDisplayArrangement*
cc_display_arrangement_new (CcDisplayConfig *config)
{
  return g_object_new (CC_TYPE_DISPLAY_ARRANGEMENT, "config", config, NULL);
}

CcDisplayConfig*
cc_display_arrangement_get_config (CcDisplayArrangement *self)
{
  return self->config;
}

void
cc_display_arrangement_set_config (CcDisplayArrangement *self,
                                   CcDisplayConfig      *config)
{
  const gchar *signals[] = { "rotation", "mode", "primary", "active", "scale", "position-changed", "is-usable" };
  GList *outputs, *l;
  guint i;

  if (self->config)
    {
      outputs = cc_display_config_get_monitors (self->config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          g_signal_handlers_disconnect_by_data (output, self);
        }
    }
  g_clear_object (&self->config);

  self->drag_active = FALSE;

  /* Listen to all the signals */
  if (config)
    {
      self->config = g_object_ref (config);

      outputs = cc_display_config_get_monitors (self->config);
      for (l = outputs; l; l = l->next)
        {
          CcDisplayMonitor *output = l->data;

          for (i = 0; i < G_N_ELEMENTS (signals); ++i)
            g_signal_connect_object (output, signals[i], G_CALLBACK (on_output_changed_cb), self, G_CONNECT_SWAPPED);
        }
    }

  cc_display_arrangement_set_selected_output (self, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_CONFIG]);
}

CcDisplayMonitor*
cc_display_arrangement_get_selected_output (CcDisplayArrangement *self)
{
  return self->selected_output;
}

void
cc_display_arrangement_set_selected_output (CcDisplayArrangement *self,
                                            CcDisplayMonitor     *output)
{
  g_return_if_fail (self->drag_active == FALSE);

  /* XXX: Could check that it actually belongs to the right config object. */
  self->selected_output = output;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_OUTPUT]);
}

static gboolean
try_snap_output (CcDisplayConfig  *config,
                 CcDisplayMonitor *output)
{
  SnapData snap_data;
  gint x, y, w, h;

  if (!cc_display_monitor_is_useful (output))
    return FALSE;

  get_scaled_geometry (config, output, &x, &y, &w, &h);

  snap_data.snapped = SNAP_DIR_NONE;
  snap_data.mon_x = x;
  snap_data.mon_y = y;
  snap_data.dist_x = 0;
  snap_data.dist_y = 0;
  cairo_matrix_init_identity (&snap_data.to_widget);
  snap_data.major_snap_distance = G_MAXUINT;

  find_best_snapping (config, output, &snap_data);

  if (x != snap_data.mon_x || y != snap_data.mon_y)
    {
      cc_display_monitor_set_position (output, snap_data.mon_x, snap_data.mon_y);
      return TRUE;
    }

  return FALSE;
}

void
cc_display_config_snap_outputs (CcDisplayConfig  *config)
{
  GList *l;

  if (cc_display_config_count_useful_monitors (config) <= 1)
    return;

  for (l = cc_display_config_get_monitors (config); l; l = l->next)
    {
      try_snap_output (config, l->data);
    }
}
