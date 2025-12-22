/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 *                 Jan Schmidt <thaytan@noraisin.net>
 */

#include <string.h>

#include <gst/gst.h>

#include "gstmpegdesc.h"

void
gst_mpeg_descriptor_free (GstMPEGDescriptor * desc)
{
  g_return_if_fail (desc != NULL);

  g_free (desc);
}

static guint
gst_mpeg_descriptor_parse_1 (guint8 * data, guint size)
{
#ifndef GST_DISABLE_GST_DEBUG
  guint8 tag;
#endif
  guint8 length;

  /* need at least 2 bytes for tag and length */
  if (size < 2)
    return 0;

#ifndef GST_DISABLE_GST_DEBUG
  tag = *data;
#endif
  data += 1;
  length = *data++;
  size -= 2;

  GST_DEBUG ("tag: 0x%02x, length: %d", tag, length);

  if (length > size)
    return 0;

  return length + 2;
}

GstMPEGDescriptor *
gst_mpeg_descriptor_parse (guint8 * data, guint size)
{
  guint8 *current;
  guint consumed, total, n_desc;
  GstMPEGDescriptor *result;

  g_return_val_if_fail (data != NULL, NULL);

  current = data;
  total = 0;
  n_desc = 0;

  do {
    consumed = gst_mpeg_descriptor_parse_1 (current, size);

    if (consumed > 0) {
      current += consumed;
      total += consumed;
      size -= consumed;
      n_desc++;
    }
  }
  while (consumed > 0);

  GST_DEBUG ("parsed %d descriptors", n_desc);

  if (total == 0)
    return NULL;

  result = g_malloc (sizeof (GstMPEGDescriptor) + total);
  result->n_desc = n_desc;
  result->data_length = total;
  result->data = ((guint8 *) result) + sizeof (GstMPEGDescriptor);

  memcpy (result->data, data, total);

  return result;
}

guint
gst_mpeg_descriptor_n_desc (GstMPEGDescriptor * desc)
{
  g_return_val_if_fail (desc != NULL, 0);

  return desc->n_desc;
}

guint8 *
gst_mpeg_descriptor_find (GstMPEGDescriptor * desc, gint tag)
{
  gint length;
  guint8 *current;
  guint size;

  g_return_val_if_fail (desc != NULL, NULL);

  current = desc->data;
  length = desc->data_length;

  while (length > 0) {
    if (DESC_TAG (current) == tag)
      return current;

    size = DESC_LENGTH (current) + 2;

    current += size;
    length -= size;
  }
  return NULL;
}

guint8 *
gst_mpeg_descriptor_nth (GstMPEGDescriptor * desc, guint i)
{
  gint length;
  guint8 *current;
  guint size;

  g_return_val_if_fail (desc != NULL, NULL);

  if (i > desc->n_desc)
    return NULL;

  current = desc->data;
  length = desc->data_length;

  while (length > 0) {
    if (i == 0)
      return current;

    size = DESC_LENGTH (current) + 2;

    current += size;
    length -= size;
    i--;
  }
  return NULL;
}
