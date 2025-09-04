/* Initialize x86 cache info.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
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

static const struct intel_02_cache_info
{
  unsigned char idx;
  unsigned char assoc;
  unsigned char linesize;
  unsigned char rel_name;
  unsigned int size;
} intel_02_known [] =
  {
#define M(sc) ((sc) - _SC_LEVEL1_ICACHE_SIZE)
    { 0x06,  4, 32, M(_SC_LEVEL1_ICACHE_SIZE),    8192 },
    { 0x08,  4, 32, M(_SC_LEVEL1_ICACHE_SIZE),   16384 },
    { 0x09,  4, 32, M(_SC_LEVEL1_ICACHE_SIZE),   32768 },
    { 0x0a,  2, 32, M(_SC_LEVEL1_DCACHE_SIZE),    8192 },
    { 0x0c,  4, 32, M(_SC_LEVEL1_DCACHE_SIZE),   16384 },
    { 0x0d,  4, 64, M(_SC_LEVEL1_DCACHE_SIZE),   16384 },
    { 0x0e,  6, 64, M(_SC_LEVEL1_DCACHE_SIZE),   24576 },
    { 0x21,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x22,  4, 64, M(_SC_LEVEL3_CACHE_SIZE),   524288 },
    { 0x23,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  1048576 },
    { 0x25,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  2097152 },
    { 0x29,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  4194304 },
    { 0x2c,  8, 64, M(_SC_LEVEL1_DCACHE_SIZE),   32768 },
    { 0x30,  8, 64, M(_SC_LEVEL1_ICACHE_SIZE),   32768 },
    { 0x39,  4, 64, M(_SC_LEVEL2_CACHE_SIZE),   131072 },
    { 0x3a,  6, 64, M(_SC_LEVEL2_CACHE_SIZE),   196608 },
    { 0x3b,  2, 64, M(_SC_LEVEL2_CACHE_SIZE),   131072 },
    { 0x3c,  4, 64, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x3d,  6, 64, M(_SC_LEVEL2_CACHE_SIZE),   393216 },
    { 0x3e,  4, 64, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x3f,  2, 64, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x41,  4, 32, M(_SC_LEVEL2_CACHE_SIZE),   131072 },
    { 0x42,  4, 32, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x43,  4, 32, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x44,  4, 32, M(_SC_LEVEL2_CACHE_SIZE),  1048576 },
    { 0x45,  4, 32, M(_SC_LEVEL2_CACHE_SIZE),  2097152 },
    { 0x46,  4, 64, M(_SC_LEVEL3_CACHE_SIZE),  4194304 },
    { 0x47,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  8388608 },
    { 0x48, 12, 64, M(_SC_LEVEL2_CACHE_SIZE),  3145728 },
    { 0x49, 16, 64, M(_SC_LEVEL2_CACHE_SIZE),  4194304 },
    { 0x4a, 12, 64, M(_SC_LEVEL3_CACHE_SIZE),  6291456 },
    { 0x4b, 16, 64, M(_SC_LEVEL3_CACHE_SIZE),  8388608 },
    { 0x4c, 12, 64, M(_SC_LEVEL3_CACHE_SIZE), 12582912 },
    { 0x4d, 16, 64, M(_SC_LEVEL3_CACHE_SIZE), 16777216 },
    { 0x4e, 24, 64, M(_SC_LEVEL2_CACHE_SIZE),  6291456 },
    { 0x60,  8, 64, M(_SC_LEVEL1_DCACHE_SIZE),   16384 },
    { 0x66,  4, 64, M(_SC_LEVEL1_DCACHE_SIZE),    8192 },
    { 0x67,  4, 64, M(_SC_LEVEL1_DCACHE_SIZE),   16384 },
    { 0x68,  4, 64, M(_SC_LEVEL1_DCACHE_SIZE),   32768 },
    { 0x78,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),  1048576 },
    { 0x79,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),   131072 },
    { 0x7a,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x7b,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x7c,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),  1048576 },
    { 0x7d,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),  2097152 },
    { 0x7f,  2, 64, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x80,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x82,  8, 32, M(_SC_LEVEL2_CACHE_SIZE),   262144 },
    { 0x83,  8, 32, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x84,  8, 32, M(_SC_LEVEL2_CACHE_SIZE),  1048576 },
    { 0x85,  8, 32, M(_SC_LEVEL2_CACHE_SIZE),  2097152 },
    { 0x86,  4, 64, M(_SC_LEVEL2_CACHE_SIZE),   524288 },
    { 0x87,  8, 64, M(_SC_LEVEL2_CACHE_SIZE),  1048576 },
    { 0xd0,  4, 64, M(_SC_LEVEL3_CACHE_SIZE),   524288 },
    { 0xd1,  4, 64, M(_SC_LEVEL3_CACHE_SIZE),  1048576 },
    { 0xd2,  4, 64, M(_SC_LEVEL3_CACHE_SIZE),  2097152 },
    { 0xd6,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  1048576 },
    { 0xd7,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  2097152 },
    { 0xd8,  8, 64, M(_SC_LEVEL3_CACHE_SIZE),  4194304 },
    { 0xdc, 12, 64, M(_SC_LEVEL3_CACHE_SIZE),  2097152 },
    { 0xdd, 12, 64, M(_SC_LEVEL3_CACHE_SIZE),  4194304 },
    { 0xde, 12, 64, M(_SC_LEVEL3_CACHE_SIZE),  8388608 },
    { 0xe2, 16, 64, M(_SC_LEVEL3_CACHE_SIZE),  2097152 },
    { 0xe3, 16, 64, M(_SC_LEVEL3_CACHE_SIZE),  4194304 },
    { 0xe4, 16, 64, M(_SC_LEVEL3_CACHE_SIZE),  8388608 },
    { 0xea, 24, 64, M(_SC_LEVEL3_CACHE_SIZE), 12582912 },
    { 0xeb, 24, 64, M(_SC_LEVEL3_CACHE_SIZE), 18874368 },
    { 0xec, 24, 64, M(_SC_LEVEL3_CACHE_SIZE), 25165824 },
  };

#define nintel_02_known (sizeof (intel_02_known) / sizeof (intel_02_known [0]))

static int
intel_02_known_compare (const void *p1, const void *p2)
{
  const struct intel_02_cache_info *i1;
  const struct intel_02_cache_info *i2;

  i1 = (const struct intel_02_cache_info *) p1;
  i2 = (const struct intel_02_cache_info *) p2;

  if (i1->idx == i2->idx)
    return 0;

  return i1->idx < i2->idx ? -1 : 1;
}


static long int
__attribute__ ((noinline))
intel_check_word (int name, unsigned int value, bool *has_level_2,
		  bool *no_level_2_or_3,
		  const struct cpu_features *cpu_features)
{
  if ((value & 0x80000000) != 0)
    /* The register value is reserved.  */
    return 0;

  /* Fold the name.  The _SC_ constants are always in the order SIZE,
     ASSOC, LINESIZE.  */
  int folded_rel_name = (M(name) / 3) * 3;

  while (value != 0)
    {
      unsigned int byte = value & 0xff;

      if (byte == 0x40)
	{
	  *no_level_2_or_3 = true;

	  if (folded_rel_name == M(_SC_LEVEL3_CACHE_SIZE))
	    /* No need to look further.  */
	    break;
	}
      else if (byte == 0xff)
	{
	  /* CPUID leaf 0x4 contains all the information.  We need to
	     iterate over it.  */
	  unsigned int eax;
	  unsigned int ebx;
	  unsigned int ecx;
	  unsigned int edx;

	  unsigned int round = 0;
	  while (1)
	    {
	      __cpuid_count (4, round, eax, ebx, ecx, edx);

	      enum { null = 0, data = 1, inst = 2, uni = 3 } type = eax & 0x1f;
	      if (type == null)
		/* That was the end.  */
		break;

	      unsigned int level = (eax >> 5) & 0x7;

	      if ((level == 1 && type == data
		   && folded_rel_name == M(_SC_LEVEL1_DCACHE_SIZE))
		  || (level == 1 && type == inst
		      && folded_rel_name == M(_SC_LEVEL1_ICACHE_SIZE))
		  || (level == 2 && folded_rel_name == M(_SC_LEVEL2_CACHE_SIZE))
		  || (level == 3 && folded_rel_name == M(_SC_LEVEL3_CACHE_SIZE))
		  || (level == 4 && folded_rel_name == M(_SC_LEVEL4_CACHE_SIZE)))
		{
		  unsigned int offset = M(name) - folded_rel_name;

		  if (offset == 0)
		    /* Cache size.  */
		    return (((ebx >> 22) + 1)
			    * (((ebx >> 12) & 0x3ff) + 1)
			    * ((ebx & 0xfff) + 1)
			    * (ecx + 1));
		  if (offset == 1)
		    return (ebx >> 22) + 1;

		  assert (offset == 2);
		  return (ebx & 0xfff) + 1;
		}

	      ++round;
	    }
	  /* There is no other cache information anywhere else.  */
	  return -1;
	}
      else
	{
	  if (byte == 0x49 && folded_rel_name == M(_SC_LEVEL3_CACHE_SIZE))
	    {
	      /* Intel reused this value.  For family 15, model 6 it
		 specifies the 3rd level cache.  Otherwise the 2nd
		 level cache.  */
	      unsigned int family = cpu_features->basic.family;
	      unsigned int model = cpu_features->basic.model;

	      if (family == 15 && model == 6)
		{
		  /* The level 3 cache is encoded for this model like
		     the level 2 cache is for other models.  Pretend
		     the caller asked for the level 2 cache.  */
		  name = (_SC_LEVEL2_CACHE_SIZE
			  + (name - _SC_LEVEL3_CACHE_SIZE));
		  folded_rel_name = M(_SC_LEVEL2_CACHE_SIZE);
		}
	    }

	  struct intel_02_cache_info *found;
	  struct intel_02_cache_info search;

	  search.idx = byte;
	  found = bsearch (&search, intel_02_known, nintel_02_known,
			   sizeof (intel_02_known[0]), intel_02_known_compare);
	  if (found != NULL)
	    {
	      if (found->rel_name == folded_rel_name)
		{
		  unsigned int offset = M(name) - folded_rel_name;

		  if (offset == 0)
		    /* Cache size.  */
		    return found->size;
		  if (offset == 1)
		    return found->assoc;

		  assert (offset == 2);
		  return found->linesize;
		}

	      if (found->rel_name == M(_SC_LEVEL2_CACHE_SIZE))
		*has_level_2 = true;
	    }
	}

      /* Next byte for the next round.  */
      value >>= 8;
    }

  /* Nothing found.  */
  return 0;
}


static long int __attribute__ ((noinline))
handle_intel (int name, const struct cpu_features *cpu_features)
{
  unsigned int maxidx = cpu_features->basic.max_cpuid;

  /* Return -1 for older CPUs.  */
  if (maxidx < 2)
    return -1;

  /* OK, we can use the CPUID instruction to get all info about the
     caches.  */
  long int result = 0;
  bool no_level_2_or_3 = false;
  bool has_level_2 = false;
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
  __cpuid (2, eax, ebx, ecx, edx);

  /* The low byte of EAX of CPUID leaf 2 should always return 1 and it
     should be ignored.  If it isn't 1, use CPUID leaf 4 instead.  */
  if ((eax & 0xff) != 1)
    return intel_check_word (name, 0xff, &has_level_2, &no_level_2_or_3,
			     cpu_features);
  else
    {
      eax &= 0xffffff00;

      /* Process the individual registers' value.  */
      result = intel_check_word (name, eax, &has_level_2,
				 &no_level_2_or_3, cpu_features);
      if (result != 0)
	return result;

      result = intel_check_word (name, ebx, &has_level_2,
				 &no_level_2_or_3, cpu_features);
      if (result != 0)
	return result;

      result = intel_check_word (name, ecx, &has_level_2,
				 &no_level_2_or_3, cpu_features);
      if (result != 0)
	return result;

      result = intel_check_word (name, edx, &has_level_2,
				 &no_level_2_or_3, cpu_features);
      if (result != 0)
	return result;
    }

  if (name >= _SC_LEVEL2_CACHE_SIZE && name <= _SC_LEVEL3_CACHE_LINESIZE
      && no_level_2_or_3)
    return -1;

  return 0;
}


static long int __attribute__ ((noinline))
handle_amd (int name)
{
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx = 0;
  unsigned int edx;
  unsigned int max_cpuid = 0;
  unsigned int fn = 0;

  /* No level 4 cache (yet).  */
  if (name > _SC_LEVEL3_CACHE_LINESIZE)
    return 0;

  __cpuid (0x80000000, max_cpuid, ebx, ecx, edx);

  if (max_cpuid >= 0x8000001D)
    /* Use __cpuid__ '0x8000_001D' to compute cache details.  */
    {
      unsigned int count = 0x1;

      if (name >= _SC_LEVEL3_CACHE_SIZE)
        count = 0x3;
      else if (name >= _SC_LEVEL2_CACHE_SIZE)
        count = 0x2;
      else if (name >= _SC_LEVEL1_DCACHE_SIZE)
        count = 0x0;

      __cpuid_count (0x8000001D, count, eax, ebx, ecx, edx);

      if (ecx != 0)
        {
          switch (name)
            {
            case _SC_LEVEL1_ICACHE_ASSOC:
            case _SC_LEVEL1_DCACHE_ASSOC:
            case _SC_LEVEL2_CACHE_ASSOC:
            case _SC_LEVEL3_CACHE_ASSOC:
              return ((ebx >> 22) & 0x3ff) + 1;
            case _SC_LEVEL1_ICACHE_LINESIZE:
            case _SC_LEVEL1_DCACHE_LINESIZE:
            case _SC_LEVEL2_CACHE_LINESIZE:
            case _SC_LEVEL3_CACHE_LINESIZE:
              return (ebx & 0xfff) + 1;
            case _SC_LEVEL1_ICACHE_SIZE:
            case _SC_LEVEL1_DCACHE_SIZE:
            case _SC_LEVEL2_CACHE_SIZE:
            case _SC_LEVEL3_CACHE_SIZE:
              return (((ebx >> 22) & 0x3ff) + 1) * ((ebx & 0xfff) + 1) * (ecx + 1);
            default:
              __builtin_unreachable ();
            }
          return -1;
        }
    }

  /* Legacy cache computation for CPUs prior to Bulldozer family.
     This is also a fail-safe mechanism for some hypervisors that
     accidentally configure __cpuid__ '0x8000_001D' to Zero.  */

  fn = 0x80000005 + (name >= _SC_LEVEL2_CACHE_SIZE);

  if (max_cpuid < fn)
    return 0;

  __cpuid (fn, eax, ebx, ecx, edx);

  if (name < _SC_LEVEL1_DCACHE_SIZE)
    {
      name += _SC_LEVEL1_DCACHE_SIZE - _SC_LEVEL1_ICACHE_SIZE;
      ecx = edx;
    }

  switch (name)
    {
      case _SC_LEVEL1_DCACHE_SIZE:
        return (ecx >> 14) & 0x3fc00;

      case _SC_LEVEL1_DCACHE_ASSOC:
        ecx >>= 16;
        if ((ecx & 0xff) == 0xff)
        {
          /* Fully associative.  */
          return (ecx << 2) & 0x3fc00;
        }
        return ecx & 0xff;

      case _SC_LEVEL1_DCACHE_LINESIZE:
        return ecx & 0xff;

      case _SC_LEVEL2_CACHE_SIZE:
        return (ecx & 0xf000) == 0 ? 0 : (ecx >> 6) & 0x3fffc00;

      case _SC_LEVEL2_CACHE_ASSOC:
        switch ((ecx >> 12) & 0xf)
          {
            case 0:
            case 1:
            case 2:
            case 4:
              return (ecx >> 12) & 0xf;
            case 6:
              return 8;
            case 8:
              return 16;
            case 10:
              return 32;
            case 11:
              return 48;
            case 12:
              return 64;
            case 13:
              return 96;
            case 14:
              return 128;
            case 15:
              return ((ecx >> 6) & 0x3fffc00) / (ecx & 0xff);
            default:
              return 0;
          }

      case _SC_LEVEL2_CACHE_LINESIZE:
        return (ecx & 0xf000) == 0 ? 0 : ecx & 0xff;

      case _SC_LEVEL3_CACHE_SIZE:
        {
        long int total_l3_cache = 0, l3_cache_per_thread = 0;
        unsigned int threads = 0;
        const struct cpu_features *cpu_features;

        if ((edx & 0xf000) == 0)
          return 0;

        total_l3_cache = (edx & 0x3ffc0000) << 1;
        cpu_features = __get_cpu_features ();

        /* Figure out the number of logical threads that share L3.  */
        if (max_cpuid >= 0x80000008)
          {
            /* Get width of APIC ID.  */
            __cpuid (0x80000008, eax, ebx, ecx, edx);
            threads = (ecx & 0xff) + 1;
          }

        if (threads == 0)
          {
            /* If APIC ID width is not available, use logical
            processor count.  */
            __cpuid (0x00000001, eax, ebx, ecx, edx);
            if ((edx & (1 << 28)) != 0)
              threads = (ebx >> 16) & 0xff;
          }

        /* Cap usage of highest cache level to the number of
           supported threads.  */
        if (threads > 0)
          l3_cache_per_thread = total_l3_cache/threads;

        /* Get shared cache per ccx for Zen architectures.  */
        if (cpu_features->basic.family >= 0x17)
          {
            long int l3_cache_per_ccx = 0;
            /* Get number of threads share the L3 cache in CCX.  */
            __cpuid_count (0x8000001D, 0x3, eax, ebx, ecx, edx);
            unsigned int threads_per_ccx = ((eax >> 14) & 0xfff) + 1;
            l3_cache_per_ccx = l3_cache_per_thread * threads_per_ccx;
            return l3_cache_per_ccx;
          }
        else
          {
            return l3_cache_per_thread;
          }
      }

    case _SC_LEVEL3_CACHE_ASSOC:
      switch ((edx >> 12) & 0xf)
      {
        case 0:
        case 1:
        case 2:
        case 4:
          return (edx >> 12) & 0xf;
        case 6:
          return 8;
        case 8:
          return 16;
        case 10:
          return 32;
        case 11:
          return 48;
        case 12:
          return 64;
        case 13:
          return 96;
        case 14:
          return 128;
        case 15:
          return ((edx & 0x3ffc0000) << 1) / (edx & 0xff);
        default:
          return 0;
      }

    case _SC_LEVEL3_CACHE_LINESIZE:
      return (edx & 0xf000) == 0 ? 0 : edx & 0xff;

    default:
      __builtin_unreachable ();
    }
  return -1;
}


static long int __attribute__ ((noinline))
handle_zhaoxin (int name)
{
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;

  int folded_rel_name = (M(name) / 3) * 3;

  unsigned int round = 0;
  while (1)
    {
      __cpuid_count (4, round, eax, ebx, ecx, edx);

      enum { null = 0, data = 1, inst = 2, uni = 3 } type = eax & 0x1f;
      if (type == null)
        break;

      unsigned int level = (eax >> 5) & 0x7;

      if ((level == 1 && type == data
        && folded_rel_name == M(_SC_LEVEL1_DCACHE_SIZE))
        || (level == 1 && type == inst
            && folded_rel_name == M(_SC_LEVEL1_ICACHE_SIZE))
        || (level == 2 && folded_rel_name == M(_SC_LEVEL2_CACHE_SIZE))
        || (level == 3 && folded_rel_name == M(_SC_LEVEL3_CACHE_SIZE)))
        {
          unsigned int offset = M(name) - folded_rel_name;

          if (offset == 0)
            /* Cache size.  */
            return (((ebx >> 22) + 1)
                * (((ebx >> 12) & 0x3ff) + 1)
                * ((ebx & 0xfff) + 1)
                * (ecx + 1));
          if (offset == 1)
            return (ebx >> 22) + 1;

          assert (offset == 2);
          return (ebx & 0xfff) + 1;
        }

      ++round;
    }

  /* Nothing found.  */
  return 0;
}

static void
get_common_cache_info (long int *shared_ptr, long int * shared_per_thread_ptr, unsigned int *threads_ptr,
                long int core)
{
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;

  /* Number of logical processors sharing L2 cache.  */
  int threads_l2;

  /* Number of logical processors sharing L3 cache.  */
  int threads_l3;

  const struct cpu_features *cpu_features = __get_cpu_features ();
  int max_cpuid = cpu_features->basic.max_cpuid;
  unsigned int family = cpu_features->basic.family;
  unsigned int model = cpu_features->basic.model;
  long int shared = *shared_ptr;
  long int shared_per_thread = *shared_per_thread_ptr;
  unsigned int threads = *threads_ptr;
  bool inclusive_cache = true;
  bool support_count_mask = true;

  /* Try L3 first.  */
  unsigned int level = 3;

  if (cpu_features->basic.kind == arch_kind_zhaoxin && family == 6)
    support_count_mask = false;

  if (shared <= 0)
    {
      /* Try L2 otherwise.  */
      level  = 2;
      shared = core;
      shared_per_thread = core;
      threads_l2 = 0;
      threads_l3 = -1;
    }
  else
    {
      threads_l2 = 0;
      threads_l3 = 0;
    }

  /* A value of 0 for the HTT bit indicates there is only a single
     logical processor.  */
  if (HAS_CPU_FEATURE (HTT))
    {
      /* Figure out the number of logical threads that share the
         highest cache level.  */
      if (max_cpuid >= 4)
        {
          int i = 0;

          /* Query until cache level 2 and 3 are enumerated.  */
          int check = 0x1 | (threads_l3 == 0) << 1;
          do
            {
              __cpuid_count (4, i++, eax, ebx, ecx, edx);

              /* There seems to be a bug in at least some Pentium Ds
                 which sometimes fail to iterate all cache parameters.
                 Do not loop indefinitely here, stop in this case and
                 assume there is no such information.  */
              if (cpu_features->basic.kind == arch_kind_intel
                  && (eax & 0x1f) == 0 )
                goto intel_bug_no_cache_info;

              switch ((eax >> 5) & 0x7)
                {
                  default:
                    break;
                  case 2:
                    if ((check & 0x1))
                      {
                        /* Get maximum number of logical processors
                           sharing L2 cache.  */
                        threads_l2 = (eax >> 14) & 0x3ff;
                        check &= ~0x1;
                      }
                    break;
                  case 3:
                    if ((check & (0x1 << 1)))
                      {
                        /* Get maximum number of logical processors
                           sharing L3 cache.  */
                        threads_l3 = (eax >> 14) & 0x3ff;

                        /* Check if L2 and L3 caches are inclusive.  */
                        inclusive_cache = (edx & 0x2) != 0;
                        check &= ~(0x1 << 1);
                      }
                    break;
                }
            }
          while (check);

          /* If max_cpuid >= 11, THREADS_L2/THREADS_L3 are the maximum
             numbers of addressable IDs for logical processors sharing
             the cache, instead of the maximum number of threads
             sharing the cache.  */
          if (max_cpuid >= 11 && support_count_mask)
            {
              /* Find the number of logical processors shipped in
                 one core and apply count mask.  */
              i = 0;

              /* Count SMT only if there is L3 cache.  Always count
                 core if there is no L3 cache.  */
              int count = ((threads_l2 > 0 && level == 3)
                           | ((threads_l3 > 0
                               || (threads_l2 > 0 && level == 2)) << 1));

              while (count)
                {
                  __cpuid_count (11, i++, eax, ebx, ecx, edx);

                  int shipped = ebx & 0xff;
                  int type = ecx & 0xff00;
                  if (shipped == 0 || type == 0)
                    break;
                  else if (type == 0x100)
                    {
                      /* Count SMT.  */
                      if ((count & 0x1))
                        {
                          int count_mask;

                          /* Compute count mask.  */
                          asm ("bsr %1, %0"
                               : "=r" (count_mask) : "g" (threads_l2));
                          count_mask = ~(-1 << (count_mask + 1));
                          threads_l2 = (shipped - 1) & count_mask;
                          count &= ~0x1;
                        }
                    }
                  else if (type == 0x200)
                    {
                      /* Count core.  */
                      if ((count & (0x1 << 1)))
                        {
                          int count_mask;
                          int threads_core
                            = (level == 2 ? threads_l2 : threads_l3);

                          /* Compute count mask.  */
                          asm ("bsr %1, %0"
                               : "=r" (count_mask) : "g" (threads_core));
                          count_mask = ~(-1 << (count_mask + 1));
                          threads_core = (shipped - 1) & count_mask;
                          if (level == 2)
                            threads_l2 = threads_core;
                          else
                            threads_l3 = threads_core;
                          count &= ~(0x1 << 1);
                        }
                    }
                }
            }
          if (threads_l2 > 0)
            threads_l2 += 1;
          if (threads_l3 > 0)
            threads_l3 += 1;
          if (level == 2)
            {
              if (threads_l2)
                {
                  threads = threads_l2;
                  if (cpu_features->basic.kind == arch_kind_intel
                      && threads > 2
                      && family == 6)
                    switch (model)
                      {
                        case 0x37:
                        case 0x4a:
                        case 0x4d:
                        case 0x5a:
                        case 0x5d:
                          /* Silvermont has L2 cache shared by 2 cores.  */
                          threads = 2;
                          break;
                        default:
                          break;
                      }
                }
            }
          else if (threads_l3)
            threads = threads_l3;
        }
      else
        {
	intel_bug_no_cache_info:
	  /* Assume that all logical threads share the highest cache
	     level.  */
	  threads = ((cpu_features->features[CPUID_INDEX_1].cpuid.ebx >> 16)
		     & 0xff);
	}
      /* Get per-thread size of highest level cache.  */
      if (shared_per_thread > 0 && threads > 0)
	shared_per_thread /= threads;
    }

  /* Account for non-inclusive L2 and L3 caches.  */
  if (!inclusive_cache)
    {
      long int core_per_thread = threads_l2 > 0 ? (core / threads_l2) : core;
      shared_per_thread += core_per_thread;
      shared += core;
    }

  *shared_ptr = shared;
  *shared_per_thread_ptr = shared_per_thread;
  *threads_ptr = threads;
}

static void
dl_init_cacheinfo (struct cpu_features *cpu_features)
{
  /* Find out what brand of processor.  */
  long int data = -1;
  long int shared = -1;
  long int shared_per_thread = -1;
  unsigned int threads = 0;
  unsigned long int level1_icache_size = -1;
  unsigned long int level1_icache_linesize = -1;
  unsigned long int level1_dcache_size = -1;
  unsigned long int level1_dcache_assoc = -1;
  unsigned long int level1_dcache_linesize = -1;
  unsigned long int level2_cache_size = -1;
  unsigned long int level2_cache_assoc = -1;
  unsigned long int level2_cache_linesize = -1;
  unsigned long int level3_cache_size = -1;
  unsigned long int level3_cache_assoc = -1;
  unsigned long int level3_cache_linesize = -1;
  unsigned long int level4_cache_size = -1;

  if (cpu_features->basic.kind == arch_kind_intel)
    {
      data = handle_intel (_SC_LEVEL1_DCACHE_SIZE, cpu_features);
      shared = handle_intel (_SC_LEVEL3_CACHE_SIZE, cpu_features);
      shared_per_thread = shared;

      level1_icache_size
	= handle_intel (_SC_LEVEL1_ICACHE_SIZE, cpu_features);
      level1_icache_linesize
	= handle_intel (_SC_LEVEL1_ICACHE_LINESIZE, cpu_features);
      level1_dcache_size = data;
      level1_dcache_assoc
	= handle_intel (_SC_LEVEL1_DCACHE_ASSOC, cpu_features);
      level1_dcache_linesize
	= handle_intel (_SC_LEVEL1_DCACHE_LINESIZE, cpu_features);
      level2_cache_size
	= handle_intel (_SC_LEVEL2_CACHE_SIZE, cpu_features);
      level2_cache_assoc
	= handle_intel (_SC_LEVEL2_CACHE_ASSOC, cpu_features);
      level2_cache_linesize
	= handle_intel (_SC_LEVEL2_CACHE_LINESIZE, cpu_features);
      level3_cache_size = shared;
      level3_cache_assoc
	= handle_intel (_SC_LEVEL3_CACHE_ASSOC, cpu_features);
      level3_cache_linesize
	= handle_intel (_SC_LEVEL3_CACHE_LINESIZE, cpu_features);
      level4_cache_size
	= handle_intel (_SC_LEVEL4_CACHE_SIZE, cpu_features);

      get_common_cache_info (&shared, &shared_per_thread, &threads,
			     level2_cache_size);
    }
  else if (cpu_features->basic.kind == arch_kind_zhaoxin)
    {
      data = handle_zhaoxin (_SC_LEVEL1_DCACHE_SIZE);
      shared = handle_zhaoxin (_SC_LEVEL3_CACHE_SIZE);
      shared_per_thread = shared;

      level1_icache_size = handle_zhaoxin (_SC_LEVEL1_ICACHE_SIZE);
      level1_icache_linesize = handle_zhaoxin (_SC_LEVEL1_ICACHE_LINESIZE);
      level1_dcache_size = data;
      level1_dcache_assoc = handle_zhaoxin (_SC_LEVEL1_DCACHE_ASSOC);
      level1_dcache_linesize = handle_zhaoxin (_SC_LEVEL1_DCACHE_LINESIZE);
      level2_cache_size = handle_zhaoxin (_SC_LEVEL2_CACHE_SIZE);
      level2_cache_assoc = handle_zhaoxin (_SC_LEVEL2_CACHE_ASSOC);
      level2_cache_linesize = handle_zhaoxin (_SC_LEVEL2_CACHE_LINESIZE);
      level3_cache_size = shared;
      level3_cache_assoc = handle_zhaoxin (_SC_LEVEL3_CACHE_ASSOC);
      level3_cache_linesize = handle_zhaoxin (_SC_LEVEL3_CACHE_LINESIZE);

      get_common_cache_info (&shared, &shared_per_thread, &threads,
			     level2_cache_size);
    }
  else if (cpu_features->basic.kind == arch_kind_amd)
    {
      data = handle_amd (_SC_LEVEL1_DCACHE_SIZE);
      shared = handle_amd (_SC_LEVEL3_CACHE_SIZE);

      level1_icache_size = handle_amd (_SC_LEVEL1_ICACHE_SIZE);
      level1_icache_linesize = handle_amd (_SC_LEVEL1_ICACHE_LINESIZE);
      level1_dcache_size = data;
      level1_dcache_assoc = handle_amd (_SC_LEVEL1_DCACHE_ASSOC);
      level1_dcache_linesize = handle_amd (_SC_LEVEL1_DCACHE_LINESIZE);
      level2_cache_size = handle_amd (_SC_LEVEL2_CACHE_SIZE);;
      level2_cache_assoc = handle_amd (_SC_LEVEL2_CACHE_ASSOC);
      level2_cache_linesize = handle_amd (_SC_LEVEL2_CACHE_LINESIZE);
      level3_cache_size = shared;
      level3_cache_assoc = handle_amd (_SC_LEVEL3_CACHE_ASSOC);
      level3_cache_linesize = handle_amd (_SC_LEVEL3_CACHE_LINESIZE);
      level4_cache_size = handle_amd (_SC_LEVEL4_CACHE_SIZE);

      if (shared <= 0)
        {
           /* No shared L3 cache.  All we have is the L2 cache.  */
           shared = level2_cache_size;
        }
      else if (cpu_features->basic.family < 0x17)
        {
           /* Account for exclusive L2 and L3 caches.  */
           shared += level2_cache_size;
        }

      shared_per_thread = shared;
    }

  cpu_features->level1_icache_size = level1_icache_size;
  cpu_features->level1_icache_linesize = level1_icache_linesize;
  cpu_features->level1_dcache_size = level1_dcache_size;
  cpu_features->level1_dcache_assoc = level1_dcache_assoc;
  cpu_features->level1_dcache_linesize = level1_dcache_linesize;
  cpu_features->level2_cache_size = level2_cache_size;
  cpu_features->level2_cache_assoc = level2_cache_assoc;
  cpu_features->level2_cache_linesize = level2_cache_linesize;
  cpu_features->level3_cache_size = level3_cache_size;
  cpu_features->level3_cache_assoc = level3_cache_assoc;
  cpu_features->level3_cache_linesize = level3_cache_linesize;
  cpu_features->level4_cache_size = level4_cache_size;

  unsigned long int cachesize_non_temporal_divisor
      = cpu_features->cachesize_non_temporal_divisor;
  if (cachesize_non_temporal_divisor <= 0)
    cachesize_non_temporal_divisor = 4;

  /* The default setting for the non_temporal threshold is [1/8, 1/2] of size
     of the chip's cache (depending on `cachesize_non_temporal_divisor` which
     is microarch specific. The default is 1/4). For most Intel processors
     with an initial release date between 2017 and 2023, a thread's
     typical share of the cache is from 18-64MB. Using a reasonable size
     fraction of L3 is meant to estimate the point where non-temporal stores
     begin out-competing REP MOVSB. As well the point where the fact that
     non-temporal stores are forced back to main memory would already occurred
     to the majority of the lines in the copy. Note, concerns about the entire
     L3 cache being evicted by the copy are mostly alleviated by the fact that
     modern HW detects streaming patterns and provides proper LRU hints so that
     the maximum thrashing capped at 1/associativity. */
  unsigned long int non_temporal_threshold
      = shared / cachesize_non_temporal_divisor;

  /* If the computed non_temporal_threshold <= 3/4 * per-thread L3, we most
     likely have incorrect/incomplete cache info in which case, default to
     3/4 * per-thread L3 to avoid regressions.  */
  unsigned long int non_temporal_threshold_lowbound
      = shared_per_thread * 3 / 4;
  if (non_temporal_threshold < non_temporal_threshold_lowbound)
    non_temporal_threshold = non_temporal_threshold_lowbound;

  /* If no ERMS, we use the per-thread L3 chunking. Normal cacheable stores run
     a higher risk of actually thrashing the cache as they don't have a HW LRU
     hint. As well, their performance in highly parallel situations is
     noticeably worse. Zhaoxin processors are an exception, the lowbound is not
     suitable for them based on actual test data.  */
  if (!CPU_FEATURE_USABLE_P (cpu_features, ERMS)
      && cpu_features->basic.kind != arch_kind_zhaoxin)
    non_temporal_threshold = non_temporal_threshold_lowbound;
  /* SIZE_MAX >> 4 because memmove-vec-unaligned-erms right-shifts the value of
     'x86_non_temporal_threshold' by `LOG_4X_MEMCPY_THRESH` (4) and it is best
     if that operation cannot overflow. Minimum of 0x4040 (16448) because the
     L(large_memset_4x) loops need 64-byte to cache align and enough space for
     at least 1 iteration of 4x PAGE_SIZE unrolled loop.  Both values are
     reflected in the manual.  */
  unsigned long int maximum_non_temporal_threshold = SIZE_MAX >> 4;
  unsigned long int minimum_non_temporal_threshold = 0x4040;

  /* If `non_temporal_threshold` less than `minimum_non_temporal_threshold`
     it most likely means we failed to detect the cache info. We don't want
     to default to `minimum_non_temporal_threshold` as such a small value,
     while correct, has bad performance. We default to 64MB as reasonable
     default bound. 64MB is likely conservative in that most/all systems would
     choose a lower value so it should never forcing non-temporal stores when
     they otherwise wouldn't be used.  */
  if (non_temporal_threshold < minimum_non_temporal_threshold)
    non_temporal_threshold = 64 * 1024 * 1024;
  else if (non_temporal_threshold > maximum_non_temporal_threshold)
    non_temporal_threshold = maximum_non_temporal_threshold;

  /* NB: The REP MOVSB threshold must be greater than VEC_SIZE * 8.  */
  unsigned long int minimum_rep_movsb_threshold;
  /* NB: The default REP MOVSB threshold is 4096 * (VEC_SIZE / 16) for
     VEC_SIZE == 64 or 32.  For VEC_SIZE == 16, the default REP MOVSB
     threshold is 2048 * (VEC_SIZE / 16).  */
  unsigned long int rep_movsb_threshold;
  if (CPU_FEATURE_USABLE_P (cpu_features, AVX512F)
      && !CPU_FEATURE_PREFERRED_P (cpu_features, Prefer_No_AVX512))
    {
      rep_movsb_threshold = 4096 * (64 / 16);
      minimum_rep_movsb_threshold = 64 * 8;
    }
  else if (CPU_FEATURE_PREFERRED_P (cpu_features,
				    AVX_Fast_Unaligned_Load))
    {
      rep_movsb_threshold = 4096 * (32 / 16);
      minimum_rep_movsb_threshold = 32 * 8;
    }
  else
    {
      rep_movsb_threshold = 2048 * (16 / 16);
      minimum_rep_movsb_threshold = 16 * 8;
    }
  /* NB: The default REP MOVSB threshold is 2112 on processors with fast
     short REP MOVSB (FSRM).  */
  if (CPU_FEATURE_USABLE_P (cpu_features, FSRM))
    rep_movsb_threshold = 2112;

  /* Non-temporal stores are more performant on Intel and AMD hardware above
     non_temporal_threshold. Enable this for both Intel and AMD hardware. */
  unsigned long int memset_non_temporal_threshold = SIZE_MAX;
  if (!CPU_FEATURES_ARCH_P (cpu_features, Avoid_Non_Temporal_Memset)
      && (cpu_features->basic.kind == arch_kind_intel
	  || cpu_features->basic.kind == arch_kind_amd))
    memset_non_temporal_threshold = non_temporal_threshold;

  /* For AMD CPUs that support ERMS (Zen3+), REP MOVSB is in a lot of
     cases slower than the vectorized path (and for some alignments,
     it is really slow, check BZ #30994).  */
  if (cpu_features->basic.kind == arch_kind_amd)
    rep_movsb_threshold = non_temporal_threshold;

  /* The default threshold to use Enhanced REP STOSB.  */
  unsigned long int rep_stosb_threshold = 2048;

  long int tunable_size;

  tunable_size = TUNABLE_GET (x86_data_cache_size, long int, NULL);
  /* NB: Ignore the default value 0.  */
  if (tunable_size != 0)
    data = tunable_size;

  tunable_size = TUNABLE_GET (x86_shared_cache_size, long int, NULL);
  /* NB: Ignore the default value 0.  */
  if (tunable_size != 0)
    shared = tunable_size;

  tunable_size = TUNABLE_GET (x86_non_temporal_threshold, long int, NULL);
  if (tunable_size > minimum_non_temporal_threshold
      && tunable_size <= maximum_non_temporal_threshold)
    non_temporal_threshold = tunable_size;

  tunable_size = TUNABLE_GET (x86_memset_non_temporal_threshold, long int, NULL);
  if (tunable_size > minimum_non_temporal_threshold
      && tunable_size <= maximum_non_temporal_threshold)
    memset_non_temporal_threshold = tunable_size;

  tunable_size = TUNABLE_GET (x86_rep_movsb_threshold, long int, NULL);
  if (tunable_size > minimum_rep_movsb_threshold)
    rep_movsb_threshold = tunable_size;

  /* NB: The default value of the x86_rep_stosb_threshold tunable is the
     same as the default value of __x86_rep_stosb_threshold and the
     minimum value is fixed.  */
  rep_stosb_threshold = TUNABLE_GET (x86_rep_stosb_threshold,
				     long int, NULL);
  if (cpu_features->basic.kind == arch_kind_amd
      && !TUNABLE_IS_INITIALIZED (x86_rep_stosb_threshold))
    /* For AMD Zen3+ architecture, the performance of the vectorized loop is
       slightly better than ERMS.  */
    rep_stosb_threshold = SIZE_MAX;

  TUNABLE_SET_WITH_BOUNDS (x86_data_cache_size, data, 0, SIZE_MAX);
  TUNABLE_SET_WITH_BOUNDS (x86_shared_cache_size, shared, 0, SIZE_MAX);
  TUNABLE_SET_WITH_BOUNDS (x86_non_temporal_threshold, non_temporal_threshold,
			   minimum_non_temporal_threshold,
			   maximum_non_temporal_threshold);
  TUNABLE_SET_WITH_BOUNDS (x86_memset_non_temporal_threshold,
			   memset_non_temporal_threshold,
			   minimum_non_temporal_threshold, SIZE_MAX);
  TUNABLE_SET_WITH_BOUNDS (x86_rep_movsb_threshold, rep_movsb_threshold,
			   minimum_rep_movsb_threshold, SIZE_MAX);
  TUNABLE_SET_WITH_BOUNDS (x86_rep_stosb_threshold, rep_stosb_threshold, 1,
			   SIZE_MAX);

  unsigned long int rep_movsb_stop_threshold;
  /* Setting the upper bound of ERMS to the computed value of
     non-temporal threshold for all architectures.  */
  rep_movsb_stop_threshold = non_temporal_threshold;

  cpu_features->data_cache_size = data;
  cpu_features->shared_cache_size = shared;
  cpu_features->non_temporal_threshold = non_temporal_threshold;
  cpu_features->memset_non_temporal_threshold = memset_non_temporal_threshold;
  cpu_features->rep_movsb_threshold = rep_movsb_threshold;
  cpu_features->rep_stosb_threshold = rep_stosb_threshold;
  cpu_features->rep_movsb_stop_threshold = rep_movsb_stop_threshold;
}
