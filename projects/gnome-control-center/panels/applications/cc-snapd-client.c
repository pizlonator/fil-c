/* cc-snapd-client.c
 *
 * Copyright 2023 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <libsoup/soup.h>

#include "cc-snapd-client.h"

// Unix socket that snapd communicates on.
#define SNAPD_SOCKET_PATH "/var/run/snapd.socket"

struct _CcSnapdClient
{
  GObject parent;

  // HTTP connection to snapd.
  SoupSession *session;
};

G_DEFINE_TYPE (CcSnapdClient, cc_snapd_client, G_TYPE_OBJECT)

// Make an HTTP request to send to snapd.
static SoupMessage *
make_message (const gchar *method, const gchar *path, JsonNode *request_body)
{
  g_autofree gchar *uri = NULL;
  SoupMessage *msg;
  SoupMessageHeaders *request_headers;

  uri = g_strdup_printf("http://locahost%s", path);
  msg = soup_message_new (method, uri);
  request_headers = soup_message_get_request_headers (msg);
  // Allow authentication via polkit.
  soup_message_headers_append (request_headers, "X-Allow-Interaction", "true");
  if (request_body != NULL)
    {
      g_autoptr(JsonGenerator) generator = NULL;
      g_autofree gchar *body_text = NULL;
      gsize body_length;
      g_autoptr(GBytes) body_bytes = NULL;

      generator = json_generator_new ();
      json_generator_set_root (generator, request_body);
      body_text = json_generator_to_data (generator, &body_length);
      body_bytes = g_bytes_new (body_text, body_length);
      soup_message_set_request_body_from_bytes (msg, "application/json", body_bytes);
    }

  return msg;
}

// Process an HTTP response recveived from snapd.
static JsonObject *
process_body (SoupMessage *msg, GBytes *body, GError **error)
{
  const gchar *content_type;
  g_autoptr(JsonParser) parser = NULL;
  const gchar *body_data;
  size_t body_length;
  JsonNode *root;
  JsonObject *response;
  gint64 status_code;
  g_autoptr(GError) internal_error = NULL;

  content_type = soup_message_headers_get_one (soup_message_get_response_headers (msg), "Content-Type");
  if (g_strcmp0 (content_type, "application/json") != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Invalid content type %s returned", content_type);
      return NULL;
    }

  parser = json_parser_new ();
  body_data = g_bytes_get_data (body, &body_length);
  if (!json_parser_load_from_data (parser, body_data, body_length, &internal_error))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Failed to decode JSON content: %s", internal_error->message);
      return NULL;
    }

  root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_OBJECT (root))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Returned JSON not an object");
      return NULL;
    }
  response = json_node_get_object (root);
  status_code = json_object_get_int_member (response, "status-code");
  if (status_code != SOUP_STATUS_OK && status_code != SOUP_STATUS_ACCEPTED)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid status code %" G_GINT64_FORMAT, status_code);
      return NULL;
    }

  return json_object_ref (response);
}

// Send an HTTP request to snapd and process the response.
static JsonObject *
call_sync (CcSnapdClient *self,
           const gchar *method, const gchar *path, JsonNode *request_body,
           GCancellable *cancellable, GError **error)
{
  g_autoptr(SoupMessage) msg = NULL;
  g_autoptr(GBytes) response_body = NULL;

  msg = make_message (method, path, request_body);
  response_body = soup_session_send_and_read (self->session, msg, cancellable, error);
  if (response_body == NULL)
    return NULL;

  return process_body (msg, response_body, error);
}

// Perform a snap interface action.
static gchar *
call_interfaces_sync (CcSnapdClient *self,
                      const gchar *action,
                      const gchar *plug_snap, const gchar *plug_name,
                      const gchar *slot_snap, const gchar *slot_name,
                      GCancellable *cancellable, GError **error)
{
  g_autoptr(JsonBuilder) builder = NULL;
  g_autoptr(JsonObject) response = NULL;
  const gchar *change_id;

  builder = json_builder_new();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, action);
  json_builder_set_member_name (builder, "plugs");
  json_builder_begin_array (builder);
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "snap");
  json_builder_add_string_value (builder, plug_snap);
  json_builder_set_member_name (builder, "plug");
  json_builder_add_string_value (builder, plug_name);
  json_builder_end_object (builder);
  json_builder_end_array (builder);
  json_builder_set_member_name (builder, "slots");
  json_builder_begin_array (builder);
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "snap");
  json_builder_add_string_value (builder, slot_snap);
  json_builder_set_member_name (builder, "slot");
  json_builder_add_string_value (builder, slot_name);
  json_builder_end_object (builder);
  json_builder_end_array (builder);
  json_builder_end_object (builder);

  response = call_sync (self, "POST", "/v2/interfaces",
                        json_builder_get_root (builder), cancellable, error);
  if (response == NULL)
    return NULL;

  change_id = json_object_get_string_member (response, "change");

  return g_strdup (change_id);
}

static void
cc_snapd_client_dispose (GObject *object)
{
  CcSnapdClient *self = CC_SNAPD_CLIENT (object);

  g_clear_object(&self->session);

  G_OBJECT_CLASS (cc_snapd_client_parent_class)->dispose (object);
}

static void
cc_snapd_client_class_init (CcSnapdClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_snapd_client_dispose;
}

static void
cc_snapd_client_init (CcSnapdClient *self)
{
  g_autoptr(GSocketAddress) address = g_unix_socket_address_new (SNAPD_SOCKET_PATH);
  self->session = soup_session_new_with_options ("remote-connectable", address, NULL);
}

CcSnapdClient *
cc_snapd_client_new (void)
{
  return CC_SNAPD_CLIENT (g_object_new (CC_TYPE_SNAPD_CLIENT, NULL));
}

JsonObject *
cc_snapd_client_get_snap_sync (CcSnapdClient *self, const gchar *name, GCancellable *cancellable, GError **error)
{
  g_autofree gchar *path = NULL;
  g_autoptr(JsonObject) response = NULL;
  JsonObject *result;

  path = g_strdup_printf ("/v2/snaps/%s", name);
  response = call_sync (self, "GET", path, NULL, cancellable, error);
  if (response == NULL)
    return NULL;
  result = json_object_get_object_member (response, "result");
  if (result == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid response to %s", path);
      return NULL;
    }

  return json_object_ref (result);
}

JsonObject *
cc_snapd_client_get_change_sync (CcSnapdClient *self, const gchar *change_id, GCancellable *cancellable, GError **error)
{
  g_autofree gchar *path = NULL;
  g_autoptr(JsonObject) response = NULL;
  JsonObject *result;

  path = g_strdup_printf ("/v2/changes/%s", change_id);
  response = call_sync (self, "GET", path, NULL, cancellable, error);
  if (response == NULL)
    return NULL;
  result = json_object_get_object_member (response, "result");
  if (result == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid response to %s", path);
      return NULL;
    }

  return json_object_ref (result);
}

gboolean
cc_snapd_client_get_all_connections_sync (CcSnapdClient *self,
                                          JsonArray **plugs, JsonArray **slots,
                                          GCancellable *cancellable, GError **error)
{
  g_autoptr(JsonObject) response = NULL;
  JsonObject *result;

  response = call_sync (self, "GET", "/v2/connections?select=all", NULL, cancellable, error);
  if (response == NULL)
    return FALSE;
  result = json_object_get_object_member (response, "result");
  if (result == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid response to /v2/connections");
      return FALSE;
    }

  *plugs = json_array_ref (json_object_get_array_member (result, "plugs"));
  *slots = json_array_ref (json_object_get_array_member (result, "slots"));
  return TRUE;
}

gchar *
cc_snapd_client_connect_interface_sync (CcSnapdClient *self,
                                        const gchar *plug_snap, const gchar *plug_name,
                                        const gchar *slot_snap, const gchar *slot_name,
                                        GCancellable *cancellable, GError **error)
{
  return call_interfaces_sync (self, "connect", plug_snap, plug_name, slot_snap, slot_name, cancellable, error);
}

gchar *
cc_snapd_client_disconnect_interface_sync (CcSnapdClient *self,
                                           const gchar *plug_snap, const gchar *plug_name,
                                           const gchar *slot_snap, const gchar *slot_name,
                                           GCancellable *cancellable, GError **error)
{
  return call_interfaces_sync (self, "disconnect", plug_snap, plug_name, slot_snap, slot_name, cancellable, error);
}
