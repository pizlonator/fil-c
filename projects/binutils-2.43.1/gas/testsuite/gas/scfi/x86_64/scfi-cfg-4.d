#as: --scfi=experimental -W
#as:
#objdump: -Wf
#name: Synthesize CFI in presence of control flow 4
#...
Contents of the .eh_frame section:

00000000 0+0014 0+0000 CIE
  Version:               1
  Augmentation:          "zR"
  Code alignment factor: 1
  Data alignment factor: -8
  Return address column: 16
  Augmentation data:     1b
  DW_CFA_def_cfa: r7 \(rsp\) ofs 8
  DW_CFA_offset: r16 \(rip\) at cfa-8
  DW_CFA_nop
  DW_CFA_nop

0+0018 0+002c 0+001c FDE cie=00000000 pc=0000000000000000..0000000000000045
  DW_CFA_advance_loc: 1 to 0000000000000001
  DW_CFA_def_cfa_offset: 16
  DW_CFA_offset: r3 \(rbx\) at cfa-16
  DW_CFA_advance_loc: 6 to 0000000000000007
  DW_CFA_def_cfa_offset: 32
  DW_CFA_advance_loc: 15 to 0000000000000016
  DW_CFA_remember_state
  DW_CFA_advance_loc: 4 to 000000000000001a
  DW_CFA_def_cfa_offset: 16
  DW_CFA_advance_loc: 1 to 000000000000001b
  DW_CFA_restore: r3 \(rbx\)
  DW_CFA_def_cfa_offset: 8
  DW_CFA_advance_loc: 1 to 000000000000001c
  DW_CFA_restore_state
  DW_CFA_advance_loc: 35 to 000000000000003f
  DW_CFA_def_cfa_offset: 16
  DW_CFA_advance_loc: 1 to 0000000000000040
  DW_CFA_restore: r3 \(rbx\)
  DW_CFA_def_cfa_offset: 8
  DW_CFA_nop
#...

#pass
