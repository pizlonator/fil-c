/*
  * Copyright © 2008-2011 Kristian Høgsberg
  * Copyright © 2011 Intel Corporation
  * Copyright © 2017, 2018 Collabora, Ltd.
  * Copyright © 2017, 2018 General Electric Company
  * Copyright (c) 2018 DisplayLink (UK) Ltd.
  *
  * Permission is hereby granted, free of charge, to any person obtaining
  * a copy of this software and associated documentation files (the
  * "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish,
  * distribute, sublicense, and/or sell copies of the Software, and to
  * permit persons to whom the Software is furnished to do so, subject to
  * the following conditions:
  *
  * The above copyright notice and this permission notice (including the
  * next paragraph) shall be included in all copies or substantial
  * portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  */

#ifndef __GST_KMS_EDID_H__
#define __GST_KMS_EDID_H__

/* from  libweston/backend-drm/modes.c unaccepted merge, modified slightly to
    remove non HDR stuff, return -1 if no HDR in EDID.
    https://gitlab.freedesktop.org/jcline/weston/-/commit/b3fa65d19ca60a45d0cc0fc1bfa68eea970344ee
 */

#include <stddef.h>
#include <stdint.h>

/* HDR Metadata as per 861.G spec from linux/hdmi.h, modified for stdint.h */
struct gst_kms_hdr_static_metadata
{
  uint8_t eotf;
  uint8_t metadata_type;
  uint16_t max_cll;
  uint16_t max_fall;
  uint16_t min_cll;
};

int
gst_kms_edid_parse (struct gst_kms_hdr_static_metadata *metadata, const uint8_t * data,
    size_t length);

#endif /* __GST_KMS_EDID_H__ */
