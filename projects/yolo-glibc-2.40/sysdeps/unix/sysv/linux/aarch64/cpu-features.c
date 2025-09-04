/* Initialize CPU feature data.  AArch64 version.
   This file is part of the GNU C Library.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.

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

#include <array_length.h>
#include <cpu-features.h>
#include <sys/auxv.h>
#include <elf/dl-hwcaps.h>
#include <sys/prctl.h>
#include <sys/utsname.h>
#include <dl-tunables-parse.h>

#define DCZID_DZP_MASK (1 << 4)
#define DCZID_BS_MASK (0xf)

/* The maximal set of permitted tags that the MTE random tag generation
   instruction may use.  We exclude tag 0 because a) we want to reserve
   that for the libc heap structures and b) because it makes it easier
   to see when pointer have been correctly tagged.  */
#define MTE_ALLOWED_TAGS (0xfffe << PR_MTE_TAG_SHIFT)

struct cpu_list
{
  const char *name;
  size_t len;
  uint64_t midr;
};

static const struct cpu_list cpu_list[] =
{
#define CPU_LIST_ENTRY(__str, __num) { __str, sizeof (__str) - 1, __num }
  CPU_LIST_ENTRY ("thunderxt88",    0x430F0A10),
  CPU_LIST_ENTRY ("thunderx2t99",   0x431F0AF0),
  CPU_LIST_ENTRY ("thunderx2t99p1", 0x420F5160),
  CPU_LIST_ENTRY ("ares",           0x411FD0C0),
  CPU_LIST_ENTRY ("emag",           0x503F0001),
  CPU_LIST_ENTRY ("kunpeng920",     0x481FD010),
  CPU_LIST_ENTRY ("a64fx",          0x460F0010),
  CPU_LIST_ENTRY ("generic",        0x0),
};

static uint64_t
get_midr_from_mcpu (const struct tunable_str_t *mcpu)
{
  for (int i = 0; i < array_length (cpu_list); i++)
    if (tunable_strcmp (mcpu, cpu_list[i].name, cpu_list[i].len))
      return cpu_list[i].midr;

  return UINT64_MAX;
}

#if __LINUX_KERNEL_VERSION < 0x060200

/* Return true if we prefer using SVE in string ifuncs.  Old kernels disable
   SVE after every system call which results in unnecessary traps if memcpy
   uses SVE.  This is true for kernels between 4.15.0 and before 6.2.0, except
   for 5.14.0 which was patched.  For these versions return false to avoid using
   SVE ifuncs.
   Parse the kernel version into a 24-bit kernel.major.minor value without
   calling any library functions.  If uname() is not supported or if the version
   format is not recognized, assume the kernel is modern and return true.  */

static inline bool
prefer_sve_ifuncs (void)
{
  struct utsname buf;
  const char *p = &buf.release[0];
  int kernel = 0;
  int val;

  if (__uname (&buf) < 0)
    return true;

  for (int shift = 16; shift >= 0; shift -= 8)
    {
      for (val = 0; *p >= '0' && *p <= '9'; p++)
	val = val * 10 + *p - '0';
      kernel |= (val & 255) << shift;
      if (*p++ != '.')
	break;
    }

  if (kernel >= 0x060200 || kernel == 0x050e00)
    return true;
  if (kernel >= 0x040f00)
    return false;
  return true;
}

#endif

static inline void
init_cpu_features (struct cpu_features *cpu_features)
{
  register uint64_t midr = UINT64_MAX;

  /* Get the tunable override.  */
  const struct tunable_str_t *mcpu = TUNABLE_GET (glibc, cpu, name,
						  struct tunable_str_t *,
						  NULL);
  if (mcpu != NULL)
    midr = get_midr_from_mcpu (mcpu);

  /* If there was no useful tunable override, query the MIDR if the kernel
     allows it.  */
  if (midr == UINT64_MAX)
    {
      if (GLRO (dl_hwcap) & HWCAP_CPUID)
	asm volatile ("mrs %0, midr_el1" : "=r"(midr));
      else
	midr = 0;
    }

  cpu_features->midr_el1 = midr;

  /* Check if ZVA is enabled.  */
  unsigned dczid;
  asm volatile ("mrs %0, dczid_el0" : "=r"(dczid));

  if ((dczid & DCZID_DZP_MASK) == 0)
    cpu_features->zva_size = 4 << (dczid & DCZID_BS_MASK);

  /* Check if BTI is supported.  */
  cpu_features->bti = GLRO (dl_hwcap2) & HWCAP2_BTI;

  /* Setup memory tagging support if the HW and kernel support it, and if
     the user has requested it.  */
  cpu_features->mte_state = 0;

#ifdef USE_MTAG
  int mte_state = TUNABLE_GET (glibc, mem, tagging, unsigned, 0);
  cpu_features->mte_state = (GLRO (dl_hwcap2) & HWCAP2_MTE) ? mte_state : 0;
  /* If we lack the MTE feature, disable the tunable, since it will
     otherwise cause instructions that won't run on this CPU to be used.  */
  TUNABLE_SET (glibc, mem, tagging, cpu_features->mte_state);

  if (cpu_features->mte_state & 4)
    /* Enable choosing system-preferred faulting mode.  */
    __prctl (PR_SET_TAGGED_ADDR_CTRL,
	     (PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC | PR_MTE_TCF_ASYNC
	      | MTE_ALLOWED_TAGS),
	     0, 0, 0);
  else if (cpu_features->mte_state & 2)
    __prctl (PR_SET_TAGGED_ADDR_CTRL,
	     (PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_SYNC | MTE_ALLOWED_TAGS),
	     0, 0, 0);
  else if (cpu_features->mte_state)
    __prctl (PR_SET_TAGGED_ADDR_CTRL,
	     (PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC | MTE_ALLOWED_TAGS),
	     0, 0, 0);
#endif

  /* Check if SVE is supported.  */
  cpu_features->sve = GLRO (dl_hwcap) & HWCAP_SVE;

  cpu_features->prefer_sve_ifuncs = cpu_features->sve;

#if __LINUX_KERNEL_VERSION < 0x060200
  if (cpu_features->sve)
    cpu_features->prefer_sve_ifuncs = prefer_sve_ifuncs ();
#endif

  /* Check if MOPS is supported.  */
  cpu_features->mops = GLRO (dl_hwcap2) & HWCAP2_MOPS;
}
