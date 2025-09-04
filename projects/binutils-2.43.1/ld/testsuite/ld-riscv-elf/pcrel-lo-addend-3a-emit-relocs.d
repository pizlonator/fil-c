#source: pcrel-lo-addend-3a.s
#as: -march=rv64i -mabi=lp64 -mno-relax
#ld: -m[riscv_choose_lp64_emul] -Tpcrel-lo-addend-3.ld --emit-relocs
#objdump: -dr

#...
Disassembly of section .text:

0+900000000 <_start>:
.*:[ 	]+[0-9a-f]+[ 	]+lui[ 	]+a5,0x2
.*:[ 	]+R_RISCV_HI20[ 	]+\*ABS\*\+0x2000
.*:[ 	]+[0-9a-f]+[ 	]+ld[ 	]+a0,0\(a5\) # 2000 <ll>
.*:[ 	]+R_RISCV_LO12_I[ 	]+\*ABS\*\+0x2000
.*:[ 	]+[0-9a-f]+[ 	]+ld[ 	]+a0,4\(a5\)
.*:[ 	]+R_RISCV_LO12_I[ 	]+\*ABS\*\+0x2004
.*:[ 	]+[0-9a-f]+[ 	]+lui[ 	]+a5,0x2
.*:[ 	]+R_RISCV_HI20[ 	]+\*ABS\*\+0x2004
.*:[ 	]+[0-9a-f]+[ 	]+ld[ 	]+a0,4\(a5\) # 2004 <ll\+0x4>
.*:[ 	]+R_RISCV_LO12_I[ 	]+\*ABS\*\+0x2004
.*:[ 	]+[0-9a-f]+[ 	]+ld[ 	]+a0,8\(a5\)
.*:[ 	]+R_RISCV_LO12_I[ 	]+\*ABS\*\+0x2008
.*:[ 	]+[0-9a-f]+[ 	]+lui[ 	]+a5,0x1
.*:[ 	]+R_RISCV_HI20[ 	]+\*ABS\*\+0x1008
.*:[ 	]+[0-9a-f]+[ 	]+ld[ 	]+a0,8\(a5\) # 1008 <_GLOBAL_OFFSET_TABLE_\+0x8>
.*:[ 	]+R_RISCV_LO12_I[ 	]+\*ABS\*\+0x1008
#pass
