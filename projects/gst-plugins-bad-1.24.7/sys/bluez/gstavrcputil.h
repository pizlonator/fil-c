/*
 * Copyright (C) 2015 Arun Raghavan <git@arunraghavan.net>
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __GST_AVRCP_UTIL_H
#define __GST_AVRCP_UTIL_H

#include <gst/gst.h>

typedef struct _GstAvrcpConnection GstAvrcpConnection;

typedef void (*GstAvrcpMetadataCb) (GstAvrcpConnection *, GstTagList *,
    gpointer);

GstAvrcpConnection *
gst_avrcp_connection_new (const gchar * dev_path, GstAvrcpMetadataCb cb,
    gpointer user_data, GDestroyNotify user_data_free_cb);

void gst_avrcp_connection_free (GstAvrcpConnection * avrcp);

#endif /* __GST_AVRCP_UTIL_H */
