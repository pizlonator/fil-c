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

/* $Id: io-sim.h,v 1.11 2013-08-28 14:45:23 mschimek Exp $ */

#ifndef __ZVBI_IO_SIM_H__
#define __ZVBI_IO_SIM_H__

#include "macros.h"
#include "sampling_par.h"

VBI_BEGIN_DECLS

/* Public */

/**
 * @addtogroup Rawenc
 * @{
 */
extern vbi_bool
vbi_raw_video_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			blank_level,
				 int			black_level,
				 int			white_level,
				 unsigned int		pixel_mask,
				 vbi_bool		swap_fields,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);
extern vbi_bool
vbi_raw_add_noise		(uint8_t *		raw,
				 const vbi_sampling_par *sp,
				 unsigned int		min_freq,
				 unsigned int		max_freq,
				 unsigned int		amplitude,
				 unsigned int		seed);
extern vbi_bool
vbi_raw_vbi_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			blank_level,
				 int			white_level,
				 vbi_bool		swap_fields,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);

#define _VBI_RAW_SWAP_FIELDS	(1 << 0)
#define _VBI_RAW_SHIFT_CC_CRI	(1 << 1)
#define _VBI_RAW_LOW_AMP_CC	(1 << 2)

/* NB. Currently this flag has no effect in _vbi_raw_*_image().
   Call vbi_raw_add_noise() instead. */
#define _VBI_RAW_NOISE_2	(1 << 17)

extern vbi_bool
_vbi_raw_video_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			blank_level,
				 int			black_level,
				 int			white_level,
				 unsigned int		pixel_mask,
				 unsigned int		flags,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);
extern vbi_bool
_vbi_raw_vbi_image		(uint8_t *		raw,
				 unsigned long		raw_size,
				 const vbi_sampling_par *sp,
				 int			blank_level,
				 int			white_level,
				 unsigned int		flags,
				 const vbi_sliced *	sliced,
				 unsigned int		n_sliced_lines);

VBI_END_DECLS

#endif /* __ZVBI_IO_SIM_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
