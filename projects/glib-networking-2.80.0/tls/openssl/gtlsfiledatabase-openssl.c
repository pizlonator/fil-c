/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlsfiledatabase-openssl.c
 *
 * Copyright (C) 2015 NICE s.r.l.
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 *
 * Authors: Ignacio Casal Quinteiro
 */

#include "config.h"

#include "gtlsfiledatabase-openssl.h"

#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include "openssl-include.h"

struct _GTlsFileDatabaseOpenssl
{
  GTlsDatabaseOpenssl parent_instance;

  /* read-only after construct */
  gchar *anchor_filename;

  /* protected by mutex */
  GMutex mutex;

  /*
   * These are hash tables of gulong -> GPtrArray<GBytes>. The values of
   * the ptr array are full DER encoded certificate values. The keys are byte
   * arrays containing either subject DNs, issuer DNs, or full DER encoded certs
   */
  GHashTable *subjects;
  GHashTable *issuers;

  /*
   * This is a table of GBytes -> GBytes. The values and keys are
   * DER encoded certificate values.
   */
  GHashTable *complete;

  /*
   * This is a table of gchar * -> GTlsCertificate.
   */
  GHashTable *certs_by_handle;
};

enum {
  STATUS_FAILURE,
  STATUS_INCOMPLETE,
  STATUS_SELFSIGNED,
  STATUS_PINNED,
  STATUS_ANCHORED,
};

enum
{
  PROP_0,
  PROP_ANCHORS,
};

static void g_tls_file_database_openssl_file_database_interface_init (GTlsFileDatabaseInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsFileDatabaseOpenssl, g_tls_file_database_openssl, G_TYPE_TLS_DATABASE_OPENSSL,
                               G_IMPLEMENT_INTERFACE (G_TYPE_TLS_FILE_DATABASE,
                                                      g_tls_file_database_openssl_file_database_interface_init)
                              )

static GHashTable *
bytes_multi_table_new (void)
{
  return g_hash_table_new_full (g_int_hash, g_int_equal,
                                (GDestroyNotify)g_free,
                                (GDestroyNotify)g_ptr_array_unref);
}

static void
bytes_multi_table_insert (GHashTable *table,
                          gulong      key,
                          GBytes     *value)
{
  GPtrArray *multi;

  multi = g_hash_table_lookup (table, &key);
  if (!multi)
    {
      int *key_ptr;

      key_ptr = g_new (int, 1);
      *key_ptr = (int)key;
      multi = g_ptr_array_new_with_free_func ((GDestroyNotify)g_bytes_unref);
      g_hash_table_insert (table, key_ptr, multi);
    }
  g_ptr_array_add (multi, g_bytes_ref (value));
}

static GBytes *
bytes_multi_table_lookup_ref_one (GHashTable *table,
                                  gulong      key)
{
  GPtrArray *multi;

  multi = g_hash_table_lookup (table, &key);
  if (!multi)
    return NULL;

  g_assert (multi->len > 0);
  return g_bytes_ref (multi->pdata[0]);
}

static GList *
bytes_multi_table_lookup_ref_all (GHashTable *table,
                                  gulong      key)
{
  GPtrArray *multi;
  GList *list = NULL;
  guint i;

  multi = g_hash_table_lookup (table, &key);
  if (!multi)
    return NULL;

  for (i = 0; i < multi->len; i++)
    list = g_list_prepend (list, g_bytes_ref (multi->pdata[i]));

  return g_list_reverse (list);
}

static gchar *
create_handle_for_certificate (const gchar *filename,
                               GBytes      *der)
{
  gchar *bookmark;
  gchar *uri_part;
  gchar *uri;

  /*
   * Here we create a URI that looks like:
   * file:///etc/ssl/certs/ca-certificates.crt#11b2641821252596420e468c275771f5e51022c121a17bd7a89a2f37b6336c8f
   */

  uri_part = g_filename_to_uri (filename, NULL, NULL);
  if (!uri_part)
    return NULL;

  bookmark = g_compute_checksum_for_bytes (G_CHECKSUM_SHA256, der);
  uri = g_strconcat (uri_part, "#", bookmark, NULL);

  g_free (bookmark);
  g_free (uri_part);

  return uri;
}

static gboolean
load_anchor_file (GTlsFileDatabaseOpenssl  *file_database,
                  const gchar              *filename,
                  GHashTable               *subjects,
                  GHashTable               *issuers,
                  GHashTable               *complete,
                  GHashTable               *certs_by_handle,
                  GError                  **error)
{
  GList *list;
  GList *l;
  GBytes *der;
  gchar *handle;
  GError *my_error = NULL;

  list = g_tls_certificate_list_new_from_file (filename, &my_error);
  if (my_error)
    {
      g_propagate_error (error, my_error);
      return FALSE;
    }

  for (l = list; l; l = l->next)
    {
      X509 *x;
      unsigned long subject;
      unsigned long issuer;

      x = g_tls_certificate_openssl_get_cert (l->data);
      subject = X509_subject_name_hash (x);
      issuer = X509_issuer_name_hash (x);

      der = g_tls_certificate_openssl_get_bytes (l->data);
      g_return_val_if_fail (der, FALSE);

      g_hash_table_insert (complete, g_bytes_ref (der),
                           g_bytes_ref (der));

      bytes_multi_table_insert (subjects, subject, der);
      bytes_multi_table_insert (issuers, issuer, der);

      handle = create_handle_for_certificate (file_database->anchor_filename, der);
      g_hash_table_insert (certs_by_handle, handle, g_object_ref (l->data));

      g_bytes_unref (der);

      g_object_unref (l->data);
    }
  g_list_free (list);

  return TRUE;
}

static void
g_tls_file_database_openssl_finalize (GObject *object)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (object);

  g_clear_pointer (&file_database->subjects, g_hash_table_destroy);
  g_clear_pointer (&file_database->issuers, g_hash_table_destroy);
  g_clear_pointer (&file_database->complete, g_hash_table_destroy);
  g_clear_pointer (&file_database->certs_by_handle, g_hash_table_destroy);

  g_free (file_database->anchor_filename);
  file_database->anchor_filename = NULL;

  g_mutex_clear (&file_database->mutex);

  G_OBJECT_CLASS (g_tls_file_database_openssl_parent_class)->finalize (object);
}

static void
g_tls_file_database_openssl_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (object);

  switch (prop_id)
    {
    case PROP_ANCHORS:
      g_value_set_string (value, file_database->anchor_filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_file_database_openssl_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (object);
  const gchar *anchor_path;

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

      g_free (file_database->anchor_filename);
      file_database->anchor_filename = g_strdup (anchor_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_file_database_openssl_init (GTlsFileDatabaseOpenssl *file_database)
{
  g_mutex_init (&file_database->mutex);
}

static gchar *
g_tls_file_database_openssl_create_certificate_handle (GTlsDatabase    *database,
                                                       GTlsCertificate *certificate)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (database);
  GBytes *der;
  gboolean contains;
  gchar *handle = NULL;

  der = g_tls_certificate_openssl_get_bytes (G_TLS_CERTIFICATE_OPENSSL (certificate));
  g_return_val_if_fail (der, FALSE);

  g_mutex_lock (&file_database->mutex);

  /* At the same time look up whether this certificate is in list */
  contains = g_hash_table_lookup (file_database->complete, der) ? TRUE : FALSE;

  g_mutex_unlock (&file_database->mutex);

  /* Certificate is in the database */
  if (contains)
    handle = create_handle_for_certificate (file_database->anchor_filename, der);

  g_bytes_unref (der);
  return handle;
}

static GTlsCertificate *
g_tls_file_database_openssl_lookup_certificate_for_handle (GTlsDatabase            *database,
                                                           const gchar             *handle,
                                                           GTlsInteraction         *interaction,
                                                           GTlsDatabaseLookupFlags  flags,
                                                           GCancellable            *cancellable,
                                                           GError                 **error)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (database);
  GTlsCertificate *cert;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  if (!handle)
    return NULL;

  g_mutex_lock (&file_database->mutex);

  cert = g_hash_table_lookup (file_database->certs_by_handle, handle);

  g_mutex_unlock (&file_database->mutex);

  return cert ? g_object_ref (cert) : NULL;
}

static GTlsCertificate *
g_tls_file_database_openssl_lookup_certificate_issuer (GTlsDatabase             *database,
                                                       GTlsCertificate          *certificate,
                                                       GTlsInteraction          *interaction,
                                                       GTlsDatabaseLookupFlags   flags,
                                                       GCancellable             *cancellable,
                                                       GError                  **error)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (database);
  X509 *x;
  unsigned long issuer_hash;
  GBytes *der;
  GTlsCertificate *issuer = NULL;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (certificate), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  if (flags & G_TLS_DATABASE_LOOKUP_KEYPAIR)
    return NULL;

  /* Dig out the issuer of this certificate */
  x = g_tls_certificate_openssl_get_cert (G_TLS_CERTIFICATE_OPENSSL (certificate));
  issuer_hash = X509_issuer_name_hash (x);

  g_mutex_lock (&file_database->mutex);
  der = bytes_multi_table_lookup_ref_one (file_database->subjects, issuer_hash);
  g_mutex_unlock (&file_database->mutex);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    issuer = NULL;
  else if (der)
    issuer = g_tls_certificate_openssl_new (der, NULL);

  if (der)
    g_bytes_unref (der);
  return issuer;

  return NULL;
}

static GList *
g_tls_file_database_openssl_lookup_certificates_issued_by (GTlsDatabase             *database,
                                                           GByteArray               *issuer_raw_dn,
                                                           GTlsInteraction          *interaction,
                                                           GTlsDatabaseLookupFlags   flags,
                                                           GCancellable             *cancellable,
                                                           GError                  **error)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (database);
  X509_NAME *x_name;
  const unsigned char *in;
  GList *issued = NULL;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  /* We don't have any private keys here */
  if (flags & G_TLS_DATABASE_LOOKUP_KEYPAIR)
    return NULL;

  in = issuer_raw_dn->data;
  x_name = d2i_X509_NAME (NULL, &in, issuer_raw_dn->len);
  if (x_name)
    {
      unsigned long issuer_hash;
      GList *ders, *l;

      issuer_hash = X509_NAME_hash (x_name);

      /* Find the full DER value of the certificate */
      g_mutex_lock (&file_database->mutex);
      ders = bytes_multi_table_lookup_ref_all (file_database->issuers, issuer_hash);
      g_mutex_unlock (&file_database->mutex);

      for (l = ders; l; l = g_list_next (l))
        {
          if (g_cancellable_set_error_if_cancelled (cancellable, error))
            {
              g_list_free_full (issued, g_object_unref);
              issued = NULL;
              break;
            }

          issued = g_list_prepend (issued, g_tls_certificate_openssl_new (l->data, NULL));
        }

      g_list_free_full (ders, (GDestroyNotify)g_bytes_unref);
      X509_NAME_free (x_name);
    }

  return issued;
}

static gboolean
g_tls_file_database_openssl_populate_trust_list (GTlsDatabaseOpenssl  *self,
                                                 X509_STORE           *store,
                                                 GError              **error)
{
  GTlsFileDatabaseOpenssl *file_database = G_TLS_FILE_DATABASE_OPENSSL (self);
  GHashTable *subjects, *issuers, *complete, *certs_by_handle;
  gboolean result;

  if (!X509_STORE_load_locations (store, file_database->anchor_filename, NULL))
    {
      char error_buffer[256];
      ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Failed to populate trust list from %s: %s"),
                   file_database->anchor_filename,
                   error_buffer);
      return FALSE;
    }

  subjects = bytes_multi_table_new ();
  issuers = bytes_multi_table_new ();

  complete = g_hash_table_new_full (g_bytes_hash, g_bytes_equal,
                                    (GDestroyNotify)g_bytes_unref,
                                    (GDestroyNotify)g_bytes_unref);

  certs_by_handle = g_hash_table_new_full (g_str_hash, g_str_equal,
                                           (GDestroyNotify)g_free,
                                           (GDestroyNotify)g_object_unref);

  if (file_database->anchor_filename)
    result = load_anchor_file (file_database,
                               file_database->anchor_filename,
                               subjects, issuers, complete,
                               certs_by_handle,
                               error);
  else
    result = TRUE;

  if (result)
    {
      g_mutex_lock (&file_database->mutex);
      if (!file_database->subjects)
        {
          file_database->subjects = subjects;
          subjects = NULL;
        }
      if (!file_database->issuers)
        {
          file_database->issuers = issuers;
          issuers = NULL;
        }
      if (!file_database->complete)
        {
          file_database->complete = complete;
          complete = NULL;
        }
      if (!file_database->certs_by_handle)
        {
          file_database->certs_by_handle = certs_by_handle;
          certs_by_handle = NULL;
        }
      g_mutex_unlock (&file_database->mutex);
    }

  if (subjects)
    g_hash_table_unref (subjects);
  if (issuers)
    g_hash_table_unref (issuers);
  if (complete)
    g_hash_table_unref (complete);
  if (certs_by_handle)
    g_hash_table_unref (certs_by_handle);
  return result;
}

static void
g_tls_file_database_openssl_class_init (GTlsFileDatabaseOpensslClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsDatabaseClass *database_class = G_TLS_DATABASE_CLASS (klass);
  GTlsDatabaseOpensslClass *openssl_database_class = G_TLS_DATABASE_OPENSSL_CLASS (klass);

  gobject_class->get_property = g_tls_file_database_openssl_get_property;
  gobject_class->set_property = g_tls_file_database_openssl_set_property;
  gobject_class->finalize     = g_tls_file_database_openssl_finalize;

  database_class->create_certificate_handle = g_tls_file_database_openssl_create_certificate_handle;
  database_class->lookup_certificate_for_handle = g_tls_file_database_openssl_lookup_certificate_for_handle;
  database_class->lookup_certificate_issuer = g_tls_file_database_openssl_lookup_certificate_issuer;
  database_class->lookup_certificates_issued_by = g_tls_file_database_openssl_lookup_certificates_issued_by;

  openssl_database_class->populate_trust_list = g_tls_file_database_openssl_populate_trust_list;

  g_object_class_override_property (gobject_class, PROP_ANCHORS, "anchors");
}

static void
g_tls_file_database_openssl_file_database_interface_init (GTlsFileDatabaseInterface *iface)
{
}
