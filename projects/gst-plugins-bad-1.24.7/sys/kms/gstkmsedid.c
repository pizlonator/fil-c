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

#include <math.h>
#include "gstkmsedid.h"

/* from  libweston/backend-drm/modes.c unaccepted merge, modified slightly to
    remove non HDR stuff, return -1 if no HDR in EDID.
    https://gitlab.freedesktop.org/jcline/weston/-/commit/b3fa65d19ca60a45d0cc0fc1bfa68eea970344ee
 */
#define EDID_OFFSET_EXT_COUNT				0x7E
#define EDID_EXTENSION_SIZE				0x80
// Indicates the EDID extension is a CTA extension
#define EDID_CTA_EXTENSION_TAG			0x02
// Indicates the data block uses the extended tag field
#define EDID_CTA_EXTENDED_TAG			0x07
// Value of the extended tag field for HDR static metadata blocks
#define EDID_CTA_STATIC_HDR_TAG			0x06

/* Extract the HDR static metadata from a CTA EDID extension. */
static int
gst_kms_parse_hdr_metadata (const uint8_t * cta_ext_data,
    struct gst_kms_hdr_static_metadata *metadata)
{
  int i, block_len;
  uint8_t cta_revision = cta_ext_data[1];
  uint8_t dtd_offset = cta_ext_data[2];
  const uint8_t *data_blocks = cta_ext_data + 4;

  if (cta_revision != 3) {
    return -1;
  }
  // The data block collection ranges from byte 4 to the dtd_offset; each
  // block begins with the block size (in bytes) in bits 0-4 of the first byte.
  for (i = 0; i < dtd_offset; i += (data_blocks[i] & 0x1f) + 1) {
    if ((data_blocks[i] & 0xe0) >> 5 == EDID_CTA_EXTENDED_TAG) {
      block_len = data_blocks[i] & 0x1f;

      if (data_blocks[i + 1] == EDID_CTA_STATIC_HDR_TAG) {
        if (block_len < 2)
          continue;

        metadata->eotf = data_blocks[i + 2];
        metadata->metadata_type = data_blocks[i + 3];

        if (block_len > 3 && data_blocks[i + 4])
          metadata->max_cll = 50.0 * pow (2, data_blocks[i + 4] / 32.0);
        if (block_len > 4 && data_blocks[i + 5])
          metadata->max_fall = 50.0 * pow (2, data_blocks[i + 5] / 32.0);
        if (block_len > 5)
          metadata->min_cll =
              metadata->max_cll * pow (data_blocks[i + 6] / 255.0, 2) / 100.0;
        return 0;
      }
    }
  }
  return -1;
}

int
gst_kms_edid_parse (struct gst_kms_hdr_static_metadata *metadata,
    const uint8_t * data, size_t length)
{
  int i;
  const uint8_t *edid_extension;

  /* check header */
  if (length < 128 || length < ((size_t) data[EDID_OFFSET_EXT_COUNT] + 1) * 128)
    return -1;
  if (data[0] != 0x00 || data[1] != 0xff)
    return -1;

  edid_extension = data + 128;
  for (i = 0; i < data[EDID_OFFSET_EXT_COUNT]; i++) {
    switch (edid_extension[0]) {
      case EDID_CTA_EXTENSION_TAG:
        return gst_kms_parse_hdr_metadata (edid_extension, metadata);
    }
    edid_extension += 128;
  }

  return 0;
}

/* END from  libweston/backend-drm/modes.c unaccepted merge */
