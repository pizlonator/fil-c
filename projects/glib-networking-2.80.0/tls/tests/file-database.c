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

#include "file-database.h"

#include <gio/gio.h>

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

/* -----------------------------------------------------------------------------
 * CERTIFICATE VERIFY
 */

typedef struct {
  GTlsCertificate *cert;
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
test_verify_database_good (TestVerify      *test,
                           gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GError *error = NULL;

  errors = g_tls_database_verify_chain (test->database, test->cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        test->identity, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, 0);

  errors = g_tls_database_verify_chain (test->database, test->cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        NULL, NULL, 0, NULL, &error);
  g_assert_cmpuint (errors, ==, 0);
}

static void
test_verify_database_bad_identity (TestVerify      *test,
                                   gconstpointer    data)
{
  GSocketConnectable *identity;
  GTlsCertificateFlags errors;
  GError *error = NULL;

  identity = g_network_address_new ("other.example.com", 80);

  errors = g_tls_database_verify_chain (test->database, test->cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        identity, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_BAD_IDENTITY);

  g_object_unref (identity);
}

static void
test_verify_database_bad_ca (TestVerify      *test,
                             gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* Use another certificate which isn't in our CA list */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server-self.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_database_verify_chain (test->database, cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        test->identity, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA);

  g_object_unref (cert);
}

static void
test_verify_database_bad_before (TestVerify      *test,
                                 gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* This is a certificate in the future */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-future.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_database_verify_chain (test->database, cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        NULL, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_NOT_ACTIVATED);

  g_object_unref (cert);
}

static void
test_verify_database_bad_expired (TestVerify      *test,
                                  gconstpointer    data)
{
  GTlsCertificateFlags errors;
  GTlsCertificate *cert;
  GError *error = NULL;

  /* This is a certificate in the future */
  cert = g_tls_certificate_new_from_file (tls_test_file_path ("client-past.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  errors = g_tls_database_verify_chain (test->database, cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        NULL, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_EXPIRED);

  g_object_unref (cert);
}

static void
test_verify_database_bad_combo (TestVerify      *test,
                                gconstpointer    data)
{
  GTlsCertificate *cert;
  GSocketConnectable *identity;
  GTlsCertificateFlags errors;
  GError *error = NULL;

  cert = g_tls_certificate_new_from_file (tls_test_file_path ("server-self.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (cert));

  /*
   * - Use is self signed
   * - Use wrong identity.
   */

  identity = g_network_address_new ("other.example.com", 80);

  errors = g_tls_database_verify_chain (test->database, cert,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        identity, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (errors, ==, G_TLS_CERTIFICATE_UNKNOWN_CA |
                    G_TLS_CERTIFICATE_BAD_IDENTITY);

  g_object_unref (cert);
  g_object_unref (identity);
}

static GTlsCertificate *
load_certificate_chain (const char  *filename,
                        GError     **error)
{
  GList *certificates;
  GTlsCertificate *chain = NULL, *prev_chain = NULL;
  GTlsBackend *backend;
  GByteArray *der;
  GList *l;

  certificates = g_tls_certificate_list_new_from_file (filename, error);
  if (!certificates)
    return NULL;

  backend = g_tls_backend_get_default ();
  certificates = g_list_reverse (certificates);
  for (l = certificates; l; l = g_list_next (l))
    {
      prev_chain = chain;
      g_object_get (l->data, "certificate", &der, NULL);
      chain = g_object_new (g_tls_backend_get_certificate_type (backend),
                            "certificate", der,
                            "issuer", prev_chain,
                            NULL);
      g_byte_array_unref (der);
      g_clear_object (&prev_chain);
    }

  g_list_free_full (certificates, g_object_unref);
  return chain;
}

static gboolean
is_certificate_in_chain (GTlsCertificate *chain,
                         GTlsCertificate *cert)
{
  while (chain)
    {
      if (g_tls_certificate_is_same (chain, cert))
        return TRUE;
      chain = g_tls_certificate_get_issuer (chain);
    }

  return FALSE;
}

static void
test_verify_with_incorrect_root_in_chain (void)
{
  GTlsCertificate *ca_verisign_sha1;
  GTlsDatabase *database;
  GError *error = NULL;
  GTlsCertificate *chain;
  GSocketConnectable *identity;
  GTlsCertificateFlags errors;

  /*
   * This database contains a single anchor certificate of:
   * C = US, O = "VeriSign, Inc.", OU = Class 3 Public Primary Certification Authority
   */
  database = g_tls_file_database_new (tls_test_file_path ("ca-verisign-sha1.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_DATABASE (database));

  ca_verisign_sha1 = g_tls_certificate_new_from_file (tls_test_file_path ("ca-verisign-sha1.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (ca_verisign_sha1));

  /*
   * This certificate chain contains a root certificate with that same issuer, public key:
   * C = US, O = "VeriSign, Inc.", OU = Class 3 Public Primary Certification Authority
   *
   * But it is not the same certificate in our database. However our database should
   * verify this chain as valid, since the issuer fields and signatures should chain up
   * to the certificate in our database.
   */
  chain = load_certificate_chain (tls_test_file_path ("chain-with-verisign-md2.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (chain));

  g_assert_nonnull (g_tls_certificate_get_issuer (chain));
  g_assert_nonnull (g_tls_certificate_get_issuer (g_tls_certificate_get_issuer (chain)));
  g_assert_true (is_certificate_in_chain (chain, chain));
  g_assert_false (is_certificate_in_chain (chain, ca_verisign_sha1));


  identity = g_network_address_new ("secure-test.streamline-esolutions.com", 443);

  errors = g_tls_database_verify_chain (database, chain,
                                        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
                                        identity, NULL, 0, NULL, &error);
  g_assert_no_error (error);
  errors &= ~G_TLS_CERTIFICATE_EXPIRED; /* so that this test doesn't expire */
  errors &= ~G_TLS_CERTIFICATE_INSECURE; /* allow MD2 */
  g_assert_cmpuint (errors, ==, 0);

  g_object_unref (chain);
  g_object_unref (ca_verisign_sha1);
  g_object_unref (identity);
  g_object_unref (database);
}

/* -----------------------------------------------------------------------------
 * FILE DATABASE
 */

typedef struct {
  GTlsDatabase *database;
  const gchar *path;
} TestFileDatabase;

static void
setup_file_database (TestFileDatabase *test,
                     gconstpointer     data)
{
  GError *error = NULL;

  test->path = tls_test_file_path ("ca-roots.pem");
  test->database = g_tls_file_database_new (test->path, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_DATABASE (test->database));
}

static void
teardown_file_database (TestFileDatabase *test,
                        gconstpointer     data)
{
  g_assert_true (G_IS_TLS_DATABASE (test->database));
  g_object_add_weak_pointer (G_OBJECT (test->database),
                             (gpointer *)&test->database);
  g_object_unref (test->database);
  g_assert_null (test->database);
}

static void
test_file_database_handle (TestFileDatabase *test,
                           gconstpointer     unused)
{
  GTlsCertificate *certificate;
  GTlsCertificate *check;
  GError *error = NULL;
  gchar *handle;

  /*
   * ca.pem is in the ca-roots.pem that the test->database represents.
   * So it should be able to create a handle for it and treat it as if it
   * is 'in' the database.
   */

  certificate = g_tls_certificate_new_from_file (tls_test_file_path ("ca.pem"), &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));

  handle = g_tls_database_create_certificate_handle (test->database, certificate);
  g_assert_nonnull (handle);
  g_assert_true (g_str_has_prefix (handle, "file:///"));

  check = g_tls_database_lookup_certificate_for_handle (test->database, handle,
                                                        NULL, G_TLS_DATABASE_LOOKUP_NONE,
                                                        NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CERTIFICATE (check));

  g_free (handle);
  g_object_unref (check);
  g_object_unref (certificate);
}

static void
test_file_database_handle_invalid (TestFileDatabase *test,
                                   gconstpointer     unused)
{
  GTlsCertificate *certificate;
  GError *error = NULL;

  certificate = g_tls_database_lookup_certificate_for_handle (test->database, "blah:blah",
                                                              NULL, G_TLS_DATABASE_LOOKUP_NONE,
                                                              NULL, &error);
  g_assert_no_error (error);
  g_assert_null (certificate);
}

/* -----------------------------------------------------------------------------
 * DATABASE
 */

static void
test_anchors_property (void)
{
  GTlsDatabase *database;
  gchar *anchor_filename = NULL;
  GError *error = NULL;

  database = g_tls_file_database_new (tls_test_file_path ("ca.pem"), &error);
  g_assert_no_error (error);

  g_object_get (database, "anchors", &anchor_filename, NULL);
  g_assert_cmpstr (anchor_filename, ==, tls_test_file_path ("ca.pem"));
  g_free (anchor_filename);

  g_object_unref (database);
}

static gboolean
certificate_is_in_list (GList *certificates,
                        const gchar *filename)
{
  GTlsCertificate *cert;
  GError *error = NULL;
  GList *l;

  cert = g_tls_certificate_new_from_file (filename, &error);
  g_assert_no_error (error);

  for (l = certificates; l; l = g_list_next (l))
    {
      if (g_tls_certificate_is_same (l->data, cert))
        break;
    }

  g_object_unref (cert);

  /* Had an early break from loop */
  return l != NULL;
}

static void
test_lookup_certificates_issued_by (void)
{
  /* This data is generated from the update-test-database.py script */
  const guchar ISSUER[] = ISSUER_DATA;

  GList *certificates;
  GByteArray *issuer_dn;
  GTlsDatabase *database;
  GError *error = NULL;

  database = g_tls_file_database_new (tls_test_file_path ("non-ca.pem"), &error);
  g_assert_no_error (error);

  issuer_dn = g_byte_array_new ();
  /* The null terminator is in the array/string above */
  g_byte_array_append (issuer_dn, ISSUER, G_N_ELEMENTS (ISSUER) - 1);

  certificates = g_tls_database_lookup_certificates_issued_by (database, issuer_dn, NULL,
                                                               G_TLS_DATABASE_LOOKUP_NONE,
                                                               NULL, &error);

  g_byte_array_unref (issuer_dn);

  g_assert_cmpuint (g_list_length (certificates), ==, 4);

  g_assert_true (certificate_is_in_list (certificates, tls_test_file_path ("client.pem")));
  g_assert_true (certificate_is_in_list (certificates, tls_test_file_path ("client-future.pem")));
  g_assert_true (certificate_is_in_list (certificates, tls_test_file_path ("client-past.pem")));
  g_assert_true (certificate_is_in_list (certificates, tls_test_file_path ("server.pem")));
  g_assert_false (certificate_is_in_list (certificates, tls_test_file_path ("server-self.pem")));

  g_list_free_full (certificates, g_object_unref);
  g_object_unref (database);
}

static void
test_default_database_is_singleton (void)
{
  GTlsBackend *backend;
  GTlsDatabase *database;
  GTlsDatabase *check;

  backend = g_tls_backend_get_default ();
  g_assert_true (G_IS_TLS_BACKEND (backend));

  database = g_tls_backend_get_default_database (backend);
  g_assert_true (G_IS_TLS_DATABASE (database));

  check = g_tls_backend_get_default_database (backend);
  g_assert_true (database == check);

  g_object_unref (database);
  g_object_unref (check);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("GIO_USE_TLS", BACKEND, TRUE);
  g_assert_cmpint (g_ascii_strcasecmp (G_OBJECT_TYPE_NAME (g_tls_backend_get_default ()), "GTlsBackend" BACKEND), ==, 0);

  g_test_add_func ("/tls/" BACKEND "/backend/default-database-is-singleton",
                   test_default_database_is_singleton);

  g_test_add ("/tls/" BACKEND "/database/verify-good", TestVerify, NULL,
              setup_verify, test_verify_database_good, teardown_verify);
  g_test_add ("/tls/" BACKEND "/database/verify-bad-identity", TestVerify, NULL,
              setup_verify, test_verify_database_bad_identity, teardown_verify);
  g_test_add ("/tls/" BACKEND "/database/verify-bad-ca", TestVerify, NULL,
              setup_verify, test_verify_database_bad_ca, teardown_verify);
  g_test_add ("/tls/" BACKEND "/database/verify-bad-before", TestVerify, NULL,
              setup_verify, test_verify_database_bad_before, teardown_verify);
  g_test_add ("/tls/" BACKEND "/database/verify-bad-expired", TestVerify, NULL,
              setup_verify, test_verify_database_bad_expired, teardown_verify);
  g_test_add ("/tls/" BACKEND "/database/verify-bad-combo", TestVerify, NULL,
              setup_verify, test_verify_database_bad_combo, teardown_verify);
  g_test_add_func ("/tls/" BACKEND "/database/verify-with-incorrect-root-in-chain",
                   test_verify_with_incorrect_root_in_chain);

  g_test_add_func ("/tls/" BACKEND "/file-database/anchors-property",
                   test_anchors_property);
  g_test_add_func ("/tls/" BACKEND "/file-database/lookup-certificates-issued-by",
                   test_lookup_certificates_issued_by);

  g_test_add ("/tls/" BACKEND "/file-database/test-handle", TestFileDatabase, NULL,
              setup_file_database, test_file_database_handle, teardown_file_database);
  g_test_add ("/tls/" BACKEND "/file-database/test-handle-invalid", TestFileDatabase, NULL,
              setup_file_database, test_file_database_handle_invalid, teardown_file_database);

  return g_test_run();
}
