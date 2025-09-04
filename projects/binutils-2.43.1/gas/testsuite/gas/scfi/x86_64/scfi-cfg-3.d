#as: --scfi=experimental -W
#as:
#objdump: -Wf
#name: Synthesize CFI in presence of control flow 3
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

0+0018 0+0010 0+001c FDE cie=00000000 pc=0+0000..0+001e
  DW_CFA_nop
#...

0+002c 0+0018 0+0030 FDE cie=00000000 pc=0+001e..0+0029
  DW_CFA_advance_loc: 1 to 0+001f
  DW_CFA_def_cfa_offset: 16
  DW_CFA_offset: r6 \(rbp\) at cfa-16
  DW_CFA_advance_loc: 3 to 0+0022
  DW_CFA_def_cfa_register: r6 \(rbp\)
  DW_CFA_nop

#pass
