/*
 *  libzvbi -- Raw VBI decoder
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: raw_decoder.h,v 1.12 2008-02-19 00:35:21 mschimek Exp $ */

#ifndef __ZVBI_RAW_DECODER_H__
#define __ZVBI_RAW_DECODER_H__

#include <stdio.h>

#include "decoder.h"
#include "sampling_par.h"
#include "bit_slicer.h"

VBI_BEGIN_DECLS

/*
 * $ingroup RawDecoder
 * $brief Raw VBI decoder.
 *
 * The contents of this structure are private.
 * Call vbi3_raw_decoder_new() to allocate a raw VBI decoder.
 */
typedef struct _vbi3_raw_decoder vbi3_raw_decoder;

/*
 * $addtogroup RawDecoder
 * ${
 */
extern vbi_bool
vbi3_raw_decoder_sampling_point	(vbi3_raw_decoder *	rd,
				 vbi3_bit_slicer_point *point,
				 unsigned int		row,
				 unsigned int		nth_bit);
extern unsigned int
vbi3_raw_decoder_decode		(vbi3_raw_decoder *	rd,
				 vbi_sliced *		sliced,
				 unsigned int		sliced_lines,
				 const uint8_t *	raw);
extern void
vbi3_raw_decoder_reset		(vbi3_raw_decoder *	rd);
extern vbi_service_set
vbi3_raw_decoder_services	(vbi3_raw_decoder *	rd);
extern vbi_service_set
vbi3_raw_decoder_remove_services
				(vbi3_raw_decoder *	rd,
				 vbi_service_set	services);
extern vbi_service_set
vbi3_raw_decoder_add_services	(vbi3_raw_decoder *	rd,
				 vbi_service_set	services,
				 int			strict);
extern vbi_bool
vbi3_raw_decoder_debug		(vbi3_raw_decoder *	rd,
				 vbi_bool		enable);
extern vbi_service_set
vbi3_raw_decoder_set_sampling_par
				(vbi3_raw_decoder *	rd,
				 const vbi_sampling_par *sp,
				 int			strict);
extern void
vbi3_raw_decoder_get_sampling_par
				(const vbi3_raw_decoder *rd,
				 vbi_sampling_par *	sp);
extern void
vbi3_raw_decoder_set_log_fn	(vbi3_raw_decoder *	rd,
				 vbi_log_fn *		log_fn,
				 void *			user_data,
				 vbi_log_mask		mask);
extern void
vbi3_raw_decoder_delete		(vbi3_raw_decoder *	rd);
extern vbi3_raw_decoder *
vbi3_raw_decoder_new		(const vbi_sampling_par *sp);

/* $} */

/* Private */

/** @internal */
#define _VBI3_RAW_DECODER_MAX_JOBS 8
/** @internal */
#define _VBI3_RAW_DECODER_MAX_WAYS 8

/** @internal */
typedef struct {
	vbi_service_set		id;
	vbi3_bit_slicer		slicer;
} _vbi3_raw_decoder_job;

/** @internal */
typedef struct {
	vbi3_bit_slicer_point   points[512];
	unsigned int		n_points;
} _vbi3_raw_decoder_sp_line;

/**
 * @internal
 * Don't dereference pointers to this structure.
 * I guarantee it will change.
 */
struct _vbi3_raw_decoder {
	vbi_sampling_par	sampling;

	vbi_service_set		services;

	_vbi_log_hook		log;
	vbi_bool		debug;

	unsigned int		n_jobs;
	unsigned int		n_sp_lines;
	int			readjust;
	int8_t *		pattern;	/* n scan lines * MAX_WAYS */
	_vbi3_raw_decoder_job	jobs[_VBI3_RAW_DECODER_MAX_JOBS];
	_vbi3_raw_decoder_sp_line *sp_lines;
};

/** @internal */
typedef enum {
	/** Requires field line numbers. */
	_VBI_SP_LINE_NUM	= (1 << 0),
	/** Requires field numbers. */
	_VBI_SP_FIELD_NUM	= (1 << 1),
} _vbi_service_par_flag;

typedef struct _vbi_service_par _vbi_service_par;

/** @internal */
struct _vbi_service_par {
	vbi_service_set		id;
	const char *		label;

	/**
	 * Video standard
	 * - 525 lines, FV = 59.94 Hz, FH = 15734 Hz
	 * - 625 lines, FV = 50 Hz, FH = 15625 Hz
	 */
	vbi_videostd_set	videostd_set;

	/**
	 * Most scan lines used by the data service, first and last
	 * line of first and second field. ITU-R numbering scheme.
	 * Zero if no data from this field, requires field sync.
	 */
	unsigned int		first[2];
        unsigned int		last[2];

	/**
	 * Leading edge hsync to leading edge first CRI one bit,
	 * half amplitude points, in nanoseconds.
	 */
	unsigned int		offset;

	unsigned int		cri_rate;	/**< Hz */
	unsigned int		bit_rate;	/**< Hz */

	/** Clock Run In and FRaming Code, LSB last txed bit of FRC. */
	unsigned int		cri_frc;

	/** CRI and FRC bits significant for identification. */
	unsigned int		cri_frc_mask;

	/**
	 * Number of significat cri_bits (at cri_rate),
	 * frc_bits (at bit_rate).
	 */
	unsigned int		cri_bits;
	unsigned int		frc_bits;

	unsigned int		payload;	/**< bits */
	vbi_modulation		modulation;

	_vbi_service_par_flag	flags;
};

extern const _vbi_service_par _vbi_service_table [];

extern void
_vbi3_raw_decoder_dump		(const vbi3_raw_decoder *rd,
				 FILE *			fp);
extern void
_vbi3_raw_decoder_destroy	(vbi3_raw_decoder *	rd);
extern vbi_bool
_vbi3_raw_decoder_init		(vbi3_raw_decoder *	rd,
				 const vbi_sampling_par *sp);

VBI_END_DECLS

#endif /* __ZVBI_RAW_DECODER_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
