  .text

.set idx, 0
.ifdef underflow
  nop
  .set idx, 4
.endif

.ifdef b16
  beq $a0, $a1, local_sym
.endif
.ifdef b21
  beqz $a0, local_sym
.endif
.ifdef b26
  b local_sym
.endif
.ifdef pcrel20
  .reloc idx, R_LARCH_PCREL20_S2, local_sym
  pcaddi $a0, 0
.endif

  .section ".local_sym"
local_sym: nop
