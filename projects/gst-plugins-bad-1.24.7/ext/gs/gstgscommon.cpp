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

#include "gstgscommon.h"

#include "google/cloud/storage/oauth2/compute_engine_credentials.h"

namespace gcs = google::cloud::storage;

std::unique_ptr<google::cloud::storage::Client> gst_gs_create_client(
    const gchar* service_account_email,
    const gchar* service_account_credentials,
    GError** error) {
  if (service_account_email || service_account_credentials) {
    google::cloud::StatusOr<std::shared_ptr<gcs::oauth2::Credentials>> creds;
    if (service_account_credentials) {
      creds = gcs::oauth2::CreateServiceAccountCredentialsFromJsonContents(
          service_account_credentials,
          {{"https://www.googleapis.com/auth/devstorage.full_control"}},
          absl::nullopt);
    } else {
      // Meant to be used from a container running in the Cloud.
      creds =
          gcs::oauth2::CreateComputeEngineCredentials(service_account_email);
    }

    if (!creds) {
      if (service_account_email) {
        g_set_error(error, GST_RESOURCE_ERROR,
                    GST_RESOURCE_ERROR_NOT_AUTHORIZED,
                    "Could not retrieve credentials for the given service "
                    "account %s (%s)",
                    service_account_email, creds.status().message().c_str());
      } else {
        g_set_error(error, GST_RESOURCE_ERROR,
                    GST_RESOURCE_ERROR_NOT_AUTHORIZED,
                    "Could not retrieve credentials for the given service "
                    "account credentials JSON (%s)",
                    creds.status().message().c_str());
      }
      return nullptr;
    }

    gcs::ClientOptions client_options(std::move(creds.value()));
    return std::make_unique<gcs::Client>(client_options,
                                         gcs::StrictIdempotencyPolicy());
  }
  // Default account. This is meant to retrieve the credentials automatically
  // using diffrent methods.
  google::cloud::StatusOr<gcs::ClientOptions> client_options =
      gcs::ClientOptions::CreateDefaultClientOptions();

  if (!client_options) {
    g_set_error(error, GST_RESOURCE_ERROR, GST_RESOURCE_ERROR_NOT_AUTHORIZED,
                "Could not create default client options (%s)",
                client_options.status().message().c_str());
    return nullptr;
  }
  return std::make_unique<gcs::Client>(client_options.value(),
                                       gcs::StrictIdempotencyPolicy());
}

gboolean gst_gs_get_buffer_date(GstBuffer* buffer,
                                GDateTime* start_date,
                                gchar** buffer_date_str_ptr) {
  gchar* buffer_date_str = NULL;
  GstClockTime buffer_timestamp = GST_CLOCK_TIME_NONE;
  GTimeSpan buffer_timespan = 0;

  if (!buffer || !start_date)
    return FALSE;

  buffer_timestamp = GST_BUFFER_PTS(buffer);

  // GTimeSpan is in micro seconds.
  buffer_timespan = GST_TIME_AS_USECONDS(buffer_timestamp);

  GDateTime* buffer_date = g_date_time_add(start_date, buffer_timespan);
  if (!buffer_date)
    return FALSE;

  buffer_date_str = g_date_time_format_iso8601(buffer_date);
  g_date_time_unref(buffer_date);

  if (!buffer_date_str)
    return FALSE;

  if (buffer_date_str_ptr)
    *buffer_date_str_ptr = buffer_date_str;

  return TRUE;
}
