  .text
.ifdef b16
  beq $a0, $a1, low
  beq $a0, $a1, high
.endif
.ifdef b21
  beqz $a0, low
  beqz $a0, high
.endif
.ifdef b26
  b low
  b high
.endif
.ifdef pcrel20
  .reloc 0, R_LARCH_PCREL20_S2, low
  pcaddi $a0, 0
  .reloc 4, R_LARCH_PCREL20_S2, high
  pcaddi $a0, 0
.endif

  .section ".low"
low: nop
  .section ".high"
high: nop
