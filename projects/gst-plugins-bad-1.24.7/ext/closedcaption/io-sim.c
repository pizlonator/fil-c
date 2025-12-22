/*
 *  libzvbi -- VBI device simulation
 *
 *  Copyright (C) 2004, 2007 Michael H. Schimek
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

/* $Id: io-sim.c,v 1.18 2009-12-14 23:43:40 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef _MSC_VER
#define _USE_MATH_DEFINES       /* Needed for M_PI and M_LN2 */
#endif
#include <math.h>               /* sin(), log() */
#include <errno.h>
#include <ctype.h>              /* isspace() */
#include <limits.h>             /* INT_MAX */

#include "misc.h"
#include "sliced.h"
#include "sampling_par.h"
#include "raw_decoder.h"
#include "hamm.h"

#  define sp_sample_format sampling_format
#  define SAMPLES_PER_LINE(sp)						\
	((sp)->bytes_per_line / VBI_PIXFMT_BPP ((sp)->sampling_format))
#  define SYSTEM_525(sp)						\
	(525 == (sp)->scanning)

#include "io-sim.h"

/*
 * @addtogroup Rawenc Raw VBI encoder
 * @ingroup Raw
 * @brief Converting sliced VBI data to raw VBI images.
 *
 * These are functions converting sliced VBI data to raw VBI images as
 * transmitted in the vertical blanking interval of analog video standards.
 * They are mainly intended for tests of the libzvbi bit slicer and
 * raw VBI decoder.
 */

#  define VBI_PIXFMT_RGB24_LE VBI_PIXFMT_RGB24
#  define VBI_PIXFMT_BGR24_LE VBI_PIXFMT_BGR24
#  define VBI_PIXFMT_RGBA24_LE VBI_PIXFMT_RGBA32_LE
#  define VBI_PIXFMT_BGRA24_LE VBI_PIXFMT_BGRA32_LE
#  define VBI_PIXFMT_RGBA24_BE VBI_PIXFMT_RGBA32_BE
#  define VBI_PIXFMT_BGRA24_BE VBI_PIXFMT_BGRA32_BE
#  define vbi_pixfmt_bytes_per_pixel VBI_PIXFMT_BPP

#define PI 3.1415926535897932384626433832795029

#define PULSE(zero_level)						\
do {									\
	if (0 == seq) {							\
		raw[i] = SATURATE (zero_level, 0, 255);			\
	} else if (3 == seq) {						\
		raw[i] = SATURATE (zero_level + (int) signal_amp,	\
				   0, 255);				\
	} else if ((seq ^ bit) & 1) { /* down */			\
		double r = sin (q * tr - (PI / 2.0));			\
		r = r * r * signal_amp;					\
		raw[i] = SATURATE (zero_level + (int) r, 0, 255);	\
	} else { /* up */						\
		double r = sin (q * tr);				\
		r = r * r * signal_amp;					\
		raw[i] = SATURATE (zero_level + (int) r, 0, 255);	\
	}								\
} while (0)

#define PULSE_SEQ(zero_level)						\
do {									\
	double tr;							\
	unsigned int bit;						\
	unsigned int byte;						\
	unsigned int seq;						\
									\
	tr = t - t1;							\
	bit = tr * bit_rate;						\
	byte = bit >> 3;						\
	bit &= 7;							\
	seq = (buf[byte] >> 7) + buf[byte + 1] * 2;			\
	seq = (seq >> bit) & 3;						\
	PULSE (zero_level);						\
} while (0)

_vbi_inline void
vbi_sincos (double x, double *sinx, double *cosx)
{
  *sinx = sin (x);
  *cosx = cos (x);
}

#define vbi_log2(x) (log (x) / M_LN2)

static void
signal_teletext (uint8_t * raw,
    const vbi_sampling_par * sp,
    int black_level,
    double signal_amp,
    double bit_rate,
    unsigned int frc, unsigned int payload, const vbi_sliced * sliced)
{
  double bit_period = 1.0 / bit_rate;
  /* Teletext System B: Sixth CRI pulse at 12 us
     (+.5 b/c we start with a 0 bit). */
  double t1 = 12e-6 - 13 * bit_period;
  double t2 = t1 + (payload * 8 + 24 + 1) * bit_period;
  double q = (PI / 2.0) * bit_rate;
  double sample_period = 1.0 / sp->sampling_rate;
  unsigned int samples_per_line;
  uint8_t buf[64];
  unsigned int i;
  double t;

  buf[0] = 0x00;
  buf[1] = 0x55;                /* clock run-in */
  buf[2] = 0x55;
  buf[3] = frc;

  memcpy (buf + 4, sliced->data, payload);

  buf[payload + 4] = 0x00;

  t = sp->offset / (double) sp->sampling_rate;

  samples_per_line = SAMPLES_PER_LINE (sp);

  for (i = 0; i < samples_per_line; ++i) {
    if (t >= t1 && t < t2)
      PULSE_SEQ (black_level);

    t += sample_period;
  }
}

static void
signal_vps (uint8_t * raw,
    const vbi_sampling_par * sp,
    int black_level, int white_level, const vbi_sliced * sliced)
{
  static const uint8_t biphase[] = {
    0xAA, 0x6A, 0x9A, 0x5A,
    0xA6, 0x66, 0x96, 0x56,
    0xA9, 0x69, 0x99, 0x59,
    0xA5, 0x65, 0x95, 0x55
  };
  double bit_rate = 15625 * 160 * 2;
  double t1 = 12.5e-6 - .5 / bit_rate;
  double t4 = t1 + ((4 + 13 * 2) * 8) / bit_rate;
  double q = (PI / 2.0) * bit_rate;
  double sample_period = 1.0 / sp->sampling_rate;
  unsigned int samples_per_line;
  double signal_amp = (0.5 / 0.7) * (white_level - black_level);
  uint8_t buf[32];
  unsigned int i;
  double t;

  CLEAR (buf);

  buf[1] = 0x55;                /* 0101 0101 */
  buf[2] = 0x55;                /* 0101 0101 */
  buf[3] = 0x51;                /* 0101 0001 */
  buf[4] = 0x99;                /* 1001 1001 */

  for (i = 0; i < 13; ++i) {
    unsigned int b = sliced->data[i];

    buf[5 + i * 2] = biphase[b >> 4];
    buf[6 + i * 2] = biphase[b & 15];
  }

  buf[6 + 12 * 2] &= 0x7F;

  t = sp->offset / (double) sp->sampling_rate;

  samples_per_line = SAMPLES_PER_LINE (sp);

  for (i = 0; i < samples_per_line; ++i) {
    if (t >= t1 && t < t4)
      PULSE_SEQ (black_level);

    t += sample_period;
  }
}

static void
wss_biphase (uint8_t buf[32], const vbi_sliced * sliced)
{
  unsigned int bit;
  unsigned int data;
  unsigned int i;

  /* 29 bit run-in and 24 bit start code, lsb first. */

  buf[0] = 0x00;
  buf[1] = 0x1F;                /* 0001 1111 */
  buf[2] = 0xC7;                /* 1100 0111 */
  buf[3] = 0x71;                /* 0111 0001 */
  buf[4] = 0x1C;                /* 000 | 1 1100 */
  buf[5] = 0x8F;                /* 1000 1111 */
  buf[6] = 0x07;                /* 0000 0111 */
  buf[7] = 0x1F;                /*    1 1111 */

  bit = 8 + 29 + 24;
  data = sliced->data[0] + sliced->data[1] * 256;

  for (i = 0; i < 14; ++i) {
    static const unsigned int biphase[] = { 0x38, 0x07 };
    unsigned int byte;
    unsigned int shift;
    unsigned int seq;

    byte = bit >> 3;
    shift = bit & 7;
    bit += 6;

    seq = biphase[data & 1] << shift;
    data >>= 1;

    assert (byte < 31);

    buf[byte] |= seq;
    buf[byte + 1] = seq >> 8;
  }
}

static void
signal_wss_625 (uint8_t * raw,
    const vbi_sampling_par * sp,
    int black_level, int white_level, const vbi_sliced * sliced)
{
  double bit_rate = 15625 * 320;
  double t1 = 11.0e-6 - .5 / bit_rate;
  double t4 = t1 + (29 + 24 + 14 * 6 + 1) / bit_rate;
  double q = (PI / 2.0) * bit_rate;
  double sample_period = 1.0 / sp->sampling_rate;
  double signal_amp = (0.5 / 0.7) * (white_level - black_level);
  unsigned int samples_per_line;
  uint8_t buf[32];
  unsigned int i;
  double t;

  CLEAR (buf);

  wss_biphase (buf, sliced);

  t = sp->offset / (double) sp->sampling_rate;

  samples_per_line = SAMPLES_PER_LINE (sp);

  for (i = 0; i < samples_per_line; ++i) {
    if (t >= t1 && t < t4)
      PULSE_SEQ (black_level);

    t += sample_period;
  }
}

static void
signal_closed_caption (uint8_t * raw,
    const vbi_sampling_par * sp,
    int blank_level,
    int white_level,
    unsigned int flags, double bit_rate, const vbi_sliced * sliced)
{
  double D = 1.0 / bit_rate;
  double t0 = 10.5e-6;          /* CRI start half amplitude (EIA 608-B) */
  double t1 = t0 - .25 * D;     /* CRI start, blanking level */
  double t2 = t1 + 7 * D;       /* CRI 7 cycles */
  /* First start bit, left edge half amplitude, minus rise time. */
  double t3 = t0 + 6.5 * D - 120e-9;
  double q1 = PI * bit_rate * 2;
  /* Max. rise/fall time 240 ns (EIA 608-B). */
  double q2 = PI / 120e-9;
  double signal_mean;
  double signal_high;
  double sample_period = 1.0 / sp->sampling_rate;
  unsigned int samples_per_line;
  double t;
  unsigned int data;
  unsigned int i;

  /* Twice 7 data + odd parity, start bit 0 -> 1 */

  data = (sliced->data[1] << 12) + (sliced->data[0] << 4) + 8;

  t = sp->offset / (double) sp->sampling_rate;

  samples_per_line = SAMPLES_PER_LINE (sp);

  if (flags & _VBI_RAW_SHIFT_CC_CRI) {
    /* Wrong signal shape found by Rich Kadel,
       zapping-misc@lists.sourceforge.net 2006-07-16. */
    t0 += D / 2;
    t1 += D / 2;
    t2 += D / 2;
  }

  if (flags & _VBI_RAW_LOW_AMP_CC) {
    /* Low amplitude signal found by Rich Kadel,
       zapping-misc@lists.sourceforge.net 2007-08-15. */
    white_level = white_level * 6 / 10;
  }

  signal_mean = (white_level - blank_level) * .25;      /* 25 IRE */
  signal_high = blank_level + (white_level - blank_level) * .5;

  for (i = 0; i < samples_per_line; ++i) {
    if (t >= t1 && t < t2) {
      raw[i] = SATURATE (blank_level + (1.0 - cos (q1 * (t - t1)))
          * signal_mean, 0, 255);
    } else {
      unsigned int bit;
      unsigned int seq;
      double d;

      d = t - t3;
      bit = d * bit_rate;
      seq = (data >> bit) & 3;

      d -= bit * D;
      if ((1 == seq || 2 == seq)
          && fabs (d) < .120e-6) {
        int level;

        if (1 == seq)
          level = blank_level + (1.0 + cos (q2 * d))
              * signal_mean;
        else
          level = blank_level + (1.0 - cos (q2 * d))
              * signal_mean;
        raw[i] = SATURATE (level, 0, 255);
      } else if (data & (2 << bit)) {
        raw[i] = SATURATE (signal_high, 0, 255);
      } else {
        raw[i] = SATURATE (blank_level, 0, 255);
      }
    }

    t += sample_period;
  }
}

static void
clear_image (uint8_t * p,
    unsigned int value,
    unsigned int width, unsigned int height, unsigned int bytes_per_line)
{
  if (width == bytes_per_line) {
    memset (p, value, height * bytes_per_line);
  } else {
    while (height-- > 0) {
      memset (p, value, width);
      p += bytes_per_line;
    }
  }
}

/*
 * @param raw Noise will be added to this raw VBI image.
 * @param sp Describes the raw VBI data in the buffer. @a sp->sampling_format
 *   must be @c VBI_PIXFMT_Y8 (@c VBI_PIXFMT_YUV420 in libzvbi 0.2.x).
 *   Note for compatibility in libzvbi 0.2.x vbi_sampling_par is a
 *   synonym of vbi_raw_decoder, but the (private) decoder fields in
 *   this structure are ignored.
 * @param min_freq Minimum frequency of the noise in Hz.
 * @param max_freq Maximum frequency of the noise in Hz. @a min_freq and
 *   @a max_freq define the cut off frequency at the half power points
 *   (gain -3 dB).
 * @param amplitude Maximum amplitude of the noise, should lie in range
 *   0 to 256.
 * @param seed Seed for the pseudo random number generator built into
 *   this function. Given the same @a seed value the function will add
 *   the same noise, which can be useful for tests.
 *
 * This function adds white noise to a raw VBI image.
 *
 * To produce realistic noise @a min_freq = 0, @a max_freq = 5e6 and
 * @a amplitude = 20 to 50 seems appropriate.
 *
 * @returns
 * FALSE if the @a sp sampling parameters are invalid.
 *
 * @since 0.2.26
 */
vbi_bool
vbi_raw_add_noise (uint8_t * raw,
    const vbi_sampling_par * sp,
    unsigned int min_freq,
    unsigned int max_freq, unsigned int amplitude, unsigned int seed)
{
  double f0, w0, sn, cs, bw, alpha, a0;
  float a1, a2, b0, b1, z0, z1, z2;
  unsigned int n_lines;
  unsigned long samples_per_line;
  unsigned long padding;
  uint32_t seed32;

  assert (NULL != raw);
  assert (NULL != sp);

  if (unlikely (!_vbi_sampling_par_valid_log (sp, /* log */ NULL)))
    return FALSE;

  switch (sp->sp_sample_format) {
    case VBI_PIXFMT_YUV420:
      break;

    default:
      return FALSE;
  }

  if (unlikely (sp->sampling_rate <= 0))
    return FALSE;

  /* Biquad bandpass filter.
     http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt */

  f0 = ((double) min_freq + max_freq) * 0.5;

  if (f0 <= 0.0)
    return TRUE;

  w0 = 2 * M_PI * f0 / sp->sampling_rate;
  vbi_sincos (w0, &sn, &cs);
  bw = fabs (vbi_log2 (MAX (min_freq, max_freq) / f0));
  alpha = sn * sinh (log (2) / 2 * bw * w0 / sn);
  a0 = 1 + alpha;
  a1 = 2 * cs / a0;
  a2 = (alpha - 1) / a0;
  b0 = sn / (2 * a0);
  b1 = 0;

  if (amplitude > 256)
    amplitude = 256;

  n_lines = sp->count[0] + sp->count[1];

  if (unlikely (0 == amplitude || 0 == n_lines || 0 == sp->bytes_per_line))
    return TRUE;

  samples_per_line = sp->bytes_per_line;
  padding = 0;

  seed32 = seed;

  z1 = 0;
  z2 = 0;

  do {
    uint8_t *raw_end = raw + samples_per_line;

    do {
      int noise;

      /* We use our own simple PRNG to produce
         predictable results for tests. */
      seed32 = seed32 * 1103515245u + 12345;
      noise = ((seed32 / 65536) % (amplitude * 2 + 1))
          - amplitude;

      z0 = noise + a1 * z1 + a2 * z2;
      noise = (int) (b0 * (z0 - z2) + b1 * z1);
      z2 = z1;
      z1 = z0;

      *raw++ = SATURATE (*raw + noise, 0, 255);
    } while (raw < raw_end);

    raw += padding;
  } while (--n_lines > 0);

  return TRUE;
}

static vbi_bool
signal_u8 (uint8_t * raw,
    const vbi_sampling_par * sp,
    int blank_level,
    int black_level,
    int white_level,
    unsigned int flags,
    const vbi_sliced * sliced, unsigned int n_sliced_lines, const char *caller)
{
  unsigned int n_scan_lines;
  unsigned int samples_per_line;

  n_scan_lines = sp->count[0] + sp->count[1];
  samples_per_line = SAMPLES_PER_LINE (sp);

  clear_image (raw,
      SATURATE (blank_level, 0, 255),
      samples_per_line, n_scan_lines, sp->bytes_per_line);

  for (; n_sliced_lines-- > 0; ++sliced) {
    unsigned int row;
    uint8_t *raw1;

    if (0 == sliced->line) {
      goto bounds;
    } else if (0 != sp->start[1]
        && sliced->line >= (unsigned int) sp->start[1]) {
      row = sliced->line - sp->start[1];
      if (row >= (unsigned int) sp->count[1])
        goto bounds;

      if (sp->interlaced) {
        row = row * 2 + !(flags & _VBI_RAW_SWAP_FIELDS);
      } else if (0 == (flags & _VBI_RAW_SWAP_FIELDS)) {
        row += sp->count[0];
      }
    } else if (0 != sp->start[0]
        && sliced->line >= (unsigned int) sp->start[0]) {
      row = sliced->line - sp->start[0];
      if (row >= (unsigned int) sp->count[0])
        goto bounds;

      if (sp->interlaced) {
        row *= 2 + !!(flags & _VBI_RAW_SWAP_FIELDS);
      } else if (flags & _VBI_RAW_SWAP_FIELDS) {
        row += sp->count[0];
      }
    } else {
    bounds:
      warn (caller, "Sliced line %u out of bounds.", sliced->line);
      return FALSE;
    }

    raw1 = raw + row * sp->bytes_per_line;

    switch (sliced->id) {
      case VBI_SLICED_TELETEXT_A:      /* ok? */
        signal_teletext (raw1, sp, black_level,
            /* amplitude */ .7 * (white_level
                - black_level),
            /* bit_rate */ 25 * 625 * 397,
            /* FRC */ 0xE7,
            /* payload */ 37,
            sliced);
        break;

      case VBI_SLICED_TELETEXT_B_L10_625:
      case VBI_SLICED_TELETEXT_B_L25_625:
      case VBI_SLICED_TELETEXT_B:
        signal_teletext (raw1, sp, black_level,
            .66 * (white_level - black_level),
            25 * 625 * 444, 0x27, 42, sliced);
        break;

      case VBI_SLICED_TELETEXT_C_625:
        signal_teletext (raw1, sp, black_level,
            .7 * (white_level - black_level), 25 * 625 * 367, 0xE7, 33, sliced);
        break;

      case VBI_SLICED_TELETEXT_D_625:
        signal_teletext (raw1, sp, black_level,
            .7 * (white_level - black_level), 5642787, 0xA7, 34, sliced);
        break;

      case VBI_SLICED_CAPTION_625_F1:
      case VBI_SLICED_CAPTION_625_F2:
      case VBI_SLICED_CAPTION_625:
        signal_closed_caption (raw1, sp,
            blank_level, white_level, flags, 25 * 625 * 32, sliced);
        break;

      case VBI_SLICED_VPS:
      case VBI_SLICED_VPS_F2:
        signal_vps (raw1, sp, black_level, white_level, sliced);
        break;

      case VBI_SLICED_WSS_625:
        signal_wss_625 (raw1, sp, black_level, white_level, sliced);
        break;

      case VBI_SLICED_TELETEXT_B_525:
        signal_teletext (raw1, sp, black_level,
            /* amplitude */ .7 * (white_level
                - black_level),
            /* bit_rate */ 5727272,
            /* FRC */ 0x27,
            /* payload */ 34,
            sliced);
        break;

      case VBI_SLICED_TELETEXT_C_525:
        signal_teletext (raw1, sp, black_level,
            .7 * (white_level - black_level), 5727272, 0xE7, 33, sliced);
        break;

      case VBI_SLICED_TELETEXT_D_525:
        signal_teletext (raw1, sp, black_level,
            .7 * (white_level - black_level), 5727272, 0xA7, 34, sliced);
        break;

      case VBI_SLICED_CAPTION_525_F1:
      case VBI_SLICED_CAPTION_525_F2:
      case VBI_SLICED_CAPTION_525:
        signal_closed_caption (raw1, sp,
            blank_level, white_level, flags, 30000 * 525 * 32 / 1001, sliced);
        break;

      default:
        warn (caller,
            "Service 0x%08x (%s) not supported.",
            sliced->id, vbi_sliced_name (sliced->id));
        return FALSE;
    }
  }

  return TRUE;
}

vbi_bool
_vbi_raw_vbi_image (uint8_t * raw,
    unsigned long raw_size,
    const vbi_sampling_par * sp,
    int blank_level,
    int white_level,
    unsigned int flags, const vbi_sliced * sliced, unsigned int n_sliced_lines)
{
  unsigned int n_scan_lines;
  unsigned int black_level;

  if (unlikely (!_vbi_sampling_par_valid_log (sp, NULL)))
    return FALSE;

  n_scan_lines = sp->count[0] + sp->count[1];
  if (unlikely (n_scan_lines * sp->bytes_per_line > raw_size)) {
    warn (__FUNCTION__,
        "(%u + %u lines) * %lu bytes_per_line "
        "> %lu raw_size.",
        sp->count[0], sp->count[1],
        (unsigned long) sp->bytes_per_line, raw_size);
    return FALSE;
  }

  if (unlikely (0 != white_level && blank_level > white_level)) {
    warn (__FUNCTION__,
        "Invalid blanking %d or peak white level %d.",
        blank_level, white_level);
  }

  if (SYSTEM_525 (sp)) {
    /* Observed value. */
    const unsigned int peak = 200;      /* 255 */

    if (0 == white_level) {
      blank_level = (int) (40.0 * peak / 140);
      black_level = (int) (47.5 * peak / 140);
      white_level = peak;
    } else {
      black_level = (int) (blank_level + 7.5 * (white_level - blank_level));
    }
  } else {
    const unsigned int peak = 200;      /* 255 */

    if (0 == white_level) {
      blank_level = (int) (43.0 * peak / 140);
      white_level = peak;
    }

    black_level = blank_level;
  }

  return signal_u8 (raw, sp,
      blank_level, black_level, white_level,
      flags, sliced, n_sliced_lines, __FUNCTION__);
}

#define RGBA_TO_RGB16(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xFC00) >> (10 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 11)))

#define RGBA_TO_RGBA15(value)						\
	(+(((value) & 0xF8) >> (3 - 0))					\
	 +(((value) & 0xF800) >> (11 - 5))				\
	 +(((value) & 0xF80000) >> (19 - 10))				\
	 +(((value) & 0x80000000) >> (31 - 15)))

#define RGBA_TO_ARGB15(value)						\
	(+(((value) & 0xF8) >> (3 - 1))					\
	 +(((value) & 0xF800) >> (11 - 6))				\
	 +(((value) & 0xF80000) >> (19 - 11))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define RGBA_TO_RGBA12(value)						\
	(+(((value) & 0xF0) >> (4 - 0))					\
	 +(((value) & 0xF000) >> (12 - 4))				\
	 +(((value) & 0xF00000) >> (20 - 8))				\
	 +(((value) & 0xF0000000) >> (28 - 12)))

#define RGBA_TO_ARGB12(value)						\
	(+(((value) & 0xF0) << -(4 - 12))				\
	 +(((value) & 0xF000) >> (12 - 8))				\
	 +(((value) & 0xF00000) >> (20 - 4))				\
	 +(((value) & 0xF0000000) >> (28 - 0)))

#define RGBA_TO_RGB8(value)						\
	(+(((value) & 0xE0) >> (5 - 0))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 6)))

#define RGBA_TO_BGR8(value)						\
	(+(((value) & 0xE0) >> (5 - 5))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 0)))

#define RGBA_TO_RGBA7(value)						\
	(+(((value) & 0xC0) >> (6 - 0))					\
	 +(((value) & 0xE000) >> (13 - 2))				\
	 +(((value) & 0xC00000) >> (22 - 5))				\
	 +(((value) & 0x80000000) >> (31 - 7)))

#define RGBA_TO_ARGB7(value)						\
	(+(((value) & 0xC0) >> (6 - 6))					\
	 +(((value) & 0xE000) >> (13 - 3))				\
	 +(((value) & 0xC00000) >> (22 - 1))				\
	 +(((value) & 0x80000000) >> (31 - 0)))

#define MST1(d, val, mask) (d) = ((d) & ~(mask)) | ((val) & (mask))
#define MST2(d, val, mask) (d) = ((d) & (mask)) | (val)

#define SCAN_LINE_TO_N(conv, n)						\
do {									\
	for (i = 0; i < samples_per_line; ++i) {			\
		uint8_t *dd = d + i * (n);				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask = ~pixel_mask;			\
									\
		value = conv (value) & pixel_mask;			\
		MST2 (dd[0], value >> 0, mask >> 0);			\
		if (n >= 2)						\
			MST2 (dd[1], value >> 8, mask >> 8);		\
		if (n >= 3)						\
			MST2 (dd[2], value >> 16, mask >> 16);		\
		if (n >= 4)						\
			MST2 (dd[3], value >> 24, mask >> 24);		\
	}								\
} while (0)

#define SCAN_LINE_TO_RGB2(conv, endian)					\
do {									\
	for (i = 0; i < samples_per_line; ++i) {			\
		uint8_t *dd = d + i * 2;				\
		unsigned int value = s[i] * 0x01010101;			\
		unsigned int mask;					\
									\
		value = conv (value) & pixel_mask;			\
		mask = ~pixel_mask;		       			\
		MST2 (dd[0 + endian], value >> 0, mask >> 0);		\
		MST2 (dd[1 - endian], value >> 8, mask >> 8);		\
	}								\
} while (0)

vbi_bool
_vbi_raw_video_image (uint8_t * raw,
    unsigned long raw_size,
    const vbi_sampling_par * sp,
    int blank_level,
    int black_level,
    int white_level,
    unsigned int pixel_mask,
    unsigned int flags, const vbi_sliced * sliced, unsigned int n_sliced_lines)
{
  unsigned int n_scan_lines;
  unsigned int samples_per_line;
  vbi_sampling_par sp8;
  unsigned int size;
  uint8_t *buf;
  uint8_t *s;
  uint8_t *d;

  if (unlikely (!_vbi_sampling_par_valid_log (sp, NULL)))
    return FALSE;

  n_scan_lines = sp->count[0] + sp->count[1];
  if (unlikely (n_scan_lines * sp->bytes_per_line > raw_size)) {
    warn (__FUNCTION__,
        "%u + %u lines * %lu bytes_per_line > %lu raw_size.",
        sp->count[0], sp->count[1],
        (unsigned long) sp->bytes_per_line, raw_size);
    return FALSE;
  }

  if (unlikely (0 != white_level
          && (blank_level > black_level || black_level > white_level))) {
    warn (__FUNCTION__,
        "Invalid blanking %d, black %d or peak "
        "white level %d.", blank_level, black_level, white_level);
  }

  switch (sp->sp_sample_format) {
    case VBI_PIXFMT_YVYU:
    case VBI_PIXFMT_VYUY:      /* 0xAAUUVVYY */
      pixel_mask = (+((pixel_mask & 0xFF00) << 8)
          + ((pixel_mask & 0xFF0000) >> 8)
          + ((pixel_mask & 0xFF0000FF)));
      break;

    case VBI_PIXFMT_RGBA24_BE: /* 0xRRGGBBAA */
      pixel_mask = SWAB32 (pixel_mask);
      break;

    case VBI_PIXFMT_BGR24_LE:  /* 0x00RRGGBB */
    case VBI_PIXFMT_BGRA15_LE:
    case VBI_PIXFMT_BGRA15_BE:
    case VBI_PIXFMT_ABGR15_LE:
    case VBI_PIXFMT_ABGR15_BE:
      pixel_mask = (+((pixel_mask & 0xFF) << 16)
          + ((pixel_mask & 0xFF0000) >> 16)
          + ((pixel_mask & 0xFF00FF00)));
      break;

    case VBI_PIXFMT_BGRA24_BE: /* 0xBBGGRRAA */
      pixel_mask = (+((pixel_mask & 0xFFFFFF) << 8)
          + ((pixel_mask & 0xFF000000) >> 24));
      break;

    default:
      break;
  }

  switch (sp->sp_sample_format) {
    case VBI_PIXFMT_RGB16_LE:
    case VBI_PIXFMT_RGB16_BE:
    case VBI_PIXFMT_BGR16_LE:
    case VBI_PIXFMT_BGR16_BE:
      pixel_mask = RGBA_TO_RGB16 (pixel_mask);
      break;

    case VBI_PIXFMT_RGBA15_LE:
    case VBI_PIXFMT_RGBA15_BE:
    case VBI_PIXFMT_BGRA15_LE:
    case VBI_PIXFMT_BGRA15_BE:
      pixel_mask = RGBA_TO_RGBA15 (pixel_mask);
      break;

    case VBI_PIXFMT_ARGB15_LE:
    case VBI_PIXFMT_ARGB15_BE:
    case VBI_PIXFMT_ABGR15_LE:
    case VBI_PIXFMT_ABGR15_BE:
      pixel_mask = RGBA_TO_ARGB15 (pixel_mask);
      break;

    default:
      break;
  }

  if (0 == pixel_mask) {
    /* Done! :-) */
    return TRUE;
  }

  /* ITU-R BT.601 sampling assumed. */

  if (SYSTEM_525 (sp)) {
    if (0 == white_level) {
      /* Cutting off the bottom of the signal
         confuses the vbi_bit_slicer (can't adjust
         the threshold fast enough), probably other
         decoders as well. */
      blank_level = 5;          /* 16 - 40 * 220 / 100; */
      black_level = 16;
      white_level = 16 + 219;
    }
  } else {
    if (0 == white_level) {
      /* Observed values: 30-30-280 (WSS PAL) -? */
      blank_level = 5;          /* 16 - 43 * 220 / 100; */
      black_level = 16;
      white_level = 16 + 219;
    }
  }

  sp8 = *sp;

  samples_per_line = SAMPLES_PER_LINE (sp);

  sp8.sampling_format = VBI_PIXFMT_YUV420;

  sp8.bytes_per_line = samples_per_line * 1 /* bpp */ ;

  size = n_scan_lines * samples_per_line;
  buf = vbi_malloc (size);
  if (NULL == buf) {
    error (NULL, "Out of memory.");
    errno = ENOMEM;
    return FALSE;
  }

  if (!signal_u8 (buf, &sp8,
          blank_level, black_level, white_level,
          flags, sliced, n_sliced_lines, __FUNCTION__)) {
    vbi_free (buf);
    return FALSE;
  }

  s = buf;
  d = raw;

  while (n_scan_lines-- > 0) {
    unsigned int i;

    switch (sp->sp_sample_format) {
      case VBI_PIXFMT_PAL8:
      case VBI_PIXFMT_YUV420:
        for (i = 0; i < samples_per_line; ++i)
          MST1 (d[i], s[i], pixel_mask);
        break;

      case VBI_PIXFMT_RGBA24_LE:
      case VBI_PIXFMT_RGBA24_BE:
      case VBI_PIXFMT_BGRA24_LE:
      case VBI_PIXFMT_BGRA24_BE:
        SCAN_LINE_TO_N (+, 4);
        break;

      case VBI_PIXFMT_RGB24_LE:
      case VBI_PIXFMT_BGR24_LE:
        SCAN_LINE_TO_N (+, 3);
        break;

      case VBI_PIXFMT_YUYV:
      case VBI_PIXFMT_YVYU:
        for (i = 0; i < samples_per_line; i += 2) {
          uint8_t *dd = d + i * 2;
          unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

          MST1 (dd[0], s[i], pixel_mask);
          MST1 (dd[1], uv, pixel_mask >> 8);
          MST1 (dd[2], s[i + 1], pixel_mask);
          MST1 (dd[3], uv, pixel_mask >> 16);
        }
        break;

      case VBI_PIXFMT_UYVY:
      case VBI_PIXFMT_VYUY:
        for (i = 0; i < samples_per_line; i += 2) {
          uint8_t *dd = d + i * 2;
          unsigned int uv = (s[i] + s[i + 1] + 1) >> 1;

          MST1 (dd[0], uv, pixel_mask >> 8);
          MST1 (dd[1], s[i], pixel_mask);
          MST1 (dd[2], uv, pixel_mask >> 16);
          MST1 (dd[3], s[i + 1], pixel_mask);
        }
        break;

      case VBI_PIXFMT_RGB16_LE:
      case VBI_PIXFMT_BGR16_LE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 0);
        break;

      case VBI_PIXFMT_RGB16_BE:
      case VBI_PIXFMT_BGR16_BE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_RGB16, 1);
        break;

      case VBI_PIXFMT_RGBA15_LE:
      case VBI_PIXFMT_BGRA15_LE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 0);
        break;

      case VBI_PIXFMT_RGBA15_BE:
      case VBI_PIXFMT_BGRA15_BE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_RGBA15, 1);
        break;

      case VBI_PIXFMT_ARGB15_LE:
      case VBI_PIXFMT_ABGR15_LE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 0);
        break;

      case VBI_PIXFMT_ARGB15_BE:
      case VBI_PIXFMT_ABGR15_BE:
        SCAN_LINE_TO_RGB2 (RGBA_TO_ARGB15, 1);
        break;

    }

    s += sp8.bytes_per_line;
    d += sp->bytes_per_line;
  }

  vbi_free (buf);

  return TRUE;
}

/*
 * @example examples/rawout.c
 * Raw VBI output example.
 */

/*
 * @param raw A raw VBI image will be stored here.
 * @param raw_size Size of the @a raw buffer in bytes. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of @a sp->bytes_per_line each, with @a sp->samples_per_line
 *   (in libzvbi 0.2.x @a sp->bytes_per_line) bytes actually written.
 * @param sp Describes the raw VBI data to generate. @a sp->sampling_format
 *   must be @c VBI_PIXFMT_Y8 (@c VBI_PIXFMT_YUV420 with libzvbi 0.2.x).
 *   @a sp->synchronous is ignored. Note for compatibility in libzvbi
 *   0.2.x vbi_sampling_par is a synonym of vbi_raw_decoder, but the
 *   (private) decoder fields in this structure are ignored.
 * @param blank_level The level of the horizontal blanking in the raw
 *   VBI image. Must be <= @a white_level.
 * @param white_level The peak white level in the raw VBI image. Set to
 *   zero to get the default blanking and white level.
 * @param swap_fields If @c TRUE the second field will be stored first
 *   in the @c raw buffer. Note you can also get an interlaced image
 *   by setting @a sp->interlaced to @c TRUE. @a sp->synchronous is
 *   ignored.
 * @param sliced Pointer to an array of vbi_sliced containing the
 *   VBI data to be encoded.
 * @param n_sliced_lines Number of elements in the @a sliced array.
 *
 * This function basically reverses the operation of the vbi_raw_decoder,
 * taking sliced VBI data and generating a raw VBI image similar to those
 * you would get from raw VBI sampling hardware. The following data services
 * are currently supported: All Teletext services, VPS, WSS 625, Closed
 * Caption 525 and 625.
 *
 * The function encodes sliced data as is, e.g. without adding or
 * checking parity bits, without checking if the line number is correct
 * for the respective data service, or if the signal will fit completely
 * in the given space (@a sp->offset and @a sp->samples_per_line at
 * @a sp->sampling_rate).
 *
 * Apart of the payload the generated video signal is invariable and
 * attempts to be faithful to related standards. You can only change the
 * characteristics of the assumed capture device. Sync pulses and color
 * bursts and not generated if the sampling parameters extend to this area.
 *
 * @note
 * This function is mainly intended for testing purposes. It is optimized
 * for accuracy, not for speed.
 *
 * @returns
 * @c FALSE if the @a raw_size is too small, if the @a sp sampling
 * parameters are invalid, if the signal levels are invalid,
 * if the @a sliced array contains unsupported services or line numbers
 * outside the @a sp sampling parameters.
 *
 * @since 0.2.22
 */
vbi_bool
vbi_raw_vbi_image (uint8_t * raw,
    unsigned long raw_size,
    const vbi_sampling_par * sp,
    int blank_level,
    int white_level,
    vbi_bool swap_fields,
    const vbi_sliced * sliced, unsigned int n_sliced_lines)
{
  return _vbi_raw_vbi_image (raw, raw_size, sp,
      blank_level, white_level,
      swap_fields ? _VBI_RAW_SWAP_FIELDS : 0, sliced, n_sliced_lines);
}

/*
 * @param raw A raw VBI image will be stored here.
 * @param raw_size Size of the @a raw buffer in bytes. The buffer
 *   must be large enough for @a sp->count[0] + count[1] lines
 *   of @a sp->bytes_per_line each, with @a sp->samples_per_line
 *   times bytes per pixel (in libzvbi 0.2.x @a sp->bytes_per_line)
 *   actually written.
 * @param sp Describes the raw VBI data to generate. Note for
 *  compatibility in libzvbi 0.2.x vbi_sampling_par is a synonym of
 *  vbi_raw_decoder, but the (private) decoder fields in this
 *  structure are ignored.
 * @param blank_level The level of the horizontal blanking in the raw
 *   VBI image. Must be <= @a black_level.
 * @param black_level The black level in the raw VBI image. Must be
 *   <= @a white_level.
 * @param white_level The peak white level in the raw VBI image. Set to
 *   zero to get the default blanking, black and white level.
 * @param pixel_mask This mask selects which color or alpha channel
 *   shall contain VBI data. Depending on @a sp->sampling_format it is
 *   interpreted as 0xAABBGGRR or 0xAAVVUUYY. A value of 0x000000FF
 *   for example writes data in "red bits", not changing other
 *   bits in the @a raw buffer. When the @a sp->sampling_format is a
 *   planar YUV the function writes the Y plane only.
 * @param swap_fields If @c TRUE the second field will be stored first
 *   in the @c raw buffer. Note you can also get an interlaced image
 *   by setting @a sp->interlaced to @c TRUE. @a sp->synchronous is
 *   ignored.
 * @param sliced Pointer to an array of vbi_sliced containing the
 *   VBI data to be encoded.
 * @param n_sliced_lines Number of elements in the @a sliced array.
 *
 * Generates a raw VBI image similar to those you get from video
 * capture hardware. Otherwise identical to vbi_raw_vbi_image().
 *
 * @returns
 * @c FALSE if the @a raw_size is too small, if the @a sp sampling
 * parameters are invalid, if the signal levels are invalid,
 * if the @a sliced array contains unsupported services or line numbers
 * outside the @a sp sampling parameters.
 *
 * @since 0.2.22
 */
vbi_bool
vbi_raw_video_image (uint8_t * raw,
    unsigned long raw_size,
    const vbi_sampling_par * sp,
    int blank_level,
    int black_level,
    int white_level,
    unsigned int pixel_mask,
    vbi_bool swap_fields,
    const vbi_sliced * sliced, unsigned int n_sliced_lines)
{
  return _vbi_raw_video_image (raw, raw_size, sp,
      blank_level, black_level,
      white_level, pixel_mask,
      swap_fields ? _VBI_RAW_SWAP_FIELDS : 0, sliced, n_sliced_lines);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
