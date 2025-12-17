/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Collabora, Ltd
 * Copyright 2018 Igalia S.L.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include "gtlsfiledatabase-gnutls.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "gtlscertificate-gnutls.h"

enum
{
  PROP_0,
  PROP_ANCHORS,
};

struct _GTlsFileDatabaseGnutls
{
  GTlsDatabaseGnutls parent_instance;

  /* read-only after construct */
  gchar *anchor_filename;
};

static void g_tls_file_database_gnutls_file_database_interface_init (GTlsFileDatabaseInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsFileDatabaseGnutls, g_tls_file_database_gnutls, G_TYPE_TLS_DATABASE_GNUTLS,
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_FILE_DATABASE,
                                                      g_tls_file_database_gnutls_file_database_interface_init);
                              );

static void
g_tls_file_database_gnutls_finalize (GObject *object)
{
  GTlsFileDatabaseGnutls *self = G_TLS_FILE_DATABASE_GNUTLS (object);

  g_clear_pointer (&self->anchor_filename, g_free);

  G_OBJECT_CLASS (g_tls_file_database_gnutls_parent_class)->finalize (object);
}

static void
g_tls_file_database_gnutls_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GTlsFileDatabaseGnutls *self = G_TLS_FILE_DATABASE_GNUTLS (object);

  switch (prop_id)
    {
    case PROP_ANCHORS:
      g_value_set_string (value, self->anchor_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_file_database_gnutls_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GTlsFileDatabaseGnutls *self = G_TLS_FILE_DATABASE_GNUTLS (object);
  const char *anchor_path;

  switch (prop_id)
    {
    case PROP_ANCHORS:
      anchor_path = g_value_get_string (value);
      if (anchor_path && !g_path_is_absolute (anchor_path))
        {
          g_warning ("The anchor file name used with a GTlsFileDatabase "
                     "must be an absolute path, and not relative: %s", anchor_path);
          return;
        }

      g_free (self->anchor_filename);
      self->anchor_filename = g_strdup (anchor_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gchar *
g_tls_file_database_gnutls_create_handle_for_certificate (GTlsDatabaseGnutls *self,
                                                          GBytes             *der)
{
  gchar *bookmark;
  gchar *uri_part;
  gchar *uri;

  /*
   * Here we create a URI that looks like
   * file:///etc/ssl/certs/ca-certificates.crt#11b2641821252596420e468c275771f5e51022c121a17bd7a89a2f37b6336c8f
   */

  uri_part = g_filename_to_uri (G_TLS_FILE_DATABASE_GNUTLS (self)->anchor_filename,
                                NULL, NULL);
  if (!uri_part)
    return NULL;

  bookmark = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, der);
  uri = g_strconcat (uri_part, "#", bookmark, NULL);

  g_free (bookmark);
  g_free (uri_part);

  return uri;
}

static gboolean
g_tls_file_database_gnutls_populate_trust_list (GTlsDatabaseGnutls        *self,
                                                gnutls_x509_trust_list_t   trust_list,
                                                GError                   **error)
{
  int ret = gnutls_x509_trust_list_add_trust_file (trust_list,
                                                   G_TLS_FILE_DATABASE_GNUTLS (self)->anchor_filename,
                                                   NULL, GNUTLS_X509_FMT_PEM, 0, 0);

  if (ret < 0)
    {
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Failed to populate trust list from %s: %s"),
                   G_TLS_FILE_DATABASE_GNUTLS (self)->anchor_filename, gnutls_strerror (ret));
      return FALSE;
    }

  return TRUE;
}

static void
g_tls_file_database_gnutls_init (GTlsFileDatabaseGnutls *self)
{
}

static void
g_tls_file_database_gnutls_class_init (GTlsFileDatabaseGnutlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsDatabaseGnutlsClass *gnutls_database_class = G_TLS_DATABASE_GNUTLS_CLASS (klass);

  gobject_class->get_property = g_tls_file_database_gnutls_get_property;
  gobject_class->set_property = g_tls_file_database_gnutls_set_property;
  gobject_class->finalize     = g_tls_file_database_gnutls_finalize;

  gnutls_database_class->create_handle_for_certificate = g_tls_file_database_gnutls_create_handle_for_certificate;
  gnutls_database_class->populate_trust_list           = g_tls_file_database_gnutls_populate_trust_list;

  g_object_class_override_property (gobject_class, PROP_ANCHORS, "anchors");
}

static void
g_tls_file_database_gnutls_file_database_interface_init (GTlsFileDatabaseInterface *iface)
{
}
