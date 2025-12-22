/* 
 * GStreamer gstreamer-aeshelper
 *
 * Copyright, LCC (C) 2015 RidgeRun, LCC <carsten.behling@ridgerun.com>
 * Copyright, 2020 Nice, Contact: Rabindra Harlalka <Rabindra.Harlalka@nice.com>
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

#ifndef __GST_AES_HELPER_H__
#define __GST_AES_HELPER_H__

#include <gst/gst.h>
#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

/**
 * GstAesCipher:
 * @GST_AES_CIPHER_128_CBC: AES cipher with 128 bit key using CBC
 * @GST_AES_CIPHER_256_CBC: AES cipher with 256 bit key using CBC
 *
 * Type of AES cipher to use
 *
 * Since: 1.20
 */

typedef enum {
	GST_AES_CIPHER_128_CBC,
	GST_AES_CIPHER_256_CBC
} GstAesCipher;

#define GST_AES_DEFAULT_SERIALIZE_IV FALSE
#define GST_AES_DEFAULT_KEY ""
#define GST_AES_DEFAULT_IV ""
#define GST_AES_DEFAULT_CIPHER_MODE GST_AES_CIPHER_128_CBC
#define GST_AES_PER_BUFFER_PADDING_DEFAULT TRUE
#define GST_AES_BLOCK_SIZE 16
/* only 128 or 256 bit key length is supported */
#define GST_AES_MAX_KEY_SIZE 32

enum
{
  PROP_0,
  PROP_CIPHER,
  PROP_SERIALIZE_IV,
  PROP_KEY,
  PROP_IV,
  PROP_PER_BUFFER_PADDING
};

G_BEGIN_DECLS

GType gst_aes_cipher_get_type (void);
#define GST_TYPE_AES_CIPHER (gst_aes_cipher_get_type ())
const gchar* gst_aes_cipher_enum_to_string (GstAesCipher cipher);

gchar
gst_aes_nibble_to_hex (gchar in);
gchar *
gst_aes_bytearray2hexstring (const guchar * in, gchar * const out,
    const gushort len);
guint
gst_aes_hexstring2bytearray (GstElement * filter, const gchar * in,
    guchar * out);

G_END_DECLS
#endif /* __GST_AES_HELPER_H__ */
