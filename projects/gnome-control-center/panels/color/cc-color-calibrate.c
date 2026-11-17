/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <colord-gtk.h>
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <math.h>
#include <colord-session/cd-session.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <gnome-rr/gnome-rr.h>

#include "cc-color-calibrate.h"

#define CALIBRATE_WINDOW_OPACITY 0.9

struct _CcColorCalibrate
{
  GObject          parent_instance;

  CdDevice        *device;
  CdSensorCap      device_kind;
  CdSensor        *sensor;
  CdProfile       *profile;
  gchar           *title;
  GDBusProxy      *proxy_helper;
  GDBusProxy      *proxy_inhibit;
  GMainLoop       *loop;
  GnomeRROutput   *output;
  GnomeRRScreen   *x11_screen;
  GtkBuilder      *builder;
  GtkWindow       *window;
  GtkWidget       *sample_widget;
  guint            gamma_size;
  CdProfileQuality quality;
  guint            target_whitepoint;   /* in Kelvin */
  gdouble          target_gamma;
  gint             inhibit_fd;
  gint             inhibit_cookie;
  CdSessionError   session_error_code;
};

#define CD_SESSION_ERROR   cc_color_calibrate_error_quark()

#define COLORD_SETTINGS_SCHEMA  "org.freedesktop.ColorHelper"

G_DEFINE_TYPE (CcColorCalibrate, cc_color_calibrate, G_TYPE_OBJECT)

static GQuark
cc_color_calibrate_error_quark (void)
{
  static GQuark quark = 0;
  if (!quark)
    quark = g_quark_from_static_string ("CcColorCalibrateError");
  return quark;
}

void
cc_color_calibrate_set_kind (CcColorCalibrate *calibrate,
                             CdSensorCap kind)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  calibrate->device_kind = kind;
}

void
cc_color_calibrate_set_temperature (CcColorCalibrate *calibrate,
                                    guint temperature)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  g_return_if_fail (temperature < 10000);
  calibrate->target_whitepoint = temperature;
}

void
cc_color_calibrate_set_quality (CcColorCalibrate *calibrate,
                                CdProfileQuality quality)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  calibrate->quality = quality;
}

CdProfileQuality
cc_color_calibrate_get_quality (CcColorCalibrate *calibrate)
{
  g_return_val_if_fail (CC_IS_COLOR_CALIBRATE (calibrate), 0);
  return calibrate->quality;
}

void
cc_color_calibrate_set_device (CcColorCalibrate *calibrate,
                               CdDevice *device)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  g_return_if_fail (CD_IS_DEVICE (device));
  if (calibrate->device != NULL)
    g_object_unref (calibrate->device);
  calibrate->device = g_object_ref (device);
}

void
cc_color_calibrate_set_sensor (CcColorCalibrate *calibrate,
                               CdSensor *sensor)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  g_return_if_fail (CD_IS_SENSOR (sensor));
  if (calibrate->sensor != NULL)
    g_object_unref (calibrate->sensor);
  calibrate->sensor = g_object_ref (sensor);
}

void
cc_color_calibrate_set_title (CcColorCalibrate *calibrate,
                              const gchar *title)
{
  g_return_if_fail (CC_IS_COLOR_CALIBRATE (calibrate));
  g_return_if_fail (title != NULL);
  g_free (calibrate->title);
  calibrate->title = g_strdup (title);
}

CdProfile *
cc_color_calibrate_get_profile (CcColorCalibrate *calibrate)
{
  g_return_val_if_fail (CC_IS_COLOR_CALIBRATE (calibrate), NULL);
  return calibrate->profile;
}

static guint
_gnome_rr_output_get_gamma_size (GnomeRROutput *output)
{
  GnomeRRCrtc *crtc;
  gint len = 0;

  crtc = gnome_rr_output_get_crtc (output);
  if (crtc == NULL)
    return 0;
  gnome_rr_crtc_get_gamma (crtc,
                           &len,
                           NULL, NULL, NULL);
  return (guint) len;
}

static gboolean
cc_color_calibrate_calib_setup_screen (CcColorCalibrate *calibrate,
                                       const gchar *name,
                                       GError **error)
{
  gboolean ret = TRUE;

  /* get screen */
  calibrate->x11_screen = gnome_rr_screen_new (gdk_display_get_default (), error);
  if (calibrate->x11_screen == NULL)
    {
      ret = FALSE;
      goto out;
    }

  /* get the output */
  calibrate->output = gnome_rr_screen_get_output_by_name (calibrate->x11_screen,
                                                     name);
  if (calibrate->output == NULL)
    {
      ret = FALSE;
      g_set_error_literal (error,
                           CD_SESSION_ERROR,
                           CD_SESSION_ERROR_INTERNAL,
                           "failed to get output");
      goto out;
    }

  /* create a lookup table */
  calibrate->gamma_size = _gnome_rr_output_get_gamma_size (calibrate->output);
  if (calibrate->gamma_size == 0)
    {
      ret = FALSE;
      g_set_error_literal (error,
                           CD_SESSION_ERROR,
                           CD_SESSION_ERROR_INTERNAL,
                           "gamma size is zero");
    }
out:
  return ret;
}

/**
 * cc_color_calibrate_calib_set_output_gamma:
 *
 * Handle this here rather than in gnome-settings-daemon for two reasons:
 *
 *  - We don't want to create a profile each time the video card gamma
 *    table is created, as that would mean ~15 DBus requests each time
 *    we get UpdateGamma from the session helper.
 *
 *  - We only have 100ms to process the request before the next update
 *    could be scheduled.
 **/
static gboolean
cc_color_calibrate_calib_set_output_gamma (CcColorCalibrate *calibrate,
                                           GPtrArray *array,
                                           GError **error)
{
  CdColorRGB *p1;
  CdColorRGB *p2;
  CdColorRGB result;
  gdouble mix;
  GnomeRRCrtc *crtc;
  g_autofree guint16 *blue = NULL;
  g_autofree guint16 *green = NULL;
  g_autofree guint16 *red = NULL;
  guint i;

  /* no length? */
  if (array->len == 0)
    {
      g_set_error_literal (error,
                           CD_SESSION_ERROR,
                           CD_SESSION_ERROR_INTERNAL,
                           "no data in the CLUT array");
      return FALSE;
    }

  /* convert to a type X understands of the right size */
  red = g_new (guint16, calibrate->gamma_size);
  green = g_new (guint16, calibrate->gamma_size);
  blue = g_new (guint16, calibrate->gamma_size);
  cd_color_rgb_set (&result, 1.0, 1.0, 1.0);
  for (i = 0; i < calibrate->gamma_size; i++)
    {
      mix = (gdouble) (array->len - 1) /
            (gdouble) (calibrate->gamma_size - 1) *
            (gdouble) i;
      p1 = g_ptr_array_index (array, (guint) floor (mix));
      p2 = g_ptr_array_index (array, (guint) ceil (mix));
      cd_color_rgb_interpolate (p1,
                                p2,
                                mix - (gint) mix,
                                &result);
      red[i] = result.R * 0xffff;
      green[i] = result.G * 0xffff;
      blue[i] = result.B * 0xffff;
    }

  /* send to LUT */
  crtc = gnome_rr_output_get_crtc (calibrate->output);
  if (crtc == NULL)
    {
      g_set_error (error,
                   CD_SESSION_ERROR,
                   CD_SESSION_ERROR_INTERNAL,
                   "failed to get ctrc for %s",
                   gnome_rr_output_get_name (calibrate->output));
      return FALSE;
    }
  gnome_rr_crtc_set_gamma (crtc, calibrate->gamma_size,
                           red, green, blue);
  return TRUE;
}

static void
cc_color_calibrate_property_changed_cb (CcColorCalibrate *calibrate,
                                        GVariant *changed_properties,
                                        GStrv invalidated_properties)
{
  gboolean ret;
  GtkWidget *widget;
  guint value;

  ret = g_variant_lookup (changed_properties,
                          "Progress",
                          "u", &value);
  if (ret)
    {
      widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                                   "progressbar_status"));
      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (widget),
                                     value / 100.0f);
    }
}

static void
cc_color_calibrate_interaction_required (CcColorCalibrate *calibrate,
                                         CdSessionInteraction code,
                                         const gchar *message,
                                         const gchar *image_path)
{
  const gchar *message_transl;
  gboolean show_button_start = FALSE;
  GtkImage *img;
  GtkLabel *label;
  GtkWidget *widget;

  /* the client helper does not ship an icon for this */
  if (code == CD_SESSION_INTERACTION_SHUT_LAPTOP_LID)
    image_path = "preferences-color-symbolic";

  /* set image */
  img = GTK_IMAGE (gtk_builder_get_object (calibrate->builder,
                                           "image_status"));
  if (image_path != NULL && image_path[0] != '\0')
    {
      g_autoptr(GdkPixbuf) pixbuf = NULL;

      g_debug ("showing image %s", image_path);
      pixbuf = gdk_pixbuf_new_from_file_at_size (image_path,
                                                 400, 400,
                                                 NULL);
      if (pixbuf != NULL)
        gtk_image_set_from_pixbuf (img, pixbuf);
      gtk_widget_set_visible (GTK_WIDGET (img), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (calibrate->sample_widget), FALSE);
    }
  else
    {
      g_debug ("hiding image");
      gtk_widget_set_visible (GTK_WIDGET (img), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (calibrate->sample_widget), TRUE);
    }

  /* set new status */
  switch (code)
    {
    case CD_SESSION_INTERACTION_ATTACH_TO_SCREEN:
      show_button_start = TRUE;
      /* TRANSLATORS: The user has to attach the sensor to the screen */
      message_transl = _("Place your calibration device over the square and press “Start”");
      break;
    case CD_SESSION_INTERACTION_MOVE_TO_CALIBRATION:
      /* TRANSLATORS: Some calibration devices need the user to move a
       * dial or switch manually. We also show a picture showing them
       * what to do... */
      message_transl = _("Move your calibration device to the calibrate position and press “Continue”");
      break;
    case CD_SESSION_INTERACTION_MOVE_TO_SURFACE:
      /* TRANSLATORS: Some calibration devices need the user to move a
       * dial or switch manually. We also show a picture showing them
       * what to do... */
      message_transl = _("Move your calibration device to the surface position and press “Continue”");
      break;
    case CD_SESSION_INTERACTION_SHUT_LAPTOP_LID:
      /* TRANSLATORS: on some hardware e.g. Lenovo W700 the sensor
       * is built into the palmrest and we need to fullscreen the
       * sample widget and shut the lid. */
      message_transl = _("Shut the laptop lid");
      break;
    default:
      message_transl = message;
      break;
    }
  label = GTK_LABEL (gtk_builder_get_object (calibrate->builder,
                                             "label_status"));
  gtk_label_set_label (label, message_transl);

  /* show the correct button */
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_start"));
  gtk_widget_set_visible (widget, show_button_start);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_resume"));
  gtk_widget_set_visible (widget, !show_button_start);
}

static const gchar *
cc_color_calibrate_get_error_translation (CdSessionError code)
{
  const gchar *str = NULL;
  switch (code)
    {
      case CD_SESSION_ERROR_FAILED_TO_FIND_DEVICE:
      case CD_SESSION_ERROR_FAILED_TO_FIND_SENSOR:
      case CD_SESSION_ERROR_INTERNAL:
      case CD_SESSION_ERROR_INVALID_VALUE:
        /* TRANSLATORS: We suck, the calibration failed and we have no
         * good idea why or any suggestions */
        str = _("An internal error occurred that could not be recovered.");
        break;
      case CD_SESSION_ERROR_FAILED_TO_FIND_TOOL:
        /* TRANSLATORS: Some required-at-runtime tools were not
         * installed, which should only affect insane distros */
        str = _("Tools required for calibration are not installed.");
        break;
      case CD_SESSION_ERROR_FAILED_TO_GENERATE_PROFILE:
      case CD_SESSION_ERROR_FAILED_TO_OPEN_PROFILE:
      case CD_SESSION_ERROR_FAILED_TO_SAVE_PROFILE:
        /* TRANSLATORS: The profile failed for some reason */
        str = _("The profile could not be generated.");
        break;
      case CD_SESSION_ERROR_FAILED_TO_GET_WHITEPOINT:
        /* TRANSLATORS: The user specified a whitepoint that was
         * unobtainable with the hardware they've got -- see
         * https://en.wikipedia.org/wiki/White_point for details */
        str = _("The target whitepoint was not obtainable.");
        break;
      default:
        break;
    }
  return str;
}

static void
cc_color_calibrate_finished (CcColorCalibrate *calibrate,
                             CdSessionError code,
                             const gchar *error_fallback)
{
  GtkWidget *widget;
  g_autoptr(GString) str = NULL;
  const gchar *tmp;

  /* save failure so we can get this after we've quit the loop */
  calibrate->session_error_code = code;

  /* show correct buttons */
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_cancel"));
  gtk_widget_set_visible (widget, FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_start"));
  gtk_widget_set_visible (widget, FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_resume"));
  gtk_widget_set_visible (widget, FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_done"));
  gtk_widget_set_visible (widget, TRUE);

  str = g_string_new ("");
  if (code == CD_SESSION_ERROR_NONE)
    {
      g_debug ("calibration succeeded");
      /* TRANSLATORS: the display calibration process is finished */
      g_string_append (str, _("Complete!"));
    }
  else
    {
      g_warning ("calibration failed with code %i: %s",
                 code, error_fallback);
      /* TRANSLATORS: the display calibration failed, and we also show
       * the translated (or untranslated) error string after this */
      g_string_append (str, _("Calibration failed!"));
      g_string_append (str, "\n\n");
      tmp = cc_color_calibrate_get_error_translation (code);
      g_string_append (str, tmp != NULL ? tmp : error_fallback);
    }
  g_string_append (str, "\n");
  /* TRANSLATORS: The user can now remove the sensor from the screen */
  g_string_append (str, _("You can remove the calibration device."));

  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "label_status"));
  gtk_label_set_label (GTK_LABEL (widget), str->str);
}

static void
cc_color_calibrate_signal_cb (CcColorCalibrate *calibrate,
                              const gchar *sender_name,
                              const gchar *signal_name,
                              GVariant *parameters)
{
  CdColorRGB color;
  CdColorRGB *color_tmp;
  const gchar *image = NULL;
  const gchar *message;
  const gchar *profile_path = NULL;
  const gchar *str = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GPtrArray *array = NULL;
  GtkImage *img;
  GtkLabel *label;
  g_autoptr(GVariant) dict = NULL;

  if (g_strcmp0 (signal_name, "Finished") == 0)
    {
      CdSessionError error_code;

      g_variant_get (parameters, "(u@a{sv})",
                     &error_code,
                     &dict);
      g_variant_lookup (dict, "ErrorDetails", "&s", &str);
      ret = g_variant_lookup (dict, "ProfilePath", "&s", &profile_path);
      if (ret)
        calibrate->profile = cd_profile_new_with_object_path (profile_path);
      cc_color_calibrate_finished (calibrate, error_code, str);
      return;
    }
  if (g_strcmp0 (signal_name, "UpdateSample") == 0)
    {
      g_variant_get (parameters, "(ddd)",
                     &color.R,
                     &color.G,
                     &color.B);
      img = GTK_IMAGE (gtk_builder_get_object (calibrate->builder,
                                               "image_status"));
      gtk_widget_set_visible (GTK_WIDGET (img), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (calibrate->sample_widget), TRUE);
      cd_sample_widget_set_color (CD_SAMPLE_WIDGET (calibrate->sample_widget),
                                  &color);

      /* for Lenovo W700 and W520 laptops we almost fullscreen the
       * sample widget as the device is actually embedded in the
       * palmrest! */
      if (cd_sensor_get_embedded (calibrate->sensor))
        {
          g_debug ("Making sample window larger for embedded sensor");
          gtk_widget_set_size_request (calibrate->sample_widget, 1000, 600);
        }

      /* set the generic label too */
      label = GTK_LABEL (gtk_builder_get_object (calibrate->builder,
                                                 "label_status"));
      /* TRANSLATORS: The user has to be careful not to knock the
       * display off the screen (although we do cope if this is
       * detected early enough) */
      gtk_label_set_label (label, _("Do not disturb the calibration device while in progress"));
      return;
    }
  if (g_strcmp0 (signal_name, "InteractionRequired") == 0)
    {
      CdSessionInteraction code;

      g_variant_get (parameters, "(u&s&s)",
                     &code,
                     &message,
                     &image);
      g_debug ("Interaction required type %i: %s",
               code, message);
      cc_color_calibrate_interaction_required (calibrate,
                                               code,
                                               message,
                                               image);
      return;
    }
  if (g_strcmp0 (signal_name, "UpdateGamma") == 0)
    {
      g_autoptr(GVariantIter) iter = NULL;

      g_variant_get (parameters,
                     "(a(ddd))",
                     &iter);
      array = g_ptr_array_new_with_free_func (g_free);
      while (g_variant_iter_loop (iter, "(ddd)",
                                  &color.R,
                                  &color.G,
                                  &color.B))
        {
          color_tmp = cd_color_rgb_new ();
          cd_color_rgb_copy (&color, color_tmp);
          g_ptr_array_add (array, color_tmp);
        }
      ret = cc_color_calibrate_calib_set_output_gamma (calibrate,
                                                       array,
                                                       &error);
      if (!ret)
        {
          g_warning ("failed to update gamma: %s",
                     error->message);
          return;
        }
      return;
    }
  g_warning ("got unknown signal %s", signal_name);
}

static void
cc_color_calibrate_cancel (CcColorCalibrate *calibrate)
{
  g_autoptr(GVariant) retval = NULL;
  g_autoptr(GError) error = NULL;

  /* cancel the calibration to ensure the helper quits */
  retval = g_dbus_proxy_call_sync (calibrate->proxy_helper,
                                   "Cancel",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (retval == NULL)
    g_warning ("Failed to send Cancel: %s", error->message);

  /* return */
  g_main_loop_quit (calibrate->loop);
}

static void
cc_color_calibrate_button_done_cb (CcColorCalibrate *calibrate)
{
  g_main_loop_quit (calibrate->loop);
}

static void
cc_color_calibrate_button_start_cb (CcColorCalibrate *calibrate)
{
  GtkWidget *widget;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) retval = NULL;

  /* set correct buttons */
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_start"));
  gtk_widget_set_visible (widget, FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_resume"));
  gtk_widget_set_visible (widget, FALSE);

  /* continue */
  retval = g_dbus_proxy_call_sync (calibrate->proxy_helper,
                                   "Resume",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  if (retval == NULL)
    g_warning ("Failed to send Resume: %s", error->message);
}

static void
cc_color_calibrate_button_cancel_cb (CcColorCalibrate *calibrate)
{
  cc_color_calibrate_cancel (calibrate);
}

#if 0
static gboolean
cc_color_calibrate_alpha_window_draw (CcColorCalibrate *calibrate, cairo_t *cr)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "dialog_calibrate"));

  if (gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget)) &&
      gdk_screen_is_composited (gtk_widget_get_screen (widget)))
    {
      /* transparent */
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, CALIBRATE_WINDOW_OPACITY);
    }
  else
    {
      /* opaque black */
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
    }
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);
  return FALSE;
}

static void
cc_color_calibrate_alpha_screen_changed_cb (CcColorCalibrate *calibrate)
{
  GtkWidget *window;
  GdkScreen *screen;
  GdkVisual *visual;

  window = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "dialog_calibrate"));
  screen = gtk_widget_get_screen (GTK_WIDGET (window));
  visual = gdk_screen_get_rgba_visual (screen);
  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);
  gtk_widget_set_visual (GTK_WIDGET (window), visual);
}
#endif

static void
cc_color_calibrate_uninhibit (CcColorCalibrate *calibrate)
{
  GtkApplication *application;

  if (calibrate->inhibit_fd != -1)
    {
      close (calibrate->inhibit_fd);
      calibrate->inhibit_fd = -1;
    }

  if (calibrate->inhibit_cookie != 0)
    {
      application = GTK_APPLICATION (g_application_get_default ());
      gtk_application_uninhibit (application, calibrate->inhibit_cookie);
      calibrate->inhibit_cookie = 0;
    }
}

static void
cc_color_calibrate_inhibit (CcColorCalibrate *calibrate, GtkWindow *window)
{
  g_autoptr(GError) error = NULL;
  gint idx;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GVariant) retval = NULL;
  GtkApplication *application;

  /* inhibit basically everything we can */
  application = GTK_APPLICATION (g_application_get_default ());
  calibrate->inhibit_cookie = gtk_application_inhibit (application,
                                                  window,
                                                  GTK_APPLICATION_INHIBIT_LOGOUT |
                                                  GTK_APPLICATION_INHIBIT_SWITCH |
                                                  GTK_APPLICATION_INHIBIT_SUSPEND |
                                                  GTK_APPLICATION_INHIBIT_IDLE,
                                                  "Display calibration in progress");

  /* tell logind to disallow the lid switch */
  retval = g_dbus_proxy_call_with_unix_fd_list_sync (calibrate->proxy_inhibit,
                                                     "Inhibit",
                                                     g_variant_new ("(ssss)",
                                                                    "shutdown:"
                                                                    "sleep:"
                                                                    "idle:"
                                                                    "handle-lid-switch",
                                                                    "Display Calibrator",
                                                                    "Display calibration in progress",
                                                                    "block"),
                                                     G_DBUS_CALL_FLAGS_NONE,
                                                     -1,
                                                     NULL,
                                                     &fd_list,
                                                     NULL,
                                                     &error);
  if (retval == NULL)
    {
      g_warning ("Failed to send Inhibit: %s", error->message);
      return;
    }
  g_variant_get (retval, "(h)", &idx);
  calibrate->inhibit_fd = g_unix_fd_list_get (fd_list, idx, &error);
  if (calibrate->inhibit_fd == -1)
    {
      g_warning ("Failed to receive system inhibitor fd: %s", error->message);
      return;
    }
  g_debug ("System inhibitor fd is %d", calibrate->inhibit_fd);
}

gboolean
cc_color_calibrate_setup (CcColorCalibrate *calibrate,
                          GError **error)
{
  gboolean ret = TRUE;

  g_return_val_if_fail (CC_IS_COLOR_CALIBRATE (calibrate), FALSE);
  g_return_val_if_fail (calibrate->device_kind != CD_SENSOR_CAP_UNKNOWN, FALSE);

  /* use logind to disable system state idle */
  calibrate->proxy_inhibit = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                            G_DBUS_PROXY_FLAGS_NONE,
                                                            NULL,
                                                            "org.freedesktop.login1",
                                                            "/org/freedesktop/login1",
                                                            "org.freedesktop.login1.Manager",
                                                            NULL,
                                                            error);
  if (calibrate->proxy_inhibit == NULL)
    {
      ret = FALSE;
      goto out;
    }

  /* start the calibration session daemon */
  calibrate->proxy_helper = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           NULL,
                                                           CD_SESSION_DBUS_SERVICE,
                                                           CD_SESSION_DBUS_PATH,
                                                           CD_SESSION_DBUS_INTERFACE_DISPLAY,
                                                           NULL,
                                                           error);
  if (calibrate->proxy_helper == NULL)
    {
      ret = FALSE;
      goto out;
    }
  g_signal_connect_object (calibrate->proxy_helper,
                           "g-properties-changed",
                           G_CALLBACK (cc_color_calibrate_property_changed_cb),
                           calibrate, G_CONNECT_SWAPPED);
  g_signal_connect_object (calibrate->proxy_helper,
                           "g-signal",
                           G_CALLBACK (cc_color_calibrate_signal_cb),
                           calibrate, G_CONNECT_SWAPPED);
out:
  return ret;
}

static GdkMonitor *
find_device_monitor (CcColorCalibrate *calibrate)
{
  GdkDisplay *display;
  GListModel *monitors;
  const char *xrandr_name;
  int num_monitors;
  int i;

  xrandr_name = cd_device_get_metadata_item (calibrate->device,
                                             CD_DEVICE_METADATA_XRANDR_NAME);

  display = gdk_display_get_default ();
  monitors = gdk_display_get_monitors (display);
  num_monitors = g_list_model_get_n_items (monitors);
  for (i = 0; i < num_monitors; i++)
    {
      g_autoptr(GdkMonitor) monitor = NULL;
      const char *connector;

      monitor = g_list_model_get_item (monitors, i);
      connector = gdk_monitor_get_connector (monitor);
      if (g_strcmp0 (connector, xrandr_name) == 0)
        return g_steal_pointer (&monitor);
    }

  return NULL;
}

gboolean
cc_color_calibrate_start (CcColorCalibrate *calibrate,
                          GtkWindow *parent,
                          GError **error)
{
  const gchar *name;
  GtkWidget *widget;
  GtkWindow *window;
  GVariantBuilder builder;
  g_autoptr(GVariant) retval = NULL;
  g_autoptr(GdkMonitor) monitor = NULL;

  g_return_val_if_fail (CC_IS_COLOR_CALIBRATE (calibrate), FALSE);

  /* get screen */
  name = cd_device_get_metadata_item (calibrate->device,
                                      CD_DEVICE_METADATA_XRANDR_NAME);
  if (!cc_color_calibrate_calib_setup_screen (calibrate, name, error))
    return FALSE;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (&builder,
                         "{sv}",
                         "Quality",
                         g_variant_new_uint32 (calibrate->quality));
  g_variant_builder_add (&builder,
                         "{sv}",
                         "Whitepoint",
                         g_variant_new_uint32 (calibrate->target_whitepoint));
  g_variant_builder_add (&builder,
                         "{sv}",
                         "Gamma",
                         g_variant_new_double (calibrate->target_gamma));
  g_variant_builder_add (&builder,
                         "{sv}",
                         "Title",
                         g_variant_new_string (calibrate->title));
  g_variant_builder_add (&builder,
                         "{sv}",
                         "DeviceKind",
                         g_variant_new_uint32 (calibrate->device_kind));
  retval = g_dbus_proxy_call_sync (calibrate->proxy_helper,
                                   "Start",
                                   g_variant_new ("(ssa{sv})",
                                                  cd_device_get_id (calibrate->device),
                                                  cd_sensor_get_id (calibrate->sensor),
                                                  &builder),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   error);
  if (retval == NULL)
    return FALSE;

  window = GTK_WINDOW (gtk_builder_get_object (calibrate->builder,
                                               "dialog_calibrate"));
  gtk_window_set_modal (window, TRUE);
  gtk_window_set_transient_for (window, parent);
  monitor = find_device_monitor (calibrate);
  if (!monitor)
    g_warning ("Failed to find monitor to place calibration window on");
  else
    gtk_window_fullscreen_on_monitor (window, monitor);
  gtk_window_present (window);

  /* show correct buttons */
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_cancel"));
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_start"));
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_resume"));
  gtk_widget_set_visible (widget, FALSE);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_done"));
  gtk_widget_set_visible (widget, FALSE);

  /* stop the computer from auto-suspending or turning off the screen */
  cc_color_calibrate_inhibit (calibrate, parent);

  g_main_loop_run (calibrate->loop);
  gtk_window_close (window);

  /* we can go idle now */
  cc_color_calibrate_uninhibit (calibrate);

  /* see if we failed */
  if (calibrate->session_error_code != CD_SESSION_ERROR_NONE)
    {
      g_set_error_literal (error,
                           CD_SESSION_ERROR,
                           CD_SESSION_ERROR_INTERNAL,
                           "failed to calibrate");
      return FALSE;
    }

  return TRUE;
}

static void
cc_color_calibrate_finalize (GObject *object)
{
  CcColorCalibrate *calibrate = CC_COLOR_CALIBRATE (object);

  g_clear_pointer (&calibrate->window, gtk_window_destroy);
  g_clear_object (&calibrate->builder);
  g_clear_object (&calibrate->device);
  g_clear_object (&calibrate->proxy_helper);
  g_clear_object (&calibrate->proxy_inhibit);
  g_clear_object (&calibrate->sensor);
  g_clear_object (&calibrate->x11_screen);
  g_free (calibrate->title);
  g_main_loop_unref (calibrate->loop);

  G_OBJECT_CLASS (cc_color_calibrate_parent_class)->finalize (object);
}

static void
cc_color_calibrate_class_init (CcColorCalibrateClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = cc_color_calibrate_finalize;
}

static void
cc_color_calibrate_init (CcColorCalibrate *calibrate)
{
  g_autoptr(GError) error = NULL;
  gint retval;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkBox *box;
  GtkWidget *widget;
  GtkWindow *window;

  calibrate->loop = g_main_loop_new (NULL, FALSE);
  calibrate->inhibit_fd = -1;

  /* load UI */
  calibrate->builder = gtk_builder_new ();
  retval = gtk_builder_add_from_resource (calibrate->builder,
                                          "/org/gnome/control-center/color/cc-color-calibrate.ui",
                                          &error);
  if (retval == 0)
    g_warning ("Could not load interface: %s", error->message);

  /* load stylesheet */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/color/color-calibrate.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* add sample widget */
  box = GTK_BOX (gtk_builder_get_object (calibrate->builder,
                 "vbox_status"));
  calibrate->sample_widget = cd_sample_widget_new ();
  gtk_widget_set_size_request (calibrate->sample_widget, 400, 400);
  gtk_box_prepend (box, calibrate->sample_widget);
  gtk_widget_set_vexpand (calibrate->sample_widget, FALSE);
  gtk_widget_set_hexpand (calibrate->sample_widget, FALSE);

  /* get defaults */
  settings = g_settings_new (COLORD_SETTINGS_SCHEMA);
  calibrate->target_whitepoint = g_settings_get_int (settings, "display-whitepoint");
  calibrate->target_gamma = g_settings_get_double (settings, "display-gamma");

  /* connect to buttons */
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_start"));
  g_signal_connect_object (widget, "clicked",
                           G_CALLBACK (cc_color_calibrate_button_start_cb), calibrate, G_CONNECT_SWAPPED);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_resume"));
  g_signal_connect_object (widget, "clicked",
                           G_CALLBACK (cc_color_calibrate_button_start_cb), calibrate, G_CONNECT_SWAPPED);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_done"));
  g_signal_connect_object (widget, "clicked",
                           G_CALLBACK (cc_color_calibrate_button_done_cb), calibrate, G_CONNECT_SWAPPED);
  widget = GTK_WIDGET (gtk_builder_get_object (calibrate->builder,
                                               "button_cancel"));
  g_signal_connect_object (widget, "clicked",
                           G_CALLBACK (cc_color_calibrate_button_cancel_cb), calibrate, G_CONNECT_SWAPPED);
  gtk_widget_set_visible (widget, TRUE);

  /* setup the specialist calibration window */
  window = GTK_WINDOW (gtk_builder_get_object (calibrate->builder,
                                               "dialog_calibrate"));
  calibrate->window = window;
}

CcColorCalibrate *
cc_color_calibrate_new (void)
{
  CcColorCalibrate *calibrate;
  calibrate = g_object_new (CC_TYPE_COLOR_CALIBRATE, NULL);
  return CC_COLOR_CALIBRATE (calibrate);
}
