/*
 *  libzvbi - Bit slicer
 *
 *  Copyright (C) 2000-2007 Michael H. Schimek
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

/* $Id: bit_slicer.c,v 1.16 2008-02-19 00:35:14 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "bit_slicer.h"

#  define VBI_PIXFMT_Y8 VBI_PIXFMT_YUV420
#  define VBI_PIXFMT_RGB24_LE VBI_PIXFMT_RGB24
#  define VBI_PIXFMT_BGR24_LE VBI_PIXFMT_BGR24
#  define VBI_PIXFMT_RGBA24_LE VBI_PIXFMT_RGBA32_LE
#  define VBI_PIXFMT_BGRA24_LE VBI_PIXFMT_BGRA32_LE
#  define VBI_PIXFMT_RGBA24_BE VBI_PIXFMT_RGBA32_BE
#  define VBI_PIXFMT_BGRA24_BE VBI_PIXFMT_BGRA32_BE
#  define vbi_pixfmt_bytes_per_pixel VBI_PIXFMT_BPP

/**
 * $addtogroup BitSlicer Bit Slicer
 * $ingroup Raw
 * $brief Converting a single scan line of raw VBI
 *   data to sliced VBI data.
 *
 * These are low level functions most useful if you want to decode
 * data services not covered by libzvbi. Usually you will want to
 * use the raw VBI decoder, converting several lines of different
 * data services at once.
 */

/* This is time critical, tinker with care.

   What about all these macros? They are templates to avoid a
   pixel format switch within time critical loops. Instead we
   compile bit slicer functions for different pixel formats.

   I would use inline functions for proper type checking, but
   there's no guarantee the compiler really will inline. */

/* Read a green sample, e.g. rrrrrggg gggbbbbb. endian is const. */
#define GREEN2(raw, endian)						\
	(((raw)[0 + endian] + (raw)[1 - endian] * 256) & bs->green_mask)

/* Read a sample with pixfmt conversion. pixfmt is const. */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define GREEN(raw)							\
	 ((VBI_PIXFMT_RGB16_LE == pixfmt) ?				\
	  *(const uint16_t *)(raw) & bs->green_mask :			\
	  ((VBI_PIXFMT_RGB16_BE == pixfmt) ?				\
	   GREEN2 (raw, 1) :						\
	   (raw)[0]))
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#define GREEN(raw)							\
	 ((VBI_PIXFMT_RGB16_LE == pixfmt) ?				\
	  GREEN2 (raw, 0) :						\
	  ((VBI_PIXFMT_RGB16_BE == pixfmt) ?				\
	   *(const uint16_t *)(raw) & bs->green_mask :			\
	   (raw)[0]))
#else
#define GREEN(raw)							\
	 ((VBI_PIXFMT_RGB16_LE == pixfmt) ?				\
	  GREEN2 (raw, 0) :						\
	  ((VBI_PIXFMT_RGB16_BE == pixfmt) ?				\
	   GREEN2 (raw, 1) :						\
	   (raw)[0]))
#endif

/* raw0 = raw[index >> 8], linear interpolated. */
#define SAMPLE(_kind)							\
do {									\
	const uint8_t *r;						\
									\
	r = raw + (i >> 8) * bpp;					\
	raw0 = GREEN (r);						\
	raw1 = GREEN (r + bpp);						\
	raw0 = (int)(raw1 - raw0) * (i & 255) + (raw0 << 8);		\
	if (collect_points) {						\
		points->kind = _kind;					\
		points->index = (raw - raw_start) * 256 + i;		\
		points->level = raw0;					\
		points->thresh = tr;					\
		++points;						\
	}								\
} while (0)

#define PAYLOAD()							\
do {									\
	i = bs->phase_shift; /* current bit position << 8 */		\
	tr *= 256;							\
	c = 0;								\
									\
	for (j = bs->frc_bits; j > 0; --j) {				\
		SAMPLE (VBI3_FRC_BIT);					\
		c = c * 2 + (raw0 >= tr);				\
		i += bs->step; /* next bit */				\
	}								\
									\
	if (c != bs->frc)						\
		return FALSE;						\
									\
	switch (bs->endian) {						\
	case 3: /* bitwise, lsb first */				\
		for (j = 0; j < bs->payload; ++j) {			\
			SAMPLE (VBI3_PAYLOAD_BIT);			\
			c = (c >> 1) + ((raw0 >= tr) << 7);		\
			i += bs->step;					\
			if ((j & 7) == 7)				\
				*buffer++ = c;				\
		}							\
		*buffer = c >> ((8 - bs->payload) & 7);			\
		break;							\
									\
	case 2: /* bitwise, msb first */				\
		for (j = 0; j < bs->payload; ++j) {			\
			SAMPLE (VBI3_PAYLOAD_BIT);			\
			c = c * 2 + (raw0 >= tr);			\
			i += bs->step;					\
			if ((j & 7) == 7)				\
				*buffer++ = c;				\
		}							\
		*buffer = c & ((1 << (bs->payload & 7)) - 1);		\
		break;							\
									\
	case 1: /* octets, lsb first */					\
		for (j = bs->payload; j > 0; --j) {			\
			for (k = 0, c = 0; k < 8; ++k) {		\
				SAMPLE (VBI3_PAYLOAD_BIT);		\
				c += (raw0 >= tr) << k;			\
				i += bs->step;				\
			}						\
			*buffer++ = c;					\
		}							\
		break;							\
									\
	default: /* octets, msb first */				\
		for (j = bs->payload; j > 0; --j) {			\
			for (k = 0; k < 8; ++k) {			\
				SAMPLE (VBI3_PAYLOAD_BIT);		\
				c = c * 2 + (raw0 >= tr);		\
				i += bs->step;				\
			}						\
			*buffer++ = c;					\
		}							\
		break;							\
	}								\
} while (0)

#define CRI()								\
do {									\
	unsigned int tavg;						\
	unsigned char b; /* current bit */				\
									\
	tavg = (t + (oversampling / 2))	/ oversampling;			\
	b = (tavg >= tr);						\
									\
	if (unlikely (b ^ b1)) {					\
		cl = bs->oversampling_rate >> 1;			\
	} else {							\
		cl += bs->cri_rate;					\
									\
		if (cl >= bs->oversampling_rate) {			\
			if (collect_points) {				\
				points->kind = VBI3_CRI_BIT;		\
				points->index = (raw - raw_start) << 8;	\
				points->level = tavg << 8;		\
				points->thresh = tr << 8;		\
				++points;				\
			}						\
									\
			cl -= bs->oversampling_rate;			\
			c = c * 2 + b;					\
			if ((c & bs->cri_mask) == bs->cri) {		\
				PAYLOAD ();				\
				if (collect_points) {			\
					*n_points = points		\
						- points_start;		\
				}					\
				return TRUE;				\
			}						\
		}							\
	}								\
									\
	b1 = b;								\
									\
	if (oversampling > 1)						\
		t += raw1;						\
} while (0)

#define CORE()								\
do {									\
	const uint8_t *raw_start;					\
	unsigned int i, j, k;						\
	unsigned int cl;	/* clock */				\
	unsigned int thresh0;	/* old 0/1 threshold */			\
	unsigned int tr;	/* current threshold */			\
	unsigned int c;		/* current byte */			\
	unsigned int t;		/* t = raw[0] * j + raw[1] * (1 - j) */	\
	unsigned int raw0;	/* oversampling temporary */		\
	unsigned int raw1;						\
	unsigned char b1;	/* previous bit */			\
									\
	thresh0 = bs->thresh;						\
	raw_start = raw;						\
	raw += bs->skip;						\
									\
	cl = 0;								\
	c = 0;								\
	b1 = 0;								\
									\
	for (i = bs->cri_samples; i > 0; --i) {				\
		tr = bs->thresh >> thresh_frac;				\
		raw0 = GREEN (raw);					\
		raw1 = GREEN (raw + bpp);				\
		raw1 -= raw0;						\
		bs->thresh += (int)(raw0 - tr) * (int) ABS ((int) raw1); \
		t = raw0 * oversampling;				\
									\
		for (j = oversampling; j > 0; --j)			\
			CRI ();						\
									\
		raw += bpp;						\
	}								\
									\
	bs->thresh = thresh0;						\
									\
	if (collect_points)						\
		*n_points = points - points_start;			\
									\
	return FALSE;							\
} while (0)

#define BIT_SLICER(fmt, os, tf)						\
static vbi_bool								\
bit_slicer_ ## fmt		(vbi3_bit_slicer *	bs,		\
				 uint8_t *		buffer,		\
				 vbi3_bit_slicer_point *points,		\
				 unsigned int *		n_points,	\
				 const uint8_t *	raw)		\
{									\
	static const vbi_pixfmt pixfmt = VBI_PIXFMT_ ## fmt;		\
	unsigned int bpp =						\
		vbi_pixfmt_bytes_per_pixel (VBI_PIXFMT_ ## fmt);	\
	static const unsigned int oversampling = os;			\
	static const vbi3_bit_slicer_point *points_start = NULL;	\
	static const vbi_bool collect_points = FALSE;			\
	unsigned int thresh_frac = tf;					\
									\
	CORE ();							\
}

#define DEF_THR_FRAC 9

BIT_SLICER (Y8, 4, DEF_THR_FRAC)        /* any format with 0 bytes between Y or G */
    BIT_SLICER (YUYV, 4, DEF_THR_FRAC)  /* 1 byte */
    BIT_SLICER (RGB24_LE, 4, DEF_THR_FRAC)      /* 2 bytes */
    BIT_SLICER (RGBA24_LE, 4, DEF_THR_FRAC)     /* 3 bytes */
    BIT_SLICER (RGB16_LE, 4, bs->thresh_frac)
    BIT_SLICER (RGB16_BE, 4, bs->thresh_frac)
     static const unsigned int LP_AVG = 4;

     static vbi_bool
         low_pass_bit_slicer_Y8 (vbi3_bit_slicer * bs,
    uint8_t * buffer,
    vbi3_bit_slicer_point * points, unsigned int *n_points, const uint8_t * raw)
{
  vbi3_bit_slicer_point *points_start;
  const uint8_t *raw_start;
  unsigned int i, j, k, m;
  unsigned int cl;              /* clock */
  unsigned int thresh0;         /* old 0/1 threshold */
  unsigned int tr;              /* current threshold */
  unsigned int c;               /* current byte */
  unsigned int raw0;            /* oversampling temporary */
  unsigned char b1;             /* previous bit */
  unsigned int bps;
  unsigned int raw0sum;

  points_start = points;

  raw_start = raw;
  raw += bs->skip;

  bps = bs->bytes_per_sample;

  thresh0 = bs->thresh;

  c = -1;
  cl = 0;
  b1 = 0;

  raw0sum = raw[0];
  for (m = bps; m < (bps << LP_AVG); m += bps) {
    raw0sum += raw[m];
  }

  i = bs->cri_samples;

  for (;;) {
    unsigned char b;            /* current bit */

    tr = bs->thresh >> bs->thresh_frac;
    raw0 = raw0sum;
    raw0sum = raw0sum + raw[bps << LP_AVG]
        - raw[0];
    raw += bps;
    bs->thresh += (int) (raw0 - tr)
        * (int) ABS ((int) (raw0sum - raw0));

    b = (raw0 >= tr);

    if (unlikely (b ^ b1)) {
      cl = bs->oversampling_rate >> 1;
    } else {
      cl += bs->cri_rate;

      if (cl >= bs->oversampling_rate) {
        if (unlikely (NULL != points)) {
          points->kind = VBI3_CRI_BIT;
          points->index = (raw - raw_start)
              * 256 / bs->bytes_per_sample + (1 << LP_AVG) * 128;
          points->level = raw0 << (8 - LP_AVG);
          points->thresh = tr << (8 - LP_AVG);
          ++points;
        }

        cl -= bs->oversampling_rate;
        c = c * 2 + b;
        if ((c & bs->cri_mask) == bs->cri) {
          break;
        }
      }
    }

    b1 = b;

    if (0 == --i) {
      bs->thresh = thresh0;

      if (unlikely (NULL != points))
        *n_points = points - points_start;

      return FALSE;
    }
  }

#define LP_SAMPLE(_kind)						\
do {									\
	unsigned int ii = (i >> 8) * bps;				\
									\
	raw0 = raw[ii];							\
	for (m = bps; m < (bps << LP_AVG); m += bps)			\
		raw0 += raw[ii + m];					\
	if (unlikely (NULL != points)) {				\
		points->kind = _kind;					\
		points->index = (raw - raw_start)			\
			* 256 / bs->bytes_per_sample			\
			+ (1 << LP_AVG) * 128				\
			+ ii * 256;					\
		points->level = raw0 << (8 - LP_AVG);			\
		points->thresh = tr << (8 - LP_AVG);			\
		++points;						\
	}								\
} while (0)

  i = bs->phase_shift;          /* current bit position << 8 */
  c = 0;

  for (j = bs->frc_bits; j > 0; --j) {
    LP_SAMPLE (VBI3_FRC_BIT);
    c = c * 2 + (raw0 >= tr);
    i += bs->step;              /* next bit */
  }

  if (c != bs->frc)
    return FALSE;

  c = 0;

  switch (bs->endian) {
    case 3:                    /* bitwise, lsb first */
      for (j = 0; j < bs->payload; ++j) {
        LP_SAMPLE (VBI3_PAYLOAD_BIT);
        c = (c >> 1) + ((raw0 >= tr) << 7);
        i += bs->step;
        if ((j & 7) == 7)
          *buffer++ = c;
      }
      *buffer = c >> ((8 - bs->payload) & 7);
      break;

    case 2:                    /* bitwise, msb first */
      for (j = 0; j < bs->payload; ++j) {
        LP_SAMPLE (VBI3_PAYLOAD_BIT);
        c = c * 2 + (raw0 >= tr);
        i += bs->step;
        if ((j & 7) == 7)
          *buffer++ = c;
      }
      *buffer = c & ((1 << (bs->payload & 7)) - 1);
      break;

    case 1:                    /* octets, lsb first */
      j = bs->payload;
      do {
        for (k = 0; k < 8; ++k) {
          LP_SAMPLE (VBI3_PAYLOAD_BIT);
          c = (c >> 1) + ((raw0 >= tr) << 7);
          i += bs->step;
        }
        *buffer++ = c;
      } while (--j > 0);
      break;

    default:                   /* octets, msb first */
      j = bs->payload;
      do {
        for (k = 0; k < 8; ++k) {
          LP_SAMPLE (VBI3_PAYLOAD_BIT);
          c = c * 2 + (raw0 >= tr);
          i += bs->step;
        }
        *buffer++ = c;
      } while (--j > 0);
      break;
  }

  if (unlikely (NULL != points)) {
    *n_points = points - points_start;
  }

  return TRUE;
}

static vbi_bool
null_function (vbi3_bit_slicer * bs,
    uint8_t * buffer,
    vbi3_bit_slicer_point * points, unsigned int *n_points, const uint8_t * raw)
{
  /* buffer = buffer;              /\* unused *\/ */
  /* points = points; */
  /* n_points = n_points; */
  /* raw = raw; */

  warn (&bs->log, "vbi3_bit_slicer_set_params() not called.");

  return FALSE;
}

/**
 * @param bs Pointer to vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new().
 * @param buffer Output data.
 * @param buffer_size Size of the output buffer. The buffer must be
 +   large enough to store the number of bits given as @a payload_bits to
 *   vbi3_bit_slicer_new().
 * @param points Information about the bits sampled by the bit slicer
 *   are stored here.
 * @param n_points The number of sampling points stored in the
 *   @a points array will be stored here.
 * @param max_points Size of the @a points array. The array must be
 *   large enough to store one sampling point for all @a crc_bits,
 *   @a frc_bits and @a payload_bits given to vbi3_bit_slicer_new().
 * @param raw Input data. At least the number of pixels or samples
 *  given as @a samples_per_line to vbi3_bit_slicer_new().
 * 
 * Like vbi3_bit_slicer_slice() but additionally provides information
 * about where and how bits were sampled. This is mainly interesting
 * for debugging.
 *
 * @returns
 * @c FALSE if the @a buffer or @a points array is too small, if the
 * pixel format is not supported or if the raw data does not contain
 * the expected information, i. e. the CRI/FRC has not been found. In
 * these cases the @a buffer remains unmodified but the @a points
 * array may contain data.
 *
 * @bug
 * Currently this function is only implemented for
 * raw data in planar YUV formats and @c VBI3_PIXFMT_Y8.
 */
vbi_bool
    vbi3_bit_slicer_slice_with_points
    (vbi3_bit_slicer * bs,
    uint8_t * buffer,
    unsigned int buffer_size,
    vbi3_bit_slicer_point * points,
    unsigned int *n_points, unsigned int max_points, const uint8_t * raw)
{
  static const vbi_pixfmt pixfmt = VBI_PIXFMT_Y8;
  static const unsigned int bpp = 1;
  static const unsigned int oversampling = 4;   /* see above */
  static const unsigned int thresh_frac = DEF_THR_FRAC;
  static const vbi_bool collect_points = TRUE;
  vbi3_bit_slicer_point *points_start;

  assert (NULL != bs);
  assert (NULL != buffer);
  assert (NULL != points);
  assert (NULL != n_points);
  assert (NULL != raw);

  points_start = points;
  *n_points = 0;

  if (bs->payload > buffer_size * 8) {
    warn (&bs->log,
        "buffer_size %u < %u bits of payload.", buffer_size * 8, bs->payload);
    return FALSE;
  }

  if (bs->total_bits > max_points) {
    warn (&bs->log,
        "max_points %u < %u CRI, FRC and payload bits.",
        max_points, bs->total_bits);
    return FALSE;
  }

  if (low_pass_bit_slicer_Y8 == bs->func) {
    return bs->func (bs, buffer, points, n_points, raw);
  } else if (bit_slicer_Y8 != bs->func) {
    warn (&bs->log,
        "Function not implemented for pixfmt %u.", bs->sample_format);
    return bs->func (bs, buffer,
        /* points */ NULL,
        /* n_points */ NULL,
        raw);
  }

  CORE ();
}

/**
 * @param bs Pointer to vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new(). You must also call
 *   vbi3_bit_slicer_set_params() before calling this function.
 * @param buffer Output data.
 * @param buffer_size Size of the output buffer. The buffer must be
 +   large enough to store the number of bits given as @a payload to
 *   vbi3_bit_slicer_new().
 * @param raw Input data. At least the number of pixels or samples
 *  given as @a samples_per_line to vbi3_bit_slicer_new().
 * 
 * Decodes one scan line of raw vbi data. Note the bit slicer tries
 * to adapt to the average signal amplitude, you should avoid
 * using the same vbi3_bit_slicer object for data from different
 * devices.
 *
 * @return
 * @c FALSE if the @a buffer is too small or if the raw data does not
 * contain the expected information, i. e. the CRI/FRC has not been
 * found. This may also result from a too weak or noisy signal. Error
 * correction must be implemented at a higher layer. When the function
 * fails, the @a buffer remains unmodified.
 */
vbi_bool
vbi3_bit_slicer_slice (vbi3_bit_slicer * bs,
    uint8_t * buffer, unsigned int buffer_size, const uint8_t * raw)
{
  assert (NULL != bs);
  assert (NULL != buffer);
  assert (NULL != raw);

  if (bs->payload > buffer_size * 8) {
    warn (&bs->log,
        "buffer_size %u < %u bits of payload.", buffer_size * 8, bs->payload);
    return FALSE;
  }

  return bs->func (bs, buffer,
      /* points */ NULL,
      /* n_points */ NULL,
      raw);
}

/**
 * @param bs Pointer to vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new().
 * @param sample_format Format of the raw data, see vbi3_pixfmt.
 *   Note the bit slicer looks only at the green component of RGB
 *   pixels.
 * @param sampling_rate Raw vbi sampling rate in Hz, that is the number
 *   of samples or pixels sampled per second by the hardware.
 * @param sample_offset The bit slicer shall skip this number of samples at
 *   the start of the line.
 * @param samples_per_line Number of samples or pixels in one raw vbi
 *   line later passed to vbi3_bit_slicer_slice(). This limits the number of
 *   bytes read from the raw data buffer. Do not to confuse the value
 *   with bytes per line.
 * @param cri The Clock Run In is a NRZ modulated sequence of '1'
 *   and '0' bits prepending most data transmissions to synchronize data
 *   acquisition circuits. The bit slicer compares the bits in this
 *   word, lsb last transmitted, against the transmitted CRI. Decoding
 *   of FRC and payload starts with the next bit after a match, thus
 *   @a cri must contain a unique bit sequence. For example 0xAB to
 *   match '101010101011xxx'.
 * @param cri_mask Of the CRI bits in @a cri, only these bits are
 *   significant for a match. For instance it is wise not to rely on
 *   the very first CRI bits transmitted.
 * @param cri_bits Number of CRI bits, must not exceed 32.
 * @param cri_rate CRI bit rate in Hz, the number of CRI bits
 *   transmitted per second.
 * @param cri_end Number of samples between the start of the line and
 *   the latest possible end of the CRI. This is useful when
 *   the transmission is much shorter than samples_per_line, otherwise
 *   just pass @c ~0 and a limit will be calculated.
 * @param frc The FRaming Code usually following the CRI is a bit
 *   sequence identifying the data service. There is no mask parameter,
 *   all bits must match. We assume FRC has the same @a modulation as
 *   the payload and is transmitted at @a payload_rate.
 * @param frc_bits Number of FRC bits, must not exceed 32.
 * @param payload_bits Number of payload bits. Only this data
 *   will be stored in the vbi3_bit_slicer_slice() output. If this number
 *   is no multiple of eight, the most significant bits of the
 *   last byte are undefined.
 * @param payload_rate Payload bit rate in Hz, the number of payload
 *   bits transmitted per second.
 * @param modulation Modulation of the payload, see vbi3_modulation.
 * 
 * Initializes a vbi3_bit_slicer object for use with
 * vbi3_bit_slicer_slice(). This is a low level function, see also
 * vbi3_raw_decoder_new().
 *
 * @returns
 * @c FALSE when the parameters are invalid (e. g.
 * @a samples_per_line too small to contain CRI, FRC and payload).
 */
vbi_bool
vbi3_bit_slicer_set_params (vbi3_bit_slicer * bs,
    vbi_pixfmt sample_format,
    unsigned int sampling_rate,
    unsigned int sample_offset,
    unsigned int samples_per_line,
    unsigned int cri,
    unsigned int cri_mask,
    unsigned int cri_bits,
    unsigned int cri_rate,
    unsigned int cri_end,
    unsigned int frc,
    unsigned int frc_bits,
    unsigned int payload_bits,
    unsigned int payload_rate, vbi3_modulation modulation)
{
  unsigned int c_mask;
  unsigned int f_mask;
  unsigned int min_samples_per_bit;
  unsigned int oversampling;
  unsigned int data_bits;
  unsigned int data_samples;
  unsigned int cri_samples;
  unsigned int skip;

  assert (NULL != bs);
  assert (cri_bits <= 32);
  assert (frc_bits <= 32);
  assert (payload_bits <= 32767);
  assert (samples_per_line <= 32767);

  if (cri_rate > sampling_rate) {
    warn (&bs->log, "cri_rate %u > sampling_rate %u.", cri_rate, sampling_rate);
    goto failure;
  }

  if (payload_rate > sampling_rate) {
    warn (&bs->log,
        "payload_rate %u > sampling_rate %u.", payload_rate, sampling_rate);
    goto failure;
  }

  min_samples_per_bit = sampling_rate / MAX (cri_rate, payload_rate);

  bs->sample_format = sample_format;

  c_mask = (cri_bits == 32) ? ~0U : (1U << cri_bits) - 1;
  f_mask = (frc_bits == 32) ? ~0U : (1U << frc_bits) - 1;

  oversampling = 4;
  skip = 0;

  /* 0-1 threshold, start value. */
  bs->thresh = 105 << DEF_THR_FRAC;
  bs->thresh_frac = DEF_THR_FRAC;

  switch (sample_format) {
    case VBI_PIXFMT_YUV420:
      bs->bytes_per_sample = 1;
      bs->func = bit_slicer_Y8;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;


    case VBI_PIXFMT_YUYV:
    case VBI_PIXFMT_YVYU:
      bs->bytes_per_sample = 2;
      bs->func = bit_slicer_YUYV;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;

    case VBI_PIXFMT_UYVY:
    case VBI_PIXFMT_VYUY:
      skip = 1;
      bs->bytes_per_sample = 2;
      bs->func = bit_slicer_YUYV;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;

    case VBI_PIXFMT_RGBA24_LE:
    case VBI_PIXFMT_BGRA24_LE:
      skip = 1;
      bs->bytes_per_sample = 4;
      bs->func = bit_slicer_RGBA24_LE;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;

    case VBI_PIXFMT_RGBA24_BE:
    case VBI_PIXFMT_BGRA24_BE:
      skip = 2;
      bs->bytes_per_sample = 4;
      bs->func = bit_slicer_RGBA24_LE;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;

    case VBI_PIXFMT_RGB24_LE:
    case VBI_PIXFMT_BGR24_LE:
      skip = 1;
      bs->bytes_per_sample = 3;
      bs->func = bit_slicer_RGB24_LE;
      if (min_samples_per_bit > (3U << (LP_AVG - 1))) {
        bs->func = low_pass_bit_slicer_Y8;
        oversampling = 1;
        bs->thresh <<= LP_AVG - 2;
        bs->thresh_frac += LP_AVG - 2;
      }
      break;

    case VBI_PIXFMT_RGB16_LE:
    case VBI_PIXFMT_BGR16_LE:
      bs->func = bit_slicer_RGB16_LE;
      bs->green_mask = 0x07E0;
      bs->thresh = 105 << (5 - 2 + 12);
      bs->thresh_frac = 12;
      bs->bytes_per_sample = 2;
      break;

    case VBI_PIXFMT_RGB16_BE:
    case VBI_PIXFMT_BGR16_BE:
      bs->func = bit_slicer_RGB16_BE;
      bs->green_mask = 0x07E0;
      bs->thresh = 105 << (5 - 2 + 12);
      bs->thresh_frac = 12;
      bs->bytes_per_sample = 2;
      break;

    case VBI_PIXFMT_RGBA15_LE:
    case VBI_PIXFMT_BGRA15_LE:
      bs->func = bit_slicer_RGB16_LE;
      bs->green_mask = 0x03E0;
      bs->thresh = 105 << (5 - 3 + 11);
      bs->thresh_frac = 11;
      bs->bytes_per_sample = 2;
      break;

    case VBI_PIXFMT_RGBA15_BE:
    case VBI_PIXFMT_BGRA15_BE:
      bs->func = bit_slicer_RGB16_BE;
      bs->green_mask = 0x03E0;
      bs->thresh = 105 << (5 - 3 + 11);
      bs->thresh_frac = 11;
      bs->bytes_per_sample = 2;
      break;

    case VBI_PIXFMT_ARGB15_LE:
    case VBI_PIXFMT_ABGR15_LE:
      bs->func = bit_slicer_RGB16_LE;
      bs->green_mask = 0x07C0;
      bs->thresh = 105 << (6 - 3 + 12);
      bs->thresh_frac = 12;
      bs->bytes_per_sample = 2;
      break;

    case VBI_PIXFMT_ARGB15_BE:
    case VBI_PIXFMT_ABGR15_BE:
      bs->func = bit_slicer_RGB16_BE;
      bs->green_mask = 0x07C0;
      bs->thresh = 105 << (6 - 3 + 12);
      bs->thresh_frac = 12;
      bs->bytes_per_sample = 2;
      break;


    default:
      warn (&bs->log,
          "Unknown sample_format 0x%x.", (unsigned int) sample_format);
      return FALSE;
  }

  bs->skip = sample_offset * bs->bytes_per_sample + skip;

  bs->cri_mask = cri_mask & c_mask;
  bs->cri = cri & bs->cri_mask;

  /* We stop searching for CRI when CRI, FRC and payload
     cannot possibly fit anymore. Additionally this eliminates
     a data end check in the payload loop. */
  cri_samples = (sampling_rate * (int64_t) cri_bits) / cri_rate;

  data_bits = payload_bits + frc_bits;
  data_samples = (sampling_rate * (int64_t) data_bits) / payload_rate;

  bs->total_bits = cri_bits + data_bits;

  if ((sample_offset > samples_per_line)
      || ((cri_samples + data_samples)
          > (samples_per_line - sample_offset))) {
    warn (&bs->log,
        "%u samples_per_line too small for "
        "sample_offset %u + %u cri_bits (%u samples) "
        "+ %u frc_bits and %u payload_bits "
        "(%u samples).",
        samples_per_line, sample_offset,
        cri_bits, cri_samples, frc_bits, payload_bits, data_samples);
    goto failure;
  }

  cri_end = MIN (cri_end, samples_per_line - data_samples);

  bs->cri_samples = cri_end - sample_offset;
  bs->cri_rate = cri_rate;

  bs->oversampling_rate = sampling_rate * oversampling;

  bs->frc = frc & f_mask;
  bs->frc_bits = frc_bits;

  /* Payload bit distance in 1/256 raw samples. */
  bs->step = (sampling_rate * (int64_t) 256) / payload_rate;

  if (payload_bits & 7) {
    /* Use bit routines. */
    bs->payload = payload_bits;
    bs->endian = 3;
  } else {
    /* Use faster octet routines. */
    bs->payload = payload_bits >> 3;
    bs->endian = 1;
  }

  switch (modulation) {
    case VBI3_MODULATION_NRZ_MSB:
      --bs->endian;

      /* fall through */

    case VBI3_MODULATION_NRZ_LSB:
      bs->phase_shift = (int)
          (sampling_rate * 256.0 / cri_rate * .5 + bs->step * .5 + 128);
      break;

    case VBI3_MODULATION_BIPHASE_MSB:
      --bs->endian;

      /* fall through */

    case VBI3_MODULATION_BIPHASE_LSB:
      /* Phase shift between the NRZ modulated CRI and the
         biphase modulated rest. */
      bs->phase_shift = (int)
          (sampling_rate * 256.0 / cri_rate * .5 + bs->step * .25 + 128);
      break;
  }

  return TRUE;

failure:
  bs->func = null_function;

  return FALSE;
}

void
vbi3_bit_slicer_set_log_fn (vbi3_bit_slicer * bs,
    vbi_log_mask mask, vbi_log_fn * log_fn, void *user_data)
{
  assert (NULL != bs);

  if (NULL == log_fn)
    mask = 0;

  bs->log.mask = mask;
  bs->log.fn = log_fn;
  bs->log.user_data = user_data;
}

/**
 * @internal
 */
void
_vbi3_bit_slicer_destroy (vbi3_bit_slicer * bs)
{
  assert (NULL != bs);

  /* Make unusable. */
  CLEAR (*bs);
}

/**
 * @internal
 */
vbi_bool
_vbi3_bit_slicer_init (vbi3_bit_slicer * bs)
{
  assert (NULL != bs);

  CLEAR (*bs);

  bs->func = null_function;

  return TRUE;
}

/**
 * @param bs Pointer to a vbi3_bit_slicer object allocated with
 *   vbi3_bit_slicer_new(), can be NULL.
 *
 * Deletes a vbi3_bit_slicer object.
 */
void
vbi3_bit_slicer_delete (vbi3_bit_slicer * bs)
{
  if (NULL == bs)
    return;

  _vbi3_bit_slicer_destroy (bs);

  vbi_free (bs);
}

/**
 * Allocates a new vbi3_bit_slicer object.
 *
 * @returns
 * @c NULL when out of memory.
 */
vbi3_bit_slicer *
vbi3_bit_slicer_new (void)
{
  vbi3_bit_slicer *bs;

  bs = vbi_malloc (sizeof (*bs));
  if (NULL == bs) {
    return NULL;
  }

  _vbi3_bit_slicer_init (bs);

  return bs;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
