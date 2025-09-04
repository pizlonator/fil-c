/* Initialize CPU feature data.
   This file is part of the GNU C Library.
   Copyright (C) 2008-2024 Free Software Foundation, Inc.

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

#include <dl-hwcap.h>
#include <libc-pointer-arith.h>
#include <isa-level.h>
#include <get-isa-level.h>
#include <cacheinfo.h>
#include <dl-cacheinfo.h>
#include <dl-minsigstacksize.h>
#include <dl-hwcap2.h>

extern void TUNABLE_CALLBACK (set_hwcaps) (tunable_val_t *)
  attribute_hidden;

#if defined SHARED
extern void _dl_tlsdesc_dynamic_fxsave (void) attribute_hidden;
extern void _dl_tlsdesc_dynamic_xsave (void) attribute_hidden;
extern void _dl_tlsdesc_dynamic_xsavec (void) attribute_hidden;

# ifdef __x86_64__
#  include <dl-plt-rewrite.h>

static void
TUNABLE_CALLBACK (set_plt_rewrite) (tunable_val_t *valp)
{
  /* We must be careful about where we put the call to
     dl_plt_rewrite_supported() since it may generate
     spurious SELinux log entries.  It should only be
     attempted if the user requested a PLT rewrite.  */
  if (valp->numval != 0 && dl_plt_rewrite_supported ())
    {
      /* Use JMPABS only on APX processors.  */
      const struct cpu_features *cpu_features = __get_cpu_features ();
      GL (dl_x86_feature_control).plt_rewrite
	  = ((valp->numval > 1 && CPU_FEATURE_PRESENT_P (cpu_features, APX_F))
		 ? plt_rewrite_jmpabs
		 : plt_rewrite_jmp);
    }
}
# else
extern void _dl_tlsdesc_dynamic_fnsave (void) attribute_hidden;
# endif
#endif

#ifdef __x86_64__
extern void _dl_runtime_resolve_fxsave (void) attribute_hidden;
extern void _dl_runtime_resolve_xsave (void) attribute_hidden;
extern void _dl_runtime_resolve_xsavec (void) attribute_hidden;
#endif

#ifdef __LP64__
static void
TUNABLE_CALLBACK (set_prefer_map_32bit_exec) (tunable_val_t *valp)
{
  if (valp->numval)
    GLRO(dl_x86_cpu_features).preferred[index_arch_Prefer_MAP_32BIT_EXEC]
      |= bit_arch_Prefer_MAP_32BIT_EXEC;
}
#endif

#if CET_ENABLED
extern void TUNABLE_CALLBACK (set_x86_ibt) (tunable_val_t *)
  attribute_hidden;
extern void TUNABLE_CALLBACK (set_x86_shstk) (tunable_val_t *)
  attribute_hidden;

# include <dl-cet.h>
#endif

static void
update_active (struct cpu_features *cpu_features)
{
  /* Copy the cpuid bits to active bits for CPU featuress whose usability
     in user space can be detected without additional OS support.  */
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE3);
  CPU_FEATURE_SET_ACTIVE (cpu_features, PCLMULQDQ);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSSE3);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CMPXCHG16B);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE4_1);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE4_2);
  CPU_FEATURE_SET_ACTIVE (cpu_features, MOVBE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, POPCNT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, AES);
  CPU_FEATURE_SET_ACTIVE (cpu_features, OSXSAVE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, TSC);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CX8);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CMOV);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CLFSH);
  CPU_FEATURE_SET_ACTIVE (cpu_features, MMX);
  CPU_FEATURE_SET_ACTIVE (cpu_features, FXSR);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE2);
  CPU_FEATURE_SET_ACTIVE (cpu_features, HTT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, BMI1);
  CPU_FEATURE_SET_ACTIVE (cpu_features, HLE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, BMI2);
  CPU_FEATURE_SET_ACTIVE (cpu_features, ERMS);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RDSEED);
  CPU_FEATURE_SET_ACTIVE (cpu_features, ADX);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CLFLUSHOPT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CLWB);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SHA);
  CPU_FEATURE_SET_ACTIVE (cpu_features, PREFETCHWT1);
  CPU_FEATURE_SET_ACTIVE (cpu_features, OSPKE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, WAITPKG);
  CPU_FEATURE_SET_ACTIVE (cpu_features, GFNI);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RDPID);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RDRAND);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CLDEMOTE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, MOVDIRI);
  CPU_FEATURE_SET_ACTIVE (cpu_features, MOVDIR64B);
  CPU_FEATURE_SET_ACTIVE (cpu_features, FSRM);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RTM_ALWAYS_ABORT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SERIALIZE);
  CPU_FEATURE_SET_ACTIVE (cpu_features, TSXLDTRK);
  CPU_FEATURE_SET_ACTIVE (cpu_features, LAHF64_SAHF64);
  CPU_FEATURE_SET_ACTIVE (cpu_features, LZCNT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SSE4A);
  CPU_FEATURE_SET_ACTIVE (cpu_features, PREFETCHW);
  CPU_FEATURE_SET_ACTIVE (cpu_features, TBM);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RDTSCP);
  CPU_FEATURE_SET_ACTIVE (cpu_features, WBNOINVD);
  CPU_FEATURE_SET_ACTIVE (cpu_features, RAO_INT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, CMPCCXADD);
  CPU_FEATURE_SET_ACTIVE (cpu_features, FZLRM);
  CPU_FEATURE_SET_ACTIVE (cpu_features, FSRS);
  CPU_FEATURE_SET_ACTIVE (cpu_features, FSRCS);
  CPU_FEATURE_SET_ACTIVE (cpu_features, PREFETCHI);
  CPU_FEATURE_SET_ACTIVE (cpu_features, PTWRITE);

  if (!CPU_FEATURES_CPU_P (cpu_features, RTM_ALWAYS_ABORT))
    CPU_FEATURE_SET_ACTIVE (cpu_features, RTM);

#if CET_ENABLED && 0
  CPU_FEATURE_SET_ACTIVE (cpu_features, IBT);
  CPU_FEATURE_SET_ACTIVE (cpu_features, SHSTK);
#endif

  enum
  {
    os_xmm = 1,
    os_ymm = 2,
    os_zmm = 4
  } os_vector_size = os_xmm;
  /* Can we call xgetbv?  */
  if (CPU_FEATURES_CPU_P (cpu_features, OSXSAVE))
    {
      unsigned int xcrlow;
      unsigned int xcrhigh;
      CPU_FEATURE_SET_ACTIVE (cpu_features, AVX10);
      asm ("xgetbv" : "=a" (xcrlow), "=d" (xcrhigh) : "c" (0));
      /* Is YMM and XMM state usable?  */
      if ((xcrlow & (bit_YMM_state | bit_XMM_state))
	  == (bit_YMM_state | bit_XMM_state))
	{
	  /* Determine if AVX is usable.  */
	  if (CPU_FEATURES_CPU_P (cpu_features, AVX))
	    {
	      os_vector_size |= os_ymm;
	      CPU_FEATURE_SET (cpu_features, AVX);
	      /* The following features depend on AVX being usable.  */
	      /* Determine if AVX2 is usable.  */
	      if (CPU_FEATURES_CPU_P (cpu_features, AVX2))
		{
		  CPU_FEATURE_SET (cpu_features, AVX2);

		  /* Unaligned load with 256-bit AVX registers are faster
		     on Intel/AMD processors with AVX2.  */
		  cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
		    |= bit_arch_AVX_Fast_Unaligned_Load;
		}
	      /* Determine if AVX-IFMA is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, AVX_IFMA);
	      /* Determine if AVX-NE-CONVERT is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, AVX_NE_CONVERT);
	      /* Determine if AVX-VNNI is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, AVX_VNNI);
	      /* Determine if AVX-VNNI-INT8 is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, AVX_VNNI_INT8);
	      /* Determine if FMA is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, FMA);
	      /* Determine if VAES is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, VAES);
	      /* Determine if VPCLMULQDQ is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, VPCLMULQDQ);
	      /* Determine if XOP is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, XOP);
	      /* Determine if F16C is usable.  */
	      CPU_FEATURE_SET_ACTIVE (cpu_features, F16C);
	    }

	  /* Check if OPMASK state, upper 256-bit of ZMM0-ZMM15 and
	     ZMM16-ZMM31 state are enabled.  */
	  if ((xcrlow & (bit_Opmask_state | bit_ZMM0_15_state
			 | bit_ZMM16_31_state))
	      == (bit_Opmask_state | bit_ZMM0_15_state | bit_ZMM16_31_state))
	    {
	      os_vector_size |= os_zmm;
	      /* Determine if AVX512F is usable.  */
	      if (CPU_FEATURES_CPU_P (cpu_features, AVX512F))
		{
		  CPU_FEATURE_SET (cpu_features, AVX512F);
		  /* Determine if AVX512CD is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512CD);
		  /* Determine if AVX512ER is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512ER);
		  /* Determine if AVX512PF is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512PF);
		  /* Determine if AVX512VL is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512VL);
		  /* Determine if AVX512DQ is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512DQ);
		  /* Determine if AVX512BW is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512BW);
		  /* Determine if AVX512_4FMAPS is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_4FMAPS);
		  /* Determine if AVX512_4VNNIW is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_4VNNIW);
		  /* Determine if AVX512_BITALG is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_BITALG);
		  /* Determine if AVX512_IFMA is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_IFMA);
		  /* Determine if AVX512_VBMI is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_VBMI);
		  /* Determine if AVX512_VBMI2 is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_VBMI2);
		  /* Determine if is AVX512_VNNI usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_VNNI);
		  /* Determine if AVX512_VPOPCNTDQ is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features,
					  AVX512_VPOPCNTDQ);
		  /* Determine if AVX512_VP2INTERSECT is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features,
					  AVX512_VP2INTERSECT);
		  /* Determine if AVX512_BF16 is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_BF16);
		  /* Determine if AVX512_FP16 is usable.  */
		  CPU_FEATURE_SET_ACTIVE (cpu_features, AVX512_FP16);
		}
	    }
	}

      if (CPU_FEATURES_CPU_P (cpu_features, AVX10)
	  && cpu_features->basic.max_cpuid >= 0x24)
	{
	  __cpuid_count (
	      0x24, 0, cpu_features->features[CPUID_INDEX_24_ECX_0].cpuid.eax,
	      cpu_features->features[CPUID_INDEX_24_ECX_0].cpuid.ebx,
	      cpu_features->features[CPUID_INDEX_24_ECX_0].cpuid.ecx,
	      cpu_features->features[CPUID_INDEX_24_ECX_0].cpuid.edx);
	  if (os_vector_size & os_xmm)
	    CPU_FEATURE_SET_ACTIVE (cpu_features, AVX10_XMM);
	  if (os_vector_size & os_ymm)
	    CPU_FEATURE_SET_ACTIVE (cpu_features, AVX10_YMM);
	  if (os_vector_size & os_zmm)
	    CPU_FEATURE_SET_ACTIVE (cpu_features, AVX10_ZMM);
	}

      /* Are XTILECFG and XTILEDATA states usable?  */
      if ((xcrlow & (bit_XTILECFG_state | bit_XTILEDATA_state))
	  == (bit_XTILECFG_state | bit_XTILEDATA_state))
	{
	  /* Determine if AMX_BF16 is usable.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, AMX_BF16);
	  /* Determine if AMX_TILE is usable.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, AMX_TILE);
	  /* Determine if AMX_INT8 is usable.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, AMX_INT8);
	  /* Determine if AMX_FP16 is usable.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, AMX_FP16);
	  /* Determine if AMX_COMPLEX is usable.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, AMX_COMPLEX);
	}

      /* APX is usable only if the APX state is supported by kernel.  */
      if ((xcrlow & bit_APX_state) != 0)
	CPU_FEATURE_SET_ACTIVE (cpu_features, APX_F);

      /* These features are usable only when OSXSAVE is enabled.  */
      CPU_FEATURE_SET (cpu_features, XSAVE);
      CPU_FEATURE_SET_ACTIVE (cpu_features, XSAVEOPT);
      CPU_FEATURE_SET_ACTIVE (cpu_features, XSAVEC);
      CPU_FEATURE_SET_ACTIVE (cpu_features, XGETBV_ECX_1);
      CPU_FEATURE_SET_ACTIVE (cpu_features, XFD);

      /* For _dl_runtime_resolve, set xsave_state_size to xsave area
	 size + integer register save size and align it to 64 bytes.  */
      if (cpu_features->basic.max_cpuid >= 0xd)
	{
	  unsigned int eax, ebx, ecx, edx;

	  __cpuid_count (0xd, 0, eax, ebx, ecx, edx);
	  if (ebx != 0)
	    {
	      /* NB: On AMX capable processors, ebx always includes AMX
		 states.  */
	      unsigned int xsave_state_full_size
		= ALIGN_UP (ebx + TLSDESC_CALL_REGISTER_SAVE_AREA, 64);

	      cpu_features->xsave_state_size
		= xsave_state_full_size;
	      cpu_features->xsave_state_full_size
		= xsave_state_full_size;

	      /* Check if XSAVEC is available.  */
	      if (CPU_FEATURES_CPU_P (cpu_features, XSAVEC))
		{
		  unsigned int xstate_comp_offsets[32];
		  unsigned int xstate_comp_sizes[32];
#ifdef __x86_64__
		  unsigned int xstate_amx_comp_offsets[32];
		  unsigned int xstate_amx_comp_sizes[32];
		  unsigned int amx_ecx;
#endif
		  unsigned int i;

		  xstate_comp_offsets[0] = 0;
		  xstate_comp_offsets[1] = 160;
		  xstate_comp_offsets[2] = 576;
		  xstate_comp_sizes[0] = 160;
		  xstate_comp_sizes[1] = 256;
#ifdef __x86_64__
		  xstate_amx_comp_offsets[0] = 0;
		  xstate_amx_comp_offsets[1] = 160;
		  xstate_amx_comp_offsets[2] = 576;
		  xstate_amx_comp_sizes[0] = 160;
		  xstate_amx_comp_sizes[1] = 256;
#endif

		  for (i = 2; i < 32; i++)
		    {
		      if ((FULL_STATE_SAVE_MASK & (1 << i)) != 0)
			{
			  __cpuid_count (0xd, i, eax, ebx, ecx, edx);
#ifdef __x86_64__
			  /* Include this in xsave_state_full_size.  */
			  amx_ecx = ecx;
			  xstate_amx_comp_sizes[i] = eax;
			  if ((AMX_STATE_SAVE_MASK & (1 << i)) != 0)
			    {
			      /* Exclude this from xsave_state_size.  */
			      ecx = 0;
			      xstate_comp_sizes[i] = 0;
			    }
			  else
#endif
			    xstate_comp_sizes[i] = eax;
			}
		      else
			{
#ifdef __x86_64__
			  amx_ecx = 0;
			  xstate_amx_comp_sizes[i] = 0;
#endif
			  ecx = 0;
			  xstate_comp_sizes[i] = 0;
			}

		      if (i > 2)
			{
			  xstate_comp_offsets[i]
			    = (xstate_comp_offsets[i - 1]
			       + xstate_comp_sizes[i -1]);
			  if ((ecx & (1 << 1)) != 0)
			    xstate_comp_offsets[i]
			      = ALIGN_UP (xstate_comp_offsets[i], 64);
#ifdef __x86_64__
			  xstate_amx_comp_offsets[i]
			    = (xstate_amx_comp_offsets[i - 1]
			       + xstate_amx_comp_sizes[i - 1]);
			  if ((amx_ecx & (1 << 1)) != 0)
			    xstate_amx_comp_offsets[i]
			      = ALIGN_UP (xstate_amx_comp_offsets[i],
					  64);
#endif
			}
		    }

		  /* Use XSAVEC.  */
		  unsigned int size
		    = xstate_comp_offsets[31] + xstate_comp_sizes[31];
		  if (size)
		    {
#ifdef __x86_64__
		      unsigned int amx_size
			= (xstate_amx_comp_offsets[31]
			   + xstate_amx_comp_sizes[31]);
		      amx_size
			= ALIGN_UP ((amx_size
				     + TLSDESC_CALL_REGISTER_SAVE_AREA),
				    64);
		      /* Set xsave_state_full_size to the compact AMX
			 state size for XSAVEC.  NB: xsave_state_full_size
			 is only used in _dl_tlsdesc_dynamic_xsave and
			 _dl_tlsdesc_dynamic_xsavec.  */
		      cpu_features->xsave_state_full_size = amx_size;
#endif
		      cpu_features->xsave_state_size
			= ALIGN_UP (size + TLSDESC_CALL_REGISTER_SAVE_AREA,
				    64);
		      CPU_FEATURE_SET (cpu_features, XSAVEC);
		    }
		}
	    }
	}
    }

  /* Determine if PKU is usable.  */
  if (CPU_FEATURES_CPU_P (cpu_features, OSPKE))
    CPU_FEATURE_SET (cpu_features, PKU);

  /* Determine if Key Locker instructions are usable.  */
  if (CPU_FEATURES_CPU_P (cpu_features, AESKLE))
    {
      CPU_FEATURE_SET (cpu_features, AESKLE);
      CPU_FEATURE_SET_ACTIVE (cpu_features, KL);
      CPU_FEATURE_SET_ACTIVE (cpu_features, WIDE_KL);
    }

  dl_check_hwcap2 (cpu_features);

  cpu_features->isa_1 = get_isa_level (cpu_features);
}

static void
get_extended_indices (struct cpu_features *cpu_features)
{
  unsigned int eax, ebx, ecx, edx;
  __cpuid (0x80000000, eax, ebx, ecx, edx);
  if (eax >= 0x80000001)
    __cpuid (0x80000001,
	     cpu_features->features[CPUID_INDEX_80000001].cpuid.eax,
	     cpu_features->features[CPUID_INDEX_80000001].cpuid.ebx,
	     cpu_features->features[CPUID_INDEX_80000001].cpuid.ecx,
	     cpu_features->features[CPUID_INDEX_80000001].cpuid.edx);
  if (eax >= 0x80000007)
    __cpuid (0x80000007,
	     cpu_features->features[CPUID_INDEX_80000007].cpuid.eax,
	     cpu_features->features[CPUID_INDEX_80000007].cpuid.ebx,
	     cpu_features->features[CPUID_INDEX_80000007].cpuid.ecx,
	     cpu_features->features[CPUID_INDEX_80000007].cpuid.edx);
  if (eax >= 0x80000008)
    __cpuid (0x80000008,
	     cpu_features->features[CPUID_INDEX_80000008].cpuid.eax,
	     cpu_features->features[CPUID_INDEX_80000008].cpuid.ebx,
	     cpu_features->features[CPUID_INDEX_80000008].cpuid.ecx,
	     cpu_features->features[CPUID_INDEX_80000008].cpuid.edx);
}

static void
get_common_indices (struct cpu_features *cpu_features,
		    unsigned int *family, unsigned int *model,
		    unsigned int *extended_model, unsigned int *stepping)
{
  if (family)
    {
      unsigned int eax;
      __cpuid (1, eax,
	       cpu_features->features[CPUID_INDEX_1].cpuid.ebx,
	       cpu_features->features[CPUID_INDEX_1].cpuid.ecx,
	       cpu_features->features[CPUID_INDEX_1].cpuid.edx);
      cpu_features->features[CPUID_INDEX_1].cpuid.eax = eax;
      *family = (eax >> 8) & 0x0f;
      *model = (eax >> 4) & 0x0f;
      *extended_model = (eax >> 12) & 0xf0;
      *stepping = eax & 0x0f;
      if (*family == 0x0f)
	{
	  *family += (eax >> 20) & 0xff;
	  *model += *extended_model;
	}
    }

  if (cpu_features->basic.max_cpuid >= 7)
    {
      __cpuid_count (7, 0,
		     cpu_features->features[CPUID_INDEX_7].cpuid.eax,
		     cpu_features->features[CPUID_INDEX_7].cpuid.ebx,
		     cpu_features->features[CPUID_INDEX_7].cpuid.ecx,
		     cpu_features->features[CPUID_INDEX_7].cpuid.edx);
      __cpuid_count (7, 1,
		     cpu_features->features[CPUID_INDEX_7_ECX_1].cpuid.eax,
		     cpu_features->features[CPUID_INDEX_7_ECX_1].cpuid.ebx,
		     cpu_features->features[CPUID_INDEX_7_ECX_1].cpuid.ecx,
		     cpu_features->features[CPUID_INDEX_7_ECX_1].cpuid.edx);
    }

  if (cpu_features->basic.max_cpuid >= 0xd)
    __cpuid_count (0xd, 1,
		   cpu_features->features[CPUID_INDEX_D_ECX_1].cpuid.eax,
		   cpu_features->features[CPUID_INDEX_D_ECX_1].cpuid.ebx,
		   cpu_features->features[CPUID_INDEX_D_ECX_1].cpuid.ecx,
		   cpu_features->features[CPUID_INDEX_D_ECX_1].cpuid.edx);

  if (cpu_features->basic.max_cpuid >= 0x14)
    __cpuid_count (0x14, 0,
		   cpu_features->features[CPUID_INDEX_14_ECX_0].cpuid.eax,
		   cpu_features->features[CPUID_INDEX_14_ECX_0].cpuid.ebx,
		   cpu_features->features[CPUID_INDEX_14_ECX_0].cpuid.ecx,
		   cpu_features->features[CPUID_INDEX_14_ECX_0].cpuid.edx);

  if (cpu_features->basic.max_cpuid >= 0x19)
    __cpuid_count (0x19, 0,
		   cpu_features->features[CPUID_INDEX_19].cpuid.eax,
		   cpu_features->features[CPUID_INDEX_19].cpuid.ebx,
		   cpu_features->features[CPUID_INDEX_19].cpuid.ecx,
		   cpu_features->features[CPUID_INDEX_19].cpuid.edx);

  dl_check_minsigstacksize (cpu_features);
}

_Static_assert (((index_arch_Fast_Unaligned_Load
		  == index_arch_Fast_Unaligned_Copy)
		 && (index_arch_Fast_Unaligned_Load
		     == index_arch_Prefer_PMINUB_for_stringop)
		 && (index_arch_Fast_Unaligned_Load
		     == index_arch_Slow_SSE4_2)
		 && (index_arch_Fast_Unaligned_Load
		     == index_arch_Fast_Rep_String)
		 && (index_arch_Fast_Unaligned_Load
		     == index_arch_Fast_Copy_Backward)),
		"Incorrect index_arch_Fast_Unaligned_Load");


/* Intel Family-6 microarch list.  */
enum
{
  /* Atom processors.  */
  INTEL_ATOM_BONNELL,
  INTEL_ATOM_SILVERMONT,
  INTEL_ATOM_AIRMONT,
  INTEL_ATOM_GOLDMONT,
  INTEL_ATOM_GOLDMONT_PLUS,
  INTEL_ATOM_SIERRAFOREST,
  INTEL_ATOM_GRANDRIDGE,
  INTEL_ATOM_TREMONT,

  /* Bigcore processors.  */
  INTEL_BIGCORE_MEROM,
  INTEL_BIGCORE_PENRYN,
  INTEL_BIGCORE_DUNNINGTON,
  INTEL_BIGCORE_NEHALEM,
  INTEL_BIGCORE_WESTMERE,
  INTEL_BIGCORE_SANDYBRIDGE,
  INTEL_BIGCORE_IVYBRIDGE,
  INTEL_BIGCORE_HASWELL,
  INTEL_BIGCORE_BROADWELL,
  INTEL_BIGCORE_SKYLAKE,
  INTEL_BIGCORE_KABYLAKE,
  INTEL_BIGCORE_COMETLAKE,
  INTEL_BIGCORE_SKYLAKE_AVX512,
  INTEL_BIGCORE_CANNONLAKE,
  INTEL_BIGCORE_ICELAKE,
  INTEL_BIGCORE_TIGERLAKE,
  INTEL_BIGCORE_ROCKETLAKE,
  INTEL_BIGCORE_SAPPHIRERAPIDS,
  INTEL_BIGCORE_RAPTORLAKE,
  INTEL_BIGCORE_EMERALDRAPIDS,
  INTEL_BIGCORE_METEORLAKE,
  INTEL_BIGCORE_LUNARLAKE,
  INTEL_BIGCORE_ARROWLAKE,
  INTEL_BIGCORE_GRANITERAPIDS,

  /* Mixed (bigcore + atom SOC).  */
  INTEL_MIXED_LAKEFIELD,
  INTEL_MIXED_ALDERLAKE,

  /* KNL.  */
  INTEL_KNIGHTS_MILL,
  INTEL_KNIGHTS_LANDING,

  /* Unknown.  */
  INTEL_UNKNOWN,
};

static unsigned int
intel_get_fam6_microarch (unsigned int model,
			  __attribute__ ((unused)) unsigned int stepping)
{
  switch (model)
    {
    case 0x1C:
    case 0x26:
      return INTEL_ATOM_BONNELL;
    case 0x27:
    case 0x35:
    case 0x36:
      /* Really Saltwell, but Saltwell is just a die shrink of Bonnell
         (microarchitecturally identical).  */
      return INTEL_ATOM_BONNELL;
    case 0x37:
    case 0x4A:
    case 0x4D:
    case 0x5D:
      return INTEL_ATOM_SILVERMONT;
    case 0x4C:
    case 0x5A:
    case 0x75:
      return INTEL_ATOM_AIRMONT;
    case 0x5C:
    case 0x5F:
      return INTEL_ATOM_GOLDMONT;
    case 0x7A:
      return INTEL_ATOM_GOLDMONT_PLUS;
    case 0xAF:
      return INTEL_ATOM_SIERRAFOREST;
    case 0xB6:
      return INTEL_ATOM_GRANDRIDGE;
    case 0x86:
    case 0x96:
    case 0x9C:
      return INTEL_ATOM_TREMONT;
    case 0x0F:
    case 0x16:
      return INTEL_BIGCORE_MEROM;
    case 0x17:
      return INTEL_BIGCORE_PENRYN;
    case 0x1D:
      return INTEL_BIGCORE_DUNNINGTON;
    case 0x1A:
    case 0x1E:
    case 0x1F:
    case 0x2E:
      return INTEL_BIGCORE_NEHALEM;
    case 0x25:
    case 0x2C:
    case 0x2F:
      return INTEL_BIGCORE_WESTMERE;
    case 0x2A:
    case 0x2D:
      return INTEL_BIGCORE_SANDYBRIDGE;
    case 0x3A:
    case 0x3E:
      return INTEL_BIGCORE_IVYBRIDGE;
    case 0x3C:
    case 0x3F:
    case 0x45:
    case 0x46:
      return INTEL_BIGCORE_HASWELL;
    case 0x3D:
    case 0x47:
    case 0x4F:
    case 0x56:
      return INTEL_BIGCORE_BROADWELL;
    case 0x4E:
    case 0x5E:
      return INTEL_BIGCORE_SKYLAKE;
    case 0x8E:
    /*
     Stepping = {9}
        -> Amberlake
     Stepping = {10}
        -> Coffeelake
     Stepping = {11, 12}
        -> Whiskeylake
     else
        -> Kabylake

     All of these are derivatives of Kabylake (Skylake client).
     */
	  return INTEL_BIGCORE_KABYLAKE;
    case 0x9E:
    /*
     Stepping = {10, 11, 12, 13}
        -> Coffeelake
     else
        -> Kabylake

     Coffeelake is a derivatives of Kabylake (Skylake client).
     */
	  return INTEL_BIGCORE_KABYLAKE;
    case 0xA5:
    case 0xA6:
      return INTEL_BIGCORE_COMETLAKE;
    case 0x66:
      return INTEL_BIGCORE_CANNONLAKE;
    case 0x55:
    /*
     Stepping = {6, 7}
        -> Cascadelake
     Stepping = {11}
        -> Cooperlake
     else
        -> Skylake-avx512

     These are all microarchitecturally identical, so use
     Skylake-avx512 for all of them.
     */
      return INTEL_BIGCORE_SKYLAKE_AVX512;
    case 0x6A:
    case 0x6C:
    case 0x7D:
    case 0x7E:
    case 0x9D:
      return INTEL_BIGCORE_ICELAKE;
    case 0x8C:
    case 0x8D:
      return INTEL_BIGCORE_TIGERLAKE;
    case 0xA7:
      return INTEL_BIGCORE_ROCKETLAKE;
    case 0x8F:
      return INTEL_BIGCORE_SAPPHIRERAPIDS;
    case 0xB7:
    case 0xBA:
    case 0xBF:
      return INTEL_BIGCORE_RAPTORLAKE;
    case 0xCF:
      return INTEL_BIGCORE_EMERALDRAPIDS;
    case 0xAA:
    case 0xAC:
      return INTEL_BIGCORE_METEORLAKE;
    case 0xbd:
      return INTEL_BIGCORE_LUNARLAKE;
    case 0xc6:
      return INTEL_BIGCORE_ARROWLAKE;
    case 0xAD:
    case 0xAE:
      return INTEL_BIGCORE_GRANITERAPIDS;
    case 0x8A:
      return INTEL_MIXED_LAKEFIELD;
    case 0x97:
    case 0x9A:
    case 0xBE:
      return INTEL_MIXED_ALDERLAKE;
    case 0x85:
      return INTEL_KNIGHTS_MILL;
    case 0x57:
      return INTEL_KNIGHTS_LANDING;
    default:
      return INTEL_UNKNOWN;
    }
}

static inline void
init_cpu_features (struct cpu_features *cpu_features)
{
  unsigned int ebx, ecx, edx;
  unsigned int family = 0;
  unsigned int model = 0;
  unsigned int stepping = 0;
  enum cpu_features_kind kind;

  cpu_features->cachesize_non_temporal_divisor = 4;
#if !HAS_CPUID
  if (__get_cpuid_max (0, 0) == 0)
    {
      kind = arch_kind_other;
      goto no_cpuid;
    }
#endif

  __cpuid (0, cpu_features->basic.max_cpuid, ebx, ecx, edx);

  /* This spells out "GenuineIntel".  */
  if (ebx == 0x756e6547 && ecx == 0x6c65746e && edx == 0x49656e69)
    {
      unsigned int extended_model;

      kind = arch_kind_intel;

      get_common_indices (cpu_features, &family, &model, &extended_model,
			  &stepping);

      get_extended_indices (cpu_features);

      update_active (cpu_features);

      if (family == 0x06)
	{
	  model += extended_model;
	  unsigned int microarch
	      = intel_get_fam6_microarch (model, stepping);

	  switch (microarch)
	    {
	      /* Atom / KNL tuning.  */
	    case INTEL_ATOM_BONNELL:
	      /* BSF is slow on Bonnell.  */
	      cpu_features->preferred[index_arch_Slow_BSF]
		  |= bit_arch_Slow_BSF;
	      break;

	      /* Unaligned load versions are faster than SSSE3
		     on Airmont, Silvermont, Goldmont, and Goldmont Plus.  */
	    case INTEL_ATOM_AIRMONT:
	    case INTEL_ATOM_SILVERMONT:
	    case INTEL_ATOM_GOLDMONT:
	    case INTEL_ATOM_GOLDMONT_PLUS:

          /* Knights Landing.  Enable Silvermont optimizations.  */
	    case INTEL_KNIGHTS_LANDING:

	      cpu_features->preferred[index_arch_Fast_Unaligned_Load]
		  |= (bit_arch_Fast_Unaligned_Load
		      | bit_arch_Fast_Unaligned_Copy
		      | bit_arch_Prefer_PMINUB_for_stringop
		      | bit_arch_Slow_SSE4_2);
	      break;

	    case INTEL_ATOM_TREMONT:
	      /* Enable rep string instructions, unaligned load, unaligned
		 copy, pminub and avoid SSE 4.2 on Tremont.  */
	      cpu_features->preferred[index_arch_Fast_Rep_String]
		  |= (bit_arch_Fast_Rep_String
		      | bit_arch_Fast_Unaligned_Load
		      | bit_arch_Fast_Unaligned_Copy
		      | bit_arch_Prefer_PMINUB_for_stringop
		      | bit_arch_Slow_SSE4_2);
	      break;

	   /*
	    Default tuned Knights microarch.
	    case INTEL_KNIGHTS_MILL:
        */

	   /*
	    Default tuned atom microarch.
	    case INTEL_ATOM_SIERRAFOREST:
	    case INTEL_ATOM_GRANDRIDGE:
	   */

	      /* Bigcore/Default Tuning.  */
	    default:
	    default_tuning:
	      /* Unknown family 0x06 processors.  Assuming this is one
		 of Core i3/i5/i7 processors if AVX is available.  */
	      if (!CPU_FEATURES_CPU_P (cpu_features, AVX))
		break;

	    enable_modern_features:
	      /* Rep string instructions, unaligned load, unaligned copy,
		 and pminub are fast on Intel Core i3, i5 and i7.  */
	      cpu_features->preferred[index_arch_Fast_Rep_String]
		  |= (bit_arch_Fast_Rep_String
		      | bit_arch_Fast_Unaligned_Load
		      | bit_arch_Fast_Unaligned_Copy
		      | bit_arch_Prefer_PMINUB_for_stringop);
	      break;

	    case INTEL_BIGCORE_NEHALEM:
	    case INTEL_BIGCORE_WESTMERE:
	      /* Older CPUs prefer non-temporal stores at lower threshold.  */
	      cpu_features->cachesize_non_temporal_divisor = 8;
	      goto enable_modern_features;

	      /* Older Bigcore microarch (smaller non-temporal store
		 threshold).  */
	    case INTEL_BIGCORE_SANDYBRIDGE:
	    case INTEL_BIGCORE_IVYBRIDGE:
	    case INTEL_BIGCORE_HASWELL:
	    case INTEL_BIGCORE_BROADWELL:
	      cpu_features->cachesize_non_temporal_divisor = 8;
	      goto default_tuning;

	      /* Newer Bigcore microarch (larger non-temporal store
		 threshold).  */
	    case INTEL_BIGCORE_SKYLAKE_AVX512:
	    case INTEL_BIGCORE_CANNONLAKE:
	      /* Benchmarks indicate non-temporal memset is not
		     necessarily profitable on SKX (and in some cases much
		     worse). This is likely unique to SKX due its it unique
		     mesh interconnect (not present on ICX or BWD). Disable
		     non-temporal on all Skylake servers. */
	      cpu_features->preferred[index_arch_Avoid_Non_Temporal_Memset]
		  |= bit_arch_Avoid_Non_Temporal_Memset;
	    case INTEL_BIGCORE_COMETLAKE:
	    case INTEL_BIGCORE_SKYLAKE:
	    case INTEL_BIGCORE_KABYLAKE:
	    case INTEL_BIGCORE_ICELAKE:
	    case INTEL_BIGCORE_TIGERLAKE:
	    case INTEL_BIGCORE_ROCKETLAKE:
	    case INTEL_BIGCORE_RAPTORLAKE:
	    case INTEL_BIGCORE_METEORLAKE:
	    case INTEL_BIGCORE_LUNARLAKE:
	    case INTEL_BIGCORE_ARROWLAKE:
	    case INTEL_BIGCORE_SAPPHIRERAPIDS:
	    case INTEL_BIGCORE_EMERALDRAPIDS:
	    case INTEL_BIGCORE_GRANITERAPIDS:
	      cpu_features->cachesize_non_temporal_divisor = 2;
	      goto default_tuning;

	      /* Default tuned Mixed (bigcore + atom SOC). */
	    case INTEL_MIXED_LAKEFIELD:
	    case INTEL_MIXED_ALDERLAKE:
	      cpu_features->cachesize_non_temporal_divisor = 2;
	      goto default_tuning;
	    }

	      /* Disable TSX on some processors to avoid TSX on kernels that
		 weren't updated with the latest microcode package (which
		 disables broken feature by default).  */
	  switch (microarch)
	    {
	    case INTEL_BIGCORE_SKYLAKE_AVX512:
	      /* 0x55 (Skylake-avx512) && stepping <= 5 disable TSX. */
	      if (stepping <= 5)
		goto disable_tsx;
	      break;

	    case INTEL_BIGCORE_KABYLAKE:
	      /* NB: Although the errata documents that for model == 0x8e
		     (kabylake skylake client), only 0xb stepping or lower are
		     impacted, the intention of the errata was to disable TSX on
		     all client processors on all steppings.  Include 0xc
		     stepping which is an Intel Core i7-8665U, a client mobile
		     processor.  */
	      if (stepping > 0xc)
		break;
	      /* Fall through.  */
	    case INTEL_BIGCORE_SKYLAKE:
		/* Disable Intel TSX and enable RTM_ALWAYS_ABORT for
		   processors listed in:

https://www.intel.com/content/www/us/en/support/articles/000059422/processors.html
		 */
	    disable_tsx:
		CPU_FEATURE_UNSET (cpu_features, HLE);
		CPU_FEATURE_UNSET (cpu_features, RTM);
		CPU_FEATURE_SET (cpu_features, RTM_ALWAYS_ABORT);
		break;

	    case INTEL_BIGCORE_HASWELL:
		/* Xeon E7 v3 (model == 0x3f) with stepping >= 4 has working
		   TSX.  Haswell also include other model numbers that have
		   working TSX.  */
		if (model == 0x3f && stepping >= 4)
		break;

		CPU_FEATURE_UNSET (cpu_features, RTM);
		break;
	    }
	}


      /* Since AVX512ER is unique to Xeon Phi, set Prefer_No_VZEROUPPER
         if AVX512ER is available.  Don't use AVX512 to avoid lower CPU
	 frequency if AVX512ER isn't available.  */
      if (CPU_FEATURES_CPU_P (cpu_features, AVX512ER))
	cpu_features->preferred[index_arch_Prefer_No_VZEROUPPER]
	  |= bit_arch_Prefer_No_VZEROUPPER;
      else
	{
	  /* Processors with AVX512 and AVX-VNNI won't lower CPU frequency
	     when ZMM load and store instructions are used.  */
	  if (!CPU_FEATURES_CPU_P (cpu_features, AVX_VNNI))
	    cpu_features->preferred[index_arch_Prefer_No_AVX512]
	      |= bit_arch_Prefer_No_AVX512;

	  /* Avoid RTM abort triggered by VZEROUPPER inside a
	     transactionally executing RTM region.  */
	  if (CPU_FEATURE_USABLE_P (cpu_features, RTM))
	    cpu_features->preferred[index_arch_Prefer_No_VZEROUPPER]
	      |= bit_arch_Prefer_No_VZEROUPPER;
	}

      /* Avoid avoid short distance REP MOVSB on processor with FSRM.  */
      if (CPU_FEATURES_CPU_P (cpu_features, FSRM))
	cpu_features->preferred[index_arch_Avoid_Short_Distance_REP_MOVSB]
	  |= bit_arch_Avoid_Short_Distance_REP_MOVSB;
    }
  /* This spells out "AuthenticAMD" or "HygonGenuine".  */
  else if ((ebx == 0x68747541 && ecx == 0x444d4163 && edx == 0x69746e65)
	   || (ebx == 0x6f677948 && ecx == 0x656e6975 && edx == 0x6e65476e))
    {
      unsigned int extended_model;

      kind = arch_kind_amd;

      get_common_indices (cpu_features, &family, &model, &extended_model,
			  &stepping);

      get_extended_indices (cpu_features);

      update_active (cpu_features);

      ecx = cpu_features->features[CPUID_INDEX_1].cpuid.ecx;

      if (CPU_FEATURE_USABLE_P (cpu_features, AVX))
	{
	  /* Since the FMA4 bit is in CPUID_INDEX_80000001 and
	     FMA4 requires AVX, determine if FMA4 is usable here.  */
	  CPU_FEATURE_SET_ACTIVE (cpu_features, FMA4);
	}

      if (family == 0x15)
	{
	  /* "Excavator"   */
	  if (model >= 0x60 && model <= 0x7f)
	  {
	    cpu_features->preferred[index_arch_Fast_Unaligned_Load]
	      |= (bit_arch_Fast_Unaligned_Load
		  | bit_arch_Fast_Copy_Backward);

	    /* Unaligned AVX loads are slower.*/
	    cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
	      &= ~bit_arch_AVX_Fast_Unaligned_Load;
	  }
	}
    }
  /* This spells out "CentaurHauls" or " Shanghai ".  */
  else if ((ebx == 0x746e6543 && ecx == 0x736c7561 && edx == 0x48727561)
	   || (ebx == 0x68532020 && ecx == 0x20206961 && edx == 0x68676e61))
    {
      unsigned int extended_model, stepping;

      kind = arch_kind_zhaoxin;

      get_common_indices (cpu_features, &family, &model, &extended_model,
			  &stepping);

      get_extended_indices (cpu_features);

      update_active (cpu_features);

      model += extended_model;
      if (family == 0x6)
	{
	  /* Tuning for older Zhaoxin processors.  */
	  if (model == 0xf || model == 0x19)
	    {
	      CPU_FEATURE_UNSET (cpu_features, AVX);
	      CPU_FEATURE_UNSET (cpu_features, AVX2);

	      cpu_features->preferred[index_arch_Slow_SSE4_2]
		  |= bit_arch_Slow_SSE4_2;

	      /*  Unaligned AVX loads are slower.  */
	      cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
		  &= ~bit_arch_AVX_Fast_Unaligned_Load;
	    }
	}
      else if (family == 0x7)
	{
	  switch (model)
	    {
	      /* Wudaokou microarch tuning.  */
	    case 0x1b:
	      CPU_FEATURE_UNSET (cpu_features, AVX);
	      CPU_FEATURE_UNSET (cpu_features, AVX2);

	      cpu_features->preferred[index_arch_Slow_SSE4_2]
		  |= bit_arch_Slow_SSE4_2;

	      cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
		  &= ~bit_arch_AVX_Fast_Unaligned_Load;
	      break;

	      /* Lujiazui microarch tuning.  */
	    case 0x3b:
	      CPU_FEATURE_UNSET (cpu_features, AVX);
	      CPU_FEATURE_UNSET (cpu_features, AVX2);

	      cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
		  &= ~bit_arch_AVX_Fast_Unaligned_Load;
	      break;

	      /* Yongfeng and Shijidadao mircoarch tuning.  */
	    case 0x5b:
	      cpu_features->cachesize_non_temporal_divisor = 2;
	    case 0x6b:
	      cpu_features->preferred[index_arch_AVX_Fast_Unaligned_Load]
		  &= ~bit_arch_AVX_Fast_Unaligned_Load;

	      /* To use sse2_unaligned versions of memset, strcpy and strcat.
	       */
	      cpu_features->preferred[index_arch_Prefer_No_VZEROUPPER]
		  |= (bit_arch_Prefer_No_VZEROUPPER
		      | bit_arch_Fast_Unaligned_Load);
	      break;
	    }
	}
    }
  else
    {
      kind = arch_kind_other;
      get_common_indices (cpu_features, NULL, NULL, NULL, NULL);
      update_active (cpu_features);
    }

  /* Support i586 if CX8 is available.  */
  if (CPU_FEATURES_CPU_P (cpu_features, CX8))
    cpu_features->preferred[index_arch_I586] |= bit_arch_I586;

  /* Support i686 if CMOV is available.  */
  if (CPU_FEATURES_CPU_P (cpu_features, CMOV))
    cpu_features->preferred[index_arch_I686] |= bit_arch_I686;

#if !HAS_CPUID
no_cpuid:
#endif

  cpu_features->basic.kind = kind;
  cpu_features->basic.family = family;
  cpu_features->basic.model = model;
  cpu_features->basic.stepping = stepping;

  dl_init_cacheinfo (cpu_features);

  TUNABLE_GET (hwcaps, tunable_val_t *, TUNABLE_CALLBACK (set_hwcaps));

#ifdef __LP64__
  TUNABLE_GET (prefer_map_32bit_exec, tunable_val_t *,
	       TUNABLE_CALLBACK (set_prefer_map_32bit_exec));
#endif

  bool disable_xsave_features = false;

  if (!CPU_FEATURE_USABLE_P (cpu_features, OSXSAVE))
    {
      /* These features are usable only if OSXSAVE is usable.  */
      CPU_FEATURE_UNSET (cpu_features, XSAVE);
      CPU_FEATURE_UNSET (cpu_features, XSAVEOPT);
      CPU_FEATURE_UNSET (cpu_features, XSAVEC);
      CPU_FEATURE_UNSET (cpu_features, XGETBV_ECX_1);
      CPU_FEATURE_UNSET (cpu_features, XFD);

      disable_xsave_features = true;
    }

  if (disable_xsave_features
      || (!CPU_FEATURE_USABLE_P (cpu_features, XSAVE)
	  && !CPU_FEATURE_USABLE_P (cpu_features, XSAVEC)))
    {
      /* Clear xsave_state_size if both XSAVE and XSAVEC aren't usable.  */
      cpu_features->xsave_state_size = 0;

      CPU_FEATURE_UNSET (cpu_features, AVX);
      CPU_FEATURE_UNSET (cpu_features, AVX2);
      CPU_FEATURE_UNSET (cpu_features, AVX_VNNI);
      CPU_FEATURE_UNSET (cpu_features, FMA);
      CPU_FEATURE_UNSET (cpu_features, VAES);
      CPU_FEATURE_UNSET (cpu_features, VPCLMULQDQ);
      CPU_FEATURE_UNSET (cpu_features, XOP);
      CPU_FEATURE_UNSET (cpu_features, F16C);
      CPU_FEATURE_UNSET (cpu_features, AVX512F);
      CPU_FEATURE_UNSET (cpu_features, AVX512CD);
      CPU_FEATURE_UNSET (cpu_features, AVX512ER);
      CPU_FEATURE_UNSET (cpu_features, AVX512PF);
      CPU_FEATURE_UNSET (cpu_features, AVX512VL);
      CPU_FEATURE_UNSET (cpu_features, AVX512DQ);
      CPU_FEATURE_UNSET (cpu_features, AVX512BW);
      CPU_FEATURE_UNSET (cpu_features, AVX512_4FMAPS);
      CPU_FEATURE_UNSET (cpu_features, AVX512_4VNNIW);
      CPU_FEATURE_UNSET (cpu_features, AVX512_BITALG);
      CPU_FEATURE_UNSET (cpu_features, AVX512_IFMA);
      CPU_FEATURE_UNSET (cpu_features, AVX512_VBMI);
      CPU_FEATURE_UNSET (cpu_features, AVX512_VBMI2);
      CPU_FEATURE_UNSET (cpu_features, AVX512_VNNI);
      CPU_FEATURE_UNSET (cpu_features, AVX512_VPOPCNTDQ);
      CPU_FEATURE_UNSET (cpu_features, AVX512_VP2INTERSECT);
      CPU_FEATURE_UNSET (cpu_features, AVX512_BF16);
      CPU_FEATURE_UNSET (cpu_features, AVX512_FP16);
      CPU_FEATURE_UNSET (cpu_features, AMX_BF16);
      CPU_FEATURE_UNSET (cpu_features, AMX_TILE);
      CPU_FEATURE_UNSET (cpu_features, AMX_INT8);

      CPU_FEATURE_UNSET (cpu_features, FMA4);
    }

#ifdef __x86_64__
  GLRO(dl_hwcap) = HWCAP_X86_64;
  if (cpu_features->basic.kind == arch_kind_intel)
    {
      const char *platform = NULL;

      if (CPU_FEATURE_USABLE_P (cpu_features, AVX512CD))
	{
	  if (CPU_FEATURE_USABLE_P (cpu_features, AVX512ER))
	    {
	      if (CPU_FEATURE_USABLE_P (cpu_features, AVX512PF))
		platform = "xeon_phi";
	    }
	  else
	    {
	      if (CPU_FEATURE_USABLE_P (cpu_features, AVX512BW)
		  && CPU_FEATURE_USABLE_P (cpu_features, AVX512DQ)
		  && CPU_FEATURE_USABLE_P (cpu_features, AVX512VL))
		GLRO(dl_hwcap) |= HWCAP_X86_AVX512_1;
	    }
	}

      if (platform == NULL
	  && CPU_FEATURE_USABLE_P (cpu_features, AVX2)
	  && CPU_FEATURE_USABLE_P (cpu_features, FMA)
	  && CPU_FEATURE_USABLE_P (cpu_features, BMI1)
	  && CPU_FEATURE_USABLE_P (cpu_features, BMI2)
	  && CPU_FEATURE_USABLE_P (cpu_features, LZCNT)
	  && CPU_FEATURE_USABLE_P (cpu_features, MOVBE)
	  && CPU_FEATURE_USABLE_P (cpu_features, POPCNT))
	platform = "haswell";

      if (platform != NULL)
	GLRO(dl_platform) = platform;
    }
#else
  GLRO(dl_hwcap) = 0;
  if (CPU_FEATURE_USABLE_P (cpu_features, SSE2))
    GLRO(dl_hwcap) |= HWCAP_X86_SSE2;

  if (CPU_FEATURES_ARCH_P (cpu_features, I686))
    GLRO(dl_platform) = "i686";
  else if (CPU_FEATURES_ARCH_P (cpu_features, I586))
    GLRO(dl_platform) = "i586";
#endif

#if CET_ENABLED
  TUNABLE_GET (x86_ibt, tunable_val_t *,
	       TUNABLE_CALLBACK (set_x86_ibt));
  TUNABLE_GET (x86_shstk, tunable_val_t *,
	       TUNABLE_CALLBACK (set_x86_shstk));
#endif

  if (MINIMUM_X86_ISA_LEVEL >= AVX_X86_ISA_LEVEL
      || (GLRO(dl_x86_cpu_features).xsave_state_size != 0))
    {
      if (CPU_FEATURE_USABLE_P (cpu_features, XSAVEC))
	{
#ifdef __x86_64__
	  GLRO(dl_x86_64_runtime_resolve) = _dl_runtime_resolve_xsavec;
#endif
#ifdef SHARED
	  GLRO(dl_x86_tlsdesc_dynamic) = _dl_tlsdesc_dynamic_xsavec;
#endif
	}
      else
	{
#ifdef __x86_64__
	  GLRO(dl_x86_64_runtime_resolve) = _dl_runtime_resolve_xsave;
#endif
#ifdef SHARED
	  GLRO(dl_x86_tlsdesc_dynamic) = _dl_tlsdesc_dynamic_xsave;
#endif
	}
    }
  else
    {
#ifdef __x86_64__
      GLRO(dl_x86_64_runtime_resolve) = _dl_runtime_resolve_fxsave;
# ifdef SHARED
      GLRO(dl_x86_tlsdesc_dynamic) = _dl_tlsdesc_dynamic_fxsave;
# endif
#else
# ifdef SHARED
      if (CPU_FEATURE_USABLE_P (cpu_features, FXSR))
	GLRO(dl_x86_tlsdesc_dynamic) = _dl_tlsdesc_dynamic_fxsave;
      else
	GLRO(dl_x86_tlsdesc_dynamic) = _dl_tlsdesc_dynamic_fnsave;
# endif
#endif
    }

#ifdef SHARED
# ifdef __x86_64__
  TUNABLE_GET (plt_rewrite, tunable_val_t *,
	       TUNABLE_CALLBACK (set_plt_rewrite));
# endif
#else
  /* NB: In libc.a, call init_cacheinfo.  */
  init_cacheinfo ();
#endif
}
