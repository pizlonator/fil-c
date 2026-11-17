/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 *
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHARING_NETWORKS (cc_sharing_networks_get_type ())
G_DECLARE_FINAL_TYPE (CcSharingNetworks, cc_sharing_networks, CC, SHARING_NETWORKS, GtkBox)

typedef enum {
  CC_SHARING_STATUS_UNSET,
  CC_SHARING_STATUS_OFF,
  CC_SHARING_STATUS_ENABLED,
  CC_SHARING_STATUS_ACTIVE
} CcSharingStatus;

GtkWidget    * cc_sharing_networks_new       (GDBusProxy *proxy,
					      const char *service_name);

G_END_DECLS
