/* elf.c - Executable Linking Format manipulation functions.
 *
 * Copyright 2023 Rob Landley <rob@landley.net>
 */

#include "toys.h"

char *elf_arch_name(int type)
{
  int i;

  // Values from include/linux/elf-em.h (plus arch/*/include/asm/elf.h)
  // Names are linux/arch/ directory (sometimes before 32/64 bit merges)
  struct {int val; char *name;} types[] = {{0x9026, "alpha"}, {93, "arc"},
    {195, "arcv2"}, {40, "arm"}, {183, "arm64"}, {0x18ad, "avr32"},
    {247, "bpf"}, {106, "blackfin"}, {140, "c6x"}, {23, "cell"}, {76, "cris"},
    {252, "csky"}, {0x5441, "frv"}, {46, "h8300"}, {164, "hexagon"},
    {50, "ia64"}, {258, "loongarch"}, {88, "m32r"}, {0x9041, "m32r"},
    {4, "m68k"}, {174, "metag"}, {189, "microblaze"},
    {0xbaab, "microblaze-old"}, {8, "mips"}, {10, "mips-old"}, {89, "mn10300"},
    {0xbeef, "mn10300-old"}, {113, "nios2"}, {92, "openrisc"},
    {0x8472, "openrisc-old"}, {15, "parisc"}, {20, "ppc"}, {21, "ppc64"},
    {243, "riscv"}, {22, "s390"}, {0xa390, "s390-old"}, {135, "score"},
    {42, "sh"}, {2, "sparc"}, {18, "sparc8+"}, {43, "sparc9"}, {188, "tile"},
    {191, "tilegx"}, {3, "386"}, {6, "486"}, {62, "x86-64"}, {94, "xtensa"},
    {0xabc7, "xtensa-old"}
  };

  for (i = 0; i<ARRAY_LEN(types); i++)
    if (type==types[i].val) return types[i].name;

  sprintf(libbuf, "unknown arch %d", type);
  return libbuf;
}

void elf_print_flags(int arch, int flags)
{
  if (arch == 40) { // arm32
    printf(", EABI%u", (flags >> 24) & 0xf);
    if (flags & 0x200) printf(", soft float");
    else if (flags & 0x400) printf(", hard float");
  } else if (arch == 243) { // riscv
    if (flags & 1) printf(", C");
    if (flags & 8) printf(", E");
    if (flags & 0x10) printf(", TSO");
    printf(", %s float",
      (char *[]){"soft", "single", "double", "quad"}[(flags&0x6)/2]);
  }
}
