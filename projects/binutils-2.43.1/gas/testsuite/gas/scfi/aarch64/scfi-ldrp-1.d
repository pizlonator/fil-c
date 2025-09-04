#as: --scfi=experimental -W
#objdump: -Wf
#name: Synthesize CFI for ldp ldr instructions
#...
Contents of the .eh_frame section:

00000000 0+0010 00000000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 4
  Data alignment factor: -8
  Return address column: 30
  Augmentation data:     1b
  DW_CFA_def_cfa: r31 \(sp\) ofs 0

00000014 0+0048 00000018 FDE cie=00000000 pc=0+0000..0+0040
  DW_CFA_advance_loc: 4 to 0+0004
  DW_CFA_def_cfa_offset: 128
  DW_CFA_offset: r29 \(x29\) at cfa-128
  DW_CFA_offset: r30 \(x30\) at cfa-120
  DW_CFA_advance_loc: 4 to 0+0008
  DW_CFA_def_cfa_register: r29 \(x29\)
  DW_CFA_advance_loc: 4 to 0+000c
  DW_CFA_offset: r19 \(x19\) at cfa-112
  DW_CFA_offset: r20 \(x20\) at cfa-104
  DW_CFA_advance_loc: 4 to 0+0010
  DW_CFA_offset: r21 \(x21\) at cfa-96
  DW_CFA_offset: r22 \(x22\) at cfa-88
  DW_CFA_advance_loc: 4 to 0+0014
  DW_CFA_offset: r23 \(x23\) at cfa-80
  DW_CFA_offset: r24 \(x24\) at cfa-72
  DW_CFA_advance_loc: 4 to 0+0018
  DW_CFA_offset: r25 \(x25\) at cfa-64
  DW_CFA_offset: r26 \(x26\) at cfa-56
  DW_CFA_advance_loc: 4 to 0+001c
  DW_CFA_offset: r27 \(x27\) at cfa-48
  DW_CFA_advance_loc: 8 to 0+0024
  DW_CFA_def_cfa_register: r31 \(sp\)
  DW_CFA_advance_loc: 4 to 0+0028
  DW_CFA_restore: r19 \(x19\)
  DW_CFA_restore: r20 \(x20\)
  DW_CFA_advance_loc: 4 to 0+002c
  DW_CFA_restore: r21 \(x21\)
  DW_CFA_restore: r22 \(x22\)
  DW_CFA_advance_loc: 4 to 0+0030
  DW_CFA_restore: r23 \(x23\)
  DW_CFA_restore: r24 \(x24\)
  DW_CFA_advance_loc: 4 to 0+0034
  DW_CFA_restore: r25 \(x25\)
  DW_CFA_restore: r26 \(x26\)
  DW_CFA_advance_loc: 4 to 0+0038
  DW_CFA_restore: r27 \(x27\)
  DW_CFA_advance_loc: 4 to 0+003c
  DW_CFA_restore: r29 \(x29\)
  DW_CFA_restore: r30 \(x30\)
  DW_CFA_def_cfa_offset: 0
  DW_CFA_nop

#pass
