/* Returns a human readable description of an ELF header e_machine value.
   Copyright (C) 2019 Red Hat, Inc.
   This file is part of elfutils.

   This file is free software; you can redistribute it and/or modify
   it under the terms of either

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at
       your option) any later version

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at
       your option) any later version

   or both in parallel, as here.

   elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see <http://www.gnu.org/licenses/>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "libdwelf.h"


const char *
dwelf_elf_e_machine_string (int machine)
{
  switch (machine)
    {
    case EM_NONE:
      return "None";
    case EM_M32:
      return "WE32100";
    case EM_SPARC:
      return "SPARC";
    case EM_386:
      return "Intel 80386";
    case EM_68K:
      return "M68K";
    case EM_88K:
      return "M88K";
    case EM_IAMCU:
      return "Intel MCU";
    case EM_860:
      return "Intel 80860";
    case EM_MIPS:
      return "MIPS R3000";
    case EM_S370:
      return "IBM System/370";
    case EM_MIPS_RS3_LE:
      return "MIPS R3000";
    case EM_PARISC:
      return "HPPA";
    case EM_VPP500:
      return "Fujitsu VPP500";
    case EM_SPARC32PLUS:
      return "SPARC v8+";
    case EM_960:
      return "Intel 80960";
    case EM_PPC:
      return "PowerPC";
    case EM_PPC64:
      return "PowerPC64";
    case EM_S390:
      return "IBM S/390";
    case EM_SPU:
      return "IBM SPU/SPC";
    case EM_V800:
      return "NEC V800";
    case EM_FR20:
      return "Fujitsu FR20";
    case EM_RH32:
      return "TRW RH-32";
    case EM_RCE:
      return "Motorola RCE";
    case EM_ARM:
      return "ARM";
    case EM_FAKE_ALPHA:
      return "Digital Alpha";
    case EM_SH:
      return "SH";
    case EM_SPARCV9:
      return "SPARC v9";
    case EM_TRICORE:
      return "Siemens Tricore";
    case EM_ARC:
      return "ARC";
    case EM_H8_300:
      return "H8/300";
    case EM_H8_300H:
      return "H8/300H";
    case EM_H8S:
      return "H8S";
    case EM_H8_500:
      return "H8/500";
    case EM_IA_64:
      return "Intel IA-64";
    case EM_MIPS_X:
      return "Stanford MIPS-X";
    case EM_COLDFIRE:
      return "Motorola Coldfire";
    case EM_68HC12:
      return "Motorola M68HC12";
    case EM_MMA:
      return "Fujitsu MMA Multimedia Accelerator";
    case EM_PCP:
      return "Siemens PCP";
    case EM_NCPU:
      return "Sony nCPU embedded RISC";
    case EM_NDR1:
      return "Denso NDR1 microprocessor";
    case EM_STARCORE:
      return "Motorola Star*Core processor";
    case EM_ME16:
      return "Toyota ME16 processor";
    case EM_ST100:
      return "STMicroelectronic ST100";
    case EM_TINYJ:
      return "Advanced Logic Corporation Tinyj";
    case EM_X86_64:
      return "AMD x86-64";
    case EM_PDSP:
      return "Sony DSP Processor";
    case EM_PDP10:
      return "Digital PDP-10";
    case EM_PDP11:
      return "Digital PDP-11";
    case EM_FX66:
      return "Siemens FX66 microcontroller";
    case EM_ST9PLUS:
      return "STMicroelectronics ST9+";
    case EM_ST7:
      return "STMicroelectronics ST7";
    case EM_68HC16:
      return "Motorola MC68HC16 microcontroller";
    case EM_68HC11:
      return "Motorola MC68HC11 microcontroller";
    case EM_68HC08:
      return "Motorola MC68HC08 microcontroller";
    case EM_68HC05:
      return "Motorola MC68HC05 microcontroller";
    case EM_SVX:
      return "Silicon Graphics SVx";
    case EM_ST19:
      return "STMicroelectronics ST19";
    case EM_VAX:
      return "Digital VAX";
    case EM_CRIS:
      return "Axis Communications 32-bit embedded processor";
    case EM_JAVELIN:
      return "Infineon Technologies 32-bit embedded processor";
    case EM_FIREPATH:
      return "Element 14 64-bit DSP Processor";
    case EM_ZSP:
      return "LSI Logic 16-bit DSP Processor";
    case EM_MMIX:
      return "Donald Knuth's educational 64-bit processor";
    case EM_HUANY:
      return "Harvard University machine-independent object";
    case EM_PRISM:
      return "SiTera Prism";
    case EM_AVR:
      return "Atmel AVR 8-bit microcontroller";
    case EM_FR30:
      return "Fujitsu FR30";
    case EM_D10V:
      return "Mitsubishi D10V";
    case EM_D30V:
      return "Mitsubishi D30V";
    case EM_V850:
      return "NEC v850";
    case EM_M32R:
      return "Mitsubishi M32R";
    case EM_MN10300:
      return "Matsushita MN10300";
    case EM_MN10200:
      return "Matsushita MN10200";
    case EM_PJ:
      return "picoJava";
    case EM_OPENRISC:
      return "OpenRISC";
    case EM_ARC_COMPACT:
      return "ARC International ARCompact";
    case EM_XTENSA:
      return "Tensilica Xtensa Architecture";
    case EM_VIDEOCORE:
      return "Alphamosaic VideoCore";
    case EM_TMM_GPP:
      return "Thompson Multimedia General Purpose Processor";
    case EM_NS32K:
      return "National Semiconductor 32000";
    case EM_TPC:
      return "Tenor Network TPC";
    case EM_SNP1K:
      return "Trebia SNP 1000";
    case EM_ST200:
      return "STMicroelectronics ST200";
    case EM_IP2K:
      return "Ubicom IP2xxx";
    case EM_MAX:
      return "MAX processor";
    case EM_CR:
      return "National Semiconductor CompactRISC";
    case EM_F2MC16:
      return "Fujitsu F2MC16";
    case EM_MSP430:
      return "Texas Instruments msp430";
    case EM_BLACKFIN:
      return "Analog Devices Blackfin DSP";
    case EM_SE_C33:
      return "Seiko Epson S1C33";
    case EM_SEP:
      return "Sharp embedded microprocessor";
    case EM_ARCA:
      return "Arca RISC";
    case EM_UNICORE:
      return "Unicore";
    case EM_EXCESS:
      return "eXcess configurable CPU";
    case EM_DXP:
      return "Icera Semiconductor Deep Execution Processor";
    case EM_ALTERA_NIOS2:
      return "Altera Nios II";
    case EM_CRX:
      return "National Semiconductor CompactRISC CRX";
    case EM_XGATE:
      return "Motorola XGATE";
    case EM_C166:
      return "Infineon C16x/XC16x";
    case EM_M16C:
      return "Renesas M16C";
    case EM_DSPIC30F:
      return "Microchip Technology dsPIC30F";
    case EM_CE:
      return "Freescale Communication Engine RISC";
    case EM_M32C:
      return "Renesas M32C";
    case EM_TSK3000:
      return "Altium TSK3000";
    case EM_RS08:
      return "Freescale RS08";
    case EM_SHARC:
      return "Analog Devices SHARC";
    case EM_ECOG2:
      return "Cyan Technology eCOG2";
    case EM_SCORE7:
      return "Sunplus S+core7 RISC";
    case EM_DSP24:
      return "New Japan Radio (NJR) 24-bit DSP";
    case EM_VIDEOCORE3:
      return "Broadcom VideoCore III";
    case EM_LATTICEMICO32:
      return "RISC for Lattice FPGA";
    case EM_SE_C17:
      return "Seiko Epson C17";
    case EM_TI_C6000:
      return "Texas Instruments TMS320C6000 DSP";
    case EM_TI_C2000:
      return "Texas Instruments TMS320C2000 DSP";
    case EM_TI_C5500:
      return "Texas Instruments TMS320C55x DSP";
    case EM_TI_ARP32:
      return "Texas Instruments Application Specific RISC";
    case EM_TI_PRU:
      return "Texas Instruments Programmable Realtime Unit";
    case EM_MMDSP_PLUS:
      return "STMicroelectronics 64bit VLIW DSP";
    case EM_CYPRESS_M8C:
      return "Cypress M8C";
    case EM_R32C:
      return "Renesas R32C";
    case EM_TRIMEDIA:
      return "NXP Semiconductors TriMedia";
    case EM_QDSP6:
      return "QUALCOMM DSP6";
    case EM_8051:
      return "Intel 8051 and variants";
    case EM_STXP7X:
      return "STMicroelectronics STxP7x";
    case EM_NDS32:
      return "Andes Technology compact code size embedded RISC";
    case EM_ECOG1X:
      return "Cyan Technology eCOG1X";
    case EM_MAXQ30:
      return "Dallas Semiconductor MAXQ30";
    case EM_XIMO16:
      return "New Japan Radio (NJR) 16-bit DSP";
    case EM_MANIK:
      return "M2000 Reconfigurable RISC";
    case EM_CRAYNV2:
      return "Cray NV2 vector architecture";
    case EM_RX:
      return "Renesas RX";
    case EM_METAG:
      return "Imagination Technologies META";
    case EM_MCST_ELBRUS:
      return "MCST Elbrus";
    case EM_ECOG16:
      return "Cyan Technology eCOG16";
    case EM_CR16:
      return "National Semiconductor CompactRISC";
    case EM_ETPU:
      return "Freescale Extended Time Processing Unit";
    case EM_SLE9X:
      return "Infineon Technologies SLE9X";
    case EM_L10M:
      return "Intel L10M";
    case EM_K10M:
      return "Intel K10M";
    case EM_AARCH64:
      return "AARCH64";
    case EM_AVR32:
      return "Amtel AVR32";
    case EM_STM8:
      return "STMicroelectronics STM8";
    case EM_TILE64:
      return "Tilera TILE64";
    case EM_TILEPRO:
      return "Tilera TILEPro";
    case EM_MICROBLAZE:
      return "Xilinx MicroBlaze";
    case EM_CUDA:
      return "NVIDIA CUDA";
    case EM_TILEGX:
      return "Tilera TILE-Gx";
    case EM_CLOUDSHIELD:
      return "CloudShield";
    case EM_COREA_1ST:
      return "KIPO-KAIST Core-A 1st gen";
    case EM_COREA_2ND:
      return "KIPO-KAIST Core-A 2nd gen";
    case EM_ARCV2:
      return "Synopsys ARCv2 ISA";
    case EM_OPEN8:
      return "Open8 RISC";
    case EM_RL78:
      return "Renesas RL78";
    case EM_VIDEOCORE5:
      return "Broadcom VideoCore V";
    case EM_78KOR:
      return "Renesas 78KOR";
    case EM_56800EX:
      return "Freescale 56800EX DSC";
    case EM_BA1:
      return "Beyond BA1";
    case EM_BA2:
      return "Beyond BA2";
    case EM_XCORE:
      return "XMOS xCORE";
    case EM_MCHP_PIC:
      return "Microchip 8-bit PIC";
    case EM_INTELGT:
      return "Intel Graphics Technology";
    case EM_KM32:
      return "KM211 KM32";
    case EM_KMX32:
      return "KM211 KMX32";
    case EM_EMX16:
      return "KM211 KMX16";
    case EM_EMX8:
      return "KM211 KMX8";
    case EM_KVARC:
      return "KM211 KVARC";
    case EM_CDP:
      return "Paneve CDP";
    case EM_COGE:
      return "Cognitive Smart Memory Processor";
    case EM_COOL:
      return "Bluechip CoolEngine";
    case EM_NORC:
      return "Nanoradio Optimized RISC";
    case EM_CSR_KALIMBA:
      return "CSR Kalimba";
    case EM_Z80:
      return "Zilog Z80";
    case EM_VISIUM:
      return "CDS VISIUMcore";
    case EM_FT32:
      return "FTDI Chip FT32";
    case EM_MOXIE:
      return "Moxie";
    case EM_AMDGPU:
      return "AMD GPU";
    case EM_RISCV:
      return "RISC-V";
    case EM_BPF:
      return "BPF";
    case EM_CSKY:
      return "C-SKY";
    case EM_LOONGARCH:
      return "LoongArch";

    case EM_ALPHA:
      return "Alpha";

    default:
      return NULL;
    }
}
