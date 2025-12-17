/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009 Red Hat, Inc
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
 */

#include "config.h"

#include <gnutls/gnutls.h>
#include <gnutls/pkcs12.h>
#include <gnutls/x509.h>
#include <string.h>

#include "gtlscertificate-gnutls.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_0,

  PROP_CERTIFICATE,
  PROP_CERTIFICATE_PEM,
  PROP_PRIVATE_KEY,
  PROP_PRIVATE_KEY_PEM,
  PROP_ISSUER,
  PROP_PKCS11_URI,
  PROP_PRIVATE_KEY_PKCS11_URI,
  PROP_NOT_VALID_BEFORE,
  PROP_NOT_VALID_AFTER,
  PROP_SUBJECT_NAME,
  PROP_ISSUER_NAME,
  PROP_DNS_NAMES,
  PROP_IP_ADDRESSES,
  PROP_PKCS12_DATA,
  PROP_PASSWORD,
};

struct _GTlsCertificateGnutls
{
  GTlsCertificate parent_instance;

  gnutls_x509_crt_t cert;
  gnutls_privkey_t key;

  gchar *pkcs11_uri;
  gchar *private_key_pkcs11_uri;

  GTlsCertificateGnutls *issuer;

  GByteArray *pkcs12_data;
  char *password;

  GError *construct_error;

  guint have_cert : 1;
  guint have_key  : 1;
};

static void     g_tls_certificate_gnutls_initable_iface_init (GInitableIface  *iface);
static GTlsCertificateGnutls *g_tls_certificate_gnutls_new_take_x509 (gnutls_x509_crt_t cert);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsCertificateGnutls, g_tls_certificate_gnutls, G_TYPE_TLS_CERTIFICATE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_certificate_gnutls_initable_iface_init);)

static void
g_tls_certificate_gnutls_finalize (GObject *object)
{
  GTlsCertificateGnutls *gnutls = G_TLS_CERTIFICATE_GNUTLS (object);

  g_clear_pointer (&gnutls->cert, gnutls_x509_crt_deinit);
  g_clear_pointer (&gnutls->key, gnutls_privkey_deinit);

  g_clear_pointer (&gnutls->pkcs11_uri, g_free);
  g_clear_pointer (&gnutls->private_key_pkcs11_uri, g_free);

  g_clear_pointer (&gnutls->pkcs12_data, g_byte_array_unref);
  g_clear_pointer (&gnutls->password, g_free);

  g_clear_object (&gnutls->issuer);

  g_clear_error (&gnutls->construct_error);

  G_OBJECT_CLASS (g_tls_certificate_gnutls_parent_class)->finalize (object);
}

static GPtrArray *
get_subject_alt_names (GTlsCertificateGnutls          *cert,
                       gnutls_x509_subject_alt_name_t  type)
{
  GPtrArray *data = NULL;
  guint8 *san = NULL;
  size_t san_size;
  guint san_type;
  guint critical;
  guint i;
  guint status;

  if (type == GNUTLS_SAN_IPADDRESS)
    data = g_ptr_array_new_with_free_func (g_object_unref);
  else
    data = g_ptr_array_new_with_free_func ((GDestroyNotify)g_bytes_unref);

  for (i = 0; ; i++)
  {
    san_size = 0;
    san = NULL;
    status = gnutls_x509_crt_get_subject_alt_name2 (cert->cert, i, san, &san_size, &san_type, &critical);
    if (status == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
      return data;
    else if (san_type != (guint)type)
      continue;

    if (san_size == 0)
      continue;

    san = g_malloc (san_size);
    status = gnutls_x509_crt_get_subject_alt_name2 (cert->cert, i, san, &san_size, &san_type, &critical);
    if (status == (guint)type)
      {
        if (status == (guint)GNUTLS_SAN_IPADDRESS)
          {
            if (san_size == 4)
              g_ptr_array_add (data, g_inet_address_new_from_bytes (san, G_SOCKET_FAMILY_IPV4));
            else if (san_size == 16)
              g_ptr_array_add (data, g_inet_address_new_from_bytes (san, G_SOCKET_FAMILY_IPV6));
          }
        else
          {
            g_assert (status == (guint)GNUTLS_SAN_DNSNAME);
            g_ptr_array_add (data, g_bytes_new (san, san_size));
          }
      }

    g_free (san);
  }

  return data;
}

static void
export_privkey (GTlsCertificateGnutls  *gnutls,
                gnutls_x509_crt_fmt_t   format,
                void                  **output_data,
                size_t                 *output_size)
{
  gnutls_x509_privkey_t x509_privkey = NULL;
  int status;

  if (!gnutls->key)
    goto err;

  status = gnutls_privkey_export_x509 (gnutls->key, &x509_privkey);
  if (status != 0)
    goto err;

  *output_size = 0;
  status = gnutls_x509_privkey_export_pkcs8 (x509_privkey,
                                             format,
                                             NULL, GNUTLS_PKCS_PLAIN,
                                             NULL, output_size);
  if (status != GNUTLS_E_SHORT_MEMORY_BUFFER)
    goto err;

  *output_data = g_malloc (*output_size);
  status = gnutls_x509_privkey_export_pkcs8 (x509_privkey,
                                             format,
                                             NULL, GNUTLS_PKCS_PLAIN,
                                             *output_data, output_size);
  if (status == 0)
    {
      gnutls_x509_privkey_deinit (x509_privkey);
      return;
    }

  g_free (*output_data);

err:
  *output_data = NULL;
  *output_size = 0;

  if (x509_privkey)
    gnutls_x509_privkey_deinit (x509_privkey);
}

static void
maybe_import_pkcs12 (GTlsCertificateGnutls *gnutls)
{
  gnutls_pkcs12_t p12 = NULL;
  gnutls_x509_privkey_t x509_key = NULL;
  gnutls_x509_crt_t *chain = NULL;
  guint chain_len;
  int status;
  gnutls_datum_t p12_data;
  GTlsError error_code = G_TLS_ERROR_BAD_CERTIFICATE;
  GTlsCertificateGnutls *previous_cert;

  /* If password is set first. */
  if (!gnutls->pkcs12_data)
    return;

  p12_data.data = gnutls->pkcs12_data->data;
  p12_data.size = gnutls->pkcs12_data->len;

  status = gnutls_pkcs12_init (&p12);
  if (status != GNUTLS_E_SUCCESS)
    goto import_failed;

  /* Only support DER, it's the common encoding and what everything including OpenSSL uses. */
  status = gnutls_pkcs12_import (p12, &p12_data, GNUTLS_X509_FMT_DER, 0);
  if (status != GNUTLS_E_SUCCESS)
      goto import_failed;

  if (gnutls->password)
    {
      status = gnutls_pkcs12_verify_mac (p12, gnutls->password);
      if (status != GNUTLS_E_SUCCESS)
        {
          error_code = G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD;
          goto import_failed;
        }
    }

  /* Note that this *requires* a cert and key, if we want to make keys optional
   * we would have to re-implement this parsing ourselves. */
  status = gnutls_pkcs12_simple_parse (p12,
                                       gnutls->password ? gnutls->password : "",
                                       &x509_key,
                                       &chain, &chain_len,
                                       NULL, NULL,
                                       NULL,
                                       GNUTLS_PKCS12_SP_INCLUDE_SELF_SIGNED);
  if (status == GNUTLS_E_DECRYPTION_FAILED)
    error_code = G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD;
  if (status != GNUTLS_E_SUCCESS)
    goto import_failed;

  /* Clear a previous error to load without a password. */
  if (g_error_matches (gnutls->construct_error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD))
    g_clear_error (&gnutls->construct_error);

  /* Clear existing initialized empty cert. */
  gnutls_x509_crt_deinit (gnutls->cert);

  /* First cert is the main one. */
  gnutls->cert = chain[0];
  gnutls->have_cert = TRUE;
  previous_cert = gnutls;

  for (guint i = 1; i < chain_len; i++)
    {
      /* GnuTLS already built us a valid chain in order by issuer. See pkcs12.c#make_chain(). */
      GTlsCertificateGnutls *new_cert = g_tls_certificate_gnutls_new_take_x509 (chain[i]);
      g_tls_certificate_gnutls_set_issuer (previous_cert, new_cert);
      previous_cert = new_cert;
      g_object_unref (new_cert);
    }

  g_clear_pointer (&chain, gnutls_free);

  /* Convert X509 privkey to abstract privkey. */
  status = gnutls_privkey_init (&gnutls->key);
  if (status != GNUTLS_E_SUCCESS)
    goto import_failed;

  status = gnutls_privkey_import_x509 (gnutls->key, x509_key, GNUTLS_PRIVKEY_IMPORT_COPY);
  if (status != GNUTLS_E_SUCCESS)
    goto import_failed;

  g_clear_pointer (&x509_key, gnutls_x509_privkey_deinit);
  gnutls->have_key = TRUE;

  g_clear_pointer (&p12, gnutls_pkcs12_deinit);
  return;

import_failed:
  g_clear_error (&gnutls->construct_error);
  g_set_error (&gnutls->construct_error, G_TLS_ERROR, error_code,
              _("Failed to import PKCS #12: %s"), gnutls_strerror (status));

  g_clear_pointer (&p12, gnutls_pkcs12_deinit);
  g_clear_pointer (&x509_key, gnutls_x509_privkey_deinit);
  g_clear_pointer (&chain, gnutls_free);
}

static void
g_tls_certificate_gnutls_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GTlsCertificateGnutls *gnutls = G_TLS_CERTIFICATE_GNUTLS (object);
  GByteArray *byte_array;
  char *pem;
  guint8 *der;
  int status;
  size_t size;
  gnutls_x509_dn_t dn;
  gnutls_datum_t data;
  time_t time;

  switch (prop_id)
    {
    case PROP_PKCS12_DATA:
      g_value_set_boxed (value, gnutls->pkcs12_data);
      break;

    case PROP_CERTIFICATE:
      size = 0;
      status = gnutls_x509_crt_export (gnutls->cert,
                                       GNUTLS_X509_FMT_DER,
                                       NULL, &size);
      if (status != GNUTLS_E_SHORT_MEMORY_BUFFER)
        byte_array = NULL;
      else
        {
          byte_array = g_byte_array_sized_new (size);
          byte_array->len = size;
          status = gnutls_x509_crt_export (gnutls->cert,
                                           GNUTLS_X509_FMT_DER,
                                           byte_array->data, &size);
          if (status != 0)
            {
              g_byte_array_free (byte_array, TRUE);
              byte_array = NULL;
            }
        }
      g_value_take_boxed (value, byte_array);
      break;

    case PROP_CERTIFICATE_PEM:
      size = 0;
      status = gnutls_x509_crt_export (gnutls->cert,
                                       GNUTLS_X509_FMT_PEM,
                                       NULL, &size);
      if (status != GNUTLS_E_SHORT_MEMORY_BUFFER)
        pem = NULL;
      else
        {
          pem = g_malloc (size);
          status = gnutls_x509_crt_export (gnutls->cert,
                                           GNUTLS_X509_FMT_PEM,
                                           pem, &size);
          if (status != 0)
            g_clear_pointer (&pem, g_free);
        }
      g_value_take_string (value, pem);
      break;

    case PROP_PRIVATE_KEY:
      export_privkey (gnutls, GNUTLS_X509_FMT_DER, (void **)&der, &size);
      if (size > 0 && size <= G_MAXUINT)
        {
          byte_array = g_byte_array_new_take (der, size);
          g_value_take_boxed (value, byte_array);
        }
      break;

    case PROP_PRIVATE_KEY_PEM:
      export_privkey (gnutls, GNUTLS_X509_FMT_PEM, (void **)&pem, &size);
      if (size > 0)
        g_value_take_string (value, pem);
      break;

    case PROP_ISSUER:
      g_value_set_object (value, gnutls->issuer);
      break;

    case PROP_PKCS11_URI:
      g_value_set_string (value, gnutls->pkcs11_uri);
      break;

    case PROP_PRIVATE_KEY_PKCS11_URI:
      g_value_set_string (value, gnutls->private_key_pkcs11_uri);
      break;

    case PROP_NOT_VALID_BEFORE:
      time = gnutls_x509_crt_get_activation_time (gnutls->cert);
      if (time != (time_t)-1)
        g_value_take_boxed (value, g_date_time_new_from_unix_utc (time));
      break;

    case PROP_NOT_VALID_AFTER:
      time = gnutls_x509_crt_get_expiration_time (gnutls->cert);
      if (time != (time_t)-1)
        g_value_take_boxed (value, g_date_time_new_from_unix_utc (time));
      break;

    case PROP_SUBJECT_NAME:
      status = gnutls_x509_crt_get_subject (gnutls->cert, &dn);
      if (status != GNUTLS_E_SUCCESS)
        return;

      status = gnutls_x509_dn_get_str (dn, &data);
      if (status != GNUTLS_E_SUCCESS)
        return;

      g_value_take_string (value, g_strndup ((gchar *)data.data, data.size));
      gnutls_free (data.data);
      break;

    case PROP_ISSUER_NAME:
      status = gnutls_x509_crt_get_issuer (gnutls->cert, &dn);
      if (status != GNUTLS_E_SUCCESS)
        return;

      status = gnutls_x509_dn_get_str (dn, &data);
      if (status != GNUTLS_E_SUCCESS)
        return;

      g_value_take_string (value, g_strndup ((gchar *)data.data, data.size));
      gnutls_free (data.data);
      break;

    case PROP_DNS_NAMES:
      g_value_take_boxed (value, get_subject_alt_names (gnutls, GNUTLS_SAN_DNSNAME));
      break;

    case PROP_IP_ADDRESSES:
      g_value_take_boxed (value, get_subject_alt_names (gnutls, GNUTLS_SAN_IPADDRESS));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#define CRITICAL_IF_KEY_INITIALIZED(property_name) G_STMT_START \
  { \
    if (gnutls->have_key) \
      { \
        g_critical ("GTlsCertificate: Failed to set construct property \"%s\" because a private key was already set earlier during construction.", property_name); \
        return; \
      } \
  } \
G_STMT_END

#define CRITICAL_IF_CERTIFICATE_INITIALIZED(property_name) G_STMT_START \
  { \
    if (gnutls->have_cert) \
      { \
        g_critical ("GTlsCertificate: Failed to set construct property \"%s\" because a certificate was already set earlier during construction.", property_name); \
        return; \
      } \
  } \
G_STMT_END

#define CRITICAL_IF_INITIALIZED(property_name) G_STMT_START \
  { \
    CRITICAL_IF_CERTIFICATE_INITIALIZED (property_name); \
    CRITICAL_IF_KEY_INITIALIZED (property_name); \
  } \
G_STMT_END

static void
g_tls_certificate_gnutls_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GTlsCertificateGnutls *gnutls = G_TLS_CERTIFICATE_GNUTLS (object);
  GByteArray *bytes;
  const char *string;
  gnutls_datum_t data;
  int status;

  switch (prop_id)
    {
    case PROP_PASSWORD:
      gnutls->password = g_value_dup_string (value);
      if (gnutls->password)
        {
          CRITICAL_IF_INITIALIZED ("password");
          maybe_import_pkcs12 (gnutls);
        }
      break;

    case PROP_PKCS12_DATA:
      gnutls->pkcs12_data = g_value_dup_boxed (value);
      if (gnutls->pkcs12_data)
        {
          CRITICAL_IF_INITIALIZED ("pkcs12-data");
          maybe_import_pkcs12 (gnutls);
        }
      break;

    case PROP_CERTIFICATE:
      bytes = g_value_get_boxed (value);
      if (!bytes)
        break;
      CRITICAL_IF_CERTIFICATE_INITIALIZED ("certificate");
      data.data = bytes->data;
      data.size = bytes->len;
      status = gnutls_x509_crt_import (gnutls->cert, &data,
                                       GNUTLS_X509_FMT_DER);
      if (status == 0)
        gnutls->have_cert = TRUE;
      else if (!gnutls->construct_error)
        {
          gnutls->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse DER certificate: %s"),
                         gnutls_strerror (status));
        }

      break;

    case PROP_CERTIFICATE_PEM:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_CERTIFICATE_INITIALIZED ("certificate-pem");
      data.data = (void *)string;
      data.size = strlen (string);
      status = gnutls_x509_crt_import (gnutls->cert, &data,
                                       GNUTLS_X509_FMT_PEM);
      if (status == 0)
        gnutls->have_cert = TRUE;
      else if (!gnutls->construct_error)
        {
          gnutls->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse PEM certificate: %s"),
                         gnutls_strerror (status));
        }
      break;

    case PROP_PRIVATE_KEY:
      bytes = g_value_get_boxed (value);
      if (!bytes)
        break;
      CRITICAL_IF_KEY_INITIALIZED ("private-key");
      data.data = bytes->data;
      data.size = bytes->len;
      if (!gnutls->key)
        gnutls_privkey_init (&gnutls->key);
      status = gnutls_privkey_import_x509_raw (gnutls->key, &data,
                                               GNUTLS_X509_FMT_DER,
                                               NULL, GNUTLS_PKCS_PLAIN);
      if (status == 0)
        gnutls->have_key = TRUE;
      else if (!gnutls->construct_error)
        {
          gnutls->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse DER private key: %s"),
                         gnutls_strerror (status));
        }
      break;

    case PROP_PRIVATE_KEY_PEM:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_KEY_INITIALIZED ("private-key-pem");
      data.data = (void *)string;
      data.size = strlen (string);
      if (!gnutls->key)
        gnutls_privkey_init (&gnutls->key);
      status = gnutls_privkey_import_x509_raw (gnutls->key, &data,
                                               GNUTLS_X509_FMT_PEM,
                                               NULL, GNUTLS_PKCS_PLAIN);
      if (status == 0)
        gnutls->have_key = TRUE;
      else if (!gnutls->construct_error)
        {
          gnutls->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse PEM private key: %s"),
                         gnutls_strerror (status));
        }
      break;

    case PROP_ISSUER:
      gnutls->issuer = g_value_dup_object (value);
      break;

    case PROP_PKCS11_URI:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_CERTIFICATE_INITIALIZED ("pkcs11-uri");

      gnutls->pkcs11_uri = g_strdup (string);

      status = gnutls_x509_crt_import_url (gnutls->cert, string, GNUTLS_PKCS11_OBJ_FLAG_CRT);
      if (status == GNUTLS_E_SUCCESS)
        {
          gnutls->have_cert = TRUE;
        }
      else if (!gnutls->construct_error)
        {
          gnutls->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                          _("Could not import PKCS #11 certificate URI: %s"),
                          gnutls_strerror (status));
        }
      break;

    case PROP_PRIVATE_KEY_PKCS11_URI:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_KEY_INITIALIZED ("private-key-pkcs11-uri");

      gnutls->private_key_pkcs11_uri = g_strdup (string);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_certificate_gnutls_init (GTlsCertificateGnutls *gnutls)
{
  gnutls_x509_crt_init (&gnutls->cert);
}

static gboolean
g_tls_certificate_gnutls_initable_init (GInitable       *initable,
                                        GCancellable    *cancellable,
                                        GError         **error)
{
  GTlsCertificateGnutls *gnutls = G_TLS_CERTIFICATE_GNUTLS (initable);

  /* After init we don't need to keep the password around. */
  g_clear_pointer (&gnutls->password, g_free);

  if (gnutls->construct_error)
    {
      g_propagate_error (error, gnutls->construct_error);
      gnutls->construct_error = NULL;
      return FALSE;
    }
  else if (!gnutls->have_cert)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("No certificate data provided"));
      return FALSE;
    }
  else
    return TRUE;
}

static GTlsCertificateFlags
g_tls_certificate_gnutls_verify (GTlsCertificate     *cert,
                                 GSocketConnectable  *identity,
                                 GTlsCertificate     *trusted_ca)
{
  GTlsCertificateGnutls *cert_gnutls;
  guint num_certs, i;
  gnutls_x509_crt_t *chain;
  GTlsCertificateFlags gtls_flags;
  GError *error = NULL;

  cert_gnutls = G_TLS_CERTIFICATE_GNUTLS (cert);
  num_certs = 0;
  do
    {
      cert_gnutls = cert_gnutls->issuer;
      num_certs++;
    }
  while (cert_gnutls);

  chain = g_new (gnutls_x509_crt_t, num_certs);
  cert_gnutls = G_TLS_CERTIFICATE_GNUTLS (cert);
  for (i = 0; i < num_certs; i++)
    {
      chain[i] = cert_gnutls->cert;
      cert_gnutls = cert_gnutls->issuer;
    }
  g_assert (!cert_gnutls);

  if (trusted_ca)
    {
      gnutls_x509_crt_t ca;
      guint gnutls_flags;
      int status;

      ca = G_TLS_CERTIFICATE_GNUTLS (trusted_ca)->cert;
      status = gnutls_x509_crt_list_verify (chain, num_certs,
                                            &ca, 1,
                                            NULL, 0, 0,
                                            &gnutls_flags);
      if (status != 0)
        {
          g_free (chain);
          return G_TLS_CERTIFICATE_GENERIC_ERROR;
        }

      gtls_flags = g_tls_certificate_gnutls_convert_flags (gnutls_flags);
    }
  else
    gtls_flags = 0;

  g_free (chain);

  if (identity)
    {
      gtls_flags |= g_tls_certificate_gnutls_verify_identity (G_TLS_CERTIFICATE_GNUTLS (cert), identity, &error);
      if (error)
        {
          g_warning ("Error verifying TLS certificate: %s", error->message);
          g_error_free (error);
        }
    }

  return gtls_flags;
}

static void
g_tls_certificate_gnutls_class_init (GTlsCertificateGnutlsClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsCertificateClass *certificate_class = G_TLS_CERTIFICATE_CLASS (klass);

  gobject_class->get_property = g_tls_certificate_gnutls_get_property;
  gobject_class->set_property = g_tls_certificate_gnutls_set_property;
  gobject_class->finalize     = g_tls_certificate_gnutls_finalize;

  certificate_class->verify = g_tls_certificate_gnutls_verify;

  g_object_class_override_property (gobject_class, PROP_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CERTIFICATE_PEM, "certificate-pem");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY, "private-key");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY_PEM, "private-key-pem");
  g_object_class_override_property (gobject_class, PROP_ISSUER, "issuer");
  g_object_class_override_property (gobject_class, PROP_PKCS11_URI, "pkcs11-uri");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY_PKCS11_URI, "private-key-pkcs11-uri");
  g_object_class_override_property (gobject_class, PROP_NOT_VALID_BEFORE, "not-valid-before");
  g_object_class_override_property (gobject_class, PROP_NOT_VALID_AFTER, "not-valid-after");
  g_object_class_override_property (gobject_class, PROP_SUBJECT_NAME, "subject-name");
  g_object_class_override_property (gobject_class, PROP_ISSUER_NAME, "issuer-name");
  g_object_class_override_property (gobject_class, PROP_DNS_NAMES, "dns-names");
  g_object_class_override_property (gobject_class, PROP_IP_ADDRESSES, "ip-addresses");
  g_object_class_override_property (gobject_class, PROP_PKCS12_DATA, "pkcs12-data");
  g_object_class_override_property (gobject_class, PROP_PASSWORD, "password");
}

static void
g_tls_certificate_gnutls_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_tls_certificate_gnutls_initable_init;
}

GTlsCertificate *
g_tls_certificate_gnutls_new (const gnutls_datum_t *datum,
                              GTlsCertificate      *issuer)
{
  GTlsCertificateGnutls *gnutls;

  gnutls = g_object_new (G_TYPE_TLS_CERTIFICATE_GNUTLS,
                         "issuer", issuer,
                         NULL);
  g_tls_certificate_gnutls_set_data (gnutls, datum);

  return G_TLS_CERTIFICATE (gnutls);
}

static GTlsCertificateGnutls *
g_tls_certificate_gnutls_new_take_x509 (gnutls_x509_crt_t cert)
{
  GTlsCertificateGnutls *gnutls;

  gnutls = g_object_new (G_TYPE_TLS_CERTIFICATE_GNUTLS, NULL);
  gnutls->cert = cert;
  gnutls->have_cert = TRUE;

  return gnutls;
}

void
g_tls_certificate_gnutls_set_data (GTlsCertificateGnutls *gnutls,
                                   const gnutls_datum_t  *datum)
{
  g_return_if_fail (G_IS_TLS_CERTIFICATE_GNUTLS (gnutls));
  g_return_if_fail (!gnutls->have_cert);

  if (gnutls_x509_crt_import (gnutls->cert, datum,
                              GNUTLS_X509_FMT_DER) == 0)
    gnutls->have_cert = TRUE;
}

const gnutls_x509_crt_t
g_tls_certificate_gnutls_get_cert (GTlsCertificateGnutls *gnutls)
{
  return gnutls->cert;
}

gboolean
g_tls_certificate_gnutls_is_pkcs11_backed (GTlsCertificateGnutls *gnutls)
{
  return gnutls->pkcs11_uri != NULL;
}

gboolean
g_tls_certificate_gnutls_has_key (GTlsCertificateGnutls *gnutls)
{
  return gnutls->have_key;
}

void
g_tls_certificate_gnutls_copy  (GTlsCertificateGnutls  *gnutls,
                                const gchar            *interaction_id,
                                gnutls_pcert_st       **pcert,
                                unsigned int           *pcert_length,
                                gnutls_privkey_t       *pkey)
{
  GTlsCertificateGnutls *chain;
  guint num_certs = 0;
  int status;

  g_return_if_fail (G_IS_TLS_CERTIFICATE_GNUTLS (gnutls));
  g_return_if_fail (pcert);
  g_return_if_fail (pcert_length);
  g_return_if_fail (pkey);

  /* We will do this loop twice. It's probably more efficient than
   * re-allocating memory.
   */
  chain = gnutls;
  while (chain)
    {
      num_certs++;
      chain = chain->issuer;
    }

  *pcert_length = 0;
  *pcert = g_malloc (sizeof (gnutls_pcert_st) * num_certs);

  /* Now do the actual copy of the whole chain. */
  chain = gnutls;
  while (chain)
    {
      gnutls_x509_crt_t cert;
      gnutls_datum_t data;

      gnutls_x509_crt_export2 (chain->cert, GNUTLS_X509_FMT_DER, &data);

      gnutls_x509_crt_init (&cert);
      status = gnutls_x509_crt_import (cert, &data, GNUTLS_X509_FMT_DER);
      g_warn_if_fail (status == 0);
      gnutls_free (data.data);

      gnutls_pcert_import_x509 (*pcert + *pcert_length, cert, 0);
      gnutls_x509_crt_deinit (cert);
      (*pcert_length)++;

      chain = chain->issuer;
    }

  if (gnutls->key)
    {
      gnutls_x509_privkey_t x509_privkey;

      gnutls_privkey_export_x509 (gnutls->key, &x509_privkey);
      gnutls_privkey_import_x509 (*pkey, x509_privkey, GNUTLS_PRIVKEY_IMPORT_COPY);
      gnutls_x509_privkey_deinit (x509_privkey);
    }
  else if (gnutls->private_key_pkcs11_uri || gnutls->pkcs11_uri)
    {
      int status;

      status = gnutls_privkey_import_pkcs11_url (*pkey,
                                                 gnutls->private_key_pkcs11_uri ? gnutls->private_key_pkcs11_uri : gnutls->pkcs11_uri);
      if (status != GNUTLS_E_SUCCESS)
        {
          gnutls_privkey_deinit (*pkey);
          *pkey = NULL;
          g_info ("Failed to copy PKCS #11 private key: %s", gnutls_strerror (status));
        }
    }
  else
    {
      gnutls_privkey_deinit (*pkey);
      *pkey = NULL;
    }
}

void
g_tls_certificate_gnutls_copy_free (gnutls_pcert_st  *pcert,
                                    unsigned int      pcert_length,
                                    gnutls_privkey_t  pkey)
{
  if (pcert)
    {
      for (unsigned int i = 0; i < pcert_length; i++)
        gnutls_pcert_deinit (&pcert[i]);
      g_free (pcert);
    }

  if (pkey)
    gnutls_privkey_deinit (pkey);
}

static const struct {
  int gnutls_flag;
  GTlsCertificateFlags gtls_flag;
} flags_map[] = {
  { GNUTLS_CERT_SIGNER_NOT_FOUND | GNUTLS_CERT_SIGNER_NOT_CA, G_TLS_CERTIFICATE_UNKNOWN_CA },
  { GNUTLS_CERT_NOT_ACTIVATED, G_TLS_CERTIFICATE_NOT_ACTIVATED },
  { GNUTLS_CERT_EXPIRED, G_TLS_CERTIFICATE_EXPIRED },
  { GNUTLS_CERT_REVOKED, G_TLS_CERTIFICATE_REVOKED },
  { GNUTLS_CERT_INSECURE_ALGORITHM, G_TLS_CERTIFICATE_INSECURE },
  { GNUTLS_CERT_UNEXPECTED_OWNER, G_TLS_CERTIFICATE_BAD_IDENTITY }
};
static const int flags_map_size = G_N_ELEMENTS (flags_map);

GTlsCertificateFlags
g_tls_certificate_gnutls_convert_flags (guint gnutls_flags)
{
  int i;
  GTlsCertificateFlags gtls_flags;

  /* Convert GNUTLS status to GTlsCertificateFlags. GNUTLS sets
   * GNUTLS_CERT_INVALID if it sets any other flag, so we want to
   * strip that out unless it's the only flag set. Then we convert
   * specific flags we recognize, and if there are any flags left over
   * at the end, we add G_TLS_CERTIFICATE_GENERIC_ERROR.
   */
  gtls_flags = 0;

  if (gnutls_flags != GNUTLS_CERT_INVALID)
    gnutls_flags = gnutls_flags & ~GNUTLS_CERT_INVALID;
  for (i = 0; i < flags_map_size && gnutls_flags != 0; i++)
    {
      if (gnutls_flags & flags_map[i].gnutls_flag)
        {
          gnutls_flags &= ~flags_map[i].gnutls_flag;
          gtls_flags |= flags_map[i].gtls_flag;
        }
    }
  if (gnutls_flags)
    gtls_flags |= G_TLS_CERTIFICATE_GENERIC_ERROR;

  return gtls_flags;
}

GTlsCertificateFlags
g_tls_certificate_gnutls_verify_identity (GTlsCertificateGnutls  *gnutls,
                                          GSocketConnectable     *identity,
                                          GError                **error)
{
  GTlsCertificateFlags result = 0;
  const char *hostname;
  char *free_hostname = NULL;

  if (G_IS_NETWORK_ADDRESS (identity))
    hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (identity));
  else if (G_IS_NETWORK_SERVICE (identity))
    hostname = g_network_service_get_domain (G_NETWORK_SERVICE (identity));
  else if (G_IS_INET_SOCKET_ADDRESS (identity))
    {
      GInetAddress *addr;

      addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (identity));
      hostname = free_hostname = g_inet_address_to_string (addr);
    }
  else
    {
      g_set_error (error, G_TLS_ERROR, G_TLS_ERROR_MISC,
                   _("Cannot verify peer identity of unexpected type %s"), G_OBJECT_TYPE_NAME (identity));
      return G_TLS_CERTIFICATE_BAD_IDENTITY;
    }

  g_assert (hostname);
  if (!gnutls_x509_crt_check_hostname (gnutls->cert, hostname))
    result |= G_TLS_CERTIFICATE_BAD_IDENTITY;

  g_free (free_hostname);

  return result;
}

void
g_tls_certificate_gnutls_set_issuer (GTlsCertificateGnutls *gnutls,
                                     GTlsCertificateGnutls *issuer)
{
  g_return_if_fail (G_IS_TLS_CERTIFICATE_GNUTLS (gnutls));
  g_return_if_fail (!issuer || G_IS_TLS_CERTIFICATE_GNUTLS (issuer));

  if (issuer)
    g_object_ref (issuer);
  if (gnutls->issuer)
    g_object_unref (gnutls->issuer);
  gnutls->issuer = issuer;
  g_object_notify (G_OBJECT (gnutls), "issuer");
}

GBytes *
g_tls_certificate_gnutls_get_bytes (GTlsCertificateGnutls *gnutls)
{
  GByteArray *array;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE_GNUTLS (gnutls), NULL);

  g_object_get (gnutls, "certificate", &array, NULL);
  return g_byte_array_free_to_bytes (array);
}

static gnutls_x509_crt_t *
convert_data_to_gnutls_certs (const gnutls_datum_t  *certs,
                              guint                  num_certs,
                              gnutls_x509_crt_fmt_t  format)
{
  gnutls_x509_crt_t *gnutls_certs;
  guint i;

  gnutls_certs = g_new (gnutls_x509_crt_t, num_certs);

  for (i = 0; i < num_certs; i++)
    {
      if (gnutls_x509_crt_init (&gnutls_certs[i]) < 0)
        {
          i--;
          goto error;
        }
    }

  for (i = 0; i < num_certs; i++)
    {
      if (gnutls_x509_crt_import (gnutls_certs[i], &certs[i], format) < 0)
        {
          i = num_certs - 1;
          goto error;
        }
    }

  return gnutls_certs;

error:
  for (; i != G_MAXUINT; i--)
    gnutls_x509_crt_deinit (gnutls_certs[i]);
  g_free (gnutls_certs);
  return NULL;
}

GTlsCertificateGnutls *
g_tls_certificate_gnutls_build_chain (const gnutls_datum_t  *certs,
                                      guint                  num_certs,
                                      gnutls_x509_crt_fmt_t  format)
{
  GPtrArray *glib_certs;
  gnutls_x509_crt_t *gnutls_certs;
  GTlsCertificateGnutls *issuer;
  GTlsCertificateGnutls *result;
  guint i, j;

  g_return_val_if_fail (certs, NULL);

  gnutls_certs = convert_data_to_gnutls_certs (certs, num_certs, format);
  if (!gnutls_certs)
    return NULL;

  glib_certs = g_ptr_array_new_full (num_certs, g_object_unref);
  for (i = 0; i < num_certs; i++)
    g_ptr_array_add (glib_certs, g_tls_certificate_gnutls_new (&certs[i], NULL));

  /* Some servers send certs out of order, or will send duplicate
   * certs, so we need to be careful when assigning the issuer of
   * our new GTlsCertificateGnutls.
   */
  for (i = 0; i < num_certs; i++)
    {
      issuer = NULL;

      /* Check if the cert issued itself */
      if (gnutls_x509_crt_check_issuer (gnutls_certs[i], gnutls_certs[i]))
        continue;

      if (i < num_certs - 1 &&
          gnutls_x509_crt_check_issuer (gnutls_certs[i], gnutls_certs[i + 1]))
        {
          issuer = glib_certs->pdata[i + 1];
        }
      else
        {
          for (j = 0; j < num_certs; j++)
            {
              if (j != i &&
                  gnutls_x509_crt_check_issuer (gnutls_certs[i], gnutls_certs[j]))
                {
                  issuer = glib_certs->pdata[j];
                  break;
                }
            }
        }

      if (issuer)
        g_tls_certificate_gnutls_set_issuer (glib_certs->pdata[i], issuer);
    }

  result = g_object_ref (glib_certs->pdata[0]);
  g_ptr_array_unref (glib_certs);

  for (i = 0; i < num_certs; i++)
    gnutls_x509_crt_deinit (gnutls_certs[i]);
  g_free (gnutls_certs);

  return result;
}
