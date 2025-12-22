/*
 * GStreamer gstreamer-aesenc
 *
 * Copyright, LCC (C) 2015 RidgeRun, LCC <carsten.behling@ridgerun.com>
 * Copyright, LCC (C) 2016 RidgeRun, LCC <jose.jimenez@ridgerun.com>
 * Copyright (C) 2020 Nice, Contact: Rabindra Harlalka <Rabindra.Harlalka@nice.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1335, USA.
 */

/**
 * SECTION:element-aesenc
 *
 * AES encryption
 *
 * ## Example
 *
 * |[
 * echo "This is an AES crypto test ... " > plain.txt && \
 *       gst-launch-1.0 filesrc location=plain.txt ! \
 *       aesenc key=1f9423681beb9a79215820f6bda73d0f iv=e9aa8e834d8d70b7e0d254ff670dd718 ! \
 *       aesdec key=1f9423681beb9a79215820f6bda73d0f iv=e9aa8e834d8d70b7e0d254ff670dd718 ! \
 *       filesink location=dec.txt && \
 *       cat dec.txt
 *
 * ]|
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <string.h>
#include "gstaeshelper.h"
#include "gstaesenc.h"

GST_DEBUG_CATEGORY_STATIC (gst_aes_enc_debug);
#define GST_CAT_DEFAULT gst_aes_enc_debug
G_DEFINE_TYPE_WITH_CODE (GstAesEnc, gst_aes_enc, GST_TYPE_BASE_TRANSFORM,
    GST_DEBUG_CATEGORY_INIT (gst_aes_enc_debug, "aesenc", 0,
        "aesenc AES encryption element")
    );
GST_ELEMENT_REGISTER_DEFINE (aesenc, "aesenc", GST_RANK_PRIMARY,
    GST_TYPE_AES_ENC);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static void gst_aes_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_aes_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_aes_enc_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstFlowReturn gst_aes_enc_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf);

static gboolean gst_aes_enc_start (GstBaseTransform * base);
static gboolean gst_aes_enc_stop (GstBaseTransform * base);
static gboolean
gst_aes_enc_sink_event (GstBaseTransform * base, GstEvent * event);

/* aes_enc helper functions */
static gboolean gst_aes_enc_openssl_init (GstAesEnc * filter);
static void gst_aes_enc_finalize (GObject * object);

/* GObject vmethod implementations */

/* initialize class */
static void
gst_aes_enc_class_init (GstAesEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_aes_enc_set_property;
  gobject_class->get_property = gst_aes_enc_get_property;
  gobject_class->finalize = gst_aes_enc_finalize;

  gst_type_mark_as_plugin_api (GST_TYPE_AES_CIPHER, 0);

  /**
   * GstAesEnc:cipher
   *
   * AES cipher mode (key length and mode)
   * Currently, 128 and 256 bit keys are supported,
   * in "cipher block chaining" (CBC) mode
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_CIPHER,
      g_param_spec_enum ("cipher",
          "Cipher",
          "cipher mode",
          GST_TYPE_AES_CIPHER, GST_AES_DEFAULT_CIPHER_MODE,
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  /**
   * GstAesEnc:serialize-iv
   *
   * If true, store initialization vector in first 16 bytes of first buffer
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_SERIALIZE_IV,
      g_param_spec_boolean ("serialize-iv", "Serialize IV",
          "Store initialization vector in first 16 bytes of first buffer",
          GST_AES_DEFAULT_SERIALIZE_IV,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  /**
   * GstAesEnc:per-buffer-padding
   *
   * If true, each buffer will be padded using PKCS7 padding
   * If false, only the final buffer in the stream will be padded
   * (by OpenSSL) using PKCS7
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_PER_BUFFER_PADDING,
      g_param_spec_boolean ("per-buffer-padding", "Per buffer padding",
          "If true, pad each buffer using PKCS7 padding scheme. Otherwise, only"
          "pad final buffer",
          GST_AES_PER_BUFFER_PADDING_DEFAULT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  /**
   * GstAesEnc:key
   *
   * AES encryption key (hexadecimal)
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_KEY,
      g_param_spec_string ("key", "Key",
          "AES encryption key (in hexadecimal). Length (in bytes) must be equivalent to "
          "the number of bits in the key length : "
          "16 bytes for AES 128 and 32 bytes for AES 256",
          (gchar *) GST_AES_DEFAULT_KEY,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  /**
   * GstAesEnc:iv
   *
   * AES encryption initialization vector (hexadecimal)
   *
   * Since: 1.20
   */
  g_object_class_install_property (gobject_class, PROP_IV,
      g_param_spec_string ("iv", "Iv",
          "AES encryption initialization vector (in hexadecimal). "
          "Length must equal AES block length (16 bytes)",
          (gchar *) GST_AES_DEFAULT_IV,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY));

  gst_element_class_set_details_simple (gstelement_class,
      "aesenc",
      "Generic/Filter",
      "AES buffer encryption",
      "Rabindra Harlalka <Rabindra.Harlalka@nice.com>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_BASE_TRANSFORM_CLASS (klass)->transform =
      GST_DEBUG_FUNCPTR (gst_aes_enc_transform);
  GST_BASE_TRANSFORM_CLASS (klass)->prepare_output_buffer =
      GST_DEBUG_FUNCPTR (gst_aes_enc_prepare_output_buffer);
  GST_BASE_TRANSFORM_CLASS (klass)->start =
      GST_DEBUG_FUNCPTR (gst_aes_enc_start);
  GST_BASE_TRANSFORM_CLASS (klass)->sink_event =
      GST_DEBUG_FUNCPTR (gst_aes_enc_sink_event);
  GST_BASE_TRANSFORM_CLASS (klass)->stop = GST_DEBUG_FUNCPTR (gst_aes_enc_stop);
}

/* Initialize element
 */
static void
gst_aes_enc_init (GstAesEnc * filter)
{
  GST_INFO_OBJECT (filter, "Initializing plugin");
  filter->cipher = GST_AES_DEFAULT_CIPHER_MODE;
  filter->awaiting_first_buffer = TRUE;
  filter->per_buffer_padding = GST_AES_PER_BUFFER_PADDING_DEFAULT;
  g_mutex_init (&filter->encoder_lock);
}

static void
gst_aes_enc_finalize (GObject * object)
{
  GstAesEnc *filter = GST_AES_ENC (object);

  g_mutex_clear (&filter->encoder_lock);
  G_OBJECT_CLASS (gst_aes_enc_parent_class)->finalize (object);
}

static void
gst_aes_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAesEnc *filter = GST_AES_ENC (object);

  g_mutex_lock (&filter->encoder_lock);
  /* no property may be set after first output buffer is prepared */
  if (filter->locked_properties) {
    GST_WARNING_OBJECT (filter,
        "Properties cannot be set once buffers begin flowing in element. Ignored");
    goto cleanup;
  }
  switch (prop_id) {
    case PROP_CIPHER:
      filter->cipher = g_value_get_enum (value);
      filter->evp_cipher =
          EVP_get_cipherbyname (gst_aes_cipher_enum_to_string (filter->cipher));
      GST_DEBUG_OBJECT (filter, "cipher: %s",
          gst_aes_cipher_enum_to_string (filter->cipher));
      break;
    case PROP_SERIALIZE_IV:
      filter->serialize_iv = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (filter, "serialize iv: %s",
          filter->serialize_iv ? "TRUE" : "FALSE");
      break;
    case PROP_PER_BUFFER_PADDING:
      filter->per_buffer_padding = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (filter, "Per buffer padding: %s",
          filter->per_buffer_padding ? "TRUE" : "FALSE");
      break;
    case PROP_KEY:
    {
      guint hex_len = gst_aes_hexstring2bytearray (GST_ELEMENT (filter),
          g_value_get_string (value), filter->key);

      if (!hex_len) {
        GST_ERROR_OBJECT (filter, "invalid key");
        goto cleanup;
      }
      GST_DEBUG_OBJECT (filter, "key: %s", g_value_get_string (value));
    }
      break;
    case PROP_IV:
    {
      gchar iv_string[2 * GST_AES_BLOCK_SIZE + 1];
      guint hex_len = gst_aes_hexstring2bytearray (GST_ELEMENT (filter),
          g_value_get_string (value), filter->iv);

      if (hex_len != GST_AES_BLOCK_SIZE) {
        GST_ERROR_OBJECT (filter, "invalid initialization vector");
        goto cleanup;
      }
      GST_DEBUG_OBJECT (filter, "iv: %s",
          gst_aes_bytearray2hexstring (filter->iv, iv_string,
              GST_AES_BLOCK_SIZE));
    }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

cleanup:
  g_mutex_unlock (&filter->encoder_lock);
}

static void
gst_aes_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAesEnc *filter = GST_AES_ENC (object);

  switch (prop_id) {
    case PROP_CIPHER:
      g_value_set_enum (value, filter->cipher);
      break;
    case PROP_SERIALIZE_IV:
      g_value_set_boolean (value, filter->serialize_iv);
      break;
    case PROP_PER_BUFFER_PADDING:
      g_value_set_boolean (value, filter->per_buffer_padding);
      break;
    case PROP_KEY:
      g_value_set_string (value, (gchar *) filter->key);
      break;
    case PROP_IV:
      g_value_set_string (value, (gchar *) filter->iv);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_aes_enc_sink_event (GstBaseTransform * base, GstEvent * event)
{
  GstAesEnc *filter = GST_AES_ENC (base);
  g_mutex_lock (&filter->encoder_lock);

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GST_DEBUG_OBJECT (filter, "Received EOS on sink pad");
    if (!filter->per_buffer_padding && !filter->awaiting_first_buffer) {
      gint len;
      GstBuffer *outbuf;
      GstMapInfo outmap;

      outbuf = gst_buffer_new_allocate (NULL, EVP_MAX_BLOCK_LENGTH, NULL);
      if (outbuf == NULL) {
        GST_DEBUG_OBJECT (filter,
            "Failed to allocate a new buffer of length %d",
            EVP_MAX_BLOCK_LENGTH);
        goto buffer_fail;
      }
      if (!gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE)) {
        GST_DEBUG_OBJECT (filter,
            "gst_buffer_map on outbuf failed for final buffer.");
        gst_buffer_unref (outbuf);
        goto buffer_fail;
      }
      if (1 != EVP_CipherFinal_ex (filter->evp_ctx, outmap.data, &len)) {
        GST_DEBUG_OBJECT (filter, "Could not finalize openssl encryption");
        gst_buffer_unmap (outbuf, &outmap);
        gst_buffer_unref (outbuf);
        goto cipher_fail;
      }
      if (len == 0) {
        GST_DEBUG_OBJECT (filter, "Not pushing final buffer as length is 0");
        gst_buffer_unmap (outbuf, &outmap);
        gst_buffer_unref (outbuf);
        goto out;
      }
      GST_DEBUG_OBJECT (filter, "Pushing final buffer of length: %d", len);
      gst_buffer_unmap (outbuf, &outmap);
      gst_buffer_set_size (outbuf, len);
      if (gst_pad_push (base->srcpad, outbuf) != GST_FLOW_OK) {
        GST_DEBUG_OBJECT (filter, "Failed to push the final buffer");
        goto push_fail;
      }
    } else {
      GST_DEBUG_OBJECT (filter,
          "Not pushing final buffer as we didn't have any input");
    }
  }

out:
  g_mutex_unlock (&filter->encoder_lock);

  return GST_BASE_TRANSFORM_CLASS (gst_aes_enc_parent_class)->sink_event (base,
      event);

  /* ERROR */
buffer_fail:
  GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, (NULL),
      ("Failed to allocate or map buffer for writing"));
  g_mutex_unlock (&filter->encoder_lock);

  return FALSE;
cipher_fail:
  GST_ELEMENT_ERROR (filter, STREAM, FAILED, ("Cipher finalization failed."),
      ("Error while finalizing the stream"));
  g_mutex_unlock (&filter->encoder_lock);

  return FALSE;
push_fail:
  GST_ELEMENT_ERROR (filter, CORE, PAD, (NULL),
      ("Failed to push the final buffer"));
  g_mutex_unlock (&filter->encoder_lock);

  return FALSE;
}

/* GstBaseTransform vmethod implementations */
static GstFlowReturn
gst_aes_enc_transform (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer * outbuf)
{
  GstAesEnc *filter = GST_AES_ENC (base);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstMapInfo inmap, outmap;
  guchar *plaintext;
  gint plaintext_len;
  guchar *ciphertext;
  gint ciphertext_len;
  gint out_len;

  if (!gst_buffer_map (inbuf, &inmap, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, (NULL),
        ("Failed to map buffer for reading"));
    goto cleanup;
  }
  if (!gst_buffer_map (outbuf, &outmap, GST_MAP_WRITE)) {
    gst_buffer_unmap (inbuf, &inmap);
    GST_ELEMENT_ERROR (filter, RESOURCE, FAILED, (NULL),
        ("Failed to map buffer for writing"));
    goto cleanup;
  }

  /* ENCRYPTING */
  plaintext = inmap.data;
  plaintext_len = inmap.size;
  if (filter->padding)
    plaintext_len += filter->padding - GST_AES_BLOCK_SIZE;
  ciphertext = outmap.data;
  if (filter->awaiting_first_buffer) {
    if (!EVP_CipherInit_ex (filter->evp_ctx, filter->evp_cipher, NULL,
            filter->key, filter->iv, TRUE)) {
      GST_ERROR_OBJECT (filter, "Could not initialize openssl cipher");
      goto cleanup;
    }
    if (filter->serialize_iv) {
      memcpy (ciphertext, filter->iv, GST_AES_BLOCK_SIZE);
      ciphertext += GST_AES_BLOCK_SIZE;
    }
  }

  /* encrypt unpadded buffer */
  if (!EVP_CipherUpdate (filter->evp_ctx, ciphertext,
          &ciphertext_len, plaintext, plaintext_len)) {
    GST_ELEMENT_ERROR (filter, STREAM, FAILED, ("Cipher update failed."),
        ("Error while updating openssl cipher"));
    goto cleanup;
  } else if (filter->padding) {
    gint temp;
    guint k;

    /* PKCS7 padding */
    memset (filter->padded_block, filter->padding, GST_AES_BLOCK_SIZE);
    for (k = 0; k < GST_AES_BLOCK_SIZE - filter->padding; ++k)
      filter->padded_block[k] = plaintext[plaintext_len + k];

    /* encrypt padding buffer */
    if (!EVP_CipherUpdate (filter->evp_ctx,
            ciphertext + ciphertext_len, &temp,
            filter->padded_block, GST_AES_BLOCK_SIZE)) {
      GST_ELEMENT_ERROR (filter, STREAM, FAILED, ("Cipher update failed."),
          ("Error while updating openssl cipher"));
      goto cleanup;
    } else {
      g_assert (temp == GST_AES_BLOCK_SIZE);
      ciphertext_len += GST_AES_BLOCK_SIZE;
      plaintext_len += GST_AES_BLOCK_SIZE;
    }
  }
  gst_buffer_unmap (inbuf, &inmap);
  gst_buffer_unmap (outbuf, &outmap);

  out_len = ciphertext_len + ((filter->serialize_iv
          && filter->awaiting_first_buffer)
      ? GST_AES_BLOCK_SIZE : 0);
  gst_buffer_set_size (outbuf, out_len);
  GST_LOG_OBJECT (filter,
      "plaintext len: %d, ciphertext len: %d, padding: %d, output buffer length: %d",
      plaintext_len, ciphertext_len, filter->padding, out_len);
  ret = GST_FLOW_OK;

cleanup:
  filter->awaiting_first_buffer = FALSE;

  return ret;
}

static GstFlowReturn
gst_aes_enc_prepare_output_buffer (GstBaseTransform * base,
    GstBuffer * inbuf, GstBuffer ** outbuf)
{
  GstAesEnc *filter = GST_AES_ENC (base);
  GstBaseTransformClass *bclass = GST_BASE_TRANSFORM_GET_CLASS (base);
  guint out_size = (guint) gst_buffer_get_size (inbuf);

  g_mutex_lock (&filter->encoder_lock);
  filter->locked_properties = TRUE;
  if (filter->per_buffer_padding) {
    /* pad to multiple of GST_AES_BLOCK_SIZE */
    filter->padding =
        GST_AES_BLOCK_SIZE - (out_size & (GST_AES_BLOCK_SIZE - 1));
    out_size += filter->padding;
  } else {
    /* we need extra space at end of output buffer
     * when we let OpenSSL handle PKCS7 padding  */
    out_size += GST_AES_BLOCK_SIZE;
  }

  /* add room for serialized IV at beginning of first output buffer */
  if (filter->serialize_iv && filter->awaiting_first_buffer)
    out_size += GST_AES_BLOCK_SIZE;
  g_mutex_unlock (&filter->encoder_lock);

  GST_LOG_OBJECT (filter,
      "Input buffer size %d, output buffer size: %d. padding : %d",
      (guint) gst_buffer_get_size (inbuf), out_size, filter->padding);
  *outbuf = gst_buffer_new_allocate (NULL, out_size, NULL);
  bclass->copy_metadata (base, inbuf, *outbuf);

  return GST_FLOW_OK;
}

static gboolean
gst_aes_enc_start (GstBaseTransform * base)
{
  GstAesEnc *filter = GST_AES_ENC (base);

  GST_INFO_OBJECT (filter, "Starting");
  if (!gst_aes_enc_openssl_init (filter)) {
    GST_ERROR_OBJECT (filter, "OpenSSL initialization failed");
    return FALSE;
  }

  GST_INFO_OBJECT (filter, "Start successful");

  return TRUE;
}

static gboolean
gst_aes_enc_stop (GstBaseTransform * base)
{
  GstAesEnc *filter = GST_AES_ENC (base);

  GST_INFO_OBJECT (filter, "Stopping");
  EVP_CIPHER_CTX_free (filter->evp_ctx);

  return TRUE;
}

/* AesEnc helper  functions */
static gboolean
gst_aes_enc_openssl_init (GstAesEnc * filter)
{
  GST_DEBUG_OBJECT (filter, "Initializing with %s",
      OpenSSL_version (OPENSSL_VERSION));

  filter->evp_cipher =
      EVP_get_cipherbyname (gst_aes_cipher_enum_to_string (filter->cipher));
  if (!filter->evp_cipher) {
    GST_ERROR_OBJECT (filter, "Could not get cipher by name from openssl");
    return FALSE;
  }
  if (!(filter->evp_ctx = EVP_CIPHER_CTX_new ()))
    return FALSE;
  GST_LOG_OBJECT (filter, "Initialization successful");

  return TRUE;
}
