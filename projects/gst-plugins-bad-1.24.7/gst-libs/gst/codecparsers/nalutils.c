/* Gstreamer
 * Copyright (C) <2011> Intel Corporation
 * Copyright (C) <2011> Collabora Ltd.
 * Copyright (C) <2011> Thibault Saunier <thibault.saunier@collabora.com>
 *
 * Some bits C-c,C-v'ed and s/4/3 from h264parse and videoparsers/h264parse.c:
 *    Copyright (C) <2010> Mark Nauwelaerts <mark.nauwelaerts@collabora.co.uk>
 *    Copyright (C) <2010> Collabora Multimedia
 *    Copyright (C) <2010> Nokia Corporation
 *
 *    (C) 2005 Michal Benes <michal.benes@itonis.tv>
 *    (C) 2008 Wim Taymans <wim.taymans@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Common code for NAL parsing from h264 and h265 parsers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "nalutils.h"
#include <string.h>


/****** Nal parser ******/

void
nal_reader_init (NalReader * nr, const guint8 * data, guint size)
{
  nr->data = data;
  nr->size = size;
  nr->n_epb = 0;

  nr->byte = 0;
  nr->bits_in_cache = 0;
  /* fill with something other than 0 to detect emulation prevention bytes */
  nr->first_byte = 0xff;
  nr->epb_cache = 0xff;
  nr->cache = 0xff;
}

gboolean
nal_reader_read (NalReader * nr, guint nbits)
{
  if (G_UNLIKELY (nr->byte * 8 + (nbits - nr->bits_in_cache) > nr->size * 8)) {
    GST_DEBUG ("Can not read %u bits, bits in cache %u, Byte * 8 %u, size in "
        "bits %u", nbits, nr->bits_in_cache, nr->byte * 8, nr->size * 8);
    return FALSE;
  }

  while (nr->bits_in_cache < nbits) {
    guint8 byte;

  next_byte:
    if (G_UNLIKELY (nr->byte >= nr->size))
      return FALSE;

    byte = nr->data[nr->byte++];
    nr->epb_cache = (nr->epb_cache << 8) | byte;

    /* check if the byte is a emulation_prevention_three_byte */
    if ((nr->epb_cache & 0xffffff) == 0x3) {
      nr->n_epb++;
      goto next_byte;
    }
    nr->cache = (nr->cache << 8) | nr->first_byte;
    nr->first_byte = byte;
    nr->bits_in_cache += 8;
  }

  return TRUE;
}

/* Skips the specified amount of bits. This is only suitable to a
   cacheable number of bits */
gboolean
nal_reader_skip (NalReader * nr, guint nbits)
{
  g_assert (nbits <= 8 * sizeof (nr->cache));

  if (G_UNLIKELY (!nal_reader_read (nr, nbits)))
    return FALSE;

  nr->bits_in_cache -= nbits;

  return TRUE;
}

/* Generic version to skip any number of bits */
gboolean
nal_reader_skip_long (NalReader * nr, guint nbits)
{
  /* Leave out enough bits in the cache once we are finished */
  const guint skip_size = 4 * sizeof (nr->cache);
  guint remaining = nbits;

  nbits %= skip_size;
  while (remaining > 0) {
    if (!nal_reader_skip (nr, nbits))
      return FALSE;
    remaining -= nbits;
    nbits = skip_size;
  }
  return TRUE;
}

guint
nal_reader_get_pos (const NalReader * nr)
{
  return nr->byte * 8 - nr->bits_in_cache;
}

guint
nal_reader_get_remaining (const NalReader * nr)
{
  return (nr->size - nr->byte) * 8 + nr->bits_in_cache;
}

guint
nal_reader_get_epb_count (const NalReader * nr)
{
  return nr->n_epb;
}

#define NAL_READER_READ_BITS(bits) \
gboolean \
nal_reader_get_bits_uint##bits (NalReader *nr, guint##bits *val, guint nbits) \
{ \
  guint shift; \
  \
  if (!nal_reader_read (nr, nbits)) \
    return FALSE; \
  \
  /* bring the required bits down and truncate */ \
  shift = nr->bits_in_cache - nbits; \
  *val = nr->first_byte >> shift; \
  \
  *val |= nr->cache << (8 - shift); \
  /* mask out required bits */ \
  if (nbits < bits) \
    *val &= ((guint##bits)1 << nbits) - 1; \
  \
  nr->bits_in_cache = shift; \
  \
  return TRUE; \
} \

NAL_READER_READ_BITS (8);
NAL_READER_READ_BITS (16);
NAL_READER_READ_BITS (32);

#define NAL_READER_PEEK_BITS(bits) \
gboolean \
nal_reader_peek_bits_uint##bits (const NalReader *nr, guint##bits *val, guint nbits) \
{ \
  NalReader tmp; \
  \
  tmp = *nr; \
  return nal_reader_get_bits_uint##bits (&tmp, val, nbits); \
}

NAL_READER_PEEK_BITS (8);

gboolean
nal_reader_get_ue (NalReader * nr, guint32 * val)
{
  guint i = 0;
  guint8 bit;
  guint32 value;

  if (G_UNLIKELY (!nal_reader_get_bits_uint8 (nr, &bit, 1)))
    return FALSE;

  while (bit == 0) {
    i++;
    if (G_UNLIKELY (!nal_reader_get_bits_uint8 (nr, &bit, 1)))
      return FALSE;
  }

  if (G_UNLIKELY (i > 31))
    return FALSE;

  if (G_UNLIKELY (!nal_reader_get_bits_uint32 (nr, &value, i)))
    return FALSE;

  *val = (1 << i) - 1 + value;

  return TRUE;
}

gboolean
nal_reader_get_se (NalReader * nr, gint32 * val)
{
  guint32 value;

  if (G_UNLIKELY (!nal_reader_get_ue (nr, &value)))
    return FALSE;

  if (value % 2)
    *val = (value / 2) + 1;
  else
    *val = -(value / 2);

  return TRUE;
}

gboolean
nal_reader_is_byte_aligned (NalReader * nr)
{
  if (nr->bits_in_cache != 0)
    return FALSE;
  return TRUE;
}

gboolean
nal_reader_has_more_data (NalReader * nr)
{
  NalReader nr_tmp;
  guint remaining, nbits;
  guint8 rbsp_stop_one_bit, zero_bits;

  remaining = nal_reader_get_remaining (nr);
  if (remaining == 0)
    return FALSE;

  nr_tmp = *nr;
  nr = &nr_tmp;

  /* The spec defines that more_rbsp_data() searches for the last bit
     equal to 1, and that it is the rbsp_stop_one_bit. Subsequent bits
     until byte boundary is reached shall be zero.

     This means that more_rbsp_data() is FALSE if the next bit is 1
     and the remaining bits until byte boundary are zero. One way to
     be sure that this bit was the very last one, is that every other
     bit after we reached byte boundary are also set to zero.
     Otherwise, if the next bit is 0 or if there are non-zero bits
     afterwards, then then we have more_rbsp_data() */
  if (!nal_reader_get_bits_uint8 (nr, &rbsp_stop_one_bit, 1))
    return FALSE;
  if (!rbsp_stop_one_bit)
    return TRUE;

  nbits = --remaining % 8;
  while (remaining > 0) {
    if (!nal_reader_get_bits_uint8 (nr, &zero_bits, nbits))
      return FALSE;
    if (zero_bits != 0)
      return TRUE;
    remaining -= nbits;
    nbits = 8;
  }
  return FALSE;
}

/***********  end of nal parser ***************/

gint
scan_for_start_codes (const guint8 * data, guint size)
{
  GstByteReader br;
  gst_byte_reader_init (&br, data, size);

  /* NALU not empty, so we can at least expect 1 (even 2) bytes following sc */
  return gst_byte_reader_masked_scan_uint32 (&br, 0xffffff00, 0x00000100,
      0, size);
}

void
nal_writer_init (NalWriter * nw, guint nal_prefix_size, gboolean packetized)
{
  g_return_if_fail (nw != NULL);
  g_return_if_fail ((packetized && nal_prefix_size > 1 && nal_prefix_size < 5)
      || (!packetized && (nal_prefix_size == 3 || nal_prefix_size == 4)));

  gst_bit_writer_init (&nw->bw);
  nw->nal_prefix_size = nal_prefix_size;
  nw->packetized = packetized;
}

void
nal_writer_reset (NalWriter * nw)
{
  g_return_if_fail (nw != NULL);

  gst_bit_writer_reset (&nw->bw);
  memset (nw, 0, sizeof (NalWriter));
}

gboolean
nal_writer_do_rbsp_trailing_bits (NalWriter * nw)
{
  g_return_val_if_fail (nw != NULL, FALSE);

  if (!gst_bit_writer_put_bits_uint8 (&nw->bw, 1, 1)) {
    GST_WARNING ("Cannot put trailing bits");
    return FALSE;
  }

  if (!gst_bit_writer_align_bytes (&nw->bw, 0)) {
    GST_WARNING ("Cannot put align bits");
    return FALSE;
  }

  return TRUE;
}

static gpointer
nal_writer_create_nal_data (NalWriter * nw, guint32 * ret_size)
{
  GstBitWriter bw;
  gint i;
  guint8 *src, *dst;
  gsize size;
  gpointer data;

  /* scan to put emulation_prevention_three_byte */
  size = GST_BIT_WRITER_BIT_SIZE (&nw->bw) >> 3;
  src = GST_BIT_WRITER_DATA (&nw->bw);

  gst_bit_writer_init_with_size (&bw, size + nw->nal_prefix_size, FALSE);
  for (i = 0; i < nw->nal_prefix_size - 1; i++)
    gst_bit_writer_put_bits_uint8 (&bw, 0, 8);
  gst_bit_writer_put_bits_uint8 (&bw, 1, 8);

  for (i = 0; i < size; i++) {
    guint pos = (GST_BIT_WRITER_BIT_SIZE (&bw) >> 3);
    dst = GST_BIT_WRITER_DATA (&bw);
    if (pos >= nw->nal_prefix_size + 2 &&
        dst[pos - 2] == 0 && dst[pos - 1] == 0 && src[i] <= 0x3) {
      gst_bit_writer_put_bits_uint8 (&bw, 0x3, 8);
    }

    gst_bit_writer_put_bits_uint8 (&bw, src[i], 8);
  }

  *ret_size = bw.bit_size >> 3;
  data = gst_bit_writer_reset_and_get_data (&bw);

  if (nw->packetized) {
    size = *ret_size - nw->nal_prefix_size;

    switch (nw->nal_prefix_size) {
      case 1:
        GST_WRITE_UINT8 (data, size);
        break;
      case 2:
        GST_WRITE_UINT16_BE (data, size);
        break;
      case 3:
        GST_WRITE_UINT24_BE (data, size);
        break;
      case 4:
        GST_WRITE_UINT32_BE (data, size);
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }

  return data;
}

GstMemory *
nal_writer_reset_and_get_memory (NalWriter * nw)
{
  guint32 size = 0;
  GstMemory *ret = NULL;
  gpointer data;

  g_return_val_if_fail (nw != NULL, NULL);

  if ((GST_BIT_WRITER_BIT_SIZE (&nw->bw) >> 3) == 0) {
    GST_WARNING ("No written byte");
    goto done;
  }

  if ((GST_BIT_WRITER_BIT_SIZE (&nw->bw) & 0x7) != 0) {
    GST_WARNING ("Written stream is not byte aligned");
    if (!nal_writer_do_rbsp_trailing_bits (nw))
      goto done;
  }

  data = nal_writer_create_nal_data (nw, &size);
  if (!data) {
    GST_WARNING ("Failed to create nal data");
    goto done;
  }

  ret = gst_memory_new_wrapped (0, data, size, 0, size, data, g_free);

done:
  gst_bit_writer_reset (&nw->bw);

  return ret;
}

guint8 *
nal_writer_reset_and_get_data (NalWriter * nw, guint32 * ret_size)
{
  guint32 size = 0;
  guint8 *data = NULL;

  g_return_val_if_fail (nw != NULL, NULL);
  g_return_val_if_fail (ret_size != NULL, NULL);

  *ret_size = 0;

  if ((GST_BIT_WRITER_BIT_SIZE (&nw->bw) >> 3) == 0) {
    GST_WARNING ("No written byte");
    goto done;
  }

  if ((GST_BIT_WRITER_BIT_SIZE (&nw->bw) & 0x7) != 0) {
    GST_WARNING ("Written stream is not byte aligned");
    if (!nal_writer_do_rbsp_trailing_bits (nw))
      goto done;
  }

  data = nal_writer_create_nal_data (nw, &size);
  if (!data) {
    GST_WARNING ("Failed to create nal data");
    goto done;
  }

  *ret_size = size;

done:
  gst_bit_writer_reset (&nw->bw);

  return data;
}

gboolean
nal_writer_put_bits_uint8 (NalWriter * nw, guint8 value, guint nbits)
{
  g_return_val_if_fail (nw != NULL, FALSE);

  if (!gst_bit_writer_put_bits_uint8 (&nw->bw, value, nbits))
    return FALSE;

  return TRUE;
}

gboolean
nal_writer_put_bits_uint16 (NalWriter * nw, guint16 value, guint nbits)
{
  g_return_val_if_fail (nw != NULL, FALSE);

  if (!gst_bit_writer_put_bits_uint16 (&nw->bw, value, nbits))
    return FALSE;

  return TRUE;
}

gboolean
nal_writer_put_bits_uint32 (NalWriter * nw, guint32 value, guint nbits)
{
  g_return_val_if_fail (nw != NULL, FALSE);

  if (!gst_bit_writer_put_bits_uint32 (&nw->bw, value, nbits))
    return FALSE;

  return TRUE;
}

gboolean
nal_writer_put_bytes (NalWriter * nw, const guint8 * data, guint nbytes)
{
  g_return_val_if_fail (nw != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);
  g_return_val_if_fail (nbytes != 0, FALSE);

  if (!gst_bit_writer_put_bytes (&nw->bw, data, nbytes))
    return FALSE;

  return TRUE;
}

gboolean
nal_writer_put_ue (NalWriter * nw, guint32 value)
{
  guint leading_zeros;
  guint rest;

  g_return_val_if_fail (nw != NULL, FALSE);

  count_exp_golomb_bits (value, &leading_zeros, &rest);

  /* write leading zeros */
  if (leading_zeros) {
    if (!nal_writer_put_bits_uint32 (nw, 0, leading_zeros))
      return FALSE;
  }

  /* write the rest */
  if (!nal_writer_put_bits_uint32 (nw, value + 1, rest))
    return FALSE;

  return TRUE;
}

gboolean
count_exp_golomb_bits (guint32 value, guint * leading_zeros, guint * rest)
{
  guint32 x;
  guint count = 0;

  /* https://en.wikipedia.org/wiki/Exponential-Golomb_coding */
  /* count bits of value + 1 */
  x = value + 1;
  while (x) {
    count++;
    x >>= 1;
  }

  if (leading_zeros) {
    if (count > 1)
      *leading_zeros = count - 1;
    else
      *leading_zeros = 0;
  }

  if (rest) {
    *rest = count;
  }

  return TRUE;
}
