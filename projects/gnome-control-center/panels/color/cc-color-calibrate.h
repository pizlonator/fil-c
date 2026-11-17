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

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

G_BEGIN_DECLS

#define CC_TYPE_COLOR_CALIBRATE (cc_color_calibrate_get_type ())
G_DECLARE_FINAL_TYPE (CcColorCalibrate, cc_color_calibrate, CC, COLOR_CALIBRATE, GObject)

CcColorCalibrate *cc_color_calibrate_new    (void);
void      cc_color_calibrate_set_kind       (CcColorCalibrate *calibrate,
                                             CdSensorCap       kind);
void      cc_color_calibrate_set_temperature (CcColorCalibrate *calibrate,
                                             guint             temperature);
void      cc_color_calibrate_set_quality    (CcColorCalibrate *calibrate,
                                             CdProfileQuality  quality);
CdProfileQuality cc_color_calibrate_get_quality (CcColorCalibrate *calibrate);
void      cc_color_calibrate_set_device     (CcColorCalibrate *calibrate,
                                             CdDevice         *device);
void      cc_color_calibrate_set_sensor     (CcColorCalibrate *calibrate,
                                             CdSensor         *sensor);
void      cc_color_calibrate_set_title      (CcColorCalibrate *calibrate,
                                             const gchar      *title);
gboolean  cc_color_calibrate_start          (CcColorCalibrate *calibrate,
                                             GtkWindow        *parent,
                                             GError          **error);
gboolean  cc_color_calibrate_setup          (CcColorCalibrate *calibrate,
                                             GError          **error);
CdProfile *cc_color_calibrate_get_profile   (CcColorCalibrate *calibrate);

G_END_DECLS
