/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO TLS tests
 *
 * Copyright 2011 Collabora, Ltd.
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
#include "certificate.h"

#include <gio/gio.h>

#ifdef BACKEND_IS_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/pkcs11.h>
#endif

#include <sys/types.h>
#include <string.h>

static const gchar *
tls_test_file_path (const char *name)
{
  const gchar *const_path;
  gchar *path;

  path = g_test_build_filename (G_TEST_DIST, "files", name, NULL);
  if (!g_path_is_absolute (path))
    {
      gchar *cwd, *abs;

      cwd = g_get_current_dir ();
      abs = g_build_filename (cwd, path, NULL);
      g_free (cwd);
      g_free (path);
      path = abs;
    }

  const_path = g_intern_string (path);
  g_free (path);
  return const_path;
}

typedef struct {
  GTlsBackend *backend;
  GType cert_gtype;
  gchar *cert_pem;
  gsize cert_pem_length;
  GByteArray *cert_der;
  gchar *key_pem;
  gsize key_pem_length;
  gchar *key_pem_pkcs8;
  gsize key_pem_pkcs8_length;
  GByteArray *key_der;
  GByteArray *key_der_pkcs8;
} TestCertificate;

static void
setup_certificate (TestCertificate *test, gconstpointer data)
{
  GError *error = NULL;
  gchar *contents;
  gsize length;

  test->backend = g_tls_backend_get_default ();
  test->cert_gtype = g_tls_backend_get_certificate_type (test->backend);

  g_file_get_contents (tls_test_file_path ("server.pem"), &test->cert_pem,
                       &test->cert_pem_length, &error);
  g_assert_no_error (error);

  g_file_get_contents (tls_test_file_path ("server.der"),
                       &contents, &length, &error);
  g_assert_no_error (error);

  test->cert_der = g_byte_array_new ();
  g_byte_array_append (test->cert_der, (guint8 *)contents, length);
  g_free (contents);

  g_file_get_contents (tls_test_file_path ("server-key.pem"), &test->key_pem,
                       &test->key_pem_length, &error);
  g_assert_no_error (error);

  g_file_get_contents (tls_test_file_path ("server-key-pkcs8.pem"), &test->key_pem_pkcs8,
                       &test->key_pem_pkcs8_length, &error);
  g_assert_no_error (error);

  g_file_get_contents (tls_test_file_path ("server-key.der"),
                       &contents, &length, &error);
  g_assert_no_error (error);

  test->key_der = g_byte_array_new ();
  g_byte_array_append (test->key_der, (guint8 *)contents, length);
  g_free (contents);

  g_file_get_contents (tls_test_file_path ("server-key-pkcs8.der"),
                       &contents, &length, &error);
  g_assert_no_error (error);

  test->key_der_pkcs8 = g_byte_array_new ();
  g_byte_array_append (test->key_der_pkcs8, (guint8 *)contents, length);
  g_free (contents);
}

static void
teardown_certificate (TestCertificate *test,
                      gconstpointer data)
{
  g_free (test->cert_pem);
  g_byte_array_free (test->cert_der, TRUE);

  g_free (test->key_pem);
  g_free (test->key_pem_pkcs8);
  g_byte_array_free (test->key_der, TRUE);
  g_byte_array_free (test->key_der_pkcs8, TRUE);
}

static void
test_create_pem (TestCertificate *test,
                 gconstpointer data)
{
  GTlsCertificate *cert;
  gchar *pem = NULL;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_pem (test->cert_pem, test->cert_pem_length, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_get (cert, "certificate-pem", &pem, NULL);
  g_assert_cmpstr (pem, ==, test->cert_pem);
  g_free (pem);

  g_object_add_weak_pointer (G_OBJECT (cert), (gpointer *)&cert);
  g_object_unref (cert);
  g_assert_null (cert);
}

static void
test_create_with_key_pem (TestCertificate *test,
                          gconstpointer data)
{
  GTlsCertificate *cert;
  GError *error = NULL;

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "certificate-pem", test->cert_pem,
                         "private-key-pem", test->key_pem,
                         NULL);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_add_weak_pointer (G_OBJECT (cert), (gpointer *)&cert);
  g_object_unref (cert);
  g_assert_null (cert);
}

static void
test_create_der (TestCertificate *test,
                 gconstpointer data)
{
  GTlsCertificate *cert;
  GByteArray *der = NULL;
  GError *error = NULL;

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "certificate", test->cert_der,
                         NULL);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_get (cert, "certificate", &der, NULL);
  g_assert_nonnull (der);
  g_assert_cmpuint (der->len, ==, test->cert_der->len);
  g_assert_cmpint (memcmp (der->data, test->cert_der->data, der->len), ==, 0);

  g_byte_array_unref (der);

  g_object_add_weak_pointer (G_OBJECT (cert), (gpointer *)&cert);
  g_object_unref (cert);
  g_assert_null (cert);
}

static void
test_create_with_key_der (TestCertificate *test,
                          gconstpointer data)
{
  GTlsCertificate *cert;
  GError *error = NULL;

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "certificate", test->cert_der,
                         "private-key", test->key_der,
                         NULL);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_add_weak_pointer (G_OBJECT (cert), (gpointer *)&cert);
  g_object_unref (cert);
  g_assert_null (cert);
}

static void
test_create_certificate_with_issuer (TestCertificate   *test,
                                     gconstpointer      data)
{
  GTlsCertificate *cert, *issuer, *check;
  GError *error = NULL;

  issuer = g_tls_certificate_new_from_file (tls_test_file_path ("ca.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (issuer));

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "certificate-pem", test->cert_pem,
                         "issuer", issuer,
                         NULL);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_add_weak_pointer (G_OBJECT (issuer), (gpointer *)&issuer);
  g_object_unref (issuer);
  g_assert_nonnull (issuer);

  check = g_tls_certificate_get_issuer (cert);
  g_assert_true (check == issuer);

  g_object_add_weak_pointer (G_OBJECT (cert), (gpointer *)&cert);
  g_object_unref (cert);
  g_assert_null (cert);
  g_assert_null (issuer);
}

static void
test_create_certificate_with_garbage_input (TestCertificate *test,
                                            gconstpointer data)
{
  GTlsCertificate *cert;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("garbage.pem"), &error);
  g_assert_null (cert);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE);
  g_clear_error (&error);

  cert = g_tls_certificate_new_from_pem ("I am not a very good certificate.", -1, &error);
  g_assert_null (cert);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE);
  g_clear_error (&error);
}

static void
test_create_certificate_pkcs11 (TestCertificate *test,
                                gconstpointer data)
{
#if !defined (BACKEND_IS_GNUTLS)
  g_test_skip ("This backend does not support PKCS #11");
#else
  GTlsCertificate *cert;
  GError *error = NULL;

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "pkcs11-uri", "pkcs11:model=mock;token=Mock%20Certificate;object=Mock%20Certificate",
                         NULL);

  g_assert_no_error (error);
  g_assert_nonnull (cert);
#endif
}

static void
test_private_key (TestCertificate *test,
                  gconstpointer    data)
{
  GTlsCertificate *cert;
  GByteArray *der;
  char *pem;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server-and-key.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  g_object_get (cert,
                "private-key", &der,
                "private-key-pem", &pem,
                NULL);
  g_assert_cmpmem (der->data, der->len, test->key_der_pkcs8->data, test->key_der_pkcs8->len);
  g_assert_cmpstr (pem, ==, test->key_pem_pkcs8);

  g_byte_array_unref (der);
  g_free (pem);
  g_object_unref (cert);
}

static void
test_private_key_pkcs11 (TestCertificate *test,
                         gconstpointer    data)
{
#if !defined (BACKEND_IS_GNUTLS)
  g_test_skip ("This backend does not support PKCS #11");
#else
  GTlsCertificate *cert;
  GByteArray *der;
  char *pem;
  GError *error = NULL;

  cert = g_initable_new (test->cert_gtype, NULL, &error,
                         "pkcs11-uri", "pkcs11:model=mock;token=Mock%20Certificate;object=Mock%20Certificate",
                         NULL);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  /* Cannot access private key because the GTlsCertificate only knows its
   * PKCS #11 handle. It doesn't actually have the private key in memory.
   */
  g_object_get (cert,
                "private-key", &der,
                "private-key-pem", &pem,
                NULL);
  g_assert_null (der);
  g_assert_null (pem);

  g_object_unref (cert);
#endif
}

static void
test_create_certificate_chain (void)
{
  GTlsCertificate *cert, *intermediate, *root;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("chain.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  intermediate = g_tls_certificate_get_issuer (cert);
  g_assert_true (G_IS_TLS_CERTIFICATE (intermediate));

  root = g_tls_certificate_get_issuer (intermediate);
  g_assert_true (G_IS_TLS_CERTIFICATE (root));

  g_assert_null (g_tls_certificate_get_issuer (root));

  g_object_unref (cert);
}

static void
test_create_certificate_no_chain (void)
{
  GTlsCertificate *cert, *issuer;
  GError *error = NULL;
  gchar *cert_pem;
  gsize cert_pem_length;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("non-ca.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  issuer = g_tls_certificate_get_issuer (cert);
  g_assert_null (issuer);
  g_object_unref (cert);

  /* Truncate a valid chain certificate file. We should only get the
   * first certificate.
   */
  g_file_get_contents (tls_test_file_path ("chain.pem"), &cert_pem,
                       &cert_pem_length, &error);
  g_assert_no_error (error);

  cert = g_tls_certificate_new_from_pem (cert_pem, cert_pem_length - 100, &error);
  g_free (cert_pem);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  issuer = g_tls_certificate_get_issuer (cert);
  g_assert_null (issuer);
  g_object_unref (cert);
}

static void
test_create_list (void)
{
  GList *list;
  GError *error = NULL;

  list = g_tls_certificate_list_new_from_file (tls_test_file_path ("ca-roots.pem"), &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_list_length (list), ==, 8);

  g_list_free_full (list, g_object_unref);
}

static void
test_create_list_bad (void)
{
  GList *list;
  GError *error = NULL;

  list = g_tls_certificate_list_new_from_file (tls_test_file_path ("ca-roots-bad.pem"), &error);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE);
  g_assert_null (list);
  g_error_free (error);
}

/* -----------------------------------------------------------------------------
 * CERTIFICATE VERIFY
 */

typedef struct {
  GTlsCertificate *cert;
  GTlsCertificate *anchor;
  GSocketConnectable *identity;
  GTlsDatabase *database;
} TestVerify;

static void
setup_verify (TestVerify     *test,
              gconstpointer   data)
{
  GError *error = NULL;

  test->cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (test->cert));

  test->identity = g_network_address_new ("server.example.com", 80);

  test->anchor = g_tls_certificate_new_from_file (tls_test_file_path ("ca.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (test->anchor));
  test->database = g_tls_file_database_new (tls_test_file_path ("ca.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_DATABASE (test->database));
}

static void
teardown_verify (TestVerify      *test,
                 gconstpointer    data)
{
  g_assert_true (G_IS_TLS_CERTIFICATE (test->cert));
  g_object_add_weak_pointer (G_OBJECT (test->cert),
                             (gpointer *)&test->cert);
  g_object_unref (test->cert);
  g_assert_null (test->cert);

  g_assert_true (G_IS_TLS_CERTIFICATE (test->anchor));
  g_object_add_weak_pointer (G_OBJECT (test->anchor),
                             (gpointer *)&test->anchor);
  g_object_unref (test->anchor);
  g_assert_null (test->anchor);

  g_assert_true (G_IS_TLS_DATABASE (test->database));
  g_object_add_weak_pointer (G_OBJECT (test->database),
                             (gpointer *)&test->database);
  g_object_unref (test->database);
  g_assert_null (test->database);

  g_object_add_weak_pointer (G_OBJECT (test->identity),
                             (gpointer *)&test->identity);
  g_object_unref (test->identity);
  g_assert_null (test->identity);
}

static void
test_verify_certificate_good (TestVerify      *test,
                              gconstpointer    data)
{
  GSocketConnectable *identity;
  GSocketAddress *addr;
  GTlsCertificateFlags errors;

  errors = g_tls_certificate_verify (test->cert, test->identity, test->anchor);
  g_assert_cmpuint (errors, ==, 0);

  errors = g_tls_certificate_verify (test->cert, NULL, test->anchor);
  g_assert_cmpuint (errors, ==, 0);

  identity = g_network_address_new ("192.168.1.10", 80);
  errors = g_tls_certificate_verify (test->cert, identity, test->anchor);
  g_assert_cmpuint (errors, ==, 0);
  g_object_unref (identity);

  addr = g_inet_socket_address_new_from_string ("192.168.1.10", 80);
  errors = g_tls_certificate_verify (test->cert, G_SOCKET_CONNECTABLE (addr), test->anchor);
  g_assert_cmpuint (errors, ==, 0);
  g_object_unref (addr);
}

static void
test_verify_certificate_bad_identity (TestVerify      *test,
                                      gconstpointer    data)
{
  GSocketConnectable *identity;
  GTlsCertificateFlags errors;
  GSocketAddress *addr;

  identity = g_network_address_new ("other.example.com", 80);
  errors = g_tls_certificate_verify (test->cert, identity, test->anchor);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_BAD_IDENTITY);
  g_object_unref (identity);

  identity = g_network_address_new ("127.0.0.1", 80);
  errors = g_tls_certificate_verify (test->cert, identity, test->anchor);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_BAD_IDENTITY);
  g_object_unref (identity);

  addr = g_inet_socket_address_new_from_string ("127.0.0.1", 80);
  errors = g_tls_certificate_verify (test->cert, G_SOCKET_CONNECTABLE (addr), test->anchor);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_BAD_IDENTITY);
  g_object_unref (addr);
}

static void
test_verify_certificate_bad_ca (TestVerify      *test,
                                gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* Use a client certificate as the CA, which is wrong */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_certificate_verify (test->cert, test->identity, cert);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA);

  g_object_unref (cert);
}

static void
test_verify_certificate_bad_before (TestVerify      *test,
                                    gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* This is a certificate in the future */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-future.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_certificate_verify (cert, NULL, test->anchor);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_NOT_ACTIVATED);

  g_object_unref (cert);
}

static void
test_verify_certificate_bad_expired (TestVerify      *test,
                                     gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* This is a certificate in the future */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-past.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_certificate_verify (cert, NULL, test->anchor);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_EXPIRED);

  g_object_unref (cert);
}

static void
test_verify_certificate_bad_combo (TestVerify      *test,
                                   gconstpointer    data)
{
  GTlsCertificate *cert;
  GTlsCertificate *cacert;
  GSocketConnectable *identity;
  GTlsCertificateFlags errors;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-past.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  /* Unrelated cert used as certificate authority */
  cacert = g_tls_certificate_new_from_file (tls_test_file_path ("server-self.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cacert));

  /*
   * - Use unrelated cert as CA
   * - Use wrong identity.
   * - Use expired certificate.
   *
   * Once upon a time, we might have asserted to see that all of these errors
   * are set. But this is impossible to do correctly, so nowadays we only
   * guarantee that at least one error will be set. See glib-networking#179 and
   * glib!2214 for rationale.
   */

  identity = g_network_address_new ("other.example.com", 80);

  errors = g_tls_certificate_verify (cert, identity, cacert);
  g_assert_cmpuint (errors, !=, 0);

  g_object_unref (cert);
  g_object_unref (cacert);
  g_object_unref (identity);
}

static void
test_certificate_is_same (void)
{
  GTlsCertificate *one;
  GTlsCertificate *two;
  GTlsCertificate *three;
  GError *error = NULL;

  one = g_tls_certificate_new_from_file (tls_test_file_path ("client.pem"), &error);
  g_assert_no_error (error);

  two = g_tls_certificate_new_from_file (tls_test_file_path ("client-and-key.pem"), &error);
  g_assert_no_error (error);

  three = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);

  g_assert_true (g_tls_certificate_is_same (one, two));
  g_assert_true (g_tls_certificate_is_same (two, one));
  g_assert_false (g_tls_certificate_is_same (three, one));
  g_assert_false (g_tls_certificate_is_same (one, three));
  g_assert_false (g_tls_certificate_is_same (two, three));
  g_assert_false (g_tls_certificate_is_same (three, two));

  g_object_unref (one);
  g_object_unref (two);
  g_object_unref (three);
}

static void
test_certificate_not_valid_before (void)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  GDateTime *actual;
  gchar *actual_str;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);

  actual = g_tls_certificate_get_not_valid_before (cert);
  g_assert_nonnull (actual);
  actual_str = g_date_time_format_iso8601 (actual);
  g_assert_cmpstr (actual_str, ==, EXPECTED_NOT_VALID_BEFORE);
  g_free (actual_str);
  g_date_time_unref (actual);
  g_object_unref (cert);
}

/* On 32-bit, GNUTLS caps expiry times at 2037-12-31 23:23:23 to avoid
 * overflowing time_t. Hopefully by 2037, either 32-bit will finally have
 * died out, or GNUTLS will rethink its approach to
 * https://gitlab.com/gnutls/gnutls/-/issues/370 */
#define GNUTLS_32_BIT_NOT_VALID_AFTER_MAX 2145914603

static void
test_certificate_not_valid_after (void)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  GDateTime *actual;
  gchar *actual_str;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);

  actual = g_tls_certificate_get_not_valid_after (cert);
  g_assert_nonnull (actual);
  actual_str = g_date_time_format_iso8601 (actual);

#if SIZEOF_TIME_T <= 4
  if (g_date_time_to_unix (actual) == GNUTLS_32_BIT_NOT_VALID_AFTER_MAX)
    g_test_incomplete ("not-valid-after date not representable on 32-bit");
  else
    g_assert_cmpstr (actual_str, ==, EXPECTED_NOT_VALID_AFTER);
#else
  g_assert_cmpstr (actual_str, ==, EXPECTED_NOT_VALID_AFTER);
#endif

  g_free (actual_str);
  g_date_time_unref (actual);
  g_object_unref (cert);
}

static void
test_certificate_subject_name (void)
{
  const char *EXPECTED_SUBJECT_NAME = "DC=COM,DC=EXAMPLE,CN=server.example.com";
  GTlsCertificate *cert;
  GError *error = NULL;
  gchar *actual;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);

  actual = g_tls_certificate_get_subject_name (cert);
  g_assert_nonnull (actual);
  g_assert_cmpstr (actual, ==, EXPECTED_SUBJECT_NAME);
  g_free (actual);
  g_object_unref (cert);
}

static void
test_certificate_issuer_name (void)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  gchar *actual;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);

  actual = g_tls_certificate_get_issuer_name (cert);
  g_assert_nonnull (actual);
  // For GnuTLS the full string includes ",EMAIL=ca@example.com" at the end while
  // OpenSSL includes ",emailAddress=ca@example.com" at the end
  g_assert (strstr (actual, "DC=COM,DC=EXAMPLE,OU=Certificate Authority,CN=ca.example.com"));
  g_free (actual);
  g_object_unref (cert);
}

static void
test_certificate_dns_names (void)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  GPtrArray *actual;
  const gchar *dns_name = "server.example.com";
  GBytes *expected = g_bytes_new_static (dns_name, strlen (dns_name));

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);
  g_assert_nonnull (cert);

  actual = g_tls_certificate_get_dns_names (cert);
  g_assert_nonnull (actual);
  g_assert_cmpuint (actual->len, ==, 1);
  g_assert_true (g_ptr_array_find_with_equal_func (actual, expected, (GEqualFunc)g_bytes_equal, NULL));

  g_ptr_array_free (actual, TRUE);
  g_bytes_unref (expected);
  g_object_unref (cert);
}

static void
test_certificate_ip_addresses (void)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  GPtrArray *actual;
  GInetAddress *expected = g_inet_address_new_from_string ("192.168.1.10");

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server.pem"), &error);
  g_assert_no_error (error);
  g_assert_nonnull (cert);

  actual = g_tls_certificate_get_ip_addresses (cert);
  g_assert_nonnull (actual);
  g_assert_cmpuint (actual->len, ==, 1);
  g_assert_true (g_ptr_array_find_with_equal_func (actual, expected, (GEqualFunc)g_inet_address_equal, NULL));

  g_ptr_array_free (actual, TRUE);
  g_object_unref (expected);
  g_object_unref (cert);
}

static GByteArray *
load_bytes_for_test_file (const char *filename)
{
  GFile *file = g_file_new_for_path (tls_test_file_path (filename));
  GBytes *bytes = g_file_load_bytes (file, NULL, NULL, NULL);

  g_assert_nonnull (bytes);
  g_object_unref (file);
  return g_bytes_unref_to_array (bytes);
}

static void
assert_cert_contains_cert_and_key (GTlsCertificate *certificate)
{
  char *cert_pem, *key_pem;

  g_object_get (certificate,
                "certificate-pem", &cert_pem,
                "private-key-pem", &key_pem,
                NULL);

  g_assert_nonnull (cert_pem);
  g_assert_nonnull (key_pem);

  g_free (cert_pem);
  g_free (key_pem);
}

static void
assert_equals_original_cert (GTlsCertificate *cert)
{
  GTlsCertificate *original_cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-and-key.pem"), NULL);
  g_assert_nonnull (original_cert);
  g_assert_true (g_tls_certificate_is_same (original_cert, cert));
  g_object_unref (original_cert);
}

static void
test_certificate_pkcs12_basic (void)
{
  GTlsCertificate *cert;
  GByteArray *pkcs12_data;
  GError *error = NULL;

  pkcs12_data = load_bytes_for_test_file ("client-and-key.p12");
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_data->data, pkcs12_data->len, NULL, &error);

  g_assert_no_error (error);
  g_assert_nonnull (cert);
  assert_cert_contains_cert_and_key (cert);
  assert_equals_original_cert (cert);

  g_byte_array_unref (pkcs12_data);
  g_object_unref (cert);
}

static void
test_certificate_pkcs12_password (void)
{
  GTlsCertificate *cert;
  GByteArray *pkcs12_data;
  GError *error = NULL;

  pkcs12_data = load_bytes_for_test_file ("client-and-key-password.p12");

  /* Without a password it fails. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_data->data, pkcs12_data->len, NULL, &error);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD);
  g_clear_error (&error);

  /* With the wrong password it fails. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_data->data, pkcs12_data->len, "oajfo", &error);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD);
  g_clear_error (&error);

  /* With the correct password it succeeds. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_data->data, pkcs12_data->len, "1234", &error);
  g_assert_no_error (error);
  g_assert_nonnull (cert);
  assert_cert_contains_cert_and_key (cert);
  assert_equals_original_cert (cert);
  g_object_unref (cert);
  g_byte_array_unref (pkcs12_data);
}

static void
test_certificate_pkcs12_encrypted (void)
{
  GTlsCertificate *cert;
  GByteArray *pkcs12_enc_data;
  GError *error = NULL;

  pkcs12_enc_data = load_bytes_for_test_file ("client-and-key-password-enckey.p12");

  /* Without a password it fails. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_enc_data->data, pkcs12_enc_data->len, NULL, &error);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD);
  g_clear_error (&error);

  /* With the wrong password it fails. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_enc_data->data, pkcs12_enc_data->len, "oajfo", &error);
  g_assert_error (error, G_TLS_ERROR, G_TLS_ERROR_BAD_CERTIFICATE_PASSWORD);
  g_clear_error (&error);

  /* With the correct password it succeeds. */
  cert = g_tls_certificate_new_from_pkcs12 (pkcs12_enc_data->data, pkcs12_enc_data->len, "1234", &error);
  g_assert_no_error (error);
  g_assert_nonnull (cert);
  assert_cert_contains_cert_and_key (cert);
  assert_equals_original_cert (cert);
  g_object_unref (cert);
  g_byte_array_unref (pkcs12_enc_data);
}

int
main (int   argc,
      char *argv[])
{
#if defined(BACKEND_IS_GNUTLS) && HAVE_GNUTLS_PKCS11
  char *module_path;
#endif

  g_test_init (&argc, &argv, NULL);

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("GIO_USE_TLS", BACKEND, TRUE);
  g_assert_cmpint (g_ascii_strcasecmp (G_OBJECT_TYPE_NAME (g_tls_backend_get_default ()), "GTlsBackend" BACKEND), ==, 0);

#if defined(BACKEND_IS_GNUTLS) && HAVE_GNUTLS_PKCS11
  module_path = g_test_build_filename (G_TEST_BUILT, "mock-pkcs11.so", NULL);
  g_assert_true (g_file_test (module_path, G_FILE_TEST_EXISTS));

  g_assert (gnutls_pkcs11_init (GNUTLS_PKCS11_FLAG_MANUAL, NULL) == GNUTLS_E_SUCCESS);
  g_assert (gnutls_pkcs11_add_provider (module_path, NULL) == GNUTLS_E_SUCCESS);
  g_free (module_path);
#endif

  g_test_add ("/tls/" BACKEND "/certificate/create-pem", TestCertificate, NULL,
              setup_certificate, test_create_pem, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/create-der", TestCertificate, NULL,
              setup_certificate, test_create_der, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/create-with-key-pem", TestCertificate, NULL,
              setup_certificate, test_create_with_key_pem, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/create-with-key-der", TestCertificate, NULL,
              setup_certificate, test_create_with_key_der, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/create-with-issuer", TestCertificate, NULL,
              setup_certificate, test_create_certificate_with_issuer, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/create-with-garbage-input", TestCertificate, NULL,
              setup_certificate, test_create_certificate_with_garbage_input, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/private-key", TestCertificate, NULL,
              setup_certificate, test_private_key, teardown_certificate);
#if HAVE_GNUTLS_PKCS11
  g_test_add ("/tls/" BACKEND "/certificate/pkcs11", TestCertificate, NULL,
              setup_certificate, test_create_certificate_pkcs11, teardown_certificate);
  g_test_add ("/tls/" BACKEND "/certificate/private-key-pkcs11", TestCertificate, NULL,
              setup_certificate, test_private_key_pkcs11, teardown_certificate);
#endif

  g_test_add_func ("/tls/" BACKEND "/certificate/create-chain", test_create_certificate_chain);
  g_test_add_func ("/tls/" BACKEND "/certificate/create-no-chain", test_create_certificate_no_chain);
  g_test_add_func ("/tls/" BACKEND "/certificate/create-list", test_create_list);
  g_test_add_func ("/tls/" BACKEND "/certificate/create-list-bad", test_create_list_bad);

  g_test_add ("/tls/" BACKEND "/certificate/verify-good", TestVerify, NULL,
              setup_verify, test_verify_certificate_good, teardown_verify);
  g_test_add ("/tls/" BACKEND "/certificate/verify-bad-identity", TestVerify, NULL,
              setup_verify, test_verify_certificate_bad_identity, teardown_verify);
  g_test_add ("/tls/" BACKEND "/certificate/verify-bad-ca", TestVerify, NULL,
              setup_verify, test_verify_certificate_bad_ca, teardown_verify);
  g_test_add ("/tls/" BACKEND "/certificate/verify-bad-before", TestVerify, NULL,
              setup_verify, test_verify_certificate_bad_before, teardown_verify);
  g_test_add ("/tls/" BACKEND "/certificate/verify-bad-expired", TestVerify, NULL,
              setup_verify, test_verify_certificate_bad_expired, teardown_verify);
  g_test_add ("/tls/" BACKEND "/certificate/verify-bad-combo", TestVerify, NULL,
              setup_verify, test_verify_certificate_bad_combo, teardown_verify);

  g_test_add_func ("/tls/" BACKEND "/certificate/is-same", test_certificate_is_same);

  g_test_add_func ("/tls/" BACKEND "/certificate/not-valid-before", test_certificate_not_valid_before);
  g_test_add_func ("/tls/" BACKEND "/certificate/not-valid-after", test_certificate_not_valid_after);
  g_test_add_func ("/tls/" BACKEND "/certificate/subject-name", test_certificate_subject_name);
  g_test_add_func ("/tls/" BACKEND "/certificate/issuer-name", test_certificate_issuer_name);
  g_test_add_func ("/tls/" BACKEND "/certificate/dns-names", test_certificate_dns_names);
  g_test_add_func ("/tls/" BACKEND "/certificate/ip-addresses", test_certificate_ip_addresses);

  g_test_add_func ("/tls/" BACKEND "/certificate/pkcs12/basic", test_certificate_pkcs12_basic);
  g_test_add_func ("/tls/" BACKEND "/certificate/pkcs12/password", test_certificate_pkcs12_password);
  g_test_add_func ("/tls/" BACKEND "/certificate/pkcs12/encrypted", test_certificate_pkcs12_encrypted);

  return g_test_run();
}
