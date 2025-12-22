/*
 * Copyright (c) 2014, Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or other
 * materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstdtlscertificate.h"

#include "gstdtlsagent.h"

#ifdef __APPLE__
# define __AVAILABILITYMACROS__
# define DEPRECATED_IN_MAC_OS_X_VERSION_10_7_AND_LATER
#endif

#ifdef G_OS_WIN32
#define _WINSOCKAPI_
#include <windows.h>
#ifdef X509_NAME
#undef X509_NAME
#endif
#endif

#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define X509_getm_notBefore X509_get_notBefore
#define X509_getm_notAfter X509_get_notAfter
#endif

GST_DEBUG_CATEGORY_STATIC (gst_dtls_certificate_debug);
#define GST_CAT_DEFAULT gst_dtls_certificate_debug

enum
{
  PROP_0,
  PROP_PEM,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES];

#define DEFAULT_PEM NULL

struct _GstDtlsCertificatePrivate
{
  X509 *x509;
  EVP_PKEY *private_key;

  gchar *pem;
};

G_DEFINE_TYPE_WITH_CODE (GstDtlsCertificate, gst_dtls_certificate,
    G_TYPE_OBJECT, G_ADD_PRIVATE (GstDtlsCertificate)
    GST_DEBUG_CATEGORY_INIT (gst_dtls_certificate_debug,
        "dtlscertificate", 0, "DTLS Certificate"));

static void gst_dtls_certificate_finalize (GObject * gobject);
static void gst_dtls_certificate_set_property (GObject *, guint prop_id,
    const GValue *, GParamSpec *);
static void gst_dtls_certificate_get_property (GObject *, guint prop_id,
    GValue *, GParamSpec *);

static void init_generated (GstDtlsCertificate *);
static void init_from_pem_string (GstDtlsCertificate *, const gchar * pem);

static void
gst_dtls_certificate_class_init (GstDtlsCertificateClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gst_dtls_certificate_set_property;
  gobject_class->get_property = gst_dtls_certificate_get_property;

  properties[PROP_PEM] =
      g_param_spec_string ("pem",
      "Pem string",
      "A string containing a X509 certificate and RSA private key in PEM format",
      DEFAULT_PEM,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  _gst_dtls_init_openssl ();

  gobject_class->finalize = gst_dtls_certificate_finalize;
}

static void
gst_dtls_certificate_init (GstDtlsCertificate * self)
{
  GstDtlsCertificatePrivate *priv;

  self->priv = priv = gst_dtls_certificate_get_instance_private (self);

  priv->x509 = NULL;
  priv->private_key = NULL;
  priv->pem = NULL;
}

static void
gst_dtls_certificate_finalize (GObject * gobject)
{
  GstDtlsCertificatePrivate *priv = GST_DTLS_CERTIFICATE (gobject)->priv;

  X509_free (priv->x509);
  priv->x509 = NULL;

  EVP_PKEY_free (priv->private_key);
  priv->private_key = NULL;


  g_free (priv->pem);
  priv->pem = NULL;

  G_OBJECT_CLASS (gst_dtls_certificate_parent_class)->finalize (gobject);
}

static void
gst_dtls_certificate_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDtlsCertificate *self = GST_DTLS_CERTIFICATE (object);
  const gchar *pem;

  switch (prop_id) {
    case PROP_PEM:
      pem = g_value_get_string (value);
      if (pem) {
        init_from_pem_string (self, pem);
      } else {
        init_generated (self);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static void
gst_dtls_certificate_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDtlsCertificate *self = GST_DTLS_CERTIFICATE (object);

  switch (prop_id) {
    case PROP_PEM:
      g_return_if_fail (self->priv->pem);
      g_value_set_string (value, self->priv->pem);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
  }
}

static const gchar base64_alphabet[64] = {
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
  'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
};

static void
init_generated (GstDtlsCertificate * self)
{
  GstDtlsCertificatePrivate *priv = self->priv;
  RSA *rsa;
  BIGNUM *serial_number;
  ASN1_INTEGER *asn1_serial_number;
  X509_NAME *name = NULL;
  gchar common_name[9] = { 0, };
  gint i;

  g_return_if_fail (!priv->x509);
  g_return_if_fail (!priv->private_key);

  priv->private_key = EVP_PKEY_new ();

  if (!priv->private_key) {
    GST_WARNING_OBJECT (self, "failed to create private key");
    return;
  }

  priv->x509 = X509_new ();

  if (!priv->x509) {
    GST_WARNING_OBJECT (self, "failed to create certificate");
    EVP_PKEY_free (priv->private_key);
    priv->private_key = NULL;
    return;
  }

  /* XXX: RSA_generate_key is actually deprecated in 0.9.8 */
#if OPENSSL_VERSION_NUMBER < 0x10100001L
  rsa = RSA_generate_key (2048, RSA_F4, NULL, NULL);
#else
  /*
   * OpenSSL 3.0 deprecated all low-level APIs, so we need to rewrite this code
   * to get rid of the warnings. The porting guide explicitly recommends
   * disabling the warnings if this is not feasible, so let's do that for now:
   * https://wiki.openssl.org/index.php/OpenSSL_3.0#Upgrading_to_OpenSSL_3.0_from_OpenSSL_1.1.1
   */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  rsa = RSA_new ();
  G_GNUC_END_IGNORE_DEPRECATIONS;
  if (rsa != NULL) {
    BIGNUM *e = BN_new ();
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    if (e == NULL || !BN_set_word (e, RSA_F4)
        || !RSA_generate_key_ex (rsa, 2048, e, NULL)) {
      RSA_free (rsa);
      rsa = NULL;
    }
    G_GNUC_END_IGNORE_DEPRECATIONS;
    if (e)
      BN_free (e);
  }
#endif

  if (!rsa) {
    GST_WARNING_OBJECT (self, "failed to generate RSA");
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    EVP_PKEY_free (priv->private_key);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    priv->private_key = NULL;
    X509_free (priv->x509);
    priv->x509 = NULL;
    return;
  }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (!EVP_PKEY_assign_RSA (priv->private_key, rsa)) {
    GST_WARNING_OBJECT (self, "failed to assign RSA");
    RSA_free (rsa);
    G_GNUC_END_IGNORE_DEPRECATIONS;
    rsa = NULL;
    EVP_PKEY_free (priv->private_key);
    priv->private_key = NULL;
    X509_free (priv->x509);
    priv->x509 = NULL;
    return;
  }
  rsa = NULL;

  X509_set_version (priv->x509, 2);

  /* Set a random 64 bit integer as serial number */
  serial_number = BN_new ();
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  BN_pseudo_rand (serial_number, 64, 0, 0);
  G_GNUC_END_IGNORE_DEPRECATIONS;
  asn1_serial_number = X509_get_serialNumber (priv->x509);
  BN_to_ASN1_INTEGER (serial_number, asn1_serial_number);
  BN_free (serial_number);

  /* Set a random 8 byte base64 string as issuer/subject */
  name = X509_NAME_new ();
  for (i = 0; i < 8; i++)
    common_name[i] =
        base64_alphabet[g_random_int_range (0, G_N_ELEMENTS (base64_alphabet))];
  X509_NAME_add_entry_by_NID (name, NID_commonName, MBSTRING_ASC,
      (const guchar *) common_name, -1, -1, 0);
  X509_set_subject_name (priv->x509, name);
  X509_set_issuer_name (priv->x509, name);
  X509_NAME_free (name);

  /* Set expiry in a year */
  X509_gmtime_adj (X509_getm_notBefore (priv->x509), 0);
  X509_gmtime_adj (X509_getm_notAfter (priv->x509), 31536000L); /* A year */
  X509_set_pubkey (priv->x509, priv->private_key);

  if (!X509_sign (priv->x509, priv->private_key, EVP_sha256 ())) {
    GST_WARNING_OBJECT (self, "failed to sign certificate");
    EVP_PKEY_free (priv->private_key);
    priv->private_key = NULL;
    X509_free (priv->x509);
    priv->x509 = NULL;
    return;
  }

  self->priv->pem = _gst_dtls_x509_to_pem (priv->x509);
}

static void
init_from_pem_string (GstDtlsCertificate * self, const gchar * pem)
{
  GstDtlsCertificatePrivate *priv = self->priv;
  BIO *bio;

  g_return_if_fail (pem);
  g_return_if_fail (!priv->x509);
  g_return_if_fail (!priv->private_key);

  bio = BIO_new_mem_buf ((gpointer) pem, -1);
  g_return_if_fail (bio);

  priv->x509 = PEM_read_bio_X509 (bio, NULL, NULL, NULL);

  if (!priv->x509) {
    GST_WARNING_OBJECT (self, "failed to read certificate from pem string");
    return;
  }

  (void) BIO_reset (bio);

  priv->private_key = PEM_read_bio_PrivateKey (bio, NULL, NULL, NULL);

  BIO_free (bio);
  bio = NULL;

  if (!priv->private_key) {
    GST_WARNING_OBJECT (self, "failed to read private key from pem string");
    X509_free (priv->x509);
    priv->x509 = NULL;
    return;
  }

  self->priv->pem = g_strdup (pem);
}

gchar *
_gst_dtls_x509_to_pem (gpointer x509)
{
#define GST_DTLS_BIO_BUFFER_SIZE 4096
  BIO *bio;
  gchar buffer[GST_DTLS_BIO_BUFFER_SIZE] = { 0 };
  gint len;
  gchar *pem = NULL;

  bio = BIO_new (BIO_s_mem ());
  g_return_val_if_fail (bio, NULL);

  if (!PEM_write_bio_X509 (bio, (X509 *) x509)) {
    g_warn_if_reached ();
    goto beach;
  }

  len = BIO_read (bio, buffer, GST_DTLS_BIO_BUFFER_SIZE);
  if (!len) {
    g_warn_if_reached ();
    goto beach;
  }

  pem = g_strndup (buffer, len);

beach:
  BIO_free (bio);

  return pem;
}

GstDtlsCertificateInternalCertificate
_gst_dtls_certificate_get_internal_certificate (GstDtlsCertificate * self)
{
  g_return_val_if_fail (GST_IS_DTLS_CERTIFICATE (self), NULL);
  return self->priv->x509;
}

GstDtlsCertificateInternalKey
_gst_dtls_certificate_get_internal_key (GstDtlsCertificate * self)
{
  g_return_val_if_fail (GST_IS_DTLS_CERTIFICATE (self), NULL);
  return self->priv->private_key;
}
