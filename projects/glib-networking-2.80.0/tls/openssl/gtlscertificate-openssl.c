/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * gtlscertificate-openssl.c
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

#include <string.h>
#include "openssl-include.h"
#include <openssl/pkcs12.h>

#include "gtlscertificate-openssl.h"
#include <glib/gi18n-lib.h>

struct _GTlsCertificateOpenssl
{
  GTlsCertificate parent_instance;

  X509 *cert;
  EVP_PKEY *key;

  GByteArray *pkcs12_data;
  char *password;

  GTlsCertificateOpenssl *issuer;

  GError *construct_error;

  guint have_cert : 1;
  guint have_key  : 1;
};

enum
{
  PROP_0,

  PROP_CERTIFICATE,
  PROP_CERTIFICATE_PEM,
  PROP_PRIVATE_KEY,
  PROP_PRIVATE_KEY_PEM,
  PROP_ISSUER,
  PROP_NOT_VALID_BEFORE,
  PROP_NOT_VALID_AFTER,
  PROP_SUBJECT_NAME,
  PROP_ISSUER_NAME,
  PROP_DNS_NAMES,
  PROP_IP_ADDRESSES,
  PROP_PKCS12_DATA,
  PROP_PASSWORD,
};

static void     g_tls_certificate_openssl_initable_iface_init (GInitableIface  *iface);
static gboolean is_issuer (GTlsCertificateOpenssl *cert, GTlsCertificateOpenssl *issuer);

G_DEFINE_FINAL_TYPE_WITH_CODE (GTlsCertificateOpenssl, g_tls_certificate_openssl, G_TYPE_TLS_CERTIFICATE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                      g_tls_certificate_openssl_initable_iface_init))

static void
g_tls_certificate_openssl_finalize (GObject *object)
{
  GTlsCertificateOpenssl *openssl = G_TLS_CERTIFICATE_OPENSSL (object);

  if (openssl->cert)
    X509_free (openssl->cert);
  if (openssl->key)
    EVP_PKEY_free (openssl->key);

  g_clear_pointer (&openssl->pkcs12_data, g_byte_array_unref);
  g_clear_pointer (&openssl->password, g_free);

  g_clear_object (&openssl->issuer);

  g_clear_error (&openssl->construct_error);

  G_OBJECT_CLASS (g_tls_certificate_openssl_parent_class)->finalize (object);
}

static GPtrArray *
get_subject_alt_names (GTlsCertificateOpenssl *cert,
                       guint                   type)
{
  GPtrArray *data = NULL;
  STACK_OF (GENERAL_NAME) *sans;
  const guint8 *san = NULL;
  size_t san_size;
  guint alt_occurrences;
  guint i;

  if (type == GEN_IPADD)
    data = g_ptr_array_new_with_free_func (g_object_unref);
  else
    data = g_ptr_array_new_with_free_func ((GDestroyNotify)g_bytes_unref);

  sans = X509_get_ext_d2i (cert->cert, NID_subject_alt_name, NULL, NULL);
  if (sans)
    {
      alt_occurrences = sk_GENERAL_NAME_num (sans);
      for (i = 0; i < alt_occurrences; i++)
        {
          const GENERAL_NAME *value = sk_GENERAL_NAME_value (sans, i);
          if (value->type != type)
            continue;

          if (type == GEN_IPADD)
            {
              g_assert (value->type == GEN_IPADD);
              san = ASN1_STRING_get0_data (value->d.ip);
              san_size = ASN1_STRING_length (value->d.ip);
              if (san_size == 4)
                g_ptr_array_add (data, g_inet_address_new_from_bytes (san, G_SOCKET_FAMILY_IPV4));
              else if (san_size == 16)
                g_ptr_array_add (data, g_inet_address_new_from_bytes (san, G_SOCKET_FAMILY_IPV6));
            }
          else
            {
              g_assert (value->type == GEN_DNS);
              san = ASN1_STRING_get0_data (value->d.ia5);
              san_size = ASN1_STRING_length (value->d.ia5);
              g_ptr_array_add (data, g_bytes_new (san, san_size));
            }
          }

      for (i = 0; i < alt_occurrences; i++)
        GENERAL_NAME_free (sk_GENERAL_NAME_value (sans, i));
      sk_GENERAL_NAME_free (sans);
    }

  return data;
}

static void
export_privkey_to_der (GTlsCertificateOpenssl  *openssl,
                       guint8                 **output_data,
                       long                    *output_size)
{
  PKCS8_PRIV_KEY_INFO *pkcs8 = NULL;
  BIO *bio = NULL;
  const guint8 *data;

  if (!openssl->key)
    goto err;

  pkcs8 = EVP_PKEY2PKCS8 (openssl->key);
  if (!pkcs8)
    goto err;

  bio = BIO_new (BIO_s_mem ());
  if (i2d_PKCS8_PRIV_KEY_INFO_bio (bio, pkcs8) == 0)
    goto err;

  *output_size = BIO_get_mem_data (bio, (char **)&data);
  if (*output_size <= 0)
    goto err;

  *output_data = g_malloc (*output_size);
  memcpy (*output_data, data, *output_size);
  goto out;

err:
  *output_data = NULL;
  *output_size = 0;
out:
  if (bio)
    BIO_free_all (bio);
  if (pkcs8)
    PKCS8_PRIV_KEY_INFO_free (pkcs8);
}

static char *
export_privkey_to_pem (GTlsCertificateOpenssl *openssl)
{
  int ret;
  BIO *bio = NULL;
  const char *data = NULL;
  char *result = NULL;

  if (!openssl->key)
    return NULL;

  bio = BIO_new (BIO_s_mem ());
  ret = PEM_write_bio_PKCS8PrivateKey (bio, openssl->key, NULL, NULL, 0, NULL, NULL);
  if (ret == 0)
    goto out;

  ret = BIO_write (bio, "\0", 1);
  if (ret != 1)
    goto out;

  BIO_get_mem_data (bio, (char **)&data);
  result = g_strdup (data);

out:
  BIO_free_all (bio);
  return result;
}

static void
maybe_import_pkcs12 (GTlsCertificateOpenssl *openssl)
{
  PKCS12 *p12 = NULL;
  X509 *cert = NULL;
  STACK_OF(X509) *ca = NULL;
  EVP_PKEY *key = NULL;
  BIO *bio = NULL;
  int status;
  char error_buffer[256] = { 0 };
  GTlsError error_code = G_TLS_ERROR_BAD_CERTIFICATE;

  /* If password is set first. */
  if (!openssl->pkcs12_data)
    return;

  bio = BIO_new (BIO_s_mem ());
  status = BIO_write (bio, openssl->pkcs12_data->data, openssl->pkcs12_data->len);
  if (status <= 0)
    goto import_failed;
  g_assert (status == openssl->pkcs12_data->len);

  p12 = d2i_PKCS12_bio (bio, NULL);
  if (p12 == NULL)
    goto import_failed;

  status = PKCS12_parse (p12, openssl->password, &key, &cert, &ca);
  g_clear_pointer (&bio, BIO_free_all);

  if (status != 1)
    {
      if (ERR_GET_REASON (ERR_peek_last_error ()) == PKCS12_R_MAC_VERIFY_FAILURE)
        error_code = G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD;
      goto import_failed;
    }

  /* Clear a previous error to load without a password. */
  if (g_error_matches (openssl->construct_error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD))
    g_clear_error (&openssl->construct_error);

  if (cert)
    {
      openssl->cert = g_steal_pointer (&cert);
      openssl->have_cert = TRUE;
    }

  if (ca)
    {
      GTlsCertificateOpenssl *last_cert = openssl;

      for (guint i = 0; i < sk_X509_num (ca); )
        {
          GTlsCertificateOpenssl *new_cert;
          new_cert = G_TLS_CERTIFICATE_OPENSSL (g_tls_certificate_openssl_new_from_x509 (sk_X509_value (ca, i),
                                                                                         NULL));

          if (is_issuer (last_cert, new_cert))
            {
              g_tls_certificate_openssl_set_issuer (last_cert, new_cert);
              last_cert = new_cert;

              /* Start the list over to find an issuer of the new cert. */
              sk_X509_delete (ca, i);
              i = 0;
            }
          else
            i++;

          g_object_unref (new_cert);
        }

      sk_X509_pop_free (ca, X509_free);
      ca = NULL;
    }

  if (key)
    {
      openssl->key = g_steal_pointer (&key);
      openssl->have_key = TRUE;
    }

  g_clear_pointer (&p12, PKCS12_free);
  return;

import_failed:
  g_clear_error (&openssl->construct_error);

  if (!error_buffer[0])
    ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));

  g_set_error (&openssl->construct_error, G_TLS_ERROR, error_code,
              _("Failed to import PKCS #12: %s"), error_buffer);

  g_clear_pointer (&p12, PKCS12_free);
  g_clear_pointer (&bio, BIO_free_all);
}

static void
g_tls_certificate_openssl_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GTlsCertificateOpenssl *openssl = G_TLS_CERTIFICATE_OPENSSL (object);
  GByteArray *certificate;
  guint8 *data;
  BIO *bio;
  GByteArray *byte_array;
  char *certificate_pem;
  long size;

  const ASN1_TIME *time_asn1;
  struct tm time_tm;
  GDateTime *time;
  GTimeZone *tz;
  X509_NAME *name;
  const char *name_string;

  switch (prop_id)
    {
    case PROP_PKCS12_DATA:
      g_value_set_boxed (value, openssl->pkcs12_data);
      break;

    case PROP_CERTIFICATE:
      /* NOTE: we do the two calls to avoid openssl allocating the buffer for us */
      size = i2d_X509 (openssl->cert, NULL);
      if (size < 0)
        certificate = NULL;
      else
        {
          certificate = g_byte_array_sized_new (size);
          certificate->len = size;
          data = certificate->data;
          size = i2d_X509 (openssl->cert, &data);
          if (size < 0)
            {
              g_byte_array_free (certificate, TRUE);
              certificate = NULL;
            }
        }
      g_value_take_boxed (value, certificate);
      break;

    case PROP_CERTIFICATE_PEM:
      bio = BIO_new (BIO_s_mem ());

      if (!PEM_write_bio_X509 (bio, openssl->cert) || !BIO_write (bio, "\0", 1))
        certificate_pem = NULL;
      else
        {
          BIO_get_mem_data (bio, &certificate_pem);
          g_value_set_string (value, certificate_pem);

          BIO_free_all (bio);
        }
      break;

    case PROP_PRIVATE_KEY:
      export_privkey_to_der (openssl, &data, &size);
      if (size > 0 && (gint64)size <= G_MAXUINT)
        {
          byte_array = g_byte_array_new_take (data, size);
          g_value_take_boxed (value, byte_array);
        }
      break;

    case PROP_PRIVATE_KEY_PEM:
      g_value_take_string (value, export_privkey_to_pem (openssl));
      break;

    case PROP_ISSUER:
      g_value_set_object (value, openssl->issuer);
      break;

    case PROP_NOT_VALID_BEFORE:
      time_asn1 = X509_get0_notBefore (openssl->cert);
      ASN1_TIME_to_tm (time_asn1, &time_tm);
      tz = g_time_zone_new_utc ();
      time = g_date_time_new (tz, time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec);
      g_value_take_boxed (value, time);
      g_time_zone_unref (tz);
      break;

    case PROP_NOT_VALID_AFTER:
      time_asn1 = X509_get0_notAfter (openssl->cert);
      ASN1_TIME_to_tm (time_asn1, &time_tm);
      tz = g_time_zone_new_utc ();
      time = g_date_time_new (tz, time_tm.tm_year + 1900, time_tm.tm_mon + 1, time_tm.tm_mday, time_tm.tm_hour, time_tm.tm_min, time_tm.tm_sec);
      g_value_take_boxed (value, time);
      g_time_zone_unref (tz);
      break;

    case PROP_SUBJECT_NAME:
      bio = BIO_new (BIO_s_mem ());
      name = X509_get_subject_name (openssl->cert);
      X509_NAME_print_ex (bio, name, 0, XN_FLAG_SEP_COMMA_PLUS);
      BIO_write (bio, "\0", 1);
      BIO_get_mem_data (bio, (char **)&name_string);
      g_value_set_string (value, name_string);
      BIO_free_all (bio);
      break;

    case PROP_ISSUER_NAME:
      bio = BIO_new (BIO_s_mem ());
      name = X509_get_issuer_name (openssl->cert);
      X509_NAME_print_ex (bio, name, 0, XN_FLAG_SEP_COMMA_PLUS);
      BIO_write (bio, "\0", 1);
      BIO_get_mem_data (bio, &name_string);
      g_value_set_string (value, name_string);
      BIO_free_all (bio);
      break;

    case PROP_DNS_NAMES:
      g_value_take_boxed (value, get_subject_alt_names (openssl, GEN_DNS));
      break;

    case PROP_IP_ADDRESSES:
      g_value_take_boxed (value, get_subject_alt_names (openssl, GEN_IPADD));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

#define CRITICAL_IF_KEY_INITIALIZED(property_name) G_STMT_START \
  { \
    if (openssl->have_key) \
      { \
        g_critical ("GTlsCertificate: Failed to set construct property \"%s\" because a private key was already set earlier during construction.", property_name); \
        return; \
      } \
  } \
G_STMT_END

#define CRITICAL_IF_CERTIFICATE_INITIALIZED(property_name) G_STMT_START \
  { \
    if (openssl->have_cert) \
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
g_tls_certificate_openssl_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GTlsCertificateOpenssl *openssl = G_TLS_CERTIFICATE_OPENSSL (object);
  GByteArray *bytes;
  guint8 *data;
  BIO *bio;
  const char *string;
  char error_buffer[256];

  switch (prop_id)
    {
    case PROP_PASSWORD:
      openssl->password = g_value_dup_string (value);
      if (openssl->password)
        {
          CRITICAL_IF_INITIALIZED ("password");
          maybe_import_pkcs12 (openssl);
        }
      break;

    case PROP_PKCS12_DATA:
      openssl->pkcs12_data = g_value_dup_boxed (value);
      if (openssl->pkcs12_data)
        {
          CRITICAL_IF_INITIALIZED ("pkcs12-data");
          maybe_import_pkcs12 (openssl);
        }
      break;

    case PROP_CERTIFICATE:
      bytes = g_value_get_boxed (value);
      if (!bytes)
        break;
      CRITICAL_IF_CERTIFICATE_INITIALIZED ("certificate");
      /* see that we cannot use bytes->data directly since it will move the pointer */
      data = bytes->data;
      openssl->cert = d2i_X509 (NULL, (const unsigned char **)&data, bytes->len);
      if (openssl->cert)
        openssl->have_cert = TRUE;
      else if (!openssl->construct_error)
        {
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          openssl->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse DER certificate: %s"),
                         error_buffer);
        }

      break;

    case PROP_CERTIFICATE_PEM:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_CERTIFICATE_INITIALIZED ("certificate-pem");
      bio = BIO_new_mem_buf ((gpointer)string, -1);
      openssl->cert = PEM_read_bio_X509 (bio, NULL, NULL, NULL);
      BIO_free (bio);
      if (openssl->cert)
        openssl->have_cert = TRUE;
      else if (!openssl->construct_error)
        {
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          openssl->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse PEM certificate: %s"),
                         error_buffer);
        }
      break;

    case PROP_PRIVATE_KEY:
      bytes = g_value_get_boxed (value);
      if (!bytes)
        break;
      CRITICAL_IF_KEY_INITIALIZED ("private-key");

      bio = BIO_new_mem_buf (bytes->data, bytes->len);
      openssl->key = d2i_PrivateKey_bio (bio, NULL);
      BIO_free (bio);
      if (openssl->key)
        openssl->have_key = TRUE;
      else if (!openssl->construct_error)
        {
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          openssl->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse DER private key: %s"),
                         error_buffer);
        }
      break;

    case PROP_PRIVATE_KEY_PEM:
      string = g_value_get_string (value);
      if (!string)
        break;
      CRITICAL_IF_KEY_INITIALIZED ("private-key-pem");

      bio = BIO_new_mem_buf ((gpointer)string, -1);
      openssl->key = PEM_read_bio_PrivateKey (bio, NULL, NULL, NULL);
      BIO_free (bio);
      if (openssl->key)
        openssl->have_key = TRUE;
      else if (!openssl->construct_error)
        {
          ERR_error_string_n (ERR_get_error (), error_buffer, sizeof (error_buffer));
          openssl->construct_error =
            g_error_new (G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                         _("Could not parse PEM private key: %s"),
                         error_buffer);
        }
      break;

    case PROP_ISSUER:
      openssl->issuer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tls_certificate_openssl_init (GTlsCertificateOpenssl *openssl)
{
}

static gboolean
g_tls_certificate_openssl_initable_init (GInitable       *initable,
                                         GCancellable    *cancellable,
                                         GError         **error)
{
  GTlsCertificateOpenssl *openssl = G_TLS_CERTIFICATE_OPENSSL (initable);

  /* After init we don't need to keep the password around. */
  g_clear_pointer (&openssl->password, g_free);

  if (openssl->construct_error)
    {
      g_propagate_error (error, openssl->construct_error);
      openssl->construct_error = NULL;
      return FALSE;
    }
  else if (!openssl->have_cert)
    {
      g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE,
                           _("No certificate data provided"));
      return FALSE;
    }
  else
    return TRUE;
}

static GTlsCertificateFlags
g_tls_certificate_openssl_verify (GTlsCertificate     *cert,
                                  GSocketConnectable  *identity,
                                  GTlsCertificate     *trusted_ca)
{
  GTlsCertificateOpenssl *cert_openssl;
  GTlsCertificateFlags gtls_flags;
  X509 *x;
  STACK_OF(X509) *untrusted;

  cert_openssl = G_TLS_CERTIFICATE_OPENSSL (cert);
  x = cert_openssl->cert;

  untrusted = sk_X509_new_null ();
  for (; cert_openssl; cert_openssl = cert_openssl->issuer)
    sk_X509_push (untrusted, cert_openssl->cert);

  gtls_flags = 0;

  if (trusted_ca)
    {
      X509_STORE *store;
      X509_STORE_CTX *csc;
      STACK_OF(X509) *trusted;

      store = X509_STORE_new ();
      csc = X509_STORE_CTX_new ();

      if (!X509_STORE_CTX_init (csc, store, x, untrusted))
        {
          sk_X509_free (untrusted);
          X509_STORE_CTX_free (csc);
          X509_STORE_free (store);
          return G_TLS_CERTIFICATE_GENERIC_ERROR;
        }

      trusted = sk_X509_new_null ();
      cert_openssl = G_TLS_CERTIFICATE_OPENSSL (trusted_ca);
      for (; cert_openssl; cert_openssl = cert_openssl->issuer)
        sk_X509_push (trusted, cert_openssl->cert);

      X509_STORE_CTX_trusted_stack (csc, trusted);
      if (X509_verify_cert (csc) <= 0)
        gtls_flags |= g_tls_certificate_openssl_convert_error (X509_STORE_CTX_get_error (csc));

      sk_X509_free (trusted);
      X509_STORE_CTX_free (csc);
      X509_STORE_free (store);
    }

  sk_X509_free (untrusted);

  if (identity)
    gtls_flags |= g_tls_certificate_openssl_verify_identity (G_TLS_CERTIFICATE_OPENSSL (cert), identity);

  return gtls_flags;
}

static void
g_tls_certificate_openssl_class_init (GTlsCertificateOpensslClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GTlsCertificateClass *certificate_class = G_TLS_CERTIFICATE_CLASS (klass);

  gobject_class->get_property = g_tls_certificate_openssl_get_property;
  gobject_class->set_property = g_tls_certificate_openssl_set_property;
  gobject_class->finalize     = g_tls_certificate_openssl_finalize;

  certificate_class->verify = g_tls_certificate_openssl_verify;

  g_object_class_override_property (gobject_class, PROP_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CERTIFICATE_PEM, "certificate-pem");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY, "private-key");
  g_object_class_override_property (gobject_class, PROP_PRIVATE_KEY_PEM, "private-key-pem");
  g_object_class_override_property (gobject_class, PROP_ISSUER, "issuer");
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
g_tls_certificate_openssl_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_tls_certificate_openssl_initable_init;
}

GTlsCertificate *
g_tls_certificate_openssl_new (GBytes          *bytes,
                               GTlsCertificate *issuer)
{
  GTlsCertificateOpenssl *openssl;

  openssl = g_object_new (G_TYPE_TLS_CERTIFICATE_OPENSSL,
                          "issuer", issuer,
                          NULL);
  g_tls_certificate_openssl_set_data (openssl, bytes);

  return G_TLS_CERTIFICATE (openssl);
}

GTlsCertificate *
g_tls_certificate_openssl_new_from_x509 (X509            *x,
                                         GTlsCertificate *issuer)
{
  GTlsCertificateOpenssl *openssl;

  openssl = g_object_new (G_TYPE_TLS_CERTIFICATE_OPENSSL,
                          "issuer", issuer,
                          NULL);

  openssl->cert = X509_dup (x);
  openssl->have_cert = TRUE;

  return G_TLS_CERTIFICATE (openssl);
}

void
g_tls_certificate_openssl_set_data (GTlsCertificateOpenssl *openssl,
                                    GBytes                 *bytes)
{
  const unsigned char *data;

  g_return_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (openssl));

  g_return_if_fail (!openssl->have_cert);

  data = (const unsigned char *)g_bytes_get_data (bytes, NULL);
  openssl->cert = d2i_X509 (NULL, &data, g_bytes_get_size (bytes));

  if (openssl->cert)
    openssl->have_cert = TRUE;
}

GBytes *
g_tls_certificate_openssl_get_bytes (GTlsCertificateOpenssl *openssl)
{
  GByteArray *array;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (openssl), NULL);

  g_object_get (openssl, "certificate", &array, NULL);
  return g_byte_array_free_to_bytes (array);
}

X509 *
g_tls_certificate_openssl_get_cert (GTlsCertificateOpenssl *openssl)
{
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (openssl), FALSE);

  return openssl->cert;
}

EVP_PKEY *
g_tls_certificate_openssl_get_key (GTlsCertificateOpenssl *openssl)
{
  g_return_val_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (openssl), FALSE);

  return openssl->key;
}

void
g_tls_certificate_openssl_set_issuer (GTlsCertificateOpenssl *openssl,
                                      GTlsCertificateOpenssl *issuer)
{
  g_return_if_fail (G_IS_TLS_CERTIFICATE_OPENSSL (openssl));
  g_return_if_fail (!issuer || G_IS_TLS_CERTIFICATE_OPENSSL (issuer));

  if (g_set_object (&openssl->issuer, issuer))
    g_object_notify (G_OBJECT (openssl), "issuer");
}

static gboolean
verify_identity_hostname (GTlsCertificateOpenssl *openssl,
                          GSocketConnectable     *identity)
{
  const char *hostname;

  if (G_IS_NETWORK_ADDRESS (identity))
    hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (identity));
  else if (G_IS_NETWORK_SERVICE (identity))
    hostname = g_network_service_get_domain (G_NETWORK_SERVICE (identity));
  else
    return FALSE;

  return X509_check_host (openssl->cert, hostname, strlen (hostname), 0, NULL) == 1;
}

static gboolean
verify_identity_ip (GTlsCertificateOpenssl *openssl,
                    GSocketConnectable     *identity)
{
  GInetAddress *addr;
  gsize addr_size;
  const guint8 *addr_bytes;
  gboolean ret;

  if (G_IS_INET_SOCKET_ADDRESS (identity))
    addr = g_object_ref (g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (identity)));
  else {
    const char *hostname;

    if (G_IS_NETWORK_ADDRESS (identity))
      hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (identity));
    else if (G_IS_NETWORK_SERVICE (identity))
      hostname = g_network_service_get_domain (G_NETWORK_SERVICE (identity));
    else
      return FALSE;

    addr = g_inet_address_new_from_string (hostname);
    if (!addr)
      return FALSE;
  }

  addr_bytes = g_inet_address_to_bytes (addr);
  addr_size = g_inet_address_get_native_size (addr);

  ret = X509_check_ip (openssl->cert, addr_bytes, addr_size, 0) == 1;

  g_object_unref (addr);
  return ret;
}

GTlsCertificateFlags
g_tls_certificate_openssl_verify_identity (GTlsCertificateOpenssl *openssl,
                                           GSocketConnectable     *identity)
{
  if (verify_identity_hostname (openssl, identity))
    return 0;
  else if (verify_identity_ip (openssl, identity))
    return 0;

  /* FIXME: check sRVName and uniformResourceIdentifier
   * subjectAltNames, if appropriate for @identity.
   */

  return G_TLS_CERTIFICATE_BAD_IDENTITY;
}

GTlsCertificateFlags
g_tls_certificate_openssl_convert_error (guint openssl_error)
{
  GTlsCertificateFlags gtls_flags;

  gtls_flags = 0;

  /* FIXME: should we add more ? */
  switch (openssl_error)
    {
    case X509_V_OK:
      break;
    case X509_V_ERR_CERT_NOT_YET_VALID:
      gtls_flags = G_TLS_CERTIFICATE_NOT_ACTIVATED;
      break;
    case X509_V_ERR_CERT_HAS_EXPIRED:
      gtls_flags = G_TLS_CERTIFICATE_EXPIRED;
      break;
    case X509_V_ERR_CERT_REVOKED:
      gtls_flags = G_TLS_CERTIFICATE_REVOKED;
      break;
    case X509_V_ERR_AKID_SKID_MISMATCH:
      gtls_flags = G_TLS_CERTIFICATE_BAD_IDENTITY;
      break;
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
      gtls_flags = G_TLS_CERTIFICATE_UNKNOWN_CA;
      break;
    default:
      gtls_flags = G_TLS_CERTIFICATE_GENERIC_ERROR;
    }

  return gtls_flags;
}

static gboolean
is_issuer (GTlsCertificateOpenssl *cert,
           GTlsCertificateOpenssl *issuer)
{
  X509 *x;
  X509 *issuer_x;
  X509_STORE *store;
  X509_STORE_CTX *csc;
  STACK_OF(X509) *trusted;
  gboolean ret = FALSE;
  gint err;

  x = g_tls_certificate_openssl_get_cert (cert);
  issuer_x = g_tls_certificate_openssl_get_cert (issuer);

  store = X509_STORE_new ();
  csc = X509_STORE_CTX_new ();

  if (!X509_STORE_CTX_init (csc, store, x, NULL))
    goto end;

  trusted = sk_X509_new_null ();
  sk_X509_push (trusted, issuer_x);

  X509_STORE_CTX_trusted_stack (csc, trusted);
  X509_STORE_CTX_set_flags (csc, X509_V_FLAG_CB_ISSUER_CHECK);

  /* FIXME: is this the right way to do it? */
  if (X509_verify_cert (csc) <= 0)
    {
      err = X509_STORE_CTX_get_error (csc);
      if (err == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT)
        ret = TRUE;
    }
  else
    ret = TRUE;

  sk_X509_free (trusted);

end:
  X509_STORE_CTX_free (csc);
  X509_STORE_free (store);

  return ret;
}

GTlsCertificateOpenssl *
g_tls_certificate_openssl_build_chain (X509            *x,
                                       STACK_OF (X509) *chain)
{
  GPtrArray *glib_certs;
  GTlsCertificateOpenssl *issuer;
  GTlsCertificateOpenssl *result;
  guint i, j;

  g_return_val_if_fail (x, NULL);
  g_return_val_if_fail (chain, NULL);

  glib_certs = g_ptr_array_new_full (sk_X509_num (chain), g_object_unref);
  g_ptr_array_add (glib_certs, g_tls_certificate_openssl_new_from_x509 (x, NULL));
  for (i = 1; i < sk_X509_num (chain); i++)
    g_ptr_array_add (glib_certs, g_tls_certificate_openssl_new_from_x509 (sk_X509_value (chain, i), NULL));

  /* Some servers send certs out of order, or will send duplicate
   * certs, so we need to be careful when assigning the issuer of
   * our new GTlsCertificateOpenssl.
   */
  for (i = 0; i < glib_certs->len; i++)
    {
      issuer = NULL;

      /* Check if the cert issued itself */
      if (is_issuer (glib_certs->pdata[i], glib_certs->pdata[i]))
        continue;

      if (i < glib_certs->len - 1 &&
          is_issuer (glib_certs->pdata[i], glib_certs->pdata[i + 1]))
        {
          issuer = glib_certs->pdata[i + 1];
        }
      else
        {
          for (j = 0; j < glib_certs->len; j++)
            {
              if (j != i &&
                  is_issuer (glib_certs->pdata[i], glib_certs->pdata[j]))
                {
                  issuer = glib_certs->pdata[j];
                  break;
                }
            }
        }

      if (issuer)
        g_tls_certificate_openssl_set_issuer (glib_certs->pdata[i], issuer);
    }

  result = g_object_ref (glib_certs->pdata[0]);
  g_ptr_array_unref (glib_certs);

  return result;
}
