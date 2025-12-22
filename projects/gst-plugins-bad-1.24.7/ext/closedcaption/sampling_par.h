/*
 *  libzvbi -- Raw VBI sampling parameters
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

/* $Id: sampling_par.h,v 1.9 2008-02-24 14:17:06 mschimek Exp $ */

#ifndef __SAMPLING_PAR_H__
#define __SAMPLING_PAR_H__

#include "decoder.h"

VBI_BEGIN_DECLS

/* Public */

typedef vbi_raw_decoder vbi_sampling_par;

#define VBI_VIDEOSTD_SET_EMPTY 0
#define VBI_VIDEOSTD_SET_PAL_BG 1
#define VBI_VIDEOSTD_SET_625_50 1
#define VBI_VIDEOSTD_SET_525_60 2
#define VBI_VIDEOSTD_SET_ALL 3
typedef uint64_t vbi_videostd_set;

/* Private */

extern vbi_service_set
vbi_sampling_par_from_services (vbi_sampling_par *    sp,
				unsigned int *         max_rate,
				vbi_videostd_set      videostd_set,
				vbi_service_set       services);
extern vbi_service_set
vbi_sampling_par_check_services
                                (const vbi_sampling_par *sp,
                                 vbi_service_set       services,
                                 unsigned int           strict)
  _vbi_pure;

extern vbi_videostd_set
_vbi_videostd_set_from_scanning	(int			scanning);

extern vbi_service_set
_vbi_sampling_par_from_services_log
                                (vbi_sampling_par *    sp,
                                 unsigned int *         max_rate,
                                 vbi_videostd_set      videostd_set,
                                 vbi_service_set       services,
                                 _vbi_log_hook *       log);
extern vbi_service_set
_vbi_sampling_par_check_services_log
                                (const vbi_sampling_par *sp,
                                 vbi_service_set       services,
                                 unsigned int           strict,
                                 _vbi_log_hook *       log)
  _vbi_pure;
extern vbi_bool
_vbi_sampling_par_valid_log    (const vbi_sampling_par *sp,
                                 _vbi_log_hook *       log)
  _vbi_pure;

VBI_END_DECLS

#endif /* __SAMPLING_PAR_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
