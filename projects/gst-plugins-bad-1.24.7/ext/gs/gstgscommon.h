/* GStreamer
 * Copyright (C) 2020 Julien Isorce <jisorce@oblong.com>
 *
 * gstgscommon.h:
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

#ifndef __GST_GS_COMMON_H__
#define __GST_GS_COMMON_H__

#include <memory>

#include <gst/gst.h>

#include <google/cloud/storage/client.h>

std::unique_ptr<google::cloud::storage::Client> gst_gs_create_client(
    const gchar* service_account_email,
    const gchar* service_account_credentials,
    GError** error);

gboolean gst_gs_get_buffer_date(GstBuffer* buffer,
                                GDateTime* start_date,
                                gchar** buffer_date_str_ptr);

#endif  // __GST_GS_COMMON_H__
