/*
 * GStreamer gstreamer-aeshelper
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstaeshelper.h"

GType
gst_aes_cipher_get_type (void)
{
  static GType aes_cipher_type = 0;

  if (g_once_init_enter (&aes_cipher_type)) {
    static GEnumValue aes_cipher_types[] = {
      {GST_AES_CIPHER_128_CBC, "AES 128 bit cipher key using CBC method",
          "aes-128-cbc"},
      {GST_AES_CIPHER_256_CBC,
            "AES 256 bit cipher key using CBC method",
          "aes-256-cbc"},
      {0, NULL, NULL},
    };

    GType temp = g_enum_register_static ("GstAesCipher",
        aes_cipher_types);

    g_once_init_leave (&aes_cipher_type, temp);
  }

  return aes_cipher_type;
}

const gchar *
gst_aes_cipher_enum_to_string (GstAesCipher cipher)
{
  switch (cipher) {
    case GST_AES_CIPHER_128_CBC:
      return "aes-128-cbc";
      break;
    case GST_AES_CIPHER_256_CBC:
      return "aes-256-cbc";
      break;
  }

  return "";
}


gchar
gst_aes_nibble_to_hex (gchar in)
{
  return in < 10 ? in + 48 : in + 55;
}

/*
 * gst_aes_bytearray2hexstring
 *
 * convert array of bytes to hex string
 *
 * @param in input byte array
 * @param out allocated hex string for output
 * @param len length of input byte array
 *
 * @return output hex string
 */
gchar *
gst_aes_bytearray2hexstring (const guchar * in, gchar * const out,
    const gushort len)
{
  gushort i;
  gchar high;
  gchar low;

  for (i = 0; i < len; i++) {
    high = (in[i] & 0xF0) >> 4;
    low = in[i] & 0x0F;
    out[i * 2] = gst_aes_nibble_to_hex (high);
    out[i * 2 + 1] = gst_aes_nibble_to_hex (low);
  }
  out[len * 2] = 0;             /* null terminate */

  return out;
}

/*
 * gst_aes_hexstring2bytearray
 *
 * convert hex string to array of bytes
 *
 * @param filter calling element
 * @param in input hex string
 * @param allocated byte array for output
 *
 * @return output byte array
 */
guint
gst_aes_hexstring2bytearray (GstElement * filter, const gchar * in,
    guchar * out)
{
  gchar byte_val;
  guint hex_count = 0;

  GST_LOG_OBJECT (filter, "Converting hex string to number");

  g_return_val_if_fail (in && out, 0);

  while (*in != 0) {
    /* Compute fist half-byte */
    if (*in >= 'A' && *in <= 'F') {
      byte_val = (*in - 55) << 4;
    } else if (*in >= 'a' && *in <= 'f') {
      byte_val = (*in - 87) << 4;
    } else if (*in >= '0' && *in <= '9') {
      byte_val = (*in - 48) << 4;
    } else {
      return 0;
    }
    in++;
    if (*in == 0) {
      break;
    }
    /* Compute second half-byte */
    if (*in >= 'A' && *in <= 'F') {
      *out = (*in - 55) + byte_val;
    } else if (*in >= 'a' && *in <= 'f') {
      *out = (*in - 87) + byte_val;
    } else if (*in >= '0' && *in <= '9') {
      *out = (*in - 48) + byte_val;
    } else {
      return 0;
    }

    GST_LOG_OBJECT (filter, "ch: %c%c, hex: 0x%x", *(in - 1), *in, *out);
    in++;
    out++;
    if (!in || !out)
      return 0;
    hex_count++;
  }
  GST_LOG_OBJECT (filter, "Hex string conversion successful");

  return hex_count;
}
