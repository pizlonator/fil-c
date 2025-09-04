.macro sign_check reloc_name reloc_bits
  .reloc off, R_LARCH_SOP_PUSH_ABSOLUTE, (1 << (\reloc_bits - 1)) - 1
  .reloc off, \reloc_name
  .word 0
  .set off, off + 4
  .reloc off, R_LARCH_SOP_PUSH_ABSOLUTE, -(1 << (\reloc_bits - 1))
  .reloc off, \reloc_name
  .word 0
  .set off, off + 4
.endm

.macro sign_check_s2 reloc_name reloc_bits
  .reloc off, R_LARCH_SOP_PUSH_ABSOLUTE, ((1 << (\reloc_bits - 1)) - 1) << 2
  .reloc off, \reloc_name
  .word 0
  .set off, off + 4
  .reloc off, R_LARCH_SOP_PUSH_ABSOLUTE, (-(1 << (\reloc_bits - 1))) << 2
  .reloc off, \reloc_name
  .word 0
  .set off, off + 4
.endm

.macro unsign_check reloc_name reloc_bits
  .reloc off, R_LARCH_SOP_PUSH_ABSOLUTE, (1 << \reloc_bits) - 1
  .reloc off, \reloc_name
  .word 0
  .set off, off + 4
.endm

.set off, 0
sign_check R_LARCH_SOP_POP_32_S_10_5 5
sign_check R_LARCH_SOP_POP_32_S_10_12 12
sign_check R_LARCH_SOP_POP_32_S_10_16 16
sign_check_s2 R_LARCH_SOP_POP_32_S_10_16_S2 16
sign_check_s2 R_LARCH_SOP_POP_32_S_0_5_10_16_S2 21
sign_check_s2 R_LARCH_SOP_POP_32_S_0_10_10_16_S2 26
sign_check R_LARCH_SOP_POP_32_S_5_20 20
unsign_check R_LARCH_SOP_POP_32_U_10_12 12
unsign_check R_LARCH_SOP_POP_32_U 32
