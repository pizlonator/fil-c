/*
 *  libzvbi -- Bit slicer
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

/* $Id: bit_slicer.h,v 1.11 2008-02-24 14:18:13 mschimek Exp $ */

#ifndef __ZVBI_BIT_SLICER_H__
#define __ZVBI_BIT_SLICER_H__

#include "sampling_par.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup BitSlicer
 * @{
 */

/**
 * @brief Modulation used for VBI data transmission.
 */
typedef enum {
	/**
	 * The data is 'non-return to zero' coded, logical '1' bits
	 * are described by high sample values, logical '0' bits by
	 * low values. The data is last significant bit first transmitted.
	 */
	VBI3_MODULATION_NRZ_LSB,

	/**
	 * 'Non-return to zero' coded, most significant bit first
	 * transmitted.
	 */
	VBI3_MODULATION_NRZ_MSB,

	/**
	 * The data is 'bi-phase' coded. Each data bit is described
	 * by two complementary signalling elements, a logical '1'
	 * by a sequence of '10' elements, a logical '0' by a '01'
	 * sequence. The data is last significant bit first transmitted.
	 */
	VBI3_MODULATION_BIPHASE_LSB,

	/** 'Bi-phase' coded, most significant bit first transmitted. */
	VBI3_MODULATION_BIPHASE_MSB
} vbi3_modulation;

/**
 * @brief Bit slicer context.
 *
 * The contents of this structure are private.
 * Call vbi3_bit_slicer_new() to allocate a bit slicer context.
 */
typedef struct _vbi3_bit_slicer vbi3_bit_slicer;

typedef enum {
	VBI3_CRI_BIT = 1,
	VBI3_FRC_BIT,
	VBI3_PAYLOAD_BIT
} vbi3_bit_slicer_bit;

/**
 * @brief Bit slicer sampling point.
 *
 * This structure contains information about
 * a bit sampled by the bit slicer.
 */
typedef struct {
	/** Whether this struct refers to a CRI, FRC or payload bit. */
	vbi3_bit_slicer_bit	kind;

	/** Number of the sample times 256. */
	unsigned int		index;

	/** Signal amplitude at this sample, in range 0 to 65535. */
	unsigned int		level;

	/** 0/1 threshold at this sample, in range 0 to 65535. */
	unsigned int		thresh;
} vbi3_bit_slicer_point;

extern vbi_bool
vbi3_bit_slicer_slice_with_points
				(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 unsigned int		buffer_size,
				 vbi3_bit_slicer_point *points,
				 unsigned int *		n_points,
				 unsigned int		max_points,
				 const uint8_t *	raw)
  _vbi_nonnull ((1, 2, 4, 5, 7));
extern vbi_bool
vbi3_bit_slicer_slice		(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 unsigned int		buffer_size,
				 const uint8_t *	raw)
  _vbi_nonnull ((1, 2, 4));
extern vbi_bool
vbi3_bit_slicer_set_params	(vbi3_bit_slicer *	bs,
				 vbi_pixfmt		sample_format,
				 unsigned int		sampling_rate,
				 unsigned int		sample_offset,
				 unsigned int		samples_per_line,
				 unsigned int		cri,
				 unsigned int		cri_mask,
				 unsigned int		cri_bits,
				 unsigned int		cri_rate,
				 unsigned int		cri_end,
				 unsigned int		frc,
				 unsigned int		frc_bits,
				 unsigned int		payload_bits,
				 unsigned int		payload_rate,
				 vbi3_modulation	modulation)
  _vbi_nonnull ((1));
extern void
vbi3_bit_slicer_set_log_fn	(vbi3_bit_slicer *	bs,
				 vbi_log_mask		mask,
				 vbi_log_fn *		log_fn,
				 void *			user_data)
  _vbi_nonnull ((1));
extern void
vbi3_bit_slicer_delete		(vbi3_bit_slicer *	bs);
extern vbi3_bit_slicer *
vbi3_bit_slicer_new		(void)
  _vbi_alloc;

/* Private */

typedef vbi_bool
_vbi3_bit_slicer_fn		(vbi3_bit_slicer *	bs,
				 uint8_t *		buffer,
				 vbi3_bit_slicer_point *points,
				 unsigned int *		n_points,
				 const uint8_t *	raw);

/** @internal */
struct _vbi3_bit_slicer {
	_vbi3_bit_slicer_fn *	func;

	vbi_pixfmt		sample_format;
	unsigned int		cri;
	unsigned int		cri_mask;
	unsigned int		thresh;
	unsigned int		thresh_frac;
	unsigned int		cri_samples;
	unsigned int		cri_rate;
	unsigned int		oversampling_rate;
	unsigned int		phase_shift;
	unsigned int		step;
	unsigned int		frc;
	unsigned int		frc_bits;
	unsigned int		total_bits;
	unsigned int		payload;
	unsigned int		endian;
	unsigned int		bytes_per_sample;
	unsigned int		skip;
	unsigned int		green_mask;

	_vbi_log_hook		log;
};

extern void
_vbi3_bit_slicer_destroy	(vbi3_bit_slicer *	bs)
  _vbi_nonnull ((1));
extern vbi_bool
_vbi3_bit_slicer_init		(vbi3_bit_slicer *	bs)
  _vbi_nonnull ((1));

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_BIT_SLICER_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
