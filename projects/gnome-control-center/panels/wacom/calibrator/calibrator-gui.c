/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 *         (based on previous work by Joaquim Rocha, Tias Guns and Soren Hauberg)
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

#include "calibrator.h"
#include "calibrator-gui.h"
#include "cc-clock.h"

struct _CcCalibArea
{
  GtkWindow parent_instance;

  struct Calib calibrator;
  XYinfo       axis;
  gboolean     swap;
  gboolean     success;
  GsdDevice   *device;

  GtkWidget  *error_revealer;
  GtkWidget  *title_revealer;
  GtkWidget  *subtitle_revealer;
  GtkWidget  *clock;
  GtkWidget  *target1, *target2, *target3, *target4;
  GtkWidget  *stack;
  GtkWidget  *success_page;
  GtkCssProvider *style_provider;

  FinishCallback callback;
  gpointer       user_data;
};

G_DEFINE_TYPE (CcCalibArea, cc_calib_area, GTK_TYPE_WINDOW)

/* Timeout parameters */
#define MAX_TIME                15000 /* 15000 = 15 sec */
#define END_TIME                750   /*  750 = 0.75 sec */

static void
cc_calib_area_notify_finish (CcCalibArea *area)
{
  gtk_widget_set_visible (GTK_WIDGET (area), FALSE);

  (*area->callback) (area, area->user_data);
}

static gboolean
on_close_request (GtkWidget   *widget,
                  CcCalibArea *area)
{
  cc_calib_area_notify_finish (area);
  return GDK_EVENT_PROPAGATE;
}

static gboolean
cc_calib_area_finish_idle_cb (CcCalibArea *area)
{
  cc_calib_area_notify_finish (area);
  return FALSE;
}

static void
set_success (CcCalibArea *area)
{
  gtk_stack_set_visible_child (GTK_STACK (area->stack), area->success_page);
}

static void
set_calibration_status (CcCalibArea *area)
{
  area->success = finish (&area->calibrator, &area->axis, &area->swap);

  if (area->success)
    {
      set_success (area);
      g_timeout_add (END_TIME,
                     (GSourceFunc) cc_calib_area_finish_idle_cb,
                     area);
    }
  else
    {
      g_idle_add ((GSourceFunc) cc_calib_area_finish_idle_cb, area);
    }
}

static void
show_error_message (CcCalibArea *area)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->error_revealer), TRUE);
}

static void
hide_error_message (CcCalibArea *area)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->error_revealer), FALSE);
}

static void
set_active_target (CcCalibArea *area,
                   int          n_target)
{
  GtkWidget *targets[] = {
    area->target1,
    area->target2,
    area->target3,
    area->target4,
  };
  int i;

  for (i = 0; i < G_N_ELEMENTS (targets); i++)
    gtk_widget_set_sensitive (targets[i], i == n_target);
}

static void
on_gesture_press (CcCalibArea     *area,
                  guint            n_press,
                  gdouble          x,
                  gdouble          y,
                  GtkGestureClick *gesture)
{
  GsdDeviceManager *manager = gsd_device_manager_get ();
  gint num_clicks;
  gboolean success;
  GdkDevice *source;
  GsdDevice *device;

  if (area->success)
    return;

  source = gtk_gesture_get_device (GTK_GESTURE (gesture));

  if (gdk_device_get_source (source) == GDK_SOURCE_TOUCHSCREEN)
    return;

  device = gsd_device_manager_lookup_gdk_device (manager, source);

  /* Check matching device if a device was provided */
  if (area->device && area->device != device)
    {
      g_debug ("Ignoring input from device %s",
	       gdk_device_get_name (source));
      return;
    }

  /* Handle click */
  /* FIXME: reset clock */
  success = add_click(&area->calibrator,
                      (int) x,
                      (int) y);

  num_clicks = area->calibrator.num_clicks;

  if (!success && num_clicks == 0)
    show_error_message (area);
  else
    hide_error_message (area);

  /* Are we done yet? */
  if (num_clicks >= 4)
    {
      set_calibration_status (area);
      return;
    }

  set_active_target (area, num_clicks);
}

static gboolean
on_key_release (CcCalibArea           *area,
		guint                  keyval,
		guint                  keycode,
		GdkModifierType        state)
{
  if (area->success || keyval != GDK_KEY_Escape)
    return GDK_EVENT_PROPAGATE;

  cc_calib_area_notify_finish (area);
  return GDK_EVENT_STOP;
}

static void
on_clock_finished (CcCalibArea *area)
{
  set_calibration_status (area);
}

static void
on_title_revealed (CcCalibArea *area)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->subtitle_revealer), TRUE);
}

static void
on_fullscreen (GtkWindow    *window,
               GParamSpec   *pspec,
               CcCalibArea  *area)
{
  if (!gtk_window_is_fullscreen (window))
    return;

  g_signal_connect_swapped (area->title_revealer,
                            "notify::child-revealed",
                            G_CALLBACK (on_title_revealed),
                            area);
  gtk_revealer_set_reveal_child (GTK_REVEALER (area->title_revealer), TRUE);

  set_active_target (area, 0);
}

static void
cc_calib_area_finalize (GObject *object)
{
  CcCalibArea *area = CC_CALIB_AREA (object);

  gtk_style_context_remove_provider_for_display (gtk_widget_get_display (GTK_WIDGET (area)),
                                                 GTK_STYLE_PROVIDER (area->style_provider));

  G_OBJECT_CLASS (cc_calib_area_parent_class)->finalize (object);
}

static void
cc_calib_area_size_allocate (GtkWidget *widget,
                             int        width,
                             int        height,
                             int        baseline)
{
  CcCalibArea *calib_area = CC_CALIB_AREA (widget);

  if (calib_area->calibrator.geometry.width != width ||
      calib_area->calibrator.geometry.height != height)
    {
      calib_area->calibrator.geometry.width = width;
      calib_area->calibrator.geometry.height = height;

      /* reset calibration if already started */
      reset (&calib_area->calibrator);
      set_active_target (calib_area, 0);
    }

  GTK_WIDGET_CLASS (cc_calib_area_parent_class)->size_allocate (widget,
                                                                width,
                                                                height,
                                                                baseline);
}

static void
cc_calib_area_class_init (CcCalibAreaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_calib_area_finalize;

  widget_class->size_allocate = cc_calib_area_size_allocate;

  g_type_ensure (CC_TYPE_CLOCK);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wacom/calibrator/calibrator.ui");

  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, error_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, title_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, subtitle_revealer);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, clock);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, target1);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, target2);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, target3);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, target4);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, stack);
  gtk_widget_class_bind_template_child (widget_class, CcCalibArea, success_page);
}

static void
cc_calib_area_init (CcCalibArea *calib_area)
{
  GtkGesture *click;
  GtkEventController *key;

  gtk_widget_init_template (GTK_WIDGET (calib_area));

  calib_area->style_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (calib_area->style_provider, "/org/gnome/control-center/wacom/calibrator/calibrator.css");
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (GTK_WIDGET (calib_area)),
                                              GTK_STYLE_PROVIDER (calib_area->style_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);

  cc_clock_set_duration (CC_CLOCK (calib_area->clock), MAX_TIME);
  g_signal_connect_swapped (calib_area->clock, "finished",
                            G_CALLBACK (on_clock_finished), calib_area);

#ifndef FAKE_AREA
  /* No cursor */
  gtk_widget_realize (GTK_WIDGET (calib_area));
  gtk_widget_set_cursor_from_name (GTK_WIDGET (calib_area), "blank");

  gtk_widget_set_can_focus (GTK_WIDGET (calib_area), TRUE);
#endif /* FAKE_AREA */

  g_signal_connect (calib_area,
                    "close-request",
                    G_CALLBACK (on_close_request),
                    calib_area);
  g_signal_connect (calib_area,
                    "notify::fullscreened",
                    G_CALLBACK (on_fullscreen),
                    calib_area);

  click = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_PRIMARY);
  g_signal_connect_swapped (click, "pressed",
                            G_CALLBACK (on_gesture_press), calib_area);
  gtk_widget_add_controller (GTK_WIDGET (calib_area),
                             GTK_EVENT_CONTROLLER (click));

  key = gtk_event_controller_key_new ();
  g_signal_connect_swapped (key, "key-released",
                            G_CALLBACK (on_key_release), calib_area);
  gtk_widget_add_controller (GTK_WIDGET (calib_area), key);
}

/**
 * Creates the windows and other objects required to do calibration
 * under GTK. When the window is closed (timed out, calibration finished
 * or user cancellation), callback will be called, where you should call
 * cc_calib_area_finish().
 */
CcCalibArea *
cc_calib_area_new (GdkDisplay     *display,
                   GdkMonitor     *monitor,
                   GsdDevice      *device,
                   FinishCallback  callback,
                   gpointer        user_data,
                   int             threshold_doubleclick,
                   int             threshold_misclick)
{
  CcCalibArea *calib_area;

  g_return_val_if_fail (callback, NULL);

  calib_area = g_object_new (CC_TYPE_CALIB_AREA, NULL);
  calib_area->callback = callback;
  calib_area->user_data = user_data;
  calib_area->device = device;
  calib_area->calibrator.threshold_doubleclick = threshold_doubleclick;
  calib_area->calibrator.threshold_misclick = threshold_misclick;

  /* Move to correct screen */
  if (monitor)
    gtk_window_fullscreen_on_monitor (GTK_WINDOW (calib_area), monitor);
  else
    gtk_window_fullscreen (GTK_WINDOW (calib_area));

  gtk_window_present(GTK_WINDOW (calib_area));

  return calib_area;
}

/* Finishes the calibration. Note that CalibArea
 * needs to be destroyed with Cccalib_area_free() afterwards */
gboolean
cc_calib_area_finish (CcCalibArea *area)
{
  g_return_val_if_fail (area != NULL, FALSE);

  if (area->success)
    g_debug ("Final calibration: %f, %f, %f, %f\n",
             area->axis.x_min,
             area->axis.y_min,
             area->axis.x_max,
             area->axis.y_max);
  else
    g_debug ("Calibration was aborted or timed out");

  return area->success;
}

void
cc_calib_area_free (CcCalibArea *area)
{
  gtk_window_destroy (GTK_WINDOW (area));
}

void
cc_calib_area_get_axis (CcCalibArea *area,
                        XYinfo      *new_axis,
                        gboolean    *swap_xy)
{
  g_return_if_fail (area != NULL);

  *new_axis = area->axis;
  *swap_xy  = area->swap;
}

void
cc_calib_area_get_padding (CcCalibArea *area,
                           XYinfo      *padding)
{
  g_return_if_fail (area != NULL);

  /* min/max values are monitor coordinates scaled to be between
   * 0 and 1, padding starts at 0 on "the edge", and positive
   * values grow towards the center of the rectangle.
   */
  padding->x_min = area->axis.x_min;
  padding->y_min = area->axis.y_min;
  padding->x_max = 1 - area->axis.x_max;
  padding->y_max = 1 - area->axis.y_max;
}
