#as: --scfi=experimental -W
#objdump: -Wf
#name: Synthesize CFI for callee-saved FP regs I
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

00000014 0+[0-9a-f]+ 0+0018 FDE cie=00000000 pc=0+0000..0+0040
  DW_CFA_advance_loc: 4 to 0+0004
  DW_CFA_def_cfa_offset: 128
  DW_CFA_offset: r29 \(x29\) at cfa-128
  DW_CFA_offset: r30 \(x30\) at cfa-120
  DW_CFA_advance_loc: 4 to 0+0008
  DW_CFA_def_cfa_register: r29 \(x29\)
  DW_CFA_advance_loc: 4 to 0+000c
  DW_CFA_offset_extended: r72 \(v8\) at cfa-112
  DW_CFA_advance_loc: 4 to 0+0010
  DW_CFA_offset_extended: r73 \(v9\) at cfa-104
  DW_CFA_advance_loc: 4 to 0+0014
  DW_CFA_offset_extended: r74 \(v10\) at cfa-96
  DW_CFA_offset_extended: r75 \(v11\) at cfa-88
  DW_CFA_advance_loc: 4 to 0+0018
  DW_CFA_offset_extended: r76 \(v12\) at cfa-80
  DW_CFA_offset_extended: r77 \(v13\) at cfa-72
  DW_CFA_advance_loc: 4 to 0+001c
  DW_CFA_offset_extended: r78 \(v14\) at cfa-64
  DW_CFA_offset_extended: r79 \(v15\) at cfa-56
  DW_CFA_advance_loc: 8 to 0+0024
  DW_CFA_def_cfa_register: r31 \(sp\)
  DW_CFA_advance_loc: 4 to 0+0028
  DW_CFA_restore_extended: r72 \(v8\)
  DW_CFA_advance_loc: 4 to 0+002c
  DW_CFA_restore_extended: r73 \(v9\)
  DW_CFA_advance_loc: 4 to 0+0030
  DW_CFA_restore_extended: r74 \(v10\)
  DW_CFA_restore_extended: r75 \(v11\)
  DW_CFA_advance_loc: 4 to 0+0034
  DW_CFA_restore_extended: r76 \(v12\)
  DW_CFA_restore_extended: r77 \(v13\)
  DW_CFA_advance_loc: 4 to 0+0038
  DW_CFA_restore_extended: r78 \(v14\)
  DW_CFA_restore_extended: r79 \(v15\)
  DW_CFA_advance_loc: 4 to 0+003c
  DW_CFA_restore: r29 \(x29\)
  DW_CFA_restore: r30 \(x30\)
  DW_CFA_def_cfa_offset: 0
  DW_CFA_nop

#pass
