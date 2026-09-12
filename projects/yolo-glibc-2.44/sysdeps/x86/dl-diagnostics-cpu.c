/* Print CPU diagnostics data in ld.so.  x86 version.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <dl-diagnostics.h>

#include <array_length.h>
#include <cpu-features.h>
#include <cpuid.h>
#include <dl-iterate_cpu.h>
#include <ldsodefs.h>
#include <stdbool.h>
#include <string.h>
#include <sysdep.h>

/* The generic CPUID dumping code.  */
static void _dl_diagnostics_cpuid (void);

static void
print_cpu_features_value (const char *label, uint64_t value)
{
  _dl_printf ("x86.cpu_features.");
  _dl_diagnostics_print_labeled_value (label, value);
}

static void
print_cpu_feature_internal (unsigned int index, const char *kind,
                            unsigned int reg, uint32_t value)
{
  _dl_printf ("x86.cpu_features.features[0x%x].%s[0x%x]=0x%x\n",
              index, kind, reg, value);
}

static void
print_cpu_feature_preferred (const char *label, unsigned int flag)
{
  _dl_printf("x86.cpu_features.preferred.%s=0x%x\n", label, flag);
}

void
_dl_diagnostics_cpu (void)
{
  const struct cpu_features *cpu_features = __get_cpu_features ();

  print_cpu_features_value ("basic.kind", cpu_features->basic.kind);
  print_cpu_features_value ("basic.max_cpuid", cpu_features->basic.max_cpuid);
  print_cpu_features_value ("basic.family", cpu_features->basic.family);
  print_cpu_features_value ("basic.model", cpu_features->basic.model);
  print_cpu_features_value ("basic.stepping", cpu_features->basic.stepping);

  for (unsigned int index = 0; index < CPUID_INDEX_MAX; ++index)
    {
      /* The index values are part of the ABI via
         <sys/platform/x86.h>, so translating them to strings is not
         necessary.  */
      for (unsigned int reg = 0; reg < 4; ++reg)
        print_cpu_feature_internal
          (index, "cpuid", reg,
           cpu_features->features[index].cpuid_array[reg]);
      for (unsigned int reg = 0; reg < 4; ++reg)
        print_cpu_feature_internal
          (index, "active", reg,
           cpu_features->features[index].active_array[reg]);
    }

  /* The preferred indicators are not part of the ABI and need to be
     translated.  */
#define BIT(x) \
  print_cpu_feature_preferred (#x, CPU_FEATURE_PREFERRED_P (cpu_features, x));
#include "cpu-features-preferred_feature_index_1.def"
#undef BIT

  print_cpu_features_value ("isa_1", cpu_features->isa_1);
  print_cpu_features_value ("xsave_state_size",
                            cpu_features->xsave_state_size);
  print_cpu_features_value ("xsave_state_full_size",
                            cpu_features->xsave_state_full_size);
  print_cpu_features_value ("tlsdesc_state_full_size",
                            _dl_x86_features_tlsdesc_state_size);
  print_cpu_features_value ("data_cache_size", cpu_features->data_cache_size);
  print_cpu_features_value ("shared_cache_size",
                            cpu_features->shared_cache_size);
  print_cpu_features_value ("non_temporal_threshold",
                            cpu_features->non_temporal_threshold);
  print_cpu_features_value ("memset_non_temporal_threshold",
                            cpu_features->memset_non_temporal_threshold);
  print_cpu_features_value ("rep_movsb_threshold",
                            cpu_features->rep_movsb_threshold);
  print_cpu_features_value ("rep_movsb_stop_threshold",
                            cpu_features->rep_movsb_stop_threshold);
  print_cpu_features_value ("rep_stosb_threshold",
                            cpu_features->rep_stosb_threshold);
  print_cpu_features_value ("level1_icache_size",
                            cpu_features->level1_icache_size);
  print_cpu_features_value ("level1_icache_linesize",
                            cpu_features->level1_icache_linesize);
  print_cpu_features_value ("level1_dcache_size",
                            cpu_features->level1_dcache_size);
  print_cpu_features_value ("level1_dcache_assoc",
                            cpu_features->level1_dcache_assoc);
  print_cpu_features_value ("level1_dcache_linesize",
                            cpu_features->level1_dcache_linesize);
  print_cpu_features_value ("level2_cache_size",
                            cpu_features->level2_cache_size);
  print_cpu_features_value ("level2_cache_assoc",
                            cpu_features->level2_cache_assoc);
  print_cpu_features_value ("level2_cache_linesize",
                            cpu_features->level2_cache_linesize);
  print_cpu_features_value ("level3_cache_size",
                            cpu_features->level3_cache_size);
  print_cpu_features_value ("level3_cache_assoc",
                            cpu_features->level3_cache_assoc);
  print_cpu_features_value ("level3_cache_linesize",
                            cpu_features->level3_cache_linesize);
  print_cpu_features_value ("level4_cache_size",
                            cpu_features->level4_cache_size);
  print_cpu_features_value ("cachesize_non_temporal_divisor",
			    cpu_features->cachesize_non_temporal_divisor);
  _Static_assert (
      offsetof (struct cpu_features, cachesize_non_temporal_divisor)
	      + sizeof (cpu_features->cachesize_non_temporal_divisor)
	  == sizeof (*cpu_features),
      "last cpu_features field has been printed");

  _dl_diagnostics_cpuid ();
}

/* The following code implements a generic CPUID dumper that tries to
   gather CPUID data without knowing about CPUID implementation
   details.  */

/* Register arguments to CPUID.  Multiple ECX subleaf values yielding
   the same result are combined, to shorten the output.  Both
   identical matches (EAX to EDX are the same) and matches where EAX,
   EBX, EDX, and ECX are equal except in the lower byte, which must
   match the query ECX value.  The latter is needed to compress ranges
   on CPUs which preserve the lowest byte in ECX if an unknown leaf is
   queried.  */
struct cpuid_query
{
  unsigned int eax;
  unsigned ecx_first;
  unsigned ecx_last;
  bool ecx_preserves_query_byte;
};

/* Single integer value that can be used for sorting/ordering
   comparisons.  Uses Q->eax and Q->ecx_first only because ecx_last is
   always greater than the previous ecx_first value and less than the
   subsequent one.  */
static inline unsigned long long int
cpuid_query_combined (struct cpuid_query *q)
{
  /* ecx can be -1 (that is, ~0U).  If this happens, this the only ecx
     value for this eax value, so the ordering does not matter.  */
  return ((unsigned long long int) q->eax << 32) | (unsigned int) q->ecx_first;
};

/* Used for differential reporting of zero/non-zero values.  */
static const struct cpuid_registers cpuid_registers_zero;

/* Register arguments to CPUID paired with the results that came back.  */
struct cpuid_query_result
{
  struct cpuid_query q;
  struct cpuid_registers r;
};

/* During a first enumeration pass, we try to collect data for
  cpuid_initial_subleaf_limit subleaves per leaf/EAX value.  If we run
  out of space, we try once more with applying the lower limit.  */
enum { cpuid_main_leaf_limit = 128 };
enum { cpuid_initial_subleaf_limit = 512 };
enum { cpuid_subleaf_limit = 32 };

/* Offset of the extended leaf area.  */
enum {cpuid_extended_leaf_offset = 0x80000000 };

/* Collected CPUID data.  Everything is stored in a statically sized
   array that is sized so that the second pass will collect some data
   for all leaves, after the limit is applied.  On the second pass,
   ecx_limit is set to cpuid_subleaf_limit.  */
struct cpuid_collected_data
{
  unsigned int used;
  unsigned int ecx_limit;
  uint64_t xgetbv_ecx_0;
  struct cpuid_query_result qr[cpuid_main_leaf_limit
                               * 2 * cpuid_subleaf_limit];
};

/* Fill in the result of a CPUID query.  Returns true if there is
   room, false if nothing could be stored.  */
static bool
_dl_diagnostics_cpuid_store (struct cpuid_collected_data *ccd,
                             unsigned eax, int ecx)
{
  if (ccd->used >= array_length (ccd->qr))
    return false;

  /* Tentatively fill in the next value.  */
  __cpuid_count (eax, ecx,
                 ccd->qr[ccd->used].r.eax,
                 ccd->qr[ccd->used].r.ebx,
                 ccd->qr[ccd->used].r.ecx,
                 ccd->qr[ccd->used].r.edx);

  /* If the ECX subleaf is next subleaf after the previous one (for
     the same leaf), and the values are the same, merge the result
     with the already-stored one.  Do this before skipping zero
     leaves, which avoids artifiacts for ECX == 256 queries.  */
  if (ccd->used > 0
      && ccd->qr[ccd->used - 1].q.eax == eax
      && ccd->qr[ccd->used - 1].q.ecx_last + 1 == ecx)
    {
      /* Exact match of the previous result. Ignore the value of
         ecx_preserves_query_byte if this is a singleton range so far
         because we can treat ECX as fixed if the same value repeats.  */
      if ((!ccd->qr[ccd->used - 1].q.ecx_preserves_query_byte
           || (ccd->qr[ccd->used - 1].q.ecx_first
               == ccd->qr[ccd->used - 1].q.ecx_last))
          && memcmp (&ccd->qr[ccd->used - 1].r, &ccd->qr[ccd->used].r,
                     sizeof (ccd->qr[ccd->used].r)) == 0)
        {
          ccd->qr[ccd->used - 1].q.ecx_last = ecx;
          /* ECX is now fixed because the same value has been observed
             twice, even if we had a low-byte match before.  */
          ccd->qr[ccd->used - 1].q.ecx_preserves_query_byte = false;
          return true;
        }
      /* Match except for the low byte in ECX, which must match the
         incoming ECX value.  */
      if (ccd->qr[ccd->used - 1].q.ecx_preserves_query_byte
          && (ecx & 0xff) == (ccd->qr[ccd->used].r.ecx & 0xff)
          && ccd->qr[ccd->used].r.eax == ccd->qr[ccd->used - 1].r.eax
          && ccd->qr[ccd->used].r.ebx == ccd->qr[ccd->used - 1].r.ebx
          && ((ccd->qr[ccd->used].r.ecx & 0xffffff00)
              == (ccd->qr[ccd->used - 1].r.ecx & 0xffffff00))
          && ccd->qr[ccd->used].r.edx == ccd->qr[ccd->used - 1].r.edx)
        {
          ccd->qr[ccd->used - 1].q.ecx_last = ecx;
          return true;
        }
    }

  /* Do not store zero results.  All-zero values usually mean that the
     subleaf is unsupported.  */
  if (ccd->qr[ccd->used].r.eax == 0
      && ccd->qr[ccd->used].r.ebx == 0
      && ccd->qr[ccd->used].r.ecx == 0
      && ccd->qr[ccd->used].r.edx == 0)
    return true;

  /* The result needs to be stored.  Fill in the query parameters and
     consume the storage.  */
  ccd->qr[ccd->used].q.eax = eax;
  ccd->qr[ccd->used].q.ecx_first = ecx;
  ccd->qr[ccd->used].q.ecx_last = ecx;
  ccd->qr[ccd->used].q.ecx_preserves_query_byte
    = (ecx & 0xff) == (ccd->qr[ccd->used].r.ecx & 0xff);
  ++ccd->used;
  return true;
}

/* Collected CPUID data into *CCD.  If LIMIT, apply per-leaf limits to
   avoid exceeding the pre-allocated space.  Return true if all data
   could be stored, false if the retrying without a limit is
   requested.  */
static bool
_dl_diagnostics_cpuid_collect_1 (struct cpuid_collected_data *ccd, bool limit)
{
  ccd->used = 0;
  ccd->ecx_limit
    = (limit ? cpuid_subleaf_limit : cpuid_initial_subleaf_limit) - 1;
  _dl_diagnostics_cpuid_store (ccd, 0x00, 0x00);
  if (ccd->used == 0)
    /* CPUID reported all 0.  Should not happen.  */
    return true;
  unsigned int maximum_leaf = ccd->qr[0x00].r.eax;
  if (limit && maximum_leaf >= cpuid_main_leaf_limit)
    maximum_leaf = cpuid_main_leaf_limit - 1;

  for (unsigned int eax = 1; eax <= maximum_leaf; ++eax)
    {
      for (unsigned int ecx = 0; ecx <= ccd->ecx_limit; ++ecx)
        if (!_dl_diagnostics_cpuid_store (ccd, eax, ecx))
          return false;
    }

  if (!_dl_diagnostics_cpuid_store (ccd, cpuid_extended_leaf_offset, 0x00))
    return false;
  maximum_leaf = ccd->qr[ccd->used - 1].r.eax;
  if (maximum_leaf < cpuid_extended_leaf_offset)
    /* No extended CPUID information.  */
    return true;
  if (limit
      && maximum_leaf - cpuid_extended_leaf_offset >= cpuid_main_leaf_limit)
    maximum_leaf = cpuid_extended_leaf_offset + cpuid_main_leaf_limit - 1;
  for (unsigned int eax = cpuid_extended_leaf_offset + 1;
       eax <= maximum_leaf; ++eax)
    {
      for (unsigned int ecx = 0; ecx <= ccd->ecx_limit; ++ecx)
        if (!_dl_diagnostics_cpuid_store (ccd, eax, ecx))
          return false;
    }
  return true;
}

/* Call _dl_diagnostics_cpuid_collect_1 twice if necessary, the
   second time with the limit applied.  */
static void
_dl_diagnostics_cpuid_collect (struct cpuid_collected_data *ccd)
{
  if (!_dl_diagnostics_cpuid_collect_1 (ccd, false))
    _dl_diagnostics_cpuid_collect_1 (ccd, true);

  /* Re-use the result of the official feature probing here.  */
  const struct cpu_features *cpu_features = __get_cpu_features ();
  if (CPU_FEATURES_CPU_P (cpu_features, OSXSAVE))
    {
      unsigned int xcrlow;
      unsigned int xcrhigh;
      asm ("xgetbv" : "=a" (xcrlow), "=d" (xcrhigh) : "c" (0));
      ccd->xgetbv_ecx_0 = ((uint64_t) xcrhigh << 32) + xcrlow;
    }
  else
    ccd->xgetbv_ecx_0 = 0;
}

/* Print a CPUID register value (passed as REG_VALUE) if it differs
   from the expected REG_REFERENCE value.  PROCESSOR_INDEX is the
   process sequence number (always starting at zero; not a kernel ID).  */
static void
_dl_diagnostics_cpuid_print_reg (unsigned int processor_index,
                                 const struct cpuid_query *q,
                                 const char *reg_label, unsigned int reg_value,
                                 bool subleaf)
{
  if (subleaf)
    _dl_printf ("x86.processor[0x%x].cpuid.subleaf_eax[0x%x]"
                ".ecx[0x%x].%s=0x%x\n",
                processor_index, q->eax, q->ecx_first, reg_label, reg_value);
  else
    _dl_printf ("x86.processor[0x%x].cpuid.eax[0x%x].%s=0x%x\n",
                processor_index, q->eax, reg_label, reg_value);
}

/* Print CPUID result values in *RESULT for the query in
   CCD->qr[CCD_IDX].  PROCESSOR_INDEX is the process sequence number
   (always starting at zero; not a kernel ID).  */
static void
_dl_diagnostics_cpuid_print_query (unsigned int processor_index,
                                   struct cpuid_collected_data *ccd,
                                   unsigned int ccd_idx,
                                   const struct cpuid_registers *result)
{
  /* Treat this as a value if subleaves if ecx isn't zero (maybe
     within the [ecx_fist, ecx_last] range), or if eax matches its
     neighbors.  If the range is [0, ecx_limit], then the subleaves
     are not distinct (independently of ecx_preserves_query_byte),
     so do not report them separately.  */
  struct cpuid_query *q = &ccd->qr[ccd_idx].q;
  bool subleaf = (q->ecx_first > 0
                  || (q->ecx_first != q->ecx_last
                      && !(q->ecx_first == 0 && q->ecx_last == ccd->ecx_limit))
                  || (ccd_idx > 0 && q->eax == ccd->qr[ccd_idx - 1].q.eax)
                  || (ccd_idx + 1 < ccd->used
                      && q->eax == ccd->qr[ccd_idx + 1].q.eax));
  _dl_diagnostics_cpuid_print_reg (processor_index, q, "eax", result->eax,
                                   subleaf);
  _dl_diagnostics_cpuid_print_reg (processor_index, q, "ebx", result->ebx,
                                   subleaf);
  _dl_diagnostics_cpuid_print_reg (processor_index, q, "ecx", result->ecx,
                                   subleaf);
  _dl_diagnostics_cpuid_print_reg (processor_index, q, "edx", result->edx,
                                   subleaf);

  if (subleaf && q->ecx_first != q->ecx_last)
    {
      _dl_printf ("x86.processor[0x%x].cpuid.subleaf_eax[0x%x]"
                  ".ecx[0x%x].until_ecx=0x%x\n",
                  processor_index, q->eax, q->ecx_first, q->ecx_last);
      if (q->ecx_preserves_query_byte)
        _dl_printf ("x86.processor[0x%x].cpuid.subleaf_eax[0x%x]"
                    ".ecx[0x%x].ecx_query_mask=0xff\n",
                    processor_index, q->eax, q->ecx_first);
    }
}

/* Perform differential reporting of the data in *CURRENT against
   *BASE.  REQUESTED_CPU is the kernel CPU ID the thread was
   configured to run on, or -1 if no configuration was possible.
   PROCESSOR_INDEX is the process sequence number (always starting at
   zero; not a kernel ID).  */
static void
_dl_diagnostics_cpuid_report (struct dl_iterate_cpu *dci,
                              struct cpuid_collected_data *current,
                              struct cpuid_collected_data *base)
{
  if (dci->requested_cpu >= 0)
    _dl_printf ("x86.processor[0x%x].requested=0x%x\n",
                dci->processor_index, dci->requested_cpu);
  if (dci->actual_cpu >= 0)
    _dl_printf ("x86.processor[0x%x].observed=0x%x\n",
                dci->processor_index, dci->actual_cpu);
  if (dci->actual_node >= 0)
    _dl_printf ("x86.processor[0x%x].observed_node=0x%x\n",
                dci->processor_index, dci->actual_node);

  _dl_printf ("x86.processor[0x%x].cpuid_leaves=0x%x\n",
              dci->processor_index, current->used);
  _dl_printf ("x86.processor[0x%x].ecx_limit=0x%x\n",
              dci->processor_index, current->ecx_limit);

  unsigned int base_idx = 0;
  for (unsigned int current_idx = 0; current_idx < current->used;
       ++current_idx)
    {
      /* Report missing data on the current CPU as 0.  */
      unsigned long long int current_query
        = cpuid_query_combined (&current->qr[current_idx].q);
      while (base_idx < base->used
             && cpuid_query_combined (&base->qr[base_idx].q) < current_query)
      {
        _dl_diagnostics_cpuid_print_query (dci->processor_index,
                                           base, base_idx,
                                           &cpuid_registers_zero);
        ++base_idx;
      }

      if (base_idx < base->used
          && cpuid_query_combined (&base->qr[base_idx].q) == current_query)
        {
          _Static_assert (sizeof (struct cpuid_registers) == 4 * 4,
                          "no padding in struct cpuid_registers");
          if (current->qr[current_idx].q.ecx_last
              != base->qr[base_idx].q.ecx_last
              || memcmp (&current->qr[current_idx].r,
                         &base->qr[base_idx].r,
                         sizeof (struct cpuid_registers)) != 0)
              /* The ECX range or the values have changed.  Show the
                 new values.  */
            _dl_diagnostics_cpuid_print_query (dci->processor_index,
                                               current, current_idx,
                                               &current->qr[current_idx].r);
          ++base_idx;
        }
      else
        /* Data is absent in the base reference.  Report the new data.  */
        _dl_diagnostics_cpuid_print_query (dci->processor_index,
                                           current, current_idx,
                                           &current->qr[current_idx].r);
    }

  if (current->xgetbv_ecx_0 != base->xgetbv_ecx_0)
    {
      /* Re-use the 64-bit printing routine.  */
      _dl_printf ("x86.processor[0x%x].", dci->processor_index);
      _dl_diagnostics_print_labeled_value ("xgetbv.ecx[0x0]",
                                           current->xgetbv_ecx_0);
    }
}

static void
_dl_diagnostics_cpuid (void)
{
#if !HAS_CPUID
  /* CPUID is not supported, so there is nothing to dump.  */
  if (__get_cpuid_max (0, 0) == 0)
    return;
#endif

  struct dl_iterate_cpu dic;
  _dl_iterate_cpu_init (&dic);

  /* Two copies of the data are used.  Data is written to the index
     (dic.processor_index & 1).  The previous version against which the
     data dump is reported is at index !(processor_index & 1).  */
  struct cpuid_collected_data ccd[2];

  /* The initial data is presumed to be all zero.  Zero results are
     not recorded.  */
  ccd[1].used = 0;
  ccd[1].xgetbv_ecx_0 = 0;

  /* Run the CPUID probing on a specific CPU.  There are expected
     differences for encoding core IDs and topology information in
     CPUID output, but some firmware/kernel bugs also may result in
     asymmetric data across CPUs in some cases.  */
  while (_dl_iterate_cpu_next (&dic))
    {
      _dl_diagnostics_cpuid_collect (&ccd[dic.processor_index & 1]);
      _dl_diagnostics_cpuid_report
        (&dic, &ccd[dic.processor_index & 1],
         &ccd[!(dic.processor_index & 1)]);
    }
}
