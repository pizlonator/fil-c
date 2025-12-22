/*
 *  libzvbi -- Old raw VBI decoder
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: decoder.c,v 1.25 2008-02-19 00:35:15 mschimek Exp $ */

/* Note this code is only retained for compatibility with older versions
   of libzvbi. vbi_raw_decoder is now just a wrapper for the new raw
   decoder (raw_decoder.c) and bit slicer (bit_slicer.c). We'll drop
   the old API in libzvbi 0.3. Other modules (e.g. io-v4l2k.c) should
   already use the new raw VBI decoder directly. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "misc.h"
#include "decoder.h"
#include "raw_decoder.h"

/**
 * @addtogroup Rawdec Raw VBI decoder
 * @ingroup Raw
 * @brief Converting raw VBI samples to bits and bytes.
 *
 * The libzvbi already offers hardware interfaces to obtain sliced
 * VBI data for further processing. However if you want to write your own
 * interface or decode data services not covered by libzvbi you can use
 * these lower level functions.
 */

#if 0                           /* LEGACY BIT SLICER */
/*
 *  Bit Slicer
 */

#define OVERSAMPLING 4          /* 1, 2, 4, 8 */
#define THRESH_FRAC 9

/*
 * Note this is just a template. The code is inlined,
 * with bpp and endian being const.
 *
 * This function translates from the image format to
 * plain bytes, with linear interpolation of samples.
 * Could be further improved with a lowpass filter.
 */
static inline unsigned int
sample (uint8_t * raw, int offs, int bpp, int endian)
{
  unsigned char frac = offs;
  int raw0, raw1;

  switch (bpp) {
    case 14:                   /* 1:5:5:5 LE/BE */
      raw += (offs >> 8) * 2;
      raw0 = (raw[0 + endian] + raw[1 - endian] * 256) & 0x07C0;
      raw1 = (raw[2 + endian] + raw[3 - endian] * 256) & 0x07C0;
      return (raw1 - raw0) * frac + (raw0 << 8);

    case 15:                   /* 5:5:5:1 LE/BE */
      raw += (offs >> 8) * 2;
      raw0 = (raw[0 + endian] + raw[1 - endian] * 256) & 0x03E0;
      raw1 = (raw[2 + endian] + raw[3 - endian] * 256) & 0x03E0;
      return (raw1 - raw0) * frac + (raw0 << 8);

    case 16:                   /* 5:6:5 LE/BE */
      raw += (offs >> 8) * 2;
      raw0 = (raw[0 + endian] + raw[1 - endian] * 256) & 0x07E0;
      raw1 = (raw[2 + endian] + raw[3 - endian] * 256) & 0x07E0;
      return (raw1 - raw0) * frac + (raw0 << 8);

    default:                   /* 8 (intermediate bytes skipped by caller) */
      raw += (offs >> 8) * bpp;
      return (raw[bpp] - raw[0]) * frac + (raw[0] << 8);
  }
}

/*
 * Note this is just a template. The code is inlined,
 * with bpp being const.
 */
static inline vbi_bool
bit_slicer_tmpl (vbi_bit_slicer * d, uint8_t * raw,
    uint8_t * buf, int bpp, int endian)
{
  unsigned int i, j, k;
  unsigned int cl = 0, thresh0 = d->thresh, tr;
  unsigned int c = 0, t;
  unsigned char b, b1 = 0;
  int raw0, raw1, mask;

  raw += d->skip;

  if (bpp == 14)
    mask = 0x07C0;
  else if (bpp == 15)
    mask = 0x03E0;
  else if (bpp == 16)
    mask = 0x07E0;

  for (i = d->cri_bytes; i > 0; raw += (bpp >= 14 && bpp <= 16) ? 2 : bpp, i--) {
    if (bpp >= 14 && bpp <= 16) {
      raw0 = (raw[0 + endian] + raw[1 - endian] * 256) & mask;
      raw1 = (raw[2 + endian] + raw[3 - endian] * 256) & mask;
      tr = d->thresh >> THRESH_FRAC;
      d->thresh += ((raw0 - tr) * (int) ABS (raw1 - raw0)) >>
          ((bpp == 15) ? 2 : 3);
      t = raw0 * OVERSAMPLING;
    } else {
      tr = d->thresh >> THRESH_FRAC;
      d->thresh += ((int) raw[0] - tr) * (int) ABS (raw[bpp] - raw[0]);
      t = raw[0] * OVERSAMPLING;
    }

    for (j = OVERSAMPLING; j > 0; j--) {
      b = ((t + (OVERSAMPLING / 2)) / OVERSAMPLING >= tr);

      if (b ^ b1) {
        cl = d->oversampling_rate >> 1;
      } else {
        cl += d->cri_rate;

        if (cl >= (unsigned int) d->oversampling_rate) {
          cl -= d->oversampling_rate;

          c = c * 2 + b;

          if ((c & d->cri_mask) == d->cri) {
            i = d->phase_shift;
            tr *= 256;
            c = 0;

            for (j = d->frc_bits; j > 0; j--) {
              c = c * 2 + (sample (raw, i, bpp, endian) >= tr);
              i += d->step;
            }

            if (c ^= d->frc)
              return FALSE;

            /* CRI/FRC found, now get the
               payload and exit */

            switch (d->endian) {
              case 3:
                for (j = 0; j < (unsigned int) d->payload; j++) {
                  c >>= 1;
                  c += (sample (raw, i, bpp, endian) >= tr) << 7;
                  i += d->step;

                  if ((j & 7) == 7)
                    *buf++ = c;
                }

                *buf = c >> ((8 - d->payload) & 7);
                break;

              case 2:
                for (j = 0; j < (unsigned int) d->payload; j++) {
                  c = c * 2 + (sample (raw, i, bpp, endian) >= tr);
                  i += d->step;

                  if ((j & 7) == 7)
                    *buf++ = c;
                }

                *buf = c & ((1 << (d->payload & 7)) - 1);
                break;

              case 1:
                for (j = d->payload; j > 0; j--) {
                  for (k = 0; k < 8; k++) {
                    c >>= 1;
                    c += (sample (raw, i, bpp, endian) >= tr) << 7;
                    i += d->step;
                  }

                  *buf++ = c;
                }

                break;

              case 0:
                for (j = d->payload; j > 0; j--) {
                  for (k = 0; k < 8; k++) {
                    c = c * 2 + (sample (raw, i, bpp, endian) >= tr);
                    i += d->step;
                  }

                  *buf++ = c;
                }

                break;
            }

            return TRUE;
          }
        }
      }

      b1 = b;

      if (OVERSAMPLING > 1) {
        if (bpp >= 14 && bpp <= 16) {
          t += raw1;
          t -= raw0;
        } else {
          t += raw[bpp];
          t -= raw[0];
        }
      }
    }
  }

  d->thresh = thresh0;

  return FALSE;
}

static vbi_bool
bit_slicer_1 (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 1, 0);
}

static vbi_bool
bit_slicer_2 (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 2, 0);
}

static vbi_bool
bit_slicer_3 (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 3, 0);
}

static vbi_bool
bit_slicer_4 (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 4, 0);
}

static vbi_bool
bit_slicer_1555_le (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 14, 0);
}

static vbi_bool
bit_slicer_5551_le (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 15, 0);
}

static vbi_bool
bit_slicer_565_le (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 16, 0);
}

static vbi_bool
bit_slicer_1555_be (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 14, 1);
}

static vbi_bool
bit_slicer_5551_be (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 15, 1);
}

static vbi_bool
bit_slicer_565_be (vbi_bit_slicer * d, uint8_t * raw, uint8_t * buf)
{
  return bit_slicer_tmpl (d, raw, buf, 16, 1);
}

/**
 * @param slicer Pointer to vbi_bit_slicer object to be initialized. 
 * @param raw_samples Number of samples or pixels in one raw vbi line
 *   later passed to vbi_bit_slice(). This limits the number of
 *   bytes read from the sample buffer.
 * @param sampling_rate Raw vbi sampling rate in Hz, that is the number of
 *   samples or pixels sampled per second by the hardware. 
 * @param cri_rate The Clock Run In is a NRZ modulated
 *   sequence of '0' and '1' bits prepending most data transmissions to
 *   synchronize data acquisition circuits. This parameter gives the CRI bit
 *   rate in Hz, that is the number of CRI bits transmitted per second.
 * @param bit_rate The transmission bit rate of all data bits following the CRI
 *   in Hz.
 * @param cri_frc The FRaming Code usually following the CRI is a bit sequence
 *   identifying the data service, and per libzvbi definition modulated
 *   and transmitted at the same bit rate as the payload (however nothing
 *   stops you from counting all nominal CRI and FRC bits as CRI).
 *   The bit slicer compares the bits in this word, lsb last transmitted,
 *   against the transmitted CRI and FRC. Decoding of payload starts
 *   with the next bit after a match.
 * @param cri_mask Of the CRI bits in @c cri_frc, only these bits are
 *   actually significant for a match. For instance it is wise
 *   not to rely on the very first CRI bits transmitted. Note this
 *   mask is not shifted left by @a frc_bits.
 * @param cri_bits 
 * @param frc_bits Number of CRI and FRC bits in @a cri_frc, respectively.
 *   Their sum is limited to 32.
 * @param payload Number of payload <em>bits</em>. Only this data
 *   will be stored in the vbi_bit_slice() output. If this number
 *   is no multiple of eight, the most significant bits of the
 *   last byte are undefined.
 * @param modulation Modulation of the vbi data, see vbi_modulation.
 * @param fmt Format of the raw data, see vbi_pixfmt.
 * 
 * Initializes vbi_bit_slicer object. Usually you will not use this
 * function but vbi_raw_decode(), the vbi image decoder which handles
 * all these details.
 */
void
vbi_bit_slicer_init (vbi_bit_slicer * slicer,
    int raw_samples, int sampling_rate,
    int cri_rate, int bit_rate,
    unsigned int cri_frc, unsigned int cri_mask,
    int cri_bits, int frc_bits, int payload,
    vbi_modulation modulation, vbi_pixfmt fmt)
{
  unsigned int c_mask = (unsigned int) (-(cri_bits > 0)) >> (32 - cri_bits);
  unsigned int f_mask = (unsigned int) (-(frc_bits > 0)) >> (32 - frc_bits);
  int gsh = 0;

  slicer->func = bit_slicer_1;

  switch (fmt) {
    case VBI_PIXFMT_RGB24:
    case VBI_PIXFMT_BGR24:
      slicer->func = bit_slicer_3;
      slicer->skip = 1;
      break;

    case VBI_PIXFMT_RGBA32_LE:
    case VBI_PIXFMT_BGRA32_LE:
      slicer->func = bit_slicer_4;
      slicer->skip = 1;
      break;

    case VBI_PIXFMT_RGBA32_BE:
    case VBI_PIXFMT_BGRA32_BE:
      slicer->func = bit_slicer_4;
      slicer->skip = 2;
      break;

    case VBI_PIXFMT_RGB16_LE:
    case VBI_PIXFMT_BGR16_LE:
      slicer->func = bit_slicer_565_le;
      gsh = 3;                  /* (green << 3) & 0x07E0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_RGBA15_LE:
    case VBI_PIXFMT_BGRA15_LE:
      slicer->func = bit_slicer_5551_le;
      gsh = 2;                  /* (green << 2) & 0x03E0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_ARGB15_LE:
    case VBI_PIXFMT_ABGR15_LE:
      slicer->func = bit_slicer_1555_le;
      gsh = 3;                  /* (green << 2) & 0x07C0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_RGB16_BE:
    case VBI_PIXFMT_BGR16_BE:
      slicer->func = bit_slicer_565_be;
      gsh = 3;                  /* (green << 3) & 0x07E0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_RGBA15_BE:
    case VBI_PIXFMT_BGRA15_BE:
      slicer->func = bit_slicer_5551_be;
      gsh = 2;                  /* (green << 2) & 0x03E0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_ARGB15_BE:
    case VBI_PIXFMT_ABGR15_BE:
      slicer->func = bit_slicer_1555_be;
      gsh = 3;                  /* (green << 2) & 0x07C0 */
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_YUV420:
      slicer->func = bit_slicer_1;
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_YUYV:
    case VBI_PIXFMT_YVYU:
      slicer->func = bit_slicer_2;
      slicer->skip = 0;
      break;

    case VBI_PIXFMT_UYVY:
    case VBI_PIXFMT_VYUY:
      slicer->func = bit_slicer_2;
      slicer->skip = 1;
      break;

    default:
      fprintf (stderr, "vbi_bit_slicer_init: unknown pixfmt %d\n", fmt);
      exit (EXIT_FAILURE);
  }

  slicer->cri_mask = cri_mask & c_mask;
  slicer->cri = (cri_frc >> frc_bits) & slicer->cri_mask;
  /* We stop searching for CRI/FRC when the payload
     cannot possibly fit anymore. */
  slicer->cri_bytes = raw_samples
      - ((long long) sampling_rate * (payload + frc_bits)) / bit_rate;
  slicer->cri_rate = cri_rate;
  /* Raw vbi data is oversampled to account for low sampling rates. */
  slicer->oversampling_rate = sampling_rate * OVERSAMPLING;
  /* 0/1 threshold */
  slicer->thresh = 105 << (THRESH_FRAC + gsh);
  slicer->frc = cri_frc & f_mask;
  slicer->frc_bits = frc_bits;
  /* Payload bit distance in 1/256 raw samples. */
  slicer->step = (int) (sampling_rate * 256.0 / bit_rate);

  if (payload & 7) {
    slicer->payload = payload;
    slicer->endian = 3;
  } else {
    slicer->payload = payload >> 3;
    slicer->endian = 1;
  }

  switch (modulation) {
    case VBI_MODULATION_NRZ_MSB:
      slicer->endian--;
    case VBI_MODULATION_NRZ_LSB:
      slicer->phase_shift = (int)
          (sampling_rate * 256.0 / cri_rate * .5
          + sampling_rate * 256.0 / bit_rate * .5 + 128);
      break;

    case VBI_MODULATION_BIPHASE_MSB:
      slicer->endian--;
    case VBI_MODULATION_BIPHASE_LSB:
      /* Phase shift between the NRZ modulated CRI and the rest */
      slicer->phase_shift = (int)
          (sampling_rate * 256.0 / cri_rate * .5
          + sampling_rate * 256.0 / bit_rate * .25 + 128);
      break;
  }
}

#endif

/**
 * @example examples/wss.c
 * WSS capture example.
 */

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param raw A raw vbi image as defined in the vbi_raw_decoder structure
 *   (rd->sampling_format, rd->bytes_per_line, rd->count[0] + rd->count[1]
 *    scan lines).
 * @param out Buffer to store the decoded vbi_sliced data. Since every
 *   vbi scan line may contain data, this must be an array of vbi_sliced
 *   with the same number of entries as scan lines in the raw image
 *   (rd->count[0] + rd->count[1]).
 * 
 * Decode a raw vbi image, consisting of several scan lines of raw vbi data,
 * into sliced vbi data. The output is sorted by line number.
 * 
 * Note this function attempts to learn which lines carry which data
 * service, or none, to speed up decoding. You should avoid using the same
 * vbi_raw_decoder structure for different sources.
 *
 * @return
 * The number of lines decoded, i. e. the number of vbi_sliced records
 * written.
 */
int
vbi_raw_decode (vbi_raw_decoder * rd, uint8_t * raw, vbi_sliced * out)
{
  vbi3_raw_decoder *rd3;
  unsigned int n_lines;

  assert (NULL != rd);
  assert (NULL != raw);
  assert (NULL != out);

  rd3 = (vbi3_raw_decoder *) rd->pattern;
  n_lines = rd->count[0] + rd->count[1];

  g_mutex_lock (&rd->mutex);

  {
    n_lines = vbi3_raw_decoder_decode (rd3, out, n_lines, raw);
  }

  g_mutex_unlock (&rd->mutex);

  return n_lines;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param start Array of start line indices for both fields
 * @param count Array of line counts for both fields
 * 
 * Grows or shrinks the internal state arrays for VBI geometry changes
 */
void
vbi_raw_decoder_resize (vbi_raw_decoder * rd, int *start, unsigned int *count)
{
#if 0                           /* Set but unused */
  vbi_service_set service_set;
#endif
  vbi3_raw_decoder *rd3;

  assert (NULL != rd);
  assert (NULL != start);
  assert (NULL != count);

  rd3 = (vbi3_raw_decoder *) rd->pattern;

  g_mutex_lock (&rd->mutex);

  {
    if ((rd->start[0] == start[0])
        && (rd->start[1] == start[1])
        && (rd->count[0] == (int) count[0])
        && (rd->count[1] == (int) count[1])) {
      g_mutex_unlock (&rd->mutex);
      return;
    }

    rd->start[0] = start[0];
    rd->start[1] = start[1];
    rd->count[0] = count[0];
    rd->count[1] = count[1];

#if 0                           /* Set but unused */
    service_set = vbi3_raw_decoder_set_sampling_par
        (rd3, (vbi_sampling_par *) rd, /* strict */ 0);
#else
    vbi3_raw_decoder_set_sampling_par
        (rd3, (vbi_sampling_par *) rd, /* strict */ 0);
#endif
  }

  g_mutex_unlock (&rd->mutex);
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of @ref VBI_SLICED_ symbols.
 * 
 * Removes one or more data services to be decoded from the
 * vbi_raw_decoder structure. This function can be called at any
 * time and does not touch sampling parameters. 
 * 
 * @return 
 * Set of @ref VBI_SLICED_ symbols describing the remaining data
 * services that will be decoded.
 */
unsigned int
vbi_raw_decoder_remove_services (vbi_raw_decoder * rd, unsigned int services)
{
  vbi_service_set service_set;
  vbi3_raw_decoder *rd3;

  assert (NULL != rd);

  rd3 = (vbi3_raw_decoder *) rd->pattern;
  service_set = services;

  g_mutex_lock (&rd->mutex);

  {
    service_set = vbi3_raw_decoder_remove_services (rd3, service_set);
  }

  g_mutex_unlock (&rd->mutex);

  return service_set;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of @ref VBI_SLICED_ symbols.
 * @param strict See description of vbi_raw_decoder_add_services()
 *
 * Check which of the given services can be decoded with current capture
 * parameters at a given strictness level.
 *
 * @return
 * Subset of services actually decodable.
 */
unsigned int
vbi_raw_decoder_check_services (vbi_raw_decoder * rd,
    unsigned int services, int strict)
{
  vbi_service_set service_set;

  assert (NULL != rd);

  service_set = services;

  g_mutex_lock (&rd->mutex);

  {
    service_set = vbi_sampling_par_check_services
        ((vbi_sampling_par *) rd, service_set, strict);
  }

  g_mutex_unlock (&rd->mutex);

  return (unsigned int) service_set;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of @ref VBI_SLICED_ symbols.
 * @param strict A value of 0, 1 or 2 requests loose, reliable or strict
 *  matching of sampling parameters. For example if the data service
 *  requires knowledge of line numbers while they are not known, @c 0
 *  will accept the service (which may work if the scan lines are
 *  populated in a non-confusing way) but @c 1 or @c 2 will not. If the
 *  data service <i>may</i> use more lines than are sampled, @c 1 will
 *  accept but @c 2 will not. If unsure, set to @c 1.
 * 
 * After you initialized the sampling parameters in @a rd (according to
 * the abilities of your raw vbi source), this function adds one or more
 * data services to be decoded. The libzvbi raw vbi decoder can decode up
 * to eight data services in parallel. You can call this function while
 * already decoding, it does not change sampling parameters and you must
 * not change them either after calling this.
 * 
 * @return
 * Set of @ref VBI_SLICED_ symbols describing the data services that actually
 * will be decoded. This excludes those services not decodable given
 * the sampling parameters in @a rd.
 */
unsigned int
vbi_raw_decoder_add_services (vbi_raw_decoder * rd,
    unsigned int services, int strict)
{
  vbi_service_set service_set;
  vbi3_raw_decoder *rd3;

  assert (NULL != rd);

  rd3 = (vbi3_raw_decoder *) rd->pattern;
  service_set = services;

  g_mutex_lock (&rd->mutex);

  {
    vbi3_raw_decoder_set_sampling_par (rd3, (vbi_sampling_par *) rd, strict);

    service_set = vbi3_raw_decoder_add_services (rd3, service_set, strict);
  }

  g_mutex_unlock (&rd->mutex);

  return service_set;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * @param services Set of VBI_SLICED_ symbols. Here (and only here) you
 *   can add @c VBI_SLICED_VBI_625 or @c VBI_SLICED_VBI_525 to include all
 *   vbi scan lines in the calculated sampling parameters.
 * @param scanning When 525 accept only NTSC services, when 625
 *   only PAL/SECAM services. When scanning is 0, determine the scanning
 *   from the requested services, an ambiguous set will pick
 *   a 525 or 625 line system at random.
 * @param max_rate If given, the highest data bit rate in Hz of all
 *   services requested is stored here. (The sampling rate
 *   should be at least twice as high; rd->sampling_rate will
 *   be set to a more reasonable value of 27 MHz derived
 *   from ITU-R Rec. 601.)
 * 
 * Calculate the sampling parameters in @a rd required to receive and
 * decode the requested data @a services. rd->sampling_format will be
 * @c VBI_PIXFMT_YUV420, rd->bytes_per_line set accordingly to a
 * reasonable minimum. This function can be used to initialize hardware
 * prior to calling vbi_raw_decoder_add_service().
 * 
 * @return
 * Set of @ref VBI_SLICED_ symbols describing the data services covered
 * by the calculated sampling parameters. This excludes services the libzvbi
 * raw decoder cannot decode.
 */
unsigned int
vbi_raw_decoder_parameters (vbi_raw_decoder * rd,
    unsigned int services, int scanning, int *max_rate)
{
  vbi_videostd_set videostd_set;
  vbi_service_set service_set;

  switch (scanning) {
    case 525:
      videostd_set = VBI_VIDEOSTD_SET_525_60;
      break;

    case 625:
      videostd_set = VBI_VIDEOSTD_SET_625_50;
      break;

    default:
      videostd_set = 0;
      break;
  }

  service_set = services;

  g_mutex_lock (&rd->mutex);

  {
    service_set = vbi_sampling_par_from_services
        ((vbi_sampling_par *) rd,
        (unsigned int *) max_rate, videostd_set, service_set);
  }

  g_mutex_unlock (&rd->mutex);

  return (unsigned int) service_set;
}

/**
 * @param rd Initialized vbi_raw_decoder structure.
 * 
 * Reset a vbi_raw_decoder structure. This removes
 * all previously added services to be decoded (if any)
 * but does not touch the sampling parameters. You are
 * free to change the sampling parameters after calling this.
 */
void
vbi_raw_decoder_reset (vbi_raw_decoder * rd)
{
  vbi3_raw_decoder *rd3;

  if (!rd)
    return;                     /* compatibility */

  assert (NULL != rd);

  rd3 = (vbi3_raw_decoder *) rd->pattern;

  g_mutex_lock (&rd->mutex);

  {
    vbi3_raw_decoder_reset (rd3);
  }

  g_mutex_unlock (&rd->mutex);
}

/**
 * @param rd Pointer to initialized vbi_raw_decoder
 *  structure, can be @c NULL.
 * 
 * Free all resources associated with @a rd.
 */
void
vbi_raw_decoder_destroy (vbi_raw_decoder * rd)
{
  vbi3_raw_decoder *rd3;

  assert (NULL != rd);

  rd3 = (vbi3_raw_decoder *) rd->pattern;

  vbi3_raw_decoder_delete (rd3);

  g_mutex_clear (&rd->mutex);

  CLEAR (*rd);
}

/**
 * @param rd Pointer to a vbi_raw_decoder structure.
 * 
 * Initializes a vbi_raw_decoder structure.
 */
void
vbi_raw_decoder_init (vbi_raw_decoder * rd)
{
  vbi3_raw_decoder *rd3;

  assert (NULL != rd);

  CLEAR (*rd);

  g_mutex_init (&rd->mutex);

  rd3 = vbi3_raw_decoder_new ( /* sampling_par */ NULL);
  assert (NULL != rd3);

  rd->pattern = (int8_t *) rd3;
}

GST_DEBUG_CATEGORY (libzvbi_debug);
void
vbi_initialize_gst_debug (void)
{
  GST_DEBUG_CATEGORY_INIT (libzvbi_debug, "libzvbi", 0, "libzvbi");
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
