/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PP_TYPE_PRINT_DEVICE (pp_print_device_get_type ())
G_DECLARE_FINAL_TYPE (PpPrintDevice, pp_print_device, PP, PRINT_DEVICE, GObject)

PpPrintDevice *pp_print_device_new                       (void);
PpPrintDevice *pp_print_device_copy                      (PpPrintDevice *device);
gchar         *pp_print_device_get_device_name           (PpPrintDevice *device);
gchar         *pp_print_device_get_display_name          (PpPrintDevice *device);
gchar         *pp_print_device_get_device_original_name  (PpPrintDevice *device);
gchar         *pp_print_device_get_device_make_and_model (PpPrintDevice *device);
gchar         *pp_print_device_get_device_location       (PpPrintDevice *device);
gchar         *pp_print_device_get_device_info           (PpPrintDevice *device);
gchar         *pp_print_device_get_device_uri            (PpPrintDevice *device);
gchar         *pp_print_device_get_device_id             (PpPrintDevice *device);
gchar         *pp_print_device_get_device_ppd            (PpPrintDevice *device);
gchar         *pp_print_device_get_host_name             (PpPrintDevice *device);
gint           pp_print_device_get_host_port             (PpPrintDevice *device);
gboolean       pp_print_device_is_authenticated_server   (PpPrintDevice *device);
gint           pp_print_device_get_acquisition_method    (PpPrintDevice *device);
gboolean       pp_print_device_is_network_device         (PpPrintDevice *device);

G_END_DECLS
