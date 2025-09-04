#as: --scfi=experimental -W
#objdump: -Wf
#name: Synthesize CFI for cfg 2
#...
Contents of the .eh_frame section:


0+0000 0+0010 0+0000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 4
  Data alignment factor: -8
  Return address column: 30
  Augmentation data:     1b
  DW_CFA_def_cfa: r31 \(sp\) ofs 0

0+0014 0+[0-9a-f]+ 0+0018 FDE cie=00000000 pc=0+0000..0+0028
  DW_CFA_advance_loc: 4 to 0+0004
  DW_CFA_def_cfa_offset: 48
  DW_CFA_offset: r29 \(x29\) at cfa-48
  DW_CFA_offset: r30 \(x30\) at cfa-40
  DW_CFA_advance_loc: 4 to 0+0008
  DW_CFA_def_cfa_register: r29 \(x29\)
  DW_CFA_advance_loc: 8 to 0+0010
  DW_CFA_remember_state
  DW_CFA_advance_loc: 8 to 0+0018
  DW_CFA_def_cfa_register: r31 \(sp\)
  DW_CFA_restore: r29 \(x29\)
  DW_CFA_restore: r30 \(x30\)
  DW_CFA_def_cfa_offset: 0
  DW_CFA_advance_loc: 4 to 0+001c
  DW_CFA_restore_state
  DW_CFA_advance_loc: 8 to 0+0024
  DW_CFA_def_cfa_register: r31 \(sp\)
  DW_CFA_restore: r29 \(x29\)
  DW_CFA_restore: r30 \(x30\)
  DW_CFA_def_cfa_offset: 0
  DW_CFA_nop

#pass
