/* aria.c  -  ARIA Cipher Algorithm
 *
 * Copyright (C) 2022-2023 Taehee Yoo <ap420073@gmail.com>
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "types.h"
#include "g10lib.h"
#include "cipher.h"
#include "bufhelp.h"
#include "cipher-internal.h"
#include "bulkhelp.h"

/* Attribute macro to force alignment to 64 bytes.  */
#ifdef HAVE_GCC_ATTRIBUTE_ALIGNED
# define ATTR_ALIGNED_64  __attribute__ ((aligned (64)))
#else
# define ATTR_ALIGNED_64
#endif

/* Attribute macro to force inlining of function. */
#if __GNUC__ >= 4
#  define ALWAYS_INLINE inline __attribute__ ((always_inline))
#else
#  define ALWAYS_INLINE inline
#endif

/* Attribute macro to prevent inlining of function. */
#if __GNUC__ >= 4
#  define NO_INLINE __attribute__ ((noinline))
#else
#  define NO_INLINE
#endif


/* USE_AESNI_AVX inidicates whether to compile with Intel AES-NI/AVX code. */
#undef USE_AESNI_AVX
#if defined(ENABLE_AESNI_SUPPORT) && defined(ENABLE_AVX_SUPPORT)
# if defined(__x86_64__) && (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
#  define USE_AESNI_AVX 1
# endif
#endif

/* USE_GFNI_AVX inidicates whether to compile with Intel GFNI/AVX code. */
#undef USE_GFNI_AVX
#if defined(USE_AESNI_AVX) && defined(ENABLE_GFNI_SUPPORT)
# define USE_GFNI_AVX 1
#endif

/* USE_AESNI_AVX2 inidicates whether to compile with Intel AES-NI/AVX2 code. */
#undef USE_AESNI_AVX2
#if defined(ENABLE_AESNI_SUPPORT) && defined(ENABLE_AVX2_SUPPORT)
# if defined(__x86_64__) && (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
#  define USE_AESNI_AVX2 1
# endif
#endif

/* USE_VAES_AVX2 inidicates whether to compile with Intel VAES/AVX2 code. */
#undef USE_VAES_AVX2
#if defined(USE_AESNI_AVX2) && defined(HAVE_GCC_INLINE_ASM_VAES_VPCLMUL)
# define USE_VAES_AVX2 1
#endif

/* USE_GFNI_AVX2 inidicates whether to compile with Intel GFNI/AVX2 code. */
#undef USE_GFNI_AVX2
#if defined(USE_AESNI_AVX2) && defined(ENABLE_GFNI_SUPPORT)
# define USE_GFNI_AVX2 1
#endif

/* USE_GFNI_AVX512 inidicates whether to compile with Intel GFNI/AVX512 code. */
#undef USE_GFNI_AVX512
#if defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX512_SUPPORT)
# if defined(__x86_64__) && (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))
#  define USE_GFNI_AVX512 1
# endif
#endif

/* How many parallel blocks to handle in bulk processing functions. */
#if defined(USE_GFNI_AVX512)
# define MAX_PARALLEL_BLKS 64
#elif defined(USE_AESNI_AVX2)
# define MAX_PARALLEL_BLKS 32
#elif defined(USE_AESNI_AVX)
# define MAX_PARALLEL_BLKS 16
#else
# define MAX_PARALLEL_BLKS 8
#endif

/* Assembly implementations use SystemV ABI, ABI conversion and additional
 * stack to store XMM6-XMM15 needed on Win64. */
#undef ASM_FUNC_ABI
#undef ASM_EXTRA_STACK
#if defined(USE_AESNI_AVX) || defined(USE_AESNI_AVX2) || \
    defined(USE_GFNI_AVX512)
# ifdef HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS
#  define ASM_FUNC_ABI __attribute__((sysv_abi))
#  define ASM_EXTRA_STACK (10 * 16)
# else
#  define ASM_FUNC_ABI
#  define ASM_EXTRA_STACK 0
# endif
#endif


static const char *aria_selftest (void);


#define ARIA_MIN_KEY_SIZE	16
#define ARIA_MAX_KEY_SIZE	32
#define ARIA_BLOCK_SIZE		16
#define ARIA_MAX_RD_KEYS	17
#define ARIA_RD_KEY_WORDS	(ARIA_BLOCK_SIZE / sizeof(u32))


typedef struct
{
  u32 enc_key[ARIA_MAX_RD_KEYS][ARIA_RD_KEY_WORDS];
  u32 dec_key[ARIA_MAX_RD_KEYS][ARIA_RD_KEY_WORDS];
  int rounds;

  unsigned int decryption_prepared:1; /* The decryption key is set up. */
  unsigned int bulk_prefetch_ready:1; /* Look-up table prefetch ready for
				       * current bulk operation. */

#ifdef USE_AESNI_AVX
  unsigned int use_aesni_avx:1;
  unsigned int use_gfni_avx:1;
#endif
#ifdef USE_AESNI_AVX2
  unsigned int use_aesni_avx2:1;
  unsigned int use_vaes_avx2:1;
  unsigned int use_gfni_avx2:1;
#endif
#ifdef USE_GFNI_AVX512
  unsigned int use_gfni_avx512:1;
#endif
} ARIA_context;


static const u32 key_rc[20] =
  {
    0x517cc1b7, 0x27220a94, 0xfe13abe8, 0xfa9a6ee0,
    0x6db14acc, 0x9e21c820, 0xff28b1d5, 0xef5de2b0,
    0xdb92371d, 0x2126e970, 0x03249775, 0x04e8c90e,
    0x517cc1b7, 0x27220a94, 0xfe13abe8, 0xfa9a6ee0,
    0x6db14acc, 0x9e21c820, 0xff28b1d5, 0xef5de2b0
  };


static struct
{
  volatile u32 counter_head;
  u32 cacheline_align[64 / 4 - 1];
  u32 s1[256];
  u32 s2[256];
  u32 x1[256];
  u32 x2[256];
  volatile u32 counter_tail;
} sboxes ATTR_ALIGNED_64 =
  {
    0,
    { 0, },

    { /* s1 */
      0x00636363, 0x007c7c7c, 0x00777777, 0x007b7b7b,
      0x00f2f2f2, 0x006b6b6b, 0x006f6f6f, 0x00c5c5c5,
      0x00303030, 0x00010101, 0x00676767, 0x002b2b2b,
      0x00fefefe, 0x00d7d7d7, 0x00ababab, 0x00767676,
      0x00cacaca, 0x00828282, 0x00c9c9c9, 0x007d7d7d,
      0x00fafafa, 0x00595959, 0x00474747, 0x00f0f0f0,
      0x00adadad, 0x00d4d4d4, 0x00a2a2a2, 0x00afafaf,
      0x009c9c9c, 0x00a4a4a4, 0x00727272, 0x00c0c0c0,
      0x00b7b7b7, 0x00fdfdfd, 0x00939393, 0x00262626,
      0x00363636, 0x003f3f3f, 0x00f7f7f7, 0x00cccccc,
      0x00343434, 0x00a5a5a5, 0x00e5e5e5, 0x00f1f1f1,
      0x00717171, 0x00d8d8d8, 0x00313131, 0x00151515,
      0x00040404, 0x00c7c7c7, 0x00232323, 0x00c3c3c3,
      0x00181818, 0x00969696, 0x00050505, 0x009a9a9a,
      0x00070707, 0x00121212, 0x00808080, 0x00e2e2e2,
      0x00ebebeb, 0x00272727, 0x00b2b2b2, 0x00757575,
      0x00090909, 0x00838383, 0x002c2c2c, 0x001a1a1a,
      0x001b1b1b, 0x006e6e6e, 0x005a5a5a, 0x00a0a0a0,
      0x00525252, 0x003b3b3b, 0x00d6d6d6, 0x00b3b3b3,
      0x00292929, 0x00e3e3e3, 0x002f2f2f, 0x00848484,
      0x00535353, 0x00d1d1d1, 0x00000000, 0x00ededed,
      0x00202020, 0x00fcfcfc, 0x00b1b1b1, 0x005b5b5b,
      0x006a6a6a, 0x00cbcbcb, 0x00bebebe, 0x00393939,
      0x004a4a4a, 0x004c4c4c, 0x00585858, 0x00cfcfcf,
      0x00d0d0d0, 0x00efefef, 0x00aaaaaa, 0x00fbfbfb,
      0x00434343, 0x004d4d4d, 0x00333333, 0x00858585,
      0x00454545, 0x00f9f9f9, 0x00020202, 0x007f7f7f,
      0x00505050, 0x003c3c3c, 0x009f9f9f, 0x00a8a8a8,
      0x00515151, 0x00a3a3a3, 0x00404040, 0x008f8f8f,
      0x00929292, 0x009d9d9d, 0x00383838, 0x00f5f5f5,
      0x00bcbcbc, 0x00b6b6b6, 0x00dadada, 0x00212121,
      0x00101010, 0x00ffffff, 0x00f3f3f3, 0x00d2d2d2,
      0x00cdcdcd, 0x000c0c0c, 0x00131313, 0x00ececec,
      0x005f5f5f, 0x00979797, 0x00444444, 0x00171717,
      0x00c4c4c4, 0x00a7a7a7, 0x007e7e7e, 0x003d3d3d,
      0x00646464, 0x005d5d5d, 0x00191919, 0x00737373,
      0x00606060, 0x00818181, 0x004f4f4f, 0x00dcdcdc,
      0x00222222, 0x002a2a2a, 0x00909090, 0x00888888,
      0x00464646, 0x00eeeeee, 0x00b8b8b8, 0x00141414,
      0x00dedede, 0x005e5e5e, 0x000b0b0b, 0x00dbdbdb,
      0x00e0e0e0, 0x00323232, 0x003a3a3a, 0x000a0a0a,
      0x00494949, 0x00060606, 0x00242424, 0x005c5c5c,
      0x00c2c2c2, 0x00d3d3d3, 0x00acacac, 0x00626262,
      0x00919191, 0x00959595, 0x00e4e4e4, 0x00797979,
      0x00e7e7e7, 0x00c8c8c8, 0x00373737, 0x006d6d6d,
      0x008d8d8d, 0x00d5d5d5, 0x004e4e4e, 0x00a9a9a9,
      0x006c6c6c, 0x00565656, 0x00f4f4f4, 0x00eaeaea,
      0x00656565, 0x007a7a7a, 0x00aeaeae, 0x00080808,
      0x00bababa, 0x00787878, 0x00252525, 0x002e2e2e,
      0x001c1c1c, 0x00a6a6a6, 0x00b4b4b4, 0x00c6c6c6,
      0x00e8e8e8, 0x00dddddd, 0x00747474, 0x001f1f1f,
      0x004b4b4b, 0x00bdbdbd, 0x008b8b8b, 0x008a8a8a,
      0x00707070, 0x003e3e3e, 0x00b5b5b5, 0x00666666,
      0x00484848, 0x00030303, 0x00f6f6f6, 0x000e0e0e,
      0x00616161, 0x00353535, 0x00575757, 0x00b9b9b9,
      0x00868686, 0x00c1c1c1, 0x001d1d1d, 0x009e9e9e,
      0x00e1e1e1, 0x00f8f8f8, 0x00989898, 0x00111111,
      0x00696969, 0x00d9d9d9, 0x008e8e8e, 0x00949494,
      0x009b9b9b, 0x001e1e1e, 0x00878787, 0x00e9e9e9,
      0x00cecece, 0x00555555, 0x00282828, 0x00dfdfdf,
      0x008c8c8c, 0x00a1a1a1, 0x00898989, 0x000d0d0d,
      0x00bfbfbf, 0x00e6e6e6, 0x00424242, 0x00686868,
      0x00414141, 0x00999999, 0x002d2d2d, 0x000f0f0f,
      0x00b0b0b0, 0x00545454, 0x00bbbbbb, 0x00161616
    },
    { /* s2 */
      0xe200e2e2, 0x4e004e4e, 0x54005454, 0xfc00fcfc,
      0x94009494, 0xc200c2c2, 0x4a004a4a, 0xcc00cccc,
      0x62006262, 0x0d000d0d, 0x6a006a6a, 0x46004646,
      0x3c003c3c, 0x4d004d4d, 0x8b008b8b, 0xd100d1d1,
      0x5e005e5e, 0xfa00fafa, 0x64006464, 0xcb00cbcb,
      0xb400b4b4, 0x97009797, 0xbe00bebe, 0x2b002b2b,
      0xbc00bcbc, 0x77007777, 0x2e002e2e, 0x03000303,
      0xd300d3d3, 0x19001919, 0x59005959, 0xc100c1c1,
      0x1d001d1d, 0x06000606, 0x41004141, 0x6b006b6b,
      0x55005555, 0xf000f0f0, 0x99009999, 0x69006969,
      0xea00eaea, 0x9c009c9c, 0x18001818, 0xae00aeae,
      0x63006363, 0xdf00dfdf, 0xe700e7e7, 0xbb00bbbb,
      0x00000000, 0x73007373, 0x66006666, 0xfb00fbfb,
      0x96009696, 0x4c004c4c, 0x85008585, 0xe400e4e4,
      0x3a003a3a, 0x09000909, 0x45004545, 0xaa00aaaa,
      0x0f000f0f, 0xee00eeee, 0x10001010, 0xeb00ebeb,
      0x2d002d2d, 0x7f007f7f, 0xf400f4f4, 0x29002929,
      0xac00acac, 0xcf00cfcf, 0xad00adad, 0x91009191,
      0x8d008d8d, 0x78007878, 0xc800c8c8, 0x95009595,
      0xf900f9f9, 0x2f002f2f, 0xce00cece, 0xcd00cdcd,
      0x08000808, 0x7a007a7a, 0x88008888, 0x38003838,
      0x5c005c5c, 0x83008383, 0x2a002a2a, 0x28002828,
      0x47004747, 0xdb00dbdb, 0xb800b8b8, 0xc700c7c7,
      0x93009393, 0xa400a4a4, 0x12001212, 0x53005353,
      0xff00ffff, 0x87008787, 0x0e000e0e, 0x31003131,
      0x36003636, 0x21002121, 0x58005858, 0x48004848,
      0x01000101, 0x8e008e8e, 0x37003737, 0x74007474,
      0x32003232, 0xca00caca, 0xe900e9e9, 0xb100b1b1,
      0xb700b7b7, 0xab00abab, 0x0c000c0c, 0xd700d7d7,
      0xc400c4c4, 0x56005656, 0x42004242, 0x26002626,
      0x07000707, 0x98009898, 0x60006060, 0xd900d9d9,
      0xb600b6b6, 0xb900b9b9, 0x11001111, 0x40004040,
      0xec00ecec, 0x20002020, 0x8c008c8c, 0xbd00bdbd,
      0xa000a0a0, 0xc900c9c9, 0x84008484, 0x04000404,
      0x49004949, 0x23002323, 0xf100f1f1, 0x4f004f4f,
      0x50005050, 0x1f001f1f, 0x13001313, 0xdc00dcdc,
      0xd800d8d8, 0xc000c0c0, 0x9e009e9e, 0x57005757,
      0xe300e3e3, 0xc300c3c3, 0x7b007b7b, 0x65006565,
      0x3b003b3b, 0x02000202, 0x8f008f8f, 0x3e003e3e,
      0xe800e8e8, 0x25002525, 0x92009292, 0xe500e5e5,
      0x15001515, 0xdd00dddd, 0xfd00fdfd, 0x17001717,
      0xa900a9a9, 0xbf00bfbf, 0xd400d4d4, 0x9a009a9a,
      0x7e007e7e, 0xc500c5c5, 0x39003939, 0x67006767,
      0xfe00fefe, 0x76007676, 0x9d009d9d, 0x43004343,
      0xa700a7a7, 0xe100e1e1, 0xd000d0d0, 0xf500f5f5,
      0x68006868, 0xf200f2f2, 0x1b001b1b, 0x34003434,
      0x70007070, 0x05000505, 0xa300a3a3, 0x8a008a8a,
      0xd500d5d5, 0x79007979, 0x86008686, 0xa800a8a8,
      0x30003030, 0xc600c6c6, 0x51005151, 0x4b004b4b,
      0x1e001e1e, 0xa600a6a6, 0x27002727, 0xf600f6f6,
      0x35003535, 0xd200d2d2, 0x6e006e6e, 0x24002424,
      0x16001616, 0x82008282, 0x5f005f5f, 0xda00dada,
      0xe600e6e6, 0x75007575, 0xa200a2a2, 0xef00efef,
      0x2c002c2c, 0xb200b2b2, 0x1c001c1c, 0x9f009f9f,
      0x5d005d5d, 0x6f006f6f, 0x80008080, 0x0a000a0a,
      0x72007272, 0x44004444, 0x9b009b9b, 0x6c006c6c,
      0x90009090, 0x0b000b0b, 0x5b005b5b, 0x33003333,
      0x7d007d7d, 0x5a005a5a, 0x52005252, 0xf300f3f3,
      0x61006161, 0xa100a1a1, 0xf700f7f7, 0xb000b0b0,
      0xd600d6d6, 0x3f003f3f, 0x7c007c7c, 0x6d006d6d,
      0xed00eded, 0x14001414, 0xe000e0e0, 0xa500a5a5,
      0x3d003d3d, 0x22002222, 0xb300b3b3, 0xf800f8f8,
      0x89008989, 0xde00dede, 0x71007171, 0x1a001a1a,
      0xaf00afaf, 0xba00baba, 0xb500b5b5, 0x81008181
    },
    { /* x1 */
      0x52520052, 0x09090009, 0x6a6a006a, 0xd5d500d5,
      0x30300030, 0x36360036, 0xa5a500a5, 0x38380038,
      0xbfbf00bf, 0x40400040, 0xa3a300a3, 0x9e9e009e,
      0x81810081, 0xf3f300f3, 0xd7d700d7, 0xfbfb00fb,
      0x7c7c007c, 0xe3e300e3, 0x39390039, 0x82820082,
      0x9b9b009b, 0x2f2f002f, 0xffff00ff, 0x87870087,
      0x34340034, 0x8e8e008e, 0x43430043, 0x44440044,
      0xc4c400c4, 0xdede00de, 0xe9e900e9, 0xcbcb00cb,
      0x54540054, 0x7b7b007b, 0x94940094, 0x32320032,
      0xa6a600a6, 0xc2c200c2, 0x23230023, 0x3d3d003d,
      0xeeee00ee, 0x4c4c004c, 0x95950095, 0x0b0b000b,
      0x42420042, 0xfafa00fa, 0xc3c300c3, 0x4e4e004e,
      0x08080008, 0x2e2e002e, 0xa1a100a1, 0x66660066,
      0x28280028, 0xd9d900d9, 0x24240024, 0xb2b200b2,
      0x76760076, 0x5b5b005b, 0xa2a200a2, 0x49490049,
      0x6d6d006d, 0x8b8b008b, 0xd1d100d1, 0x25250025,
      0x72720072, 0xf8f800f8, 0xf6f600f6, 0x64640064,
      0x86860086, 0x68680068, 0x98980098, 0x16160016,
      0xd4d400d4, 0xa4a400a4, 0x5c5c005c, 0xcccc00cc,
      0x5d5d005d, 0x65650065, 0xb6b600b6, 0x92920092,
      0x6c6c006c, 0x70700070, 0x48480048, 0x50500050,
      0xfdfd00fd, 0xeded00ed, 0xb9b900b9, 0xdada00da,
      0x5e5e005e, 0x15150015, 0x46460046, 0x57570057,
      0xa7a700a7, 0x8d8d008d, 0x9d9d009d, 0x84840084,
      0x90900090, 0xd8d800d8, 0xabab00ab, 0x00000000,
      0x8c8c008c, 0xbcbc00bc, 0xd3d300d3, 0x0a0a000a,
      0xf7f700f7, 0xe4e400e4, 0x58580058, 0x05050005,
      0xb8b800b8, 0xb3b300b3, 0x45450045, 0x06060006,
      0xd0d000d0, 0x2c2c002c, 0x1e1e001e, 0x8f8f008f,
      0xcaca00ca, 0x3f3f003f, 0x0f0f000f, 0x02020002,
      0xc1c100c1, 0xafaf00af, 0xbdbd00bd, 0x03030003,
      0x01010001, 0x13130013, 0x8a8a008a, 0x6b6b006b,
      0x3a3a003a, 0x91910091, 0x11110011, 0x41410041,
      0x4f4f004f, 0x67670067, 0xdcdc00dc, 0xeaea00ea,
      0x97970097, 0xf2f200f2, 0xcfcf00cf, 0xcece00ce,
      0xf0f000f0, 0xb4b400b4, 0xe6e600e6, 0x73730073,
      0x96960096, 0xacac00ac, 0x74740074, 0x22220022,
      0xe7e700e7, 0xadad00ad, 0x35350035, 0x85850085,
      0xe2e200e2, 0xf9f900f9, 0x37370037, 0xe8e800e8,
      0x1c1c001c, 0x75750075, 0xdfdf00df, 0x6e6e006e,
      0x47470047, 0xf1f100f1, 0x1a1a001a, 0x71710071,
      0x1d1d001d, 0x29290029, 0xc5c500c5, 0x89890089,
      0x6f6f006f, 0xb7b700b7, 0x62620062, 0x0e0e000e,
      0xaaaa00aa, 0x18180018, 0xbebe00be, 0x1b1b001b,
      0xfcfc00fc, 0x56560056, 0x3e3e003e, 0x4b4b004b,
      0xc6c600c6, 0xd2d200d2, 0x79790079, 0x20200020,
      0x9a9a009a, 0xdbdb00db, 0xc0c000c0, 0xfefe00fe,
      0x78780078, 0xcdcd00cd, 0x5a5a005a, 0xf4f400f4,
      0x1f1f001f, 0xdddd00dd, 0xa8a800a8, 0x33330033,
      0x88880088, 0x07070007, 0xc7c700c7, 0x31310031,
      0xb1b100b1, 0x12120012, 0x10100010, 0x59590059,
      0x27270027, 0x80800080, 0xecec00ec, 0x5f5f005f,
      0x60600060, 0x51510051, 0x7f7f007f, 0xa9a900a9,
      0x19190019, 0xb5b500b5, 0x4a4a004a, 0x0d0d000d,
      0x2d2d002d, 0xe5e500e5, 0x7a7a007a, 0x9f9f009f,
      0x93930093, 0xc9c900c9, 0x9c9c009c, 0xefef00ef,
      0xa0a000a0, 0xe0e000e0, 0x3b3b003b, 0x4d4d004d,
      0xaeae00ae, 0x2a2a002a, 0xf5f500f5, 0xb0b000b0,
      0xc8c800c8, 0xebeb00eb, 0xbbbb00bb, 0x3c3c003c,
      0x83830083, 0x53530053, 0x99990099, 0x61610061,
      0x17170017, 0x2b2b002b, 0x04040004, 0x7e7e007e,
      0xbaba00ba, 0x77770077, 0xd6d600d6, 0x26260026,
      0xe1e100e1, 0x69690069, 0x14140014, 0x63630063,
      0x55550055, 0x21210021, 0x0c0c000c, 0x7d7d007d
    },
    { /* x2 */
      0x30303000, 0x68686800, 0x99999900, 0x1b1b1b00,
      0x87878700, 0xb9b9b900, 0x21212100, 0x78787800,
      0x50505000, 0x39393900, 0xdbdbdb00, 0xe1e1e100,
      0x72727200, 0x09090900, 0x62626200, 0x3c3c3c00,
      0x3e3e3e00, 0x7e7e7e00, 0x5e5e5e00, 0x8e8e8e00,
      0xf1f1f100, 0xa0a0a000, 0xcccccc00, 0xa3a3a300,
      0x2a2a2a00, 0x1d1d1d00, 0xfbfbfb00, 0xb6b6b600,
      0xd6d6d600, 0x20202000, 0xc4c4c400, 0x8d8d8d00,
      0x81818100, 0x65656500, 0xf5f5f500, 0x89898900,
      0xcbcbcb00, 0x9d9d9d00, 0x77777700, 0xc6c6c600,
      0x57575700, 0x43434300, 0x56565600, 0x17171700,
      0xd4d4d400, 0x40404000, 0x1a1a1a00, 0x4d4d4d00,
      0xc0c0c000, 0x63636300, 0x6c6c6c00, 0xe3e3e300,
      0xb7b7b700, 0xc8c8c800, 0x64646400, 0x6a6a6a00,
      0x53535300, 0xaaaaaa00, 0x38383800, 0x98989800,
      0x0c0c0c00, 0xf4f4f400, 0x9b9b9b00, 0xededed00,
      0x7f7f7f00, 0x22222200, 0x76767600, 0xafafaf00,
      0xdddddd00, 0x3a3a3a00, 0x0b0b0b00, 0x58585800,
      0x67676700, 0x88888800, 0x06060600, 0xc3c3c300,
      0x35353500, 0x0d0d0d00, 0x01010100, 0x8b8b8b00,
      0x8c8c8c00, 0xc2c2c200, 0xe6e6e600, 0x5f5f5f00,
      0x02020200, 0x24242400, 0x75757500, 0x93939300,
      0x66666600, 0x1e1e1e00, 0xe5e5e500, 0xe2e2e200,
      0x54545400, 0xd8d8d800, 0x10101000, 0xcecece00,
      0x7a7a7a00, 0xe8e8e800, 0x08080800, 0x2c2c2c00,
      0x12121200, 0x97979700, 0x32323200, 0xababab00,
      0xb4b4b400, 0x27272700, 0x0a0a0a00, 0x23232300,
      0xdfdfdf00, 0xefefef00, 0xcacaca00, 0xd9d9d900,
      0xb8b8b800, 0xfafafa00, 0xdcdcdc00, 0x31313100,
      0x6b6b6b00, 0xd1d1d100, 0xadadad00, 0x19191900,
      0x49494900, 0xbdbdbd00, 0x51515100, 0x96969600,
      0xeeeeee00, 0xe4e4e400, 0xa8a8a800, 0x41414100,
      0xdadada00, 0xffffff00, 0xcdcdcd00, 0x55555500,
      0x86868600, 0x36363600, 0xbebebe00, 0x61616100,
      0x52525200, 0xf8f8f800, 0xbbbbbb00, 0x0e0e0e00,
      0x82828200, 0x48484800, 0x69696900, 0x9a9a9a00,
      0xe0e0e000, 0x47474700, 0x9e9e9e00, 0x5c5c5c00,
      0x04040400, 0x4b4b4b00, 0x34343400, 0x15151500,
      0x79797900, 0x26262600, 0xa7a7a700, 0xdedede00,
      0x29292900, 0xaeaeae00, 0x92929200, 0xd7d7d700,
      0x84848400, 0xe9e9e900, 0xd2d2d200, 0xbababa00,
      0x5d5d5d00, 0xf3f3f300, 0xc5c5c500, 0xb0b0b000,
      0xbfbfbf00, 0xa4a4a400, 0x3b3b3b00, 0x71717100,
      0x44444400, 0x46464600, 0x2b2b2b00, 0xfcfcfc00,
      0xebebeb00, 0x6f6f6f00, 0xd5d5d500, 0xf6f6f600,
      0x14141400, 0xfefefe00, 0x7c7c7c00, 0x70707000,
      0x5a5a5a00, 0x7d7d7d00, 0xfdfdfd00, 0x2f2f2f00,
      0x18181800, 0x83838300, 0x16161600, 0xa5a5a500,
      0x91919100, 0x1f1f1f00, 0x05050500, 0x95959500,
      0x74747400, 0xa9a9a900, 0xc1c1c100, 0x5b5b5b00,
      0x4a4a4a00, 0x85858500, 0x6d6d6d00, 0x13131300,
      0x07070700, 0x4f4f4f00, 0x4e4e4e00, 0x45454500,
      0xb2b2b200, 0x0f0f0f00, 0xc9c9c900, 0x1c1c1c00,
      0xa6a6a600, 0xbcbcbc00, 0xececec00, 0x73737300,
      0x90909000, 0x7b7b7b00, 0xcfcfcf00, 0x59595900,
      0x8f8f8f00, 0xa1a1a100, 0xf9f9f900, 0x2d2d2d00,
      0xf2f2f200, 0xb1b1b100, 0x00000000, 0x94949400,
      0x37373700, 0x9f9f9f00, 0xd0d0d000, 0x2e2e2e00,
      0x9c9c9c00, 0x6e6e6e00, 0x28282800, 0x3f3f3f00,
      0x80808000, 0xf0f0f000, 0x3d3d3d00, 0xd3d3d300,
      0x25252500, 0x8a8a8a00, 0xb5b5b500, 0xe7e7e700,
      0x42424200, 0xb3b3b300, 0xc7c7c700, 0xeaeaea00,
      0xf7f7f700, 0x4c4c4c00, 0x11111100, 0x33333300,
      0x03030300, 0xa2a2a200, 0xacacac00, 0x60606000
    },
    0
  };

#ifdef USE_AESNI_AVX
extern unsigned int
_gcry_aria_aesni_avx_ecb_crypt_blk1_16(const void *ctx, byte *out,
				       const byte *in, const void *key,
				       u64 nblks) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_aesni_avx_ctr_crypt_blk16(const void *ctx, byte *out,
				     const byte *in, byte *iv) ASM_FUNC_ABI;

#ifdef USE_GFNI_AVX
extern unsigned int
_gcry_aria_gfni_avx_ecb_crypt_blk1_16(const void *ctx, byte *out,
				      const byte *in, const void *key,
				      u64 nblks) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_gfni_avx_ctr_crypt_blk16(const void *ctx, byte *out,
				    const byte *in, byte *iv) ASM_FUNC_ABI;
#endif /* USE_GFNI_AVX */

static inline unsigned int
aria_avx_ecb_crypt_blk1_16(const ARIA_context *ctx, byte *out, const byte *in,
			   const u32 key[][ARIA_RD_KEY_WORDS], size_t nblks)
{
  if (0) { }
#ifdef USE_GFNI_AVX
  else if (ctx->use_gfni_avx)
    return _gcry_aria_gfni_avx_ecb_crypt_blk1_16(ctx, out, in, key, nblks)
		+ ASM_EXTRA_STACK;
#endif /* USE_GFNI_AVX */
  else
    return _gcry_aria_aesni_avx_ecb_crypt_blk1_16(ctx, out, in, key, nblks)
		+ ASM_EXTRA_STACK;
}

static inline unsigned int
aria_avx_ctr_crypt_blk16(const ARIA_context *ctx, byte *out, const byte *in,
			 byte *iv)
{
  if (0) { }
#ifdef USE_GFNI_AVX
  else if (ctx->use_gfni_avx)
    return _gcry_aria_gfni_avx_ctr_crypt_blk16(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
#endif /* USE_GFNI_AVX */
  else
    return _gcry_aria_aesni_avx_ctr_crypt_blk16(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
}
#endif /* USE_AESNI_AVX */

#ifdef USE_AESNI_AVX2
extern unsigned int
_gcry_aria_aesni_avx2_ecb_crypt_blk32(const void *ctx, byte *out,
				      const byte *in,
				      const void *key) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_aesni_avx2_ctr_crypt_blk32(const void *ctx, byte *out,
				      const byte *in, byte *iv) ASM_FUNC_ABI;

#ifdef USE_VAES_AVX2
extern unsigned int
_gcry_aria_vaes_avx2_ecb_crypt_blk32(const void *ctx, byte *out,
				     const byte *in,
				     const void *key) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_vaes_avx2_ctr_crypt_blk32(const void *ctx, byte *out,
				     const byte *in, byte *iv) ASM_FUNC_ABI;
#endif /* USE_VAES_AVX2 */

#ifdef USE_GFNI_AVX2
extern unsigned int
_gcry_aria_gfni_avx2_ecb_crypt_blk32(const void *ctx, byte *out,
				     const byte *in,
				     const void *key) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_gfni_avx2_ctr_crypt_blk32(const void *ctx, byte *out,
				     const byte *in, byte *iv) ASM_FUNC_ABI;
#endif /* USE_GFNI_AVX2 */

static inline unsigned int
aria_avx2_ecb_crypt_blk32(const ARIA_context *ctx, byte *out, const byte *in,
			  const u32 key[][ARIA_RD_KEY_WORDS])
{
  if (0) { }
#ifdef USE_GFNI_AVX2
  else if (ctx->use_gfni_avx2)
    return _gcry_aria_gfni_avx2_ecb_crypt_blk32(ctx, out, in, key)
		+ ASM_EXTRA_STACK;
#endif /* USE_GFNI_AVX2 */
#ifdef USE_VAES_AVX2
  else if (ctx->use_vaes_avx2)
    return _gcry_aria_vaes_avx2_ecb_crypt_blk32(ctx, out, in, key)
		+ ASM_EXTRA_STACK;
#endif /* USE_VAES_AVX2 */
  else
    return _gcry_aria_aesni_avx2_ecb_crypt_blk32(ctx, out, in, key)
		+ ASM_EXTRA_STACK;
}

static inline unsigned int
aria_avx2_ctr_crypt_blk32(const ARIA_context *ctx, byte *out, const byte *in,
			  byte *iv)
{
  if (0) { }
#ifdef USE_GFNI_AVX2
  else if (ctx->use_gfni_avx2)
    return _gcry_aria_gfni_avx2_ctr_crypt_blk32(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
#endif /* USE_GFNI_AVX2 */
#ifdef USE_VAES_AVX2
  else if (ctx->use_vaes_avx2)
    return _gcry_aria_vaes_avx2_ctr_crypt_blk32(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
#endif /* USE_VAES_AVX2 */
  else
    return _gcry_aria_aesni_avx2_ctr_crypt_blk32(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
}
#endif /* USE_AESNI_AVX2 */

#ifdef USE_GFNI_AVX512
extern unsigned int
_gcry_aria_gfni_avx512_ecb_crypt_blk64(const void *ctx, byte *out,
				       const byte *in,
				       const void *key) ASM_FUNC_ABI;
extern unsigned int
_gcry_aria_gfni_avx512_ctr_crypt_blk64(const void *ctx, byte *out,
				       const byte *in, byte *iv) ASM_FUNC_ABI;

static inline unsigned int
aria_gfni_avx512_ecb_crypt_blk64(const ARIA_context *ctx, byte *out,
				 const byte *in,
				 const u32 key[][ARIA_RD_KEY_WORDS])
{
  return _gcry_aria_gfni_avx512_ecb_crypt_blk64(ctx, out, in, key)
		+ ASM_EXTRA_STACK;
}

static inline unsigned int
aria_gfni_avx512_ctr_crypt_blk64(const ARIA_context *ctx, byte *out,
				 const byte *in, byte *iv)
{
  return _gcry_aria_gfni_avx512_ctr_crypt_blk64(ctx, out, in, iv)
		+ ASM_EXTRA_STACK;
}
#endif /* USE_AESNI_AVX2 */

/* Prefetching for sbox tables. */
static inline void
prefetch_table(const volatile byte *tab, size_t len)
{
  size_t i;

  for (i = 0; len - i >= 8 * 32; i += 8 * 32)
    {
      (void)tab[i + 0 * 32];
      (void)tab[i + 1 * 32];
      (void)tab[i + 2 * 32];
      (void)tab[i + 3 * 32];
      (void)tab[i + 4 * 32];
      (void)tab[i + 5 * 32];
      (void)tab[i + 6 * 32];
      (void)tab[i + 7 * 32];
    }
  for (; i < len; i += 32)
    {
      (void)tab[i];
    }

  (void)tab[len - 1];
}

static inline void
prefetch_sboxes(void)
{
  /* Modify counters to trigger copy-on-write and unsharing if physical pages
   * of look-up table are shared between processes.  Modifying counters also
   * causes checksums for pages to change and hint same-page merging algorithm
   * that these pages are frequently changing.  */
  sboxes.counter_head++;
  sboxes.counter_tail++;

  /* Prefetch look-up tables to cache.  */
  prefetch_table((const void *)&sboxes, sizeof(sboxes));
}


static ALWAYS_INLINE
u32 rotr32(u32 v, u32 r)
{
  return ror(v, r);
}

static ALWAYS_INLINE
u32 bswap32(u32 v)
{
  return _gcry_bswap32(v);
}

static ALWAYS_INLINE u32
get_u8(u32 x, u32 y)
{
  return (x >> ((3 - y) * 8)) & 0xFF;
}

static ALWAYS_INLINE u32
make_u32(byte v0, byte v1, byte v2, byte v3)
{
  return ((u32)v0 << 24) | ((u32)v1 << 16) | ((u32)v2 <<  8) | ((u32)v3);
}

static ALWAYS_INLINE u32
aria_m(u32 t0)
{
  return rotr32(t0, 8) ^ rotr32(t0 ^ rotr32(t0, 8), 16);
}

/* S-Box Layer 1 + M */
static ALWAYS_INLINE void
aria_sbox_layer1_with_pre_diff(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  *t0 = sboxes.s1[get_u8(*t0, 0)] ^
	sboxes.s2[get_u8(*t0, 1)] ^
	sboxes.x1[get_u8(*t0, 2)] ^
	sboxes.x2[get_u8(*t0, 3)];
  *t1 = sboxes.s1[get_u8(*t1, 0)] ^
	sboxes.s2[get_u8(*t1, 1)] ^
	sboxes.x1[get_u8(*t1, 2)] ^
	sboxes.x2[get_u8(*t1, 3)];
  *t2 = sboxes.s1[get_u8(*t2, 0)] ^
	sboxes.s2[get_u8(*t2, 1)] ^
	sboxes.x1[get_u8(*t2, 2)] ^
	sboxes.x2[get_u8(*t2, 3)];
  *t3 = sboxes.s1[get_u8(*t3, 0)] ^
	sboxes.s2[get_u8(*t3, 1)] ^
	sboxes.x1[get_u8(*t3, 2)] ^
	sboxes.x2[get_u8(*t3, 3)];
}

/* S-Box Layer 2 + M */
static ALWAYS_INLINE void
aria_sbox_layer2_with_pre_diff(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  *t0 = sboxes.x1[get_u8(*t0, 0)] ^
	sboxes.x2[get_u8(*t0, 1)] ^
	sboxes.s1[get_u8(*t0, 2)] ^
	sboxes.s2[get_u8(*t0, 3)];
  *t1 = sboxes.x1[get_u8(*t1, 0)] ^
	sboxes.x2[get_u8(*t1, 1)] ^
	sboxes.s1[get_u8(*t1, 2)] ^
	sboxes.s2[get_u8(*t1, 3)];
  *t2 = sboxes.x1[get_u8(*t2, 0)] ^
	sboxes.x2[get_u8(*t2, 1)] ^
	sboxes.s1[get_u8(*t2, 2)] ^
	sboxes.s2[get_u8(*t2, 3)];
  *t3 = sboxes.x1[get_u8(*t3, 0)] ^
	sboxes.x2[get_u8(*t3, 1)] ^
	sboxes.s1[get_u8(*t3, 2)] ^
	sboxes.s2[get_u8(*t3, 3)];
}

/* Word-level diffusion */
static ALWAYS_INLINE void
aria_diff_word(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  *t1 ^= *t2;
  *t2 ^= *t3;
  *t0 ^= *t1;

  *t3 ^= *t1;
  *t2 ^= *t0;
  *t1 ^= *t2;
}

/* Byte-level diffusion */
static inline void aria_diff_byte(u32 *t1, u32 *t2, u32 *t3)
{
  *t1 = ((*t1 << 8) & 0xff00ff00) ^ ((*t1 >> 8) & 0x00ff00ff);
  *t2 = rotr32(*t2, 16);
  *t3 = bswap32(*t3);
}

/* Key XOR Layer */
static ALWAYS_INLINE void
aria_add_round_key(u32 *rk, u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  *t0 ^= rk[0];
  *t1 ^= rk[1];
  *t2 ^= rk[2];
  *t3 ^= rk[3];
}

/* Odd round Substitution & Diffusion */
static ALWAYS_INLINE void
aria_subst_diff_odd(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  aria_sbox_layer1_with_pre_diff(t0, t1, t2, t3);
  aria_diff_word(t0, t1, t2, t3);
  aria_diff_byte(t1, t2, t3);
  aria_diff_word(t0, t1, t2, t3);
}

/* Even round Substitution & Diffusion */
static ALWAYS_INLINE void
aria_subst_diff_even(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  aria_sbox_layer2_with_pre_diff(t0, t1, t2, t3);
  aria_diff_word(t0, t1, t2, t3);
  aria_diff_byte(t3, t0, t1);
  aria_diff_word(t0, t1, t2, t3);
}

/* Last round */
static ALWAYS_INLINE void
aria_last_round(u32 *t0, u32 *t1, u32 *t2, u32 *t3)
{
  *t0 = make_u32((byte)(sboxes.x1[get_u8(*t0, 0)]),
		 (byte)(sboxes.x2[get_u8(*t0, 1)] >> 24),
		 (byte)(sboxes.s1[get_u8(*t0, 2)]),
		 (byte)(sboxes.s2[get_u8(*t0, 3)]));
  *t1 = make_u32((byte)(sboxes.x1[get_u8(*t1, 0)]),
		 (byte)(sboxes.x2[get_u8(*t1, 1)] >> 24),
		 (byte)(sboxes.s1[get_u8(*t1, 2)]),
		 (byte)(sboxes.s2[get_u8(*t1, 3)]));
  *t2 = make_u32((byte)(sboxes.x1[get_u8(*t2, 0)]),
		 (byte)(sboxes.x2[get_u8(*t2, 1)] >> 24),
		 (byte)(sboxes.s1[get_u8(*t2, 2)]),
		 (byte)(sboxes.s2[get_u8(*t2, 3)]));
  *t3 = make_u32((byte)(sboxes.x1[get_u8(*t3, 0)]),
		 (byte)(sboxes.x2[get_u8(*t3, 1)] >> 24),
		 (byte)(sboxes.s1[get_u8(*t3, 2)]),
		 (byte)(sboxes.s2[get_u8(*t3, 3)]));
}

/* Q, R Macro expanded ARIA GSRK */
static ALWAYS_INLINE void
aria_gsrk(u32 *rk, u32 *x, u32 *y, u32 n)
{
  int q = 4 - (n / 32);
  int r = n % 32;

  rk[0] = (x[0]) ^
	  ((y[q % 4]) >> r) ^
	  ((y[(q + 3) % 4]) << (32 - r));
  rk[1] = (x[1]) ^
	  ((y[(q + 1) % 4]) >> r) ^
	  ((y[q % 4]) << (32 - r));
  rk[2] = (x[2]) ^
	  ((y[(q + 2) % 4]) >> r) ^
	  ((y[(q + 1) % 4]) << (32 - r));
  rk[3] = (x[3]) ^
	  ((y[(q + 3) % 4]) >> r) ^
	  ((y[(q + 2) % 4]) << (32 - r));
}


static NO_INLINE void
aria_set_encrypt_key(ARIA_context *ctx, const byte *in_key, u32 key_len)
{
  u32 w0[4], w1[4], w2[4], w3[4];
  u32 reg0, reg1, reg2, reg3;
  const u32 *ck;
  int rkidx = 0;

  ctx->rounds = (key_len + 32) / 4;
  prefetch_sboxes();

  ck = &key_rc[(key_len - 16) / 2];

  w0[0] = buf_get_be32(in_key + 0);
  w0[1] = buf_get_be32(in_key + 4);
  w0[2] = buf_get_be32(in_key + 8);
  w0[3] = buf_get_be32(in_key + 12);

  reg0 = w0[0] ^ ck[0];
  reg1 = w0[1] ^ ck[1];
  reg2 = w0[2] ^ ck[2];
  reg3 = w0[3] ^ ck[3];

  aria_subst_diff_odd(&reg0, &reg1, &reg2, &reg3);

  if (key_len > 16)
    {
      w1[0] = buf_get_be32(in_key + 16);
      w1[1] = buf_get_be32(in_key + 20);
      if (key_len > 24)
	{
	  w1[2] = buf_get_be32(in_key + 24);
	  w1[3] = buf_get_be32(in_key + 28);
	}
      else
	{
	  w1[2] = 0;
	  w1[3] = 0;
	}
    }
  else
    {
      w1[0] = 0;
      w1[1] = 0;
      w1[2] = 0;
      w1[3] = 0;
    }

  w1[0] ^= reg0;
  w1[1] ^= reg1;
  w1[2] ^= reg2;
  w1[3] ^= reg3;

  reg0 = w1[0];
  reg1 = w1[1];
  reg2 = w1[2];
  reg3 = w1[3];

  reg0 ^= ck[4];
  reg1 ^= ck[5];
  reg2 ^= ck[6];
  reg3 ^= ck[7];

  aria_subst_diff_even(&reg0, &reg1, &reg2, &reg3);

  reg0 ^= w0[0];
  reg1 ^= w0[1];
  reg2 ^= w0[2];
  reg3 ^= w0[3];

  w2[0] = reg0;
  w2[1] = reg1;
  w2[2] = reg2;
  w2[3] = reg3;

  reg0 ^= ck[8];
  reg1 ^= ck[9];
  reg2 ^= ck[10];
  reg3 ^= ck[11];

  aria_subst_diff_odd(&reg0, &reg1, &reg2, &reg3);

  w3[0] = reg0 ^ w1[0];
  w3[1] = reg1 ^ w1[1];
  w3[2] = reg2 ^ w1[2];
  w3[3] = reg3 ^ w1[3];

  aria_gsrk(ctx->enc_key[rkidx], w0, w1, 19);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w1, w2, 19);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w2, w3, 19);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w3, w0, 19);

  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w0, w1, 31);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w1, w2, 31);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w2, w3, 31);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w3, w0, 31);

  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w0, w1, 67);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w1, w2, 67);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w2, w3, 67);
  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w3, w0, 67);

  rkidx++;
  aria_gsrk(ctx->enc_key[rkidx], w0, w1, 97);
  if (key_len > 16)
    {
      rkidx++;
      aria_gsrk(ctx->enc_key[rkidx], w1, w2, 97);
      rkidx++;
      aria_gsrk(ctx->enc_key[rkidx], w2, w3, 97);

      if (key_len > 24)
	{
	  rkidx++;
	  aria_gsrk(ctx->enc_key[rkidx], w3, w0, 97);

	  rkidx++;
	  aria_gsrk(ctx->enc_key[rkidx], w0, w1, 109);
	}
    }

  wipememory(w0, sizeof(w0));
  wipememory(w1, sizeof(w1));
  wipememory(w2, sizeof(w2));
  wipememory(w3, sizeof(w3));
}

static void
aria_set_decrypt_key(ARIA_context *ctx)
{
  int i;

  for (i = 0; i < 4; i++)
    {
      ctx->dec_key[0][i] = ctx->enc_key[ctx->rounds][i];
      ctx->dec_key[ctx->rounds][i] = ctx->enc_key[0][i];
    }

  for (i = 1; i < ctx->rounds; i++)
    {
      ctx->dec_key[i][0] = aria_m(ctx->enc_key[ctx->rounds - i][0]);
      ctx->dec_key[i][1] = aria_m(ctx->enc_key[ctx->rounds - i][1]);
      ctx->dec_key[i][2] = aria_m(ctx->enc_key[ctx->rounds - i][2]);
      ctx->dec_key[i][3] = aria_m(ctx->enc_key[ctx->rounds - i][3]);

      aria_diff_word(&ctx->dec_key[i][0], &ctx->dec_key[i][1],
		     &ctx->dec_key[i][2], &ctx->dec_key[i][3]);
      aria_diff_byte(&ctx->dec_key[i][1],
		     &ctx->dec_key[i][2], &ctx->dec_key[i][3]);
      aria_diff_word(&ctx->dec_key[i][0], &ctx->dec_key[i][1],
		     &ctx->dec_key[i][2], &ctx->dec_key[i][3]);
    }
}

static NO_INLINE unsigned int
aria_crypt(ARIA_context *ctx, byte *out, const byte *in,
	   u32 key[][ARIA_RD_KEY_WORDS])
{
  u32 reg0, reg1, reg2, reg3;
  int rounds = ctx->rounds;
  int rkidx = 0;

  reg0 = buf_get_be32(in + 0);
  reg1 = buf_get_be32(in + 4);
  reg2 = buf_get_be32(in + 8);
  reg3 = buf_get_be32(in + 12);

  aria_add_round_key(key[rkidx], &reg0, &reg1, &reg2, &reg3);
  rkidx++;

  while (1)
    {
      aria_subst_diff_odd(&reg0, &reg1, &reg2, &reg3);
      aria_add_round_key(key[rkidx], &reg0, &reg1, &reg2, &reg3);
      rkidx++;

      if (rkidx >= rounds)
	break;

      aria_subst_diff_even(&reg0, &reg1, &reg2, &reg3);
      aria_add_round_key(key[rkidx], &reg0, &reg1, &reg2, &reg3);
      rkidx++;
    }

  aria_last_round(&reg0, &reg1, &reg2, &reg3);
  aria_add_round_key(key[rkidx], &reg0, &reg1, &reg2, &reg3);

  buf_put_be32(out + 0, reg0);
  buf_put_be32(out + 4, reg1);
  buf_put_be32(out + 8, reg2);
  buf_put_be32(out + 12, reg3);

  return 4 * sizeof(void *) + 4 * sizeof(u32); /* stack burn depth */
}

unsigned int
aria_encrypt(void *c, byte *outbuf, const byte *inbuf)
{
  ARIA_context *ctx = (ARIA_context *)c;

  prefetch_sboxes ();

  return aria_crypt (ctx, outbuf, inbuf, ctx->enc_key);
}

unsigned int
aria_decrypt(void *c, byte *outbuf, const byte *inbuf)
{
  ARIA_context *ctx = (ARIA_context *)c;

  if (!ctx->decryption_prepared)
    {
      aria_set_decrypt_key (ctx);
      ctx->decryption_prepared = 1;
    }

  prefetch_sboxes ();

  return aria_crypt (ctx, outbuf, inbuf, ctx->dec_key);
}


static unsigned int
aria_crypt_2blks(ARIA_context *ctx, byte *out, const byte *in,
		 u32 key[][ARIA_RD_KEY_WORDS])
{
  u32 ra0, ra1, ra2, ra3;
  u32 rb0, rb1, rb2, rb3;
  int rounds = ctx->rounds;
  int rkidx = 0;

  ra0 = buf_get_be32(in + 0);
  ra1 = buf_get_be32(in + 4);
  ra2 = buf_get_be32(in + 8);
  ra3 = buf_get_be32(in + 12);
  rb0 = buf_get_be32(in + 16);
  rb1 = buf_get_be32(in + 20);
  rb2 = buf_get_be32(in + 24);
  rb3 = buf_get_be32(in + 28);

  while (1)
    {
      aria_add_round_key(key[rkidx], &ra0, &ra1, &ra2, &ra3);
      aria_add_round_key(key[rkidx], &rb0, &rb1, &rb2, &rb3);
      rkidx++;

      aria_subst_diff_odd(&ra0, &ra1, &ra2, &ra3);
      aria_subst_diff_odd(&rb0, &rb1, &rb2, &rb3);
      aria_add_round_key(key[rkidx], &ra0, &ra1, &ra2, &ra3);
      aria_add_round_key(key[rkidx], &rb0, &rb1, &rb2, &rb3);
      rkidx++;

      if (rkidx >= rounds)
	break;

      aria_subst_diff_even(&ra0, &ra1, &ra2, &ra3);
      aria_subst_diff_even(&rb0, &rb1, &rb2, &rb3);
    }

  aria_last_round(&ra0, &ra1, &ra2, &ra3);
  aria_last_round(&rb0, &rb1, &rb2, &rb3);
  aria_add_round_key(key[rkidx], &ra0, &ra1, &ra2, &ra3);
  aria_add_round_key(key[rkidx], &rb0, &rb1, &rb2, &rb3);

  buf_put_be32(out + 0, ra0);
  buf_put_be32(out + 4, ra1);
  buf_put_be32(out + 8, ra2);
  buf_put_be32(out + 12, ra3);
  buf_put_be32(out + 16, rb0);
  buf_put_be32(out + 20, rb1);
  buf_put_be32(out + 24, rb2);
  buf_put_be32(out + 28, rb3);

  return 4 * sizeof(void *) + 8 * sizeof(u32); /* stack burn depth */
}

static unsigned int
aria_crypt_blocks (ARIA_context *ctx, byte *out, const byte *in,
		   size_t num_blks, u32 key[][ARIA_RD_KEY_WORDS])
{
  unsigned int burn_depth = 0;

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      unsigned int nburn = 0;

      while (num_blks >= 64)
	{
	  nburn = aria_gfni_avx512_ecb_crypt_blk64 (ctx, out, in, key);
	  in += 64 * ARIA_BLOCK_SIZE;
	  out += 64 * ARIA_BLOCK_SIZE;
	  num_blks -= 64;
	}

      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      if (num_blks == 0)
	return burn_depth;
    }
#endif /* USE_AESNI_AVX2 */

#ifdef USE_AESNI_AVX2
  if (ctx->use_aesni_avx2 || ctx->use_gfni_avx2)
    {
      unsigned int nburn = 0;

      while (num_blks >= 32)
	{
	  nburn = aria_avx2_ecb_crypt_blk32 (ctx, out, in, key);
	  in += 32 * ARIA_BLOCK_SIZE;
	  out += 32 * ARIA_BLOCK_SIZE;
	  num_blks -= 32;
	}

      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      if (num_blks == 0)
	return burn_depth;
    }
#endif /* USE_AESNI_AVX2 */

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx || ctx->use_gfni_avx)
    {
      unsigned int nburn = 0;

      while (num_blks >= 3)
	{
	  size_t curr_blks = num_blks < 16 ? num_blks : 16;
	  nburn = aria_avx_ecb_crypt_blk1_16 (ctx, out, in, key, curr_blks);
	  in += curr_blks * ARIA_BLOCK_SIZE;
	  out += curr_blks * ARIA_BLOCK_SIZE;
	  num_blks -= curr_blks;
	}

      burn_depth = nburn > burn_depth ? nburn : burn_depth;

      if (num_blks == 0)
	return burn_depth;
    }
#endif /* USE_AESNI_AVX */

  if (!ctx->bulk_prefetch_ready)
    {
      prefetch_sboxes();
      ctx->bulk_prefetch_ready = 1;
    }

  while (num_blks >= 2)
    {
      unsigned int nburn = aria_crypt_2blks (ctx, out, in, key);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;
      out += 2 * ARIA_BLOCK_SIZE;
      in += 2 * ARIA_BLOCK_SIZE;
      num_blks -= 2;
    }

  while (num_blks)
    {
      unsigned int nburn = aria_crypt (ctx, out, in, key);
      burn_depth = nburn > burn_depth ? nburn : burn_depth;
      out += ARIA_BLOCK_SIZE;
      in += ARIA_BLOCK_SIZE;
      num_blks--;
    }

  if (burn_depth)
    burn_depth += sizeof(void *) * 5;
  return burn_depth;
}

static unsigned int
aria_enc_blocks (void *c, byte *out, const byte *in, size_t num_blks)
{
  ARIA_context *ctx = (ARIA_context *)c;

  return aria_crypt_blocks (ctx, out, in, num_blks, ctx->enc_key);
}

static unsigned int
aria_dec_blocks (void *c, byte *out, const byte *in, size_t num_blks)
{
  ARIA_context *ctx = (ARIA_context *)c;

  return aria_crypt_blocks (ctx, out, in, num_blks, ctx->dec_key);
}


/* Bulk encryption of complete blocks in CTR mode.  This function is only
   intended for the bulk encryption feature of cipher.c.  CTR is expected to be
   of size 16. */
static void
_gcry_aria_ctr_enc(void *context, unsigned char *ctr,
		   void *outbuf_arg, const void *inbuf_arg,
		   size_t nblocks)
{
  ARIA_context *ctx = context;
  byte *outbuf = outbuf_arg;
  const byte *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

#ifdef USE_GFNI_AVX512
  if (ctx->use_gfni_avx512)
    {
      size_t nburn = 0;

      while (nblocks >= 64)
	{
	  nburn = aria_gfni_avx512_ctr_crypt_blk64 (ctx, outbuf, inbuf, ctr);
	  inbuf += 64 * ARIA_BLOCK_SIZE;
	  outbuf += 64 * ARIA_BLOCK_SIZE;
	  nblocks -= 64;
	}

      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;
    }
#endif /* USE_AESNI_AVX */

#ifdef USE_AESNI_AVX2
  if (ctx->use_aesni_avx2 || ctx->use_gfni_avx2)
    {
      size_t nburn = 0;

      while (nblocks >= 32)
	{
	  nburn = aria_avx2_ctr_crypt_blk32 (ctx, outbuf, inbuf, ctr);
	  inbuf += 32 * ARIA_BLOCK_SIZE;
	  outbuf += 32 * ARIA_BLOCK_SIZE;
	  nblocks -= 32;
	}

      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;
    }
#endif /* USE_AESNI_AVX */

#ifdef USE_AESNI_AVX
  if (ctx->use_aesni_avx || ctx->use_gfni_avx)
    {
      size_t nburn = 0;

      while (nblocks >= 16)
	{
	  nburn = aria_avx_ctr_crypt_blk16 (ctx, outbuf, inbuf, ctr);
	  inbuf += 16 * ARIA_BLOCK_SIZE;
	  outbuf += 16 * ARIA_BLOCK_SIZE;
	  nblocks -= 16;
	}

      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;
    }
#endif /* USE_AESNI_AVX */

  /* Process remaining blocks. */
  if (nblocks)
    {
      byte tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      size_t nburn = 0;

      ctx->bulk_prefetch_ready = 0;

      nburn = bulk_ctr_enc_128(ctx, aria_enc_blocks, outbuf, inbuf,
			       nblocks, ctr, tmpbuf,
			       sizeof(tmpbuf) / ARIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption of complete blocks in CBC mode. */
static void
_gcry_aria_cbc_enc (void *context, unsigned char *iv,
		    void *outbuf_arg, const void *inbuf_arg,
		    size_t nblocks, int cbc_mac)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned char *last_iv;
  unsigned int burn_depth = 0;

  prefetch_sboxes();

  last_iv = iv;

  for (; nblocks; nblocks--)
    {
      cipher_block_xor (outbuf, inbuf, last_iv, ARIA_BLOCK_SIZE);

      burn_depth = aria_crypt (ctx, outbuf, outbuf, ctx->enc_key);

      last_iv = outbuf;
      inbuf += ARIA_BLOCK_SIZE;
      if (!cbc_mac)
	outbuf += ARIA_BLOCK_SIZE;
    }

  if (last_iv != iv)
    cipher_block_cpy (iv, last_iv, ARIA_BLOCK_SIZE);

  if (burn_depth)
    _gcry_burn_stack (burn_depth + 4 * sizeof(void *));
}

/* Bulk decryption of complete blocks in CBC mode.  This function is only
   intended for the bulk encryption feature of cipher.c. */
static void
_gcry_aria_cbc_dec(void *context, unsigned char *iv,
		   void *outbuf_arg, const void *inbuf_arg,
		   size_t nblocks)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  if (!ctx->decryption_prepared)
    {
      aria_set_decrypt_key (ctx);
      ctx->decryption_prepared = 1;
    }

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;

      nburn = bulk_cbc_dec_128(ctx, aria_dec_blocks, outbuf, inbuf,
			       nblocks, iv, tmpbuf,
			       sizeof(tmpbuf) / ARIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption of complete blocks in CFB mode. */
static void
_gcry_aria_cfb_enc (void *context, unsigned char *iv,
		    void *outbuf_arg, const void *inbuf_arg,
		    size_t nblocks)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  unsigned int burn_depth = 0;

  prefetch_sboxes();

  for (; nblocks; nblocks--)
    {
      /* Encrypt the IV. */
      burn_depth = aria_crypt (ctx, iv, iv, ctx->enc_key);
      /* XOR the input with the IV and store input into IV.  */
      cipher_block_xor_2dst(outbuf, iv, inbuf, ARIA_BLOCK_SIZE);
      outbuf += ARIA_BLOCK_SIZE;
      inbuf += ARIA_BLOCK_SIZE;
    }

  if (burn_depth)
    _gcry_burn_stack (burn_depth + 4 * sizeof(void *));
}

/* Bulk decryption of complete blocks in CFB mode.  This function is only
   intended for the bulk encryption feature of cipher.c. */
static void
_gcry_aria_cfb_dec(void *context, unsigned char *iv,
		   void *outbuf_arg, const void *inbuf_arg,
		   size_t nblocks)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;

      nburn = bulk_cfb_dec_128(ctx, aria_enc_blocks, outbuf, inbuf,
			       nblocks, iv, tmpbuf,
			       sizeof(tmpbuf) / ARIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption/decryption in ECB mode. */
static void
_gcry_aria_ecb_crypt (void *context, void *outbuf_arg,
		      const void *inbuf_arg, size_t nblocks, int encrypt)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  if (!encrypt && !ctx->decryption_prepared)
    {
      aria_set_decrypt_key (ctx);
      ctx->decryption_prepared = 1;
    }

  /* Process remaining blocks. */
  if (nblocks)
    {
      bulk_crypt_fn_t crypt_blk1_n;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;
      crypt_blk1_n = encrypt ? aria_enc_blocks : aria_dec_blocks;

      nburn = bulk_ecb_crypt_128(ctx, crypt_blk1_n,
				 outbuf, inbuf, nblocks, MAX_PARALLEL_BLKS);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption/decryption of complete blocks in XTS mode. */
static void
_gcry_aria_xts_crypt (void *context, unsigned char *tweak, void *outbuf_arg,
		      const void *inbuf_arg, size_t nblocks, int encrypt)
{
  ARIA_context *ctx = context;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  if (!encrypt && !ctx->decryption_prepared)
    {
      aria_set_decrypt_key (ctx);
      ctx->decryption_prepared = 1;
    }

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      bulk_crypt_fn_t crypt_blk1_n;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;
      crypt_blk1_n = encrypt ? aria_enc_blocks : aria_dec_blocks;

      nburn = bulk_xts_crypt_128(ctx, crypt_blk1_n,
				 outbuf, inbuf, nblocks,
				 tweak, tmpbuf,
				 sizeof(tmpbuf) / ARIA_BLOCK_SIZE,
				 &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption of complete blocks in CTR32LE mode (for GCM-SIV). */
static void
_gcry_aria_ctr32le_enc(void *context, unsigned char *ctr,
		       void *outbuf_arg, const void *inbuf_arg,
		       size_t nblocks)
{
  ARIA_context *ctx = context;
  byte *outbuf = outbuf_arg;
  const byte *inbuf = inbuf_arg;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;

      nburn = bulk_ctr32le_enc_128 (ctx, aria_enc_blocks, outbuf, inbuf,
				    nblocks, ctr, tmpbuf,
				    sizeof(tmpbuf) / ARIA_BLOCK_SIZE,
				    &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);
}

/* Bulk encryption/decryption of complete blocks in OCB mode. */
static size_t
_gcry_aria_ocb_crypt (gcry_cipher_hd_t c, void *outbuf_arg,
		      const void *inbuf_arg, size_t nblocks, int encrypt)
{
  ARIA_context *ctx = (void *)&c->context.c;
  unsigned char *outbuf = outbuf_arg;
  const unsigned char *inbuf = inbuf_arg;
  u64 blkn = c->u_mode.ocb.data_nblocks;
  int burn_stack_depth = 0;

  if (!encrypt && !ctx->decryption_prepared)
    {
      aria_set_decrypt_key (ctx);
      ctx->decryption_prepared = 1;
    }

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      bulk_crypt_fn_t crypt_blk1_n;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;
      crypt_blk1_n = encrypt ? aria_enc_blocks : aria_dec_blocks;

      nburn = bulk_ocb_crypt_128 (c, ctx, crypt_blk1_n, outbuf, inbuf, nblocks,
				  &blkn, encrypt, tmpbuf,
				  sizeof(tmpbuf) / ARIA_BLOCK_SIZE,
				  &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  c->u_mode.ocb.data_nblocks = blkn;

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);

  return 0;
}

/* Bulk authentication of complete blocks in OCB mode. */
static size_t
_gcry_aria_ocb_auth (gcry_cipher_hd_t c, const void *abuf_arg, size_t nblocks)
{
  ARIA_context *ctx = (void *)&c->context.c;
  const unsigned char *abuf = abuf_arg;
  u64 blkn = c->u_mode.ocb.aad_nblocks;
  int burn_stack_depth = 0;

  /* Process remaining blocks. */
  if (nblocks)
    {
      unsigned char tmpbuf[MAX_PARALLEL_BLKS * ARIA_BLOCK_SIZE];
      unsigned int tmp_used = ARIA_BLOCK_SIZE;
      size_t nburn;

      ctx->bulk_prefetch_ready = 0;

      nburn = bulk_ocb_auth_128 (c, ctx, aria_enc_blocks, abuf, nblocks,
				 &blkn, tmpbuf,
				 sizeof(tmpbuf) / ARIA_BLOCK_SIZE, &tmp_used);
      burn_stack_depth = nburn > burn_stack_depth ? nburn : burn_stack_depth;

      wipememory (tmpbuf, tmp_used);
    }

  c->u_mode.ocb.aad_nblocks = blkn;

  if (burn_stack_depth)
    _gcry_burn_stack (burn_stack_depth);

  return 0;
}


static gcry_err_code_t
aria_setkey(void *c, const byte *key, unsigned keylen,
	    cipher_bulk_ops_t *bulk_ops)
{
  ARIA_context *ctx = c;
  static int initialized = 0;
  static const char *selftest_failed = NULL;
  unsigned int hwf = _gcry_get_hw_features ();

  (void)hwf;

  if (keylen != 16 && keylen != 24 && keylen != 32)
    return GPG_ERR_INV_KEYLEN;

  if (!initialized)
    {
      initialized = 1;
      selftest_failed = aria_selftest ();
      if (selftest_failed)
	log_error("%s\n", selftest_failed);
    }

  if (selftest_failed)
    return GPG_ERR_SELFTEST_FAILED;

#ifdef USE_GFNI_AVX512
  ctx->use_gfni_avx512 = (hwf & HWF_INTEL_GFNI) && (hwf & HWF_INTEL_AVX512);
#endif
#ifdef USE_AESNI_AVX2
  ctx->use_aesni_avx2 = (hwf & HWF_INTEL_AESNI) && (hwf & HWF_INTEL_AVX2);
#endif
#ifdef USE_GFNI_AVX2
  ctx->use_gfni_avx2 = (hwf & HWF_INTEL_GFNI) && (hwf & HWF_INTEL_AVX2);
#endif
#ifdef USE_VAES_AVX2
  ctx->use_vaes_avx2 = (hwf & HWF_INTEL_VAES_VPCLMUL) && (hwf & HWF_INTEL_AVX2);
#endif
#ifdef USE_AESNI_AVX
  ctx->use_aesni_avx = (hwf & HWF_INTEL_AESNI) && (hwf & HWF_INTEL_AVX);
#endif
#ifdef USE_GFNI_AVX
  ctx->use_gfni_avx = (hwf & HWF_INTEL_GFNI) && (hwf & HWF_INTEL_AVX);
#endif

  /* Setup bulk encryption routines.  */
  memset (bulk_ops, 0, sizeof(*bulk_ops));
  bulk_ops->cbc_enc = _gcry_aria_cbc_enc;
  bulk_ops->cbc_dec = _gcry_aria_cbc_dec;
  bulk_ops->cfb_enc = _gcry_aria_cfb_enc;
  bulk_ops->cfb_dec = _gcry_aria_cfb_dec;
  bulk_ops->ctr_enc = _gcry_aria_ctr_enc;
  bulk_ops->ctr32le_enc = _gcry_aria_ctr32le_enc;
  bulk_ops->ecb_crypt = _gcry_aria_ecb_crypt;
  bulk_ops->xts_crypt = _gcry_aria_xts_crypt;
  bulk_ops->ocb_crypt = _gcry_aria_ocb_crypt;
  bulk_ops->ocb_auth = _gcry_aria_ocb_auth;

  /* Setup context and encryption key. */
  ctx->decryption_prepared = 0;
  aria_set_encrypt_key (ctx, key, keylen);

  _gcry_burn_stack (3 * sizeof(void *) + 5 * 4 * sizeof(u32));
  return 0;
}


static const char *
aria_selftest (void)
{
  ARIA_context ctx;
  byte scratch[16];

  static const byte key[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
  };
  static const byte plaintext[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
  };
  static const byte ciphertext[16] = {
    0xd7, 0x18, 0xfb, 0xd6, 0xab, 0x64, 0x4c, 0x73,
    0x9d, 0xa9, 0x5f, 0x3b, 0xe6, 0x45, 0x17, 0x78
  };

  memset (&ctx, 0, sizeof(ctx));

  aria_set_encrypt_key (&ctx, key, 16);
  aria_encrypt (&ctx, scratch, plaintext);
  if (memcmp (scratch, ciphertext, sizeof (ciphertext)))
    return "ARIA test encryption failed.";
  aria_decrypt (&ctx, scratch, scratch);
  if (memcmp (scratch, plaintext, sizeof (plaintext)))
    return "ARIA test decryption failed.";

  return NULL;
}


static const gcry_cipher_oid_spec_t aria128_oids[] =
  {
    { "1.2.410.200046.1.1.1", GCRY_CIPHER_MODE_ECB },
    { "1.2.410.200046.1.1.2", GCRY_CIPHER_MODE_CBC },
    { "1.2.410.200046.1.1.3", GCRY_CIPHER_MODE_CFB },
    { "1.2.410.200046.1.1.4", GCRY_CIPHER_MODE_OFB },
    { "1.2.410.200046.1.1.5", GCRY_CIPHER_MODE_CTR },
    { "1.2.410.200046.1.1.34", GCRY_CIPHER_MODE_GCM },
    { "1.2.410.200046.1.1.37", GCRY_CIPHER_MODE_CCM },
    { NULL }
  };

static const gcry_cipher_oid_spec_t aria192_oids[] =
  {
    { "1.2.410.200046.1.1.6", GCRY_CIPHER_MODE_ECB },
    { "1.2.410.200046.1.1.7", GCRY_CIPHER_MODE_CBC },
    { "1.2.410.200046.1.1.8", GCRY_CIPHER_MODE_CFB },
    { "1.2.410.200046.1.1.9", GCRY_CIPHER_MODE_OFB },
    { "1.2.410.200046.1.1.10", GCRY_CIPHER_MODE_CTR },
    { "1.2.410.200046.1.1.35", GCRY_CIPHER_MODE_GCM },
    { "1.2.410.200046.1.1.38", GCRY_CIPHER_MODE_CCM },
    { NULL }
  };

static const gcry_cipher_oid_spec_t aria256_oids[] =
  {
    { "1.2.410.200046.1.1.11", GCRY_CIPHER_MODE_ECB },
    { "1.2.410.200046.1.1.12", GCRY_CIPHER_MODE_CBC },
    { "1.2.410.200046.1.1.13", GCRY_CIPHER_MODE_CFB },
    { "1.2.410.200046.1.1.14", GCRY_CIPHER_MODE_OFB },
    { "1.2.410.200046.1.1.15", GCRY_CIPHER_MODE_CTR },
    { "1.2.410.200046.1.1.36", GCRY_CIPHER_MODE_GCM },
    { "1.2.410.200046.1.1.39", GCRY_CIPHER_MODE_CCM },
    { NULL }
  };

gcry_cipher_spec_t _gcry_cipher_spec_aria128 =
  {
    GCRY_CIPHER_ARIA128, { 0, 0 },
    "ARIA128", NULL, aria128_oids, ARIA_BLOCK_SIZE, 128,
    sizeof(ARIA_context), aria_setkey, aria_encrypt, aria_decrypt
  };

gcry_cipher_spec_t _gcry_cipher_spec_aria192 =
  {
    GCRY_CIPHER_ARIA192, { 0, 0 },
    "ARIA192",NULL,aria192_oids, ARIA_BLOCK_SIZE, 192,
    sizeof(ARIA_context), aria_setkey, aria_encrypt, aria_decrypt
  };

gcry_cipher_spec_t _gcry_cipher_spec_aria256 =
  {
    GCRY_CIPHER_ARIA256, { 0, 0 },
    "ARIA256", NULL, aria256_oids, ARIA_BLOCK_SIZE, 256,
    sizeof(ARIA_context), aria_setkey, aria_encrypt, aria_decrypt
  };
