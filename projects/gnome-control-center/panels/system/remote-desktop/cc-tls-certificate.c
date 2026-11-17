/* cc-tls-certificate.c
 *
 * Copyright 2018 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "config.h"

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gnutls/x509.h>

#include "cc-tls-certificate.h"

#define DEFAULT_KEY_SIZE   4096
#define DEFAULT_EXPIRATION (60L*60L*24L*2L*365L)

static void
_gnutls_datum_clear (gnutls_datum_t *datum)
{
  if (datum->data != NULL)
    gnutls_free (datum->data);
}

static void
_gnutls_crt_free (gnutls_x509_crt_t *cert)
{
  if (cert != NULL)
    gnutls_x509_crt_deinit (*cert);
}

static void
_gnutls_privkey_free (gnutls_x509_privkey_t *privkey)
{
  if (privkey != NULL)
    gnutls_x509_privkey_deinit (*privkey);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (gnutls_datum_t, _gnutls_datum_clear)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (gnutls_x509_crt_t, _gnutls_crt_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (gnutls_x509_privkey_t, _gnutls_privkey_free)

typedef struct
{
  gchar *public_key_path;
  gchar *private_key_path;
  gchar *c;
  gchar *cn;
} GenerateData;

static void
generate_data_free (GenerateData *data)
{
  g_clear_pointer (&data->public_key_path, g_free);
  g_clear_pointer (&data->private_key_path, g_free);
  g_clear_pointer (&data->c, g_free);
  g_clear_pointer (&data->cn, g_free);
  g_slice_free (GenerateData, data);
}

static gboolean
make_directory_parent (const gchar  *path,
                       GError      **error)
{
  g_autofree gchar *dir = NULL;

  g_assert (path != NULL);
  g_assert (error != NULL);

  dir = g_path_get_dirname (path);

  if (g_mkdir_with_parents (dir, 0750) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errno),
                           g_strerror (errno));
      return FALSE;
    }

  return TRUE;
}

static void
bonsai_tls_certificate_generate_worker (GTask        *task,
                                        gpointer      source_object,
                                        gpointer      task_data,
                                        GCancellable *cancellable)
{
  GenerateData *data = task_data;
  g_autoptr(GTlsCertificate) certificate = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(gnutls_x509_crt_t) certptr = NULL;
  g_autoptr(gnutls_x509_privkey_t) privkeyptr = NULL;
  g_auto(gnutls_datum_t) pubkey_data = {0};
  g_auto(gnutls_datum_t) privkey_data = {0};
  g_autofree char *dn = NULL;
  gnutls_x509_privkey_t privkey;
  gnutls_x509_crt_t cert;
  guint32 serial = 1;
  int gtlsret = 0;

  g_assert (G_IS_TASK (task));
  g_assert (source_object == NULL);
  g_assert (data != NULL);
  g_assert (data->public_key_path != NULL);
  g_assert (data->private_key_path != NULL);
  g_assert (data->c != NULL);
  g_assert (data->cn != NULL);

  if (!make_directory_parent (data->public_key_path, &error) ||
      !make_directory_parent (data->private_key_path, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /*
   * From the GnuTLS documentation:
   *
   * To be consistent with the X.509/PKIX specifications the provided serial
   * should be a big-endian positive number (i.e. it's leftmost bit should be
   * zero).
   */
  serial = GUINT32_TO_BE (serial);

#define HANDLE_FAILURE(x)            \
  G_STMT_START {                     \
    gtlsret = x;                     \
    if (gtlsret != GNUTLS_E_SUCCESS) \
      goto failure;                  \
  } G_STMT_END

  HANDLE_FAILURE (gnutls_x509_crt_init (&cert));
  certptr = &cert;
  HANDLE_FAILURE (gnutls_x509_crt_set_version (cert, 3));
  HANDLE_FAILURE (gnutls_x509_crt_set_activation_time (cert, time (NULL)));
  dn = g_strdup_printf ("C=%s,CN=%s", data->c, data->cn);
  HANDLE_FAILURE (gnutls_x509_crt_set_dn (cert, dn, NULL));
  HANDLE_FAILURE (gnutls_x509_crt_set_serial (cert, &serial, sizeof serial));
  /* 5 years. We'll figure out key rotation in that time... */
  HANDLE_FAILURE (gnutls_x509_crt_set_expiration_time (cert, time (NULL) + DEFAULT_EXPIRATION));
  HANDLE_FAILURE (gnutls_x509_privkey_init (&privkey));
  privkeyptr = &privkey;
  HANDLE_FAILURE (gnutls_x509_privkey_generate (privkey, GNUTLS_PK_RSA, DEFAULT_KEY_SIZE, 0));
  HANDLE_FAILURE (gnutls_x509_crt_set_key (cert, privkey));
  HANDLE_FAILURE (gnutls_x509_crt_sign (cert, cert, privkey));
  HANDLE_FAILURE (gnutls_x509_crt_export2 (cert, GNUTLS_X509_FMT_PEM, &pubkey_data));
  if (!g_file_set_contents (data->public_key_path, (char *)pubkey_data.data, pubkey_data.size, &error))
    goto failure;

  HANDLE_FAILURE (gnutls_x509_privkey_export2 (privkey, GNUTLS_X509_FMT_PEM, &privkey_data));
  if (!g_file_set_contents (data->private_key_path, (char*)privkey_data.data, privkey_data.size, &error))
    goto failure;

#undef HANDLE_FAILURE

  if ((certificate = g_tls_certificate_new_from_files (data->public_key_path, data->private_key_path, &error)))
    {
      g_task_return_pointer (task, g_steal_pointer (&certificate), g_object_unref);
      return;
    }

failure:

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else if (gtlsret != 0)
    g_task_return_new_error (task,
                             G_TLS_ERROR,
                             G_TLS_ERROR_MISC,
                             "GnuTLS Error: %s",
                             gnutls_strerror (gtlsret));
  else
    g_task_return_new_error (task,
                             G_IO_ERROR,
                             G_IO_ERROR_FAILED,
                             "Failed to generate TLS certificate pair");
}

void
bonsai_tls_certificate_new_generate_async (const gchar         *public_key_path,
                                           const gchar         *private_key_path,
                                           const gchar         *c,
                                           const gchar         *cn,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  GenerateData *data;

  g_return_if_fail (public_key_path != NULL);
  g_return_if_fail (private_key_path != NULL);
  g_return_if_fail (c != NULL);
  g_return_if_fail (cn != NULL);
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, bonsai_tls_certificate_new_generate_async);

  data = g_slice_new0 (GenerateData);
  data->public_key_path = g_strdup (public_key_path);
  data->private_key_path = g_strdup (private_key_path);
  data->c = g_strdup (c);
  data->cn = g_strdup (cn);
  g_task_set_task_data (task, data, (GDestroyNotify)generate_data_free);

  g_task_run_in_thread (task, bonsai_tls_certificate_generate_worker);
}

GTlsCertificate *
bonsai_tls_certificate_new_generate_finish (GAsyncResult  *result,
                                            GError       **error)
{
  g_return_val_if_fail (G_IS_TASK (result), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

GTlsCertificate *
bonsai_tls_certificate_new_generate (const gchar   *public_key_path,
                                     const gchar   *private_key_path,
                                     const gchar   *c,
                                     const gchar   *cn,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  g_autoptr(GTask) task = NULL;
  GenerateData *data;

  g_return_val_if_fail (public_key_path != NULL, NULL);
  g_return_val_if_fail (private_key_path != NULL, NULL);
  g_return_val_if_fail (c != NULL, NULL);
  g_return_val_if_fail (cn != NULL, NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  task = g_task_new (NULL, cancellable, NULL, NULL);
  g_task_set_source_tag (task, bonsai_tls_certificate_new_generate);

  data = g_slice_new0 (GenerateData);
  data->public_key_path = g_strdup (public_key_path);
  data->private_key_path = g_strdup (private_key_path);
  data->c = g_strdup (c);
  data->cn = g_strdup (cn);
  g_task_set_task_data (task, data, (GDestroyNotify)generate_data_free);

  bonsai_tls_certificate_generate_worker (task, NULL, data, cancellable);

  return g_task_propagate_pointer (task, error);
}

gchar *
bonsai_tls_certificate_get_hash (GTlsCertificate *cert)
{
  g_autoptr(GByteArray) bytesarray = NULL;
  g_autoptr(GChecksum) checksum = NULL;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (cert), NULL);

  g_object_get (cert, "certificate", &bytesarray, NULL);

  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, bytesarray->data, bytesarray->len);

  return g_ascii_strdown (g_checksum_get_string (checksum), -1);
}

typedef struct
{
  gchar *public_key_path;
  gchar *private_key_path;
  gchar *c;
  gchar *cn;
} NewFromFilesOrGenerate;

static void
new_from_files_or_generate_free (gpointer data)
{
  NewFromFilesOrGenerate *state = data;

  g_clear_pointer (&state->public_key_path, g_free);
  g_clear_pointer (&state->private_key_path, g_free);
  g_clear_pointer (&state->c, g_free);
  g_clear_pointer (&state->cn, g_free);
  g_free (state);
}

static void
bonsai_tls_certificate_new_from_files_or_generate_worker (GTask        *task,
                                                          gpointer      source_object,
                                                          gpointer      task_data,
                                                          GCancellable *cancellable)
{
  NewFromFilesOrGenerate *state = task_data;
  g_autoptr(GTlsCertificate) certificate = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (state != NULL);
  g_assert (state->public_key_path != NULL);
  g_assert (state->private_key_path != NULL);

  /* Generate new public/private key for server if we need one.
   * Ideally, we would generate something signed by a real CA
   * for the user. But since this is "private cloud" oriented,
   * we should be fine for now.
   */
  if (!g_file_test (state->public_key_path, G_FILE_TEST_EXISTS) ||
      !g_file_test (state->private_key_path, G_FILE_TEST_EXISTS))
    certificate = bonsai_tls_certificate_new_generate (state->public_key_path,
                                                       state->private_key_path,
                                                       state->c,
                                                       state->cn,
                                                       cancellable,
                                                       &error);
  else
    certificate = g_tls_certificate_new_from_files (state->public_key_path,
                                                    state->private_key_path,
                                                    &error);

  if (certificate == NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&certificate), g_object_unref);
}

/**
 * bonsai_tls_certificate_new_from_files_or_generate_async:
 * @public_key_path: the path to the public key file
 * @private_key_path: the path to the private key file
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @callback: a callback to execute upon completion
 * @user_data: closure data for @callback
 *
 * Asynchronously requests that a certificate is loaded, or generate one if it
 * does not yet exist. The generated certificate is a self-signed certificate.
 *
 * Since: 0.2
 */
void
bonsai_tls_certificate_new_from_files_or_generate_async (const gchar         *public_key_path,
                                                         const gchar         *private_key_path,
                                                         const gchar         *c,
                                                         const gchar         *cn,
                                                         GCancellable        *cancellable,
                                                         GAsyncReadyCallback  callback,
                                                         gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;
  NewFromFilesOrGenerate state;

  g_assert (public_key_path != NULL);
  g_assert (private_key_path != NULL);
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  state.public_key_path = g_strdup (public_key_path);
  state.private_key_path = g_strdup (private_key_path);
  state.c = g_strdup (c);
  state.cn = g_strdup (cn);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, bonsai_tls_certificate_new_from_files_or_generate_async);
  g_task_set_task_data (task, g_memdup2 (&state, sizeof state), new_from_files_or_generate_free);
  g_task_run_in_thread (task, bonsai_tls_certificate_new_from_files_or_generate_worker);
}

/**
 * bonsai_tls_certificate_new_from_files_or_generate_finish:
 * @result: a #GAsyncResult provided to callback
 * @error: a location for a #GError, or %NULL
 *
 * Completes a request to
 * bonsai_tls_certificate_new_from_files_or_generate_async() which will
 * either load a #GTlsCertificate for the files if they exist, or generate
 * a new self-signed certificate in their place.
 *
 * Returns: (transfer none): a #GTlsCertificate or %NULL and @error is set.
 *
 * Since: 0.2
 */
GTlsCertificate *
bonsai_tls_certificate_new_from_files_or_generate_finish (GAsyncResult  *result,
                                                          GError       **error)
{
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * bonsai_tls_certificate_new_from_files_or_generate:
 * @public_key_path: the path to the public key
 * @private_key_path: the path to the private key
 * @c:  the C for the certificate
 * @cn:  the CN for the certificate
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @error: the location for the error
 *
 * Loads a certificate or generates a new self-signed certificate in
 * it's place.
 *
 * Returns: (transfer full): a #GTlsCertificate or %NULL and @error is set
 *
 * Since: 0.2
 */
GTlsCertificate *
bonsai_tls_certificate_new_from_files_or_generate (const gchar   *public_key_path,
                                                   const gchar   *private_key_path,
                                                   const gchar   *c,
                                                   const gchar   *cn,
                                                   GCancellable  *cancellable,
                                                   GError       **error)
{
  GTlsCertificate *ret;

  g_return_val_if_fail (public_key_path != NULL, NULL);
  g_return_val_if_fail (private_key_path != NULL, NULL);
  g_return_val_if_fail (c != NULL, NULL);
  g_return_val_if_fail (cn != NULL, NULL);
  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  if (!(ret = g_tls_certificate_new_from_files (public_key_path, private_key_path, NULL)))
    ret = bonsai_tls_certificate_new_generate (public_key_path,
                                               private_key_path,
                                               c,
                                               cn,
                                               cancellable,
                                               error);

  return g_steal_pointer (&ret);
}

/**
 * bonsai_tls_certificate_new_for_user:
 * @public_key_path: the path to the public key
 * @private_key_path: the path to the private key
 *
 * This is a simplified form to create a new certificate or load a previously
 * created certificate for the current user.
 *
 * Returns: (transfer none): a #GTlsCertificate or %NULL and @error is set.
 *
 * Since: 0.2
 */
GTlsCertificate *
bonsai_tls_certificate_new_for_user (GCancellable  *cancellable,
                                     GError       **error)
{
  g_autofree gchar *public_key_path = NULL;
  g_autofree gchar *private_key_path = NULL;

  g_return_val_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable), NULL);

  public_key_path = g_build_filename (g_get_user_config_dir (), "bonsai", "public.key", NULL);
  private_key_path = g_build_filename (g_get_user_config_dir (), "bonsai", "private.key", NULL);

  return bonsai_tls_certificate_new_from_files_or_generate (public_key_path,
                                                            private_key_path,
                                                            "US",
                                                            "GNOME",
                                                            cancellable,
                                                            error);
}

gboolean
bonsai_is_tls_hash (const gchar *hash)
{
  guint len = 0;

  if (hash == NULL)
    return FALSE;

  for (; *hash; hash++)
    {
      if (len == 64)
        return FALSE;

      switch (*hash)
        {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          len++;
          break;

        default:
          return FALSE;
        }
    }

  return len == 64;
}

