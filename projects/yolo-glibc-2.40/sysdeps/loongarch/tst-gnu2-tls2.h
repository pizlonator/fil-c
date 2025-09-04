/* Test TLSDESC relocation.  LoongArch64 version.
   Copyright (C) 2024 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <sys/auxv.h>

/* The instruction between BEFORE_TLSDESC_CALL and _dl_tlsdesc_dynamic,
   and the instruction between _dl_tlsdesc_dynamic and AFTER_TLSDESC_CALL,
   may modified most of the general-purpose register.  */
#define	SAVE_REGISTER(src)						\
  asm volatile ("st.d $r3, %0" :"=m"(src) :);

#ifdef __loongarch_soft_float

#define BEFORE_TLSDESC_CALL()						\
  uint64_t src;								\
  SAVE_REGISTER (src);

#define AFTER_TLSDESC_CALL()						\
  uint64_t restore;							\
  SAVE_REGISTER (restore);						\
  if (src != restore)							\
    abort ();

#else /* hard float */

#define SAVE_REGISTER_FCC(src)				\
  asm volatile ("movcf2gr %0, $fcc0" :"=r"(src[0]));	\
  asm volatile ("movcf2gr %0, $fcc1" :"=r"(src[1]));	\
  asm volatile ("movcf2gr %0, $fcc2" :"=r"(src[2]));	\
  asm volatile ("movcf2gr %0, $fcc3" :"=r"(src[3]));	\
  asm volatile ("movcf2gr %0, $fcc4" :"=r"(src[4]));	\
  asm volatile ("movcf2gr %0, $fcc5" :"=r"(src[5]));	\
  asm volatile ("movcf2gr %0, $fcc6" :"=r"(src[6]));	\
  asm volatile ("movcf2gr %0, $fcc7" :"=r"(src[7]));	\

#define LOAD_REGISTER_FCSR()				\
  uint64_t src_fcsr = 0x01010101;			\
  asm volatile ("li.d $t0, 0x01010101" ::: "$t0");	\
  asm volatile ("movgr2fcsr $fcsr0, $t0" :::);

#define SAVE_REGISTER_FCSR()						\
  uint64_t restore_fcsr;						\
  asm volatile ("movfcsr2gr %0, $fcsr0" :"=r"(restore_fcsr));		\
  if (src_fcsr != restore_fcsr)						\
    {									\
      printf ("FCSR registers compare failed!\n");			\
      abort ();								\
    }									\

#define INIT_TLSDESC_CALL()						\
  unsigned long hwcap = getauxval (AT_HWCAP);

#define	LOAD_REGISTER_FLOAT()						\
  for (int i = 0; i < 32; i++)						\
    src_float[i] = i + 1;						\
  asm volatile ("fld.d $f0, %0" ::"m"(src_float[0]) :"$f0");		\
  asm volatile ("fld.d $f1, %0" ::"m"(src_float[1]) :"$f1"); 		\
  asm volatile ("fld.d $f2, %0" ::"m"(src_float[2]) :"$f2"); 		\
  asm volatile ("fld.d $f3, %0" ::"m"(src_float[3]) :"$f3"); 		\
  asm volatile ("fld.d $f4, %0" ::"m"(src_float[4]) :"$f4"); 		\
  asm volatile ("fld.d $f5, %0" ::"m"(src_float[5]) :"$f5"); 		\
  asm volatile ("fld.d $f6, %0" ::"m"(src_float[6]) :"$f6"); 		\
  asm volatile ("fld.d $f7, %0" ::"m"(src_float[7]) :"$f7"); 		\
  asm volatile ("fld.d $f8, %0" ::"m"(src_float[8]) :"$f8"); 		\
  asm volatile ("fld.d $f9, %0" ::"m"(src_float[9]) :"$f9"); 		\
  asm volatile ("fld.d $f10, %0" ::"m"(src_float[10]) :"$f10");		\
  asm volatile ("fld.d $f11, %0" ::"m"(src_float[11]) :"$f11");		\
  asm volatile ("fld.d $f12, %0" ::"m"(src_float[12]) :"$f12");		\
  asm volatile ("fld.d $f13, %0" ::"m"(src_float[13]) :"$f13");		\
  asm volatile ("fld.d $f14, %0" ::"m"(src_float[14]) :"$f14");		\
  asm volatile ("fld.d $f15, %0" ::"m"(src_float[15]) :"$f15");		\
  asm volatile ("fld.d $f16, %0" ::"m"(src_float[16]) :"$f16");		\
  asm volatile ("fld.d $f17, %0" ::"m"(src_float[17]) :"$f17");		\
  asm volatile ("fld.d $f18, %0" ::"m"(src_float[18]) :"$f18");		\
  asm volatile ("fld.d $f19, %0" ::"m"(src_float[19]) :"$f19");		\
  asm volatile ("fld.d $f20, %0" ::"m"(src_float[20]) :"$f20");		\
  asm volatile ("fld.d $f21, %0" ::"m"(src_float[21]) :"$f21");		\
  asm volatile ("fld.d $f22, %0" ::"m"(src_float[22]) :"$f22");		\
  asm volatile ("fld.d $f23, %0" ::"m"(src_float[23]) :"$f23");		\
  asm volatile ("fld.d $f24, %0" ::"m"(src_float[24]) :"$f24");		\
  asm volatile ("fld.d $f25, %0" ::"m"(src_float[25]) :"$f25");		\
  asm volatile ("fld.d $f26, %0" ::"m"(src_float[26]) :"$f26");		\
  asm volatile ("fld.d $f27, %0" ::"m"(src_float[27]) :"$f27");		\
  asm volatile ("fld.d $f28, %0" ::"m"(src_float[28]) :"$f28");		\
  asm volatile ("fld.d $f29, %0" ::"m"(src_float[29]) :"$f29");		\
  asm volatile ("fld.d $f30, %0" ::"m"(src_float[30]) :"$f30");		\
  asm volatile ("fld.d $f31, %0" ::"m"(src_float[31]) :"$f31");

#define	SAVE_REGISTER_FLOAT()						\
  double restore_float[32];						\
  asm volatile ("fst.d $f0, %0" :"=m"(restore_float[0]));		\
  asm volatile ("fst.d $f1, %0" :"=m"(restore_float[1])); 		\
  asm volatile ("fst.d $f2, %0" :"=m"(restore_float[2])); 		\
  asm volatile ("fst.d $f3, %0" :"=m"(restore_float[3])); 		\
  asm volatile ("fst.d $f4, %0" :"=m"(restore_float[4])); 		\
  asm volatile ("fst.d $f5, %0" :"=m"(restore_float[5])); 		\
  asm volatile ("fst.d $f6, %0" :"=m"(restore_float[6])); 		\
  asm volatile ("fst.d $f7, %0" :"=m"(restore_float[7])); 		\
  asm volatile ("fst.d $f8, %0" :"=m"(restore_float[8])); 		\
  asm volatile ("fst.d $f9, %0" :"=m"(restore_float[9])); 		\
  asm volatile ("fst.d $f10, %0" :"=m"(restore_float[10]));		\
  asm volatile ("fst.d $f11, %0" :"=m"(restore_float[11]));		\
  asm volatile ("fst.d $f12, %0" :"=m"(restore_float[12]));		\
  asm volatile ("fst.d $f13, %0" :"=m"(restore_float[13]));		\
  asm volatile ("fst.d $f14, %0" :"=m"(restore_float[14]));		\
  asm volatile ("fst.d $f15, %0" :"=m"(restore_float[15]));		\
  asm volatile ("fst.d $f16, %0" :"=m"(restore_float[16]));		\
  asm volatile ("fst.d $f17, %0" :"=m"(restore_float[17]));		\
  asm volatile ("fst.d $f18, %0" :"=m"(restore_float[18]));		\
  asm volatile ("fst.d $f19, %0" :"=m"(restore_float[19]));		\
  asm volatile ("fst.d $f20, %0" :"=m"(restore_float[20]));		\
  asm volatile ("fst.d $f21, %0" :"=m"(restore_float[21]));		\
  asm volatile ("fst.d $f22, %0" :"=m"(restore_float[22]));		\
  asm volatile ("fst.d $f23, %0" :"=m"(restore_float[23]));		\
  asm volatile ("fst.d $f24, %0" :"=m"(restore_float[24]));		\
  asm volatile ("fst.d $f25, %0" :"=m"(restore_float[25]));		\
  asm volatile ("fst.d $f26, %0" :"=m"(restore_float[26]));		\
  asm volatile ("fst.d $f27, %0" :"=m"(restore_float[27]));		\
  asm volatile ("fst.d $f28, %0" :"=m"(restore_float[28]));		\
  asm volatile ("fst.d $f29, %0" :"=m"(restore_float[29]));		\
  asm volatile ("fst.d $f30, %0" :"=m"(restore_float[30]));		\
  asm volatile ("fst.d $f31, %0" :"=m"(restore_float[31]));		\
  if (memcmp (src_float, restore_float, sizeof (src_float)) != 0)	\
    {									\
      printf ("Float registers compare failed!\n");			\
      abort ();								\
    }

#ifdef HAVE_LOONGARCH_VEC_COM
  #define	LOAD_REGISTER_LSX()					\
    /* Every byte in $vr0 is 1.  */					\
    asm volatile ("vldi $vr0, 1" ::: "$vr0");				\
    asm volatile ("vldi $vr1, 2" ::: "$vr1"); 				\
    asm volatile ("vldi $vr2, 3" ::: "$vr2"); 				\
    asm volatile ("vldi $vr3, 4" ::: "$vr3"); 				\
    asm volatile ("vldi $vr4, 5" ::: "$vr4"); 				\
    asm volatile ("vldi $vr5, 6" ::: "$vr5"); 				\
    asm volatile ("vldi $vr6, 7" ::: "$vr6"); 				\
    asm volatile ("vldi $vr7, 8" ::: "$vr7"); 				\
    asm volatile ("vldi $vr8, 9" ::: "$vr8"); 				\
    asm volatile ("vldi $vr9, 10" ::: "$vr9"); 				\
    asm volatile ("vldi $vr10, 11" ::: "$vr10");			\
    asm volatile ("vldi $vr11, 12" ::: "$vr11");			\
    asm volatile ("vldi $vr12, 13" ::: "$vr12");			\
    asm volatile ("vldi $vr13, 14" ::: "$vr13");			\
    asm volatile ("vldi $vr14, 15" ::: "$vr14");			\
    asm volatile ("vldi $vr15, 16" ::: "$vr15");			\
    asm volatile ("vldi $vr16, 17" ::: "$vr16");			\
    asm volatile ("vldi $vr17, 18" ::: "$vr17");			\
    asm volatile ("vldi $vr18, 19" ::: "$vr18");			\
    asm volatile ("vldi $vr19, 20" ::: "$vr19");			\
    asm volatile ("vldi $vr20, 21" ::: "$vr20");			\
    asm volatile ("vldi $vr21, 22" ::: "$vr21");			\
    asm volatile ("vldi $vr22, 23" ::: "$vr22");			\
    asm volatile ("vldi $vr23, 24" ::: "$vr23");			\
    asm volatile ("vldi $vr24, 25" ::: "$vr24");			\
    asm volatile ("vldi $vr25, 26" ::: "$vr25");			\
    asm volatile ("vldi $vr26, 27" ::: "$vr26");			\
    asm volatile ("vldi $vr27, 28" ::: "$vr27");			\
    asm volatile ("vldi $vr28, 29" ::: "$vr28");			\
    asm volatile ("vldi $vr29, 30" ::: "$vr29");			\
    asm volatile ("vldi $vr30, 31" ::: "$vr30");			\
    asm volatile ("vldi $vr31, 32" ::: "$vr31");
#else
  #define	LOAD_REGISTER_LSX()
#endif

#ifdef HAVE_LOONGARCH_VEC_COM
  #define	SAVE_REGISTER_LSX()					\
    int src_lsx[32][4];							\
    int restore_lsx[32][4];						\
    asm volatile ("vst $vr0, %0" :"=m"(restore_lsx[0]));		\
    asm volatile ("vst $vr1, %0" :"=m"(restore_lsx[1])); 		\
    asm volatile ("vst $vr2, %0" :"=m"(restore_lsx[2])); 		\
    asm volatile ("vst $vr3, %0" :"=m"(restore_lsx[3])); 		\
    asm volatile ("vst $vr4, %0" :"=m"(restore_lsx[4])); 		\
    asm volatile ("vst $vr5, %0" :"=m"(restore_lsx[5])); 		\
    asm volatile ("vst $vr6, %0" :"=m"(restore_lsx[6])); 		\
    asm volatile ("vst $vr7, %0" :"=m"(restore_lsx[7])); 		\
    asm volatile ("vst $vr8, %0" :"=m"(restore_lsx[8])); 		\
    asm volatile ("vst $vr9, %0" :"=m"(restore_lsx[9])); 		\
    asm volatile ("vst $vr10, %0" :"=m"(restore_lsx[10]));		\
    asm volatile ("vst $vr11, %0" :"=m"(restore_lsx[11]));		\
    asm volatile ("vst $vr12, %0" :"=m"(restore_lsx[12]));		\
    asm volatile ("vst $vr13, %0" :"=m"(restore_lsx[13]));		\
    asm volatile ("vst $vr14, %0" :"=m"(restore_lsx[14]));		\
    asm volatile ("vst $vr15, %0" :"=m"(restore_lsx[15]));		\
    asm volatile ("vst $vr16, %0" :"=m"(restore_lsx[16]));		\
    asm volatile ("vst $vr17, %0" :"=m"(restore_lsx[17]));		\
    asm volatile ("vst $vr18, %0" :"=m"(restore_lsx[18]));		\
    asm volatile ("vst $vr19, %0" :"=m"(restore_lsx[19]));		\
    asm volatile ("vst $vr20, %0" :"=m"(restore_lsx[20]));		\
    asm volatile ("vst $vr21, %0" :"=m"(restore_lsx[21]));		\
    asm volatile ("vst $vr22, %0" :"=m"(restore_lsx[22]));		\
    asm volatile ("vst $vr23, %0" :"=m"(restore_lsx[23]));		\
    asm volatile ("vst $vr24, %0" :"=m"(restore_lsx[24]));		\
    asm volatile ("vst $vr25, %0" :"=m"(restore_lsx[25]));		\
    asm volatile ("vst $vr26, %0" :"=m"(restore_lsx[26]));		\
    asm volatile ("vst $vr27, %0" :"=m"(restore_lsx[27]));		\
    asm volatile ("vst $vr28, %0" :"=m"(restore_lsx[28]));		\
    asm volatile ("vst $vr29, %0" :"=m"(restore_lsx[29]));		\
    asm volatile ("vst $vr30, %0" :"=m"(restore_lsx[30]));		\
    asm volatile ("vst $vr31, %0" :"=m"(restore_lsx[31]));		\
    for (int i = 0; i < 32; i++)					\
      for (int j = 0; j < 4; j++)					\
	{								\
	  src_lsx[i][j] = 0x01010101 * (i + 1);				\
	  if (src_lsx[i][j] != restore_lsx[i][j])			\
	    {								\
	      printf ("LSX registers compare failed!\n");		\
	      abort ();							\
	    }								\
	}
#else
  #define	SAVE_REGISTER_LSX()
#endif

#ifdef HAVE_LOONGARCH_VEC_COM
  #define	LOAD_REGISTER_LASX()					\
    /* Every byte in $xr0 is 1.  */					\
    asm volatile ("xvldi $xr0, 1" ::: "$xr0");				\
    asm volatile ("xvldi $xr1, 2" ::: "$xr1"); 				\
    asm volatile ("xvldi $xr2, 3" ::: "$xr2"); 				\
    asm volatile ("xvldi $xr3, 4" ::: "$xr3"); 				\
    asm volatile ("xvldi $xr4, 5" ::: "$xr4"); 				\
    asm volatile ("xvldi $xr5, 6" ::: "$xr5"); 				\
    asm volatile ("xvldi $xr6, 7" ::: "$xr6"); 				\
    asm volatile ("xvldi $xr7, 8" ::: "$xr7"); 				\
    asm volatile ("xvldi $xr8, 9" ::: "$xr8"); 				\
    asm volatile ("xvldi $xr9, 10" ::: "$xr9"); 			\
    asm volatile ("xvldi $xr10, 11" ::: "$xr10");			\
    asm volatile ("xvldi $xr11, 12" ::: "$xr11");			\
    asm volatile ("xvldi $xr12, 13" ::: "$xr12");			\
    asm volatile ("xvldi $xr13, 14" ::: "$xr13");			\
    asm volatile ("xvldi $xr14, 15" ::: "$xr14");			\
    asm volatile ("xvldi $xr15, 16" ::: "$xr15");			\
    asm volatile ("xvldi $xr16, 17" ::: "$xr16");			\
    asm volatile ("xvldi $xr17, 18" ::: "$xr17");			\
    asm volatile ("xvldi $xr18, 19" ::: "$xr18");			\
    asm volatile ("xvldi $xr19, 20" ::: "$xr19");			\
    asm volatile ("xvldi $xr20, 21" ::: "$xr20");			\
    asm volatile ("xvldi $xr21, 22" ::: "$xr21");			\
    asm volatile ("xvldi $xr22, 23" ::: "$xr22");			\
    asm volatile ("xvldi $xr23, 24" ::: "$xr23");			\
    asm volatile ("xvldi $xr24, 25" ::: "$xr24");			\
    asm volatile ("xvldi $xr25, 26" ::: "$xr25");			\
    asm volatile ("xvldi $xr26, 27" ::: "$xr26");			\
    asm volatile ("xvldi $xr27, 28" ::: "$xr27");			\
    asm volatile ("xvldi $xr28, 29" ::: "$xr28");			\
    asm volatile ("xvldi $xr29, 30" ::: "$xr29");			\
    asm volatile ("xvldi $xr30, 31" ::: "$xr30");			\
    asm volatile ("xvldi $xr31, 32" ::: "$xr31");
#else
  #define	LOAD_REGISTER_LASX()
#endif

#ifdef HAVE_LOONGARCH_VEC_COM
  #define	SAVE_REGISTER_LASX()					\
    int src_lasx[32][8];						\
    int restore_lasx[32][8];						\
    asm volatile ("xvst $xr0, %0" :"=m"(restore_lasx[0]));		\
    asm volatile ("xvst $xr1, %0" :"=m"(restore_lasx[1])); 		\
    asm volatile ("xvst $xr2, %0" :"=m"(restore_lasx[2])); 		\
    asm volatile ("xvst $xr3, %0" :"=m"(restore_lasx[3])); 		\
    asm volatile ("xvst $xr4, %0" :"=m"(restore_lasx[4])); 		\
    asm volatile ("xvst $xr5, %0" :"=m"(restore_lasx[5])); 		\
    asm volatile ("xvst $xr6, %0" :"=m"(restore_lasx[6])); 		\
    asm volatile ("xvst $xr7, %0" :"=m"(restore_lasx[7])); 		\
    asm volatile ("xvst $xr8, %0" :"=m"(restore_lasx[8])); 		\
    asm volatile ("xvst $xr9, %0" :"=m"(restore_lasx[9])); 		\
    asm volatile ("xvst $xr10, %0" :"=m"(restore_lasx[10]));		\
    asm volatile ("xvst $xr11, %0" :"=m"(restore_lasx[11]));		\
    asm volatile ("xvst $xr12, %0" :"=m"(restore_lasx[12]));		\
    asm volatile ("xvst $xr13, %0" :"=m"(restore_lasx[13]));		\
    asm volatile ("xvst $xr14, %0" :"=m"(restore_lasx[14]));		\
    asm volatile ("xvst $xr15, %0" :"=m"(restore_lasx[15]));		\
    asm volatile ("xvst $xr16, %0" :"=m"(restore_lasx[16]));		\
    asm volatile ("xvst $xr17, %0" :"=m"(restore_lasx[17]));		\
    asm volatile ("xvst $xr18, %0" :"=m"(restore_lasx[18]));		\
    asm volatile ("xvst $xr19, %0" :"=m"(restore_lasx[19]));		\
    asm volatile ("xvst $xr20, %0" :"=m"(restore_lasx[20]));		\
    asm volatile ("xvst $xr21, %0" :"=m"(restore_lasx[21]));		\
    asm volatile ("xvst $xr22, %0" :"=m"(restore_lasx[22]));		\
    asm volatile ("xvst $xr23, %0" :"=m"(restore_lasx[23]));		\
    asm volatile ("xvst $xr24, %0" :"=m"(restore_lasx[24]));		\
    asm volatile ("xvst $xr25, %0" :"=m"(restore_lasx[25]));		\
    asm volatile ("xvst $xr26, %0" :"=m"(restore_lasx[26]));		\
    asm volatile ("xvst $xr27, %0" :"=m"(restore_lasx[27]));		\
    asm volatile ("xvst $xr28, %0" :"=m"(restore_lasx[28]));		\
    asm volatile ("xvst $xr29, %0" :"=m"(restore_lasx[29]));		\
    asm volatile ("xvst $xr30, %0" :"=m"(restore_lasx[30]));		\
    asm volatile ("xvst $xr31, %0" :"=m"(restore_lasx[31]));		\
    /* memcmp_lasx/strlen_lasx corrupts LSX/LASX registers, */		\
    for (int i = 0; i < 32; i++)					\
      for (int j = 0; j < 8; j++)					\
	{								\
	  src_lasx[i][j] = 0x01010101 * (i + 1);			\
	  if (src_lasx[i][j] != restore_lasx[i][j])			\
	    {								\
	      printf ("LASX registers compare failed!\n");		\
	      abort ();							\
	    }								\
	}
#else
  #define	SAVE_REGISTER_LASX()
#endif

#define BEFORE_TLSDESC_CALL()						\
  uint64_t src;								\
  double src_float[32];							\
  uint64_t src_fcc[8];							\
  SAVE_REGISTER (src);							\
									\
  if (hwcap & HWCAP_LOONGARCH_LASX)					\
    {									\
      LOAD_REGISTER_LASX ();						\
    }									\
  else if (hwcap & HWCAP_LOONGARCH_LSX)					\
    {									\
      LOAD_REGISTER_LSX ();						\
    }									\
  else									\
    {									\
      LOAD_REGISTER_FLOAT ();						\
    }									\
									\
  /* LOAD_REGISTER_FLOAT convert int double may change fcsr.  */	\
  LOAD_REGISTER_FCSR ();						\
  SAVE_REGISTER_FCC (src_fcc)


#define AFTER_TLSDESC_CALL()						\
  uint64_t restore;							\
  uint64_t restore_fcc[8];						\
									\
  SAVE_REGISTER (restore);						\
  if (src != restore)							\
    {									\
      printf ("General registers compare failed!\n");			\
      abort ();								\
    }									\
									\
  SAVE_REGISTER_FCSR ();						\
									\
  SAVE_REGISTER_FCC (restore_fcc)					\
  for (int i = 0; i < 8; i++)						\
    if (src_fcc[i] != restore_fcc[i])					\
      {									\
	printf ("FCC registers compare failed!\n");			\
	abort ();							\
      }									\
									\
  if (hwcap & HWCAP_LOONGARCH_LASX)					\
    {									\
      SAVE_REGISTER_LASX ();						\
    }									\
  else if (hwcap & HWCAP_LOONGARCH_LSX)					\
    {									\
      SAVE_REGISTER_LSX	();						\
    }									\
  else									\
    {									\
      SAVE_REGISTER_FLOAT ();						\
    }									\

#endif /* #ifdef __loongarch_soft_float */

#include_next <tst-gnu2-tls2.h>
