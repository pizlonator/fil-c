/* 
 * GStreamer gstreamer-aesdec
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

#ifndef __GST_AES_DEC_H__
#define __GST_AES_DEC_H__

#include "gstaeshelper.h"
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_AES_DEC            (gst_aes_dec_get_type())
G_DECLARE_FINAL_TYPE (GstAesDec, gst_aes_dec, GST, AES_DEC, GstBaseTransform)
#define GST_AES_DEC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AES_DEC,GstAesDec))
#define GST_AES_DEC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AES_DEC,GstAesDecClass))
#define GST_AES_DEC_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_AES_DEC,GstAesDecClass))
#define GST_IS_AES_DEC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AES_DEC))
#define GST_IS_AES_DEC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AES_DEC))

struct _GstAesDec
{
  GstBaseTransform element;

  /* Properties */
  GstAesCipher cipher;
  guchar key[EVP_MAX_KEY_LENGTH];
  guchar iv[GST_AES_BLOCK_SIZE];
  gboolean serialize_iv;
  gboolean per_buffer_padding;

  /* Element variables */
  const EVP_CIPHER *evp_cipher;
  EVP_CIPHER_CTX *evp_ctx;
  gboolean awaiting_first_buffer;
  GMutex decoder_lock;
  /* if TRUE, then properties cannot be changed */
  gboolean locked_properties;
};

struct _GstAesDecClass
{
  GstBaseTransformClass parent_class;
};

GST_ELEMENT_REGISTER_DECLARE (aesdec)

G_END_DECLS
#endif /* __GST_AES_DEC_H__ */
