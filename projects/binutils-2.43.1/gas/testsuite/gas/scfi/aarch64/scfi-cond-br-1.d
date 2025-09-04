#as: --scfi=experimental -W
#objdump: -Wf
#name: Synthesize CFI for conditional br 1
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

0+0014 0+0010 00000018 FDE cie=00000000 pc=0+0000..0+0010
  DW_CFA_nop

#pass
