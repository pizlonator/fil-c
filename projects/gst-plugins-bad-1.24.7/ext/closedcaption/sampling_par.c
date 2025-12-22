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

/* $Id: sampling_par.c,v 1.12 2013-08-28 14:45:00 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>

#include "misc.h"
#include "raw_decoder.h"
#include "sampling_par.h"
#include "sliced.h"

#  define vbi_pixfmt_bytes_per_pixel VBI_PIXFMT_BPP
#  define sp_sample_format sampling_format

/**
 * @addtogroup Sampling Raw VBI sampling
 * @ingroup Raw
 * @brief Raw VBI data sampling interface.
 */

/**
 * @internal
 * Compatibility.
 */
vbi_videostd_set
_vbi_videostd_set_from_scanning (int scanning)
{
  switch (scanning) {
    case 525:
      return VBI_VIDEOSTD_SET_525_60;

    case 625:
      return VBI_VIDEOSTD_SET_625_50;

    default:
      break;
  }

  return 0;
}

_vbi_inline vbi_bool
range_check (unsigned int start,
    unsigned int count, unsigned int min, unsigned int max)
{
  /* Check bounds and overflow. */
  return (start >= min && (start + count) <= max && (start + count) >= start);
}

/**
 * @internal
 * @param sp Sampling parameters to verify.
 * 
 * @return
 * TRUE if the sampling parameters are valid (as far as we can tell).
 */
vbi_bool
_vbi_sampling_par_valid_log (const vbi_sampling_par * sp, _vbi_log_hook * log)
{
  vbi_videostd_set videostd_set;
  unsigned int bpp;

  assert (NULL != sp);

  switch (sp->sp_sample_format) {
    case VBI_PIXFMT_YUV420:
      /* This conflicts with the ivtv driver, which returns an
         odd number of bytes per line.  The driver format is
         _GREY but libzvbi 0.2 has no VBI_PIXFMT_Y8. */
      break;

    default:
      bpp = vbi_pixfmt_bytes_per_pixel (sp->sp_sample_format);
      if (0 != (sp->bytes_per_line % bpp))
        goto bad_samples;
      break;
  }

  if (0 == sp->bytes_per_line)
    goto no_samples;

  if (0 == sp->count[0]
      && 0 == sp->count[1])
    goto bad_range;

  videostd_set = _vbi_videostd_set_from_scanning (sp->scanning);

  if (VBI_VIDEOSTD_SET_525_60 & videostd_set) {
    if (VBI_VIDEOSTD_SET_625_50 & videostd_set)
      goto ambiguous;

    if (0 != sp->start[0]
        && !range_check (sp->start[0], sp->count[0], 1, 262))
      goto bad_range;

    if (0 != sp->start[1]
        && !range_check (sp->start[1], sp->count[1], 263, 525))
      goto bad_range;
  } else if (VBI_VIDEOSTD_SET_625_50 & videostd_set) {
    if (0 != sp->start[0]
        && !range_check (sp->start[0], sp->count[0], 1, 311))
      goto bad_range;

    if (0 != sp->start[1]
        && !range_check (sp->start[1], sp->count[1], 312, 625))
      goto bad_range;
  } else {
  ambiguous:
    info (log, "Ambiguous videostd_set 0x%lx.", (unsigned long) videostd_set);
    return FALSE;
  }

  if (sp->interlaced && (sp->count[0] != sp->count[1]
          || 0 == sp->count[0])) {
    info (log,
        "Line counts %u, %u must be equal and "
        "non-zero when raw VBI data is interlaced.",
        sp->count[0], sp->count[1]);
    return FALSE;
  }

  return TRUE;

no_samples:
  info (log, "samples_per_line is zero.");
  return FALSE;


bad_samples:
  info (log,
      "bytes_per_line value %u is no multiple of "
      "the sample size %u.",
      sp->bytes_per_line, vbi_pixfmt_bytes_per_pixel (sp->sp_sample_format));
  return FALSE;

bad_range:
  info (log,
      "Invalid VBI scan range %u-%u (%u lines), "
      "%u-%u (%u lines).",
      sp->start[0], sp->start[0] + sp->count[0] - 1,
      sp->count[0],
      sp->start[1], sp->start[1] + sp->count[1] - 1, sp->count[1]);
  return FALSE;
}

static vbi_bool
    _vbi_sampling_par_permit_service
    (const vbi_sampling_par * sp,
    const _vbi_service_par * par, unsigned int strict, _vbi_log_hook * log)
{
  const unsigned int unknown = 0;
  double signal;
  unsigned int field;
  unsigned int samples_per_line;
  vbi_videostd_set videostd_set;

  assert (NULL != sp);
  assert (NULL != par);

  videostd_set = _vbi_videostd_set_from_scanning (sp->scanning);
  if (0 == (par->videostd_set & videostd_set)) {
    info (log,
        "Service 0x%08x (%s) requires "
        "videostd_set 0x%lx, "
        "have 0x%lx.",
        par->id, par->label,
        (unsigned long) par->videostd_set, (unsigned long) videostd_set);
    return FALSE;
  }

  if (par->flags & _VBI_SP_LINE_NUM) {
    if ((par->first[0] > 0 && unknown == (unsigned int) sp->start[0])
        || (par->first[1] > 0 && unknown == (unsigned int) sp->start[1])) {
      info (log,
          "Service 0x%08x (%s) requires known "
          "line numbers.", par->id, par->label);
      return FALSE;
    }
  }

  {
    unsigned int rate;

    rate = MAX (par->cri_rate, par->bit_rate);

    switch (par->id) {
      case VBI_SLICED_WSS_625:
        /* Effective bit rate is just 1/3 max_rate,
           so 1 * max_rate should suffice. */
        break;

      default:
        rate = (rate * 3) >> 1;
        break;
    }

    if (rate > (unsigned int) sp->sampling_rate) {
      info (log,
          "Sampling rate %f MHz too low "
          "for service 0x%08x (%s).",
          sp->sampling_rate / 1e6, par->id, par->label);
      return FALSE;
    }
  }

  signal = par->cri_bits / (double) par->cri_rate
      + (par->frc_bits + par->payload) / (double) par->bit_rate;

  samples_per_line = sp->bytes_per_line / VBI_PIXFMT_BPP (sp->sampling_format);

  if (0 && sp->offset > 0 && strict > 0) {
    double sampling_rate;
    double offset;
    double end;

    sampling_rate = (double) sp->sampling_rate;

    offset = sp->offset / sampling_rate;
    end = (sp->offset + samples_per_line) / sampling_rate;

    if (offset > (par->offset / 1e3 - 0.5e-6)) {
      info (log,
          "Sampling starts at 0H + %f us, too "
          "late for service 0x%08x (%s) at "
          "%f us.", offset * 1e6, par->id, par->label, par->offset / 1e3);
      return FALSE;
    }

    if (end < (par->offset / 1e9 + signal + 0.5e-6)) {
      info (log,
          "Sampling ends too early at 0H + "
          "%f us for service 0x%08x (%s) "
          "which ends at %f us",
          end * 1e6,
          par->id, par->label, par->offset / 1e3 + signal * 1e6 + 0.5);
      return FALSE;
    }
  } else {
    double samples;

    samples = samples_per_line / (double) sp->sampling_rate;

    if (strict > 0)
      samples -= 1e-6;          /* headroom */

    if (samples < signal) {
      info (log,
          "Service 0x%08x (%s) signal length "
          "%f us exceeds %f us sampling length.",
          par->id, par->label, signal * 1e6, samples * 1e6);
      return FALSE;
    }
  }

  if ((par->flags & _VBI_SP_FIELD_NUM)
      && !sp->synchronous) {
    info (log,
        "Service 0x%08x (%s) requires "
        "synchronous field order.", par->id, par->label);
    return FALSE;
  }

  for (field = 0; field < 2; ++field) {
    unsigned int start;
    unsigned int end;

    start = sp->start[field];
    end = start + sp->count[field] - 1;

    if (0 == par->first[field]
        || 0 == par->last[field]) {
      /* No data on this field. */
      continue;
    }

    if (0 == sp->count[field]) {
      info (log,
          "Service 0x%08x (%s) requires "
          "data from field %u", par->id, par->label, field + 1);
      return FALSE;
    }

    /* (int) <= 0 for compatibility with libzvbi 0.2.x */
    if ((int) strict <= 0 || 0 == sp->start[field])
      continue;

    if (1 == strict && par->first[field] > par->last[field]) {
      /* May succeed if not all scanning lines
         available for the service are actually used. */
      continue;
    }

    if (start > par->first[field]
        || end < par->last[field]) {
      info (log,
          "Service 0x%08x (%s) requires "
          "lines %u-%u, have %u-%u.",
          par->id, par->label, par->first[field], par->last[field], start, end);
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * @internal
 */
vbi_service_set
    _vbi_sampling_par_check_services_log
    (const vbi_sampling_par * sp,
    vbi_service_set services, unsigned int strict, _vbi_log_hook * log)
{
  const _vbi_service_par *par;
  vbi_service_set rservices;

  assert (NULL != sp);

  rservices = 0;

  for (par = _vbi_service_table; par->id; ++par) {
    if (0 == (par->id & services))
      continue;

    if (_vbi_sampling_par_permit_service (sp, par, strict, log))
      rservices |= par->id;
  }

  return rservices;
}

/**
 * @internal
 */
vbi_service_set
    _vbi_sampling_par_from_services_log
    (vbi_sampling_par * sp,
    unsigned int *max_rate,
    vbi_videostd_set videostd_set_req,
    vbi_service_set services, _vbi_log_hook * log)
{
  const _vbi_service_par *par;
  vbi_service_set rservices;
  vbi_videostd_set videostd_set;
  unsigned int rate;
  unsigned int samples_per_line;

  assert (NULL != sp);

  videostd_set = 0;

  if (0 != videostd_set_req) {
    if (0 == (VBI_VIDEOSTD_SET_ALL & videostd_set_req)
        || ((VBI_VIDEOSTD_SET_525_60 & videostd_set_req)
            && (VBI_VIDEOSTD_SET_625_50 & videostd_set_req))) {
      warn (log,
          "Ambiguous videostd_set 0x%lx.", (unsigned long) videostd_set_req);
      CLEAR (*sp);
      return 0;
    }

    videostd_set = videostd_set_req;
  }

  samples_per_line = 0;
  sp->sampling_rate = 27000000; /* ITU-R BT.601 */
  sp->offset = (int) (64e-6 * sp->sampling_rate);
  sp->start[0] = 30000;
  sp->count[0] = 0;
  sp->start[1] = 30000;
  sp->count[1] = 0;
  sp->interlaced = FALSE;
  sp->synchronous = TRUE;

  rservices = 0;
  rate = 0;

  for (par = _vbi_service_table; par->id; ++par) {
#if 0                           /* Set but unused */
    double margin;
#endif
    double signal;
    int offset;
    unsigned int samples;
    unsigned int i;

    if (0 == (par->id & services))
      continue;

    if (0 == videostd_set_req) {
      vbi_videostd_set set;

      set = par->videostd_set | videostd_set;

      if (0 == (set & ~VBI_VIDEOSTD_SET_525_60)
          || 0 == (set & ~VBI_VIDEOSTD_SET_625_50))
        videostd_set |= par->videostd_set;
    }
#if 0                           /* Set but unused */
    if (VBI_VIDEOSTD_SET_525_60 & videostd_set)
      margin = 1.0e-6;
    else
      margin = 2.0e-6;
#endif

    if (0 == (par->videostd_set & videostd_set)) {
      info (log,
          "Service 0x%08x (%s) requires "
          "videostd_set 0x%lx, "
          "have 0x%lx.",
          par->id, par->label,
          (unsigned long) par->videostd_set, (unsigned long) videostd_set);
      continue;
    }

    rate = MAX (rate, par->cri_rate);
    rate = MAX (rate, par->bit_rate);

    signal = par->cri_bits / (double) par->cri_rate
        + ((par->frc_bits + par->payload) / (double) par->bit_rate);

    offset = (int) ((par->offset / 1e9) * sp->sampling_rate);
    samples = (int) ((signal + 1.0e-6) * sp->sampling_rate);

    sp->offset = MIN (sp->offset, offset);

    samples_per_line = MAX (samples_per_line + sp->offset,
        samples + offset) - sp->offset;

    for (i = 0; i < 2; ++i)
      if (par->first[i] > 0 && par->last[i] > 0) {
        sp->start[i] = MIN
            ((unsigned int) sp->start[i], (unsigned int) par->first[i]);
        sp->count[i] = MAX ((unsigned int) sp->start[i]
            + sp->count[i], (unsigned int) par->last[i] + 1)
            - sp->start[i];
      }

    rservices |= par->id;
  }

  if (0 == rservices) {
    CLEAR (*sp);
    return 0;
  }

  if (0 == sp->count[1]) {
    sp->start[1] = 0;

    if (0 == sp->count[0]) {
      sp->start[0] = 0;
      sp->offset = 0;
    }
  } else if (0 == sp->count[0]) {
    sp->start[0] = 0;
  }

  sp->scanning = (videostd_set & VBI_VIDEOSTD_SET_525_60)
      ? 525 : 625;
  sp->sp_sample_format = VBI_PIXFMT_YUV420;

  /* Note bpp is 1. */
  sp->bytes_per_line = MAX (1440U, samples_per_line);

  if (max_rate)
    *max_rate = rate;

  return rservices;
}

/**
 * @param sp Sampling parameters to check against.
 * @param services Set of data services.
 * @param strict See description of vbi_raw_decoder_add_services().
 *
 * Check which of the given services can be decoded with the given
 * sampling parameters at the given strictness level.
 *
 * @return
 * Subset of @a services decodable with the given sampling parameters.
 */
vbi_service_set
    vbi_sampling_par_check_services
    (const vbi_sampling_par * sp, vbi_service_set services, unsigned int strict)
{
  return _vbi_sampling_par_check_services_log (sp, services, strict,
      /* log_hook */ NULL);
}

/**
 * @param sp Sampling parameters calculated by this function
 *   will be stored here.
 * @param max_rate If not NULL, the highest data bit rate in Hz of
 *   all services requested will be stored here. The sampling rate
 *   should be at least twice as high; @sp sampling_rate will
 *   be set to a more reasonable value of 27 MHz, which is twice
 *   the video sampling rate defined by ITU-R Rec. BT.601.
 * @param videostd_set Create sampling parameters matching these
 *   video standards. When 0 determine video standard from requested
 *   services.
 * @param services Set of VBI_SLICED_ symbols. Here (and only here) you
 *   can add @c VBI_SLICED_VBI_625 or @c VBI_SLICED_VBI_525 to include all
 *   vbi scan lines in the calculated sampling parameters.
 *
 * Calculate the sampling parameters required to receive and decode the
 * requested data @a services. The @a sp sampling_format will be
 * @c VBI_PIXFMT_Y8, offset and bytes_per_line will be set to
 * reasonable minimums. This function can be used to initialize hardware
 * prior to creating a vbi_raw_decoder object.
 * 
 * @return
 * Subset of @a services covered by the calculated sampling parameters.
 */
vbi_service_set
vbi_sampling_par_from_services (vbi_sampling_par * sp,
    unsigned int *max_rate,
    vbi_videostd_set videostd_set, vbi_service_set services)
{
  return _vbi_sampling_par_from_services_log (sp, max_rate,
      videostd_set, services,
      /* log_hook */ NULL);
}


/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
